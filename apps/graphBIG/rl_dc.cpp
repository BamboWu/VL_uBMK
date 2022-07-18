//====== Graph Benchmark Suites ======//
//======== Degree Centrality =========//

#include "common.h"
#include "def.h"
#include "openG.h"
#include <math.h>
#include <stack>
#include "omp.h"
#include <raft>

#ifdef SIM
#include "SIM.h"
#endif
#ifdef HMC
#include "HMC.h"
#endif

#ifndef NOGEM5
#include "gem5/m5ops.h"
#endif

using namespace std;
size_t beginiter = 0;
size_t enditer = 0;

class vertex_property
{
public:
    vertex_property():indegree(0),outdegree(0){}

    int16_t indegree;
    int16_t outdegree;
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
    int16_t* pcount; // pointer to vertex_property.indegree
    int16_t cnt; // count of in-coming edges to add to *pcount
};

class reduce_kernel : public raft::parallel_k
{
public:
    reduce_kernel(int dc_num) : raft::parallel_k() {
        for (int i = 0; dc_num > i; ++i) {
            addPortTo<struct reduction_msg>(input);
        }
    }
    virtual raft::kstatus run() {
        for (auto& port : input) {
            if (0 < port.size()) {
                auto& msg(port.template peek<struct reduction_msg>());
                *msg.pcount += msg.cnt;
                port.recycle(1);
            }
        }
        return raft::proceed;
    }
};

class dc_kernel : public raft::kernel
{
public:
    dc_kernel(graph_t& g) : raft::kernel(), g_(g) {
        input.addPort<vertex_iterator>("input");
        output.addPort<struct reduction_msg>("output");
    }
    dc_kernel(const dc_kernel& other) : raft::kernel(), g_(other.g_) {
        input.addPort<vertex_iterator>("input");
        output.addPort<struct reduction_msg>("output");
    }
    ~dc_kernel() = default;
    virtual raft::kstatus run() {
        // run degree count now
        vertex_iterator& vit(input["input"].peek<vertex_iterator>());

        // out degree
        vit->property().outdegree = vit->edges_size();

        for (edge_iterator eit=vit->edges_begin();eit!=vit->edges_end();eit++) 
        {
            vertex_iterator vit_targ = g_.find_vertex(eit->target());
            output["output"].template push<struct reduction_msg>(
                    {&(vit_targ->property().indegree), 1});
        }
        input["input"].recycle();
        return raft::proceed;
    }
    CLONE(); // enable cloning
private:
    graph_t& g_;
};

class workset_kernel : public raft::parallel_k
{
public:
    workset_kernel(graph_t& g, int dc_num) : raft::parallel_k(), g_(g) {
        for (int i = 0; dc_num > i; ++i) {
            addPortTo<vertex_iterator>(output);
        }
        vid = 0;
    }
    virtual raft::kstatus run() {
        for (auto& port : output) {
            if (port.space_avail()) {
                vertex_iterator vit = g_.find_vertex(vid);
                port.push(vit);
                if (g_.num_vertices() <= ++vid) {
                    return raft::stop;
                }
            }
        }
        return raft::proceed;
    }
    graph_t& g_;
    uint64_t vid;
};

//==============================================================//

//==============================================================//
void dc(graph_t& g, gBenchPerf_event & perf, int perf_group) 
{
    perf.open(perf_group);
    perf.start(perf_group);
#ifdef SIM
    SIM_BEGIN(true);
#endif
    vertex_iterator vit;
    for (vit=g.vertices_begin(); vit!=g.vertices_end(); vit++) 
    {
        // out degree
        vit->property().outdegree = vit->edges_size();

        // in degree
        edge_iterator eit;
        for (eit=vit->edges_begin(); eit!=vit->edges_end(); eit++) 
        {
            vertex_iterator targ = g.find_vertex(eit->target());
            (targ->property().indegree)++;
        }
    }
#ifdef SIM
    SIM_END(true);
#endif
    perf.stop(perf_group);
}// end dc
void parallel_dc(graph_t& g, unsigned threadnum, gBenchPerf_multi & perf, int perf_group)
{
    // UNUSED(perf);
    // UNUSED(perf_group);
    workset_kernel workset_k(g, threadnum);
    reduce_kernel reduce_k(threadnum);
    dc_kernel dc_k(g);

    raft::map m;

    m += workset_k <= dc_k >= reduce_k;

    m.exe();

    for (unsigned i = 0; threadnum > i; ++i) {
        perf.stop(i, perf_group);
    }
}
void degree_analyze(graph_t& g, 
                    uint64_t& indegree_max, uint64_t& indegree_min,
                    uint64_t& outdegree_max, uint64_t& outdegree_min)
{
    vertex_iterator vit;
    indegree_max=outdegree_max=0;
    indegree_min=outdegree_min=numeric_limits<uint64_t>::max();


    for (vit=g.vertices_begin(); vit!=g.vertices_end(); vit++) 
    {
        if (indegree_max < (uint64_t)vit->property().indegree)
            indegree_max = (uint64_t)vit->property().indegree;

        if (outdegree_max < (uint64_t)vit->property().outdegree)
            outdegree_max = (uint64_t)vit->property().outdegree;

        if (indegree_min > (uint64_t)vit->property().indegree)
            indegree_min = (uint64_t)vit->property().indegree;

        if (outdegree_min > (uint64_t)vit->property().outdegree)
            outdegree_min = (uint64_t)vit->property().outdegree;
    }

    return;
}
void output(graph_t& g) 
{
    cout<<"Degree Centrality Results: \n";
    vertex_iterator vit;
    for (vit=g.vertices_begin(); vit!=g.vertices_end(); vit++)
    {
        cout<<"== vertex "<<vit->id()<<": in-"<<vit->property().indegree
            <<" out-"<<vit->property().outdegree<<"\n";
    }
}
void reset_graph(graph_t & g)
{
    vertex_iterator vit;
    for (vit=g.vertices_begin(); vit!=g.vertices_end(); vit++)
    {
        vit->property().indegree = 0;
        vit->property().outdegree = 0;
    }
}


int main(int argc, char * argv[])
{
    graphBIG::print();
    cout<<"Benchmark: Degree Centrality\n";

    argument_parser arg;
    gBenchPerf_event perf;
    if (arg.parse(argc,argv,perf,false)==false)
    {
        arg.help();
        return -1;
    }
    string path, separator;
    arg.get_value("dataset",path);
    arg.get_value("separator",separator);

    size_t threadnum;
    arg.get_value("threadnum",threadnum);
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
   
    size_t vertex_num = graph.num_vertices();
    size_t edge_num = graph.num_edges();
    t2 = timer::get_usec();
    cout<<"== "<<vertex_num<<" vertices  "<<edge_num<<" edges\n";
#ifndef ENABLE_VERIFY
    cout<<"== time: "<<t2-t1<<" sec\n";
#endif

    cout<<"\ncomputing DC for all vertices...\n";

    gBenchPerf_multi perf_multi(threadnum, perf);
    unsigned run_num = ceil(perf.get_event_cnt() / (double)DEFAULT_PERF_GRP_SZ);
    if (run_num==0) run_num = 1;
    double elapse_time = 0;
    
#ifndef NOGEM5
    m5_reset_stats(0, 0);
#endif
    for (unsigned i=0;i<run_num;i++)
    {
        // Degree Centrality
        t1 = timer::get_usec();
        
        if (threadnum==1)
            dc(graph, perf, i);
        else
            parallel_dc(graph, threadnum, perf_multi, i);

        t2 = timer::get_usec();
        elapse_time += t2-t1;
        if ((i+1)<run_num) reset_graph(graph);
    }
#ifndef NOGEM5
    m5_dump_reset_stats(0, 0);
#endif

    uint64_t indegree_max, indegree_min, outdegree_max, outdegree_min;
    degree_analyze(graph, indegree_max, indegree_min, outdegree_max, outdegree_min);

    cout<<"DC finish: \n";
    cout<<"== inDegree[Max-"<<indegree_max<<" Min-"<<indegree_min
        <<"]  outDegree[Max-"<<outdegree_max<<" Min-"<<outdegree_min
        <<"]"<<endl;
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

