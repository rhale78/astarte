/**********************************************************************
 * File:    utils/m_strops.c
 * Purpose: Miscellaneous string functions for the interpreter
 * Author:  Karl Abrahamson
 **********************************************************************/

#ifdef COPYRIGHT
/************************************************************************
 * Copyright (c) 2000 Karl Abrahamson					*
 * All rights reserved.							*
 * See file COPYRIGHT.txt for details.					*
 ************************************************************************/
#endif

/**********************************************************************
 * This file contains assorted operations on strings for the          *
 * interpreter.				      			      *
 **********************************************************************/

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "../misc/misc.h"
#ifdef UNIX
# include <unistd.h>
#endif
#ifdef USE_MALLOC_H
#  include <malloc.h>
#endif
#include "../alloc/allocate.h"
#include "../utils/strops.h"
#include "../utils/lists.h"
#include "../utils/hash.h"
#include "../tables/tables.h"
#include "../clstbl/classtbl.h"
#include "../classes/classes.h"
#include "../error/error.h"
#include "../parser/tokens.h"
#include "../parser/parser.h"
#include "../intrprtr/intrprtr.h"
#ifdef TRANSLATOR
# include "../standard/stdids.h"
#endif
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/****************************************************************
 *			IS_PACKAGE_LOCAL_NAME			*
 ****************************************************************
 * Return TRUE if NAME is package-local.  That means that NAME  *
 * contains NAME_SEP_CHAR.					*
 ****************************************************************/

Boolean is_package_local_name(CONST char *name)
{
  return strchr(name, NAME_SEP_CHAR) != NULL;
}


/****************************************************************
 *			IS_WHITE				*
 ****************************************************************
 * Return TRUE if C is a blank, tab or newline.			*
 ****************************************************************/

Boolean is_white(char c)
{
  return isspace(c);
}


/****************************************************************
 *			PRINTABLE				*
 ****************************************************************
 * Return TRUE if C is a printable character.			*
 ****************************************************************/

Boolean printable(char c)
{
  return c >= 32 || isspace(c);
}


/****************************************************************
 *			ONLY_PRINTABLE				*
 ****************************************************************
 * Return TRUE if buffer S, of length N, contains only		*
 * printable characters.					*
 ****************************************************************/

Boolean only_printable(CONST char* s, LONG n)
{
  LONG i;
  for(i = 0; i < n; i++) {
    int c = s[i];
    if(c < 32 && !isspace(c)) return FALSE;
  }
  return TRUE;
}

