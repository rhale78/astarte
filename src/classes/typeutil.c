/**************************************************************
 * File:    classes/typeutil.c
 * Purpose: General utilities for types, used by both the
 *          compiler and the interpreter.
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
#include "../classes/classes.h"
#include "../clstbl/classtbl.h"
#include "../clstbl/meettbl.h"
#include "../standard/stdtypes.h"
#include "../utils/lists.h"
#include "../unify/unify.h"
#include "../unify/constraint.h"
#include "../generate/generate.h"
#ifdef DEBUG
#  include "../debug/debug.h"
#endif


/****************************************************************
 *			SEEN_RECORD_LIST	 		*
 ****************************************************************
 * seen_record_list is a list of variables whose seen field has *
 * been set to 1.  It is used for clearing the seen fields.	*
 ****************************************************************/

TYPE_LIST* seen_record_list = NIL;


/****************************************************************
 *			TYPE_ID_OR_VAR		 		*
 ****************************************************************
 * Return true just when t is a species id or type variable or	*
 * a wrapped-type.  These are the things that can be given as	*
 * link labels in genus or meet declarations.			*
 ****************************************************************/

Boolean type_id_or_var(TYPE *t)
{
  if(t == NULL) return FALSE;

  {TYPE_TAG_TYPE kind = TKIND(find_u(t));
   return kind == TYPE_ID_T || 
	  kind == WRAP_TYPE_T ||
	  kind == TYPE_VAR_T;
  }
}


/****************************************************************
 *			GOOD_CLASS_UNION_PAIR			*
 ****************************************************************
 * Return true if t is an acceptable pair of types for adding	*
 * a family or pair or function to a genus.  That is, return    *
 * true if you can declare					*
 *								*
 *   Genus G => ty.						*
 ****************************************************************/

Boolean good_class_union_pair(TYPE *ty)
{
  if(ty == NULL) return FALSE;

  {TYPE* ty1 = ty->TY1;
   TYPE* ty2 = ty->TY2;

   return (type_id_or_var(ty1) &&
	   type_id_or_var(ty2) &&
	   (TKIND(ty1) != TYPE_VAR_T || ty1 != ty2));
  }
}


/****************************************************************
 *			IS_HERMIT_TYPE				*
 ****************************************************************
 * Return true if t is type ().					*
 ****************************************************************/

Boolean is_hermit_type(TYPE *t)
{
  t = find_u(t);
  if(t == NULL_T || t->ctc == NULL) return FALSE;
  if(TKIND(t) == TYPE_ID_T && t->ctc->num == Hermit_num) return TRUE;
  return FALSE;
}


/****************************************************************
 *			OCCURS_IN				*
 ****************************************************************
 * occurs_in(a,b) is true if variable, type or family a occurs 	* 
 * in type b, outside a lower bound.  a must not require find_u.*
 ****************************************************************/

Boolean occurs_in(TYPE *a, TYPE *b)
{
  if(b == NULL) return FALSE;

  b = find_u(b);
  switch(TKIND(b)) {
    case PAIR_T:
    case FUNCTION_T:
    case FAM_MEM_T:

      /*--------------------------------------*
       * Search parts of b recursively for a. *
       *--------------------------------------*/

      return occurs_in(a, b->TY1) || occurs_in(a, b->TY2);

    case TYPE_ID_T:
    case FAM_T:
    case WRAP_TYPE_T:
    case WRAP_FAM_T:
	
      /*----------------------------------------------------------------*
       * a occurs in a type or family b only if a is the same as b. 	*
       * Check sameness by comparing kinds and ctc pointers.		*
       *----------------------------------------------------------------*/

      return TKIND(a) == TKIND(b) && a->ctc == b->ctc;

    case FICTITIOUS_TYPE_T:
    case FICTITIOUS_FAM_T:
    case FICTITIOUS_WRAP_TYPE_T:
    case FICTITIOUS_WRAP_FAM_T:
      return TKIND(a) == TKIND(b) && 
	     a->ctc == b->ctc && 
	     a->TNUM == b->TNUM;

    case TYPE_VAR_T:
    case FAM_VAR_T:
    case PRIMARY_TYPE_VAR_T:
    case PRIMARY_FAM_VAR_T:
    case WRAP_TYPE_VAR_T:
    case WRAP_FAM_VAR_T:

      /*--------------------------------------------------------*
       * If b is a variable, the a occurs in b only if a is the *
       * same variable. 					*
       *--------------------------------------------------------*/

      return a == b;

    /*------------------------------------------------------------*
     * The default action handles BAD_T.  Note that MARK_T cannot *
     * occur, because of the find_u above, which skips over	  *
     * MARK_T nodes.						  *
     *------------------------------------------------------------*/

    default:
      return FALSE;

  }
}


/****************************************************************
 *			MARK_SEEN				*
 ****************************************************************
 * Mark type node t as seen, and record it in seen_record_list. *
 ****************************************************************/

void mark_seen(TYPE *t)
{
  t->seen = 1;
  SET_LIST(seen_record_list, type_cons(t, seen_record_list));
}


/****************************************************************
 *			RESET_SEEN				*
 ****************************************************************
 * Set the seen field to 0 in all type nodes in list 		*
 * seen_record_list, and set seen_record_list to NIL.		*
 ****************************************************************/

void reset_seen(void)
{
  TYPE_LIST *p;
  for(p = seen_record_list; p != NIL; p = p->tail) {
    p->head.type->seen = 0;
  }
  SET_LIST(seen_record_list, NIL);
}


/****************************************************************
 *			REPLACE_NULL_VARS			*
 ****************************************************************
 * Replace all nulls in *tt by new unbound variables.  If *tt	*
 * itself is null, then it is bound to a new variable, and this *
 * new variable is reference counted.				*
 ****************************************************************/

PRIVATE void replace_null_vars_help(TYPE **tt)
{
  TYPE* t = *tt;

 tail_recur:
  if(t == NULL) bump_type(*tt = type_var_t(NULL));
  else {
    switch(TKIND(t)) {
      case PAIR_T:
      case FUNCTION_T:
      case FAM_MEM_T:
        replace_null_vars_help(&(t->TY1));
        t = t->TY2;
        goto tail_recur;

      case BAD_T:
      case FAM_T:
      case TYPE_ID_T:
      case FICTITIOUS_TYPE_T:
      case FICTITIOUS_FAM_T:
      case WRAP_TYPE_T:
      case WRAP_FAM_T:
      case FICTITIOUS_WRAP_TYPE_T:
      case FICTITIOUS_WRAP_FAM_T:
        return;

      case MARK_T:
        t = t->TY1;
        goto tail_recur;

      case TYPE_VAR_T:
      case FAM_VAR_T:
      case PRIMARY_TYPE_VAR_T:
      case PRIMARY_FAM_VAR_T:
      case WRAP_TYPE_VAR_T:
      case WRAP_FAM_VAR_T:
        if(t->TY1 != NULL) {
	  t = t->TY1;
	  goto tail_recur;
        }
        else if(!t->seen) {
	  TYPE_LIST *p;

	  mark_seen(t);
	  for(p = lower_bound_list(t); p != NIL; p = p->tail) {
	    if(LKIND(p) != TYPE2_L) {
	      replace_null_vars_help(&(p->head.type));
	    }
	  }
          return;
	}
    }
  }
}
 
/*----------------------------------------------------------*/

void replace_null_vars(TYPE **tt)
{
  replace_null_vars_help(tt);
  reset_seen();
}


/****************************************************************
 * 			FULL_TYPE_EQUAL				*
 ****************************************************************
 * Return true if ss and tt are structurally equal.  Ignore	*
 * MARK_T nodes, and skip over bound variables.			*
 ****************************************************************/

Boolean full_type_equal(TYPE *ss, TYPE *tt)
{
  TYPE *s, *t;
  TYPE_TAG_TYPE skind, tkind;
 
  if(ss == NULL) {
     if(tt == NULL) return TRUE;
     else return FALSE;
  }
  if(tt == NULL) return FALSE;

  s     = FIND_U_NONNULL(ss);
  t     = FIND_U_NONNULL(tt);
  skind = TKIND(s);
  tkind = TKIND(t);

  if(skind != tkind) return FALSE;

  else switch(skind) {
    case FUNCTION_T:
    case PAIR_T:
    case FAM_MEM_T:
      return full_type_equal(s->ty1, t->ty1) && 
	     full_type_equal(s->TY2, t->TY2);

    case TYPE_ID_T:
    case FAM_T:
    case WRAP_TYPE_T:
    case WRAP_FAM_T:
      return s->ctc == t->ctc;

    case FICTITIOUS_TYPE_T:
    case FICTITIOUS_FAM_T:
    case FICTITIOUS_WRAP_TYPE_T:
    case FICTITIOUS_WRAP_FAM_T:
      return s->ctc == t->ctc && s->TNUM == t->TNUM;

    case TYPE_VAR_T:
    case FAM_VAR_T:
    case PRIMARY_TYPE_VAR_T:
    case PRIMARY_FAM_VAR_T:
    case WRAP_TYPE_VAR_T:
    case WRAP_FAM_VAR_T:
      return s == t;

    /*------------------------------------------------------------*
     * The default action handles BAD_T.  Note that MARK_T cannot *
     * occur, because of the find_u above, which skips over	  *
     * MARK_T nodes.						  *
     *------------------------------------------------------------*/

    default: return FALSE;
  }
}


/****************************************************************
 *			TYPE_MEM				*
 ****************************************************************
 * Return the first member of list l that is structurally equal	*
 * to type t, or NULL if t does not occur in l.			*
 *								*
 * If tag is nonzero, ignore any entries in list l that have	*
 * a tag different from tag.  If tag is 0, then look at all	*
 * members of list l.						*
 ****************************************************************/

TYPE* type_mem(TYPE *t, TYPE_LIST *l, int tag)
{
  TYPE_LIST *p;

  for(p = l; p != NIL; p = p->tail) {
    if(tag == 0 || LKIND(p) == tag) {
      if(full_type_equal(t, p->head.type)) return p->head.type;
    }
  }
  return NULL;
}


/****************************************************************
 *			  TYPE_VAR_MEM	 			*
 ****************************************************************
 * Return true if type variable V occurs in list L.		*
 ****************************************************************/

Boolean type_var_mem(TYPE *V, TYPE_LIST *L)
{
  TYPE_LIST *p;

  for(p = L; p != NIL; p = p->tail) {
    TYPE* X = p->head.type;
    IN_PLACE_FIND_U(X);
    if(X == V) return TRUE; 
  }
  return FALSE;
}


/*****************************************************************
 *			VAR_LIST_INTERSECT			 *
 *****************************************************************
 * This function returns the intersection of lists a and b 	 *
 * (those items that are members of both).  It uses == to	 *
 * compare list members.  					 *
 *****************************************************************/

TYPE_LIST* var_list_intersect(TYPE_LIST *a, TYPE_LIST *b)
{
  STR_LIST *p, *result;
  Boolean nonb_seen = FALSE;

  if(b == NIL) return NIL;

  result = NIL;
  for(p = a; p != NIL; p = p->tail) {
    if(type_var_mem(p->head.type, b)) {
      SET_LIST(result, type_cons(p->head.type, result));
    }
    else nonb_seen = TRUE;
  }

  if(!nonb_seen) {
    drop_list(result);
    return a;
  }
  else {
    if(result != NIL) result->ref_cnt--;
    return result;
  }
}


/****************************************************************
 *			ADD_VAR_TO_LIST				*
 ****************************************************************
 * Return the result by adding variable V to list L, if it is   *
 * not there already.  If V is in L, then just return L.	*
 ****************************************************************/

TYPE_LIST* add_var_to_list(TYPE *V, TYPE_LIST *L)
{
  if(type_var_mem(V, L)) return L;
  else return type_cons(V, L);
}


/****************************************************************
 *			TYPE_JOIN				*
 ****************************************************************
 * A and B must be two constant types: they must not be		*
 * structured or be variables.  This function returns the	*
 * type that is the join of A and B.				*
 ****************************************************************/

TYPE* type_join(TYPE *A, TYPE *B)
{
  int join_ctc_num;

  if(A->ctc == B->ctc) return A;

  join_ctc_num = full_get_join(A->ctc, B->ctc);
  return wrap_tf(ctcs[join_ctc_num]);  
}


/****************************************************************
 *			TYPE_MEET				*
 ****************************************************************
 * A and B must be two constant types: they must not be		*
 * structured or be variables.  This function returns the	*
 * type that is the meet of A and B, or NULL if the meet of A	*
 * and B is bottom.						*
 ****************************************************************/

TYPE* type_meet(TYPE *A, TYPE *B)
{
  int meet_ctc_num;

  if(A->ctc == B->ctc) return A;

  meet_ctc_num = full_get_intersection_tm(A->ctc, B->ctc);
  if(meet_ctc_num == -1) return NULL;
  else return wrap_tf(ctcs[meet_ctc_num]);  
}


/****************************************************************
 *			DROP_HASH_TYPE				*
 ****************************************************************
 * Drop the reference count of h->val.type.			*
 ****************************************************************/

void drop_hash_type(HASH2_CELLPTR h)
{
  drop_type(h->val.type);
}


/****************************************************************
 *			BUMP_HASH_TYPE				*
 ****************************************************************
 * Bump the reference count of h->val.type.			*
 ****************************************************************/

void bump_hash_type(HASH2_CELLPTR h)
{
  bump_type(h->val.type);
}
