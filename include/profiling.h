#ifndef _PROFILING_H__
#define _PROFILING_H__  1

#ifndef __USE_GNU
#define __USE_GNU
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern void cycleBeg();
extern long long cycleEnd();

extern void l1dBeg();
extern void l1dEnd(long long *cntvals);

extern void l2dBeg();
extern void l2dEnd(long long *cntvals);

#ifdef __cplusplus
}
#endif

#endif /* END _PROFILING_H__ */
