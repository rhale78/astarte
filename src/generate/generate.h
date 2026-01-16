/******************************************************************
 * File:    generate.h
 * Purpose: Describe code generation functions and variables
 * Author:  Karl Abrahamson
 ******************************************************************/

#define ENV_MAX	255	/* largest environment frame */

extern FILE*     genf;
extern int       next_llabel;
extern CODE_PTR  glob_genloc, exec_genloc;
extern CODE_PTR  glob_genlast, exec_genlast;
extern CODE_PTR* genloc, *genlast;
extern INT_STACK envloc_st, numlocals_st;
extern LIST_STACK env_id_st, rt_bound_vars_st;
extern CODE_PTR  glob_code_array, exec_code_array, current_code_array;
extern SIZE_T    glob_code_array_size, exec_code_array_size, 
                 current_code_array_size;
extern CODE_PTR  last_label_loc;
extern Boolean suppress_tro, forever_suppress_tro;
#ifdef TRANSLATOR
 extern Boolean gen_code;
 extern Boolean main_gen_code;
#endif

/******************** From generate.c *****************************/

/* Target array selection */

void init_global_code_array	(void);
void select_global_code_array	(void);
void init_exec_code_array	(void);
void select_exec_code_array	(void);
void write_code_array		(CODE_PTR code, CODE_PTR endaddr);
void write_global_code_array	(void);

/* Fingering and retracting */

package_index	finger_code_array	(void);
void 		retract_code_to		(package_index finger);


/* Instruction generation */

package_index	gen_g		(int i);
void		write_g		(int n);
package_index	genP_g		(int i, long param);
void		gen_str_g	(char *s);
int 		get_label_g	(void);
void		gen_long_label_g(int l);
package_index	begin_labeled_instr_g(void);
int 		label_g		(package_index high, package_index low);
int		glabel_g	(void);
int     	gen_let_g	(EXPR *id);
void            gen_name_instr_g(int kind, char *name);
void		note_id_g	(EXPR *id);



/* Miscellaneous */

void 		set_offsets_g		(EXPR_LIST *a, EXPR_LIST *b);
int		un_env_size_g		(int n, Boolean local);
int		incr_env_g		(void);
void 		pop_env_scope_g		(int envmark);
void		pop_scope_g		(int envmark);
void		true_push_scope_g	(void);
void		true_pop_scope_g	(void);
void 		end_package_g		(void);

/* Initialization */

void		init_generate_g		(char *file_name);


/********************* From improve.c *********************************/

extern Boolean suppress_code_improvement;

void		code_out_g		(CODE_PTR code, 
					 CODE_PTR *endloc);

/********************* From genstd.c *********************************/

extern Boolean generating_standard_funs;
void gen_standard_funs	(EXPR **chain);

/********************* From genexec.c ********************************/

extern Boolean generate_implicit_pop;
extern int     irregular_gen, in_typesafe;

int		generate_code_g		(EXPR **e, int scope, 
					 int kind, char *name);
void	 	generate_exec_g		(EXPR *e, Boolean tro, int line);

/********************* From gendcl.c *********************************/

extern TYPE_LIST *glob_bound_vars, *other_vars;

int 		generate_exception_g	(char* name, TYPE* domain, 
					 Boolean trap, char* descr);
int 		generate_str_dcl_g	(int kind, char *s, int line);
void 		generate_irregular_dcl_g(char *name, TYPE *ty);
void 		generate_define_dcl_g	(EXPR *s, TYPE_LIST *types, 
					 MODE_TYPE *mode);
void		generate_execute_dcl_g	(EXPR *e, MODE_TYPE *mode);
void		generate_import_dcl_g	(char *fname);
void 		generate_default_dcl_g  (CLASS_TABLE_CELL *C, TYPE *t);
void		generate_relate_dcl_g	(CLASS_TABLE_CELL *a, 
					 CLASS_TABLE_CELL *b);
void 		generate_meet_dcl_g	(CLASS_TABLE_CELL *a, char *aname,
					 CLASS_TABLE_CELL *b, char *bname,
			 		 CLASS_TABLE_CELL *c, char *cname,
					 int instr);

/********************* From genglob.c *******************************/

extern int      num_globals;

void	   	gen_global_id_g    (EXPR *e, Boolean special_ok);
void 		init_gen_globals   (void);
int 		global_offset_g	   (EXPR *e);
int 		type_offset_g	   (TYPE *t);
void	 	take_apart_g	   (TYPE *t, MODE_TYPE *mode);
Boolean		generate_globals_g (void);

/********************** From gentype.c ***********************************/

void   gen_free_vars		(TYPE *t);
void   bare_generate_type_g	(TYPE *t, HASH2_TABLE **ty_b, int *st_loc,
			  	int mode);
void   generate_type_g		(TYPE *t, int mode);
void   generate_constraints_g	(TYPE_LIST *L);

/********************** From genwrap.c ***********************************/

void	gen_wrap_g		(TYPE *t);
void 	gen_unwrap_g		(TYPE *t, EXPR *fun, int line);
void 	gen_unwrap_test_g	(TYPE *A, TYPE *t, EXPR *the_fun, 
				 Boolean *did_something);

