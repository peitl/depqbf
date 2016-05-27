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
    
    int j;
    while(fscanf(f, "%d:", &active_variable) != EOF){
        if(active_variable > capacity){
            // need to alloc more space for variables
            capacity = 2*active_variable;
            dmr->adjacency_list = realloc(dmr->adjacency_list, sizeof(int*) * (capacity + 1));
        }
        num_edges = 1;
        if(active_variable > dmr->n){
            for(j = dmr->n + 1; j <= active_variable; j++){
                dmr->adjacency_list[j] = malloc(sizeof(int) * num_edges);
                /*
                The first element of the adjacency list is its size.
                */
                dmr->adjacency_list[j][0] = 0;
            }            
            dmr->n = active_variable;
        }

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
    /*printf("Candidates at the beginning: ");
    printf("%d\n", dmr->num_sources);
    for(i = 0; i < dmr->num_sources; i++){
      printf("%d ", dmr->sources[i]);
    }
    printf("\n");*/
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
    //printf("%d\n", j);
}

VarID qdpll_res_dep_man_get_candidate(QDPLLDepManGeneric * dm){

    QDPLLDepManRes * dmr = (QDPLLDepManRes *) dm;

    if(dmr->current_candidate >= dmr->num_sources){
        /*
        WARNING
        If we already returned all candidates, return NULL. This
        should be the specified behaviour, but I am not entirely
        sure it's correct.
        */
        dmr->current_candidate = 0;
        //printf("end reached\n");
        return NULL;
    }
    VarID candidate = dmr->sources[dmr->current_candidate];
    dmr->current_candidate++;
    //printf("%d\n", candidate);
    return candidate;
}

int qdpll_res_dep_man_is_candidate(QDPLLDepManGeneric *dm, VarID variable){

    QDPLLDepManRes * dmr = (QDPLLDepManRes *) dm;

    return dmr->var_status[variable].is_source;
}

void qdpll_res_dep_man_notify_inactive(QDPLLDepManGeneric * dm, VarID variable){

    QDPLLDepManRes * dmr = (QDPLLDepManRes *) dm;
    
    //printf("inactivating %d\n", variable);

    dmr->var_status[variable].active = false;
    
    /*
    Unmark as source, however keep the number of active incoming edges. In typical
    usage a variable is only marked as inactive when assigned by the solver and
    therefore is a source at the moment and has 0 active incoming edges.
    
    However, again, I am not sure about the internal processes of DepQBF and whether
    this assumption is justified. Therefore, we unset the is_source flag, remove the
    variable from the list of sources, but keep the number of active incoming edges in
    order to be able to restore the value of is_source upon reactivation.
    */
    delete_source(dmr, variable);
    
    unsigned int j;
    VarID target;
    /*
    Update all children to see if any become sources.
    */
    for(j = 1; j < dmr->adjacency_list[variable][0]; j++){
        target = dmr->adjacency_list[variable][j];
        dmr->var_status[target].num_active_incoming_edges--;
        if(dmr->var_status[target].num_active_incoming_edges == 0){
            add_source(dmr, target);
        }
    }
    /*for(int i = 0; i < dmr->num_sources; i++){
      printf("%d ", dmr->sources[i]);
    }
    printf("\n");*/
}

void qdpll_res_dep_man_notify_active(QDPLLDepManGeneric * dm, VarID variable){

    QDPLLDepManRes * dmr = (QDPLLDepManRes *) dm;
    
    //printf("reactivating %d\n", variable);

    dmr->var_status[variable].active = true;
    unsigned int j;
    VarID target;
    
    /*
    If there are zero active incoming edges to the activated variable,
    mark it as a source. See the above function (qdpll_res_dep_man_notify_inactive)
    for a discussion of this.
    */
    if(dmr->var_status[variable].num_active_incoming_edges == 0){
        add_source(dmr, variable);
    }
    
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
            delete_source(dmr, target);
        }
    }
    /*for(int i = 0; i < dmr->num_sources; i++){
      printf("%d ", dmr->sources[i]);
    }
    printf("\n");*/
}

void add_source(QDPLLDepManRes * dmr, VarID var){
    if(!dmr->var_status[var].is_source){
        dmr->var_status[var].is_source = true;
        dmr->var_status[var].source_index = dmr->num_sources;
        dmr->sources[dmr->num_sources] = var;
        dmr->num_sources++;
    }
}

void delete_source(QDPLLDepManRes * dmr, VarID var){
    if(dmr->var_status[var].is_source){
        dmr->var_status[dmr->sources[dmr->num_sources - 1]].source_index = dmr->var_status[var].source_index;
        dmr->sources[dmr->var_status[var].source_index] = dmr->sources[dmr->num_sources - 1];
        dmr->num_sources--;
        dmr->var_status[var].is_source = false;
        dmr->var_status[var].source_index = -1;
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
          //is_var_reducible_with_respect_to_sorted_clause_tail(dmr, current_var->id, current_lit + 1, (*lit_stack)->top))
          is_var_reducible_with_respect_to_clause(dmr, current_var->id, (*lit_stack)->start, (*lit_stack)->top))
      {
          /*
          Variable 'current_var' is of the type that we are removing, and nothing further depends on it.
           */
          LEARN_VAR_UNMARK (current_var); // copied from the original version, I have a slight idea what it does

          /* Must shift the tailing part of the stack to the left */
          //show_reduction(current_var->id, (*lit_stack)->start, (*lit_stack)->top);
          
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
                  fprintf (stderr, "CDCL: type-reducing lit %d by res\n", lit);
              else
                  fprintf (stderr, "SDCL: type-reducing lit %d by res\n", lit);
          }
      }else{
          /* In this case we must increment current_lit */
          current_lit++;
      }
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

This function takes O(n+m) where n is the number of variables in the clause and m is the out-degree of @var.

WARNING: Don't use this function as it's not clear at the moment, if the sortedness assumption is met.
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
        if(x == y && dmr->var_status[x].active){
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


bool is_var_reducible_with_respect_to_clause(QDPLLDepManRes * dmr, VarID var, LitID * begin, LitID * end){
    unsigned int i = 1, num_var_deps = dmr->adjacency_list[var][0];
    VarID x, y;
    LitID* iter = begin;

    while(iter < end){
        x = LIT2VARID(*iter);
        //printf("%d ", x);
        for(i = 1; i < num_var_deps; i++){
            y = dmr->adjacency_list[var][i];
            /* Check for match. */
            //if(x == y && dmr->var_status[x].active){
            if(x == y){
                //printf(" not reducing %d\n", var);
                return false;
            }
        }
        iter++;
    }
    //printf(" reducing %d\n", var);
    /* No match found, so var is reducible. */
    //show_reduction(var, begin, end);
    return true;
}

void show_reduction(VarID var, LitID* begin, LitID* end){
    printf("%d:", var);
    while(begin < end){
        printf(" %d", *begin);
        begin++;
    }
    printf("\n");
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
    /*
    Copied from QDAG, stripped of asserts. Not quite sure about the use.
    */
    QDPLLDepManRes *dmr = (QDPLLDepManRes *) dmg;
    
    /* Count candidates. */
    unsigned int cnt = dmr->num_sources;
    Var *vars = dmr->pcnf->vars;
    Var *c;

    /* Allocate zero-terminated array of candidates. Caller is
       responsible for releasing that memory. We do not use memory
       manager here because caller might not have access to it to call
       'free' afterwards. */
    LitID *result = malloc ((cnt + 1) * sizeof (LitID));
    memset (result, 0, (cnt + 1) * sizeof (LitID));
    LitID *rp = result;
    
    unsigned int i = 0;
    for (i = 0; i < cnt; i++)
    {
        c = VARID2VARPTR (vars, dmr->sources[i]);
        /* Existential (universal) variables are exported as positive (negative) ID. */
        *rp++ = c->scope->type == QDPLL_QTYPE_EXISTS ? c->id : -c->id;
    }

    return result;
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


