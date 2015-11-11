#include "qdpll_dep_man_res.h"

void qdpll_res_dep_man_init (QDPLLDepManGeneric * dm){
    /*
    The adjacency list for variable k is adjacency_list[k], for
    the sake of simplicity. Therefore, adjacency_list[0] is just
    NULL.

    The whole stack-like behaviour of adjacency_list is explicitly
    implemented here - TODO implement using QDPLL_STACK.

    Using classical mallocs and reallocs instead of the qdpll_ versions.
    TODO implement it using the qdpll_ versions.
    */
    QDPLLDepManRes * dmr = (QDPLLDepManRes *) dm;

    /*
    TODO adjust to be the number of variables in the PCNF,
    because now we know it beforehand.
    */
    int capacity = 2; // default number of variables
    dmr->adjacency_list = malloc(sizeof(int*) * (capacity + 1));

    //printf("%s\n", dm->qdpll->options.depman_res_dep_filename);
    FILE *f = fopen(dm->qdpll->options.depman_res_dep_filename, "r");

    VarID active_variable = 0;

    int q = -1, num_edges = 1;
    dmr->n = 0;

    while(fscanf(f, "%d:", &active_variable) != EOF){
        if(active_variable > dmr->n)
            dmr->n = active_variable;
        if(active_variable > capacity){
            // need to alloc more space for variables
            capacity *= 2;
            dmr->adjacency_list = realloc(dmr->adjacency_list, sizeof(int*) * (capacity + 1));
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
                dmr->adjacency_list[active_variable] = realloc(dmr->adjacency_list[active_variable], sizeof(int) * num_edges);
            }
            dmr->adjacency_list[active_variable][i] = q;
            i++;
            fscanf(f, "%d", &q);
        }
        num_edges = i;
        dmr->adjacency_list[active_variable] = realloc(dmr->adjacency_list[active_variable], sizeof(int) * num_edges);
        dmr->adjacency_list[active_variable][0] = num_edges;
    }
    dmr->adjacency_list = realloc(dmr->adjacency_list, sizeof(int*) * (dmr->n + 1));
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

    dmr->is_initialized = true;
}

int qdpll_res_dep_man_depends(QDPLLDepManGeneric * dm, VarID u, VarID e){

    QDPLLDepManRes * dmr = (QDPLLDepManRes *) dm;

    unsigned int i;
    for(i = 1; i < dmr->adjacency_list[u][0]; i++){
        if(dmr->adjacency_list[u][i] == e)
            return 1;
    }
    return 0;
}

/*
This function must be called during init to initialize the source
detection system - to find the original sources in the graph.
*/
void qdpll_res_dep_man_init_sources(QDPLLDepManRes * dmr){
    unsigned int i, j;
    for(i = 1; i <= dmr->n; i++){
        dmr->var_status[i].is_source = true;
        dmr->var_status[i].num_active_incoming_edges = 0;
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

VarID qdpll_res_dep_man_get_candidate(QDPLLDepManGeneric * dm){

    QDPLLDepManRes * dmr = (QDPLLDepManRes *) dm;

    if(dmr->current_candidate == dmr->num_sources){
        /*
        WARNING
        If we already returned all candidates, return NULL. This
        should be the specified behaviour, but I am not entirely
        sure it's correct.
        */
        dmr->current_candidate = 0;
        return NULL;
    }
    dmr->current_candidate++;
    return dmr->sources[dmr->current_candidate - 1];
}

int qdpll_res_dep_man_is_candidate(QDPLLDepManGeneric *dm, VarID variable){

    QDPLLDepManRes * dmr = (QDPLLDepManRes *) dm;

    return dmr->var_status[variable].is_source;
}

void qdpll_res_dep_man_notify_inactive(QDPLLDepManGeneric * dm, VarID variable){

    QDPLLDepManRes * dmr = (QDPLLDepManRes *) dm;

    dmr->var_status[variable].active = false;
    unsigned int j;
    VarID target;
    /*
    Update all children to see if any become sources.
    */
    for(j = 1; j < dmr->adjacency_list[variable][0]; j++){
        target = dmr->adjacency_list[variable][j];
        dmr->var_status[target].num_active_incoming_edges--;
        if(dmr->var_status[target].num_active_incoming_edges == 0){
            /*
            target has become a source. Update corresponding labels, add it
            to the sources list and update it's source_index which locates
            it.
            */
            dmr->var_status[target].is_source = true;
            dmr->sources[dmr->num_sources] = target;
            dmr->var_status[target].source_index = dmr->num_sources;
            dmr->num_sources++;
        }
    }
}

void qdpll_res_dep_man_notify_active(QDPLLDepManGeneric * dm, VarID variable){

    QDPLLDepManRes * dmr = (QDPLLDepManRes *) dm;

    dmr->var_status[variable].active = true;
    unsigned int i,j;
    VarID target;
    VarID last_source;
    unsigned int target_index;
    /*
    Update all children to see if any stop being sources.
    */
    for(j = 1; j < dmr->adjacency_list[variable][0]; j++){
        target = dmr->adjacency_list[variable][j];
        dmr->var_status[target].num_active_incoming_edges++;
        if(dmr->var_status[target].num_active_incoming_edges == 1){
            /*
            If we have exactly one active incoming edge, we must have
            added it just now, so target stops being a source.
            */
            dmr->var_status[target].is_source = false;
            dmr->num_sources--;
            
            /*
            Locate target in the list of sources.
            */
            target_index = dmr->var_status[target].source_index;
            
            /*
            We decremented num_sources, so it now points to the
            last source.
            */
            last_source = dmr->sources[dmr->num_sources];
            
            /*
            Replace target with last_source and update last_source's
            source_index.
            */
            dmr->sources[target_index] = last_source;
            dmr->var_status[last_source].source_index = target_index;
            
            /*
            Deactivate target's source_index.
            */
            dmr->var_status[target].source_index = -1;
        }
    }
}

void qdpll_res_dep_man_reset(QDPLLDepManGeneric * dm){

    QDPLLDepManRes * dmr = (QDPLLDepManRes *) dm;

    dmr->is_initialized = false;
    int i;
    for(i = 1; i <= dmr->n; i++){
        free(dmr->adjacency_list[i]);
    }
    free(dmr->adjacency_list);
    free(dmr->var_status);
    free(dmr->sources);
}

void qdpll_res_dep_man_notify_init_variable(QDPLLDepManGeneric * dm, VarID variable){

    QDPLLDepManRes * dmr = (QDPLLDepManRes *) dm;

    // TODO
    return;
}

void qdpll_res_dep_man_notify_reset_variable(QDPLLDepManGeneric * dm, VarID variable){
    // TODO
    return;
}

int qdpll_res_dep_man_is_init(QDPLLDepManGeneric * dm){

    QDPLLDepManRes * dmr = (QDPLLDepManRes *) dm;

    return dmr->is_initialized;
}

void qdpll_res_dep_man_reduce_lits (QDPLLDepManGeneric * dmg, LitIDStack ** lit_stack,
                                           LitIDStack ** lit_stack_tmp,
                                           const QDPLLQuantifierType other_type,
                                           const int lits_sorted)
{
  QDPLLDepManRes *dmr = (QDPLLDepManRes *) dmg;
  QDPLL *qdpll = dmg->qdpll;
#if COMPUTE_TIMES
  const double start = time_stamp ();
#endif

  LitID lit, *temp_lit, *current_lit = (*lit_stack)->start;
  Var *current_var;

  while (current_lit < (*lit_stack)->top){
      current_var = LIT2VARPTR (qdpll->pcnf.vars, *current_lit);
      if (!current_var->is_internal && other_type != current_var->scope->type &&
          is_var_reducible_with_respect_to_sorted_clause_tail(dmr, current_var->id, current_lit + 1, (*lit_stack)->top))
      {
          /*
          Variable 'current_var' is of the type that we are removing, and nothing further depends on it.
           */
          LEARN_VAR_UNMARK (current_var); // copied from the original version, no idea what it does

          /* Must shift the tailing part of the stack to the left */
          temp_lit = current_lit + 1;
          while(temp_lit < (*lit_stack)->top){
            *(temp_lit - 1) = *temp_lit;
            temp_lit++;
          }
          QDPLL_POP_STACK (**lit_stack);

          /* In this case it's not necessary to increment current_lit, because the tail collapsed.*/

          if (qdpll->options.verbosity > 1)
          {
              if (other_type == QDPLL_QTYPE_EXISTS)
                  fprintf (stderr, "CDCL: type-reducing lit %d by ordering\n", lit);
              else
                  fprintf (stderr, "SDCL: type-reducing lit %d by ordering\n", lit);
          }
      }
      /* In this case we must increment current_lit */
      current_lit++;
  }



#if COMPUTE_TIMES
  dm->dmg.qdpll->time_stats.total_reduce_time += (time_stamp () - start);
#endif
}

/*
Returns true if and only if none of the literals stored in the memory interval [begin, end)
depend on @variable which is equivalent to @variable being reducible with respect to a clause
with the corresponding tail.

NOTE that it is assumed that the literals in the interval [begin, end) and in the dependency manager
adjacency list are sorted. This fact is used to reduce the number of comparisons required to determine
if there is a match.

Currently, this function takes O(n+m) where n is the number of variables in the clause and m is the out-degre of @var.
*/
bool is_var_reducible_with_respect_to_sorted_clause_tail(QDPLLDepManRes * dmr, VarID var, LitID * begin, LitID * end){
    unsigned int i = 1, num_var_deps = dmr->adjacency_list[var][0];
    VarID x, y;

    /*
    We will simply use the variable begin to iterate.
    */
    while(begin < end || i < num_var_deps){
        x = LIT2VARID(*begin);
        y = dmr->adjacency_list[var][i];

        /* Check for match. */
        if(x == y){
            return false;
        }

        /* Advance to the next step. */
        if(x < y){
          begin++;
        }else{
          /* The only option left is that x > y. */
          i++;
        }
    }
    /* No match found, so var is reducible. */
    return true;
}

void
qdpll_res_dep_man_print_deps_by_graph (QDPLLDepManGeneric * dm, VarID id)
{
  // TODO
  return;
}

/* 'print_deps' function. */
void
qdpll_res_dep_man_print_deps_by_search (QDPLLDepManGeneric * dm, VarID id)
{
  // TODO
  return;
}

static void qdpll_res_dep_man_dump_dep_graph (QDPLLDepManGeneric * dm){
    return;
}

static LitID * qdpll_res_dep_man_get_candidates (QDPLLDepManGeneric * dmg)
{
  // TODO
  QDPLLDepManRes *dmr = (QDPLLDepManRes *) dmg;

  return NULL;
}

VarID qdpll_res_dep_man_get_class_rep (QDPLLDepManGeneric * dmg, VarID x, const unsigned int ufoffset){
  return x;
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
  dm->dmg.get_class_rep = qdpll_res_dep_man_get_class_rep;

  // TODO
  if (print_deps_by_search)
    dm->dmg.print_deps = qdpll_res_dep_man_print_deps_by_search;
  else
    dm->dmg.print_deps = qdpll_res_dep_man_print_deps_by_graph;

  dm->dmg.dump_dep_graph = qdpll_res_dep_man_dump_dep_graph; // TODO
  dm->dmg.depends = qdpll_res_dep_man_depends;
  //dm->dmg.get_class_rep = qdpll_res_dep_man_get_class_rep; // TODO
  dm->dmg.reduce_lits = qdpll_res_dep_man_reduce_lits; // TODO
  dm->dmg.get_candidates = qdpll_res_dep_man_get_candidates; // TODO

  return dm;
}


void
qdpll_qdag_res_dep_man_delete (QDPLLDepManRes * dm)
{
  qdpll_free (dm->mm, dm, sizeof (QDPLLDepManRes));
}


