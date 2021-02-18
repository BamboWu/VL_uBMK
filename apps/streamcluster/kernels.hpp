/*
 * Copyright (C) 2008 Princeton University
 * All rights reserved.
 * Authors: Jia Deng, Gilberto Contreras
 *
 * streamcluster - Online clustering algorithm
 *
 */

/**
 * kernels.hpp -
 * @author: James Wood, Qinzhe Wu
 * @version: Mon January 18 16:25:00 2021
 *
 * Copyright 2021 Jonathan Beard
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _STREAMCLUSTER_KERNELS_HPP__
#define _STREAMCLUSTER_KERNELS_HPP__

#include "raftlib_src.hpp"
#include <raft>
#include <unistd.h>

struct SearchTask {
    uint8_t id;
    SearchTask(uint64_t _id = 0) : id(_id) {}
};

struct RangedTask {
    long k1, k2;
    RangedTask(long _k1 = 0, long _k2 = 0) : k1(_k1), k2(_k2) {}
};

struct RangedRes {
    long k1, k2;
    double local;
    RangedRes(long _k1 = 0, long _k2 = 0, double _local = 0.0) :
        k1(_k1), k2(_k2), local(_local) {}
};

struct ZKPair {
    double z;
    long k;
    ZKPair(double _z = 0.0, long _k = 0) : z(_z), k(_k) {}
};

typedef struct ZKPair PSpeedyTask;

struct CostKPair {
    double cost;
    long k;
    CostKPair(double _cost = 0.0, long _k = 0) : cost(_cost), k(_k) {}
};

typedef struct CostKPair PFLRes;
typedef struct CostKPair PGainRes;
typedef struct ZKCostTuple PSpeedyRes;

struct ZKCostTuple {
    double z;
    long k;
    double cost;
    ZKCostTuple(double _z = 0.0, long _k = 0, double _cost = 0.0) :
        z(_z), k(_k), cost(_cost) {}
};

typedef struct ZKCostTuple PSpeedyLoopRes;

struct ZKIdxTuple {
    double z;
    long k;
    long idx;
    ZKIdxTuple(double _z = 0.0, long _k = 0, long _idx = 0) :
        z(_z), k(_k), idx(_idx) {}
};

typedef struct ZKIdxTuple PGainTask;

struct K1K2LocalTuple {
    long k1, k2, local;
    K1K2LocalTuple(long _k1 = 0, long _k2 = 0, long _local = 0) :
        k1(_k1), k2(_k2), local(_local) {}
};

typedef struct K1K2LocalTuple CenterUpdateTask;
typedef struct K1K2LocalTuple CenterOffsetTask;
typedef struct K1K2LocalTuple CenterCountRes;

struct K1K2IdxLower {
    long k1, k2;
    long idx;
    double *lower_tab;
    K1K2IdxLower(long _k1 = 0, long _k2 = 0, long _idx = 0,
                 double *_lower_tab = nullptr) :
        k1(_k1), k2(_k2), idx(_idx), lower_tab(_lower_tab) {}
};

typedef struct K1K2IdxLower SwitchCostCalcTask;
typedef struct K1K2IdxLower CommitSwitchTask;

struct LowerSumupTask {
    long k1, k2;
    long stride;
    double z;
    double *lower_tab;
    LowerSumupTask(long _k1 = 0, long _k2 = 0, long _stride = 0,
                   double _z = 0.0, double *_lower_tab = nullptr) :
        k1(_k1), k2(_k2), stride(_stride), z(_z), lower_tab(_lower_tab) {}
};

struct LowerSumupRes {
    long nclose;
    double lowersum;
    LowerSumupRes(long _nclose = 0, double _lowersum = 0.0) :
        nclose(_nclose), lowersum(_lowersum) {}
};

struct ZKCostNFeasible {
    double z;
    long k;
    double cost;
    int nfeasible;
    ZKCostNFeasible(double _z = 0.0, long _k = 0, double _cost = 0.0,
                    int _nfeasible = 0) :
        z(_z), k(_k), cost(_cost), nfeasible(_nfeasible) {}
};

typedef struct ZKCostNFeasible FeasibleRes;
typedef struct ZKCostNFeasible PFLTask;

struct ZKCostIdx {
    double z;
    long k;
    double cost;
    long idx;
    ZKCostIdx(double _z = 0.0, long _k = 0, double _cost = 0.0,
              long _idx = 0) :
        z(_z), k(_k), cost(_cost), idx(_idx) {}
};

class SourceKernel : public raft::kernel {

  public:
    SourceKernel() : raft::kernel() {
        output.addPort<int>("output");
    }
    virtual raft::kstatus run() {
        std::cout << "starts\n";
        output["output"].push<int>(0);
        sleep(10);
        return raft::stop;
    }

};

class SinkKernel : public raft::kernel {

  public:
    SinkKernel() : raft::kernel() {
        input.addPort<int>("input");
    }
    virtual raft::kstatus run() {
        int tmp;
        input["input"].pop(tmp);
        std::cout << "done\n";
        return raft::stop;
    }

};


/**
 * Manages streamcluster execution.
 */
class MasterKernel : public raft::kernel {

private:
    PStream* stream;
    Points *points;
    float *block;
    int dim;
    long chunk_size;
    bool waiting;
    uint8_t task_id;

public:
    MasterKernel(PStream* _stream, Points *_points, float *_block, int _dim,
                 long _chunk_size) :
        raft::kernel(), stream(_stream), points(_points), block(_block),
        dim(_dim), chunk_size(_chunk_size) {
        // Create ports
        input.addPort<int>("from_source"); // dummy
        input.addPort<uint8_t>("from_copycenters"); // task_id
        output.addPort<uint8_t>("to_search"); // task_id
        output.addPort<int>("to_sink"); // dummy
        task_id = 0;
        waiting = false;
#ifdef DBG
        std::cout << __func__ << " " << this->get_id() << std::endl;
#endif
    }

    virtual raft::kstatus run() {
        size_t numRead = 0;

        if (waiting) {
            uint8_t tmp;
            input["from_copycenters"].pop(tmp);
            waiting = false;
            if (stream->feof()) {
                output["to_sink"].push<int>(0);
                return raft::stop;
            }
        } else {
            int tmp;
            input["from_source"].pop(tmp);
        }

        // Get the number of points to operate on
        numRead = stream->read(block, dim, chunk_size);
        std::cout << "Master reads " << numRead << std::endl;
        points->num = numRead;

        // Error checking
        if (stream->ferror() ||
            numRead < ((unsigned int) chunk_size && !stream->feof())) {
            std::cerr << "Error reading data!" << std::endl;
            exit(EXIT_FAILURE);
        }

        // Push our output data
        output["to_search"].push<uint8_t>(task_id++);
        waiting = true;

        return raft::proceed;
    }
};


class LocalSearchKernel : public raft::kernel {

public:
    LocalSearchKernel() : raft::kernel() {
        // Create ports
        input.addPort<uint8_t>("from_master"); // task_id
        input.addPort<uint8_t>("from_pkmedian"); // signal
        output.addPort<uint8_t>("to_pkmedian"); // task_id
        output.addPort<uint8_t>("to_contcenters"); // task_id
#ifdef DBG
        std::cout << __func__ << " " << this->get_id() << std::endl;
#endif
    }
    virtual raft::kstatus run() {
        uint8_t &task0( input["from_master"].peek<uint8_t>() );
        output["to_pkmedian"].push<uint8_t>(task0);
        input["from_master"].recycle();

        uint8_t &task1( input["from_pkmedian"].peek<uint8_t>() );
        output["to_contcenters"].push<uint8_t>(task1);
        input["from_pkmedian"].recycle();

        return raft::proceed;
    }
};


class PKMedianInitKernel : public raft::kernel {
    Points *points;
    int nshards;

public:
    PKMedianInitKernel(Points *_points, int _nshards) : raft::kernel(),
        points(_points), nshards(_nshards) {
        // Create ports
        input.addPort<uint8_t>("from_search"); // signal
        input.addPort<double>("from_worker"); // local cost
        output.addPort<RangedTask>("to_worker"); // k1, k2
        output.addPort<double>("to_pspeedy"); // hiz
#ifdef DBG
        std::cout << __func__ << " " << this->get_id() << std::endl;
#endif
    }
    virtual raft::kstatus run() {
        uint8_t sig;
        input["from_search"].pop<uint8_t>(sig);
        long stride = points->num / nshards;
        long remain = points->num % nshards;
        // Send tasks to workers
        for (int i = 0; remain > i; ++i) {
            RangedTask &task_tmp(
                    output["to_worker"].allocate<RangedTask>() );
            task_tmp.k1 = i * stride + i;
            task_tmp.k2 = task_tmp.k1 + stride + 1;
            output["to_worker"].send();
        }
        for (int i = remain; nshards > i; ++i) {
            RangedTask &task_tmp(
                    output["to_worker"].allocate<RangedTask>() );
            task_tmp.k1 = i * stride + remain;
            task_tmp.k2 = task_tmp.k1 + stride;
            output["to_worker"].send();
        }

        // Sum up local costs
        double hiz = 0.0;
        for (int i = 0; nshards > i; ++i) {
            double tmp;
            input["from_worker"].pop<double>(tmp);
            hiz += tmp;
        }

        // Shuffle points
        shuffle(points);

        output["to_pspeedy"].push<double>(hiz);
        //input["from_search"].recycle();

        return raft::proceed;
    }
};


class CostTo0Kernel : public raft::kernel {
    Points *points;

public:
    CostTo0Kernel(Points *_points) : raft::kernel(), points(_points) {
        // Create ports
        input.addPort<RangedTask>("from_pkmedian"); // k1, k2
        output.addPort<double>("to_pkmedian"); // local cost
#ifdef DBG
        std::cout << __func__ << " " << this->get_id() << std::endl;
#endif
    }
    virtual raft::kstatus run() {
        double local_cost = 0.0;
        RangedTask &task( input["from_pkmedian"].peek<RangedTask>() );
        for (long i = task.k1; task.k2 > i; ++i) {
            local_cost += dist(points->p[i], points->p[0], points->dim) *
                points->p[i].weight;
        }
        output["to_pkmedian"].push<double>(local_cost);
        input["from_pkmedian"].recycle();

        return raft::proceed;
    }
};


class PKMedianPSpeedyLoopKernel : public raft::kernel {
    Points *points;
    long kmin, kmax;

public:
    PKMedianPSpeedyLoopKernel(Points *_points, long _kmin, long _kmax) :
        raft::kernel(), points(_points), kmin(_kmin), kmax(_kmax) {
        // Create ports
        input.addPort<double>("from_init"); // hiz
        input.addPort<PSpeedyRes>("from_pspeedy"); // cost, k, z
        output.addPort<PSpeedyTask>("to_pspeedy"); // z, k
        output.addPort<PSpeedyLoopRes>("to_selectfeasible"); // z, k
#ifdef DBG
        std::cout << __func__ << " " << this->get_id() << std::endl;
#endif
    }
    virtual raft::kstatus run() {
        double hiz, loz, z, cost;
        long k = 1;

        input["from_init"].pop<double>(hiz);
        loz = 0.0;
        z = (hiz + loz) / 2.0;

        //shuffle(points); // no need as shuffled at the end of pkmedian_init
        int pspeedy_cnt = 0;
        do {
            // calling pspeedy()
            PSpeedyTask &task_tmp(
                    output["to_pspeedy"].allocate<PSpeedyTask>() );
            task_tmp.z = z;
            task_tmp.k = k;
            output["to_pspeedy"].send();
            PSpeedyRes &task_res( input["from_pspeedy"].peek<PSpeedyRes>() );
            cost = task_res.cost;
            k = task_res.k;
            input["from_pspeedy"].recycle();
            pspeedy_cnt++;
        } while ((k < kmin) && (pspeedy_cnt < SP));

        while (k < kmin) {
            if (pspeedy_cnt >= SP) { // no enough centers, assume z is too high
                hiz = z;
                z = (hiz + loz) / 2.0;
                pspeedy_cnt = 0;
            }
            shuffle(points);
            // calling pspeedy()
            PSpeedyTask &task_tmp(
                    output["to_pspeedy"].allocate<PSpeedyTask>() );
            task_tmp.z = z;
            task_tmp.k = k;
            output["to_pspeedy"].send();
            PSpeedyRes &task_res( input["from_pspeedy"].peek<PSpeedyRes>() );
            cost = task_res.cost;
            k = task_res.k;
            input["from_pspeedy"].recycle();
            pspeedy_cnt++;
        }

        PSpeedyLoopRes &res(
                output["to_selectfeasible"].allocate<PSpeedyLoopRes>() );
        res.k = k;
        res.z = z;
        res.cost = cost;
        output["to_selectfeasible"].send();

        return raft::proceed;
    }
};


class PSpeedyKernel : public raft::kernel {
    Points *points;
    int nshards;

public:
    PSpeedyKernel(Points *_points, int _nshards) :
        raft::kernel(), points(_points), nshards(_nshards) {
        // Create ports
        input.addPort<PSpeedyTask>("from_pkmedian"); // z, k
        input.addPort<uint8_t>("from_centerTo0"); // signal
        input.addPort<uint8_t>("from_centerUpdate"); // signal
        input.addPort<double>("from_costSumup"); // local cost
        output.addPort<RangedTask>("to_centerTo0"); // k1, k2
        output.addPort<CenterUpdateTask>("to_centerUpdate"); // k1, k2, open
        output.addPort<RangedTask>("to_costSumup"); // k1, k2
        output.addPort<PSpeedyRes>("to_pkmedian"); // cost, k
#ifdef DBG
        std::cout << __func__ << " " << this->get_id() << std::endl;
#endif
    }
    virtual raft::kstatus run() {
        PSpeedyTask &task0( input["from_pkmedian"].peek<PSpeedyTask>() );
        double z = task0.z;
        long k = task0.k;
        long stride = points->num / nshards;
        long remain = points->num % nshards;
        // Set each point to center Point 0
        for (int i = 0; remain > i; ++i) {
            RangedTask &task_tmp(
                    output["to_centerTo0"].allocate<RangedTask>() );
            task_tmp.k1 = i * stride + i;
            task_tmp.k2 = task_tmp.k1 + stride + 1;
            output["to_centerTo0"].send();
        }
        for (int i = remain; nshards > i; ++i) {
            RangedTask &task_tmp(
                    output["to_centerTo0"].allocate<RangedTask>() );
            task_tmp.k1 = i * stride + remain;
            task_tmp.k2 = task_tmp.k1 + stride;
            output["to_centerTo0"].send();
        }

        // Wait for all points to be centered to 0
        for (int i = 0; nshards > i; ++i) {
            uint8_t sig;
            input["from_centerTo0"].pop<uint8_t>(sig);
        }

        // Randomly pick points to open and update centers
        for (long i = 1; points->num > i; ++i) {
            bool to_open =
                ((float)lrand48() / (float)INT_MAX) < (points->p[i].cost / z);
            if (!to_open) {
                continue;
            }
            for (int j = 0; remain > j; ++j) {
                CenterUpdateTask &task_tmp(
                        output["to_centerUpdate"].allocate<CenterUpdateTask>()
                        );
                task_tmp.k1 = j * stride + j;
                task_tmp.k2 = task_tmp.k1 + stride + 1;
                task_tmp.local = i;
                output["to_centerUpdate"].send();
            }
            for (int j = remain; nshards > j; ++j) {
                CenterUpdateTask &task_tmp(
                        output["to_centerUpdate"].allocate<CenterUpdateTask>()
                        );
                task_tmp.k1 = j * stride + remain;
                task_tmp.k2 = task_tmp.k1 + stride;
                task_tmp.local = i;
                output["to_centerUpdate"].send();
            }
            k++;
            for (int j = 0; nshards > j; ++j) {
                uint8_t sig;
                input["from_centerUpdate"].pop<uint8_t>(sig);
            }
        }

        // Sumup cost locally first
        for (int i = 0; remain > i; ++i) {
            RangedTask &task_tmp(
                    output["to_costSumup"].allocate<RangedTask>() );
            task_tmp.k1 = i * stride + i;
            task_tmp.k2 = task_tmp.k1 + stride + 1;
            output["to_costSumup"].send();
        }
        for (int i = remain; nshards > i; ++i) {
            RangedTask &task_tmp(
                    output["to_costSumup"].allocate<RangedTask>() );
            task_tmp.k1 = i * stride + remain;
            task_tmp.k2 = task_tmp.k1 + stride;
            output["to_costSumup"].send();
        }

        // Accumulate local cost
        double cost = 0;
        for (int i = 0; nshards > i; ++i) {
            double tmp;
            input["from_costSumup"].pop<double>(tmp);
            cost += tmp;
        }

        PSpeedyLoopRes &task_res(
                output["to_pkmedian"].allocate<PSpeedyLoopRes>() );
        task_res.k = k;
        task_res.z = z;
        task_res.cost = cost;
        output["to_pkmedian"].send();

        input["from_pkmedian"].recycle();

        return raft::proceed;
    }
};


class CenterTo0Kernel : public raft::kernel {
    Points *points;

public:
    CenterTo0Kernel(Points *_points) : raft::kernel(), points(_points) {
        // Create ports
        input.addPort<RangedTask>("from_pspeedy"); // k1, k2
        output.addPort<uint8_t>("to_pspeedy"); // signal
#ifdef DBG
        std::cout << __func__ << " " << this->get_id() << std::endl;
#endif
    }
    virtual raft::kstatus run() {
        RangedTask &task( input["from_pspeedy"].peek<RangedTask>() );
        for (long i = task.k1; task.k2 > i; ++i) {
            points->p[i].assign = 0;
            points->p[i].cost = dist(points->p[i], points->p[0], points->dim) *
                points->p[i].weight;
        }
        output["to_pspeedy"].push<uint8_t>(0);
        input["from_pspeedy"].recycle();

        return raft::proceed;
    }
};


class CenterUpdateKernel : public raft::kernel {
    Points *points;

public:
    CenterUpdateKernel(Points *_points) : raft::kernel(), points(_points) {
        // Create ports
        input.addPort<CenterUpdateTask>("from_pspeedy"); // k1, k2, open
        output.addPort<uint8_t>("to_pspeedy"); // signal
#ifdef DBG
        std::cout << __func__ << " " << this->get_id() << std::endl;
#endif
    }
    virtual raft::kstatus run() {
        CenterUpdateTask &task(
                input["from_pspeedy"].peek<CenterUpdateTask>() );
        for (long j = task.k1; task.k2 > j; ++j) {
            float distance = dist(points->p[task.local], points->p[j],
                                  points->dim);
            if (distance * points->p[j].weight < points->p[j].cost) {
                points->p[j].assign = task.local;
                points->p[j].cost = distance * points->p[j].weight;
            }
        }
        output["to_pspeedy"].push<uint8_t>(0);
        input["from_pspeedy"].recycle();

        return raft::proceed;
    }
};


class CostSumupKernel : public raft::kernel {
    Points *points;

public:
    CostSumupKernel(Points *_points) : raft::kernel(), points(_points) {
        // Create ports
        input.addPort<RangedTask>("from_pspeedy"); // k1, k2
        output.addPort<double>("to_pspeedy"); // cost
#ifdef DBG
        std::cout << __func__ << " " << this->get_id() << std::endl;
#endif
    }
    virtual raft::kstatus run() {
        RangedTask &task( input["from_pspeedy"].peek<RangedTask>() );
        double cost = 0.0;
        for (long i = task.k1; task.k2 > i; ++i) {
            cost += points->p[i].cost;
        }
        output["to_pspeedy"].push<double>(cost);
        input["from_pspeedy"].recycle();

        return raft::proceed;
    }
};


class SelectFeasibleFastKernel : public raft::kernel {
    Points *points;
    int *feasible;
    int maxfeasible;
    float *accumweight;

public:
    SelectFeasibleFastKernel(Points *_points, int *_feasible,
                             int _maxfeasible, long chunksize) :
        raft::kernel(), points(_points), feasible(_feasible),
        maxfeasible(_maxfeasible) {
        // Create ports
        input.addPort<PSpeedyLoopRes>("from_pkmedian"); // k, z
        output.addPort<FeasibleRes>("to_pkmedian"); // k, z, nfeasible
        // Allocate local buffer for accumulated weights
        accumweight = (float*)malloc(sizeof(float) * chunksize);
#ifdef DBG
        std::cout << __func__ << " " << this->get_id() << std::endl;
#endif
    }
    virtual raft::kstatus run() {
        PSpeedyLoopRes &kzcost( input["from_pkmedian"].peek<PSpeedyRes>() );
        FeasibleRes &res( output["to_pkmedian"].allocate<FeasibleRes>() );
        res.k = kzcost.k;
        res.z = kzcost.z;
        res.cost = kzcost.cost;
        input["from_pkmedian"].recycle();

        if (maxfeasible >= points->num) { // simply let all be feasible
            for (long i = 0; points->num > i; ++i) {
                feasible[i] = i;
            }
            res.nfeasible = points->num;
            output["to_pkmedian"].send();
            return raft::proceed;
        }

        accumweight[0] = points->p[0].weight;
        for (long i = 1; points->num > i; ++i) {
            accumweight[i] = accumweight[i - 1] + points->p[i].weight;
        }
        float totalweight = accumweight[points->num - 1];

        long l, r;
        for (int i = 0; maxfeasible > i; ++i) {
            const float w = (lrand48() / (float)INT_MAX) * totalweight;
            // binary search
            l = 0;
            r = points->num - 1;
            if (w < accumweight[0]) {
                feasible[i] = 0;
                continue;
            }
            while (l + 1 < r) {
                long m = (l + r) / 2;
                if (w < accumweight[m]) {
                    r = m;
                } else {
                    l = m;
                }
            }
            feasible[i] = r;
        }

        res.nfeasible = maxfeasible;
        output["to_pkmedian"].send();
        return raft::proceed;
    }
};


class WeightSumupKernel : public raft::kernel {
    Points *points;
    float *accumweight;

public:
    WeightSumupKernel(Points *_points, float *_accumweight) :
        raft::kernel(), points(_points), accumweight(_accumweight) {
        // Create ports
        input.addPort<RangedTask>("from_selectfeasible"); // k1, k2
        output.addPort<RangedRes>("to_selectfeasible"); // k1, k2, local
#ifdef DBG
        std::cout << __func__ << " " << this->get_id() << std::endl;
#endif
    }
    virtual raft::kstatus run() {
        RangedTask &task( input["from_selectfeasible"].peek<RangedTask>() );
        double weight = 0.0;
        for (long i = task.k1; task.k2 > i; ++i) {
            accumweight[i] = weight;
            weight += points->p[i].weight;
        }
        RangedRes &res( output["to_selectfeasible"].allocate<RangedRes>() );
        res.k1 = task.k1;
        //res.k2 = task.k2; // not used by caller kernel
        res.local = weight;
        output["to_selectfeasible"].send();
        input["from_selectfeasible"].recycle();
        return raft::proceed;
    }
};


class WeightOffsetKernel : public raft::kernel {
    Points *points;

public:
    WeightOffsetKernel(Points *_points) : raft::kernel(), points(_points) {
        // Create ports
        input.addPort<RangedRes>("from_selectfeasible"); // k1, k2, prefixsum
        output.addPort<uint8_t>("to_selectfeasible"); // signal
#ifdef DBG
        std::cout << __func__ << " " << this->get_id() << std::endl;
#endif
    }
    virtual raft::kstatus run() {
        RangedRes &task( input["from_selectfeasible"].peek<RangedRes>() );
        for (long i = task.k1; task.k2 > i; ++i) {
            points->p[i].weight += task.local;
        }
        output["to_selectfeasible"].push<uint8_t>(0);
        input["from_selectfeasible"].recycle();

        return raft::proceed;
    }
};


class PKMedianPFLLoopKernel : public raft::kernel {
    Points *points;
    bool *is_center;
    long kmin, kmax;

public:
    PKMedianPFLLoopKernel(Points *_points, bool *_is_center,
                          long _kmin, long _kmax) :
        raft::kernel(), points(_points), is_center(_is_center),
        kmin(_kmin), kmax(_kmax) {
        // Create ports
        input.addPort<FeasibleRes>("from_selectfeasible");
        // k, z, cost, nfeasible
        input.addPort<PFLRes>("from_pFL"); // cost, k
        output.addPort<PFLTask>("to_pFL"); // k, z, cost, nfeasible
        output.addPort<uint8_t>("to_search"); // signal
#ifdef DBG
        std::cout << __func__ << " " << this->get_id() << std::endl;
#endif
    }
    virtual raft::kstatus run() {
        FeasibleRes &task( input["from_selectfeasible"].peek<FeasibleRes>() );

        double z = task.z;
        double cost = task.cost;
        long k = task.k;
        int nfeasible = task.nfeasible;
        double loz = 0.0;
        double hiz = z * 2.0;

        // Initializing is_center
        memset((void*)is_center, 0, points->num * sizeof(bool));
        for (int i = 0; points->num > i; ++i) {
            is_center[points->p[i].assign] = true;
        }

        while (1) {
            // launching pFL()
            PFLTask &task_tmp( output["to_pFL"].allocate<PFLTask>() );
            task_tmp.z = z;
            task_tmp.k = k;
            task_tmp.cost = cost;
            task_tmp.nfeasible = nfeasible;
            output["to_pFL"].send();

            // getting updated cost from pFL()
            PFLRes &res_tmp( input["from_pFL"].peek<PFLRes>() );
            cost = res_tmp.cost;
            k = res_tmp.k;
            input["from_pFL"].recycle();

            // update facility cost based on k
            if (kmax < k) {
                loz = z; z = (hiz + loz) / 2.0;
                cost += (z - loz) * k;
            } else if (kmin > k) {
                hiz = z; z = (hiz + loz) / 2.0;
                cost += (z - hiz) * k;
            }
            if (((kmin <= k) && (k <= kmax)) || ((0.999 * hiz) <= loz)) {
#ifdef DBG
                std::cout << "exit pFL loop with k = " << k << std::endl;
#endif
                break;
#ifdef DBG
            } else {
                std::cout << "repeat pFL loop with k = " << k << std::endl;
#endif
            }
        }

        input["from_selectfeasible"].recycle();
        output["to_search"].push<uint8_t>(0);

        return raft::proceed;
    }
};


class PFLKernel : public raft::kernel {
    int *feasible;
    int iter; // iterations to do every invocation of pFL()

public:
    PFLKernel(int *_feasible, int _iter) :
        raft::kernel(), feasible(_feasible), iter(_iter) {
        // Create ports
        input.addPort<PFLTask>("from_pkmedian"); // k, z, cost, nfeasible
        input.addPort<PGainRes>("from_pgain"); // k, change
        output.addPort<PGainTask>("to_pgain"); // k, z, idx
        output.addPort<PFLRes>("to_pkmedian"); // k, cost
#ifdef DBG
        std::cout << __func__ << " " << this->get_id() << std::endl;
#endif
    }
    virtual raft::kstatus run() {
        PFLTask &task( input["from_pkmedian"].peek<PFLTask>() );
        long k = task.k;
        double z = task.z;
        double cost = task.cost;
        int nfeasible = task.nfeasible;

        double change = cost;
        while ((1.0 * 0.1) < (change / cost)) {
            change = 0.0;
            intshuffle(feasible, nfeasible);
            for (int i = 0; iter > i; ++i) {
                // calling pgain() with one of the feasibles
                PGainTask &task_tmp(
                        output["to_pgain"].allocate<PGainTask>() );
                task_tmp.k = k;
                task_tmp.z = z;
                task_tmp.idx = feasible[i % nfeasible];
                output["to_pgain"].send();

                // waiting pgain() return cost change
                PGainRes &task_res( input["from_pgain"].peek<PGainRes>() );
                k = task_res.k;
                change += task_res.cost;
                input["from_pgain"].recycle();
            }
            cost -= change;
#ifdef DBG
            std::cout << "change/cost = " << change << "/" << cost << " = " <<
                (change / cost) << " k = " << k << std::endl;
#endif
        }
        PFLRes &res( output["to_pkmedian"].allocate<PFLRes>() );
        res.k = k;
        res.cost = cost;
        output["to_pkmedian"].send();
        input["from_pkmedian"].recycle();

        return raft::proceed;
    }
};


class PGainKernel : public raft::kernel {
    Points *points;
    int nshards;
    bool *is_center;
    long *local_cnts;

public:
    PGainKernel(Points *_points, int _nshards, bool *_is_center) :
        raft::kernel(), points(_points), nshards(_nshards),
        is_center(_is_center) {
        // Create ports
        input.addPort<PGainTask>("from_pFL"); // k, z, idx
        input.addPort<CenterCountRes>("from_centerCount"); // k1, k2, cnt
        input.addPort<uint8_t>("from_centerOffset"); // signal
        input.addPort<double>("from_switchCostCalc"); // cost
        input.addPort<LowerSumupRes>("from_lowerSumup"); // nclose, lowersum
        input.addPort<uint8_t>("from_commitSwitch"); // signal
        output.addPort<RangedTask>("to_centerCount"); // k1, k2
        output.addPort<CenterOffsetTask>("to_centerOffset"); // k1, k2, prefix
        output.addPort<SwitchCostCalcTask>("to_switchCostCalc");
        // k1, k2, idx, lower_tab
        output.addPort<LowerSumupTask>("to_lowerSumup");
        // k1, k2, lower_stride, z, lower_tab
        output.addPort<CommitSwitchTask>("to_commitSwitch");
        // k1, k2, lower_tab
        output.addPort<PGainRes>("to_pFL"); // cost, k
        // Allocate buffer
        local_cnts = (long*)malloc((nshards + 1) * sizeof(long));
#ifdef DBG
        std::cout << __func__ << " " << this->get_id() << std::endl;
#endif
    }
    virtual raft::kstatus run() {
        PGainTask &task( input["from_pFL"].peek<PGainTask>() );
        long k = task.k;
        double z = task.z;
        long idx = task.idx;

        long stride = points->num / nshards;
        long remain = points->num % nshards;

        // First count centers in each region
        for (int i = 0; remain > i; ++i) {
            RangedTask &task_tmp(
                    output["to_centerCount"].allocate<RangedTask>() );
            task_tmp.k1 = i * remain + i;
            task_tmp.k2 = task_tmp.k1 + stride + 1;
            output["to_centerCount"].send();
        }
        for (int i = remain; nshards > i; ++i) {
            RangedTask &task_tmp(
                    output["to_centerCount"].allocate<RangedTask>() );
            task_tmp.k1 = i * remain + remain;
            task_tmp.k2 = task_tmp.k1 + stride;
            output["to_centerCount"].send();
        }
        for (int i = 0; nshards > i; ++i) {
            CenterCountRes &res_tmp(
                    input["from_centerCount"].peek<CenterCountRes>() );
            if ((remain * (stride + 1)) >= res_tmp.k1) {
                local_cnts[res_tmp.k1 / (stride + 1) + 1] = res_tmp.local;
            } else {
                local_cnts[(res_tmp.k1 - remain) / stride + 1] = res_tmp.local;
            }
            input["from_centerCount"].recycle();
        }
        // Calculate prefix-sum
        local_cnts[0] = 0;
        for (int i = 1; nshards >= i; ++i) {
            local_cnts[i] += local_cnts[i - 1];
        }
        // assert(k == local_cnts[nshards]);
        // Offset center count in each region
        for (int i = 1; remain > i; ++i) {
            CenterOffsetTask &task_tmp(
                    output["to_centerUpdate"].allocate<CenterOffsetTask>() );
            task_tmp.k1 = i * remain + i;
            task_tmp.k2 = task_tmp.k1 + stride + 1;
            //task_tmp.local = local_cnts[i];
            output["to_centerUpdate"].send();
        }
        for (int i = (remain > 1) ? remain : 1; nshards > i; ++i) {
            CenterOffsetTask &task_tmp(
                    output["to_centerUpdate"].allocate<CenterOffsetTask>() );
            task_tmp.k1 = i * remain + remain;
            task_tmp.k2 = task_tmp.k1 + stride;
            //task_tmp.local = local_cnts[i];
            output["to_centerUpdate"].send();
        }

        // Allocate buffer to store lower values
        int cl = CACHE_LINE / sizeof(double);
        int lower_stride = (k + 2);
        lower_stride = ((lower_stride + cl - 1) / cl) * cl; // cacheline align
        double *lower_tab = (double*) malloc(lower_stride * (nshards + 1) *
                                             sizeof(double));

        // Join offset center count tasks
        for (int i = 1; nshards > i; ++i) {
            uint8_t sig;
            input["from_centerUpdate"].pop(sig);
        }

        // Issue tasks to calculate cost of switching
        for (int i = 0; remain > i; ++i) {
            SwitchCostCalcTask &task_tmp(
                    output["to_switchCostCalc"].allocate<SwitchCostCalcTask>()
                    );
            task_tmp.k1 = i * remain + i;
            task_tmp.k2 = task_tmp.k1 + stride + 1;
            task_tmp.idx = idx;
            task_tmp.lower_tab = &lower_tab[lower_stride * i];
            output["to_switchCostCalc"].send();
        }
        for (int i = remain; nshards > i; ++i) {
            SwitchCostCalcTask &task_tmp(
                    output["to_switchCostCalc"].allocate<SwitchCostCalcTask>()
                    );
            task_tmp.k1 = i * remain + remain;
            task_tmp.k2 = task_tmp.k1 + stride;
            task_tmp.idx = idx;
            task_tmp.lower_tab = &lower_tab[lower_stride * i];
            output["to_switchCostCalc"].send();
        }

        // Accumulate all local opening cost
        double gl_cost_of_opening_x = z;
        for (int i = 0; nshards > i; ++i) {
            double tmp;
            input["from_switchCostCalc"].pop<double>(tmp);
            gl_cost_of_opening_x += tmp;
        }

        // Issue tasks to accumulate lower in local regions first
        long kstride = k / nshards;
        long kremain = k % nshards;
        for (int i = 0; kremain > i; ++i) {
            LowerSumupTask &task_tmp(
                    output["to_lowerSumup"].allocate<LowerSumupTask>() );
            task_tmp.k1 = i * kremain + i;
            task_tmp.k2 = task_tmp.k1 + kstride + 1;
            task_tmp.stride = lower_stride;
            task_tmp.z = z;
            task_tmp.lower_tab = lower_tab;
            output["to_lowerSumup"].send();
        }
        for (int i = kremain; nshards > i; ++i) {
            LowerSumupTask &task_tmp(
                    output["to_lowerSumup"].allocate<LowerSumupTask>() );
            task_tmp.k1 = i * kremain + kremain;
            task_tmp.k2 = task_tmp.k1 + kstride;
            task_tmp.stride = lower_stride;
            task_tmp.z = z;
            task_tmp.lower_tab = lower_tab;
            output["to_lowerSumup"].send();
        }

        // Accumulate lowers
        int gl_number_of_centers_to_close = 0;
        //double gl_lower = &lower_tab[nshards * lower_stride];
        for (int i = 0; nshards > i; ++i) {
            LowerSumupRes &res_tmp(
                    input["from_lowerSumup"].peek<LowerSumupRes>() );
            gl_number_of_centers_to_close += res_tmp.nclose;
            gl_cost_of_opening_x -= res_tmp.lowersum;
            input["from_lowerSumup"].recycle();
        }

        // Decide to open x
        if (0 > gl_cost_of_opening_x) {
#ifdef DBG
            std::cout << "before opening " << idx << ": ";
            for (int i = 0; points->num > i; ++i) {
                if (is_center[i]) {
                    std::cout << " " << i;
                }
            }
            std::cout << std::endl;
#endif
            // Issue tasks to commit switch
            for (int i = 0; remain > i; ++i) {
                CommitSwitchTask &task_tmp(
                        output["to_commitSwitch"].allocate<CommitSwitchTask>()
                        );
                task_tmp.k1 = i * remain + i;
                task_tmp.k2 = task_tmp.k1 + stride + 1;
                task_tmp.idx = idx;
                task_tmp.lower_tab = &lower_tab[lower_stride * nshards];
                output["to_commitSwitch"].send();
            }
            for (int i = remain; nshards > i; ++i) {
                CommitSwitchTask &task_tmp(
                        output["to_commitSwitch"].allocate<CommitSwitchTask>()
                        );
                task_tmp.k1 = i * remain + remain;
                task_tmp.k2 = task_tmp.k1 + stride;
                task_tmp.idx = idx;
                task_tmp.lower_tab = &lower_tab[lower_stride * nshards];
                output["to_commitSwitch"].send();
            }
            // Wait for tasks to finish
            for (int i = 0; nshards > i; ++i) {
                uint8_t tmp;
                input["from_commitSwitch"].pop<uint8_t>(tmp);
            }
            is_center[idx] = true;
            k -= gl_number_of_centers_to_close - 1;
#ifdef DBG
            std::cout << "after opening " << idx << ": ";
            for (int i = 0; points->num > i; ++i) {
                if (is_center[i]) {
                    std::cout << " " << i;
                }
            }
            std::cout << std::endl;
#endif
        } else {
            gl_cost_of_opening_x = 0;
        }
        free(lower_tab);
        PGainRes &res( output["to_pFL"].allocate<PGainRes>() );
        res.cost = -gl_cost_of_opening_x;
        res.k = k;
        output["to_pFL"].send();
        input["from_pFL"].recycle();
        return raft::proceed;
    }
};


class CenterCountKernel : public raft::kernel {
    bool *is_center;
    long *center_tab;

public:
    CenterCountKernel(bool *_is_center, long *_center_tab) :
        raft::kernel(), is_center(_is_center), center_tab(_center_tab) {
        // Create ports
        input.addPort<RangedTask>("from_pgain"); // k1, k2
        output.addPort<CenterCountRes>("to_pgain"); // k1, k2, local
#ifdef DBG
        std::cout << __func__ << " " << this->get_id() << std::endl;
#endif
    }
    virtual raft::kstatus run() {
        RangedTask &task( input["from_pgain"].peek<RangedTask>() );
        long cnt = 0;
        for (long i = task.k1; task.k2 > i; ++i) {
            if (is_center[i]) {
                center_tab[i] = cnt++;
            }
        }
        CenterCountRes &res( output["to_pgain"].allocate<CenterCountRes>() );
        res.k1 = task.k1;
        //res.k2 = task.k2; // caller does not actually use
        res.local = cnt;
        output["to_pgain"].send();
        input["from_pgain"].recycle();
        return raft::proceed;
    }
};


class CenterOffsetKernel : public raft::kernel {
    bool *is_center;
    long *center_tab;

public:
    CenterOffsetKernel(bool *_is_center, long *_center_tab) :
        raft::kernel(), is_center(_is_center), center_tab(_center_tab) {
        // Create ports
        input.addPort<CenterOffsetTask>("from_pgain"); // k1, k2, local
        output.addPort<uint8_t>("to_pgain"); // signal
#ifdef DBG
        std::cout << __func__ << " " << this->get_id() << std::endl;
#endif
    }
    virtual raft::kstatus run() {
        CenterOffsetTask &task( input["from_pgain"].peek<CenterOffsetTask>() );
        for (long i = task.k1; task.k2 > i; ++i) {
            if (is_center[i]) {
                center_tab[i] += task.local;
            }
        }
        output["to_pgain"].push<uint8_t>(0);
        input["from_pgain"].recycle();

        return raft::proceed;
    }
};


class SwitchCostCalcKernel : public raft::kernel {
    Points *points;
    bool *switch_membership;
    long *center_tab;

public:
    SwitchCostCalcKernel(Points *_points, bool *_switch, long *_center_tab) :
        raft::kernel(), points(_points), switch_membership(_switch),
        center_tab(_center_tab) {
        // Create ports
        input.addPort<SwitchCostCalcTask>("from_pgain");
        // k1, k2, idx, lower_tab
        output.addPort<double>("to_pgain"); // cost of open
#ifdef DBG
        std::cout << __func__ << " " << this->get_id() << std::endl;
#endif
    }
    virtual raft::kstatus run() {
        SwitchCostCalcTask &task(
                input["from_pgain"].peek<SwitchCostCalcTask>() );
        long k1 = task.k1;
        long k2 = task.k2;
        long x = task.idx;
        double *lower_tab = task.lower_tab;
        double cost_of_opening_x = 0.0;
        for (long i = k1; k2 > i; ++i) {
            float x_cost = dist(points->p[i], points->p[x], points->dim) *
                points->p[i].weight;
            float current_cost = points->p[i].cost;
            if (x_cost < current_cost) {
                // point i would save cost just by switching to task.idx
                switch_membership[i] = 1;
                cost_of_opening_x += x_cost - current_cost;
            } else {
                int assign = points->p[i].assign;
                lower_tab[center_tab[assign]] += current_cost - x_cost;
            }
        }
        output["to_pgain"].push<double>(cost_of_opening_x);
        input["from_pgain"].recycle();
        return raft::proceed;
    }
};


class LowerSumupKernel : public raft::kernel {
    int nshards;

public:
    LowerSumupKernel(int _nshards) : raft::kernel(), nshards(_nshards) {
        // Create ports
        input.addPort<LowerSumupTask>("from_pgain");
        // k1, k2, stride, lower_tab
        output.addPort<LowerSumupRes>("to_pgain"); // nclose, lowersum
#ifdef DBG
        std::cout << __func__ << " " << this->get_id() << std::endl;
#endif
    }
    virtual raft::kstatus run() {
        LowerSumupTask &task( input["from_pgain"].peek<LowerSumupTask>() );
        double *lower_tab = task.lower_tab;
        long stride = task.stride;
        long number_of_centers_to_close = 0;
        double cost_of_opening_x = 0.0;
        for (long i = task.k1; task.k2 > i; ++i) {
            double low = task.z;
            for (int j = 0; nshards > j; ++j) {
                low += lower_tab[j * stride + i];
            }
            lower_tab[nshards * stride + i] = low;
            if (0 < low) {
                ++number_of_centers_to_close;
                cost_of_opening_x -= low;
            }
        }
        LowerSumupRes &res( output["to_pgain"].allocate<LowerSumupRes>() );
        res.nclose = number_of_centers_to_close;
        res.lowersum = cost_of_opening_x;
        output["to_pgain"].send();
        input["from_pgain"].recycle();
        return raft::proceed;
    }
};


class CommitSwitchKernel : public raft::kernel {
    Points *points;
    bool *switch_membership;
    bool *is_center;
    long *center_tab;

public:
    CommitSwitchKernel(Points *_points, bool *_switch, bool *_is_center,
                       long *_center_tab) :
        raft::kernel(), points(_points), switch_membership(_switch),
        is_center(_is_center), center_tab(_center_tab) {
        // Create ports
        input.addPort<CommitSwitchTask>("from_pgain");
        // k1, k2, idx, lower_tab
        output.addPort<uint8_t>("to_pgain"); // signal
#ifdef DBG
        std::cout << __func__ << " " << this->get_id() << std::endl;
#endif
    }
    virtual raft::kstatus run() {
        CommitSwitchTask &task( input["from_pgain"].peek<CommitSwitchTask>() );
        double *gl_lower = task.lower_tab;
        for (long i = task.k1; task.k2 > i; ++i) {
            bool close_center = gl_lower[center_tab[points->p[i].assign]] > 0;
            if (switch_membership[i] || close_center) {
                points->p[i].cost = points->p[i].weight *
                    dist(points->p[i], points->p[task.idx], points->dim);
                points->p[i].assign = task.idx;
            }
            if (is_center[i] && gl_lower[center_tab[i]] > 0) {
                is_center[i] = false;
            }
        }
        output["to_pgain"].push<uint8_t>(0);
        input["from_pgain"].recycle();
        return raft::proceed;
    }
};


class ContCentersKernel : public raft::kernel {
    Points *points;

public:
    ContCentersKernel(Points *_points) : raft::kernel(), points(_points) {
        // Create ports
        input.addPort<uint8_t>("from_search"); // task_id
        output.addPort<uint8_t>("to_copycenters"); // task_id
#ifdef DBG
        std::cout << __func__ << " " << this->get_id() << std::endl;
#endif
    }
    virtual raft::kstatus run() {
        uint8_t task_id;
        input["from_search"].pop<uint8_t>(task_id);
        float relweight;
        for (long i = 0; points->num > i; ++i) {
            relweight = points->p[points->p[i].assign].weight +
                points->p[i].weight;
            relweight = points->p[i].weight / relweight;
            for (long j = 0; points->dim > j; ++j) {
                points->p[points->p[i].assign].coord[j] *= 1.0 - relweight;
                points->p[points->p[i].assign].coord[j] +=
                    points->p[i].coord[j] * relweight;
            }
            points->p[points->p[i].assign].weight += points->p[i].weight;
        }
        output["to_copycenters"].push<uint8_t>(task_id);
        return raft::proceed;
    }
};


class CopyCentersKernel : public raft::kernel {
    Points *points;
    Points *centers;
    bool *is_center;
    long *centerIDs;
    long IDoffset;

public:
    CopyCentersKernel(Points *_points, Points *_centers, bool *_is_center,
                      long *_centerIDs) :
        raft::kernel(), points(_points), centers(_centers),
        is_center(_is_center), centerIDs(_centerIDs) {
        // Create ports
        input.addPort<uint8_t>("from_contcenters"); // task_id
        output.addPort<uint8_t>("to_master"); // task_id
        IDoffset = 0;
#ifdef DBG
        std::cout << __func__ << " " << this->get_id() << std::endl;
#endif
    }
    virtual raft::kstatus run() {
        uint8_t task_id;
        input["from_contcenters"].pop<uint8_t>(task_id);
        long k = centers->num;
        for (long i = 0; points->num > i; ++i) {
            if (is_center[i]) {
                memcpy(centers->p[k].coord, points->p[i].coord,
                       points->dim * sizeof(float));
                centers->p[k].weight = points->p[i].weight;
                centerIDs[k] = i + IDoffset;
                k++;
            }
        }
        centers->num = k;
        IDoffset += points->num;
#ifdef DBG
        std::cout << centers->num << " centers out of " << IDoffset <<
            std::endl;
#endif
        output["to_master"].push<uint8_t>(task_id);

        return raft::proceed;
    }
};

#endif
