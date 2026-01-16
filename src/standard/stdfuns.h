/*********************************************************************
 * File:    standard/stdfuns.h
 * Author:  Karl Abrahamson
 * Purpose: Declare standard functions
 *********************************************************************/

extern EXPR *std_fun_descr;
extern EXPR *import_prim_fun_descr;

void std_funs		(void);
void do_prim_import	(char *which);
void ov_std_funs        (void);
void special_std_funs   (void);
