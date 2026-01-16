/****************************************************************
 * File:    dcls/dcls.h
 * Purpose: Handling declarations in translator
 * Author:  Karl Abrahamson
 ****************************************************************/

/************************ From dcl.c **********************************/
/**********************************************************************/

extern Boolean in_pat_dcl, do_show_at_type_err, force_show;
extern Boolean echo_expr;
extern UBYTE   show_types;
extern char*   main_id_being_defined;
extern int     solution_count, max_overloads, main_max_overloads;
extern EXPR*   viable_expr;

Boolean issue_dcl_p	 (EXPR *e, int kind, MODE_TYPE *mode);
int     end_initial_stage(int kind, EXPR *e, MODE_TYPE *mode, 
			  int num_prior_successes, Boolean report_errs);
int  final_issue_dcl_p	 (EXPR *e, int kind, MODE_TYPE *mode, 
			  int num_prior_successes);
void do_echo_expr	 (EXPR *e, int context);
void do_show_types1	 (EXPR *e, PRINT_TYPE_CONTROL *ctl, int show_mode);
void do_show_types       (EXPR *e);

/************************* From dclclass.c ***************************/
/***********************************************************************/

#define LEFT_SIDE	0
#define RIGHT_SIDE	1

extern Boolean declaring_type;

CLASS_TABLE_CELL* declare_cg_p		   (char *s, int which, 
					    int extens, MODE_TYPE *mode);
void		expect_tf_p		   (char *id, TYPE *arg, 
					    MODE_TYPE *mode, Boolean full);
void 		declare_tf_p		   (char *id, RTYPE arg, LIST *L, 
					    MODE_TYPE *mode);
void 		do_class_preamble	   (char *id, TYPE *arg,
					    char *superclass);
void 		declare_class_p		   (char *id, RTYPE arg,
					    char *superclass, int line);


/*********************** From dclutil.c *********************************/
/************************************************************************/

TYPE* 		cuc_rep_type		(CLASS_UNION_CELL *cuc);
void 		define_by_cast		(char *new_id, TYPE *new_ty,
					 EXPR *old_expr,
					 MODE_TYPE *mode, Boolean defer, 
					 int line);
void 		define_by_cast_from_id	(char *new_id, TYPE *new_ty,
					 char *old_id, TYPE *old_ty,
					 MODE_TYPE *mode, Boolean defer, 
					 int line);
void 		define_by_cast_from_nat	(char *new_id, TYPE *new_ty,
					 LONG val, MODE_TYPE *mode, 
					 Boolean defer, int line);
void 		define_by_compose	(char *a, char *b, char *c, 
		       			 TYPE *X, TYPE *Y, TYPE *Z, 
		       			 Boolean defer, MODE_TYPE *mode,
					 int line);
void            pad_sym_def             (int sym, TYPE *t);
void 		basic_pat_fun_p		(char *constr_name,TYPE *constr_type, 
					 char *destr_name, TYPE *destr_type,
					 Boolean defer);
void            auto_pat_const_p        (char *id, TYPE *id_type, 
					 char *tester, TYPE *tester_type,
					 Boolean defer);
void		auto_pat_fun_p		(char *constr_name, TYPE *constr_type,
					 ROLE *r, EXPR *destructor,
					 Boolean defer);
void 		long_auto_pat_fun_p	(char *constr_name, TYPE *constr_type, 
					 ROLE *r, EXPR  *destructor, 
					 Boolean defer, Boolean set_prims, 
					 INT_LIST *posn_list,
					 int discr, Boolean irregular);


/*********************** From dclcnstr.c ********************************/
/************************************************************************/

extern Boolean suppress_role_extras;

Boolean		declare_equality_by_cast   (TYPE *t, TYPE *r);
void		declare_equality_by_qeq    (TYPE *t, CTC_LIST *l);
void		declare_constructor_p	   (char *const_name,
					    char *true_constr_name,
					    TYPE *domain, ROLE *dRole,
					    TYPE *codomain, ROLE *cRole,
					    int prim, int arg,
					    MODE_TYPE *mode,
					    int irregular, LIST *withs,
					    Boolean trap, Boolean defer,
					    Boolean build_declaration);
void 		declare_curried_constructor_p
      					   (char *constr_name, 
					    char *true_constr_name,
       					    TYPE_LIST *domain_types,   
   					    ROLE_LIST *domain_roles,
					    TYPE *rep_type, ROLE *rep_role,
   					    TYPE *codomain, ROLE *cRole,
   					    MODE_TYPE *mode, 
					    int is_irregular, LIST *withs, 
   					    Boolean defer);
void		declare_fields_p	   (TYPE *main_type, 
					    TYPE *main_rep_type, 
					    RTYPE reptype,
					    INT_LIST *posnlist, int discr,
					    Boolean special, 
					    MODE_TYPE *mode);
void            declare_dollar_p           (LIST *L, TYPE *t);
void 		declare_pull_p		   (LIST *L, TYPE *t);
void 		declare_copy_p		   (LIST *L, TYPE *t);
void 		declare_move_copy_p	   (LIST *L, TYPE *t);
void 		declare_upcast_p	   (LIST *L, TYPE *F, TYPE *arg);
void 		declare_downcast_p	   (LIST *L, TYPE *F, TYPE *arg);

/************************** From report.c *******************************/
/************************************************************************/

extern int		delay_reports;
extern Boolean 		show_all_reports, set_show_all_reports;
extern Boolean		list_reports_without_listing;
extern REPORT_RECORD*	reports;
extern REPORT_RECORD*	import_reports;

void		report_dcl_p		   (char *name, int kind, 
					    MODE_TYPE *mode,
					    TYPE *ty, ROLE *r);
void		report_dcl_aux_p	   (char *name, int kind, 
					    MODE_TYPE *mode,
					    TYPE *ty, ROLE *r, char *aux,
					    CLASS_TABLE_CELL *ctc, LPAIR lp);
void		print_reports_p		   (void);

/************************** From deferdcl.c ******************************/
/************************************************************************/

void 		defer_attach_property	   (char *prop, char *name, TYPE *t);
Boolean		defer_issue_dcl_p	   (EXPR *ex, int kind, 
					    MODE_TYPE *mode);
void 		defer_issue_description_p  (EXPR *ex, MODE_TYPE *mode);
void            defer_issue_missing_tm     (char *name, TYPE *type, 
					    MODE_TYPE *mode);
void 		defer_expect_ent_id_p	   (char *name, TYPE *type, ROLE *role,
					    int context, MODE_TYPE *mode, 
					    int line);
void		handle_deferred_dcls_p	   (void);

/************************** From defdcls.c ******************************/
/************************************************************************/

void 		quick_issue_define_p	   (EXPR *id, char *name, 
					    MODE_TYPE *mode, int line);
int		issue_define_p		   (EXPR *e, MODE_TYPE *mode,
					    Boolean check_if_should_take);
int		process_define_heading_p   (EXPR *heading, STR_LIST *where,
					    Boolean is_define,
					    MODE_TYPE *mode);
EXPR*		define_dcl_p		   (EXPR *heading, EXPR *body, 
					    Boolean is_define);
void 		check_for_undeclared_role  (STR_LIST *this_namelist,
					    STR_LIST *declared_namelist,
					    char *spec_name, TYPE *ty,
					    int line);
void 		declare_bring_from	   (LIST *ids, RTYPE tag, 
					    EXPR *fro, int line);
void 		declare_bring_by	   (LIST *ids, RTYPE tag, 
					    EXPR *fun, int line);
void 		def_for_bring_p		   (char *new, RTYPE rt, 
					    EXPR *from, MODE_TYPE *mode, 
					    int line);
int		issue_team_p		   (EXPR *e);
Boolean		do_execute_p		   (EXPR *e, MODE_TYPE *mode);
void 		issue_dispatch_definitions (char *id, TYPE *ty);


/************************* From somedcls.c *******************************/
/*************************************************************************/

/* Exception Declarations */

void 		exception_p		  (char *name, Boolean trap, 
					   RTYPE wt, char *descr,
					   MODE_TYPE *mode);

/* Description Declarations */

int      issue_description_p             (EXPR *this_dcl, MODE_TYPE *mode);
void 	 issue_embedded_description	 (EXPR *def, char *descr, 
					  MODE_TYPE *mode);
void     create_description_p		 (char *id, char *descr, TYPE* ty, 
			  		  MODE_TYPE *mode, int man_form, 
					  int line);

/* Assume Declarations */

void 		assume_p		   (STR_LIST *l, RTYPE rt, 
					    MODE_TYPE *mode);

/* Operator Declarations */

void 		operator_p		   (char *s, char *op, Boolean open,
					    MODE_TYPE *mode);

/************************* From expnddcl.c *******************************/
/*************************************************************************/

Boolean		issue_patexp_dcl_p	   (EXPR *e, MODE_TYPE *mode);
Boolean		issue_pattern_dcl_p	   (EXPR *e, MODE_TYPE *mode);
Boolean		issue_expand_dcl_p	   (EXPR *e, MODE_TYPE *mode);

