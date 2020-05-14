#ifndef _CHECK_H__
#define _CHECK_H__  1

#include <stdio.h>
#include <stdlib.h>

/*
 * A wrapper checking and print errors.
 */
#define checkResults(string, val) {              \
  if (val) {                                     \
    printf("Failed with %d at %s", val, string); \
    exit(1);                                     \
  }                                              \
}

#define errorReturn(retval) {                    \
  fprintf(stderr, "Error %d %s:line %d: \n",     \
          retval,__FILE__,__LINE__);             \
  exit(retval);                                  \
}

#endif /* END _CHECK_H__ */
