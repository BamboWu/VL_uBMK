/*
 * Copyright (C) 2008 Princeton University
 * All rights reserved.
 * Authors: Jia Deng, Gilberto Contreras
 *
 * streamcluster - Online clustering algorithm
 *
 */

/**
 * raftlib_src.cpp -
 * @author: James Wood
 * @version: Thu July 23 13:25:00 2020
 *
 * Copyright 2020 Jonathan Beard
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

#include <cstdio>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <assert.h>
#include <math.h>
#include <sys/resource.h>
#include <climits>
#include <vector>
#include <limits>
#include <chrono>

using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::nanoseconds;

#include "timing.h"

#include "raftlib_src.hpp"
#include "kernels.hpp"
#define USE_RAFT

// Helper functions

/**
 * Shuffle points into random order.
 */
void shuffle(Points *points) {
  for (long i=0; i < points->num - 1; i++) {
    long j = (lrand48() % (points->num - i)) + i;
    Point temp = points->p[i];
    points->p[i] = points->p[j];
    points->p[j] = temp;
  }
}

/**
 *  Shuffle an array of integers.
 */
void intshuffle(int *intarray, int length) {
  for (long i=0; i < length; i++) {
    long j = (lrand48() % (length - i)) + i;
    int temp = intarray[i];
    intarray[i] = intarray[j];
    intarray[j] = temp;
  }
}

void streamCluster_raftlib(PStream* stream, long kmin, long kmax, int dim,
                           long chunksize, long centersize, char* outfile,
                           int nproc) {
  // Create our "global" arrays
  float* block = new float[chunksize * dim];
  float* centerBlock = new float[centersize * dim];
  long* centerIDs = new long[centersize];

  bool *switch_membership = new bool[chunksize]; // whether switch membership
  bool *is_center = new bool[chunksize](); // whether a point is a center
  long *center_tab = new long[chunksize]; // index table of centers

  const int max_feasible = ITER*kmin*log((double)kmin);
  int *feasible = new int[max_feasible]; // center candidates

  if(NULL == block) {
    std::cerr << "Not enough memory for a chunk!" << std::endl;
    exit(EXIT_FAILURE);
  }

  // Create the points and centers arrays

  Points points;
  points.dim = dim;
  points.num = chunksize;
  points.p = new Point[chunksize];

  for(int i = 0; i < chunksize; i++)
    points.p[i].coord = &block[i*dim];

  Points centers;
  centers.dim = dim;
  centers.p = new Point[centersize];
  centers.num = 0;

  for(int i = 0; i< centersize; i++)
    centers.p[i].coord = &centerBlock[i*dim];

  SourceKernel dummy_source;
  SinkKernel dummy_sink;
  MasterKernel master(stream, &points, block, dim, chunksize);
  LocalSearchKernel search;
  ContCentersKernel contCenters(&points);
  CopyCentersKernel copyCenters(&points, &centers, is_center, centerIDs);
  PKMedianInitKernel pkmedianInit(&points, nproc);
  PKMedianPSpeedyLoopKernel pkmedianPSpeedyLoop(&points, kmin, kmax);
  PKMedianPFLLoopKernel pkmedianPFLLoop(&points, is_center, kmin, kmax);
  PSpeedyKernel pspeedy(&points, nproc);
  SelectFeasibleFastKernel selectFeasible(&points, feasible, max_feasible,
                                          chunksize);
  PFLKernel pFL(feasible, (long)(ITER*kmax*log((double)kmax)));
  PGainKernel pgain(&points, nproc, is_center);

  CostTo0Kernel costTo0(&points);
  CenterTo0Kernel centerTo0(&points);
  CenterUpdateKernel centerUpdate(&points);
  CostSumupKernel costSumup(&points);
  CenterCountKernel centerCount(is_center, center_tab);
  CenterOffsetKernel centerOffset(is_center, center_tab);
  SwitchCostCalcKernel switchCostCalc(&points, switch_membership, center_tab);
  LowerSumupKernel lowerSumup(nproc);
  CommitSwitchKernel commitSwitch(&points, switch_membership, is_center,
                                  center_tab);

  raft::map m;

  m += dummy_source >> master["from_source"];
  m += master["to_search"] >> search["from_master"];
  m += search["to_contcenters"] >> contCenters >> copyCenters >>
      master["from_copycenters"];
  m += master["to_sink"] >> dummy_sink;

  m += search["to_pkmedian"] >> pkmedianInit["from_search"];
  m += pkmedianInit["to_pspeedy"] >> pkmedianPSpeedyLoop["from_init"];
  m += pkmedianPSpeedyLoop["to_selectfeasible"] >> selectFeasible >>
      pkmedianPFLLoop["from_selectfeasible"];
  m += pkmedianPFLLoop["to_search"] >> search["from_pkmedian"];

  m += pkmedianPSpeedyLoop["to_pspeedy"] >> pspeedy["from_pkmedian"];
  m += pspeedy["to_pkmedian"] >> pkmedianPSpeedyLoop["from_pspeedy"];

  m += pkmedianPFLLoop["to_pFL"] >> pFL["from_pkmedian"];
  m += pFL["to_pgain"] >> pgain["from_pFL"];
  m += pgain["to_pFL"] >> pFL["from_pgain"];
  m += pFL["to_pkmedian"] >> pkmedianPFLLoop["from_pFL"];


  // to multi-instance workers
  m += pkmedianInit["to_worker"] >> costTo0 >>
      pkmedianInit["from_worker"];
  m += pspeedy["to_centerTo0"] >> centerTo0 >>
      pspeedy["from_centerTo0"];
  m += pspeedy["to_centerUpdate"] >> centerUpdate >>
      pspeedy["from_centerUpdate"];
  m += pspeedy["to_costSumup"] >> costSumup >> pspeedy["from_costSumup"];
  m += pgain["to_centerCount"] >> centerCount >> pgain["from_centerCount"];
  m += pgain["to_centerOffset"] >> centerOffset >> pgain["from_centerOffset"];
  m += pgain["to_switchCostCalc"] >> switchCostCalc >>
      pgain["from_switchCostCalc"];
  m += pgain["to_lowerSumup"] >> lowerSumup >> pgain["from_lowerSumup"];
  m += pgain["to_commitSwitch"] >> commitSwitch >> pgain["from_commitSwitch"];

  // Execute the map

  const uint64_t beg_tsc = rdtsc();
  const auto beg( high_resolution_clock::now() );

  m.exe< partition_dummy,
#if USEUT
        ut_schedule,
#elif USEQTHREAD
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
        no_parallel >();

  const uint64_t end_tsc = rdtsc();
  const auto end( high_resolution_clock::now() );
  const auto elapsed( duration_cast< nanoseconds >( end - beg ) );
  std::cout << ( end_tsc - beg_tsc ) << " ticks elapsed\n";
  std::cout << elapsed.count() << " ns elapsed\n";

  // Cleanup
  delete[] centers.p;
  delete[] points.p;

  delete[] block;
  delete[] centerBlock;
  delete[] centerIDs;

  delete[] switch_membership;
  delete[] is_center;
  delete[] center_tab;
}
