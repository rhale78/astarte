/*****************************************************************
 * File:    ids/ids.h
 * Purpose: Functions for handling identifiers in expressions.
 * Author:  Karl Abrahamson
 *****************************************************************/

#define DECLARATION_B 1
#define TRY_ELSE_B    2
#define LET_B         4

extern int outer_scope_num;

void    do_process_ids      (EXPR *e, int kind);
EXPR*   process_ids	    (EXPR *e, int context);
Boolean	mark_pat_funs       (EXPR *e, Boolean all, int context);
void    declare_patvars	    (EXPR *e);
void    declare_all	    (EXPR *e);
void    local_declare_all   (EXPR_LIST *l);
void    add_to_temps        (EXPR_LIST *l, int k);

void    do_process_scopes   (EXPR *e);
void    process_scopes	    (EXPR *e, int context);
void    mark_final_ids      (EXPR *e);


