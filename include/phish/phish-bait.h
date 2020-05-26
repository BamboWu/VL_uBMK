/* ----------------------------------------------------------------------
   PHISH = Parallel Harness for Informatic Stream Hashing
   http://www.sandia.gov/~sjplimp/phish.html
   Steve Plimpton, sjplimp@sandia.gov, Sandia National Laboratories

   Copyright (2012) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the modified Berkeley Software Distribution (BSD) License.

   See the README file in the top-level PHISH directory.
------------------------------------------------------------------------- */

/* PHISH library API */

#ifndef PHISH_BAIT_H
#define PHISH_BAIT_H

#ifdef __cplusplus
extern "C" {
#endif

#define PHISH_BAIT_SINGLE "single"
#define PHISH_BAIT_PAIRED "paired"
#define PHISH_BAIT_HASHED "hashed"
#define PHISH_BAIT_ROUND_ROBIN "roundrobin"
#define PHISH_BAIT_DIRECT "direct"
#define PHISH_BAIT_BROADCAST "bcast"
#define PHISH_BAIT_CHAIN "chain"
#define PHISH_BAIT_RING "ring"
#define PHISH_BAIT_PUBLISH "publish"
#define PHISH_BAIT_SUBSCRIBE "subscribe"

void phish_bait_reset();
void phish_bait_set(const char* name, const char* value);
int phish_bait_school(const char* id, int count, const char** hosts, int argc, const char** argv);
int phish_bait_hook(const char* output_id, int output_port, const char* style, int input_port, const char* input_id);
int phish_bait_start();

#ifdef __cplusplus
}
#endif

#endif // !PHISH_BAIT_H
