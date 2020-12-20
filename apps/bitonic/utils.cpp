#include "utils.hpp"

/* Try to pair two sorted adjacent subarrays */
bool ispaired(uint8_t *scnts, uint64_t idx, uint64_t *pidx_1st) {
  uint64_t cnt = scnts[idx] - MINI_TASK_EXP;
  uint64_t len_sorted = 1 << cnt;
  cnt++;
  *pidx_1st = (idx >> cnt) << cnt;
  uint64_t idx_2nd = *pidx_1st + len_sorted;
  return (scnts[*pidx_1st] == scnts[idx_2nd]);
}

/* Find the maximum sorted super array */
uint8_t maxsorted(uint8_t *scnts, uint64_t idx, uint64_t *pidx_1st) {
  uint8_t cnt;
  uint64_t idx_1st = idx;
  do {
    *pidx_1st = idx_1st;
    cnt = scnts[idx_1st] - MINI_TASK_EXP + 1;
    idx_1st = (*pidx_1st >> cnt) << cnt; // super array of *pidx_1st
  } while ((cnt + MINI_TASK_EXP) <= scnts[idx_1st]); // is sorted
  return cnt + MINI_TASK_EXP - 1;
}

/* Find the maximum completed segement by traversing the tree */
uint8_t maxdone(uint8_t *dcnts, uint64_t idx, uint8_t max_exp) {
  uint8_t cnt;
  uint64_t idx_1st, idx_2nd;
  idx_1st = idx;
  dcnts[idx_1st] = 0;
  do {
    dcnts[idx_1st]++;
    cnt = dcnts[idx_1st];
    idx_1st = (idx_1st >> cnt) << cnt;
    idx_2nd = idx_1st + (1 << (cnt - 1));
  } while (cnt < max_exp && dcnts[idx_1st] == dcnts[idx_2nd]);
  return cnt;
}
