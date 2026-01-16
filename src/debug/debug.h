/****************************************************************
 * File:    debug/debug.h
 * Purpose: Definitions for debugging.
 * Author:  Karl Abrahamson
 ****************************************************************/

#ifdef DEBUG

/*----------------------------------------------------------------------*
 * The following are the number of noncomment lines in dmsgs.txt, 	*
 * dmsgi.txt and dmsgt.txt, respectively.  Each must be one greater	*
 * than the number of the last line, since numbering starts at 0.	*
 *----------------------------------------------------------------------*/

#define NUM_DEBUG_MSG_S 111	/* For messages/dmsgs.txt */
#define NUM_DEBUG_MSG_I 356	/* For messages/dmsgi.txt */
#define NUM_DEBUG_MSG_T 593	/* For messages/dmsgt.txt */

CONST char* nonnull(CONST char *s);

/****************************************************************
 *			From debug.c				*
 ****************************************************************/

extern UBYTE force_ok_kind;
extern UBYTE trace;
extern UBYTE trace_frees;
extern UBYTE trace_infer;
extern UBYTE trace_unify;
extern UBYTE trace_gen;
extern UBYTE trace_classtbl;
extern UBYTE trace_overlap;
extern UBYTE trace_missing;
extern UBYTE trace_complete;
extern UBYTE trace_missing_type;

extern char** debug_msg_s;

extern LONG allocated_lists, allocated_types, allocated_stacks;
extern LONG allocated_exprs, allocated_hash2s, allocated_hash1s;
extern LONG allocated_cucs, allocated_report_records, allocated_roles;
extern LONG allocated_states, allocated_controls, allocated_continuations;
extern LONG allocated_activations, allocated_trap_vecs;
extern LONG allocated_environments, allocated_global_envs;
extern LONG allocated_descr_chains;
extern FILE *TRACE_FILE;
extern int yydebug;

void tracenl			(void);
void trace_ty			(TYPE *t);
void trace_ty_without_constraints(TYPE *t);
void print_type_list_separate_lines(TYPE_LIST *L, char *pref);
void print_type_list	        (TYPE_LIST *l);
void print_type_list_without_constraints(TYPE_LIST *l);
void print_type_list_separate_lines_without_constraints(TYPE_LIST *L, 
							char *pref);
void trace_s			(int,...);
void pause_for_debug		(void);
void indent			(int n);
void print_str_list		(struct list *l);
void print_str_list_nl		(struct list *l);

/*-----------------------------------------------------------------*
 * The following are defined in both t_debug.c and m_debug.c. 	   *
 * They are available for both the compiler and the interpreter.   *
 *-----------------------------------------------------------------*/

void read_debug_messages	(void);
void print_two_types	        (TYPE *t1, TYPE *t2);

/****************************************************************
 *			From t_debug.c				*
 ****************************************************************/

#ifdef TRANSLATOR
extern UBYTE do_yydebug;
extern UBYTE trace_arg, trace_imports;
extern UBYTE trace_standard, trace_preamble;
extern UBYTE trace_locals, do_trace_locals;
extern UBYTE trace_defs, do_trace_defs;
extern UBYTE trace_implicit_defs, do_trace_implicit_defs;
extern UBYTE trace_lexical, do_trace_lexical;
extern UBYTE trace_importation, do_trace_importation;
extern UBYTE trace_exprs, do_trace_exprs;
extern UBYTE trace_ids, do_trace_ids;
extern UBYTE trace_pm, do_trace_pm;
extern UBYTE trace_role, do_trace_role;
extern UBYTE trace_check, do_trace_check;
extern UBYTE trace_open_expr, do_trace_open_expr;
extern UBYTE trace_context_expr, do_trace_context_expr;
extern UBYTE trace_pat_complete, do_trace_pat_complete;
extern struct debug_tbl_struct inline_debug_tbl[];

void trace_t			(int,...);
void trace_rol			(ROLE *r);
int  set_inline_debug           (char *opt, char *pref, int varnum);
void set_traces			(Boolean should_trace);
void print_expr_list            (EXPR_LIST *l);
void print_list			(LIST *l, int n);
#ifdef FOS
void print_glob_bound_vars      (PRINT_TYPE_CONTROL *ty_b);
#endif
#endif


/****************************************************************
 *			From m_debug.c				*
 ****************************************************************/

#ifdef MACHINE
extern UBYTE full_trace, gctrace, gcshow, rctrace, trace_puts;
extern UBYTE alloctrace, statetrace;
extern UBYTE trace_env_descr, trace_print_rts, smallgctrace;
extern UBYTE time_out_often, trace_state, trace_env, trace_tv;
extern UBYTE trace_types, trace_extra, trace_global_eval;
extern UBYTE trace_mem, trace_control, trace_docmd;
extern LIST* trace_fun;
extern char** instr_name;

void 	trace_i			(int,...);
Boolean set_interpreter_debug	(char *opt);
void 	check_cont_rc		(CONTINUATION *c);
void 	print_coroutines	(LIST *cl);
void 	print_trapvec		(TRAP_VEC *tr);
void 	print_trapvecs		(LIST *l);
int  	print_instruction	(FILE* where, CODE_PTR addr, int instr);
void 	show			(void);
void 	print_gcend_info	(void);
#endif

/****************************************************************
 *			From prtgic.c				*
 ****************************************************************/

#ifdef TRANSLATOR
void print_role_chain		(ROLE_CHAIN *rc, int n);
void print_descrip_chain	(DESCRIP_CHAIN *dc, int n);
void print_expectations		(EXPECTATION *q, int n);
void print_entpart		(ENTPART *p, int n);
void print_entpart_chain	(ENTPART *q, int n);
void print_expand_part		(EXPAND_PART *p, int n);
void print_expand_part_chain	(EXPAND_PART *q, int n);
void print_patfun_expectations	(PATFUN_EXPECTATION *q, int n);
void print_gic			(GLOBAL_ID_CELL *g, int n);
void print_ent_table		(void);
#endif


/****************************************************************
 *			From dprtent.c				*
 ****************************************************************/

#ifdef MACHINE
void long_print_entity(ENTITY e, int n, Boolean full);
#endif

/****************************************************************
 *			From dprtexpr.c				*
 ****************************************************************/


extern char* expr_kind_name[];

#ifdef TRANSLATOR
void print_expr		(EXPR *e, int n);
void print_expr1	(EXPR *e, int n, PRINT_TYPE_CONTROL *ty_b);
#endif

/****************************************************************
 *			From dprintty.c				*
 ****************************************************************/

extern char* type_kind_name[];

void long_print_ty	(TYPE *t, int n);

#endif

