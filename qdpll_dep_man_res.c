#include "qdpll_dep_man_res.h"

void qdpll_res_dep_man_init (QDPLLDepManRes * dmr, char ** filename){
    /*
    The adjacency list for variable k is adjacency_list[k], for
    the sake of simplicity. Therefore, adjacency_list[0] is just
    NULL.
    */
    int capacity = 2; // default number of variables
    dmr->adjacency_list = malloc(sizeof(int*) * (capacity + 1));

    FILE *f = fopen(filename, "r");

    int active_variable = 0;

    int q = -1, num_edges = 1;
    dmr->n = 0;

    while(fscanf(f, "%d:", &active_variable) != EOF){
        if(active_variable > dmr->n)
            dmr->n = active_variable;
        if(active_variable > capacity){
            // need to alloc more space for variables
            capacity *= 2;
            realloc(dmr->adjacency_list, sizeof(int*) * (capacity + 1));
        }
        num_edges = 1;
        dmr->adjacency_list[active_variable] = malloc(sizeof(int) * num_edges);
        /*
        The first element of the adjacency list is its size.
        */
        dmr->adjacency_list[active_variable][0] = 0;

        int i = 1;

        fscanf(f, "%d", &q);
        while(q != 0){
            if(i >= num_edges){
                num_edges *= 2;
                realloc(dmr->adjacency_list[active_variable], sizeof(int) * num_edges);
            }
            dmr->adjacency_list[active_variable][i] = q;
            i++;
            fscanf(f, "%d", &q);
        }
        num_edges = i;
        realloc(dmr->adjacency_list[active_variable], sizeof(int) * num_edges);
        dmr->adjacency_list[active_variable][0] = num_edges;
    }
    realloc(dmr->adjacency_list, sizeof(int*) * (dmr->n + 1));
    fclose(f);

    // allocate all other structures with known variable count
    dmr->var_status = malloc((dmr->n + 1) * sizeof(VarStatus));
    dmr->sources = malloc(dmr->n * sizeof(int));
    dmr->num_sources = 0;
    int i;
    for(i = 1; i <= dmr->n; i++){
        dmr->var_status[i].active = true;
    }

    qdpll_res_dep_man_init_sources(dmr);

    dmr->current_candidate = 0;
    printf("%d %d\n", dmr->num_sources, dmr->current_candidate);

    dmr->is_initialized = true;
    return dmr;
}

int qdpll_res_dep_man_depends(QDPLLDepManRes * dmr, int e, int u){
    int i;
    for(i = 1; i < dmr->adjacency_list[u][0]; i++){
        if(dmr->adjacency_list[u][i] == e)
            return 1;
    }
    return 0;
}

void qdpll_res_dep_man_init_sources(QDPLLDepManRes * dmr){
    int i, j;
    for(i = 1; i <= dmr->n; i++){
        dmr->var_status[i].is_source = true;
        dmr->var_status[dmr->adjacency_list[i][j]].num_active_incoming_edges = 0;
        dmr->var_status[i].source_index = -1;
    }
    for(i = 1; i <= dmr->n; i++){
        for(j = 1; j < dmr->adjacency_list[i][0]; j++){
            dmr->var_status[dmr->adjacency_list[i][j]].is_source = false;
            dmr->var_status[dmr->adjacency_list[i][j]].num_active_incoming_edges++;
        }
    }
    j = 0;
    for(i = 1; i <= dmr->n; i++){
        if(dmr->var_status[i].is_source){
            dmr->sources[j] = i;
            dmr->var_status[i].source_index = j;
            j++;
        }
    }
    dmr->num_sources = j;
}

VarID qdpll_res_dep_man_get_candidate(QDPLLDepManRes * dmr){
    if(dmr->current_candidate == dmr->num_sources){
        /*
        WARNING
        If we already returned all candidates, return -1,
        which is not a valid variable identifier. This function
        is specified to return NULL in this case, but I don't know
        how to overcome the type incompatibility.
        */
        return -1;
    }
    dmr->current_candidate++;
    return dmr->sources[dmr->current_candidate - 1];
}

int qdpll_res_dep_man_is_candidate(QDPLLDepManRes *dmr, int variable){
    return dmr->var_status[variable].is_source;
}

void qdpll_res_dep_man_notify_inactive(QDPLLDepManRes * dmr, int variable){
    dmr->var_status[variable].active = false;
    int j, target;
    /*
    Update all children to see if any become sources.
    */
    for(j = 1; j < dmr->adjacency_list[variable][0]; j++){
        target = dmr->adjacency_list[variable][j];
        dmr->var_status[target].num_active_incoming_edges--;
        if(dmr->var_status[target].num_active_incoming_edges == 0){
            dmr->var_status[target].is_source = true;
            dmr->sources[dmr->num_sources] = target;
            dmr->var_status[target].source_index = dmr->num_sources;
            dmr->num_sources++;
        }
    }
}

void qdpll_res_dep_man_notify_active(QDPLLDepManRes * dmr, int variable){
    dmr->var_status[variable].active = true;
    int i,j, target;
    /*
    Update all children to see if any stop being sources.
    */
    for(j = 1; j < dmr->adjacency_list[variable][0]; j++){
        target = dmr->adjacency_list[variable][j];
        dmr->var_status[target].num_active_incoming_edges++;
        if(dmr->var_status[target].num_active_incoming_edges == 1){
            /*
            If we have exactly one active incoming edge, we must have
            added it just now, so target stops being a source
            */
            dmr->var_status[target].is_source = false;
            dmr->var_status[target].source_index = -1;
            for(i = 0; i < dmr->num_sources; i++){
                if(dmr->sources[i] == target){
                    dmr->num_sources--;
                    dmr->sources[i] = dmr->sources[dmr->num_sources];

                    // update source_index for the moved variable
                    dmr->var_status[dmr->sources[i]].source_index = i;
                    break;
                }
            }
        }
    }
}

void qdpll_res_dep_man_reset(QDPLLDepManRes * dmr){
    dmr->is_initialized = false;
    int i;
    for(i = 1; i <= dmr->n; i++){
        free(dmr->adjacency_list[i]);
    }
    free(dmr->adjacency_list);
    free(dmr->var_status);
    free(dmr->sources);
}

void qdpll_res_dep_man_notify_init_variable(QDPLLDepManRes * dmr, int variable){
    // TODO
    return;
}

void qdpll_res_dep_man_notify_reset_variable(QDPLLDepManRes * dmr, int variable){
    // TODO
    return;
}

int qdpll_res_dep_man_is_init(QDPLLDepManRes * dmr){
    return dmr->is_initialized;
}

void
qdpll_res_dep_man_print_deps_by_graph (QDPLLDepManGeneric * dmg, VarID id)
{
  // TODO
  return;
}

/* 'print_deps' function. */
void
qdpll_res_dep_man_print_deps_by_search (QDPLLDepManGeneric * dmg, VarID id)
{
  // TODO
  return;
}

static void qdpll_res_dep_man_dump_dep_graph (QDPLLDepManGeneric * dmg){
    return;
}

static VarID qdpll_res_dep_man_get_class_rep (QDPLLDepManGeneric * dmg, VarID x, const unsigned int ufoffset){
    return -1;
}

static void qdpll_res_dep_man_reduce_lits (QDPLLDepManGeneric * dmg, LitIDStack ** lit_stack,
                           LitIDStack ** lit_stack_tmp,
                           const QDPLLQuantifierType other_type,
                           const int lits_sorted)
{
    // TODO!!
    QDPLLDepManRes *dm = (QDPLLDepManRes *) dmg;
#if COMPUTE_TIMES
    const double start = time_stamp ();
#endif
    //type_reduce (dmg, lit_stack, lit_stack_tmp, other_type, lits_sorted);
#if COMPUTE_TIMES
    dm->dmg.qdpll->time_stats.total_reduce_time += (time_stamp () - start);
#endif
}

static LitID * qdpll_res_dep_man_get_candidates (QDPLLDepManGeneric * dmg)
{
  // TODO
  QDPLLDepManRes *dm = (QDPLLDepManRes *) dmg;

  return NULL;
}

// ###########################################################DEPQBF:

QDPLLDepManRes * qdpll_res_dep_man_create (QDPLLMemMan * mm, QDPLLPCNF * pcnf, QDPLLDepManType type, int print_deps_by_search, QDPLL * qdpll)
{
  /*
  A copy of the create method for QDPLLDepManQDAG
  */
  QDPLLDepManRes *dm = (QDPLLDepManRes *) qdpll_malloc (mm, sizeof (QDPLLDepManRes));

  dm->is_initialized = false;

  dm->mm = mm;
  dm->pcnf = pcnf;

  dm->dmg.qdpll = qdpll;
  dm->dmg.type = type;

  /* Set fptrs */
  dm->dmg.init = qdpll_res_dep_man_init;
  dm->dmg.reset = qdpll_res_dep_man_reset;
  dm->dmg.get_candidate = qdpll_res_dep_man_get_candidate;
  dm->dmg.notify_inactive = qdpll_res_dep_man_notify_inactive;
  dm->dmg.notify_active = qdpll_res_dep_man_notify_active;
  dm->dmg.is_candidate = qdpll_res_dep_man_is_candidate;
  dm->dmg.notify_init_variable = qdpll_res_dep_man_notify_init_variable; // TODO
  dm->dmg.notify_reset_variable = qdpll_res_dep_man_notify_reset_variable; // TODO
  dm->dmg.is_init = qdpll_res_dep_man_is_init; // TODO

  // TODO
  if (print_deps_by_search)
    dm->dmg.print_deps = qdpll_res_dep_man_print_deps_by_search;
  else
    dm->dmg.print_deps = qdpll_res_dep_man_print_deps_by_graph;

  dm->dmg.dump_dep_graph = qdpll_res_dep_man_dump_dep_graph; // TODO
  dm->dmg.depends = qdpll_res_dep_man_depends;
  dm->dmg.get_class_rep = qdpll_res_dep_man_get_class_rep; // TODO
  dm->dmg.reduce_lits = qdpll_res_dep_man_reduce_lits; // TODO
  dm->dmg.get_candidates = qdpll_res_dep_man_get_candidates; // TODO

  return dm;
}


void
qdpll_qdag_res_dep_man_delete (QDPLLDepManRes * dm)
{
  qdpll_free (dm->mm, dm, sizeof (QDPLLDepManRes));
}


