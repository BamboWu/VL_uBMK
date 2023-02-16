/* ******************************************************************************
 * Copyright (c) 2011-2018 Google, Inc.  All rights reserved.
 * Copyright (c) 2010 Massachusetts Institute of Technology  All rights reserved.
 * ******************************************************************************/

/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of VMware, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* Code Manipulation API Sample:
 * armq_trace.c
 *
 * Collects trace of dynamic instruction at selected PC and dumps it to a
 * text file.
 *
 * (1) It fills a per-thread-buffer from inlined instrumentation.
 * (2) It calls a clean call to dump the buffer into a file.
 *
 * The trace is a simple text file with each line containing the PC (at
 * run time), tsc counter timestamp, and the operand of the instruction.
 * For memory reference instructions, if the operand refers to the memory,
 * it is the memory address gets dumped.
 *
 * To run the client:
 * ../bin64/drrun -c bin/libarmq_trace.so <pc> <operand idx> [<pc> <op idx> ...] -- <cmd>
 *
 * This client is a simple implementation of an instruction tracing tool
 * without instrumentation optimization.  It also uses simple absolute PC
 * values and does not separate them into library offsets.
 *
 * The OUTPUT_TEXT define controls the format of the trace: text or binary.
 * Creating a text trace file makes the tool an order of magnitude (!) slower
 * than creating a binary file; thush, the default is binary.
 */

#include <stdio.h>
#include <stddef.h> /* for offsetof */
#include "dr_api.h"
#include "drmgr.h"
#include "drreg.h"
#include "drutil.h"
#include "utils.h"

//#define OUTPUT_TEXT

#define HASH_SET_SIZE 65537
#define NSLOTS 2
#define PC_HASH(PC) ((PC >> 2) % HASH_SET_SIZE)

u_int64_t pc_roi[HASH_SET_SIZE][NSLOTS] = {{ 0 }};
u_int64_t reg_roi[HASH_SET_SIZE][NSLOTS] = {{ 0 }};

/* Each ins_ref_t describes an executed instruction. */
typedef struct _ins_ref_t {
    app_pc pc;
    u_int64_t tsc;
    u_int64_t val;
} ins_ref_t;

/* Max number of ins_ref a buffer can have. It should be big enough
 * to hold all entries between clean calls.
 */
#define MAX_NUM_INS_REFS 8192
/* The maximum size of buffer for holding ins_refs. */
#define MEM_BUF_SIZE (sizeof(ins_ref_t) * MAX_NUM_INS_REFS)

/* thread private log file and counter */
typedef struct {
    byte *seg_base;
    ins_ref_t *buf_base;
    file_t log;
    FILE *logf;
    //uint64 num_refs;
} per_thread_t;

static client_id_t client_id;
//static void *mutex;     /* for multithread support */
//static uint64 num_refs; /* keep a global instruction reference count */
static app_pc exe_start;

/* Allocated TLS slot offsets */
enum {
    INSTRACE_TLS_OFFS_BUF_PTR,
    INSTRACE_TLS_COUNT, /* total number of TLS slots allocated */
};
static reg_id_t tls_seg;
static uint tls_offs;
static int tls_idx;
#define TLS_SLOT(tls_base, enum_val) (void **)((byte *)(tls_base) + tls_offs + (enum_val))
#define BUF_PTR(tls_base) *(ins_ref_t **)TLS_SLOT(tls_base, INSTRACE_TLS_OFFS_BUF_PTR)

#define MINSERT instrlist_meta_preinsert
#define NINSERT instrlist_meta_postinsert

static void
instrace(void *drcontext)
{
    per_thread_t *data;
    ins_ref_t *ins_ref, *buf_ptr;

    data = drmgr_get_tls_field(drcontext, tls_idx);
    buf_ptr = BUF_PTR(data->seg_base);
    /* Example of dumped file content:
     *   0x7f59c2d002d3,call,0
     *   0x7ffeacab0ec8,mov,ffffff00
     */
#ifdef OUTPUT_TEXT
    /* We use libc's fprintf as it is buffered and much faster than dr_fprintf
     * for repeated printing that dominates performance, as the printing does here.
     */
    for (ins_ref = (ins_ref_t *)data->buf_base; ins_ref < buf_ptr; ins_ref++) {
        /* We use PIFX to avoid leading zeroes and shrink the resulting file. */
        fprintf(data->logf, PIFX ",%lu,%lx\n", (ptr_uint_t)ins_ref->pc,
                ins_ref->tsc, ins_ref->val);
        //data->num_refs++;
    }
#else
    //dr_write_file(data->log, (char*)data->buf_base,
    //              (size_t)((char*)buf_ptr - (char*)data->buf_base));
    fwrite((void*)data->buf_base,
           (size_t)((char*)buf_ptr - (char*)data->buf_base), 1, data->logf);
#endif
    BUF_PTR(data->seg_base) = data->buf_base;
}

/* clean_call dumps the memory reference info to the log file */
static void
clean_call(void)
{
    void *drcontext = dr_get_current_drcontext();
    instrace(drcontext);
}

static void
insert_load_buf_ptr(void *drcontext, instrlist_t *ilist, instr_t *where, reg_id_t reg_ptr)
{
    dr_insert_read_raw_tls(drcontext, ilist, where, tls_seg,
                           tls_offs + INSTRACE_TLS_OFFS_BUF_PTR, reg_ptr);
}

static void
insert_update_buf_ptr(void *drcontext, instrlist_t *ilist, instr_t *where,
                      reg_id_t reg_ptr, int adjust)
{
    MINSERT(
        ilist, where,
        XINST_CREATE_add(drcontext, opnd_create_reg(reg_ptr), OPND_CREATE_INT16(adjust)));
    dr_insert_write_raw_tls(drcontext, ilist, where, tls_seg,
                            tls_offs + INSTRACE_TLS_OFFS_BUF_PTR, reg_ptr);
}

static void
insert_save_addr(void *drcontext, instrlist_t *ilist, instr_t *where, reg_id_t base,
                 reg_id_t scratch, opnd_t oprand)
{
    bool ok = drutil_insert_get_mem_addr(drcontext, ilist, where, oprand, scratch, base);
    DR_ASSERT(ok);
    insert_load_buf_ptr(drcontext, ilist, where, base);
    MINSERT(ilist, where,
            XINST_CREATE_store(
                drcontext, OPND_CREATE_MEM64(base, offsetof(ins_ref_t, val)),
                opnd_create_reg(scratch)));
}

static void
insert_save_dst(void *drcontext, instrlist_t *ilist, instr_t *where, reg_id_t base,
                reg_id_t scratch, opnd_t oprand)
{
    NINSERT(ilist, where,
            XINST_CREATE_store(
                drcontext, OPND_CREATE_MEM64(base, offsetof(ins_ref_t, val)),
                oprand));
}

static void
insert_save_src(void *drcontext, instrlist_t *ilist, instr_t *where, reg_id_t base,
                reg_id_t scratch, opnd_t oprand)
{
    if (opnd_uses_reg(oprand, scratch) &&
        drreg_get_app_value(drcontext, ilist, where, scratch, scratch) !=
        DRREG_SUCCESS) {
        DR_ASSERT(false); /* cannot recover */
        return;
    } else if (opnd_uses_reg(oprand, base)) {
        if (drreg_get_app_value(drcontext, ilist, where, base, scratch) !=
            DRREG_SUCCESS) {
            DR_ASSERT(false); /* cannot recover */
            return;
        }
        MINSERT(ilist, where,
                XINST_CREATE_store(
                    drcontext,
                    OPND_CREATE_MEM64(base, offsetof(ins_ref_t, val)),
                    opnd_create_reg(scratch)));
        return;
    }
    MINSERT(ilist, where,
            XINST_CREATE_store(
                drcontext, OPND_CREATE_MEM64(base, offsetof(ins_ref_t, val)),
                oprand));
}

static void
insert_save_tsc(void *drcontext, instrlist_t *ilist, instr_t *where, reg_id_t base,
                reg_id_t scratch)
{
    MINSERT(ilist, where,
            INSTR_CREATE_mrs(drcontext, opnd_create_reg(scratch),
                             opnd_create_reg(DR_REG_CNTVCT_EL0)));
    MINSERT(ilist, where,
            XINST_CREATE_store(
                drcontext, OPND_CREATE_MEM64(base, offsetof(ins_ref_t, tsc)),
                opnd_create_reg(scratch)));
}

//static void
//insert_save_opcode(void *drcontext, instrlist_t *ilist, instr_t *where, reg_id_t base,
//                   reg_id_t scratch, int opcode)
//{
//    scratch = reg_resize_to_opsz(scratch, OPSZ_2);
//    MINSERT(ilist, where,
//            XINST_CREATE_load_int(drcontext, opnd_create_reg(scratch),
//                                  OPND_CREATE_INT16(opcode)));
//    MINSERT(ilist, where,
//            XINST_CREATE_store_2bytes(
//                drcontext, OPND_CREATE_MEM16(base, offsetof(ins_ref_t, opcode)),
//                opnd_create_reg(scratch)));
//}

static void
insert_save_pc(void *drcontext, instrlist_t *ilist, instr_t *where, reg_id_t base,
               reg_id_t scratch, app_pc pc)
{
    instrlist_insert_mov_immed_ptrsz(drcontext, (ptr_int_t)pc, opnd_create_reg(scratch),
                                     ilist, where, NULL, NULL);
    MINSERT(ilist, where,
            XINST_CREATE_store(drcontext,
                               OPND_CREATE_MEMPTR(base, offsetof(ins_ref_t, pc)),
                               opnd_create_reg(scratch)));
}

/* insert inline code to add an instruction entry into the buffer */
static void
instrument_instr(void *drcontext, instrlist_t *ilist, instr_t *where)
{
    /* We need two scratch registers */
    reg_id_t reg_ptr, reg_tmp;
    if (drreg_reserve_register(drcontext, ilist, where, NULL, &reg_ptr) !=
            DRREG_SUCCESS ||
        drreg_reserve_register(drcontext, ilist, where, NULL, &reg_tmp) !=
            DRREG_SUCCESS) {
        DR_ASSERT(false); /* cannot recover */
        return;
    }

    u_int64_t pc = (u_int64_t) instr_get_app_pc(where);
    //u_int64_t base_addr = (u_int64_t) (dr_lookup_module((byte *)pc)->start);
    //u_int64_t pc_hash = PC_HASH(pc - base_addr);
    u_int64_t pc_hash = PC_HASH(pc - (u_int64_t)exe_start);
    bool hit_roi = false;
    int reg_idx;
    for (int idx = 0; NSLOTS > idx; ++idx) {
        if ((pc - (u_int64_t)exe_start) == pc_roi[pc_hash][idx]) {
            hit_roi = true;
            reg_idx = reg_roi[pc_hash][idx];
            break;
        }
    }
    if (hit_roi) {
    //printf("%lx - %lx = %lx: %s\n", pc, base_addr, decode_opcode_name(instr_get_opcode(where)));
    insert_load_buf_ptr(drcontext, ilist, where, reg_ptr);
    insert_save_pc(drcontext, ilist, where, reg_ptr, reg_tmp, instr_get_app_pc(where));
    insert_save_tsc(drcontext, ilist, where, reg_ptr, reg_tmp);
    //printf("%lx %s has %d srcs oprands\n", pc, decode_opcode_name(instr_get_opcode(where)), instr_num_srcs(where));
    if (0 > reg_idx && -reg_idx <= instr_num_dsts(where)) {
        opnd_t oprand = instr_get_dst(where, -1 - reg_idx);
        if (opnd_is_memory_reference(oprand)) {
            //insert_load_buf_ptr(drcontext, ilist, where, reg_ptr);
            insert_save_addr(drcontext, ilist, where, reg_ptr, reg_tmp, oprand);
        } else {
            //insert_load_buf_ptr(drcontext, ilist, where, reg_ptr);
            insert_save_dst(drcontext, ilist, where, reg_ptr, reg_tmp, oprand);
        }
    } else if (reg_idx < instr_num_srcs(where)) {
        //if (opnd_uses_reg(instr_get_src(where, reg_idx), reg_ptr)) {
        //    printf("conflict with reg_ptr\n");
        //} else if (opnd_uses_reg(instr_get_src(where, reg_idx), reg_tmp)) {
        //    printf("conflict with reg_tmp\n");
        //}
        opnd_t oprand = instr_get_src(where, reg_idx);
        if (opnd_is_memory_reference(oprand)) {
            //insert_load_buf_ptr(drcontext, ilist, where, reg_ptr);
            insert_save_addr(drcontext, ilist, where, reg_ptr, reg_tmp, oprand);
        } else {
            //insert_load_buf_ptr(drcontext, ilist, where, reg_ptr);
            insert_save_src(drcontext, ilist, where, reg_ptr, reg_tmp, oprand);
        }
    }
    //insert_save_opcode(drcontext, ilist, where, reg_ptr, reg_tmp,
    //                   instr_get_opcode(where));
    insert_update_buf_ptr(drcontext, ilist, where, reg_ptr, sizeof(ins_ref_t));
    }

    /* Restore scratch registers */
    if (drreg_unreserve_register(drcontext, ilist, where, reg_ptr) != DRREG_SUCCESS ||
        drreg_unreserve_register(drcontext, ilist, where, reg_tmp) != DRREG_SUCCESS)
        DR_ASSERT(false);
}

static dr_emit_flags_t
event_bb_analysis(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                  bool translating, void **user_data)
{
    /* Only app BBs */
    if (true) {
        module_data_t *mod = dr_lookup_module(dr_fragment_app_pc(tag));
        if (NULL != mod) {
            bool from_exe = (mod->start == exe_start);
            dr_free_module_data(mod);
            if (!from_exe) {
                *user_data = NULL;
                return DR_EMIT_DEFAULT;
            }
        }
    }

    bool hit_roi = false;

    u_int64_t pc_beg = (u_int64_t) instr_get_app_pc(instrlist_first(bb));
    u_int64_t pc_end = (u_int64_t) instr_get_app_pc(instrlist_last(bb));
    /* TODO: do not hardcode instruction length to be 4 words */
    for (u_int64_t pc = pc_beg; pc_end >= pc; pc += 4) {
        u_int64_t pc_hash = PC_HASH(pc - (u_int64_t)exe_start);
        for (int idx = 0; NSLOTS > idx; ++idx) {
            if ((pc - (u_int64_t)exe_start) == pc_roi[pc_hash][idx]) {
                hit_roi = true;
                break;
            }
        }
    }

    if (!hit_roi) {
        *user_data = NULL;
    } else {
        *user_data = (void *)1;
    }
    return DR_EMIT_DEFAULT;
}

/* For each app instr, we insert inline code to fill the buffer. */
static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                      bool for_trace, bool translating, void *user_data)
{
    /* we don't want to auto-predicate any instrumentation */
    drmgr_disable_auto_predication(drcontext, bb);

    if (NULL == user_data) {
        return DR_EMIT_DEFAULT;
    }
    //if (!instr_is_app(instr))
    //    return DR_EMIT_DEFAULT;

    /* insert code to add an entry to the buffer */
    instrument_instr(drcontext, bb, instr);

    /* insert code once per bb to call clean_call for processing the buffer */
    if (drmgr_is_first_instr(drcontext, instr)
        /* XXX i#1698: there are constraints for code between ldrex/strex pairs,
         * so we minimize the instrumentation in between by skipping the clean call.
         * We're relying a bit on the typical code sequence with either ldrex..strex
         * in the same bb, in which case our call at the start of the bb is fine,
         * or with a branch in between and the strex at the start of the next bb.
         * However, there is still a chance that the instrumentation code may clear the
         * exclusive monitor state.
         * Using a fault to handle a full buffer should be more robust, and the
         * forthcoming buffer filling API (i#513) will provide that.
         */
        IF_AARCHXX(&&!instr_is_exclusive_store(instr)))
        dr_insert_clean_call(drcontext, bb, instr, (void *)clean_call, false, 0);

    return DR_EMIT_DEFAULT;
}

static void
event_thread_init(void *drcontext)
{
    per_thread_t *data = dr_thread_alloc(drcontext, sizeof(per_thread_t));
    DR_ASSERT(data != NULL);
    drmgr_set_tls_field(drcontext, tls_idx, data);

    /* Keep seg_base in a per-thread data structure so we can get the TLS
     * slot and find where the pointer points to in the buffer.
     */
    data->seg_base = dr_get_dr_segment_base(tls_seg);
    data->buf_base =
        dr_raw_mem_alloc(MEM_BUF_SIZE, DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
    //data->buf_base = dr_thread_alloc(drcontext, MEM_BUF_SIZE);
    DR_ASSERT(data->seg_base != NULL && data->buf_base != NULL);
    /* put buf_base to TLS as starting buf_ptr */
    BUF_PTR(data->seg_base) = data->buf_base;

    //data->num_refs = 0;

    /* We're going to dump our data to a per-thread file.
     * On Windows we need an absolute path so we place it in
     * the same directory as our library. We could also pass
     * in a path as a client argument.
     */
    data->log =
        log_file_open(client_id, drcontext, NULL /* using client lib path */, "armq_trace",
#ifndef WINDOWS
                      DR_FILE_CLOSE_ON_FORK |
#endif
                          DR_FILE_ALLOW_LARGE);
    data->logf = log_stream_from_file(data->log);
#ifdef OUTPUT_TEXT
    fprintf(data->logf, "pc,tsc,val\n");
#endif
}

static void
event_thread_exit(void *drcontext)
{
    per_thread_t *data;
    instrace(drcontext); /* dump any remaining buffer entries */
    data = drmgr_get_tls_field(drcontext, tls_idx);
    //dr_mutex_lock(mutex);
    //num_refs += data->num_refs;
    //dr_mutex_unlock(mutex);
    log_stream_close(data->logf); /* closes fd too */
    dr_raw_mem_free(data->buf_base, MEM_BUF_SIZE);
    //dr_thread_free(drcontext, data->buf_base, MEM_BUF_SIZE);
    dr_thread_free(drcontext, data, sizeof(per_thread_t));
}

static void
event_exit(void)
{
    //dr_log(NULL, DR_LOG_ALL, 1, "Client 'instrace' num refs seen: " SZFMT "\n", num_refs);
    dr_log(NULL, DR_LOG_ALL, 1, "done\n");
    if (!dr_raw_tls_cfree(tls_offs, INSTRACE_TLS_COUNT))
        DR_ASSERT(false);

    if (!drmgr_unregister_tls_field(tls_idx) ||
        !drmgr_unregister_thread_init_event(event_thread_init) ||
        !drmgr_unregister_thread_exit_event(event_thread_exit) ||
        !drmgr_unregister_bb_instrumentation_event(event_bb_analysis) ||
        drreg_exit() != DRREG_SUCCESS)
        DR_ASSERT(false);

    //dr_mutex_destroy(mutex);
    drmgr_exit();
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    for (int i = 2; argc >= i; i += 2) {
        long pc = strtol(argv[i - 1], NULL, 0);
        int pc_hash = PC_HASH(pc);
        int reg = atoi(argv[i]);
        for (int idx = 0; NSLOTS > idx; ++idx) {
            if (0 == pc_roi[pc_hash][idx]) {
                pc_roi[pc_hash][idx]= pc;
                reg_roi[pc_hash][idx] = reg;
                printf("pc_roi[%d][%d]={%lx, %d}\n", pc_hash, idx, pc, reg);
                break;
            }
        }
    }

    /* We need 2 reg slots beyond drreg's eflags slots => 3 slots */
    drreg_options_t ops = { sizeof(ops), 3, false };
    dr_set_client_name("DynamoRIO Sample Client 'armq_trace'",
                       "http://dynamorio.org/issues");
    if (!drmgr_init() || drreg_init(&ops) != DRREG_SUCCESS)
        DR_ASSERT(false);

    /* Set main module address to filter BBs from lib modules */
    if (true) {
        module_data_t *exe = dr_get_main_module();
        if (NULL != exe) {
            exe_start = exe->start;
        }
        dr_free_module_data(exe);
    }

    /* register events */
    dr_register_exit_event(event_exit);
    if (!drmgr_register_thread_init_event(event_thread_init) ||
        !drmgr_register_thread_exit_event(event_thread_exit) ||
        !drmgr_register_bb_instrumentation_event(event_bb_analysis,
                                                 event_app_instruction, NULL))
        DR_ASSERT(false);

    client_id = id;
    //mutex = dr_mutex_create();

    tls_idx = drmgr_register_tls_field();
    DR_ASSERT(tls_idx != -1);
    /* The TLS field provided by DR cannot be directly accessed from the code cache.
     * For better performance, we allocate raw TLS so that we can directly
     * access and update it with a single instruction.
     */
    if (!dr_raw_tls_calloc(&tls_seg, &tls_offs, INSTRACE_TLS_COUNT, 0))
        DR_ASSERT(false);

    dr_log(NULL, DR_LOG_ALL, 1, "Client 'armq_trace' initializing\n");
}
