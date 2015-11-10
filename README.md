Copyright (c) 2011-2013  University of Texas at Austin. All rights reserved.

## $COPYRIGHT$

Additional copyrights may follow

This file is part of PerfExpert.

PerfExpert is free software: you can redistribute it and/or modify it under
the terms of the The University of Texas at Austin Research License

PerfExpert is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.

Authors: Leonardo Fialho and Ashay Rane

$HEADER$


## Documentation:

See doc/ directory.

## PerfExpert.pptx info

### P14: Performance metric
LCPI(Local Cycles Per Instruction)
Compute upper bounds on CPI contribution for various categories, e.g., branches and memory accesses
	brancches = (BR_INS*BR_lat + BR_MSP*BR_miss_lat)/TOT_INS
	memory_accesses = (L1_DCA*L1_dlat + L2_DCA*L2_lat + L2_DCM*Mem_lat)/TOT_INS
	......


### P38: Conclusion
Output is easy to understand
	Code sections sorted by importance
	The longer the bar, the more important to optimize

### P42: Determining What to Optimize
PerfExpert reports "cost" of
	Data accesses
	Instruction accesses
	Data TLB
	Instructions TLB
	Branch Instructions
	Floating-point instructions
for each key code segment

### P63 & P66 & 68 & 69 & 70: Porting and Installing Outline
PerfExpert configuration(P66)
	Establish categories: Each bar in the output corresponds to a category
	define LCPI formulae for each category
		Overall LCPI = TOT_CYC / TOT_INS
		Upper LCPI bounds by category
			data_accesses = (L1_DCA * L1_dlat + L2_DCA * L2_lat + L2_DCM * Mem_lat) / TOT_INS
			instr_accesses = (L1_ICA * L1_ilat + L2_ICA * L2_lat + L2_ICM * Mem_lat) / TOT_INS
			data_TLB = (TLB_DM * TLB_lat) / TOT_INS
			instr_TLB = (TLB_IM * TLB_lat) / TOT_INS
			branch_instr = (BR_INS * BR_lat + BR_MSP * BR_miss_lat) / TOT_INS
			floating-point_instr = ((FML_INS + FAD_INS) * FP_lat + FDV_INS * FP_slow_lat) / TOT_INS
	select hardware performance counters
		Determine available performance counters
			papi_avail and papi_native_avail
		Select counters and test with microbenchmarks
			CPU cycles (TOT_CYC)
			Instructions committed (TOT_INS)
			L1 data cache accesses (L1_DCA)
			L2 cache data accesses (L2_DCA)
			L2 cache data misses (L2_DCM)
			L1 instr cache accesses (L1_ICA)
			L2 cache instr accesses (L2_ICA)
			L2 cache instr misses (L2_ICM)
			Data TLB misses (TLB_DM)
			Instruction TLB misses (TLB_IM)
			Branch instructions (BR_INS)
			Branch mispredictions (BR_MSP)
			Floating-point add/sub (FAD_INS)
			Floating-point mul (FML_INS)
			Floating-point div/sqrt (FDV_INS)
	measure system parameters: Messure the necessary system properties
		For parameters with little or no variability, look up or measure constant/worst-case value
			CPU frequency
			L1 data cache latency (L1_dlat)
			L1 instr cache latency (L1_ilat)
			L2 cache latency (L2_lat)
			FP add/sub/mul latency (FP_lat)
			FP div/sqrt latency (FP_slow_lat)
			Branch instruction latency (BR_lat)
			Branch penalty (BR_miss_lat)
		For parameters with large range, use conservative value (may have to be tuned)
			TLB miss latency (TLB_lat)
			Memory access latency (Mem_lat)
			Good CPI threshold
The categories, formulae, counters, and parameters that follow are appropriate for Ranger

### P72 & P73: Porting from Ranger to Intel Nehalem(Longhorn)
不同的处理器拥有不同的hardware performance counters，所以切换处理器要更改perfeormance counters
