/****************************************************************
 * File:    parser/parser.h
 * Purpose: Variables and definitions for parser
 * Author:  Karl Abrahamson
 ****************************************************************/

/******************** From parsvars.c ************************/

extern INT_STACK		stream_st;
extern INT_STACK		choose_st;
extern INT_STACK		case_kind_st;
extern INT_STACK		var_context_st;
extern EXPR_LIST*		class_vars;
extern EXPR_LIST*		class_consts;
extern EXPR_LIST*		class_expects;
extern EXPR*			initializer_for_class;
extern INT_STACK		list_expr_st;
extern EXPR_STACK		list_map_st;
extern INT_STACK		deflet_st;
extern INT_STACK		deflet_line_st;
extern STR_STACK		defining_id_st;
extern INT_STACK		if_dcl_st;
extern INT_STACK		if_dcl_then_st;
extern int			abbrev_code;
extern TYPE*	 		abbrev_val;
extern HASH2_TABLE*		abbrev_var_table;
extern int			propagate_var_opt;
extern EXPR_TAG_TYPE		patexp_dcl_kind;
extern char*			context_dcl_id;
extern Boolean			in_context_dcl;
extern char*			defining_id;
extern int			expect_context;
extern int			union_context;
extern int			num_params;
extern EXPR*			var_body_lets;
extern EXPR* 			var_body_inits;
extern FILE*			index_file;
extern char*			index_file_dir;
extern FILE*			description_file;
extern Boolean			wrote_description;
extern char*			root_file_name;
extern char*			main_file_name;
extern Boolean			all_init;
extern Boolean			tag_present;
extern Boolean			have_read_main_package_name;
extern Boolean 			about_to_load_interface_package;
extern Boolean			subcases_context;
extern Boolean			dcl_context;
extern Boolean			pat_rule_opts_ok;
extern Boolean			advisory_dcl;
extern Boolean			suppress_reset;
extern Boolean			inside_extension;
extern Boolean			interface_package;
extern Boolean			compiling_standard_asi;
extern Boolean			seen_export;
extern Boolean			echo_all_exprs;
extern UBYTE			show_all_types;
extern Boolean			no_copy_abbrev_vars;
extern Boolean 			open_pat_dcl;
extern Boolean			pat_dcl_has_pat_consts;
extern Boolean			stop_at_first_err;
extern EXPR*			dcl_pat_fun;
extern STR_LIST*		new_import_ids;
extern int			outer_context;

/****************************************************************
 *			CONTEXT AND ATTRIBUTE VALUES   		*
 ****************************************************************/

#define BODY_CX		1	/* In implementation part of main package */
#define INIT_CX		2	/* In initialization			  */
#define EXPORT_CX	3	/* In interface part of main package	  */
#define TYPE_DCL_CX	4	/* In a species declaration		  */
#define RELATE_DCL_CX	5	/* In a relate declaration		  */
#define IMPORT_CX	9	/* Importing a package			  */

#define ORDERED_ATT	1
#define UNORDERED_ATT	2
#define COMBINED_ATT	3


/****************************************************************
 *			FUNCTIONS				*
 ****************************************************************/

#define SET_EXPR1_P(l,k,e,n) 	    bmp_expr(l = new_expr1(k,e,n))
#define SET_EXPR2_P(l,k,e1,e2,n)    bmp_expr(l = new_expr2(k,e1,e2,n))
#define SET_EXPR3_P(l,k,e1,e2,e3,n) bmp_expr(l = new_expr3(k,e1,e2,e3,n))
#define DO_OPERATOR(l,a,b,c,d)	    bump_expr(l = op_expr_p(a,b,c,d))


/******************* From parseerr.c *********************************/

extern Boolean suppress_panic;
extern char** syntax_form;
extern char** alt_syntax_form;

void		read_syntax_forms(void);
void 		read_alt_syntax_forms(void);
void 		check_braces	(int kind1, int kind2);
void		check_else_p	(void);
void            check_empty_block_p(struct lstr* attr);
void 		check_end_p	(struct lstr *beginner, int num, 
				 struct lstr *ender);
Boolean 	dcl_panic_l	(void);
void		check_for_case_after_else_p(int line);
void		check_for_bad_loop_case_p  (int line);
void 		check_for_duplicate_ids_in_pattern_formal(EXPR *formal);


/******************* From parsutil.c *********************************/


RTYPE 		make_role_type_expr_p   (char *r, RTYPE v);
TYPE* 		type_id_tok_p   	(char *name);
TYPE* 		type_id_tok_from_ctc_p  (CLASS_TABLE_CELL *ctc);
TYPE*           make_meet_type_from_id_p(char *s);
EXPR* 		make_for_advisory	(LIST *props, LIST *ids, RTYPE tag,
				  	 int line);
void 		do_for_advisory   	(LIST *props, LIST *ids, TYPE *tag);
EXPR*		handle_loop_p		(struct lstr d1, EXPR *d5, EXPR *d7, 
					 struct lstr d8);
void 		finish_choose	  	(void);
char* 		get_name_from_cucs_p	(LIST *cucs);
void 		perform_meet_p		(char *A, char *B, LIST *rhs);
void            add_class_consts	(STR_LIST *ids, RTYPE T);
void 		add_class_expects	(STR_LIST *ids, RTYPE rt, char *descr);

CLASS_UNION_CELL* get_discrim_p	  	(char *dis, RTLIST_PAIR ts, 
					 LIST *withs, int irr, int line,
					 MODE_TYPE *mode);
CLASS_UNION_CELL* fam_cu_mem_p		(CLASS_TABLE_CELL *ctc, int line);


/******************* From compiler.c ***************************/

void 	handle_option      	(char *opt, Boolean all);
void 	clean_up_and_exit	(int val);
void 	intrupt			(int signo);
Boolean doing_export    	(void);
