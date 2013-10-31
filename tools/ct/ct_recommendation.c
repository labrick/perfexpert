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

#ifdef __cplusplus
extern "C" {
#endif

/* PerfExpert headers */
#include "ct.h"
#include "perfexpert_constants.h"
#include "perfexpert_list.h"
#include "perfexpert_output.h"

/* apply_recommendations */
int apply_recommendations(fragment_t *fragment) {
    recommendation_t *recommendation = NULL;
    int rc = PERFEXPERT_NO_TRANS;

    OUTPUT_VERBOSE((4, "%s", _BLUE("Applying recommendations")));

    /* For each recommendation in this fragment... */
    recommendation = (recommendation_t *)perfexpert_list_get_first(
        &(fragment->recommendations));
    while ((perfexpert_list_item_t *)recommendation !=
        &(fragment->recommendations.sentinel)) {

        /* Skip if other recommendation has already been applied */
        if (PERFEXPERT_SUCCESS == rc) {
            OUTPUT_VERBOSE((8, "   [%s ] [%d] (code already changed)",
                _MAGENTA("SKIP"), recommendation->id));
            goto move_on;
        }

        /* Apply transformations */
        switch (apply_transformations(fragment, recommendation)) {
            case PERFEXPERT_ERROR:
                OUTPUT_VERBOSE((8, "   [%s] [%d]", _BOLDYELLOW("ERROR"),
                    recommendation->id));
                return PERFEXPERT_ERROR;

            case PERFEXPERT_FAILURE:
                OUTPUT_VERBOSE((8, "   [%s ] [%d]", _BOLDRED("FAIL"),
                    recommendation->id));
                break;

            case PERFEXPERT_NO_TRANS:
                OUTPUT_VERBOSE((8, "   [%s ] [%d] (no transformations)",
                    _MAGENTA("SKIP"), recommendation->id));
                break;

            case PERFEXPERT_SUCCESS:
                OUTPUT_VERBOSE((8, "   [ %s  ] [%d]", _BOLDGREEN("OK"),
                    recommendation->id));
                rc = PERFEXPERT_SUCCESS;
                break;

            default:
                break;
        }

        /* Move to the next code recommendation */
        move_on:
        recommendation = (recommendation_t *)perfexpert_list_get_next(
            recommendation);
    }
    return rc;
}

#ifdef __cplusplus
}
#endif

// EOF