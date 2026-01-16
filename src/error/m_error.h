/************************************************************
 * File:    error/m_error.h
 * Purpose: Error handling in interpreter
 * Author:  Karl Abrahamson
 ************************************************************/

#define CLOSURE_ERR	    27
#define BAD_LINK_LABEL_ERR  28
#define CYCLE_ERR           174

void syntax_error	(int k, int line);
void semantic_error 	(int n, int line);
void semantic_error1	(int n, char *s, int line);
void semantic_error2	(int n, char *s, char *s2, int line);
void dup_intersect_err	(char *a_name, char *b_name, CLASS_TABLE_CELL *c, 
		         LPAIR c_labs, int h_val, LPAIR h_labs);
