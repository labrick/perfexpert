/*
 * Copyright (c) 2011-2013  University of Texas at Austin. All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * This file is part of PerfExpert.
 *
 * PerfExpert is free software: you can redistribute it and/or modify it under
 * the terms of the The University of Texas at Austin Research License
 * 
 * PerfExpert is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.
 * 
 * Authors: Leonardo Fialho and Ashay Rane
 *
 * $HEADER$
 */

#ifndef SNIFFER_H_
#define SNIFFER_H_

#define LCPI_DEF_LENGTH 600

#define BOOL unsigned char
#define EVENT_NOT_PRESENT ((BOOL) 0)
#define EVENT_PRESENT     ((BOOL) 1)
#define EVENT_USED        ((BOOL) 2)
#define EVENT_ADDED       ((BOOL) 3)

#define USE_EVENT(X)       (event_list[X].use = EVENT_USED)		// 准备采用该事件
#define ADD_EVENT(X)       (event_list[X].use = EVENT_ADDED)	// 事件已加入事件组
#define DOWNGRADE_EVENT(X) (event_list[X].use = EVENT_PRESENT)	

#define IS_EVENT_AVAILABLE(X) (X < EVENT_COUNT && event_list[X].use != EVENT_NOT_PRESENT)
#define IS_EVENT_USED(X)      (X < EVENT_COUNT && event_list[X].use == EVENT_USED)
#define IS_EVENT_ADDED(X)     (X < EVENT_COUNT && event_list[X].use == EVENT_ADDED)

typedef struct lcpi {
        char overall[LCPI_DEF_LENGTH];
        char ratio_floating_point[LCPI_DEF_LENGTH];
        char ratio_data_accesses[LCPI_DEF_LENGTH];
        char gflops_overall[LCPI_DEF_LENGTH];
        char gflops_packed[LCPI_DEF_LENGTH];
        char gflops_scalar[LCPI_DEF_LENGTH];
        char data_accesses_overall[LCPI_DEF_LENGTH];
        char data_accesses_L1_hits[LCPI_DEF_LENGTH];
        char data_accesses_L2_hits[LCPI_DEF_LENGTH];
        char data_accesses_L3_hits[LCPI_DEF_LENGTH];
        char data_accesses_LLC_misses[LCPI_DEF_LENGTH];
        char instruction_accesses_overall[LCPI_DEF_LENGTH];
        char instruction_accesses_L1_hits[LCPI_DEF_LENGTH];
        char instruction_accesses_L2_hits[LCPI_DEF_LENGTH];
        char instruction_accesses_L2_misses[LCPI_DEF_LENGTH];
        char data_TLB_overall[LCPI_DEF_LENGTH];
        char instruction_TLB_overall[LCPI_DEF_LENGTH];
        char branch_instructions_overall[LCPI_DEF_LENGTH];
        char branch_instructions_correctly_predicted[LCPI_DEF_LENGTH];
        char branch_instructions_mispredicted[LCPI_DEF_LENGTH];
        char floating_point_instr_overall [LCPI_DEF_LENGTH];
        char floating_point_instr_fast_FP_instr[LCPI_DEF_LENGTH];
        char floating_point_instr_slow_FP_instr[LCPI_DEF_LENGTH];
} lcpi_t;

typedef struct event_entry {
    int PAPI_event_code;
    unsigned int sampling_freq;
    unsigned short use, sampling_category_id;
    char* PAPI_event_name;
} event_entry_t;

// Lower numbering implying greater importance
enum {
    SAMPLING_CATEGORY_01 = 0,
    SAMPLING_CATEGORY_02,
    SAMPLING_CATEGORY_03,
    SAMPLING_CATEGORY_04,
    SAMPLING_CATEGORY_COUNT
};

unsigned int sampling_limits[SAMPLING_CATEGORY_COUNT + 1] = {
    99999999,
    9999999,
    5000000,
    500000,
    100000
};

enum {
    // General, 2 events
    TOT_INS = 0,
    TOT_CYC,
    // Cache, 13 events
    LD_INS,
    L1_DCA,
    L2_DCA,
    L2_TCA,
    L1_ICA,
    L2_ICA,
    L2_DCM,
    L2_ICM,
    L2_TCM,
    ICACHE,
    L2_RQSTS_DEMAND_DATA_RD_HIT,
    L3_TCM,
    L3_TCA,
    // TLB, 4 events
    TLB_DM,
    TLB_IM,
    DTLB_LOAD_MISSES_WALK_DURATION,
    ITLB_MISSES_WALK_DURATION,
    DTLB_LOAD_MISSES_CAUSES_A_WALK,
    DTLB_STORE_MISSES_CAUSES_A_WALK,
    // Instruction branches, 2 events
    BR_INS,
    BR_MSP,
    // Float-point
    FP_INS,
    FML_INS,
    FDV_INS,
    FAD_INS,
    L1I_CYCLES_STALLED,
    ARITH_CYCLES_DIV_BUSY,
    FP_COMP_OPS_EXE_X87,
    FP_COMP_OPS_EXE_SSE_FP,
    FP_COMP_OPS_EXE_SSE_FP_PACKED,
    FP_COMP_OPS_EXE_SSE_FP_SCALAR,
    FP_COMP_OPS_EXE_SSE_SINGLE_PRECISION,
    FP_COMP_OPS_EXE_SSE_DOUBLE_PRECISION,
    RETIRED_SSE_OPERATIONS_ALL,
    RETIRED_SSE_OPS_ALL,
    SSEX_UOPS_RETIRED_PACKED_DOUBLE,
    SSEX_UOPS_RETIRED_PACKED_SINGLE,
    SSEX_UOPS_RETIRED_SCALAR_DOUBLE,
    SSEX_UOPS_RETIRED_SCALAR_SINGLE,
    SIMD_COMP_INST_RETIRED_PACKED_DOUBLE,
    SIMD_COMP_INST_RETIRED_PACKED_SINGLE,
    SIMD_COMP_INST_RETIRED_SCALAR_DOUBLE,
    SIMD_COMP_INST_RETIRED_SCALAR_SINGLE,
    FP_COMP_OPS_EXE_SSE_FP_PACKED_DOUBLE,
    FP_COMP_OPS_EXE_SSE_FP_SCALAR_SINGLE,
    FP_COMP_OPS_EXE_SSE_PACKED_SINGLE,
    FP_COMP_OPS_EXE_SSE_SCALAR_DOUBLE,
    SIMD_FP_256_PACKED_SINGLE,
    SIMD_FP_256_PACKED_DOUBLE,
    SIMD_FP_256_PACKED_SINGLE_PACKED_DOUBLE,
    ARITH,
    // Do not remothe this!
    PURE_EVENT_COUNT
};

enum {
    // Dummy counters
    SSEX_UOPS_RETIRED_SCALAR_DOUBLE_SCALAR_SINGLE = PURE_EVENT_COUNT,
    SIMD_COMP_INST_RETIRED_SCALAR_SINGLE_SCALAR_DOUBLE,
    FP_COMP_OPS_EXE_SSE_DOUBLE_PRECISION_SSE_FP_SSE_FP_PACKED_SSE_FP_SCALAR_SSE_SINGLE_PRECISION_X87,
    FP_COMP_OPS_EXE_SSE_FP_SCALAR_SINGLE_SSE_SCALAR_DOUBLE,
    EVENT_COUNT
};

event_entry_t event_list[EVENT_COUNT] = {
    // General, 2 events
    -1, 0, 0, SAMPLING_CATEGORY_01, "PAPI_TOT_INS",
    -1, 0, 0, SAMPLING_CATEGORY_01, "PAPI_TOT_CYC",
    // Cache, 13 events
    -1, 0, 0, SAMPLING_CATEGORY_02, "PAPI_LD_INS",
    -1, 0, 0, SAMPLING_CATEGORY_02, "PAPI_L1_DCA",
    -1, 0, 0, SAMPLING_CATEGORY_03, "PAPI_L2_DCA",
    -1, 0, 0, SAMPLING_CATEGORY_03, "PAPI_L2_TCA",
    -1, 0, 0, SAMPLING_CATEGORY_02, "PAPI_L1_ICA",
    -1, 0, 0, SAMPLING_CATEGORY_03, "PAPI_L2_ICA",
    -1, 0, 0, SAMPLING_CATEGORY_03, "PAPI_L2_DCM",
    -1, 0, 0, SAMPLING_CATEGORY_03, "PAPI_L2_ICM",
    -1, 0, 0, SAMPLING_CATEGORY_03, "PAPI_L2_TCM",
    -1, 0, 0, SAMPLING_CATEGORY_02, "ICACHE",
    -1, 0, 0, SAMPLING_CATEGORY_03, "L2_RQSTS:DEMAND_DATA_RD_HIT",
    -1, 0, 0, SAMPLING_CATEGORY_03, "PAPI_L3_TCM",
    -1, 0, 0, SAMPLING_CATEGORY_03, "PAPI_L3_TCA",
    // TLB, 4 events
    -1, 0, 0, SAMPLING_CATEGORY_04, "PAPI_TLB_DM",
    -1, 0, 0, SAMPLING_CATEGORY_04, "PAPI_TLB_IM",
    -1, 0, 0, SAMPLING_CATEGORY_04, "DTLB_LOAD_MISSES:WALK_DURATION",
    -1, 0, 0, SAMPLING_CATEGORY_04, "ITLB_MISSES:WALK_DURATION",
    -1, 0, 0, SAMPLING_CATEGORY_04, "DTLB_LOAD_MISSES:CAUSES_A_WALK",
    -1, 0, 0, SAMPLING_CATEGORY_04, "DTLB_STORE_MISSES:CAUSES_A_WALK",
    // Instruction branches, 2 events
    -1, 0, 0, SAMPLING_CATEGORY_03, "PAPI_BR_INS",
    -1, 0, 0, SAMPLING_CATEGORY_03, "PAPI_BR_MSP",
    // Float-point
    -1, 0, 0, SAMPLING_CATEGORY_02, "PAPI_FP_INS",
    -1, 0, 0, SAMPLING_CATEGORY_02, "PAPI_FML_INS",
    -1, 0, 0, SAMPLING_CATEGORY_02, "PAPI_FDV_INS",
    -1, 0, 0, SAMPLING_CATEGORY_02, "PAPI_FAD_INS",
    -1, 0, 0, SAMPLING_CATEGORY_01, "L1I_CYCLES_STALLED",
    -1, 0, 0, SAMPLING_CATEGORY_02, "ARITH:CYCLES_DIV_BUSY",
    -1, 0, 0, SAMPLING_CATEGORY_02, "FP_COMP_OPS_EXE:X87",
    -1, 0, 0, SAMPLING_CATEGORY_02, "FP_COMP_OPS_EXE:SSE_FP",
    -1, 0, 0, SAMPLING_CATEGORY_02, "FP_COMP_OPS_EXE:SSE_FP_PACKED",
    -1, 0, 0, SAMPLING_CATEGORY_02, "FP_COMP_OPS_EXE:SSE_FP_SCALAR",
    -1, 0, 0, SAMPLING_CATEGORY_02, "FP_COMP_OPS_EXE:SSE_SINGLE_PRECISION",
    -1, 0, 0, SAMPLING_CATEGORY_02, "FP_COMP_OPS_EXE:SSE_DOUBLE_PRECISION",
    -1, 0, 0, SAMPLING_CATEGORY_02, "RETIRED_SSE_OPERATIONS:ALL",
    -1, 0, 0, SAMPLING_CATEGORY_02, "RETIRED_SSE_OPS:ALL",
    -1, 0, 0, SAMPLING_CATEGORY_02, "SSEX_UOPS_RETIRED:PACKED_DOUBLE",
    -1, 0, 0, SAMPLING_CATEGORY_02, "SSEX_UOPS_RETIRED:PACKED_SINGLE",
    -1, 0, 0, SAMPLING_CATEGORY_02, "SSEX_UOPS_RETIRED:SCALAR_DOUBLE",
    -1, 0, 0, SAMPLING_CATEGORY_02, "SSEX_UOPS_RETIRED:SCALAR_SINGLE",
    -1, 0, 0, SAMPLING_CATEGORY_02, "SIMD_COMP_INST_RETIRED:PACKED_DOUBLE",
    -1, 0, 0, SAMPLING_CATEGORY_02, "SIMD_COMP_INST_RETIRED:PACKED_SINGLE",
    -1, 0, 0, SAMPLING_CATEGORY_02, "SIMD_COMP_INST_RETIRED:SCALAR_DOUBLE",
    -1, 0, 0, SAMPLING_CATEGORY_02, "SIMD_COMP_INST_RETIRED:SCALAR_SINGLE",
    -1, 0, 0, SAMPLING_CATEGORY_02, "FP_COMP_OPS_EXE:SSE_FP_PACKED_DOUBLE",
    -1, 0, 0, SAMPLING_CATEGORY_02, "FP_COMP_OPS_EXE:SSE_FP_SCALAR_SINGLE",
    -1, 0, 0, SAMPLING_CATEGORY_02, "FP_COMP_OPS_EXE:SSE_PACKED_SINGLE",
    -1, 0, 0, SAMPLING_CATEGORY_02, "FP_COMP_OPS_EXE:SSE_SCALAR_DOUBLE",
    -1, 0, 0, SAMPLING_CATEGORY_02, "SIMD_FP_256:PACKED_SINGLE",
    -1, 0, 0, SAMPLING_CATEGORY_02, "SIMD_FP_256:PACKED_DOUBLE",
    -1, 0, 0, SAMPLING_CATEGORY_02, "SIMD_FP_256:PACKED_SINGLE:PACKED_DOUBLE",
    -1, 0, 0, SAMPLING_CATEGORY_02, "ARITH",
    /* dummy counters ahead */
    -1, 0, 0, SAMPLING_CATEGORY_02, "SSEX_UOPS_RETIRED:SCALAR_DOUBLE:SCALAR_SINGLE",
    -1, 0, 0, SAMPLING_CATEGORY_02, "SIMD_COMP_INST_RETIRED:SCALAR_SINGLE:SCALAR_DOUBLE",
    -1, 0, 0, SAMPLING_CATEGORY_02, "FP_COMP_OPS_EXE:SSE_DOUBLE_PRECISION:SSE_FP:SSE_FP_PACKED:SSE_FP_SCALAR:SSE_SINGLE_PRECISION:X87",
    -1, 0, 0, SAMPLING_CATEGORY_02, "FP_COMP_OPS_EXE:SSE_FP_SCALAR_SINGLE:SSE_SCALAR_DOUBLE"
};

/* Function declarations */
void counter_err(char *lcpi);
int test_counter(unsigned counter);
int is_derived(int event_code);
int get_prime(unsigned short sampling_category, unsigned int start);
int attach_sampling_rate(int categories[SAMPLING_CATEGORY_COUNT]);
void get_overall(char *output);
void get_ratio_floating_point(char *output);
void get_ratio_data_accesses(char *output);
void get_gflops(char *overall, char *packed, char *scalar);
void get_data_accesses(char *overall, char *L1_hits, char *L2_hits,
    char *L3_hits, char *LLC_misses);
void get_instruction_accesses(char *overall, char *L1_hits, char *L2_hits,
    char *L2_misses);
void get_tlb(char *data, char *instruction);
void get_branch_instructions(char *overall, char *predicted,
    char *mispredicted);
void get_floating_point_instr(char *overall, char *slow, char *fast);
void write_experiment(void);
void write_lcpi(lcpi_t lcpi);

#endif /* SNIFFER_H_ */
