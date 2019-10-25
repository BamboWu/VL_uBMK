#ifndef _CHECK_H__
#define _CHECK_H__  1

#include <stdio.h>
#include <stdlib.h>

/*
 * A wrapper checking and print errors.
 */
#define checkResults(string, val) {             \
 if (val) {                                     \
   printf("Failed with %d at %s", val, string); \
   exit(1);                                     \
 }                                              \
}

#endif /* END _CHECK_H__ */
