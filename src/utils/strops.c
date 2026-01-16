/**********************************************************************
 * File:    utils/strops.c
 * Purpose: Miscellaneous string functions
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
 * This file contains assorted operations on strings, both for the    *
 * compiler and the interpreter.				      *
 **********************************************************************/

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "../misc/misc.h"
#ifdef UNIX
# include <unistd.h>
#endif
#ifdef SMALL_STACK
#  ifdef USE_MALLOC_H
#    include <malloc.h>
#  endif
#endif
#include "../alloc/allocate.h"
#include "../utils/strops.h"
#include "../utils/lists.h"
#include "../utils/hash.h"
#include "../tables/tables.h"
#include "../clstbl/classtbl.h"
#include "../classes/classes.h"
#include "../parser/tokens.h"
#include "../parser/parser.h"
#include "../intrprtr/intrprtr.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif
PRIVATE char lc(char a);


/****************************************************************
 *			NONNULL					*
 ****************************************************************
 * Return S, if S is not null.  If S is null, return "(null)".  *
 ****************************************************************/

CONST char* nonnull(CONST char* s)
{
  return s == NULL ? "(null)" : s;
}


/********************************************************
 *			FGETS1				*
 ********************************************************
 * Reads a null terminated string from file F, and	*
 * places it in S, followed by a null character. A 	*
 * maximum of N+1 characters will be read, including    *
 * the null character. Array S should have at least N+1 *
 * bytes.						*
 ********************************************************/

void fgets1(char *s, int n, FILE *f)
{
  register char *p;
  register int i, ch;

  p = s;
  i = 0;
  while(i <= n && (ch = getc(f)) != 0 && ch != EOF) {
    *(p++) = tochar(ch);
    i++;
  }
  if(i > n) p--;
  *p = '\0';
}


/****************************************************************
 *			COPY_DOTS				*
 ****************************************************************
 * Return the result of copying a prefix of S that consists     *
 * entirely of HIDE_CHARs to the front of R, but after removing *
 * any occurrences of HIDE_CHAR from the front of R.		*
 *								*
 * For example, if R is "kangaroo" and S is ";;camel" (and	*
 * HIDE_CHAR is ';'), then the returned string is ";;kangaroo". *
 *								*
 * The result is in the identifier table, except that, if there *
 * are no HIDE_CHARs at the front of S, then R is returned	*
 * unchanged.							*
 ****************************************************************/

PRIVATE CONST char* copy_dots(CONST char *s, CONST char *r)
{
  int k;
  char result[MAX_ID_SIZE + 1];

  if(s[0] != HIDE_CHAR) return r;

  k = 0;
  while(s[k] == HIDE_CHAR) result[k++] = HIDE_CHAR;
  while(*r == HIDE_CHAR) r++;
  strcpy(result + k, r);
  return id_tb0(result);
}


/****************************************************************
 *			NAME_TAIL				*
 ****************************************************************
 * Return the tail of S, defined to be S if S does not contain  *
 * NAME_SEP_CHAR, or the part of S after the first colon.  For 	*
 * example, name_tail("abc.def") = "def", and name_tail("abcd") *
 * = "abcd".  							*
 *								*
 * A special case is an identifier that starts with HIDE_CHAR.	*
 * Any prefix of HIDE_CHARs is copied to the front of the	*
 * returned string, after removing any occurrence of HIDE_CHAR  *
 * that were already there.  So if S is ";;a.x" then the	*
 * returned string is ";;x", and if S is ";a.;x", the returned  *
 * string is ";x".						*
 *								*
 * When S does not contain NAME_SEP_CHAR, the returned pointer 	*
 * is exactly S.  If S does contain NAME_SEP_CHAR, the returned *
 * pointer is in the identifier table.				*
 *								*
 * If s is NULL, then NULL is returned.				*
 ****************************************************************/

CONST char* name_tail(CONST char *s)
{
  CONST char *p;

  if(s == NULL) return NULL;

  for(p = s; *p != '\0' && *p != NAME_SEP_CHAR; p++) /* Null body */;

  if(*p == NAME_SEP_CHAR) {
    if(s[0] == HIDE_CHAR) return copy_dots(s, p+1);
    else return id_tb0(p+1);
  }
  else return s;
}


/****************************************************************
 *			DISPLAY_NAME				*
 ****************************************************************
 * Return name S as it should appear when shown.  The result	*
 * will be in the identifier table as long as S is in the	*
 * identifier table, and is not NULL.				*
 ****************************************************************/

CONST char* display_name(CONST char *s)
{
  CONST char* result = name_tail(s);
  if(result == NULL) return "(null)";
  else if(result[0] != HIDE_CHAR) return result;
  else if(result[1] != HIDE_CHAR) return concat_id("tmp:", result+1);
  else return concat_id("hidden:", result+2);
}


/****************************************************************
 *			QUICK_DISPLAY_NAME			*
 ****************************************************************
 * Return name S as it should appear when shown.  The result    *
 * string points into buffer S, so this function should 	*
 * only be used for getting temporary strings.			*
 ****************************************************************/

CONST char* quick_display_name(CONST char *s)
{
  CONST char *p;
  if(s == NULL) return "(null)";
  if(s[0] == HIDE_CHAR) return display_name(s);
  for(p = s; *p != '\0' && *p != NAME_SEP_CHAR; p++) /* Null body */;
  if(*p == NAME_SEP_CHAR) return p+1;
  else return s;
}


/********************************************************
 *			SUFFIX_IGNORE_CASE		*
 ********************************************************
 * Returns true if T is a suffix of S, with case	*
 * ignored in comparisons.				*
 ********************************************************/

Boolean suffix_ignore_case(CONST char *t, CONST char *s)
{
  int n,m;

  n = strlen(s);
  m = strlen(t);
  return n >= m && lcequal(s + n - m, t);
}


/****************************************************************
 *		       SUFFIX					*
 ****************************************************************
 * Return true if X is a suffix of Y.				*
 ****************************************************************/

Boolean suffix(CONST char *x, CONST char *y)
{
  int n,m;

  n = strlen(y);
  m = strlen(x);
  return n >= m && strcmp(y + n - m, x) == 0;
}


/****************************************************************
 *			PREFIX_IGNORE_CASE			*
 ****************************************************************
 * Return true if X is a prefix of Y, ignoring the case of	*
 * letters.							*
 ****************************************************************/

Boolean prefix_ignore_case(CONST char *x, CONST char *y)
{
  CONST char *p, *q;

  if(x == NULL || y == NULL) return FALSE;
  for(p = x, q = y; 
      *p != '\0' && *q != '\0' && lc(*p) == lc(*q);
      p++, q++) {
   /* Null body */
  }
  return *p == '\0';
}


/****************************************************************
 *			PREFIX					*
 ****************************************************************
 * Return true if X is a prefix of Y.				*
 ****************************************************************/

Boolean prefix(CONST char *x, CONST char *y)
{
  CONST char *p, *q;

  if(x == NULL || y == NULL) return FALSE;
  for(p = x, q = y; 
      *p != '\0' && *q != '\0' && *p == *q;
      p++, q++) {
   /* Null body */
  }
  return *p == '\0';
}


/****************************************************************
 *			CONCAT_STRING				*
 *			CONCAT_ID				*
 ****************************************************************
 * All of the following functions return the concatenation of 	*
 * strings A and B.						*
 *								*
 * concat_id returns a string that is in the identifier table.	*
 *								*
 * concat_string returns a string that is in the string		*
 * constant table.						*
 *								*
 * concat is shared code.  It takes an extra parameter STRTB,	*
 * through which the result string (in a temporary array) is	*
 * passed.  Function STRTB is responsible for making a 		*
 * permanent copy of the string.				*
 ****************************************************************/

PRIVATE CONST char* concat(CONST char *a, char *b, char* (*strtb)(char*))
{
  int la = strlen(a);
  int lb = strlen(b);
  char* temp;
# if (defined SMALL_STACK) || (!defined USE_ALLOCA)
    temp = (char*) BAREMALLOC(la + lb + 1);
# else
    temp = (char*) alloca(la + lb + 1);
# endif

  strcpy(temp, a);
  strcpy(temp+la, b);
# if (defined SMALL_STACK) || (!defined USE_ALLOCA)
    {register char* result = strtb(temp);
     FREE(temp);
     return result;
    }
# else
    return strtb(temp);
# endif
}

/*----------------------------------------------------------*/

CONST char* concat_string(CONST char *a, CONST char *b)
{
  return concat(a, b, string_const_tb0);
}

/*----------------------------------------------------------*/

CONST char* concat_id(CONST char *a, CONST char *b)
{
  return concat(a, b, id_tb0);
}


/****************************************************************
 *			COLONSEP_TO_LIST       			*
 ****************************************************************
 * Return a list of the strings that occur in S, separated by	*
 * colons or semicolons.  For example, 				*
 * colonsep_to_list("abc:def:ghi") = ["abc", "def", "ghi"].	*
 *								*
 * Each member of the result list is in the string constant	*
 * table.							*
 ****************************************************************/

STR_LIST* colonsep_to_list(CONST char *s)
{
  char *p;
  STR_LIST *l;

  int   len  = strlen(s);
  char* scpy = (char*) BAREMALLOC(len + 1);

  strcpy(scpy, s);
  p = scpy + len - 1;
  l = NIL;
  while(p >= scpy) {
    while(p >= scpy && *p != ':' && *p != ';') p--;
    l = str_cons(string_const_tb0(p+1), l);
    if(p >= scpy) {*p = '\0'; p--;}
  }

  FREE(scpy);
  return l;
} 


/****************************************************************
 *			LCEQUAL					*
 ****************************************************************
 * Return true just when A and B are the same when all letters  *
 * are forced to lower case.					*
 ****************************************************************/

PRIVATE char lc(char a)
{
  if('A' <= a && a <= 'Z') return a + ('a' - 'A');
  else return a;
}

/*-----------------------------------------------------*/

Boolean lcequal(CONST char *a, CONST char *b)
{
  char *p, *q;

  for(p = a, q = b; *p != '\0' && *q != '\0'; p++, q++) {
    if(lc(*p) != lc(*q)) return FALSE;
  }
  if(*p == *q) return TRUE;
  return FALSE;
}
