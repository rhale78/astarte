/************************************************************
 * File:    error/error.h
 * Purpose: Error handling in translator
 * Author:  Karl Abrahamson
 ************************************************************/

extern FILE *LISTING_FILE;
extern FILE *ERR_FILE;

/****************************************************************
 *			From newrole.c				*
 ****************************************************************/

#define BASIC_ROLE_KIND 1
#define PAIR_ROLE_KIND  2
#define FUN_ROLE_KIND   3

#define ROLE_ROLE_MODE  1
#define CHECK_ROLE_MODE 2
#define NO_ROLE_MODE	4
#define KILL_ROLE_MODE	8

#define SET_ROLE(x,r) 	set_role(&(x), r)
#ifdef GCTEST
# define RKIND(r)	(rkindf(r))
#else
# define RKIND(r)	((r)->kind)
#endif

extern Boolean 	role_error_occurred;
extern Boolean 	ignore_trapped_exceptions;
extern Boolean 	forever_ignore_trapped_exceptions;
extern ROLE *	colon_equal_role;   /* Role -(destination,source) */
extern char *	pot_produce_stream, *act_produce_stream;
extern char *	pot_fail, *act_fail;
extern char *	pot_side_effect, *act_side_effect;
extern char *	is_pure_role, *is_lazy_role, *is_lazy_list_role;
extern char *	select_left_role, *select_right_role;

void 	init_properties		(void);
void 	init_roles         	(void);
void 	declare_role		(char *name, TYPE *ty, char *r);
Boolean is_singleton_role	(char *name);
int  	rkindf			(ROLE *r);
Boolean subrole			(ROLE *r1, ROLE *r2);
Boolean role_equal		(ROLE *r1, ROLE *r2);
void    do_def_dcl_role_check   (EXPR *sse, EXPR *id, char *spec_name,
			         TYPE_LIST *types, int line);
void    attach_property		(char *r, char *name, TYPE *ty);
ROLE* 	allocate_role		(void);
void 	bump_role		(ROLE *r);
void 	drop_role		(ROLE *r);
void 	set_role		(ROLE **x, ROLE *r);
void 	bump_rtype		(RTYPE r);
void 	drop_rtype		(RTYPE r);
void 	drop_hash_role  	(HASH2_CELLPTR h);
ROLE* 	new_role		(int kind, ROLE *r1, ROLE *r2);
ROLE* 	basic_role		(char *name);
ROLE* 	pair_role		(ROLE *r1, ROLE *r2);
ROLE* 	fun_role	       	(ROLE *r1, ROLE *r2);
Boolean has_act_fail		(STR_LIST *l);
Boolean is_present		(int mode);
Boolean is_potential		(char *s);
char* 	get_singleton		(ROLE *r);
char* 	make_tagged_string	(int mode, char *name);
Boolean rolelist_member		(char *name, STR_LIST *l, Boolean killed);
ROLE* 	meld_roles		(ROLE *r1, ROLE *r2);
ROLE* 	checked_meld_roles	(ROLE *r1, ROLE *r2, EXPR *e);
STR_LIST* complete_namelist	(STR_LIST *l);
ROLE*    complete_role		(ROLE *r, Boolean full);
ROLE* 	remove_knc		(ROLE *r, int mode);
STR_LIST* clean_role		(STR_LIST *l, Boolean *sing);
void 	check_namelist		(STR_LIST *l, EXPR *e);
void 	check_role		(ROLE *r, EXPR *e);
ROLE* 	role_check		(EXPR  *e, Boolean suppress_errs);
void 	do_role_check		(int kind, EXPR *e);
void 	copy_main_roles		(EXPR *a, EXPR *b);
ROLE* 	role_power		(ROLE *r, int k);
void    drop_hash_role  	(HASH2_CELLPTR h);
void    bump_hash_role          (HASH2_CELLPTR h);


/****************************************************************
 *			From error.c				*
 ****************************************************************/

/**********************************************************************
 * One ERR_FLAGS_TYPE structure is created to hold flags that control *
 * error and warning reporting.  It is called err_flags.	      *
 * ********************************************************************/

typedef struct err_flags_type {

 /****************************************************************
  *			need_err_nl			 	 *
  ****************************************************************
  * need_err_nl is set true in the lexer whenever any listing	 *
  * material is printed.  It is used so that error reporting	 *
  * knows whether to print a new line before beginning a	 *
  * report.							 *
  ****************************************************************/

 Boolean need_err_nl;

 /****************************************************************
  *			novice					 *
  ****************************************************************
  * True if error messages should be taken from file		 *
  * NOVICE_ERR_MSGS_FILE rather than from the more terse	 *
  * ERR_MSGS_FILE.						 *
  ****************************************************************/

 Boolean novice;		

 /****************************************************************
  *			suppress_warnings			*
  *			no_suppress_warnings			*
  *			forever_suppress_warnings		*
  ****************************************************************
  * suppress_warnings is true if warnings should not be printed  *
  * for the currrent declaration.  forever_suppress_warnings     *
  * is the value that suppress_warnings is set to at the start   *
  * of a declaration.  So it is the default value.  		*
  * no_suppress_warnings overrides suppress_warnings, and causes *
  * warnings to be printed always, if true.			*
  ****************************************************************/

 Boolean no_suppress_warnings;
 Boolean suppress_warnings;
 Boolean forever_suppress_warnings;

 /****************************************************************
  *		suppress_global_shadow_warnings			*
  *		forever_suppress_global_shadow_warnings		*
  ****************************************************************
  * suppress_global_shadow_warnings is true if warnings about	*
  * global shadowing should be suppressed.			*
  ****************************************************************/

 Boolean suppress_global_shadow_warnings;
 Boolean forever_suppress_global_shadow_warnings;

 /****************************************************************
  *		suppress_dangerous_default_warnings		 *
  *		forever_suppress_dangerous_default_warnings	 *
  ****************************************************************
  * suppress_dangerous_default_warnings is true to suppress	 *
  * printing of warnings about defaults that might not be 	 *
  * intended.  forever_suppress_dangerous_default_warnings is    *
  * the value that suppress_dangerous_default_warnings is set to *
  * at the start of each declaration.				 *
  ****************************************************************/

 Boolean suppress_dangerous_default_warnings;
 Boolean forever_suppress_dangerous_default_warnings;

 /****************************************************************
  *		suppress_pm_incomplete_warnings			 *
  *		forever_suppress_pm_incomplete_warnings		 *
  ****************************************************************
  * suppress_pm_incomplete_warnings is true to suppress printing *
  * of warnings about patterns not being exhaustive.		 *
  * forever_suppress_pm_incomplete_warnings is the value that	 *
  * suppress_pm_incomplete_warnings is set to at the start of	 *
  * each declaration.						 *
  ****************************************************************/

 Boolean suppress_pm_incomplete_warnings;
 Boolean forever_suppress_pm_incomplete_warnings;

 /****************************************************************
  *			suppress_missing_warnings		 *
  *			forever_suppress_missing_warnings	 *
  ****************************************************************
  * suppress_missing_warnings is true to suppress printing of	 *
  * warnings about use of an identifier of a missing species.	 *
  * forever_suppress_missing_warnings is the value that		 *
  * suppress_missing_warnings is set to at the start of each	 *
  * declaration.						 *
  ****************************************************************/

 Boolean suppress_missing_warnings;
 Boolean forever_suppress_missings;

 /****************************************************************
  *			suppress_property_warnings		 *
  *			forever_suppress_property_warnings	 *
  ****************************************************************
  * suppress_property_warnings is true to suppress printing of	 *
  * warnings about property violations.				 *
  * forever_suppress_property_warnings is the value that	 *
  * suppress_property_warnings is set to at the start of each	 *
  * declaration.						 *
  ****************************************************************/

 Boolean suppress_property_warnings;
 Boolean forever_suppress_property_warnings;

 /****************************************************************
  *			suppress_unused_warnings		 *
  *			forever_suppress_unused_warnings	 *
  ****************************************************************
  * suppress_unused_warnings is true to suppress printing of	 *
  * warnings about identifiers that are defined but not used.	 *
  * forever_suppress_unused_warnings is the value that		 *
  * suppress_unused_warnings is set to at the start of each	 *
  * declaration.						 *
  ****************************************************************/

 Boolean suppress_unused_warnings;
 Boolean forever_suppress_unused_warnings;

 /****************************************************************
  *			did_unbound_tyvar_err			 *
  *			reported_non_assoc_err			 *
  ****************************************************************
  * did_unbound_tyvar_err is true if an "unbound type or family	 *
  * variable" error was reported in the current declaration.  If *
  * is used to prevent multiple error reports of this kind.	 *
  *								 *
  * reported_non_assoc_err is true if an error was reported in	 *
  * the current extension that the hierarchy is not associative. *
  * It is used to prevent multiple error reports.		 *
  ****************************************************************/

 Boolean did_unbound_tyvar_err;
 Boolean reported_non_assoc_error;

 /************************************************************************
  *			check_indentation				 *
  *			forever_check_indentation			 *
  *			suppress_indentation_errs			 *
  ************************************************************************
  * check_indentation is true if the lexer should check the indentation  *
  * of declarations.  forever_check_indentation is the value that	 *
  * check_indentation will be set to at the beginning of each		 *
  * declaration.  When an indentation warning occurs, the lexer      	 *
  * sets suppress_indentation_errs true, to keep from giving multiple    *
  * errors in a single declaration.  The parser sets 			 *
  * suppress_indentation_errs true at the start of each declaration.	 *
  ************************************************************************/

 Boolean suppress_indentation_errs;
 Boolean check_indentation;
 Boolean forever_check_indentation;

} ERR_FLAGS_TYPE;

extern ERR_FLAGS_TYPE err_flags;

extern Boolean
  error_occurred, warning_occurred,
  local_error_occurred, error_reported;

extern char **error_strings;

void	err_reset_for_dcl	(void);
void    load_error_strings	(void);
void    err_print_str		(CONST char *s, ...);
void    err_print		(int f, ...);
void	err_shrt_pr_expr	(EXPR *e);
void    err_short_print_expr	(EXPR *e);
void    err_print_ty    	(TYPE *t);
void 	err_print_ty_with_constraints_indented
				(TYPE *t, int n);
void    err_print_role  	(ROLE *r);
void    err_print_rt1		(TYPE *t, ROLE *r, PRINT_TYPE_CONTROL *c);
void    err_print_rt1_with_constraints_indented
				(TYPE *t, ROLE *r, PRINT_TYPE_CONTROL *c, 
				 int n);
void 	err_print_rt		(TYPE *t, ROLE *r);
void    err_print_rt_with_constraints_indented
				(TYPE *t, ROLE *r, int n);
void 	err_nl			(void);
void    err_nl_if_needed	(void);
void    err_begin_dcl   	(void);
void    err_head		(int kind, int line);
void    warn0			(int n, int line);
void    warn1			(int n, CONST char *s, int line);
void    warn2			(int n, CONST char *s1, CONST char *s2, 
				 int line);
Boolean should_suppress_warning	(Boolean suppress_this_warning);
void    def_prop_warn   	(CONST char *id, CONST char *prop, 
				 TYPE *t, int line);
void    dangerous_default_warn	(EXPR *e, TYPE *v, TYPE *dfault, 
			         CONST char *defining);
void 	warn_global_shadow	(CONST char *name, int line);
void 	warn_strong_missing	(CONST char *name, TYPE *def_type, 
				 TYPE *miss_type);
void    mo_warn			(EXPR *e, LIST *properties);
void    sure_type_mismatch_warn	(EXPR *fun, TYPE *t, TYPE *A);
void    constrained_dyn_var_err	(TYPE *V, EXPR *fun);
void    warne			(int f, CONST char *a, EXPR *e);
void 	syntax_error		(int n, int line);
void	syntax_error1		(int n, CONST char *s, int line);
void    syntax_error2   	(int n, CONST char *s1, CONST char *s2, 
				 int line);
void    syntax_error3   	(int n, CONST char *s1, CONST char *s2, 
				 CONST char *s3, 
				 int line);
void 	soft_syntax_error 	(int n, int line);
void	soft_syntax_error1	(int n, CONST char *s, int line);
void    soft_syntax_error2	(int n, CONST char *s, CONST char *s2, 
				 int line);
void	semantic_error		(int n, int line);

#define semantic_error1		syntax_error1
#define semantic_error2		syntax_error2
#define semantic_error3		syntax_error3

void    err_print_labeled_type	(CLASS_TABLE_CELL *ctc, LPAIR lp);
int 	yyerror			(char *s);
void    show_syntax		(int tok);
void	eof_err			(void);
void    bad_cu_mem_err  	(TYPE *t, int line);
void    bad_patfun_type_error	(CONST char *name, TYPE *prev_type, 
				 TYPE *neww_type);
void    bad_extens_err  	(CLASS_UNION_CELL *t, CLASS_TABLE_CELL *c,
			 	 CONST char *s);
void    dup_intersect_err	(CONST char *a_name, CONST char *b_name, 
				 CLASS_TABLE_CELL *c, 
			 	 LPAIR c_labs, int h_val, LPAIR h_labs);
void    bad_overlap     	(int n, CONST char *name, 
				 TYPE *old_overlap_type, 
				 TYPE *new_overlap_type, 
				 CONST char *old_package_name,
				 int old_line, int line);
void    parts_overlap_err	(CONST char *name, ENTPART *p1, ENTPART *p2);
void    polymorphism_error	(EXPR *e);
void	type_error		(EXPR *e, TYPE *t);
void	role_error		(EXPR *e, CONST char *r1, CONST char *r2,
				 Boolean property_err);
void    report_role     	(ROLE *this_role, CONST char *name,
				 STR_LIST *pack_names);
void    err_print_expr  	(EXPR *e);
void    expr_error		(int n, EXPR *e);
void    unbound_tyvar_error	(EXPR *e, TYPE *V, CONST char *defining);
void	appl_type_error		(EXPR *e1, EXPR *e2);
void    no_pat_parts_error	(EXPR *pf);
void    check_for_bad_compile_id(void);
Boolean unknown_id_error	(int err, CONST char *id, int line);
Boolean check_for_ent_id	(CONST char *name, int line);
void    report_errors		(void);
void	init_error_strings	(void);


/****************************************************************
 *			ERRORS					*
 ****************************************************************/


/*----------------------------------------------------------*
 * Do not use numbers 0 and 1: they are reserved by error.c *
 *----------------------------------------------------------*/

#define MEM_ERR				2
#define FILE_NAME_ERR			3
#define NO_FILE_ERR			4
#define CANNOT_DO_DIR_DEF_ERR           5
#define PAT_VAR_IS_RESERVED_ERR		6
#define CLASS_ID_AS_ENT_ID_ERR          7
#define QUAL_ERR			8
#define STRING_EOF_ERR			9
#define NEWLINE_IN_STRING_ERR   	10
#define BAD_CHAR_ERR			11
#define BAD_PERCENT_ERR			12
#define BAD_SGL_QUOTE_ERR		13
#define CLASS_ID_AFTER_ENT_ID_ERR	14
#define UNEXPECTED_TOKEN_ERR		15
#define TEXT_AFTER_END_PACKAGE_ERR	16
#define NUMBER_CHAR_TOO_LARGE_ERR	17
#define NULL_IN_STRING_CONST_ERR	18
#define CYCLIC_IMPORT_ERR		19
#define DUPLICATE_PACKAGE_NAMES_ERR	20
#define ENT_ID_AS_CLASS_ID_ERR		21
#define SYNTAX_ERR			22
#define PANIC_ERR               	23
#define MEANT_ERR			24
#define PERIOD_ERR			25
#define END_ON_PERIOD_ERR		26
#define BAD_END_ERR			27
#define END_PROVIDED_ERR		28
#define PAREN_PROVIDED_ERR		29
#define PROC_ENDED_ERR			30
#define END_IGNORED_ERR			31
#define EOF_ERR				32
#define PACKAGE_ENDED_EXPECTING_ERR	33
#define REQUIRE_UC_ERR			34
#define REQUIRE_LC_ERR			35
#define EMPTY_BLOCK_ERR			36
#define DO_DBL_ARROW_NOT_MATCH_ERR	37
#define CASE_AFTER_ELSE_ERR		38
#define NO_CASES_ERR			39
#define FIRST_MIXED_ERR			40
#define EMPTY_CONT_ERR			42
#define FULL_CONT_ERR			43
#define PERHAPS_NOT_ALLOWED_ERR		44
#define WORD_EXPECTED_ERR		45
#define DEFLET_UNTIL_ERR		46
#define BAD_WHEN_ERR			47
#define FUN_IS_PATVAR_ERR		48
#define ONE_DO_ERR			49
#define SYNTAX_IS_ERR			50
#define SUCCEED_IGNORED_ERR		51
#define BAD_UNTIL_ERR			52
#define CANNOT_EVAL_COND_ERR		53
#define RECURSIVE_LET_ERR		54
#define SUBCASES_EXPECTED_ERR		55
#define SUBCASES_NOT_ALLOWED_ERR	56
#define RELET_IN_TEAM_ERR		57
#define RELET_DCL_ERR			58
#define BAD_AHEAD_MEET_ERR		59
#define OPEN_DEFINE_ERR			60
#define DEF_CASE_PARAM_MISMATCH_ERR     61
#define BAD_DEF_HEAD_ERR		62
#define UNDER_OVER_ERR			63
#define BAD_MODE_ERR			64
#define NAME_MISMATCH_ERR		65
#define DEFLET_NO_EQUAL_ERR		66
#define INIT_WITH_UNKNOWN_ARRAY_ERR	67
#define THEN_NOT_ALLOWED_ERR		68
#define MULTI_DESCRIP_IN_WITH_ERR	69
#define BAD_LET_COPY_ERR		70
#define IRREG_EQUAL_REG_ERR		71
#define WHILE_DEFINING_ERR		72
#define NONFUN_IRREGULAR_ERR		73
#define ID_DEFINED_ERR			74  
#define NONLOCAL_RELET_ERR		75
#define EXPECTATION_ERR 		76
#define OVERLOADS_TOO_LARGE_ERR		77
#define ASSUME_OVERLOAD_ERR		78
#define TOO_MANY_OVLDS_ERR		79
#define SPEC_NONSPEC_ERR		80
#define RESTRICTED_ID_DEF_ERR		81
#define DCL_PROP_ERR			82
#define DEF_PROP_ERR			83
#define DCL_PRODUCE_STREAM_ERR		84
#define DCL_FAIL_ERR			85
#define WRAP_PATFUN_ERR			86
#define VAR_DIM_TOO_LARGE_ERR		87
#define UNKNOWN_PREV_ID_ERR		88
#define POLYMORPHISM_ERR		89
#define SPECIFIC_BOX_TYPE_ERR		90
#define BAD_FAM_MACRO_USE_ERR		91
#define BAD_CU_MEM_ERR			92  
#define COLON_EXP_ERR  			93
#define BARE_EXTENSION_ERR		94
#define FAM_EXPECTED_ERR		95
#define TYPE_EXPECTED_ERR		96
#define PRIV_PROT_ERR			97
#define GEN_FAM_MEM_ERR			98
#define BAD_DISCRIM_ERR			99   
#define VAR_EXPECTED_ERR		100
#define CONSTR_REQD_ERR			101
#define BAD_MEET_ERR2			102
#define CANNOT_DEFINE_EQUAL_ERR		103
#define CANNOT_BRING_ERR   		104
#define BRING_WITH_LIST_EQUAL_ERR       105
#define DUPLICATE_TYPE_ERR		106
#define CANNOT_EXTEND_ERR		107
#define CURRIED_EXTEND_ERR		108
#define NOT_EXTENS_ERR			109
#define VAR_IN_LEFT_CONTEXT_ERR		110
#define CYCLE_ERR			111
#define BAD_ASSUME_ERR			112
#define DEFINING_ERR			113
#define INTERSECT_CLOSURE_ERR		114
#define CLOSURE_ERR			115
#define BAD_LINK_LABEL_ERR		116
#define NO_CG_ERR 			117
#define BAD_BINDING_ERR			118
#define CANNOT_UNIFY_ERR		119
#define WITH_IN_CLASS_DCL_ERR		120
#define BAD_VAR_ERR			121
#define NO_SUCH_TYPE_ERR		122
#define CANNOT_TRANSFER_MARKS_ERR	123
#define PATMATCH_DEPTH_ERR		124
#define BAD_PAT_FORMALS_ERR		125
#define BAD_PATFUN_TYPE_ERR		126
#define DUP_IN_PATRULE_ERR		127
#define PATVAR_AS_NONPATVAR_ERR		128
#define PATVAR_NOT_BOUND_ERR		129
#define NO_PAT_VARS_ERR			130
#define NO_PAT_PARTS_ERR		131
#define NO_RULE_ERR			132
#define BAD_PAT_ERR			133
#define TARGET_ERR			134
#define LOCAL_PATFUN_ERR		135
#define UNBOUND_VAR_IN_WHERE_ERR	136
/*--------------------------------*
 * The following two are the same *
 *--------------------------------*/
#define PRMODE_OVERLAP_ERR		137
#define INCONSISTENT_MODES_ERR		137 
#define PM_INCOMPLETE_ERR		138
#define ALSO_CONFLICT_ERR		139
#define DUP_TAG_ERR			140
#define UNKNOWN_ID_ERR			141
#define UNK_ID_ERR			142
#define LATE_UNKNOWN_ERR		143
#define BAD_UNARY_OP_ERR		144
#define IRREGULAR_CONTEXT_ERR		145
#define DEFINED_BUT_NOT_USED_ERR	146
#define HIDDEN_EXP_NONHIDDEN_ID_ERR	147
#define CANNOT_MATCH_TYPE_ERR		148
#define LOCAL_EXPECT_ERR		149
#define TYPE_ERR			150
#define EXECUTE_OVLD_ERR		151
#define TYPE_UNBOUND_ERR		152
#define EXPECT_ASSUMPTION_ERR		153
#define BAD_REPRESENTATION_ERR		154
#define TYPE_ID_EXP_ERR			155
#define TYP_COMPL_ERR			156
#define PAT_SUBST_TYPE_ERR		157
#define ROLE_MISMATCH_ERR		158
#define LATE_DEFER_ERR			159
#define DEFAULT_WARN_ERR		160
#define RESTRICTION_ERR			161
#define MISSING_TYPE_ERR		162
#define PATMAT_EQ_ERR			163
#define STRONG_MISSING_ERR		164
#define SELF_OVERLAP_ERR		165
#define UNKNOWN_ID_REASONS_ERR		166
#define LABEL_ERR			167
#define LOCAL_IDS_ERR			168
#define NO_GEN_ERR			169
#define BAD_CONST_KIND_ERR		170
#define BAD_APPLY_PRIM_ERR		171
#define BAD_SPECIAL_ERR			172
#define BAD_EXPR_KIND_ERR		173
#define CANNOT_DCL_STR_ERR		174
#define PM_SPLITS_NONPAIR_ERR		175
#define BAD_MEET_ERR3			176
#define MARKED_WITH_ROLE_ERR		177
#define POSSIBLY_ROLE_ERR		178
#define GEN_PATVAR_ERR			179
#define NOT_OP_ERR			180
#define BAD_OP_ERR			181
#define OPEN_UNARY_OP_ERR		182
#define MAN_MISMATCH_ERR		183
#define MULTI_DESCR_ERR			184
#define TOO_MANY_EXC_ERR		185
#define EXCEPTION_CONTEXT_ERR           186
#define EXPECTED_IMPORT_ERR		187
#define BAD_VAROPT_ERR			188
#define RHS_NO_EXPAND_ERR		189
#define RHS_NOT_PATFUN_ERR		190
#define LAST_RESERVED_ERR		191
#define USE_SHOW_ERR			192
#define DASHES_ERR			193
#define SHOW_RES_ERR			194
#define BAD_ADVISORY_ERR		195
#define BAD_UNKNOWN_ID_ERR		196
#define EQ_IN_MATCH_ERR			197
#define HIDDEN_IMPORT_ERR		198
#define BAD_PACKAGE_HEADING_ERR		199
#define BAD_CONTEXT_ERR			200
#define SHOULD_NOT_ERR			201
#define STREAM_BEH_ERR			202
#define SHOULD_BE_ERR			203
#define LAZY_STREAM_ERR			204
#define LAZY_FAIL_ERR			205
#define AVAIL_TYPES_ERR			206
#define FUN_GEN_MEM_ERR			207
#define AVAILABLE_TYPES_ERR		208
#define CONSIDER_TAG_ERR		209
#define EXPRESSION_ERR			210
#define PREV_ROLE_ERR			211
#define CANNOT_APPLY_ERR		212
#define FUNCTION_IS_ERR			213
#define ARGUMENT_IS_ERR			214
#define INCONSISTENT_INTERSECT_ERR	215
#define REQUEST_ERR			216
#define NON_ASSOC_ERR			217
#define DEF_OVERLAP_ERR			218
#define DEFAULT_ERR			219
#define MISSING_CASE_ERR		220
#define CURRENT_TYPE_ERR		221
#define NOW_NEED_ERR			222
#define UNBOUND_VAR_IS_ERR		223
#define DEFINED_TYPE_IS_ERR		224
#define MISSING_TYPE_IS_ERR		225
#define BAD_MEM_IS_ERR			226
#define PACKAGE_FILE_ERR		227
#define NO_INTERFACE_ERR		228
#define EXP_OVERLAP_ERR			229
#define SPACE2_STR_ERR			230
#define POSSIBLE_TYPES_ERR		231
#define SPACE3_ERR			232
#define THIS_DCL_ERR			233
#define EXPECTATIONS_ERR		234
#define SPACE2_ERR			235
#define EXPECTED_STR1_ERR		236
#define EXPECTED_STR2_ERR		237
#define MATCHING_LEFT_ERR		238
#define MATCHING_BEGIN_ERR		239
#define FAM_INTERSECT_ERR		240
#define CURRENT_TYPE_STR_ERR		241
#define RESTRICTED_TYPE_ERR		242
#define STR_ERR				243
#define PREVIOUS_LABEL_IS_ERR		244
#define NEW_LABEL_IS_ERR		245
#define EXPECTED_BUT_SEE_ERR		246
#define MISSING_QUESTION_MARK_ERR	247
#define HOLDING_REQUIRED_ERR		248
#define INCONSISTENT_JOIN_ERR		249
#define FOR_EXAMPLE_STR_ERR		250
#define MISMATCH_DESCR_PACKAGE_ERR	251
#define TYPE_OF_STR_ERR			252
#define COLON_ERR			253
#define PACKAGE_STR_ERR			254
#define STR_LINE_ERR			255
#define ROLE1_ERR			256
#define ROLE2_ERR			257
#define INFERRED_STR_ERR		258
#define MEET2_ERR			259
#define THIS_DCL_TYPE_ERR		260
#define OVERLOAD_POSSIBILITIES_ERR	261
#define UNSAT_EXPECTATIONS_ERR		262
#define EG_MISSING_ERR			263
#define FAM_DOMAIN_MISMATCH_ERR		264
#define EXPECTATIONS_FOR_ERR		265
#define PACKAGE_LINE_ERR		266
#define NO_EXPECTATIONS_FOR_ERR		267
#define EXPECTATIONS_THIS_PACKAGE_ERR   268
#define INFER_RESULTS_ERR		269
#define ASSUMPTIONS_USED_ERR		270
#define LINE_STR_ERR			271
#define MISSING_EXPECTED_TF_ERR		272
#define TAG_MISMATCH_ERR		273
#define ROLE_MODIFY_OVERLAP_ERR		274
#define PREVIOUS_TYPE_IS_ERR		275
#define NEW_TYPE_IS_ERR			276
#define EXPECTED_SPECIES_PARAM_ERR	277
#define BAD_SPECIES_PARAM_ERR		278
#define INDENTATION_ERR			279
#define CONST_TOO_LONG_ERR		280
#define ERROR_ERR			281
#define WARNING_ERR			282
#define PACK_LINE_TYPE_ERR		283
#define UNKNOWN_PRIM_IMPORT_ERR		284
#define CONTEXT_EXISTS_ERR		285
#define PM_INCOMPLETE_EXPLAIN_ERR	286
#define NO_FILE_DIR_ERR			287
#define DUP_CONTEXT_ERR			288
#define ID_NOT_ALLOWED_ERR		289
#define LOCAL_ENV_TOO_LARGE_ERR		290
#define BAD_IRREGULAR_CODOMAIN_OVERLAP_ERR 291
#define PACKAGE_HAS_ERR			292
#define CANNOT_INSTALL_RESTRICTION_ERR  293
#define RESTRICTIVE_PATRULE_ERR		294
#define PATFUN_IS_ERR			295
#define UNBOUND_ID_IN_GEN_ERR		296
#define DUPLICATE_MODES_ERR		297
#define DEFAULTS_DONE_ERR		298
#define MISMATCHED_BRACE_ERR		299
#define IRRFUN_IS_ERR			300
#define MISMATCHED_BRACES_ERR		301
#define NOT_CLASS_ERR			302
#define VARIABLE_IS_ERR			303
#define LOWER_BOUND_FAIL_ERR		304
#define CURRENT_CONTEXT_ERR		305
#define CONSTRAINED_DYN_VAR_ERR		306
#define ID_TOO_LONG_ERR			307
#define SYM_PACKAGE_NAME_ERR		308
#define OLD_EXTENSION_ERR		309
#define MOST_RECENT_BEGIN_ERR		310
#define GLOBAL_SHADOW_ERR		311
#define MODE_WITH_IF_ERR		312
#define DUPLICATE_TRY_MODE_ERR		313
#define ALL_CASE_WITH_MODE_ERR		314
#define BAD_TRAP_ERR			315
#define IRREG_OVERLAP_ERR		316
#define BRACE_MISMATCH_ERR		317
#define MODE_IS_ERR			318
#define PAT_AVAIL_ERR			319
#define BAD_MEET_ERR4			320
#define BAD_MEET_ERR			321
#define BAD_MEET_ERR5			322
#define BRING_CONTEXT_ERR		323
#define BAD_MARKED_VAR_IN_BRING_ERR	324
#define NONPRIMARY_RANKED_IN_BRING_ERR  325
#define DUPLICATE_MODE_PACKAGE_ERR	326
#define FAM_OPAQUE_MISMATCH_ERR		327
#define PARTIAL_MISMATCH_ERR		328
#define SPECIES_EXPECTATION_IN_CLASS_ERR 329
#define DEF_PACKAGE_NAME_IN_CLASS_ERR	330
#define BAD_SUPERCLASS_ERR		331
#define NO_CLASS_SUPER_REP		332
#define NO_TYPE_IN_CLASS_COMPONENT	333
#define ID_FOUND_IN_ERR			334

#define LARGEST_ERR_NO			334   /* Largest error number */


