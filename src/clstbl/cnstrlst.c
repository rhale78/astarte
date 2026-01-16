/****************************************************************
 * File:    clstbl/cnstrlst.c
 * Purpose: Functions for handling constructor lists for species.
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
 * This file contains functions that handle constructor lists for the   *
 * class is table.							*
 *									*
 * The constructor list of a species or family is a list of     	*
 * the names of constructors for the family.  For example, for  	*
 * family List, the constructors are nil and ::.  If a species T is     *
 * defined by								*
 *									*
 *    Species T = c1 A | c2 B | c3 C.					*
 *									*
 * then the constructor list for T is ["c1", "c2", "c3"].		*
 *									*
 * The constructors correspond to tags on the members of the species,   *
 * so they are also called tags.					*
 ************************************************************************/

#include <string.h>
#include "../misc/misc.h"
#include "../parser/tokens.h"
#include "../utils/hash.h"
#include "../utils/lists.h"
#include "../utils/strops.h"
#include "../tables/tables.h"
#include "../clstbl/classtbl.h"
#include "../clstbl/cnstrlst.h"
#include "../classes/classes.h"
#include "../unify/unify.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/****************************************************************
 *			NUM_TYPE_TAGS				*
 ****************************************************************
 * Return the number of tags in type c.  For example, if c is   *
 * the class table entry for species T, and T is defined by     *
 *								*
 *  Species T = x | y(S) | z(R).				*
 *								*
 * then num_type_tags(c) returns 3, since there are 3 tags x,   *
 * y and z.							*
 ****************************************************************/

int num_type_tags(CLASS_TABLE_CELL *c)
{
  return list_length(c->constructors);
}


/****************************************************************
 *			TAG_NAME_TO_TAG_NUM			*
 ****************************************************************
 * Return the index of the constructor for species or family t 	*
 * that has name constr_name.  The indices are from 0.  For     *
 * example, if t is a species S defined by			*
 *								*
 *   Species S = c0(S0) | c1(S1) | c2(S2).			*
 *								*
 * and constr_name is "c1", then tag_name_to_tag_num returns	*
 * 1.								*
 *								*
 * If t is not a species or family id, or if there is no	*
 * appropriate tag, this function return -1.			*
 * (If t is a family member, it will be treated as the family.) *
 ****************************************************************/

int tag_name_to_tag_num(TYPE *t, char *constr_name)
{
  STR_LIST *p;
  int n;
  TYPE_TAG_TYPE kind;

# ifdef DEBUG
    if(trace_pat_complete > 2) {
      trace_s(79, constr_name);
      trace_ty( t);
      tracenl();
    }
# endif

  if(t == NULL) return -1;
  kind = TKIND(t);

  /*--------------------*
   * Replace F(T) by F. *
   *--------------------*/

  if(kind == FAM_MEM_T) {
    t = find_u(t->TY2);
    kind = TKIND(t);
  }

  if(kind != TYPE_ID_T && kind != FAM_T) {
#   ifdef DEBUG
      if(trace_pat_complete > 2) {
	trace_s(80);
      }
#   endif
    return -1;
  }

  /*---------------------------------------------*
   * Search the constructors list for this type. *
   *---------------------------------------------*/

# ifdef DEBUG
    if(trace_pat_complete > 2) {
      trace_t(498);
      print_str_list_nl(t->ctc->constructors);
    }
# endif

  for(n = 0, p = t->ctc->constructors;
      p != NULL && strcmp(p->head.str, constr_name) != 0;
      n++, p = p->tail) {
    /* Null body */
  }

  /*----------------------------------*
   * If nothing was found, return -1. *
   *----------------------------------*/

  if(p == NULL) {
#   ifdef DEBUG
      if(trace_pat_complete > 2) {
        trace_s(81);
      }
#   endif
    return -1;
  }

  /*----------------------------------------------*
   * If something was found, the it must be in n. *
   *----------------------------------------------*/

  else {
#   ifdef DEBUG
      if(trace_pat_complete > 2) {
        trace_s(82, n);
      }
#   endif
    return n;
  }
}


/****************************************************************
 *			TAG_NUM_TO_TAG_NAME			*
 ****************************************************************
 * Return the name of the constructor for type T that has index *
 * n, where indexing is from 0.  Return NULL if there is none.	*
 * This is similar to the preceding function, but does the	*
 * translation in the opposite order.				*
 ****************************************************************/

char* tag_num_to_tag_name(TYPE *t, int n)
{
  STR_LIST *p;
  TYPE_TAG_TYPE kind;
  int k;

# ifdef DEBUG
    if(trace_pat_complete > 2) {
      trace_s(83, n);
      trace_ty(t);
      tracenl();
    }
# endif

  if(t == NULL) return NULL;
  kind = TKIND(t);

  /*--------------------*
   * Replace F(T) by F. *
   *--------------------*/

  if(kind == FAM_MEM_T) {
    t = find_u(t->TY2);
    kind = TKIND(t);
  }

  if(kind != FAM_T && kind != TYPE_ID_T) return NULL;

  /*------------------------------------*
   * Search the constructor list for t. *
   *------------------------------------*/

  for(k = 0, p = t->ctc->constructors;
      p != NULL && k < n;
      k++, p = p->tail) {
    /* Null body */
  }

  /*---------------------------------*
   * If no constructor, return NULL. *
   *---------------------------------*/

  if(p == NULL) {
#   ifdef DEBUG
      if(trace_pat_complete > 2) {
        trace_s(84, n);
      }
#   endif
    return NULL;
  }

  /*-------------------------------------------------*
   * If we didn't exhaust the constructor list, then *
   * p->head.str is the desired constructor.	     *
   *-------------------------------------------------*/

  else {
#   ifdef DEBUG
      if(trace_pat_complete > 2) {
        trace_s(85, p->head.str);
      }
#   endif
    return p->head.str;
  }
}


