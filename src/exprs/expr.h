/*****************************************************************
 * File:    exprs/expr.h
 * Purpose: Expression type and discriminators, and exports from expr.c
 * Author:  Karl Abrahamson
 *****************************************************************/
    
#define AND_BOOL	1
#define OR_BOOL		2
#define IMPLIES_BOOL    3


/****************************************************************
 * 			CONSTANTS		  		*
 ****************************************************************/

#define CHAR_CONST	1
#define STRING_CONST	2
#define NAT_CONST	3
#define REAL_CONST	4
#define HERMIT_CONST	5
#define BOOLEAN_CONST   6
#define FAIL_CONST	7

#define TEAM_NOTE	-2
#define TEAM_DCL_NOTE   -3


/****************************************************************
 * 			From expr.c		  		*
 ****************************************************************/

EXPR_TAG_TYPE  ekindf	(EXPR *e);
EXPR* new_expr1		(EXPR_TAG_TYPE kind, EXPR *e1, int line);
EXPR* new_expr1t	(EXPR_TAG_TYPE kind, EXPR *e1, TYPE *t, int line);
EXPR* new_expr2		(EXPR_TAG_TYPE kind, EXPR *e1, EXPR *e2, int line);
EXPR* new_expr3		(EXPR_TAG_TYPE kind, EXPR *e1, EXPR *e2, EXPR *e3, 
			 int line);
EXPR* const_expr	(char *s, int k, TYPE *t, int line);
EXPR* global_id_expr    (char *s, int line);
EXPR* global_sym_expr   (int n, int line);
EXPR* typed_global_id_expr(char *s, TYPE *t, int line);
EXPR* typed_global_sym_expr(int n, TYPE *t, int line);
EXPR* id_expr		(char *s, int line);
EXPR* typed_id_expr	(char *s, TYPE *t, int line);
EXPR* apply_expr	(EXPR *a, EXPR *b, int line);
EXPR* try_expr		(EXPR *a, EXPR *b, EXPR *c, int kind, int line);
EXPR* cuthere_expr	(EXPR *e);
EXPR* wsame_e		(EXPR *e, int line);
EXPR* zsame_e		(EXPR *e, int line);
EXPR* same_e		(EXPR *e, int line);
Boolean expr_equal	(EXPR *A, EXPR *B);
void  init_exprs	(void);

/****************************************************************
 * 			From exprutil.c		  		*
 ****************************************************************/

void    scan_expr       	(EXPR **e, Boolean (*f)(EXPR **), Boolean pr);
void    set_expr_line		(EXPR *ex, int line);
EXPR_TAG_TYPE let_expr_kind	(ATTR_TYPE att);
EXPR*   bind_apply_expr		(EXPR *a, EXPR *b, int line);
EXPR*   possibly_apply 		(EXPR *a, EXPR *b, int line);
EXPR*	tagged_id_p		(char *id, RTYPE t, int line);
EXPR* 	tagged_pat_var_p	(char *id, RTYPE t, int line);
EXPR* 	anon_pat_var_p 		(RTYPE t, int line);
EXPR*	new_pat_var     	(char *name, int line);
EXPR*   fresh_id_expr		(char *s, int line);
EXPR*   fresh_pat_var		(char *name, int line);
EXPR*   var_to_pat_var		(EXPR *id);
EXPR* 	typed_target		(TYPE *t, EXPR *patfun, int line);
EXPR* 	attach_tag_p		(EXPR *e, RTYPE t);
EXPR*   build_species_as_val_expr(TYPE *T, int line);
EXPR*   build_stack_expr	(TYPE *ty, int line);
EXPR* 	build_cons_p		(EXPR *h, EXPR *t);
EXPR*   make_wrap		(EXPR *e, int line);
EXPR*   make_await_expr		(EXPR *body, EXPR_LIST *tests, int nostore, 
				 int line);
EXPR* 	make_apply_p		(char *name, EXPR *e1, EXPR *e2);
EXPR* 	op_expr_p		(EXPR *e1, struct lstr op, 
				 RTYPE t, EXPR *e2);
EXPR*   make_ask_expr		(EXPR *agent, EXPR *action, int line);
EXPR*	build_proc_apply	(EXPR *Proc, LIST *params);
EXPR*	remove_appls_p		(EXPR *heading, EXPR *body, int basekind);
EXPR* 	force_eval_p		(EXPR *e, EXPR **v);
Boolean is_irregular_fun	(EXPR *e);
Boolean is_star			(EXPR *e);
Boolean is_bang			(EXPR *e);
Boolean is_anon_pat_var 	(EXPR *e);
Boolean is_ordinary_pat_var	(EXPR *e);
Boolean is_const_expr		(EXPR *e);
Boolean is_id_p			(EXPR *e);
Boolean is_id_or_patvar_p	(EXPR *e);
Boolean is_false_expr   	(EXPR *e);
Boolean is_definition_kind	(EXPR_TAG_TYPE kind);
Boolean is_stack_expr   	(EXPR *e);
Boolean is_hermit_expr  	(EXPR *e);
Boolean is_target_expr		(EXPR *e);
Boolean binds_type_variables	(EXPR *e);
EXPR*   get_applied_fun 	(EXPR *e, Boolean report_err);
EXPR*   get_defined_id 		(EXPR *e);
EXPR*	make_list_mem_expr	(EXPR *e);
EXPR*   make_box_expr		(EXPR *content, int mode);
EXPR* 	list_to_expr		(EXPR_LIST *l, int mode);
EXPR* 	list_to_list_expr	(EXPR_LIST *l, int line);
EXPR* 	skip_sames		(EXPR *e);
EXPR*   skip_sames_to_dcl_mode  (EXPR *e);
EXPR* 	add_expr		(EXPR *g, EXPR *e);
void 	clear_offsets		(EXPR *e);
void    check_for_bare_ids	(EXPR *e, EXPR_LIST *l);
int  	expr_size		(EXPR *e);
void 	bump_expr_parts 	(EXPR *e);
void 	drop_hash_expr  	(HASH2_CELLPTR h);
void    mark_all_pat_funs	(EXPR *e);
int     id_occurrences  	(EXPR *id, EXPR *e);
EXPR*   image_to_id		(EXPR *image, Boolean use_stack, 
				 EXPR *translation, EXPR **preamble, 
				 Boolean open, int line);
void    simplify_expr   	(EXPR **e);
Boolean uses_exception		(EXPR *e);
EXPR* 	make_cast		(TYPE *s, TYPE *t, int line);
EXPR*   ignore_expr 	   	(EXPR *e, int line);
EXPR* 	make_var_expr_p		(LIST *l, TYPE* v_content_type, EXPR *init_val,
				 Boolean all, int line, MODE_TYPE *mode);
EXPR* 	make_execute_p 		(EXPR *e, int line);
void    add_ids_and_pats	(EXPR **ex, EXPR **pat, EXPR_LIST *L, 
				 int line);
void    add_ids			(EXPR **ex, EXPR_LIST *L, int line);
EXPR*   make_class_constructor_param(EXPR_LIST *consts, char *superclass,
				     int line);
EXPR*   perform_simple_define_p (EXPR *A, EXPR *B);
EXPR*   build_constructor_def_p (char *T, EXPR *A, EXPR *B, EXPR *C);
EXPR*   role_select_expr	(LIST *l, int line);
EXPR*   role_modify_expr	(EXPR *x, LIST *l, int line);


/****************************************************************
 * 			From context.c	 	 		*
 ****************************************************************/

EXPR*   make_context_expr	(char *op, EXPR *val, EXPR *bdy, int line,
				 Boolean complain);

/****************************************************************
 * 			From choose.c		  		*
 ****************************************************************/

extern int formal_num;

EXPR*		make_cases_p		(CLASS_UNION_CELL *guards, int arrow, 
					 EXPR *caseBody, EXPR *when, 
					 EXPR *cases, int line);
EXPR* 		make_while_cases_p	(CLASS_UNION_CELL *guards, int arrow, 
				   	 EXPR *caseBody, EXPR *cont,
					 EXPR *when, EXPR *cases,  int line);
EXPR*           make_define_else_case_p (EXPR *lhs, EXPR *rhs);
EXPR* 		make_else_expr_p	(ATTR_TYPE n, int exc_num, int line);
EXPR* 		make_empty_else_p	(void);
CLASS_UNION_CELL* case_cuc_p		(ATTR_TYPE case_kind, int succeed, 
					 EXPR *guard, int line);
EXPR_LIST* 	get_define_case_parts_p	(EXPR *heading, EXPR_LIST *rest, 
					 EXPR *body, EXPR **outbody);
EXPR* 		make_define_case_p	(EXPR *heading, EXPR *body, EXPR *when,
				   	 EXPR *rest);
EXPR* 		add_then_to_body	(EXPR *body_expr, EXPR *then_expr);
EXPR* 		make_def_by_cases_p	(EXPR *id, EXPR *image, EXPR *body, 
				    	 int defkind);
EXPR* 		make_do_dblarrow_case_p	(EXPR *a, EXPR *b, EXPR *rest, 
					 int line);
void		check_for_cases  	(EXPR *e, int line);
EXPR*		short_continue_p	(void);
void 		make_unify_pattern	(EXPR *pat, int eqkind, int line);
EXPR* 		make_for_expr_p		(EXPR_LIST *iterators, 
					 EXPR_LIST *body);
void 		add_list_to_choose_matching_lists_p(EXPR *fun, EXPR_LIST *pl);
EXPR* 		build_choose_p		(EXPR *target_builder, EXPR *cases, 
					 Boolean subcase, int line);


/****************************************************************
 * 			From copyexpr.c		  		*
 ****************************************************************/

EXPR* 	   copy_global_id_expr  		(EXPR *e);
EXPR* 	   copy_expr				(EXPR *e);
EXPR_LIST* copy_expr_list 			(EXPR_LIST *e);
EXPR*      copy_expr_and_choose_matching_lists	(EXPR *e);


/****************************************************************
 * 			From alocexpr.c	 	 		*
 ****************************************************************/

#ifndef SET_EXPR
# define SET_EXPR(x,t)       set_expr(&(x), t) 
# define FRESH_SET_EXPR(x,t) bump_expr(x = t)
# define bmp_expr(e)	(e)->ref_cnt++	
#endif

void set_expr		(EXPR **x, EXPR *t);
void bump_expr		(EXPR *x);
void drop_expr		(EXPR *x);


/****************************************************************
 *			STANDARD EXPRESSIONS, SETS		*
 ****************************************************************/

extern EXPR* hermit_expr;
extern EXPR* nilstr_expr;
extern EXPR* bad_expr;
extern EXPR* eqch_expr;
extern EXPR* shared_expr;
extern EXPR* nonshared_expr;
