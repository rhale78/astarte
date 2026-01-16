/**********************************************************************
 * File:    utils/t_strops.c
 * Purpose: Miscellaneous string functions for translator
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
 * compiler.  These operations are not used by the interpreter.	      *
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
# include "../standard/stdids.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/****************************************************************
 *			PUBLIC VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			temp_var_num				*
 ****************************************************************
 * The next number to use for a temporary variable name.  This  *
 * variable is used here and set back to 0 each time a 		*
 * declaration is started (in start_dcl).			*
 ****************************************************************/

int temp_var_num = 0;

/****************************************************************
 *			SKIP_WHITE				*
 ****************************************************************
 * Return a pointer to the first nonwhite character in S.	*
 ****************************************************************/

char* skip_white(char *s)
{
  char* p = s;
  int c = *s;
  
  while(isspace(c)) c = *(++p);
  return p;
}


/****************************************************************
 *			REMOVE_DOTS				*
 ****************************************************************
 * Return the result of removing any leading HIDE_CHARs from	*
 * string NAME.  The result string is identical to NAME if      *
 * NAME does not start with HIDE_CHAR, and is in the id table   *
 * if NAME begins with HIDE_CHAR.				*
 ****************************************************************/

CONST char* remove_dots(CONST char *name)
{
  if(name[0] != HIDE_CHAR) return name;
  else {
    CONST char *p = name;
    while(*p == HIDE_CHAR) p++;
    return id_tb0(p);
  }
}

/****************************************************************
 *				MY_NAME				*
 ****************************************************************
 * Return the result of adding my- to the front of S.		*
 ****************************************************************/

char* my_name(char *s)
{
  return attach_prefix_to_id("my-", s, 0);
}


/****************************************************************
 *			NEW_TEMP_VAR_NAME			*
 ****************************************************************
 * Return a new temporary variable name.  Each call within	*
 * a given declaration returns a different name.		*
 ****************************************************************/

CONST char* new_temp_var_name(void)
{
  char name[10];
  sprintf(name, HIDE_CHAR_STR "x%d", temp_var_num++);
  return id_tb0(name);
}


/****************************************************************
 *			NEW_NAME				*
 ****************************************************************
 * Translates S to its internal name.  If S is something that is*
 * globally known, then there is no change to S.  If S is 	*
 * hidden in the body, then place p; (or p followed by whatever *
 * NAME_SEP_CHAR is) in front of S, where p is the package 	*
 * name. The case of the first letter of the new    		*
 * name is the same as the case of S.				*
 *								*
 * When the package name is added to an identifier that starts  *
 * with a sequence of dots, then the dots are moved to the 	*
 * front of the modified identifier.  For example, ";xx"	*
 * becomes ";p.x".						*
 *								*
 * If S already has a package name attached to it,		*
 * no change is made to S.					*
 *								*
 * The result is in the string  table.				*
 *								*
 * Parameter ENT indicates whether the id will name an entity   *
 * or a type.  It is used to determine where to look to		*
 * determine whether S is globally known or not.  Use		*
 *								*
 *   ent = 0    to check for an entity				*
 *   ent = 1    to check for a type ( or genus, etc.)		*
 *   ent = 2    to suppress check				*
 ****************************************************************/

CONST char* new_name(CONST char *s, Boolean ent)
{
  char name[MAX_ID_SIZE+1], *package_name, *ss, s_first, name0;
  LONG hash;
  Boolean s_is_upper_case;
  int dot_count;

  if(s == NULL) return stat_id_tb("none");

  hash         = strhash(s);
  ss           = id_tb10(s, hash);
  package_name = current_package_name;

  /*-----------------------------------------------------------------*
   * If this name is hidden, or if this is not the body, then return *
   * s unchanged. 						     *
   *-----------------------------------------------------------------*/

  if(main_context != BODY_CX || is_hidden_id(s)) return ss;

  /*-------------------------------------*
   * If s is global, return s unchanged. *
   *-------------------------------------*/

  if(ent== 1) {
    if(is_visible_global_tm(ss, package_name, FALSE)) {
      return ss;
    }
  }
  else if(ent == 0) {
    if(get_ctc_with_hash_tm(ss, hash) != NULL) {
      return ss;
    }
  }

  /*---------------------------------------------------------------*
   * We need to modify s.  First count any HIDE_CHARs at the front *
   * of s.  They will be moved to the front of the modified	   *
   * identifier.						   *
   *---------------------------------------------------------------*/

  dot_count = 0;
  while(ss[dot_count] == HIDE_CHAR) dot_count++;
  ss += dot_count;

  /*--------------------------------------------------------------------*
   * Now determine whether the first letter of s (after the HIDE_CHARs)	*
   * is upper case.							*
   *--------------------------------------------------------------------*/

  s_first = ss[0];
  s_is_upper_case = ('A' <= s_first) && (s_first <= 'Z');
  name0 = package_name[0];

  /*---------------------------*
   * Adjust the case of name0. *
   *---------------------------*/

  if(s_is_upper_case) {
    if('a' <= name0 && name0 <= 'z') name0 += 'A' - 'a';
  }
  else {
    if('A' <= name0 && name0 <= 'Z') name0 += 'a' - 'A';
  }

  /*------------------------------------------*
   * If the new id is too long, truncate it.  *
   *------------------------------------------*/

  {int package_name_len = strlen(package_name);
   int i;
   char adjusted_s = 0;
   if(package_name_len + strlen(s) + 1 > MAX_ID_SIZE) {
     syntax_error(ID_TOO_LONG_ERR, 0);
     adjusted_s = s[MAX_ID_SIZE - package_name_len];
     s[MAX_ID_SIZE - package_name_len] = 0;
   }

   /*----------------------------------------------------------------------*
    * Copy the HIDE_CHARs, package name, NAME_SEP_CHAR, and the id name.   *
    *----------------------------------------------------------------------*/

   for(i = 0; i < dot_count; i++) name[i] = HIDE_CHAR;
   sprintf(name + dot_count, "%c%s%c%s", 
	   name0, package_name + 1, NAME_SEP_CHAR, s);
   if(adjusted_s) s[MAX_ID_SIZE - package_name_len] = adjusted_s;
  }

  return id_tb0(name);
}


/****************************************************************
 *			NEW_CONCAT_ID				*
 ****************************************************************
 * new_concat_id(A,B,E) is the same as				*
 * new_name(concat_id(A,B), E).					*
 ****************************************************************/

CONST char* new_concat_id(CONST char *a, CONST char *b, Boolean ent)
{
  return new_name(concat_id(a,b), ent);
}


/****************************************************************
 *			ATTACH_PREFIX_TO_ID			*
 ****************************************************************
 * Return PREF ++ ID, but place prefix after the package prefix	*
 * on ID if there is one.  For example,				*
 *								*
 *  attach_prefix_to_id("un", "pack.f", 0) = "pack.unf".	*
 *								*
 * The case of the first character of pref is adjusted as	*
 * follows, according to the value of parameter CASE_SELECT.    *
 *								*
 *   CASE_SELECT	case of first character of new id	*
 *   -----------        ----------------------------------	*
 *      0		the same as the case of id.		*
 *								*
 *      1               lower case				*
 *								*
 *	2		upper case				*
 ****************************************************************/

#define MAX_PREFIX_LENGTH 30    /* Maximum length of a prefix of an id  */


CONST char* 
attach_prefix_to_id(CONST char *pref, CONST char *id, int case_select)
{
  CONST char *result;
  char *nameseploc;
  char new_pref[MAX_PREFIX_LENGTH + 1];
  int pref_mod;

  CONST char* basic_name = name_tail(id);
  char  basic_name_first = basic_name[0];
  char  pref_first       = pref[0];
  strncpy(new_pref, pref, MAX_PREFIX_LENGTH);
  new_pref[MAX_PREFIX_LENGTH] = 0;

  /*-----------------------------------------------------*
   * Decide by how much to adjust the first character of *
   * the prefix.					 *
   *-----------------------------------------------------*/

  if(islower(pref_first) &&
     ((case_select == 0 && isupper(basic_name_first)) ||
      case_select == 2)) {
    pref_mod = 'A' - 'a';
  }
  else if(isupper(pref_first) &&
	  ((case_select == 0 && islower(basic_name_first)) ||
	   case_select == 1)) {
    pref_mod = 'a' - 'A';
  }
  else pref_mod = 0;
  new_pref[0] = (char) (new_pref[0] + pref_mod);

  /*---------------------------------------------*
   * Concatenate the prefix with the basic name. *
   *---------------------------------------------*/

  result = concat_id(new_pref, basic_name);

  /*--------------------------------------------*
   * Attach a package name, if one was present. *
   *--------------------------------------------*/

  nameseploc = strchr(id, NAME_SEP_CHAR);
  if(nameseploc != NULL) {
    char hold = nameseploc[1];
    nameseploc[1] = 0;
    result        = concat_id(id, result);
    nameseploc[1] = hold;
  }
 
  return result;  
}


/****************************************************************
 *			IS_HIDDEN_ID				*
 ****************************************************************
 * Return true if NAME is a hidden id (i.e. if it has a		*
 * NAME_SEP_CHAR).						*
 ****************************************************************/

Boolean is_hidden_id(CONST char *name)
{
  return name != NULL && strchr(name, NAME_SEP_CHAR) != NULL;
}


/****************************************************************
 *			IS_PROC_ID				*
 ****************************************************************
 * Return true if ID is a procedure id.				*
 ****************************************************************/

Boolean is_proc_id(CONST char *id)
{
  return ('A' <= *id && *id <= 'Z');
}


/****************************************************************
 *			LAST_CHAR				*
 ****************************************************************
 * Return the last character in string S.			*
 ****************************************************************/

int last_char(CONST char *s)
{
  if(s == NULL || *s == '\0') return 0;
  return s[strlen(s)-1];
}


/****************************************************************
 *			LOWER_CASE				*
 ****************************************************************
 * Return a lower case equivalent of ID.  That is, make the	*
 * first character lower case.  If the ID contains a package	*
 * name, then both the package name and the id that comes	*
 * after the package name are made lower case.  For example,	*
 *								*
 * lower_case("Pack;IdTest") = "pack;idTest"			*
 ****************************************************************/

CONST char* lower_case(CONST char *id)
{
  char temp[MAX_ID_SIZE + 1], *p, *q;

  if('A' <= *id && *id <= 'Z') {
    temp[0] = id[0] + ('a' - 'A');
    p = temp + 1;
    q = id + 1;
    while(*q != '\0' && *q != NAME_SEP_CHAR) {*(p++) = *(q++);}
    if(*q == NAME_SEP_CHAR) {
      *(p++) = *(q++);
      if('A' <= *q && *q <= 'Z') *(p++) = (*q++) + 'a' - 'A';
      else *(p++) = *(q++);
      while(*q != '\0') {*(p++) = *(q++);}
    }
    *p = '\0';
    return id_tb0(temp);
  }
  else return id;
}


/****************************************************************
 *			CONSTRUCTOR_NAME			*
 ****************************************************************
 * Return the name of the default constructor associated with	*
 * species NAME.						*
 ****************************************************************/

CONST char* constructor_name(CONST char *name)
{
  /*------------------------------------------------------*
   * To make a constructor name, just make the identifier *
   * lower case.					  *
   *------------------------------------------------------*/

  return lower_case(name);
}


/****************************************************************
 *			TESTER_NAME				*
 ****************************************************************
 * Return the name of the tester associated with constructor    *
 * NAME.							*
 ****************************************************************/

CONST char* tester_name(CONST char *name)
{
  return concat_id(name, "?");
}


/****************************************************************
 *			FORGETFUL_TESTER_NAME			*
 ****************************************************************
 * Return the name of the forgetfull tester associated with	*
 * constructor NAME.  This is just name followed by "??".	*
 ****************************************************************/

CONST char* forgetful_tester_name(CONST char *name)
{
  return concat_id(name, "??");
}


/****************************************************************
 *			DESTRUCTOR_NAME				*
 ****************************************************************
 * Return the name of the destructor associated with		*
 * constructor ID.						*
 ****************************************************************/

CONST char* destructor_name(CONST char *id)
{
  char* tname = name_tail(id);
  if('a' <= tname[0] && tname[0] <= 'z') {
    return attach_prefix_to_id("un", id, 1);
  }
  else if(strcmp(id, std_id[CONS_SYM]) == 0) {
    return std_id[CONSINV_SYM];
  }
  else {
    return concat_id(id, "/-");
  }
}


/****************************************************************
 *			MAKE_ROLE_MOD_ID			*
 *			MAKE_ROLE_SEL_ID			*
 ****************************************************************
 * Return the identifier ID forced to a role modifier 		*
 * (make_role_mod_id) or role selector (make_role_sel_id).  For	*
 * example, if ID is "golf", then the identifier returned by	*
 * make_role_mod_id is ";golf'!!'".  				*
 *								*
 * The returned string is in the identifier table.		*
 ****************************************************************/

PRIVATE CONST char* make_role_id(CONST char *id, CONST char *suff)
{
  int id_len = strlen(id);
  char* temp;
# if (defined SMALL_STACK) || (!defined USE_ALLOCA)
    temp = (char*) BAREMALLOC(id_len + 6);
# else
    temp = (char*) alloca(id_len + 6);
# endif

  temp[0] = HIDE_CHAR;
  strcpy(temp + 1, id);
  strcpy(temp + id_len + 1, suff);

# if (defined SMALL_STACK) || (!defined USE_ALLOCA)
    {register char* result = id_tb0(temp);
     FREE(temp);
     return result;
    }
# else
    return id_tb0(temp);
# endif
  
}

/*--------------------------------------*/

CONST char* make_role_mod_id(CONST char *id)
{
  return make_role_id(id, "'!!'");
}

/*--------------------------------------*/

CONST char* make_role_sel_id(CONST char *id)
{
  return make_role_id(id, "'??'");
}


/****************************************************************
 *			CHAR_VAL				*
 ****************************************************************
 * S is a string that represents a character code.  It is 	*
 * either a simple character, or is of the form \n or \t or \\	*
 * or \{k} where k is a decimal constant.  It can also be \c    *
 * for some other character c, which is taken to mean c.        *
 *								*
 * Return the code that the string S represents.		*
 ****************************************************************/

int char_val(CONST char *s, int line)
{
  char c;
  if(s[0] != '\\') return s[0];
  c = s[1];
  if(c == 'n') return '\n';
  if(c == 't') return '\t';
  if(c == '{') {
    int i = 2, k = 0;
    c = s[i];
    while(i < 6 && c != '}') {
      if(c < '0' || c > '9') syntax_error(NUMBER_CHAR_TOO_LARGE_ERR, line);
      k = 10*k + (c - '0');
      c = s[++i];
    }
    if(k > 255) {
      syntax_error(NUMBER_CHAR_TOO_LARGE_ERR, line);
      k = 0;
    }
    return k;
  }
    
  return c;
}


/****************************************************************
 *			RESTORE_CC_VAR_NAME    			*
 ****************************************************************
 * The lexer replaces "gen`xx" by "`xx#gen".			*
 *								*
 * Restore the name of type or family variable ORIG_NAME that   *
 * has been mangled by the lexer, and return the resulting	*
 * name.							*
 ****************************************************************/

CONST char* restore_cc_var_name(CONST char *orig_name)
{
  char *p;
  CONST char *result;
  char temp[MAX_ID_SIZE + 1];

  strcpy(temp, orig_name);

  p = strchr(temp, '#');
  if(p == NULL) return orig_name;
  *p = 0;
  result = concat_id(p+1, temp);
  return result;
}


/****************************************************************
 *			REAL_CONSTS_EQUAL      			*
 ****************************************************************
 * Return true just when strings S1 and S2 represent the same	*
 * real number.  						*
 *								*
 * Function canonical_real takes a string S representing a real *
 * number, and produces a string SOUT and an exponent EOUT such *
 * that SOUT*10^EOUT is a canonical representation of real	*
 * number S.							*
 *								*
 * Sometimes, canonical_real cannot work.  (It does not deal    *
 * with really large exponents.)  It sets OK to true if the     *
 * conversion to canonical form is successful.  		*
 *								*
 * SOUT_PHYS is space for canonical_real to work with.  On 	*
 * success, *SOUT is a pointer into array SOUT_PHYS.		*
 *								*
 * LONG_DECIMAL_DIGITS is a number k such that 10^k < LONG_MAX. *
 ****************************************************************/

#if (LONG_BITS >= 64)
# define LONG_DECIMAL_DIGITS 18 
#else 
# define LONG_DECIMAL_DIGITS 9
#endif

PRIVATE void 
canonical_real(CONST char *s, char *sout_phys, char **sout, 
	       LONG *eout, Boolean *ok)
{
  CONST char *p;
  char *q;
  Boolean seen_decimal_point = FALSE;
  LONG exp = 0, exp_from_s;

  q = sout_phys;

  /*------------------------------------------------------------------*
   * Scan over the number part, up to the E or the end of the string. *
   * Copy the digits into the sout_phys array, leaving out the        *
   * decimal point, but set exp to the negative of the number of      *
   * digits that are to the right of the decimal point.		      *
   *------------------------------------------------------------------*/

  for(p = s; *p != '\0' && *p != 'e' && *p != 'E'; p++) {
    if(*p == '.') seen_decimal_point = TRUE;
    else {
      *(q++) = *p;
      if(seen_decimal_point) exp--;
    }
  }
  *q = '\0';

  /*----------------------------------------------------*
   * Get the exponent, if there is one.  Adjust exp 	*
   * according to the given exponent.  	If the exponent	*
   * is too large, give up.				*
   *----------------------------------------------------*/

  if(*p != '\0') {
    if(strlen(p) <= LONG_DECIMAL_DIGITS) {
      sscanf(p+1, "%ld", &exp_from_s);
      exp += exp_from_s;
    }
    else {
      *ok = FALSE;
      return;
    }
  }

  /*--------------------------------------------*
   * Remove trailing zeros from the out string. *
   *--------------------------------------------*/

  for(q = sout_phys + strlen(sout_phys) - 1; 
      q != sout_phys && *q == '0';
      q--) {
    exp++;
    *q = '\0';
  }

  /*-------------------------------------------*
   * Remove leading zeros from the out string. *
   *-------------------------------------------*/

  for(p = sout_phys; *p == '0' && p[1] != '\0'; p++);
  *sout = p;

  /*---------------------------------------------------------*
   * If the number is 0, then set the exponent to 0 as well. *
   *---------------------------------------------------------*/

  if(*p == '0') exp = 0;

  /*------------------------*
   * Set the return values. *
   *------------------------*/

  *eout = exp;
  *ok = TRUE;
}

/*------------------------------------------------------------*/

Boolean real_consts_equal(CONST char *s1, CONST char *s2)
{
# ifndef SMALL_STACK
    char s1_space[MAX_REAL_CONST_LENGTH+1], s2_space[MAX_REAL_CONST_LENGTH+1];
# else
    char* s1_space = (char*) BAREMALLOC(MAX_REAL_CONST_LENGTH + 1);
    char* s2_space = (char*) BAREMALLOC(MAX_REAL_CONST_LENGTH + 1);
# endif
  char *s1canon, *s2canon;
  LONG s1exp, s2exp;
  Boolean ok1, ok2, result;

  canonical_real(s1, s1_space, &s1canon, &s1exp, &ok1);
  canonical_real(s2, s2_space, &s2canon, &s2exp, &ok2);
  if(!ok1 || !ok2) result = FALSE;
  else if(s1exp != s2exp || strcmp(s1canon, s1canon) != 0) result = FALSE;
  else result = TRUE;

# ifdef SMALL_STACK
    FREE(s1_space);
    FREE(s2_space);
# endif
  return result;
}


