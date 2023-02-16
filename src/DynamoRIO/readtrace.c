/**
 * $gcc -o readtrace readtrace.c
 * $./readtrace bin/armq_trace.search_ut_std.1234.0001.log \
 *  bin/armq_trace.search_ut_std.14T4C.0001.log
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define MAX_INS_SLOT 8192

typedef struct _ins_ref_t {
    uint64_t pc;
    uint64_t tsc;
    uint64_t val;
} ins_ref_t;

int main(int argc, char* argv[]) {
    if (2 >= argc) {
        printf("Usage: %s <input> <output>\n", argv[0]);
        return 0;
    }

    FILE* fi = fopen(argv[1], "r");
    fseek(fi, 0, SEEK_END);
    const long fsz = ftell(fi);
    const long cnt = fsz / sizeof(ins_ref_t);
    fseek(fi, 0, SEEK_SET);

    FILE* fo = fopen(argv[2], "w");
    fprintf(fo, "pc,tsc,val\n");

    ins_ref_t buf[MAX_INS_SLOT];
    long acc = 0;
    while (acc < cnt) {
        int batch = (MAX_INS_SLOT < (cnt - acc)) ? MAX_INS_SLOT : (cnt - acc);
        if (1 != fread((void*)buf, batch * sizeof(ins_ref_t), 1, fi)) {
            printf("Error: failed to read file %s\n", argv[1]);
            return 0;
        }
        for (int i = 0; batch > i; ++i) {
            //printf("0x%lx,%lu,%lx\n", buf[i].pc, buf[i].tsc, buf[i].val);
            fprintf(fo, "0x%lx,%lu,%lx\n", buf[i].pc, buf[i].tsc, buf[i].val);
        }
        acc += batch;
    }

    fclose(fo);
    fclose(fi);

    return 0;
}
