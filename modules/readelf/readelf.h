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

#ifndef PERFEXPERT_MODULE_READELF_H_
#define PERFEXPERT_MODULE_READELF_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Tools headers */
#include "tools/perfexpert/perfexpert_types.h"

#ifdef PROGRAM_PREFIX
#undef PROGRAM_PREFIX
#endif
#define PROGRAM_PREFIX "[perfexpert_module_readelf]"

/* Module interface */
int module_load(void);
int module_init(void);
int module_fini(void);
int module_measurements(void);

/* Module functions */
static int database_write(const char *compiler, int language);

#ifdef __cplusplus
}
#endif

#endif /* PERFEXPERT_MODULE_READELF_H_ */
