/**********************************************************************
 * File:    infer/infer.h
 * Purpose: Type inference functions.
 * Author:  Karl Abrahamson
 **********************************************************************/

/*********************** From t_unify.c ***************************/

extern Boolean deferral_ok;

Boolean unify_type_tc	  (EXPR *e, TYPE **t, TYPE *h, Boolean report);
Boolean unify_type_g_tc	  (EXPR *e, TYPE **t, TYPE *h, Boolean report,
			   EXPR *err_ex);

/*********************** From defaults.c **************************/

extern Boolean full_handle_defaults;
extern EXPR_LIST* default_bindings;

void 	bind_glob_bound_vars	(TYPE *t, MODE_TYPE *mode);
void    process_defaults_tc	(EXPR *def, MODE_TYPE *mode, Boolean showit,
				 Boolean report);

/*********************** From infer.c *****************************/

Boolean infer_type_tc	  (EXPR *e);

/*********************** From defer.c *****************************/

extern struct with_info version_list;
extern LIST *unification_defer_types;
extern LIST *unification_defer_exprs;
extern LIST *unification_defer_holds;
extern LIST *fragmented_defer_exprs;
extern LIST *fragmented_pat_funs;
extern Boolean suppress_using_local_expectations;

int     handle_expects_tc   (EXPR *e_orig, EXPR *e, EXPR *rest, int kind, 
		      	     MODE_TYPE *mode, INT_LIST **EDL, int *LEDL,
		      	     INT_LIST **DL, int *LDL);
int 	handle_deferrals_tc (EXPR_LIST *expr_defers, TYPE_LIST *type_defers, 
			     EXPR *e, int kind, MODE_TYPE *mode, 
			     INT_LIST **DL, int *LDL, int num_prior_successes);
int     handle_fragmented_tc(int kind, EXPR *e, LIST *defers, 
			     Boolean report_errs);
void	redo_deferrals_tc    (int kind, EXPR *e, INT_LIST *EDL, INT_LIST *DL, 
			     EXPR_LIST **rest_ed,
		             LIST **rest_td, EXPR_LIST **rest_fe, 
			     MODE_TYPE *mode);
void    get_fragments_tc    (EXPR *e);
void    print_overload_info (EXPR *e);
void    drop_overload_lists (void);




