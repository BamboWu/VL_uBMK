//====== Graph Benchmark Suites ======//
//======== Connected Component =======//
//
// Usage: ./tc.exe --dataset <dataset path>

#include "common.h"
#include "def.h"
#include "openG.h"
#include "omp.h"
#include <set>
#include <vector>
#include <algorithm>
#include <raft>

#if RAFTLIB_ORIG
#include "raftlib_orig.hpp"
#endif

#ifdef HMC
#include "HMC.h"
#endif

#ifdef SIM
#include "SIM.h"
#endif

#include "timing.h"

#ifndef NOGEM5
#include "gem5/m5ops.h"
#endif

using namespace std;

size_t repetition = 0;
size_t maxiter = 0;
size_t beginiter = 0;
size_t enditer = 0;

class vertex_property
{
public:
    vertex_property():count(0){}

    int16_t count;
    std::vector<uint64_t> neighbor_set;
};

class edge_property
{
public:
    edge_property():value(0){}
    edge_property(uint8_t x):value(x){}

    uint8_t value;
};

typedef openG::extGraph<vertex_property, edge_property> graph_t;
typedef graph_t::vertex_iterator    vertex_iterator;
typedef graph_t::edge_iterator      edge_iterator;

//====================RaftLib Structures========================//
size_t get_intersect_cnt(vector<size_t>& setA, vector<size_t>& setB);
struct intersect_msg
{
    size_t src;
    size_t dst;
    vector<size_t> *src_set;
    vector<size_t> *dst_set;
    intersect_msg() = default;
    intersect_msg(const intersect_msg& rhs) : src(rhs.src), dst(rhs.dst),
        src_set(rhs.src_set), dst_set(rhs.dst_set) {}
    intersect_msg(size_t s, size_t d, vector<size_t>& sset,
            vector<size_t>& dset) :
        src(s), dst(d), src_set(&sset), dst_set(&dset) {}
};
struct reduction_msg
{
    size_t src; // src vertex
    size_t dst; // dst vertex
    int16_t cnt; // count of triangles to add to *pcount
};

class reduce_kernel : public raft::parallel_k
{
public:
    reduce_kernel(int16_t* cnts, int tc_num) :
        raft::parallel_k(), pcnts(cnts) {
#if RAFTLIB_ORIG
        for (int i = 0; tc_num > i; ++i) {
            addPortTo<struct reduction_msg>(input);
        }
#else
        add_input<struct reduction_msg>("0"_port);
#endif
    }
#if RAFTLIB_ORIG
    virtual raft::kstatus run() {
        for (auto& port : input) {
            if (0 < port.size()) {
                auto& msg(port.template peek<struct reduction_msg>());
                update_cnt(msg);
                port.recycle(1);
            }
        }
        return raft::kstatus::proceed;
    }
#else
    virtual raft::kstatus::value_t compute(raft::StreamingData &dataIn,
                                           raft::StreamingData &bufOut) {
        auto& msg(dataIn.template peek<struct reduction_msg>());
        update_cnt(msg);
        dataIn.recycle();
        return raft::kstatus::proceed;
    }
    virtual bool pop(raft::Task *task, bool dryrun) { return true; }
    virtual bool allocate(raft::Task *task, bool dryrun) { return true; }
#endif

    inline void update_cnt(struct reduction_msg& msg) {
#ifdef __GNUC__
        __atomic_add_fetch(&pcnts[msg.src], msg.cnt, __ATOMIC_RELAXED);
        __atomic_add_fetch(&pcnts[msg.dst], msg.cnt, __ATOMIC_RELAXED);
#else
        pcnts[msg.src] += msg.cnt;
        pcnts[msg.dst] += msg.cnt;
#endif
    }
    int16_t* pcnts;
};

class count_kernel : public raft::parallel_k
{
public:
    count_kernel() : raft::parallel_k() {
        add_input<struct intersect_msg>("input"_port);
        add_output<struct reduction_msg>("output"_port);
    }
    ~count_kernel() = default;
#if RAFTLIB_ORIG
    count_kernel(const count_kernel& other) : raft::parallel_k() {
        add_input<struct intersect_msg>("input"_port);
        add_output<struct reduction_msg>("output"_port);
    }
    CLONE(); // enable cloning
    virtual raft::kstatus run() {
        // run triangle count now
        struct intersect_msg& msg(
                input["input"_port].peek<struct intersect_msg>());
        int16_t cnt = get_intersect_cnt(*msg.src_set, *msg.dst_set);
        output["output"_port].template push<struct reduction_msg>(
                {msg.src, msg.dst, cnt});
        input["input"_port].recycle();
        return raft::kstatus::proceed;
    }
#else
    virtual raft::kstatus::value_t compute(raft::StreamingData &dataIn,
                                           raft::StreamingData &bufOut) {
        auto& msg(dataIn.template peek<struct intersect_msg>());
        int16_t cnt = get_intersect_cnt(*msg.src_set, *msg.dst_set);
        bufOut.template push<struct reduction_msg>(
                {msg.src, msg.dst, cnt});
        dataIn.recycle();
        return raft::kstatus::proceed;
    }
    virtual bool pop(raft::Task *task, bool dryrun) { return true; }
    virtual bool allocate(raft::Task *task, bool dryrun) { return true; }
#endif
};

class lookup_kernel : public raft::parallel_k
{
public:
    lookup_kernel(graph_t& g) : raft::parallel_k(), g_(g) {
        add_input<vertex_iterator>("input"_port);
        add_output<struct intersect_msg>("output"_port);
    }
    ~lookup_kernel() = default;
#if RAFTLIB_ORIG
    lookup_kernel(const lookup_kernel& other) :
        raft::parallel_k(), g_(other.g_) {
        add_input<vertex_iterator>("input"_port);
        add_output<struct intersect_msg>("output"_port);
    }
    CLONE(); // enable cloning
    virtual raft::kstatus run() {
        vertex_iterator& vit(input["input"_port].peek<vertex_iterator>());

        vector<uint64_t> & src_set = vit->property().neighbor_set;

        for (edge_iterator eit=vit->edges_begin();eit!=vit->edges_end();eit++) 
        {
            if (vit->id() > eit->target()) continue; // skip reverse edges
            vertex_iterator vit_targ = g_.find_vertex(eit->target());

            vector<uint64_t> & dest_set = vit_targ->property().neighbor_set;
            output["output"_port].template push<struct intersect_msg>(
                    {vit->id(), vit_targ->id(), src_set, dest_set});
        }
        input["input"_port].recycle();
        return raft::kstatus::proceed;
    }
#else
    virtual raft::kstatus::value_t compute(raft::StreamingData &dataIn,
                                           raft::StreamingData &bufOut) {
        vertex_iterator& vit(dataIn.peek<vertex_iterator>());

        vector<uint64_t> & src_set = vit->property().neighbor_set;

        for (edge_iterator eit=vit->edges_begin();eit!=vit->edges_end();eit++) 
        {
            if (vit->id() > eit->target()) continue; // skip reverse edges
            vertex_iterator vit_targ = g_.find_vertex(eit->target());

            vector<uint64_t> & dest_set = vit_targ->property().neighbor_set;
            bufOut.template push<struct intersect_msg>(
                    {vit->id(), vit_targ->id(), src_set, dest_set});
        }
        dataIn.recycle();
        return raft::kstatus::proceed;
    }
    virtual bool pop(raft::Task *task, bool dryrun) { return true; }
    virtual bool allocate(raft::Task *task, bool dryrun) { return true; }
#endif
private:
    graph_t& g_;
};

class workset_kernel : public raft::parallel_k
{
public:
    workset_kernel(graph_t& g, int tc_num) : raft::parallel_k(), g_(g) {
#if RAFTLIB_ORIG
        for (int i = 0; tc_num > i; ++i) {
            addPortTo<vertex_iterator>(output);
        }
#else
        add_output<vertex_iterator>("0"_port);
#endif
        vid = 0;
        rep = 0;
    }
#if RAFTLIB_ORIG
    virtual raft::kstatus run() {
        for (auto& port : output) {
            if (port.space_avail()) {
                vertex_iterator vit = g_.find_vertex(vid);
                port.push(vit);
                if (g_.num_vertices() <= ++vid) {
                    if (repetition <= ++rep) {
                        return raft::kstatus::stop;
                    }
                    vid = 0;
                }
            }
        }
        return raft::kstatus::proceed;
    }
#else
    virtual raft::kstatus::value_t compute(raft::StreamingData &dataIn,
                                           raft::StreamingData &bufOut) {
        vertex_iterator vit = g_.find_vertex(vid);
        bufOut.push(vit);
        if (g_.num_vertices() <= ++vid) {
            if (repetition <= ++rep) {
                return raft::kstatus::stop;
            }
            vid = 0;
        }
        return raft::kstatus::proceed;
    }
    virtual bool pop(raft::Task *task, bool dryrun) { return true; }
    virtual bool allocate(raft::Task *task, bool dryrun) { return true; }
#endif
    graph_t& g_;
    uint64_t vid;
    size_t rep;
};

//==============================================================//
void arg_init(argument_parser & arg)
{
    arg.add_arg("maxiter","0","maximum loop iteration (0-unlimited, only set for simulation purpose)");
    arg.add_arg("repetition","1","repetition on the same graph");
    arg.add_arg("runnum","1","run per graph load");
}
//==============================================================//
size_t get_intersect_cnt(vector<size_t>& setA, vector<size_t>& setB)
{
    size_t ret=0;
    vector<uint64_t>::iterator iter1=setA.begin(), iter2=setB.begin();

    while (iter1!=setA.end() && iter2!=setB.end()) 
    {
        if ((*iter1) < (*iter2)) 
            iter1++;
        else if ((*iter1) > (*iter2)) 
            iter2++;
        else
        {
            ret++;
            iter1++;
            iter2++;
        }
    }

    return ret;
}

void tc_init(graph_t& g)
{
    // prepare neighbor set for each vertex
    for (vertex_iterator vit=g.vertices_begin(); vit!=g.vertices_end(); vit++) 
    {
        vit->property().count = 0;
        vector<uint64_t> & cur_set = vit->property().neighbor_set;
        cur_set.reserve(vit->edges_size());
        for (edge_iterator eit=vit->edges_begin();eit!=vit->edges_end();eit++) 
        {
            cur_set.push_back(eit->target());
        }
        std::sort(cur_set.begin(),cur_set.end());
    }
}

size_t triangle_count(graph_t& g, gBenchPerf_event & perf, int perf_group)
{
    perf.open(perf_group);
    perf.start(perf_group);

    size_t ret=0;

    // run triangle count now
    for (vertex_iterator vit=g.vertices_begin(); vit!=g.vertices_end(); vit++) 
    {
        vector<uint64_t> & src_set = vit->property().neighbor_set;

        for (edge_iterator eit=vit->edges_begin();eit!=vit->edges_end();eit++) 
        {
            if (vit->id() > eit->target()) continue; // skip reverse edges
            vertex_iterator vit_targ = g.find_vertex(eit->target());

            vector<uint64_t> & dest_set = vit_targ->property().neighbor_set;
            size_t cnt = get_intersect_cnt(src_set, dest_set);

            vit->property().count += cnt;
            vit_targ->property().count += cnt;
        }
    }

    // tune the per-vertex count
    for (vertex_iterator vit=g.vertices_begin(); vit!=g.vertices_end(); vit++) 
    {
        vit->property().count /= 2;
        ret += vit->property().count;
    }

    ret /= 3;

    perf.stop(perf_group);
    return ret;
}

void gen_workset(graph_t& g, vector<unsigned>& workset, unsigned threadnum)
{
    unsigned chunk = (unsigned)ceil(g.num_edges()/(double)threadnum);
    unsigned last=0, curr=0, th=1;
    workset.clear();
    workset.resize(threadnum+1,0);
    for (vertex_iterator vit=g.vertices_begin(); vit!=g.vertices_end(); vit++) 
    {
        curr += vit->edges_size();
        if ((curr-last)>=chunk)
        {
            last = curr;
            workset[th] = vit->id();
            if (th<threadnum) th++;
        }
    }
    workset[threadnum] = g.num_vertices();
    //for (unsigned i=0;i<=threadnum;i++)
    //    cout<<workset[i]<<" ";
    //cout<<endl;
}

void parallel_tc_init(graph_t& g, unsigned threadnum)
{
    vector<unsigned> ws;
    gen_workset(g, ws, threadnum);

    #pragma omp parallel num_threads(threadnum)
    {
        unsigned tid = omp_get_thread_num();

        // prepare neighbor set for each vertex        
        for (uint64_t vid=ws[tid];vid<ws[tid+1];vid++)
        {
            vertex_iterator vit = g.find_vertex(vid);
            if (vit == g.vertices_end()) continue;

            vit->property().count = 0;
            vector<uint64_t> & cur_set = vit->property().neighbor_set;
            cur_set.reserve(vit->edges_size());
            for (edge_iterator eit=vit->edges_begin();eit!=vit->edges_end();eit++) 
            {
                cur_set.push_back(eit->target());
            }
            std::sort(cur_set.begin(),cur_set.end());
        }
    }
}

void parallel_workset_init(graph_t&g, vector<unsigned>& workset, unsigned threadnum)
{
    vector<unsigned> n_op(g.num_vertices(),0);
    unsigned totalcnt=0;
    vector<unsigned> ws;
    gen_workset(g, ws, threadnum);

    #pragma omp parallel num_threads(threadnum)
    {
        unsigned tid = omp_get_thread_num();
   
        unsigned tot=0; 
        for (uint64_t vid=ws[tid];vid<ws[tid+1];vid++)
        {
            vertex_iterator vit = g.find_vertex(vid);
            if (vit == g.vertices_end()) continue;

            for (edge_iterator eit=vit->edges_begin();eit!=vit->edges_end();eit++) 
            {
                vertex_iterator vit_targ = g.find_vertex(eit->target());
                n_op[vid] += vit->edges_size() + vit_targ->edges_size();
            }
            tot += n_op[vid];
        }
        __sync_fetch_and_add(&(totalcnt),tot);
    }
    workset.clear();
    workset.resize(threadnum+1,0);
    unsigned last=0, cnt=0, th=1;
    unsigned cnt_chunk = (unsigned)ceil(totalcnt/(double)threadnum);
    for (uint64_t vid=0; vid<g.num_vertices();vid++)
    {
        cnt += n_op[vid];
        if ((cnt-last) > cnt_chunk)
        {
            if (th<threadnum) workset[th] = vid;
            th++;
            last = cnt;
        }
    }
    workset[threadnum] = g.num_vertices();
    
}

size_t parallel_triangle_count(graph_t& g, unsigned threadnum,
        vector<unsigned>& workset, gBenchPerf_multi & perf, int perf_group)
{
    // UNUSED(workset);
    // UNUSED(perf);
    // UNUSED(perf_group);
    size_t ret = 0;

    int16_t* cnts = new int16_t[g.num_vertices()];

    workset_kernel workset_k(g, threadnum);
    reduce_kernel reduce_k(cnts, threadnum);
    lookup_kernel lookup_k(g);
    count_kernel count_k;

    raft::DAG dag;

#if RAFTLIB_ORIG
    dag += workset_k <= lookup_k >> count_k >= reduce_k;
#else
    dag += workset_k >> lookup_k * threadnum >> count_k * threadnum >>
        reduce_k;
#endif

    dag.exe<
#if RAFTLIB_ORIG
        partition_dummy,
#if USE_UT or USE_QTHREAD
        pool_schedule,
#else
        simple_schedule,
#endif
#ifdef VL
        vlalloc,
#elif STDALLOC
        stdalloc,
#else
        dynalloc,
#endif
        no_parallel
#else // NOT( RAFTLIB_ORIG )
#if RAFTLIB_MIX
        raft::RuntimeMix
#elif RAFTLIB_ONESHOT
        raft::RuntimeNewBurst
#elif RAFTLIB_CV
        raft::RuntimeFIFOCV
#else
        raft::RuntimeFIFO
#endif
#endif // RAFTLIB_ORIG
            >();

    for (size_t vid = 0; g.num_vertices() > vid; ++vid) {
        cnts[vid] /= 2;
        ret += cnts[vid];
        vertex_iterator vit = g.find_vertex(vid);
        vit->property().count = cnts[vid];
    }

    for (unsigned i = 0; threadnum > i; ++i) {
        perf.stop(i, perf_group);
    }

    ret /= 3;

    return ret;
}

void output(graph_t& g)
{
    cout<<"Triangle Count Results: \n";
    for (vertex_iterator vit=g.vertices_begin(); vit!=g.vertices_end(); vit++) 
    {
        cout<<"== vertex "<<vit->id()<<": count "<<vit->property().count<<endl;
    }
}

void reset_graph(graph_t & g)
{
    vertex_iterator vit;
    for (vit=g.vertices_begin(); vit!=g.vertices_end(); vit++)
    {
        vit->property().count = 0;
    }

}

int main(int argc, char * argv[])
{
    graphBIG::print();
    cout<<"Benchmark: triangle count\n";

    argument_parser arg;
    gBenchPerf_event perf;
    arg_init(arg);
    if (arg.parse(argc,argv,perf,false)==false)
    {
        arg.help();
        return -1;
    }
    string path, separator;
    unsigned arg_run_num;
    arg.get_value("dataset",path);
    arg.get_value("separator",separator);
    arg.get_value("repetition",repetition);
    arg.get_value("runnum",arg_run_num);

    size_t threadnum;
    arg.get_value("threadnum",threadnum);
    arg.get_value("maxiter",maxiter);
#ifdef SIM
    arg.get_value("beginiter",beginiter);
    arg.get_value("enditer",enditer);
#endif

    double t1, t2;
    graph_t graph;

    cout<<"loading data... \n";
    t1 = timer::get_usec();
    string vfile = path + "/vertex.csv";
    string efile = path + "/edge.csv";

#ifndef EDGES_ONLY    
    if (graph.load_csv_vertices(vfile, true, separator, 0) == -1)
        return -1;
    if (graph.load_csv_edges(efile, true, separator, 0, 1) == -1) 
        return -1;
#else
    if (graph.load_csv_edges(efile, true, separator, 0, 1) == -1)
        return -1;
#endif

    uint64_t vertex_num = graph.num_vertices();
    uint64_t edge_num = graph.num_edges();
    t2 = timer::get_usec();
    cout<<"== "<<vertex_num<<" vertices  "<<edge_num<<" edges\n";
#ifndef ENABLE_VERIFY
    cout<<"== time: "<<t2-t1<<" sec\n";
#endif

    cout<<"\npreparing neighbor sets..."<<endl;
    vector<unsigned> workset;
    if (threadnum==1)
        tc_init(graph);
    else
    {
        parallel_tc_init(graph, threadnum);
        cout<<"preparing workset..."<<endl;
        //parallel_workset_init(graph, workset, arguments.threadnum);
        //gen_workset(graph, workset, threadnum);
    }

    if (maxiter != 0) cout<<"\nmax iteration: "<<maxiter;
    cout<<"\ncomputing triangle count..."<<endl;
    size_t tcount;

    gBenchPerf_multi perf_multi(threadnum, perf);
    unsigned run_num = ceil(perf.get_event_cnt() /(double) DEFAULT_PERF_GRP_SZ);
    if (run_num==0) run_num = arg_run_num;
    double elapse_time = 0;
    
#ifndef NOGEM5
    m5_reset_stats(0, 0);
#endif


    for (unsigned i=0;i<run_num;i++)
    {
        t1 = timer::get_usec();

        const uint64_t beg_tsc = rdtsc();
        if (threadnum==1)
            tcount = triangle_count(graph, perf, i);
        else
            tcount = parallel_triangle_count(graph, threadnum, workset, perf_multi, i);
        const uint64_t end_tsc = rdtsc();
        cout << (end_tsc - beg_tsc) << " ticks elapsed\n";

        t2 = timer::get_usec();

        elapse_time += t2 - t1;
        if ((i+1)<run_num) reset_graph(graph);
    }
#ifndef NOGEM5
    m5_dump_reset_stats(0, 0);
#endif

    cout<<"== total triangle count: "<<tcount<<endl;
#ifndef ENABLE_VERIFY
    cout<<"== time: "<<elapse_time/run_num<<" sec\n";
    if (threadnum == 1)
        perf.print();
    else
        perf_multi.print();
#endif

#ifdef ENABLE_OUTPUT
    cout<<endl;
    output(graph);
#endif
    cout<<"==================================================================\n";
    return 0;
}  // end main

