//====== Graph Benchmark Suites ======//
//====== Betweenness Centrality ======//
//
// BC for unweighted graph
// Brandes' algorithm
// Usage: ./bc.exe --dataset <dataset path>

#include "common.h"
#include "def.h"
#include "openG.h"
#include "omp.h"
#include <stack>
#include <queue>
#include <vector>
#include <list>
#include <raft>

#if ! RAFTLIB_ORIG
#include <mutex>
#endif

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

#define MY_INFINITY 0xfff0

using namespace std;

size_t repetition = 0;
size_t maxiter = 0;
size_t beginiter = 0;
size_t enditer = 0;

class vertex_property
{
public:
    vertex_property():BC(0){}
    vertex_property(const vertex_property &other):BC(other.BC){}

#if ! RAFTLIB_ORIG
    mutex mu;
#endif
    double BC;
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
struct reduction_msg
{
    double* pbc;
#if ! RAFTLIB_ORIG
    mutex* pmu;
#endif
    double update;
};

class reduce_kernel : public raft::parallel_k
{
public:
    reduce_kernel(int bc_num) : raft::parallel_k() {
#if RAFTLIB_ORIG
        for (int i = 0; bc_num > i; ++i) {
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
                *msg.pbc += msg.update;
                port.recycle(1);
            }
        }
        return raft::kstatus::proceed;
    }
#else
    virtual raft::kstatus::value_t compute(raft::StreamingData &dataIn,
                                           raft::StreamingData &bufOut) {
        auto& msg(dataIn.template peek<struct reduction_msg>());
        msg.pmu->lock();
        *msg.pbc += msg.update;
        msg.pmu->unlock();
        dataIn.recycle();
        return raft::kstatus::proceed;
    }
    virtual bool pop(raft::Task *task, bool dryrun) { return true; }
    virtual bool allocate(raft::Task *task, bool dryrun) { return true; }
    std::mutex mu;
#endif
};

class bc_kernel : public raft::Kernel
{
    using vertex_list_t = list<size_t>;
public:
    bc_kernel(graph_t& g, bool undirected) :
        raft::Kernel(), g_(g), vnum(g.num_vertices()), directed(!undirected) {
        add_input<size_t>("input"_port);
        add_output<struct reduction_msg>("output"_port);
        normalizer = (directed)? 1.0 : 2.0;
    }
    ~bc_kernel() = default;

#if RAFTLIB_ORIG
    bc_kernel(const bc_kernel& other) :
        raft::Kernel(), g_(other.g_), directed(other.directed) {
        add_input<size_t>("input"_port);
        add_output<struct reduction_msg>("output"_port);
        vnum = g_.num_vertices();
        normalizer = (directed)? 1.0 : 2.0;
    }
    CLONE(); // enable cloning

    virtual raft::kstatus run() { /* } */
#else
    virtual raft::kstatus::value_t compute(raft::StreamingData &dataIn,
                                           raft::StreamingData &bufOut) {
#endif
        vector<vertex_list_t> shortest_path_parents(vnum);
        vector<int16_t> num_of_paths(vnum, 0);
        vector<uint16_t> depth_of_vertices(vnum, MY_INFINITY); // 8 bits signed
        vector<float> centrality_update(vnum, 0);

        stack<size_t> order_seen_stack;
        queue<size_t> BFS_queue;

        size_t vertex_s;
#if RAFTLIB_ORIG
        input["input"_port].pop<size_t>(vertex_s);
#else
        dataIn.pop<size_t>(vertex_s);
#endif

        BFS_queue.push(vertex_s);
        num_of_paths[vertex_s] = 1;
        depth_of_vertices[vertex_s] = 0;

        // BFS traversal
        while (!BFS_queue.empty())
        {
            size_t v = BFS_queue.front();
            BFS_queue.pop();
            order_seen_stack.push(v);

            vertex_iterator vit = g_.find_vertex(v);
            uint16_t newdepth = depth_of_vertices[v]+1;
            for (edge_iterator eit=vit->edges_begin(); eit!= vit->edges_end(); eit++)
            {
                size_t w = eit->target();
                if (depth_of_vertices[w] == MY_INFINITY)
                {
                    BFS_queue.push(w);
                    depth_of_vertices[w] = newdepth;
                }

                if (depth_of_vertices[w] == newdepth)
                {
                    num_of_paths[w] += num_of_paths[v];
                    shortest_path_parents[w].push_back(v);
                }
            }

        }

        // dependency accumulation
        while (!order_seen_stack.empty())
        {
            size_t w = order_seen_stack.top();
            order_seen_stack.pop();

            float coeff = (1+centrality_update[w])/(double)num_of_paths[w];
            vertex_list_t::iterator iter;
            for (iter=shortest_path_parents[w].begin();
                    iter!=shortest_path_parents[w].end(); iter++)
            {
                size_t v=*iter;
                centrality_update[v] += (num_of_paths[v]*coeff);
            }

            if (w!=vertex_s)
            {
                vertex_iterator vit = g_.find_vertex(w);
#if RAFTLIB_ORIG
                output["output"_port].template push<struct reduction_msg>(
                        {&(vit->property().BC),
                         centrality_update[w]/normalizer});
#else
                bufOut.template push<struct reduction_msg>(
                        {&(vit->property().BC),
                         &(vit->property().mu),
                         centrality_update[w]/normalizer});
#endif
            }
        }
        return raft::kstatus::proceed;
    }
#if ! RAFTLIB_ORIG
    virtual bool pop(raft::Task *task, bool dryrun) { return true; }
    virtual bool allocate(raft::Task *task, bool dryrun) { return true; }
#endif
private:
    graph_t& g_;
    size_t vnum;
    bool directed;
    double normalizer;
};

class workset_kernel : public raft::parallel_k
{
public:
    workset_kernel(int vnum, int tc_num) :
        raft::parallel_k(), vid(vnum - 1), max_vid(vnum - 1) {
#if RAFTLIB_ORIG
        for (int i = 0; tc_num > i; ++i) {
            addPortTo<size_t>(output);
        }
#else
        add_output<size_t>("0"_port);
#endif
        rep = 0;
    }
#if RAFTLIB_ORIG
    virtual raft::kstatus run() {
        for (auto& port : output) {
            if (port.space_avail()) {
                port.push(vid);
                if (0 == vid--) {
                    if (repetition <= ++rep) {
                        return raft::kstatus::stop;
                    }
                    vid = max_vid;
                }
            }
        }
        return raft::kstatus::proceed;
    }
#else
    virtual raft::kstatus::value_t compute(raft::StreamingData &dataIn,
                                           raft::StreamingData &bufOut) {
        bufOut.push(vid);
        if (0 == vid--) {
            if (repetition <= ++rep) {
                return raft::kstatus::stop;
            }
            vid = max_vid;
        }
        return raft::kstatus::proceed;
    }
    virtual bool pop(raft::Task *task, bool dryrun) { return true; }
    virtual bool allocate(raft::Task *task, bool dryrun) { return true; }
#endif
    size_t vid;
    size_t rep;
    size_t max_vid;
};

//==============================================================//
void arg_init(argument_parser & arg)
{
    arg.add_arg("undirected","1","graph directness", false);
    arg.add_arg("maxiter","0","maximum loop iteration (0-unlimited, only set for simulation purpose)");
    arg.add_arg("repetition","1","repetition on the same graph");
    arg.add_arg("runnum","1","run per graph load");
}
//==============================================================//

void bc(graph_t& g, bool undirected,
        gBenchPerf_event & perf, int perf_group)
{
    typedef list<size_t> vertex_list_t;
    // initialization
    size_t vnum = g.num_vertices();

    vector<vertex_list_t> shortest_path_parents(vnum);
    vector<size_t> num_of_paths(vnum);
    vector<int8_t> depth_of_vertices(vnum); // 8 bits signed
    vector<double> centrality_update(vnum);

    double normalizer;
    normalizer = (undirected)? 2.0 : 1.0;

    perf.open(perf_group);
    perf.start(perf_group);


    vertex_iterator vit;
    for (vit=g.vertices_begin(); vit!=g.vertices_end(); vit++)
    {
        size_t vertex_s = vit->id();
        stack<size_t> order_seen_stack;
        queue<size_t> BFS_queue;

        BFS_queue.push(vertex_s);

        for (size_t i=0;i<vnum;i++)
        {
            shortest_path_parents[i].clear();

            num_of_paths[i] = (i==vertex_s) ? 1 : 0;
            depth_of_vertices[i] = (i==vertex_s) ? 0: -1;
            centrality_update[i] = 0;
        }

        // BFS traversal
        while (!BFS_queue.empty())
        {
            size_t v = BFS_queue.front();
            BFS_queue.pop();
            order_seen_stack.push(v);

            vertex_iterator vit = g.find_vertex(v);

            for (edge_iterator eit=vit->edges_begin(); eit!= vit->edges_end(); eit++)
            {
                size_t w = eit->target();

                if (depth_of_vertices[w]<0)
                {
                    BFS_queue.push(w);
                    depth_of_vertices[w] = depth_of_vertices[v] + 1;
                }

                if (depth_of_vertices[w] == (depth_of_vertices[v] + 1))
                {
                    num_of_paths[w] += num_of_paths[v];
                    shortest_path_parents[w].push_back(v);
                }
            }

        }

        // dependency accumulation
        while (!order_seen_stack.empty())
        {
            size_t w = order_seen_stack.top();
            order_seen_stack.pop();

            double coeff = (1+centrality_update[w])/(double)num_of_paths[w];
            vertex_list_t::iterator iter;
            for (iter=shortest_path_parents[w].begin();
                  iter!=shortest_path_parents[w].end(); iter++)
            {
                size_t v=*iter;

                centrality_update[v] += (num_of_paths[v]*coeff);
            }

            if (w!=vertex_s)
            {
                vertex_iterator vit = g.find_vertex(w);
                vit->property().BC += centrality_update[w]/normalizer;
            }
        }
    }

    perf.stop(perf_group);

    return;
}

void parallel_bc(graph_t& g, unsigned threadnum, bool undirected,
        gBenchPerf_multi & perf, int perf_group)
{
    workset_kernel workset_k(g.num_vertices(), threadnum);
    reduce_kernel reduce_k(threadnum);
    bc_kernel bc_k(g, undirected);

    raft::DAG dag;

#if RAFTLIB_ORIG
    dag += workset_k <= bc_k >= reduce_k;
#else
    dag += workset_k >> bc_k * threadnum >> reduce_k;
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

    for (unsigned i = 0; threadnum > i; ++i) {
        perf.stop(i, perf_group);
    }
    return;
}
//==============================================================//
void output(graph_t& g)
{
    cout<<"Betweenness Centrality Results: \n";
    vertex_iterator vit;
    for (vit=g.vertices_begin(); vit!=g.vertices_end(); vit++)
    {
        cout<<"== vertex "<<vit->id()<<": "<<vit->property().BC<<"\n";
    }
}
void reset_graph(graph_t & g)
{
    vertex_iterator vit;
    for (vit=g.vertices_begin(); vit!=g.vertices_end(); vit++)
    {
        vit->property().BC = 0;
    }

}

//==============================================================//

int main(int argc, char * argv[])
{
    graphBIG::print();
    cout<<"Benchmark: Betweenness Centrality\n";

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

    bool undirected;
    arg.get_value("undirected",undirected);


    graph_t graph;
    double t1, t2;

    //loading data
    cout<<"loading data... \n";
    if (undirected)
        cout<<"undirected graph\n";
    else
        cout<<"directed graph\n";

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

    size_t vertex_num = graph.num_vertices();
    size_t edge_num = graph.num_edges();
    t2 = timer::get_usec();
    cout<<"== "<<vertex_num<<" vertices  "<<edge_num<<" edges\n";

#ifndef ENABLE_VERIFY
    cout<<"== time: "<<t2-t1<<" sec\n";
#endif

    if (maxiter != 0 && threadnum != 1)
        cout<<"\nenable maxiter: "<<maxiter<<" per thread";
    //processing
    cout<<"\ncomputing BC for all vertices...\n";

    gBenchPerf_multi perf_multi(threadnum, perf);
    unsigned run_num = ceil(perf.get_event_cnt() / (double)DEFAULT_PERF_GRP_SZ);
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
            bc(graph,undirected,perf,i);
        else
            parallel_bc(graph,threadnum,undirected,perf_multi,i);
        const uint64_t end_tsc = rdtsc();
        cout << (end_tsc - beg_tsc) << " ticks elapsed\n";

        t2 = timer::get_usec();
        elapse_time += t2-t1;
        if ((i+1)<run_num) reset_graph(graph);
    }
#ifndef NOGEM5
    m5_dump_reset_stats(0, 0);
#endif
    cout<<"== finish\n";

#ifndef ENABLE_VERIFY
    cout<<"== time: "<<elapse_time/run_num<<" sec\n";
    if (threadnum == 1)
        perf.print();
    else
        perf_multi.print();
#endif

    //print output
#ifdef ENABLE_OUTPUT
    cout<<"\n";
    output(graph);
#endif
    cout<<"=================================================================="<<endl;
    return 0;
}  // end main

