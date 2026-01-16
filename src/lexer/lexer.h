/****************************************************************
 * File:    lexer/lexer.h
 * Purpose: Definitions to support lexical level of parser
 * Author:  Karl Abrahamson
 ****************************************************************/

/****************************************************************
 *			YYLVAL					*
 ****************************************************************
 * yylval is declared in parser.c.  It is the attribute value   *
 * of a token.							*
 ****************************************************************/

extern YYSTYPE yylval;

/************************************************
 * Package names, file names and file handling. *
 ************************************************/

extern STR_LIST* imported_files;
extern STR_LIST* imported_package_names;
extern int  	 chars_in_this_line;
extern int       open_col;
extern FILE* 	 LISTING_FILE;
extern FILE*	 ERR_FILE;

/******************************
 * Lexeme and token handling. *
 ******************************/

struct unput_tokens_buffer_type {
  int token;
  YYSTYPE lval;
  short int kind;
  short int opc;
  char *name;
};

extern struct unput_tokens_buffer_type unput_tokens_buffer[];
extern int    		unput_tokens_top;
extern int  		last_tok;
extern int  		last_last_tok;
extern int		last_popped_tok;
extern int		last_popped_tok_line;
extern YYSTYPE 		last_yylval;
extern YYSTYPE 		last_last_yylval;
extern char HUGEPTR 	string_const;
extern LONG 		string_const_len;
extern ROLE* 		abbrev_tok_role;

/***********************
 * Contexts and modes. *
 ***********************/

extern int 	import_level;
extern int 	gen_listing;
extern char  	import_start;
extern Boolean  start_in_comment_mode;
extern Boolean  recent_begin_from_semicolon;
extern Boolean  opContext;
extern Boolean 	force_no_op;
extern Boolean 	long_force_no_op;
extern Boolean 	panic_mode;
extern Boolean 	reading_string;
extern Boolean 	seen_package_end;    
extern Boolean 	no_line_num;
extern Boolean 	no_print_this_token;
extern Boolean 	no_load_standard;
extern Boolean 	bring_context;
extern Boolean 	in_operator_dcl;
extern Boolean  verbose_mode;

/****************************************************************
 *		      EXPORTS FROM LEXER.C			*
 ****************************************************************/

/**************************************
 * YY_BUF_SIZE is the size of yytext. *
 **************************************/

#define YY_BUF_SIZE 16384

int	yylex			(void);
void    yy_switch_to_buffer 	(YY_BUFFER_STATE new_buffer);
void    yy_load_buffer_state	(void);
YY_BUFFER_STATE yy_create_buffer(FILE *file, int size);
void    yy_delete_buffer	(YY_BUFFER_STATE b);
void    yy_init_buffer		(YY_BUFFER_STATE b, FILE *file);
int     input_yy		(void);
void    unput_yy		(int c);
void    read_string_l		(Boolean longstring);


/****************************************************************
 *              EXPORTS FROM PREFLEX.C                          *
 ****************************************************************/

Boolean init_lexical_l	(char *s);
void 	import_l        (int n);
void 	end_import_l	(void);
void 	body_tok_l	(void);
Boolean push_input_file	(char *s);
void  	pop_input_file	(Boolean archive);
void 	push_back_token	(int token, YYSTYPE lval, int kind, char *name);
void	unput_str	(char *s);
int     paren_ahead	(void);
Boolean should_list	(void);
void 	echo_tab_l 	(void);
void 	echo_char_l     (int c);
void    echo_char_line_l(int c);
Boolean	echo_token_l    (char *lexeme);
void 	new_line_l	(void);
void 	terminate_line_l(void);
int  	yylexx		(void);


/****************************************************************
 *		EXPORTS FROM LEXIDS.C    			*
 ****************************************************************/

int   normal_id_l	(char *name);
int   symbol_l		(void);
int   symbolic_id_l	(int sym);
int   class_id_l	(char *text, int textlen, Boolean qual);
char* get_my_id_l	(char *text);


/****************************************************************
 *		EXPORTS FROM LEXERR.C				*
 ****************************************************************/

void 	shadow_top		(LIST *st, int *t, char **name, Boolean *open);
void 	shadow_top_nums    	(LIST *st, int *line, int *col);
void 	push_shadow		(int n);
void 	push_shadow_str    	(char *s);
void	pop_shadow		(void);
void 	pop_proc_id        	(void);
int  	end_word_l		(int word);
Boolean end_match_l		(int top, char *proc, int word);
void    indentation_warn	(Boolean k);
void 	check_indentation_l	(void);
int  	check_for_paren_l  	(int tok);
void 	require_uc		(void);
void 	require_lc		(void);
void 	check_case		(struct lstr ls, ATTR_TYPE att);
int 	line_tok_l		(void);


/****************************************************************
 *		EXPORTS FROM LEXSUP.C				*
 ****************************************************************/

extern struct token_map token_map[];

void 	init_reserved_words_l	(void);
char* 	rw_name			(int tok);
char*   lparen_from_rparen	(int tok);
char* 	token_name		(int tok, YYSTYPE lval);
int  	token_kind		(int tok);
int  	token_attr		(int tok);
Boolean	is_begin_tok		(int tok);
Boolean is_reserved_word_str	(CONST char *word);
Boolean is_reserved_word_tok	(int tok);
void 	begin_word_l		(int word);
void 	rest_context_l 		(int kind);
void 	one_context_l		(int kind);
void 	zero_context_l		(int kind);
int  	do_semicolon_l		(void);
void    fix_yytext_for_end_my_l (void);
int  	handle_endword_l   	(void);
int  	handle_string_l    	(void);
