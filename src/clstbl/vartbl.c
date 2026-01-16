/****************************************************************
 * File:    clstbl/vartbl.c
 * Purpose: Table manager for variable names.
 * Author:  Karl Abrahamson
 ****************************************************************/

#ifdef COPYRIGHT
/************************************************************************
 * Copyright (c) 1997 Karl Abrahamson					*
 * All rights reserved.							*
 *									*
 * Redistribution and use in source and binary forms, with or without	*
 * modification, are permitted provided that the following conditions	*
 * are met:								*
 *									*
 * 1. Redistributions of source code must retain the above copyright	*
 *    notice, this list of conditions and the following disclaimer.	*
 *									*
 * 2. Redistributions in binary form must reproduce the above copyright	*
 *    notice in the documentation and/or other materials provided with 	*
 *    the distribution.							*
 *									*
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY		*
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE	*
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 	*
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE	*
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 	*
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 	*
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR 	*
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,*
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE *
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,    * 
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.			*
 *									*
 ************************************************************************/
#endif

/************************************************************************
 * This file maintains a table of the current bindings of symbolic 	*
 * variable names (such as `a) to actual variables (TYPE nodes).  That	*
 * table, called tf_var_table, is used to find the variable that is	*
 * referred to by a name in the program.				*
 ************************************************************************/

#include <string.h>
#include "../misc/misc.h"
#include "../utils/hash.h"
#include "../utils/strops.h"
#include "../tables/tables.h"
#include "../clstbl/classtbl.h"
#include "../clstbl/vartbl.h"
#include "../classes/classes.h"
#include "../error/error.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/****************************************************************
 * 			VARIABLES				*
 ****************************************************************/

/****************************************************************
 * 			tf_var_table				*
 ****************************************************************
 * Each declaration has its own space of species/family		*
 * variable names.  If a variable name is used twice, then each *
 * use refers to the same variable.  tf_var_table maps names to *
 * variables, for the current declaration.  			*
 *								*
 * The key is the name of the variable.  A variable called	*
 * "C`a" in the program has name "`a#C" in the table.  This is  *
 * the name mapping that is done by the lexer.  The associated  *
 * value is a TYPE* value, pointing to the variable itself.     *
 *								*
 * XREF: Only used here and to set to NULL in parser at start   *
 * of a declaration.						*
 ****************************************************************/

HASH2_TABLE* tf_var_table = NULL;


/****************************************************************
 *			    MAKE_VAR				*
 ****************************************************************
 * Return a variable of the given kind, with the given domain.  *
 ****************************************************************/

PRIVATE TYPE* make_var(TYPE_TAG_TYPE kind, char *domain)
{
  switch(kind) {
    case TYPE_VAR_T         : return type_var_t(domain);
    case FAM_VAR_T          : return fam_var_t(domain);
    case PRIMARY_TYPE_VAR_T : return primary_type_var_t(domain);
    case PRIMARY_FAM_VAR_T  : return primary_fam_var_t(domain);
    case WRAP_TYPE_VAR_T    : return wrap_type_var_t(domain);
    case WRAP_FAM_VAR_T     : return wrap_fam_var_t(domain);
    default		    : return NULL;
  }
}


/****************************************************************
 *			    TF_VAR_TM				*
 ****************************************************************
 * tf_var_tm attempts to return a type or family variable 	*
 * for identifier s.  If s is already in the variable table,	*
 * the entry there is returned.  Otherwise, a new entry		*
 * is created and returned.  					*
 *								*
 * Parameter kind is TYPE_VAR_T if s is a type variable, and	*
 * FAM_VAR_T if s is a family variable.				*
 * It will be changed to a wrapped or primary kind if		*
 * necessary, according to what id looks like.			*
 *								*
 * String id has the form `qual#name, where qual is the		*
 * qualifier and name is the class or community name.   	*
 * The qualifier indicates the kind of variable.  For example,  *
 * a variable that appears in a program as REAL`*~xyz will show *
 * up here as id `*~xyz#REAL.  The genus might be the empty 	*
 * string, indicating ANY.					*
 *								*
 * If string id has the form `qual#name where name begins with  *
 * an upper case letter, then the variable that is returned is  *
 * marked with its PREFER_SECONDARY flag set.			*
 ****************************************************************/

TYPE* tf_var_tm(char *id, TYPE_TAG_TYPE kind)
{
  TYPE *result;
  HASH_KEY u;
  HASH2_CELLPTR h;
  char *domain, *sharploc;
  Boolean norestrict;
  int after_kind_index, qual_name_index;

  /*-------------------------------------*
   * Get the genus or community name.    *
   * Note that an empty name is ANY.  We *
   * use a domain of NULL for ANY.       *
   *-------------------------------------*/

  sharploc = strchr(id, '#');
  domain   = (sharploc[1] == '\0') ? NULL : id_tb0(sharploc + 1);

  /*----------------------------------------------*
   * Modify the kind if necessary, and		  *
   * find the index (after_kind_index) just after *
   * the ` or `` or `*. 			  *
   *----------------------------------------------*/

  if(id[1] == '*') {
    after_kind_index = 2;
    kind += 2;             /* Force primary */
  }
  else if(id[1] == '`') {
    after_kind_index = 2;
    kind += 4;             /* Force wrapped */
  }
  else after_kind_index = 1;

  /*------------------------------------------------------------*
   * Check for a nonrestrictable id, and find where the actual	*
   * qualifier name begins.					*
   *------------------------------------------------------------*/

  if(id[after_kind_index] == '~') {
    qual_name_index = after_kind_index + 1;
    norestrict = TRUE;
  }
  else {
    qual_name_index = after_kind_index;
    norestrict = FALSE;
  }

  /*---------------------------------------------------------*
   * If the qualifier is ? then just return a new variable.  *
   *---------------------------------------------------------*/

  if(id[qual_name_index] == '?') {
    result = make_var(kind, domain);
    if(norestrict) result->norestrict = 1;
    result->anon = 1;
    return result;
  }

  /*-------------------*
   * Look up in table. *
   *-------------------*/

  u.str = id;
  h     = insert_loc_hash2(&tf_var_table, u, strhash(id), eq);

  /*-------------------------------------------------------*
   * If not found, make a new variable and put into table. *
   *-------------------------------------------------------*/

  if(h->key.num == 0) {
    bmp_type(result = make_var(kind, domain));
    if(norestrict) result->norestrict = 1;
    h->key.str = id;
    h->val.type = result;      /* Ref from result. */
  }

  /*---------------------------*
   * If found, get from table. *
   *---------------------------*/

  else {
    result = h->val.type;
  }

  /*---------------------------------------------*
   * Install PREFER_SECONDARY tag if called for. *
   *---------------------------------------------*/

  if('A' <= id[qual_name_index] && id[qual_name_index] <= 'Z') {
    result->PREFER_SECONDARY = 1;
  }

  return result;
}
