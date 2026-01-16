/****************************************************************
 * File:    tables/tables.h
 * Purpose: Exports from table managers
 * Author:  Karl Abrahamson
 ****************************************************************/


/************************ From secimp.c ******************************/
/***********************************************************************/

void    add_global_expectation	(char *name, TYPE *t, ROLE *r,
			    	 LIST *who_sees, MODE_TYPE *mode,
			    	 char *package_name, int line);
void 	add_global_anticipation (char *id, TYPE *ty, LIST* who_sees,
			         MODE_TYPE *mode, 
				 char *package_name, int line);
void 	second_import_tm	(char *imported_package, 
				 char *file_name,
				 MODE_TYPE *mode);

/************************ From primtbl.c ******************************/
/***********************************************************************/

extern Boolean no_gen_prim;

void	mode_primitive_tm	(char *id, TYPE *t, ROLE *r, 
				 int k, int instr, MODE_TYPE *mode);
void    sym_mode_prim_tm	(int sym, TYPE *t, ROLE *r,
		      		 int kind, int instr, MODE_TYPE *mode);
void    import_sym_mode_prim_tm	(int sym, TYPE *t, ROLE *r,
		      		 int kind, int instr, MODE_TYPE *mode);
void	primitive_tm		(char *id, TYPE *t, int k, int instr);
void	sym_prim_tm		(int sym, TYPE *t, int k, int instr);
void	import_sym_prim_tm	(int sym, TYPE *t, int k, int instr);
void	prim_const_tm		(char *name, TYPE *t, int val);
void    sym_prim_const_tm       (int sym, TYPE *t, int val);
void	prim_exception_const_tm	(int sym, int prim);
void 	prim_exception_fun_tm	(int sym, int prim);
void    prim_box_tm     	(char *name, TYPE *t, int val);
void 	sym_prim_box_tm		(int sym, TYPE *t, int val);
void    expect_sym_prim_tm  	(int sym, TYPE *t, MODE_TYPE *mode, 
				 int antic);

/************************ From assume.c ********************************/
/***********************************************************************/

void 		assume_tm		(char *id, TYPE *t, Boolean global);
void 		assume_role_tm		(char *id, ROLE *r, Boolean global);
RTYPE		assumed_rtype_tm	(char *id);
void		patfun_assume_tm	(char *id, Boolean global);
void		delete_patfun_assume_tm (char *id, Boolean global);
Boolean		is_assumed_patfun_tm	(char *id);
void 		do_const_assume_tm	(int kind, char *c, TYPE *t,
					 Boolean global);


/************************ From tableman.c ******************************/
/***********************************************************************/

extern HASH2_TABLE* pat_fun_table;
extern HASH2_TABLE* import_dir_table;
extern HASH2_TABLE* local_suppress_property_table;
extern HASH2_TABLE* unattach_property_table;

/* Initialization */

void 		init_tables_tm		(void);
void 		clear_table_memory	(void);

IMPORT_STACK*   current_or_standard_frame(void);
void 		pop_hash2		(LIST **x, 
					 void (*scanner)(HASH2_CELLPTR ));

/* Operators */

int 		operator_code_tm(char *s, Boolean *l_open);
Boolean 	is_unary_op_tm  (char *s);
void 		operator_tm	(char *s, int tok, Boolean l_open,
				 MODE_TYPE *mode);
void		sym_op_tm	(int sym, int tok);
void		prim_unop_tm	(char *name);
void		sym_prim_unop_tm(int sym);

/* Directory name table */

void 	def_import_dir		(char *name, char *dir);
char* 	get_import_dir		(char *name);
void    set_import_dir		(char *name, Boolean is_id);

/* Advise dcls */

void	no_tro_tm			(char *w, Boolean all_status);
void    no_tro_restore_tm		(void);
Boolean no_troq_tm			(char *w);
void 	global_suppress_property	(char *w);
void 	global_unsuppress_property	(char *w);
void 	local_suppress_property		(char *w, Boolean suppress);
Boolean is_suppressed_property		(char *w);

/* Roles */

void 	  add_role_completion_tm	(char *s, char *t);
STR_LIST* get_role_completion_list_tm	(char *name);

/* Context dcls */

void 	add_context_tm		(char *op, char *id, EXPR *val);
void 	add_simple_context	(char *context_id, char *f, int line);
void 	add_simple_contexts	(char *context_id, STR_LIST *idlist, int line);
LIST* 	get_context_tm		(char *cntx, Boolean complain, Boolean *found,
				 int line);
void    do_context_inherit	(char *to_context, char *from_context);
void    do_context_inherits	(char *to_context, STR_LIST *from_contexts);

/* Exception trapping */

void exc_trap_tm			(char *ex_name, int val);
void sym_exc_trap_tm			(int sym, int val);
int  usually_trapped_tm			(char *ex_name);


/************************ From strtbl.c *********************************/
/************************************************************************/

#define NO_VALUE -1

#define id_loc(s,k) 		str_loc(s,k,0)
#define string_const_loc(s,k)	str_loc(s,k,1)
#define id_tb(s,k) 		str_tb1(s,k,strhash(s),0)
#define id_tb1(s,k,h)		str_tb1(s,k,h,0)
#define string_const_tb(s,k)	str_tb1(s,k,strhash(s),1)
#define string_const_tb1(s,k,h)	str_tb1(s,k,h,1)

char*   str_tb1			(char *s, int kind, LONG hash, 
				 Boolean t);
int    	str_loc			(char *s, int k, Boolean t);
char*   id_tb_check   		(char *s);
char* 	id_tb10			(char *s, LONG hash);
char* 	id_tb0			(char *s) ;
char*	stat_id_tb		(char *s);
char*   string_const_tb_check 	(char *s);
char* 	string_const_tb10	(char *s, LONG hash);
char* 	string_const_tb0	(char *s) ;

void    clear_string_table_data	(void);
void    init_str_tbl    	(void);


/************************* From descrip.c ****************************/
/*********************************************************************/

Boolean compare_descriptions		(char *descr1, char *descr2);
void 	install_description_tm		(char *descr, TYPE *ty, 
					 STR_LIST *who_sees, 
		     	    		 GLOBAL_ID_CELL *gic, char *name, 
					 int line);
void    insert_ahead_description_tm	(char *id, char *descr, TYPE *t,
					 MODE_TYPE *mode);
void    do_ahead_description_tm		(char *id, TYPE *ty);

/************************* From missing.c ****************************/
/*********************************************************************/

/* Don't reorder the following.  Order is used in missing.c */

#define BASIC_MISSING_TABLE		0
#define STRONG_MISSING_TABLE		1
#define IMPORTED_MISSING_TABLE		2

extern HASH2_TABLE *missing_tables[];

void 		issue_missing_tm	(char *name, TYPE *t, 
					 MODE_TYPE *mode);
TYPE_LIST* 	types_declared_missing_tm(char *id, INT_LIST *l, LONG hash);
void	 	check_strong_missing_tm	(char *id, TYPE *t);
void		check_missing_tm	(EXPR *e);

/************************ From loctbl.c *******************************/
/**********************************************************************/

extern STR_LIST*		local_id_names;
extern EXPR_LIST*		local_id_exprs;
extern int			current_scope;
extern int			next_scope;
extern struct scopes_struct*	scopes;
extern int			scopes_size;
extern Boolean			suppress_id_defined_but_not_used;
extern LIST_LIST* 		pat_var_table;
extern Boolean			pat_vars_ok;

typedef LIST* LOCAL_FINGER;

#ifdef DEBUG
 void		show_local_ids_tm	(void);
 void 		print_local_env		(void);
#endif

EXPR*		new_id_tm		(EXPR *e, STR_LIST *where, int kind,
					 int no_check_global);
EXPR* 		get_local_id_tm		(char *name);
LOCAL_FINGER 	get_local_finger_tm	(void);
Boolean		is_local_id_tm		(char *id);
void		pop_scope_to_tm 	(STR_LIST *where);
void 		fix_scopes_array_g	(void);
void 		push_local_assume_tm	(char *x, RTYPE t);
void 		pop_local_assumes_tm	(void);
RTYPE 		get_local_assume_tm	(char *x);
void 		push_local_const_assume_tm(int kind, TYPE *t);
TYPE* 		get_local_const_assume_tm(int kind);
void 		push_local_pattern_assume_tm(char *x);
Boolean 	get_local_pattern_assume_tm(char *x);
void 		push_assume_finger_tm	(void);
void 		pop_assume_finger_tm	(void);
void		pop_local_assumes_and_finger_tm(void);
void 		clear_local_assumes_tm	(void);
void		init_loctbl_tm  	(void);
void            init_dcl_tm     	(int outer_scope);
EXPR*		pat_var_tm			(EXPR *e);
void		dcl_pat_vars_tm			(EXPR_LIST *l);
void 		check_pat_vars_declared_tm	(EXPR_LIST *l);



/************************* From idtbl.c ******************************/
/*********************************************************************/

extern Boolean unknowns_ok;

EXPR* 		cp_old_id_tm		(EXPR *id, int outer);
EXPR*		old_id_tm		(EXPR *id, int outer);

/************************* From visible.c ****************************/
/*********************************************************************/

Boolean 	is_visible		(char *dcl_package_name, 
					 STR_LIST *visible_in,
		   			char *req_package_name);
Boolean 	is_invisible		(char *name, STR_LIST *v);
Boolean         is_visible_global_tm	(char *name, char *package_name, 
					 Boolean force);
Boolean 	is_visible_typed_global_tm(char *name, char *package_name, 
					   TYPE *t, Boolean force);
Boolean 	visible_expectation	(char *package_name, 
					 EXPECTATION *expec);
Boolean 	visible_part		(char *package_name, PART *part);
Boolean 	some_visible		(char *package_name, 
					 EXPECTATION *expec);
Boolean 	visible_intersect	(char *a_name, STR_LIST *a_list, 
			  		 char *b_name, STR_LIST *b_list);
void 		remove_visibility	(char **a_name, STR_LIST **a_list,
		 		         char *b_name, STR_LIST *b_list);

/************************* From globtbl.c ****************************/
/*********************************************************************/

extern Boolean		suppress_expectation_descriptions;
extern Boolean 		disallow_polymorphism;
extern Boolean 		copy_expect_types;
extern Boolean 		put_restriction;
extern Boolean 		force_prim;
extern HASH2_TABLE *	global_id_table;
extern HASH2_TABLE *	expect_id_table;
extern HASH2_TABLE *	anticipate_id_table;
extern LIST *		local_expect_tables;
extern LIST *		other_local_expect_tables;
extern TYPE *		put_restriction_type;

GLOBAL_ID_CELL* get_gic_tm		(char *name, Boolean force);
GLOBAL_ID_CELL* get_gic_hash_tm		(char *name, LONG hash);
Boolean		is_global_id_tm		(char *name);
void 		mark_hidden		(char *name, TYPE *t);
TYPE* 	        get_part_cover_type	(PART *pt);
TYPE_LIST* 	expectation_type_list	(EXPECTATION *expec, 
					 char *package_name);
TYPE_LIST* 	entpart_type_list	(ENTPART *pts, Boolean include_hidden);

void 		note_expectation_tm	(char *id, TYPE *ty, 
					 STR_LIST *who_sees, 
					 int kind, MODE_TYPE *mode,
					 char *package_name, int line);
void 		note_expectations_p	(STR_LIST *l, TYPE *t,
					 int kind, MODE_TYPE *mode,
					 char *package_name, int line);
TYPE_LIST*	local_expectation_list_tm(char *id, Boolean other);
void		expect_ent_ids_p	(STR_LIST *l, TYPE *v_type, 
					 ROLE *v_role, int context,
					 MODE_TYPE *mode, int line,
					 Boolean report, 
					 char *def_package_name);
void		expect_ent_id_p	   	(char *name, TYPE *v_type, 
					 ROLE *v_role, int context,
					 MODE_TYPE *mode, int line,
					 Boolean report,
					 char *def_package_name);
EXPECTATION* 	get_expectation_tm	(EXPR *id);
EXPECT_LIST*	expect_global_id_tm	(char *name, TYPE *t, ROLE *r, 
					 STR_LIST *who_sees,
				         Boolean force, Boolean cpy, 
					 Boolean add_to_local, 
					 Boolean warn_if_hidden, 
					 MODE_TYPE *mode, 
					 char *package_name,
					 int line, EXPR *report_exp);
void 		add_restriction_tm	(char *name, TYPE *ty);
TYPE* 		get_restriction_type_tm	(EXPR *id);
TYPE_LIST*	define_global_id_tm	(EXPR *id, EXPR *the_dcl,
					 Boolean errok, MODE_TYPE *mode,
					 Boolean from_expect, 
					 EXPR_TAG_TYPE kind, int line,
					 STR_LIST *who_sees,
					 char *def_package_name);
void 		define_prim_id_tm	(EXPR *id, MODE_TYPE *mode);
void 		overlap_all_parts_tm	(void);
void 		add_expand_part_to_chain(EXPAND_PART **part, 
					 EXPAND_PART *newpart,
			      		 MODE_TYPE *mode);
void 		report_pat_avail	(EXPR *fun);
Boolean		get_expand_type_and_rule_tm(EXPR *patfun, EXPR *sspattern, 
					 int unif, Boolean lazy, int kind,
					 EXPR **outformals, EXPR **outrule,
					 int *status, char **pack_name);
TYPE* 		get_patfun_expectation_tm(GLOBAL_ID_CELL *gic, TYPE *t);
void 		install_role_tm		(ROLE *r, TYPE *ty, 
					 STR_LIST *who_sees, 
		     			 GLOBAL_ID_CELL *gic, EXPR *id, 
					 int line);
ROLE*		get_role_tm		(EXPR *id, STR_LIST **pack_names);

/************************* From contrib.c *****************************/
/*********************************************************************/


extern HASH2_TABLE* self_overlap_table;
extern INT_LIST*    expectation_contributions;

INT_LIST* 	contribution_list	(EXPR *defs, int pass);
int 		compare_contribution_lists(INT_LIST *a, INT_LIST *b, 
					   INT_LIST **result);
Boolean 	should_take		(EXPR *dcl);
void            do_first_pass_tm        (EXPR *e);
void 		check_for_self_overlaps (EXPR *e, MODE_TYPE *mode);

/************************* From consts.c *****************************/
/*********************************************************************/

#define NAT_CON		0
#define RAT_CON		1

extern char** id_names;

ENTITY make_str		(charptr s);
ENTITY make_strn        (charptr s, LONG n);
ENTITY make_cstr	(char *s);
char*  make_perm_str    (char *s);
char*  str_tb		(char *s, int k);
char*  stat_str_tb	(char *s);
char*  stat_str_tb1	(char *s, LONG hash);
int    name_tb		(char *s);
LONG   const_tb		(char *s, int k);
LONG   string_tb	(char *s);
ENTITY intern_string_stdf(ENTITY s);
void   init_str_tb	(void);

extern LONG 	      next_const;
extern ENTITY HUGEPTR constants;


/*************************** From m_glob.c ********************************/
/**************************************************************************/

/****************************************************************
 * 			VARIABLES				*
 ****************************************************************/

extern GLOBAL_HEADER_PTR 	outer_bindings;
extern int                      next_ent_num;
extern ENTITY 			theCommandLine;
extern GLOBAL_TABLE_NODE**	set_start_loc;

/****************************************************************
 * 			FUNCTIONS				*
 ****************************************************************/

SIZE_T          ent_str_tb		(char *s);
void		init_mono_globals	(void);
void		mono_global     	(char *name, CODE_PTR type_instrs, 
					 TYPE *t, ENTITY val);
Boolean 	mono_lookup		(TYPE *t, LONG name_index, ENTITY *e);
void   		insert_global		(LONG name_index, CODE_PTR type_instrs,
					 struct type *t, int packnum, 
					 LONG offset, int mode, Boolean dummy);
ENVIRONMENT* 	setup_globals		(CODE_PTR *cc, FILE *f);
void            reallocate_outer_bindings (void);
void            init_globals         	(void);

GLOBAL_TABLE_NODE* poly_lookup		(GLOBAL_TABLE_NODE *start, TYPE *t);


/*********************** From fileinfo.c *****************************/
/*********************************************************************/

extern char* standard_package_name;
extern char* alt_standard_package_name;
extern char* main_package_name;
extern char* main_package_imp_name;
extern int   import_seq_num;

extern IMPORT_STACK* file_info_st;
extern IMPORT_STACK* standard_import_frame;
extern IMPORT_STACK* import_archive_list;

#define current_resume_long_comment_at_eol file_info_st->resume_long_comment_at_eol
#define current_line_number 		file_info_st->line
#define main_context			file_info_st->context
#define current_package_name		file_info_st->package_name
#define current_file_name		file_info_st->file_name
#define current_shadow_stack    	file_info_st->shadow_stack
#define current_shadow_nums		file_info_st->shadow_nums
#define current_public_packages 	file_info_st->public_packages
#define current_private_packages 	file_info_st->private_packages
#define current_protected_packages 	file_info_st->protected_packages
#define current_import_ids		file_info_st->import_ids
#define current_import_dir 		file_info_st->import_dir
#define current_import_dir_table	file_info_st->import_dir_table
#define current_assume_table		file_info_st->assume_table
#define current_assume_role_table 	file_info_st->assume_role_table
#define current_patfun_assume_table 	file_info_st->patfun_assume_table
#define current_pat_const_table		file_info_st->pat_const_table
#define current_no_tro_table		file_info_st->no_tro_table
#define current_nat_const_assumption 	file_info_st->nat_const_assumption
#define current_real_const_assumption 	file_info_st->real_const_assumption
#define current_abbrev_id_table 	file_info_st->abbrev_id_table
#define current_ahead_descr_table 	file_info_st->ahead_descr_table
#define current_local_expect_table	file_info_st->local_expect_table
#define current_other_local_expect_table file_info_st->other_local_expect_table
#define current_op_table		file_info_st->op_table
#define current_unary_op_table		file_info_st->unary_op_table
#define current_default_table		file_info_st->default_table
#define current_import_seq_num		file_info_st->import_seq_num

#define standard_op_table standard_import_frame->op_table
#define standard_unary_op_table standard_import_frame->unary_op_table
#define standard_pat_const_table standard_import_frame->pat_const_table
#define standard_no_tro_table    standard_import_frame->no_tro_table
#define standard_import_dir_table standard_import_frame->import_dir_table
#define standard_abbrev_id_table standard_import_frame->global_abbrev_id_table

Boolean	check_imported		(char *name, Boolean complain);
void    note_imported		(char *package_name);
char*   install_package_name	(char *name, STR_LIST *inherits);
void    push_file_info_frame	(char *infileName, FILE *file);

STR_LIST* get_visible_in		(MODE_TYPE *mode, char *name);

void  install_second_import_info	(char *package_name);
void  archive_import_frame		(IMPORT_STACK *p);
void  archive_main_frame		(void);
void  copy_tables_down	 		(void);
char* package_name_from_file_name	(char *fname);

/****************************************************************
 *			From t_compl.c				*
 ****************************************************************/

extern Boolean printed_missing_expect_header;

void check_expects	(void);
void check_tf_expects   (void);
