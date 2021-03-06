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

#ifndef PERFEXPERT_MODULE_MACVEC_TYPES_H_
#define PERFEXPERT_MODULE_MACVEC_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

/* PerfExpert common headers */
#include "common/perfexpert_hash.h"
#include "common/perfexpert_list.h"

/* Module types */
typedef struct {
    char *name;
    char name_md5[33];
    double instructions;
    double importance;
    double cycles;
    perfexpert_hash_handle_t hh_str;
} macvec_module_t;

typedef struct {
    volatile perfexpert_list_item_t *next;
    volatile perfexpert_list_item_t *prev;
    long long int id;
    char *name;
    hotspot_type_t type;
    macvec_module_t *module;
    char *file;
    int line;
    int depth;
    int experiments;
    double max_inst;
    double min_inst;
    double instructions;
    double importance;
    double variance;
    double cycles;
} macvec_hotspot_t;

typedef struct {
    volatile perfexpert_list_item_t *next;
    volatile perfexpert_list_item_t *prev;
    char *name;
    double instructions;
    double cycles;
    macvec_module_t *modules_by_name;
    perfexpert_list_t hotspots; /* for both procedures and loops */
} macvec_profile_t;

#ifdef __cplusplus
}
#endif

#endif /* PERFEXPERT_MODULE_MACVEC_TYPES_H_ */
