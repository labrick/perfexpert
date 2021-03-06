/*
 * Copyright (c) 2011-2015  University of Texas at Austin. All rights reserved.
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
 * Authors: Antonio Gomez-Iglesias, Leonardo Fialho and Ashay Rane
 *
 * $HEADER$
 */

#ifdef __cplusplus
extern "C" {
#endif

/* System standard headers */
#include <stdio.h>
#include <strings.h>
#include <sqlite3.h>

/* Modules headers */
#include "vtune_types.h"
#include "vtune_database.h"

/* PerfExpert common headers */
#include "common/perfexpert_cpuinfo.h"
#include "common/perfexpert_alloc.h"
#include "common/perfexpert_constants.h"
#include "common/perfexpert_database.h"
#include "common/perfexpert_list.h"
#include "common/perfexpert_output.h"

/* init the tables, in case they didn't exist */
int init_database(void) {
    char *error = NULL;

    /* Check if the required tables are available */
    char sql[] = "PRAGMA foreign_keys = ON;             \
        CREATE TABLE IF NOT EXISTS perfexpert_hotspot ( \
            perfexpert_id INTEGER NOT NULL,             \
            id            INTEGER PRIMARY KEY,          \
            name          VARCHAR NOT NULL,             \
            type          INTEGER NOT NULL,             \
            profile       VARCHAR NOT NULL,             \
            module        VARCHAR NOT NULL,             \
            file          VARCHAR NOT NULL,             \
            line          INTEGER NOT NULL,             \
            depth         INTEGER NOT NULL,             \
            relevance     INTEGER);                     \
        CREATE TABLE IF NOT EXISTS perfexpert_event (   \
            id            INTEGER PRIMARY KEY AUTOINCREMENT,\
            name          VARCHAR NOT NULL,             \
            thread_id     INTEGER NOT NULL,             \
            mpi_task      INTEGER NOT NULL,             \
            experiment    INTEGER NOT NULL,             \
            value         REAL    NOT NULL,             \
            hotspot_id    INTEGER NOT NULL,             \
        FOREIGN KEY (hotspot_id) REFERENCES perfexpert_hotspot(id));";

    OUTPUT_VERBOSE((5, "%s", _BLUE("Initializing database")));

    if (SQLITE_OK != sqlite3_exec(globals.db, sql, NULL, NULL, &error)) {
        OUTPUT(("%s %s", _ERROR("SQL error"), error));
        sqlite3_free(error);
        return PERFEXPERT_ERROR;
    }

    return PERFEXPERT_SUCCESS;
}

/* Takes the output of a SELECT statement and adds events to
 * the global variable */
int add_event(void *unused, int argc, char **argv, char **event) {
    int i;

    for (i = 0; i < argc; ++i) {
        vtune_event_t * event = NULL;
        char name_md5[33];
        perfexpert_hash_find_str(my_module_globals.events_by_name,
                                 perfexpert_md5_string(argv[i]), event);
        if (event != NULL) {
            /* For whatever reason the event is already in the set, 
               so don't duplicate it
            */
            continue;
        }
        PERFEXPERT_ALLOC(vtune_event_t, event, sizeof(vtune_event_t));
        PERFEXPERT_ALLOC(char, event->name, (strlen(argv[i])));
        strcpy(event->name, argv[i]);
        strcpy(event->name_md5, perfexpert_md5_string(argv[i]));
        perfexpert_add_hash_str(my_module_globals.events_by_name, name_md5, event);
        OUTPUT_VERBOSE((6, "event %s set", _CYAN(event->name)));
    }
}

/* Read the default events for this CPU */
int database_default_events(void) {
    /*
    char sql[MAX_BUFFER_SIZE];
    int family = -1;
    char *error = NULL;
    
    family = perfexpert_cpuinfo_get_family();
    if (family == -1){
        OUTPUT (("%s", _ERROR("incorrect CPU family")));
        return PERFEXPERT_ERROR;  
    }

    sprintf (sql, "SELECT name FROM arch_event, vtune_default_events WHERE "
            "arch_event.id=vtune_default_events.id AND "
            "vtune_default_events.arch=%d", family);

    if (SQLITE_OK != sqlite3_exec(globals.db, sql, add_event, 0, &error)) {
        OUTPUT(("%s %s", _ERROR("SQL error"), error));
        sqlite3_free(error);
        return PERFEXPERT_ERROR;
    }
    */
    return PERFEXPERT_SUCCESS;
}

/* database_hw_events */
int database_hw_events(vtune_hw_profile_t *profile) {
    char *error = NULL, sql[MAX_BUFFER_SIZE];
    int id = 0;
    const char * hp_id; /* hotspot id as returned from the database */

    int family = perfexpert_cpuinfo_get_family();

    vtune_hotspots_t * h = NULL;

    bzero(sql, MAX_BUFFER_SIZE);
    sprintf(sql, "SELECT id FROM perfexpert_hotspot ORDER BY id DESC LIMIT 1");

    OUTPUT_VERBOSE((9, "  SQL: %s", sql));

    if (SQLITE_OK != sqlite3_exec(globals.db, sql, NULL, (void *) id, &error)) {
        OUTPUT(("%s %s", _ERROR("SQL error"), error));
        sqlite3_free(error);
        return PERFEXPERT_ERROR;
    }

    perfexpert_list_for(h, &(profile->hotspots), vtune_hotspots_t) {
        bzero(sql, MAX_BUFFER_SIZE);
        /* TODO(agomez): the module is the second column in the 
                         VTune's profile. collect it.
        */
        sprintf(sql, "INSERT INTO perfexpert_hotspot (perfexpert_id, "
                "id, name, type, profile, module, file, line, depth, "
                "relevance) VALUES (%llu, %llu, '%s', 0, 'profile', '%s', "
                "'file', 0,0,0);", globals.unique_id, id, h->name, h->module);

        OUTPUT_VERBOSE((9, "  Hotspot: %s SQL: %s", _YELLOW(h->name), sql));

        if (SQLITE_OK != sqlite3_exec(globals.db, sql, NULL, NULL, &error)) {
            OUTPUT(("%s %s", _ERROR("SQL error"), error));
            sqlite3_free(error);
            return PERFEXPERT_ERROR;
        }

        vtune_event_t * e = NULL;
        perfexpert_list_for(e, &(h->events), vtune_event_t) {
            bzero(sql, MAX_BUFFER_SIZE);
            sprintf(sql, "INSERT INTO perfexpert_event (name, \
                thread_id, mpi_task, experiment, value, hotspot_id) VALUES \
                ('%s', %d, %d, %d, %llu, %d)", e->name, h->thread,
                h->mpi_rank, globals.cycle, e->value, id);

            OUTPUT_VERBOSE((9, "  [%d] %s SQL: %s", id,
                _YELLOW(e->name), sql));

            /* Insert procedure in database */
            if (SQLITE_OK != sqlite3_exec(globals.db, sql, NULL, NULL, &error)) {
                OUTPUT(("%s %s", _ERROR("SQL error"), error));
                sqlite3_free(error);
                return PERFEXPERT_ERROR;
            }
        }
        id++;
    }

    return PERFEXPERT_SUCCESS;
}

#ifdef __cplusplus
}
#endif

// EOF
