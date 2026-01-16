/**************************************************************
 * File:    exprs/prtexpr.h
 * Purpose: Print expressions
 * Author:  Karl Abrahamson
 **************************************************************/


#define SPE_SUFFIX_BUFFER_SIZE 20
#define SPE_PREFIX_BUFFER_SIZE 60
#define SHORT_PRINT_EXPR_PREFIX_LENGTH_DEFAULT 20

void short_print_expr  	(FILE *f, EXPR *e);
void short_print_two_exprs(FILE *f, char *s1, EXPR *e1, char *s2, EXPR *e2);
Boolean try_short_print_expr(FILE *f, EXPR *e, int max_chars);
void shrt_pr_expr	(FILE *f, EXPR *e);

void pretty_print_expr(FILE *f, EXPR *e, int n, int context);
void pretty_print_pat_rule(FILE *f, EXPR *formals, EXPR *translation, 
		   	   int n, int context);

#define AFTER_PATTERN_MATCH 		1
#define SUPPRESS_INITIAL_INDENT 	2
#define SUPPRESS_FINAL_NEWLINE		4

