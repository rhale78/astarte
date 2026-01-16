/**************************************************************
 * File:    classes/copytype.c
 * Purpose: Copy TYPE structures
 * Author:  Karl Abrahamson
 **************************************************************/

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

#include "../misc/misc.h"
#include "../alloc/allocate.h"
#include "../utils/hash.h"
#include "../utils/lists.h"
#include "../classes/classes.h"
#include "../unify/unify.h"
#include "../unify/constraint.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif


/************************************************************************
 *			COPY_TYPE	 				*
 ************************************************************************
 * Return a copy of type t.  Variables in t are copied.			*
 * The bits of mode are							*
 *									*
 *   bit 0: norestrict							*
 *									*
 *	    (If norestrict is 1, then copy the norestrict field. 	*
 *	     Otherwise, set the norestrict field to 0.)			*
 *									*
 *   bit 1: anon							*
 *									*
 *	    (If anon is 1, then copy anonymous variables as new		*
 *	     variables; otherwise treat them like other variables.)	*
 *									*
 *   bit 2: mark							*
 *									*
 *	    (If mark is 1, then preserve MARK_T nodes.  Otherwise	*
 *	     get rid of them.  						*
 ************************************************************************/

TYPE* copy_type(TYPE *t, int mode)
{
  TYPE* result;

  /*---------------------------------------------------------*
   * Set up a table (ty_b) that is used to store previously  *
   * encountered variables, so that only one copy of each    *
   * variable will be created.  Run copy_type1 with that     *
   * table.  Then drop the references that are made by       *
   * the table                                               *
   *---------------------------------------------------------*/

  HASH2_TABLE* ty_b = NULL;
  bump_type(result = copy_type1(t, &ty_b, mode));
  scan_and_clear_hash2(&ty_b, drop_hash_type);
  if(result != NULL) result->ref_cnt--;
  return result;
}


/******************************************************************
 * 			COPY_TYPE_LIST				  *
 ******************************************************************
 * Return a copy of list l, with copies of the types in list l.   *
 * Use table ty_b to look up variables, and use given copy mode.  *
 * Maintain list tags.						  *
 ******************************************************************/

PRIVATE TYPE_LIST* copy_type_list(TYPE_LIST *l, HASH2_TABLE **ty_b, int mode)
{
  if(l == NIL) return NIL;
  else {
    HEAD_TYPE hd;
    hd.type = copy_type1(l->head.type, ty_b, mode);
    return general_cons(hd,
		        copy_type_list(l->tail, ty_b, mode),
			LKIND(l));
  }
}


/************************************************************************
 *			COPY_VARIABLE	 				*
 ************************************************************************
 * Return a copy of variable t, from table ty_b if there is a binding	*
 * there.  If there is no binding in ty_b, make one, holding a		*
 * new copy.  If (mode & 2) is nonzero and t is an anonymous variable,	*
 * don't look in the table; just get a new variable.  If		*
 * (mode & 1) is nonzero, don't copy norestrict status of the variable.	*
 ************************************************************************/

PRIVATE TYPE* copy_variable(TYPE *t, HASH2_TABLE **ty_b, int mode)
{
  register TYPE *cpyt;
  register HASH2_CELLPTR h;
  HASH_KEY u;
  Boolean anon, norestrict;

  norestrict = mode & 1;
  anon       = mode & 2 && t->anon;

  /*--------------------------------------------------------------*
   * If the variable has a binding in *ty_b, return that binding. *
   * But do not get the type for an anonymous variable.		  *
   *--------------------------------------------------------------*/

  u.num = tolong(t);
  h     = insert_loc_hash2(ty_b, u, inthash(u.num), eq);
  if(h->key.num != 0 && !anon) return h->val.type;

  /*-----------------------------------------------------------*
   * Otherwise, this is the first time t has been encountered. *
   * Make a copy and put the copy in ty_b.                     *
   *-----------------------------------------------------------*/

  cpyt  = new_type(0, NULL_T);
  *cpyt = *t;
  cpyt->ref_cnt = 1;
  if(!norestrict) cpyt->norestrict = 0;
  h->key.num  = tolong(t);
  h->val.type = cpyt;     /* Ref from cpyt */

  /*------------------------------------------------------------*
   * Now copy the lower bounds of a variable.  This must        *
   * come after putting cpyt into the table, since it can cause *
   * the table to be reallocated, and hence can cause pointer   *
   * h to become stale.					        *
   *------------------------------------------------------------*/

  bump_list(cpyt->LOWER_BOUNDS = 
	    copy_type_list(lower_bound_list(cpyt), ty_b, mode));

  return cpyt;
}
    

/********************************************************************
 *			COPY_TYPE1	 			    *
 ********************************************************************
 * Return a copy of type tt, with variables that are bound in table *
 * *ty_b replaced by the variable to which they are bound.  If ty_b *
 * is NULL, then do not replace variables.  Parameter mode is as    *
 * for copy_type.						    *
 ********************************************************************/

TYPE* copy_type1(TYPE *tt, HASH2_TABLE **ty_b, int mode)
{
  register TYPE_TAG_TYPE kind;
  Boolean marked;

  if(tt == NULL_T) return NULL_T;

  /*-----------------------------------------------------------------*
   * If tt->copy == 0, then there are no variables in the tree rooted*
   * at tt, so it is safe just to return tt itself.                  *
   *-----------------------------------------------------------------*/

  if(tt->copy == 0) return tt;

  tt = find_mark_u(tt, &marked);

  /*-------------------------------------*
   * Copy variables using copy_variable. *
   *-------------------------------------*/

  kind = TKIND(tt);
  if(IS_VAR_T(kind)) {
    register TYPE* result = 
	(ty_b == NULL) ? tt : copy_variable(tt, ty_b, mode);
    if(marked && (mode & 4)) result = new_type(MARK_T, result);
    return result;
  }

  /*---------------------------*
   * Do not copy simple types. *
   *---------------------------*/

  if(kind >= BAD_T) {
    if(marked && (mode & 4)) return new_type(MARK_T, tt);
    else return tt;
  }

  /*------------------------------------------------------------*
   * Copy structured types recursively.  Note that we will	*
   * not get here with things like TYPE_ID_T, since they will	*
   * be handled at the case where tt->copy == 0.		*
   *------------------------------------------------------------*/

  {register TYPE* cpyt = allocate_type();
   register TYPE *ty1, *ty2;

   *cpyt          = *tt;
   cpyt->ref_cnt  = 0;
   cpyt->hermit_f = 0;
   bump_type(cpyt->TY2 = copy_type1(tt->TY2, ty_b, mode));
   bump_type(cpyt->TY1 = copy_type1(tt->TY1, ty_b, mode));
   ty1 = cpyt->TY1;
   ty2 = cpyt->TY2;
   if(ty2 != NULL && !ty2->copy && ty1 != NULL && !ty1->copy) cpyt->copy = 0;
   return cpyt;
  }
}


/******************************************************************
 * 			COPY_TYPE_NODE				  *
 ******************************************************************
 * Return a copy of node t, but do not recursively copy children. *
 ******************************************************************/

TYPE* copy_type_node(TYPE *t)
{
  TYPE* tt   = new_type(TKIND(t), t->ty1);
  tt->ctc    =  t->ctc;
  bump_type(tt->TY2 = t->TY2);
  return tt;
}

