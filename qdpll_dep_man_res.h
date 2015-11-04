/*
 This file is part of DepQBF.

 DepQBF, a solver for quantified boolean formulae (QBF).

 Copyright 2010, 2011, 2012, 2013, 2014, 2015
 Florian Lonsing, Johannes Kepler University, Linz, Austria and
 Vienna University of Technology, Vienna, Austria.

 Copyright 2012 Aina Niemetz, Johannes Kepler University, Linz, Austria.

 DepQBF is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or (at
 your option) any later version.

 DepQBF is distributed in the hope that it will be useful, but
 WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with DepQBF.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
#############################################################################################################
In order to compile this file outside of the DepQBF environment, comment out everything that is marked DEPQBF.
#############################################################################################################
*/

#ifndef QDPLL_DEPMAN_RES_H_INCLUDED
#define QDPLL_DEPMAN_RES_H_INCLUDED

// ###########################################################DEPQBF:
#include "qdpll_dep_man_generic.h"
// ###########################################################DEPQBF:
#include "qdpll_pcnf.h"
// ###########################################################DEPQBF:
#include "qdpll_mem.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct
{
    bool active;
    int num_active_incoming_edges;
    /*
    The field is_source exists just for convenience, in fact
    is_source == (num_active_incoming_edges == 0)
    */
    bool is_source;
    /*
    Helper variable pointing to the index in the list or sources
    where this variable is stored. If not a source, the value is -1.
    */
    int source_index;
}
VarStatus;

typedef struct
{
    // ###########################################################DEPQBF:
    QDPLLDepManGeneric dmg;
    QDPLLMemMan * mm;
    QDPLLPCNF *pcnf;

    bool is_initialized;

    int n; // number of variables
    int ** adjacency_list;
    VarStatus * var_status;
    /*
    sources is a set of current sources (or alternatively candidates) of the DAG.
    Addition, membership (via testing the is_source value) and removal take O(1)
    */
    int * sources;
    int num_sources;
    int current_candidate;
}
QDPLLDepManRes;


/* Res Dependency manager. */

/* Creates a qdag dependency manager. Last parameter indicates whether
to print dependencies by explicit search of CNF or by graph (for
testing only). */
// ###########################################################DEPQBF:
QDPLLDepManRes *qdpll_res_dep_man_create (QDPLLMemMan * mm,
                                            QDPLLPCNF * pcnf,
                                            QDPLLDepManType type,
                                            int print_deps_by_search,
                                            QDPLL * qdpll);

void qdpll_res_dep_man_init(QDPLLDepManRes * dmr, char ** filename);
void qdpll_res_dep_man_reset(QDPLLDepManRes * dmr);
int qdpll_res_dep_man_depends(QDPLLDepManRes * dmr, int e, int u);
VarID qdpll_res_dep_man_get_candidate(QDPLLDepManRes * dmr);
void qdpll_res_dep_man_notify_inactive(QDPLLDepManRes * dmr, int variable);
void qdpll_res_dep_man_notify_active(QDPLLDepManRes * dmr, int variable);
void qdpll_res_dep_man_init_sources(QDPLLDepManRes * dmr);
int qdpll_res_dep_man_is_candidate(QDPLLDepManRes *dmr, int variable);

/* Deletes a Res dependency manager. */
// ###########################################################DEPQBF:
void qdpll_res_dep_man_delete (QDPLLDepManRes * dm);

#endif
