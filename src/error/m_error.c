/**************************************************************
 * File:    error/m_error.c
 * Purpose: Error reporting for interpreter.
 * Author:  Karl Abrahamson
 ***************************************************************/

#ifdef COPYRIGHT
/************************************************************************
 * Copyright (c) 2000 Karl Abrahamson					*
 * All rights reserved.							*
 * See file COPYRIGHT.txt for details.					*
 ************************************************************************/
#endif

#include "../misc/misc.h"
#include "../error/m_error.h"

/****************************************************************
 * The following functions are needed for compatability with	*
 * some functions that normally work in the compiler, but	*
 * might try to report some errors in the interpreter.  	*
 * (They almost certainly never will.)				*
 ****************************************************************/

void semantic_error(int n, int line)
{
  die(n, line);
}

void semantic_error1(int n, CONST char *s, int line)
{
  die(n, s, line);
}

void semantic_error2(int n, CONST char *s, CONST char *s2, int line)
{
  die(n, s, s2, line);
}

void dup_intersect_err(CONST char *a_name, CONST char *b_name, 
		       CLASS_TABLE_CELL *c, 
		       LPAIR c_labs_unused, int h_val_unused, 
		       LPAIR h_labs_unused)
{
  die(143, a_name, b_name, c->name);
}

void syntax_error(int k, int line) 
{
  die(142, k, "(null)", "(null)", line);
}


