/**********************************************************************
 * File:    misc/m_stubs.c
 * Purpose: Stubs for the interpreter.  These are functions that are 
 *          needed by the translator, but should do nothing when 
 *          run in the interpreter, or should not be called.
 * Author:  Karl Abrahamson
 **********************************************************************/

#include "../misc/misc.h"
#include "../exprs/expr.h"
#include "../clstbl/abbrev.h"

EXPR_TAG_TYPE ekindf(EXPR *e_unused) {return 0;}

void abbrev_tm(char *id_unused, int tok_unused, TYPE *t_unused, 
	       ROLE *r_unused, Boolean global_unused){}



