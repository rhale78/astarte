/**************************************************************
 * File:    unify/unify.c
 * Purpose: Unification of polymorphic types.
 * Author:  Juan Du and Karl Abrahamson
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

/************************************************************************
 * This file implements unification of polymorphic types.  		*
 ************************************************************************/

#include "../misc/misc.h"
#include "../parser/tokens.h"
#include "../utils/lists.h"
#include "../utils/hash.h"
#include "../exprs/expr.h"
#include "../classes/classes.h"
#include "../unify/unify.h"
#include "../unify/constraint.h"
#include "../standard/stdtypes.h"
#include "../clstbl/classtbl.h"
#include "../clstbl/meettbl.h"
#include "../alloc/allocate.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif


/****************************************************************
 *			PUBLIC VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			binding_list				*
 *			use_binding_list			*
 ****************************************************************
 * When use_binding_list is 0, bindings done during		*
 * unification are stored directly in the TYPE nodes.  The	*
 * binding of a variable is stored in its TY1 field.		*
 * In the case, binding_list is not used.			*
 *								*
 * When use_binding_list is nonzero, bindings done during	*
 * unification are instead stored in list binding_list, and	*
 * that list is consulted to find bindings.			*
 *								*
 * NOTE: When use_binding_list is nonzero, bindings of NULL 	*
 * variables are not recorded.  So there must be no NULL	*
 * variables then.  NULL variables are handled as anonymous	*
 * variables when use_binding_list is 0.			*
 *								*
 * binding_list holds the following kinds of nodes.		*
 *								*
 *   (1) List nodes of kind TYPE_L, each having a pointer to a	*
 *	 type (V,t).  This indicates that variable V is		*
 * 	 bound to t.						*
 *								*
 *   (2) List nodes of kind LIST_L each having a pointer to a   *
 *	 list V::L, where V is a variable and L is V's          *
 *	 LOWER_BOUND value.					*
 ****************************************************************/

LIST*   binding_list     = NULL;
int     use_binding_list = 0;

/****************************************************************
 *			PRIVATE VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			unify_seen_constraints			*
 ****************************************************************
 * unify_seen_constraints is set true when unification sees a   *
 * variable that has constraints (lower bounds) on it.		*
 ****************************************************************/

PRIVATE Boolean unify_seen_constraints = FALSE;

/****************************************************************
 *			new_bindings				*
 ****************************************************************
 * new_bindings records bindings done by unification, 		*
 * so that those bindings can be undone to back out of a	*
 * unification.  It is only used when use_binding_list is 0.	*
 *								*
 * new_bindings has four kinds of nodes.			*
 *								*
 *   (1) List nodes of kind TYPE_L, each having a pointer to a	*
 *	 bound variable.  In this case, the variable pointed to *
 *	 should be unbound to back out, by setting its TY1	*
 *	 field to NULL.	(This is used to back out of		*
 *	 an ordinary variable binding.)				*
 *								*
 *   (2) List nodes of kind STYPE_L each having a pointer to a	*
 *	 pointer p to a type.  In this case, p itself should be *
 *	 set to NULL to back out.  (This is used to back out of	*
 *	 a binding of a NULL field.  A NULL field is an		*
 *	 anonymous variable, where we have not bothered to 	*
 *	 create the variable.)					*
 *								*
 *   (3) List nodes of kind TYPE_L each having a pointer to a	*
 *	 pair type (A,B).  This indicates that type node B	*
 *	 should be copied on top of type node A to back out.	*
 *	 (The unification algorithm needs to replace a function *
 *	 type by type () sometimes, and it does so by copying ()*
 *	 into the node that formerly stored the function type.	*
 *	 This backs out of that.)				*
 *								*
 *   (4) List nodes of kind LIST_L each having a pointer to a   *
 *	 list V::L, where V is a variable and L is the     	*
 *	 list that V's LOWER_BOUND field should be set to upon	*
 *	 backing out.  This is used to undo changes to the	*
 *	 lower bounds.						*
 ****************************************************************/

PRIVATE LIST* new_bindings = NIL;

/****************************************************************
 *			FINGER_NEW_BINDING_LIST			*
 ****************************************************************
 * Return the value of new_bindings.  This is used so that 	*
 * bindings can be undone back to this point, using 		*
 * undo_bindings_u.						*
 *								*
 * When use_binding_list is true, finger_new_binding_list 	*
 * instead returns the value of binding_list, which can be	*
 * used to back out of bindings.				*
 ****************************************************************/

LIST* finger_new_binding_list(void)
{
  return use_binding_list ? binding_list : new_bindings;
}


/****************************************************************
 *			COMMIT_NEW_BINDING_LIST			*
 ****************************************************************
 * Set new_bindings to mark.  This should be used to            *
 * commit all of the bindings that have been done since 	*
 * fingering the new binding list at mark, so that		*
 * undo_bindings_u cannot undo them.				*
 *								*
 * When use_binding_list is true, this function has no effect.	*
 ****************************************************************/

void commit_new_binding_list(LIST *mark)
{
  if(!use_binding_list) SET_LIST(new_bindings, mark);
}


/****************************************************************
 *			CLEAR_NEW_BINDING_LIST_U		*
 ****************************************************************
 * Empty out new_bindings.  When use_binding_list is true, this *
 * function has no effect.					*
 ****************************************************************/

void clear_new_binding_list_u(void)
{
  if(!use_binding_list) SET_LIST(new_bindings, NIL);
}


/****************************************************************
 *			UNDO_BINDINGS_U				*
 ****************************************************************
 * Undo the bindings recorded in new_bindings up to but not	*
 * including the node pointed to by mark.  If mark is NULL, all *
 * bindings in new_bindings will be undone.			*
 *								*
 * See the documentation for new_bindings, above.		*
 *								*
 * When use_binding_list is nonzero, this function removes	*
 * bindings from binding_list.					*
 ****************************************************************/

void undo_bindings_u(LIST *mark)
{
  LIST* p;
  TYPE** s;
  TYPE_TAG_TYPE kind;
  TYPE* t = NULL;

  if(use_binding_list) {
    SET_LIST(binding_list, mark);
  }

  else {
    for(p = new_bindings; p != NIL && p != mark; p = p->tail) {

      /*----------------------------------------*
       * Handle a lower bound list replacement. *
       *----------------------------------------*/

      if(LKIND(p) == LIST_L) {
	LIST* this_list = p->head.list;
	TYPE* V         = this_list->head.type;
	LIST* lwbs      = this_list->tail;

#       ifdef DEBUG
          if(trace_unify > 1) trace_s(95, V);
#       endif

        SET_LIST(V->LOWER_BOUNDS, lwbs);
      }

      /*----------------------------*
       * Handle a variable binding. *
       *----------------------------*/

      else /* LKIND(p) != LIST_L */{

	/*----------------------------------------------------------*
	 * Get the type t, and a pointer s to the memory cell that  *
	 * needs to be set to NULL, in the case where t is not a    *
	 * pair.						    *
	 *----------------------------------------------------------*/

	if(LKIND(p) == TYPE_L) {
	  t    = p->head.type;
	  s    = &(t->ty1);
	  kind = TKIND(t);

#         ifdef DEBUG
            if(trace_unify > 1) {
	      if(kind != PAIR_T) {
	        trace_s(28, t);
	      }
	      else {
	        trace_s(29, *s);
	      }
	    }
#         endif

        }
	else /* LKIND(p) == STYPE_L */ {
          /* t = NULL;  (default from above) */
	  s    = p->head.stype;
	  kind = TYPE_VAR_T;
#         ifdef DEBUG
            if(trace_unify > 1) trace_s(96, s);
#         endif
        }

	/*--------------------------------------------------------------*
	 * If we are unbinding a variable or a pointer, then just 	*
	 * drop its current value and bind to NULL.			*
	 *--------------------------------------------------------------*/

	if(IS_VAR_T(kind)) {
	  drop_type(*s);
	  *s = NULL_T;
	}

	/*--------------------------------------------------------------*
	 * If we are unbinding a PAIR_T node (A,B), then we must copy 	*
	 * node B on top of node A.  Be careful about reference counts.	*
	 * Note that this is only used when A = (), so there is no need	*
	 * to drop references in A.  B is always a function type.	*
	 *--------------------------------------------------------------*/

	else if(kind == PAIR_T) {
	  TYPE* where  = t->TY1;
	  TYPE* hold   = t->TY2;
	  LONG  rc     = where->ref_cnt;

	  *where = *hold;
	  bump_type(where->TY1);
	  bump_type(where->TY2);
	  where->ref_cnt = rc;
	}
      } /* end else (LKIND(p) != LIST_L) */
    } /* end for(p = ...) */
  } /* end else(!use_binding_list) */

  SET_LIST(new_bindings, mark);
}


/****************************************************************
 *			HAVE_REPLACED_LOWER_BOUNDS		*
 ****************************************************************
 * Record in new_bindings that lower bound list lwb in variable *
 * V is being replaced by another list, so that we can back out *
 * by setting V's lower bound list to lwb.			*
 ****************************************************************/

void have_replaced_lower_bounds(TYPE *V, TYPE_LIST *lwb)
{
  SET_LIST(new_bindings, 
	   list_cons(type_cons(V, lwb), new_bindings));
}


/****************************************************************
 *			HAVE_BOUND_NULL_U			*
 ****************************************************************
 * Record that an implicit variable, represented by a null	*
 * pointer in address *t, has been bound.  			*
 *								*
 * See the documentation for new_bindings.			*
 ****************************************************************/

PRIVATE void have_bound_null_u(TYPE **t)
{
  SET_LIST(new_bindings, stype_cons(t, new_bindings));
}


/****************************************************************
 *			HAVE_BOUND_U				*
 ****************************************************************
 * Notes that variable t has been bound.  			*
 *								*
 * See the documentation for new_bindings.			*
 ****************************************************************/

PRIVATE void have_bound_u(TYPE *t)
{
# ifdef DEBUG
    if(trace_unify > 1) trace_s(19);
# endif

  SET_LIST(new_bindings, type_cons(t, new_bindings));
}


/****************************************************************
 *		GET_BINDING_FROM_BINDING_LIST			*
 ****************************************************************
 * Return the binding of V in list binding_list, or NULL if	*
 * there is no binding.						*
 ****************************************************************/

PRIVATE TYPE* get_binding_from_binding_list(TYPE *V)
{
  register LIST *p;

  for(p = binding_list; p != NIL; p = p->tail) {
    if(LKIND(p) == TYPE_L) {
      register TYPE* w = p->head.type;
      if(w->TY1 == V) return w->TY2;
    }
  }
  return NULL;
}


/****************************************************************
 *			FIND_U					*
 ****************************************************************
 * Returns type A after scanning over chains of bound variables *
 * and MARK_T nodes.						*
 *								*
 * This function is heavily used, so we try to make it tight.   *
 * Macro FIND_U does the same thing, but is more efficient.     *
 * Macro FIND_U_NONNULL is the same, but assumes that its 	*
 * argument is not null.  It is still more efficient.		*
 ****************************************************************/

TYPE* find_u(TYPE *A)    
{
  if(A == NULL_T) return NULL_T;

  /*---------------------------------------------------*
   * Stop the scan at something that is not a variable *
   *---------------------------------------------------*/

  if(TKIND(A) < FIRST_BOUND_T) return A;

  /*------------------------------------------------------------*
   * At this point, we must have a variable or MARK_T node.	*
   *------------------------------------------------------------*/

  for(;;) {

    /*------------------------------------------------------*
     * If A is bound via the ty1 field, follow the binding. *
     *------------------------------------------------------*/

    if(A->TY1 != NULL) {
      A = A->TY1;
      if(TKIND(A) < FIRST_BOUND_T) return A;
      else continue;
    }

    /*-------------------------------------------------------*
     * If A is bound by binding_list, then get that binding. *
     *-------------------------------------------------------*/

    if(use_binding_list && binding_list != NIL) {
      register TYPE* t = get_binding_from_binding_list(A);
      if(t == NULL) return A;
      else {
        A = t;
        if(TKIND(A) < FIRST_BOUND_T) return A;
        else continue;
      }
    }

    /*-----------------------------*
     * If A is unbound, return it. *
     *-----------------------------*/

    return A;
  }
}
 

/****************************************************************
 *			FIND_MARK_U				*
 ****************************************************************
 * Returns type A after scanning over chains of bound variables *
 * and MARK_T nodes.						*
 *								*
 * The difference between this function and find_u is that, if  *
 * any MARK_T nodes are encountered, this function sets *marked *
 * to TRUE.  If no MARK_T nodes are found, *marked is set to	*
 * FALSE.							*
 ****************************************************************/

TYPE* find_mark_u(TYPE *A, Boolean *marked)    
{
  register TYPE_TAG_TYPE A_kind;
  *marked = FALSE;

  if(A == NULL_T) return NULL_T;

  /*---------------------------------------------------*
   * Stop the scan at something that is not a variable *
   *---------------------------------------------------*/

  A_kind = TKIND(A);
  if(A_kind < FIRST_BOUND_T) return A;
  if(A_kind == MARK_T) *marked = TRUE;

  /*------------------------------------------------------------*
   * At this point, we must have a variable or MARK_T node.	*
   *------------------------------------------------------------*/

  for(;;) {

    /*------------------------------------------------------*
     * If A is bound via the ty1 field, follow the binding. *
     *------------------------------------------------------*/

    if(A->TY1 != NULL) {
      A = A->TY1;
      A_kind = TKIND(A);
      if(A_kind < FIRST_BOUND_T) return A;
      if(A_kind == MARK_T) *marked = TRUE;
      else continue;
    }

    /*-------------------------------------------------------*
     * If A is bound by binding_list, then get that binding. *
     *-------------------------------------------------------*/

    if(use_binding_list && binding_list != NIL) {
      register TYPE* t = get_binding_from_binding_list(A);
      if(t == NULL) return A;
      else {
        A = t;
        A_kind = TKIND(A);
        if(A_kind < FIRST_BOUND_T) return A;
        if(A_kind == MARK_T) *marked = TRUE;
        else continue;
      }
    }

    /*-----------------------------*
     * If A is unbound, return it. *
     *-----------------------------*/

    return A;
  }
}


/****************************************************************
 *			UNIFY_PARTS_U				*
 ****************************************************************
 * kind must be a variable kind (TYPE_VAR_T, etc.)  		*
 *								*
 * If kind is an ordinary variable kind (TYPE_VAR_T or 		*
 * FAM_VAR_T), then unify a copy of ctcs[num]->ty with *b.	*
 *								*
 * If kind is a wrap variable kind, then unify a wrap variable  *
 * or species or family whose domain is ctcs[num] with *b.	*
 * But if ctcs[num] is the table entry for a species or family, *
 * then fail.							*
 *								*
 * If kind is a primary variable kind, then unify a primary 	*
 * variable or species or family whose domain is ctcs[num] with *
 * *b.								*
 *								*
 * Record bindings if record is true.				*
 *								*
 * Return TRUE on success, FALSE on failure.  (Failure occurs	*
 * if the unification fails.)					*
 *								*
 * Helper function type_for_unify_parts gets the type to 	*
 * unify with *b.						*
 ****************************************************************/

PRIVATE TYPE* type_for_unify_parts(TYPE_TAG_TYPE kind, int num)
{
  CLASS_TABLE_CELL* ctc = ctcs[num];

  if(IS_ORDINARY_KNOWN_VAR_T(kind)) return tf_or_var_t(ctc);
  else if(IS_WRAP_VAR_T(kind)) return wrap_var_t(ctc);
  else return primary_tf_or_var_t(ctc);
}

/*-----------------------------------------------------------*/

PRIVATE Boolean 
unify_parts_u(int num, TYPE_TAG_TYPE kind, TYPE **b, Boolean record)
{
  TYPE *t;
  Boolean result;

  /*----------------------------------------------------*
   * A variable `a can unify with anything, and never	*
   * causes any restriction, so skip that case.		*
   *----------------------------------------------------*/

  if(num == 0 && IS_ORDINARY_KNOWN_VAR_T(kind)) return TRUE;

  /*----------------------------------------------------*
   * A wrap variable cannot unify with a primary family *
   * or species.					*
   *----------------------------------------------------*/

  if(IS_WRAP_VAR_T(kind)) {
    register int code = ctcs[num]->code;
    if(code == FAM_ID_CODE || code == TYPE_ID_CODE) {
      return FALSE;
    }
  }
  
  /*---------------*
   * General case. *
   *---------------*/

  bump_type(t = type_for_unify_parts(kind, num));
  result = basic_unify_u(&t, b, record, NULL);
  drop_type(t);
  return result;
}


/****************************************************************
 *			FORCE_TO_HERMIT				*
 ****************************************************************
 * Try to bind b (a function type) to ().  Type A -> B can 	*
 * only be bound to () if A and B unify with one another, since *
 * () is like the identity function.				*
 *								*
 * Return TRUE on success, FALSE on failure.			*
 *								*
 * Record the binding if record is true.  See the documentation *
 * of new_bindings.						*
 *								*
 * cylist is for detecting cycles, and is as in basic_unify_u.  *
 ****************************************************************/

PRIVATE Boolean 
force_to_hermit(TYPE *b, Boolean record, TYPE_LIST *cylist)
{
  TYPE *hold;
  LONG rc;

  /*----------------------------------------------------*
   * A function type A -> B can only unify with () if A *
   * unifies with B, since () is something like the	*
   * identity function.					*
   *----------------------------------------------------*/

  if(BASIC_UNIFY(b->TY1, b->TY2, record, cylist)) {
#   ifdef DEBUG
      if(trace_unify > 1) trace_s(104, b);
#   endif

    if(record) {
      hold = allocate_type();
      *hold = *b;              /* refs from *b: *b loses its pointers below. */
      hold->ref_cnt = 0;
      SET_LIST(new_bindings, type_cons(pair_t(b,hold), new_bindings));
    }
    else {
      drop_type(b->TY1);  /* These pointers are lost below. */
      drop_type(b->TY2);
    }
    rc = b->ref_cnt;
    *b = *hermit_type;  /* Here, *b loses its pointers */
    b->ref_cnt = rc;
    return TRUE;
  }
  else return FALSE;
}


/****************************************************************
 *			FORCE_TO_HERMIT2			*
 ****************************************************************
 * Bind a (a variable) and b (a function type) to () if		*
 * possible.  Return TRUE on success, FALSE on failure.		*
 *								*
 * Record bindings if record is true.				*
 *								*
 * cylist is for detecting cycles, and is as in basic_unify_u.  *
 ****************************************************************/

PRIVATE Boolean 
force_to_hermit2(TYPE *a, TYPE *b, Boolean record, TYPE_LIST *cylist)
{
  if(bind_u(a, hermit_type, record, cylist)) {
    return force_to_hermit(b, record, cylist);
  }
  else return FALSE;
}


/****************************************************************
 *			BIND_VAR_U				*
 ****************************************************************
 * Bind a to b.	 a must be an unbound variable.  Record		*
 * the binding if record is true.				*
 *								*
 * This function is here to help bind_u.  It assumes that a	*
 * must be bound directly to b, and makes no decisions about	*
 * how or whether that binding should be done.  To bind a	*
 * variable, use bind_u, which is more intelligent about	*
 * bindings.							*
 * 								*
 * Lower bounds of a are handled by propagating them to b.	*
 *								*
 * Return TRUE on success, FALSE on failure.			*
 ****************************************************************/

PRIVATE Boolean 
bind_var_u(TYPE *a, TYPE *b, Boolean record)
{
  TYPE_TAG_TYPE a_kind = TKIND(a);
  TYPE_TAG_TYPE b_kind = TKIND(b);

# ifdef DEBUG
    if(trace_unify > 1) {
      trace_s(20, toint(record), a, a_kind);
      trace_ty(b);
      tracenl();
    }
# endif

  /*-------------------------------------------------------------*
   * Refuse to bind a family variable to a non-family thing, or  *
   * a type variable to a family.  This should never happen, but *
   * the test is here just to be sure. 				 *
   *-------------------------------------------------------------*/

  {register LONG a_is_fam = MEMBER(a_kind, fam_tkind_set);
   register LONG b_is_fam = MEMBER(b_kind, fam_tkind_set);
   if((a_is_fam && !b_is_fam) || (b_is_fam && !a_is_fam)) {
#    ifdef DEBUG
       if(trace) {
	 trace_s(24);
	 trace_ty(a);
	 tracenl();
	 trace_ty(b);
	 tracenl();
       }
#    endif
     return FALSE;
   }
  }

  /*-------------------------------------------------------------*
   * Perform the binding.  If a and b are both variables and b   *
   * does not prefer a secondary default, but a does, then we    *
   * cannot bind a directly to b, but instead create a third     *
   * variable that is a copy of b, and that prefers a secondary  *
   * default.  We bind a and b to that third variable.		 *
   *-------------------------------------------------------------*/

  if(IS_VAR_T(b_kind)) {
    if(lower_bound_list(b) != NIL) unify_seen_constraints = TRUE;
    if(!b->PREFER_SECONDARY && a->PREFER_SECONDARY) {
      TYPE* c = copy_type(b, 3);
      c->PREFER_SECONDARY = 1;
      return bind_var_u(a, c, record) && bind_var_u(b, c, record);
    }
  }

  if(use_binding_list) {
    SET_LIST(binding_list, type_cons(pair_t(a, b), binding_list));
  }
  else {
    bump_type(a->TY1 = b);
    if(record) have_bound_u(a);
  }

  /*----------------------------------------*
   * Handle lower bounds, if there are any. *
   *----------------------------------------*/
   
  {LIST* lwb = lower_bound_list(a);

   if(lwb != NIL) {
     unify_seen_constraints = TRUE;
     return bind_lower_bounds_u(a, lwb, record);
   }
   else return TRUE;
  }
}


/****************************************************************
 *			BIND_SAME_DOMAIN_VAR_U			*
 ****************************************************************
 * bind a and b, if possible.  a and b must both be variables,  *
 * and they must have the same domain.  Also, it must not be	*
 * the case that one of a and b is a wrap variable and the 	*
 * other is a primary variable.					*
 *								*
 * This is a special case of bind_u, below.			*
 *								*
 * If record is true, then record bindings.			*
 *								*
 * Return true if the binding succeeded.			*
 ****************************************************************/

PRIVATE Boolean bind_same_domain_var_u(TYPE *a, TYPE *b, Boolean record)
{
  register TYPE_TAG_TYPE a_kind = TKIND(a);
  register TYPE_TAG_TYPE b_kind = TKIND(b);
  register Boolean a_norestrict = a->norestrict;

  /*--------------------------------------------------------------------*
   * If a and b have the same variable kinds, then it is safe		*
   * to bind one to the other.  Prefer not to bind a nonrestrictable	*
   * or special variable, to maintain flags.				*
   *--------------------------------------------------------------------*/

  if(a_kind == b_kind) {
    if(a_norestrict || a->special) return bind_var_u(b, a, record);
    else                           return bind_var_u(a, b, record);
  }

  /*------------------------------------------------------------*
   * If the kinds are different, then one of them must		*
   * be an ordinary variable, due to the precondition that	*
   * they cannot be a wrap/primary pair.			*
   *------------------------------------------------------------*/

  else if(IS_ORDINARY_KNOWN_VAR_T(b_kind)) {
    if(b->norestrict) return FALSE;
    else              return bind_var_u(b, a, record);
  }

  else if(a_norestrict) return FALSE;
  else                  return bind_var_u(a, b, record);
}


/****************************************************************
 *			BIND_BENEATH_VAR_U			*
 ****************************************************************
 * bind a and b, if possible.  a and b must both be variables,  *
 * and a must have a domain that properly contains the domain   *
 * of b.  							*
 *								*
 * Precondition: It cannot be the case that one of a and b is	*
 * wrapped and the other is primary.				*
 *								*
 * This is a special case of bind_u, below.			*
 *								*
 * If record is true, then record bindings.			*
 *								*
 * Return true if the binding succeeded.			*
 ****************************************************************/

PRIVATE Boolean bind_beneath_var_u(TYPE *a, TYPE *b, Boolean record)
{
  register TYPE_TAG_TYPE a_kind = TKIND(a);
  register TYPE_TAG_TYPE b_kind = TKIND(b);

  /*---------------------------------------------------------------*
   * Variable a is going to be bound to a variable with a smaller  *
   * domain.  If a cannot be restricted, then this fails. 	   *
   *---------------------------------------------------------------*/

  if(a->norestrict) return FALSE;

  /*--------------------------------------------------------------*
   * If a and b have the same kind, or if a is ordinary, then it  *
   * is safe to bind a to b.					  *
   *--------------------------------------------------------------*/

  else if(a_kind == b_kind || IS_ORDINARY_KNOWN_VAR_T(a_kind)) {
    return bind_var_u(a, b, record);
  }

  /*---------------------------------------------------------------*
   * Here, variable b is going to be bound to a variable with a    *
   * different kind.  This constitutes a restriction of b.  If b   *
   * cannot be restricted, then this fails. 	   		   *
   *---------------------------------------------------------------*/

  else if(b->norestrict) return FALSE;

  /*------------------------------------------------------------*
   * Here, a = G``a or G`*a and b = H`b where H < G.  We	*
   * must bind each of a and b to a third variable c = H``c     *
   * or H`*c.							*
   *------------------------------------------------------------*/

  else {
    TYPE* c = (IS_WRAP_VAR_T(a_kind)) 
	        ? wrap_var_t(b->ctc) 
	        : primary_var_t(b->ctc);
    return bind_var_u(a, c, record) && bind_var_u(b, c, record);
  }       
}


/****************************************************************
 *			BIND_ANY_VAR_U				*
 ****************************************************************
 * bind a and b, if possible.  a must be a variable that has    *
 * domain ANY.  This is a special case of bind_u, below.	*
 *								*
 * Note that we will not necessarily bind a directly to b, but  *
 * might instead bind b to a, or bind a and b each to another	*
 * variable.							*
 *								*
 * If record is true, then record bindings.			*
 *								*
 * Return true if the binding succeeded.			*
 *								*
 * Helper function bind_ANY_fam_mem handles the case where	*
 * b is a family member.					*
 ****************************************************************/

PRIVATE Boolean 
bind_ANY_fam_mem(TYPE *a, TYPE *b, Boolean record)
{
  CLASS_TABLE_CELL* fam_ctc = find_u(b->TY2)->ctc;
  CLASS_TABLE_CELL* comm_ctc = (fam_ctc->opaque) 
    			         ? OPAQUE_ctc 
				 : TRANSPARENT_ctc;
  return unify_parts_u(comm_ctc->num, TKIND(a), &(b->TY2), record)
         && bind_var_u(a, b, record);
}

/*------------------------------------------------------------*/

PRIVATE Boolean 
bind_ANY_var_u(TYPE *a, TYPE *b, Boolean record)
{
  TYPE_TAG_TYPE b_kind = TKIND(b);
  TYPE_TAG_TYPE a_kind = TKIND(a);

  /*----------------------------------*
   * Case where b is also a variable. *
   *----------------------------------*/

  if(IS_VAR_T(b_kind)) {

    /*--------------------------------------------------*
     * We cannot bind a primary variable and a wrap 	*
     * variable together.				*
     *--------------------------------------------------*/

    if(IS_WRAP_VAR_T(a_kind) && IS_PRIMARY_VAR_T(b_kind)) return FALSE;
    if(IS_WRAP_VAR_T(b_kind) && IS_PRIMARY_VAR_T(a_kind)) return FALSE;

    if(b->ctc == NULL) return bind_same_domain_var_u(a, b, record);
    else               return bind_beneath_var_u(a, b, record);
  }

  /*------------------------------------------------------------*
   * Case where b is not a variable.  If a is an ordinary 	*
   * variable, then bind a to b.  But fail if a cannot be bound	* 
   * restrictively. If a is a wrap-variable, then only do	*
   * the binding if b is a wrapped type or family or is a	*
   * family member.  If a is a primary variable, then only do	*
   * the binding if b is not a wrapped type or family.		*
   *------------------------------------------------------------*/

  else /* b is not a variable */ {

    if(a->norestrict) return FALSE;

    else if(IS_WRAP_VAR_T(a_kind)) {
      if(IS_WRAP_TYPE_T(b_kind)) {
	return bind_var_u(a, b, record);
      }
      else if(b_kind == FAM_MEM_T) {
	return bind_ANY_fam_mem(a, b, record);
      }
      else return FALSE;
    }

    else if(IS_ORDINARY_KNOWN_VAR_T(a_kind)) {
      return bind_var_u(a, b, record);
    }

    else /* IS_PRIMARY_VAR_T(a_kind) */ {
      if(IS_WRAP_TYPE_T(b_kind)) return FALSE;
      else if(b_kind == FAM_MEM_T) {
	return bind_ANY_fam_mem(a, b, record);
      }
      return bind_var_u(a, b, record);
    }
  }
}


/****************************************************************
 *			BIND_TWO_VARS_U				*
 ****************************************************************
 * bind a and b, if possible.  both must be variables, and a	*
 * must not range over ANY.					*
 *								*
 * If record is true, then record bindings.			*
 *								*
 * Return true if the binding succeeded.			*
 ****************************************************************/

PRIVATE Boolean bind_two_vars_u(TYPE *a, TYPE *b, Boolean record)
{
  int ab;
  CLASS_TABLE_CELL* a_ctc  = a->ctc;
  CLASS_TABLE_CELL* b_ctc  = b->ctc;
  TYPE_TAG_TYPE     a_kind = TKIND(a);
  TYPE_TAG_TYPE     b_kind = TKIND(b);

  /*-----------------------------------------------*
   * We cannot bind a primary variable and a wrap  *
   * variable together.				   *
   *-----------------------------------------------*/

  if(IS_WRAP_VAR_T(a_kind) && IS_PRIMARY_VAR_T(b_kind)) return FALSE;
  if(IS_WRAP_VAR_T(b_kind) && IS_PRIMARY_VAR_T(a_kind)) return FALSE;

  /*----------------------------------------------------*
   * If b ranges over ANY, then try to bind it instead. *
   *----------------------------------------------------*/

  if(b_ctc == NULL) {
    return bind_ANY_var_u(b, a, record);
  }

  /*-----------------------------------*
   * Case where Domain(b) = Domain(a). *
   *-----------------------------------*/

  if(a_ctc == b_ctc) {
    return bind_same_domain_var_u(a, b, record);
  }

  /*--------------------------------------------------------*
   * If both a and b range over something other than ANY,   *
   * and the domains are not the same, then we need to      *
   * intersect the domains.				    *
   *--------------------------------------------------------*/

  ab = full_get_intersection_tm(a_ctc, b_ctc);

  /*-------------------------------------*
   * Case of a null intersection.  Fail. *
   *-------------------------------------*/

  if(ab == -1) return FALSE;

  /*------------------------------------*
   * Case where domain(b) <= domain(a). *
   *------------------------------------*/

  if(ab == ctc_num(b_ctc)) {
    return bind_beneath_var_u(a, b, record);
  }   

  /*------------------------------------*
   * Case where domain(a) <= domain(b). *
   *------------------------------------*/

  if(ab == ctc_num(a_ctc)) {
    return bind_beneath_var_u(b, a, record);
  }

  /*----------------------------------------------------*
   * Case where a /\ b = c, where c is neither a nor b. *
   * We must bind both restrictively to a new variable  *
   * or family member or pair.  Note that, if the	*
   * intersection is a non-opaque family member, then   *
   * the intersection is really empty, so fail in that  *
   * case.  Also, if c is a primary species or family   *
   * and either of a or b is wrapped, then we must fail.*
   *----------------------------------------------------*/

  if(a->norestrict || b->norestrict) return FALSE;

  else {
    Boolean result;
    register TYPE *common, *new_t;
    CLASS_TABLE_CELL* ab_ctc = ctcs[ab];
    int common_code = ab_ctc->code;

    if(ab_ctc == Pair_ctc) {
      common = pair_t(copy_type(a, 0), copy_type(b, 0));
    }
    else if(IS_WRAP_VAR_T(a_kind) || IS_WRAP_VAR_T(b_kind)) {
      if(common_code == FAM_ID_CODE || common_code == TYPE_ID_CODE) {
	return FALSE;
      }
      common = wrap_var_t(ab_ctc);
    }
    else if(IS_PRIMARY_VAR_T(a_kind) || IS_PRIMARY_VAR_T(b_kind)) {
      common = primary_tf_or_var_t(ab_ctc);
    }
    else common = tf_or_var_t(ab_ctc);

    if(a_ctc->code == GENUS_ID_CODE && 
       (common_code == COMM_ID_CODE || common_code == FAM_ID_CODE)) {
      if(!ab_ctc->opaque) return FALSE;
      new_t = fam_mem_t(common, var_t(NULL));
    }
    else new_t = common;

    bump_type(new_t);
    result = bind_var_u(a, new_t, record) && bind_var_u(b, new_t, record);
    drop_type(new_t);
    return result;
  }
}


/****************************************************************
 *			BIND_U					*
 ****************************************************************
 * Bind a and b, if possible.  a must be a variable.  		*
 * Note that we will not necessarily bind a directly to b, but  *
 * might instead bind b to a, or bind a and b each to another	*
 * variable or type.						*
 *								*
 * If record is true, then record bindings.			*
 *								*
 * Return true if the binding succeeded, false if it failed.	*
 *								*
 * cylist is for detecting cycles, and is as in basic_unify_u.  *
 ****************************************************************/

Boolean bind_u(TYPE *a, TYPE *b, Boolean record, TYPE_LIST *cylist)
{
  Boolean a_norestrict;
  TYPE_TAG_TYPE b_kind, a_kind;
  CLASS_TABLE_CELL *a_ctc, *b_ctc;

  /*------------------------------------------------------------*
   * If a and b are the same variable, then nothing should	*
   * be done.							*
   *------------------------------------------------------------*/

  b = FIND_U(b);
  if(a == b) return TRUE;

  /*------------------------------------------------------------*
   * If b is a function type and variable a occurs in b, 	*
   * then unification will fail.  We can get it to		*
   * succeed (possibly) by forcing b to be ().  Try that.	*
   *------------------------------------------------------------*/

  b_kind = TKIND(b);
  if(b_kind == FUNCTION_T && b->hermit_f != 0 && occurs_in(a,b)) {
    return force_to_hermit2(a, b, record, cylist);
  }

  /*------------------------------------*
   * Case where a ranges over ANY.	*
   *------------------------------------*/

  a_ctc = a->ctc;
  if(a_ctc == NULL) {
    return bind_ANY_var_u(a, b, record);
  }

  /*----------------------------------------------------*
   * Case where a ranges over something other than ANY.	*
   *----------------------------------------------------*/

  a_kind       = TKIND(a);
  a_norestrict = a->norestrict;

  switch(b_kind) {
    /*-------------------------------------------------------*/
    case FAM_MEM_T:

      /*----------------------------------------------------------*
       * This is similar to the case of pairs, but there is only  *
       * one link label.  There is a slight complication because  *
       * a wrap variable can bind to a family member if the	  *
       * family or community is wrapped.	  		  *
       *----------------------------------------------------------*/

      {int af, lab;
       CLASS_TABLE_CELL* comm_ctc;

       if(a_norestrict) return FALSE;

       af = full_get_intersection_tm(a_ctc, find_u(b->TY2)->ctc);
       if(af == -1) return FALSE;

       if(ctcs[af]->opaque) {
	 lab      = 0;
	 comm_ctc = OPAQUE_ctc;
       }
       else {
         lab      = ctc_num(a_ctc);
         comm_ctc = TRANSPARENT_ctc;
       }
       if(!unify_parts_u(comm_ctc->num, a_kind, &(b->TY2), record) ||
          !unify_parts_u(lab, TYPE_VAR_T, &(b->TY1), record)) {
	 return FALSE;
       }
       return bind_var_u(a, b, record);
      }

    /*-------------------------------------------------------*/
    case FUNCTION_T:
      return FALSE;

    /*-------------------------------------------------------*/
    case PAIR_T:

      /*----------------------------------------------------------*
       * A binding to a structured type is restrictive, and no	  *
       * wrap-variable can be bound to a structured type.	  *
       * To do the binding, we need to check that the appropriate *
       * structured type is a member of the domain of a, and must *
       * also check that the link labels are respected, by doing  *
       * unifications on the parts of the structure type.	  *
       *----------------------------------------------------------*/

       if(a_norestrict || IS_WRAP_VAR_T(a_kind)) return FALSE;
       else if(!ancestor_tm(a_ctc, Pair_ctc) ||
               !unify_parts_u(ctc_num(a_ctc), TYPE_VAR_T, &(b->TY1),record) ||
               !unify_parts_u(ctc_num(a_ctc), TYPE_VAR_T, &(b->TY2),record)) {
	 return FALSE;
       }
       else return bind_var_u(a, b, record);
      
    /*-------------------------------------------------------*/
    case TYPE_ID_T:
    case FAM_T:
    case FICTITIOUS_TYPE_T:
    case FICTITIOUS_FAM_T:
    case WRAP_TYPE_T:
    case WRAP_FAM_T:
    case FICTITIOUS_WRAP_TYPE_T:
    case FICTITIOUS_WRAP_FAM_T:

      /*----------------------------------------------------------------*
       * Binding a to b will surely restrict a, and can only be done if *
       * b is a member of the domain of a.  Also, a wrap-variable can   *
       * only be bound to a wrap-species, and a primary variable can    *
       * only be bound to a primary species.			        *
       *----------------------------------------------------------------*/

       if(a_norestrict) return FALSE;

       b_ctc = b->ctc;
       if(a_ctc != b_ctc && !ancestor_tm(a_ctc, b_ctc)) return FALSE;
       if(IS_KNOWN_WRAP_TYPE_T(b_kind)) {
         if(IS_PRIMARY_VAR_T(a_kind)) return FALSE;
       }
       else {
         if(IS_WRAP_VAR_T(a_kind)) return FALSE;
       }
       return bind_var_u(a, b, record);

    /*-------------------------------------------------------*/
    case TYPE_VAR_T:
    case FAM_VAR_T:
    case PRIMARY_TYPE_VAR_T:
    case PRIMARY_FAM_VAR_T:
    case WRAP_TYPE_VAR_T:
    case WRAP_FAM_VAR_T:
      return bind_two_vars_u(a, b, record);
 
    /*-------------------------------------------------------*/
    default: {}
  }
  return FALSE;
}


/****************************************************************
 *			BASIC_UNIFY_U				*
 ****************************************************************
 * Unify *aa with *bb, without an occur check.  (Bindings that  *
 * would be disallowed by the occur check will be cyclic.)	*
 *								*
 * Null pointers as types are treated as anonymous variables.	*
 *								*
 *			Return value				*
 *								*
 * Return TRUE on successful unification, and FALSE on		*
 * failed unification.  Bindings that are done here are		*
 * not undone.							*
 *								*
 *			record					*
 *								*
 * Record the bindings in new_bindings if record is true.	*
 *								*
 *			functions and ()			*
 *								*
 * This function has special behavior when unifying function	*
 * types.  If an attempt to unify a function type A -> B with	*
 * () is made, and type A -> B is marked as allowed to be	*
 * forced to (), then A and B are unified together, and the	*
 * function type is replaced by ().				*
 *								*
 *			cylist					*
 *								*
 * Parameter cylist holds structured types that have been	*
 * encountered during this unification.  It is used to break    *
 * cycles that were created.  It will not detect all cycles, 	*
 * but just prevents basic_unify_u from getting stuck in a 	*
 * cycle.  							*
 *								*
 *			BASIC_UNIFY macro			*
 *								*
 * Macro BASIC_UNIFY(x,y,r,t), defined in unify.h, does		*
 * basic_unify_u(&(x), &(y), r,t).				*
 ****************************************************************/

Boolean basic_unify_u(TYPE **aa, TYPE **bb, Boolean record, TYPE_LIST *cylist)
{
  TYPE_TAG_TYPE a_kind, b_kind;
  CLASS_TABLE_CELL *a_ctc, *b_ctc;
  TYPE *a, *b;
  Boolean result;

  bump_type(a = *aa);
  bump_type(b = *bb);

# ifdef DEBUG
    if(trace_unify > 1) {
      trace_s(21);
      print_two_types(a, b);
    }
# endif

  /*------------------------------------------------------*
   *** Handle null values. A null value is an anonymous	***
   *** variable.		 			***
   *------------------------------------------------------*/

  if(a == NULL_T) {
    if(b == NULL_T) {
      {register TYPE* c = var_t(NULL);
       set_type(aa, c);
       set_type(bb, c);
      }
      if(record) {
        have_bound_null_u(aa);
        have_bound_null_u(bb);
      }

#     ifdef DEBUG
        if(trace_unify > 1) {
          trace_s(22, aa, bb, *aa);
        }
#     endif
    }

    else {
      set_type(aa, b);
      if(record) have_bound_null_u(aa);

#     ifdef DEBUG
        if(trace_unify > 1) {
	  trace_s(23, aa);
	  trace_ty(b);
	  tracenl();
        }
#     endif

      drop_type(b);
    }
    return TRUE;

  } /* end if(a == NULL_T) */

  if (b == NULL_T) {
    set_type(bb, a);
    if(record) have_bound_null_u(bb);

#   ifdef DEBUG
      if(trace_unify > 1) {
        trace_s(23, bb);
        trace_ty(a);
        tracenl();
      }
#   endif

    drop_type(a);
    return TRUE;

  } /* end if(b == NULL_T) */


  /*------------------------------------------------------*
   *** Handle case where both a and b are nonnull.	***
   *------------------------------------------------------*/

  /*-----------------------------------------------------------*
   * Skip over bound variables, and check for identical nodes. *
   *-----------------------------------------------------------*/

  SET_TYPE(a, FIND_U_NONNULL(a));
  SET_TYPE(b, FIND_U_NONNULL(b));
  if(a == b) return TRUE;

  a_kind = TKIND(a);
  b_kind = TKIND(b);

  /*----------------------------------------------------*
   * Handle the special case where b is a variable.	*
   *----------------------------------------------------*/

  bump_list(cylist);
  if(IS_VAR_T(b_kind)) {
    result = bind_u(b, a, record, cylist);
    goto out;
  }

  /*-----------------------------------------------------------*
   *** Case where both are nonnull, and b is not a variable. ***
   *-----------------------------------------------------------*/

  /*------------------------------------------------------------*
   * Check for cycles from previous bindings.  Since 		*
   * basic_unify_u does not do an occur check, it can create	*
   * cycles, and we don't want basic_unify_u to get caught in a *
   * loop following a cycle. 					*
   *------------------------------------------------------------*/

  if(cylist != NIL) {
    Boolean a_seen = FALSE, 
            b_seen = FALSE;

    if(IS_STRUCTURED_T(a_kind)) {
      if(int_member((LONG) a, cylist)) a_seen = TRUE;
      else SET_LIST(cylist, int_cons((LONG) a, cylist));
    }

    if(IS_STRUCTURED_T(b_kind)) {
      if(int_member((LONG) b, cylist)) b_seen = TRUE;
      else SET_LIST(cylist, int_cons((LONG) b, cylist));
    }
     
    if(a_seen || b_seen) {
#     ifdef DEBUG
        if(trace_unify) {
	  trace_s(25);
	  if(trace_unify > 1) {
	    if(a_seen) long_print_ty(a,1);
	    if(b_seen) long_print_ty(b,1);
	  }
	}
#     endif
      result = FALSE;
      goto out;
    }
  }

  a_ctc  = a->ctc;
  b_ctc  = b->ctc;

  switch (a_kind) {
    
    /*-------------------------------------------------------*/
    case FAM_MEM_T:
    case PAIR_T :
      if (b_kind != a_kind) result = FALSE; 
      else {
        result = BASIC_UNIFY(a->TY2, b->TY2, record, cylist) && 
		 BASIC_UNIFY(a->ty1, b->ty1, record, cylist);
      }
      break;

    /*-------------------------------------------------------*/
    case FUNCTION_T:
      if (b_kind == FUNCTION_T) {
	a->hermit_f = b->hermit_f = (a->hermit_f & b->hermit_f);
	result = BASIC_UNIFY(a->ty1, b->ty1, record, cylist) && 
		 BASIC_UNIFY(a->TY2, b->TY2, record, cylist);
      }
      else if (is_hermit_type(b) && a->hermit_f != 0) {
	result = force_to_hermit(a, record, cylist);
      }
      else result = FALSE;
      break;
      
    /*-------------------------------------------------------*/
    case TYPE_ID_T:
      if (b_kind == TYPE_ID_T) {
	if(a->special && b->special) result = (a->TOFFSET == b->TOFFSET);
	else result = (a_ctc == b_ctc);
      }
      else if(is_hermit_type(a) && 
	      b_kind == FUNCTION_T && b->hermit_f != 0) {
	result = force_to_hermit(b, record, cylist);
      }
      else result = FALSE;
      break;

    /*-------------------------------------------------------*/
    case FAM_T:
    case WRAP_TYPE_T:
    case WRAP_FAM_T:
      if (b_kind != a_kind) result = FALSE;
      else if(a->special && b->special) result = (a->TOFFSET == b->TOFFSET);
      else result = (a_ctc == b_ctc);
      break;

    case FICTITIOUS_TYPE_T:
    case FICTITIOUS_FAM_T:
    case FICTITIOUS_WRAP_TYPE_T:
    case FICTITIOUS_WRAP_FAM_T:
      if(b_kind != a_kind) result = FALSE;
      else result = (a->ctc == b->ctc) && (a->TNUM == b->TNUM);
      break;

    /*-------------------------------------------------------*/
    case TYPE_VAR_T:
    case FAM_VAR_T:
    case PRIMARY_TYPE_VAR_T:
    case PRIMARY_FAM_VAR_T:
    case WRAP_TYPE_VAR_T:
    case WRAP_FAM_VAR_T:
      result = bind_u(a, b, record, cylist);
      break;

    /*-------------------------------------------------------*/
    case BAD_T:
      result = FALSE;
      break;

    /*-------------------------------------------------------*/
    default: 
	die(11, toint(TKIND(a)));
	result = FALSE;
  }

 out:

# ifdef DEBUG
    if((trace_unify) && !result) {
      trace_s(26); 
      print_two_types(a, b);
    }
    if(trace_unify > 1 && result) {
      trace_s(27);
      trace_ty(a);
      tracenl();
    }
# endif

  drop_list(cylist);
  drop_type(a);
  drop_type(b);
  return result;
}


/****************************************************************
 *			ACYCLIC_U				*
 ****************************************************************
 * Returns TRUE if type t is acyclic.  This fills in for	*
 * the occur check.						*
 ****************************************************************/

PRIVATE Boolean basic_acyclic_u	(TYPE *t);

PRIVATE Boolean acyclic_u(TYPE *t) 
{
  Boolean result;

  result = basic_acyclic_u(t);
  reset_seen();
  return result;
}


/****************************************************************
 *			BASIC_ACYCLIC_U				*
 ****************************************************************
 * Returns true if t is acyclic.  The seen fields are used	*
 * to detect cycles.						*
 ****************************************************************/

PRIVATE Boolean basic_acyclic_u(TYPE *t)
{
  Boolean ok;
  int seen;

  /*----------------------------*
   * A null type has no cycles. *
   *----------------------------*/

  if(t == NULL_T) return TRUE;

  /*--------------------------------------------------------*
   * If this node has no variables, then there is no cycle. *
   *--------------------------------------------------------*/

  if(t->copy == 0) return TRUE;

  /*--------------------------------------------*
   * If this node is finished already, skip it. *
   * A node has its seen field set to 2 when it *
   * is finished.				*
   *--------------------------------------------*/

  if((seen = t->seen) == 2) return TRUE;

  /*--------------------------------------------------------------*
   * If this node is currently being processed, there is a cycle. *
   *--------------------------------------------------------------*/

  if(seen == 1) return FALSE;

  /*--------------------*
   * Process this node. *
   *--------------------*/

  mark_seen(t);
  ok = TRUE;     
  switch(TKIND(t)) {
    /*-------------------------------------------------------*/
    case FAM_MEM_T:
      ok = basic_acyclic_u(t->ty1);
      break;

    /*-------------------------------------------------------*/
    case FUNCTION_T:
    case PAIR_T:
      /*----------------------------------------------------*
       * Do not reorder && below.  We must scan the entire  *
       * data structure to set the seen fields.             *
       *----------------------------------------------------*/

      ok = basic_acyclic_u(t->ty1);
      ok = basic_acyclic_u(t->TY2) && ok;
      break;

    /*-------------------------------------------------------*/
    case MARK_T:
    case TYPE_VAR_T:
    case FAM_VAR_T:
    case PRIMARY_TYPE_VAR_T:
    case PRIMARY_FAM_VAR_T:
    case WRAP_TYPE_VAR_T:
    case WRAP_FAM_VAR_T:
      if(t->ty1 != NULL_T) ok = basic_acyclic_u(t->ty1);
      break;
  
    /*-------------------------------------------------------*/
    default:
      break;

  }

  t->seen = 2;
  return ok;
}


/****************************************************************
 *			UNIFY_U					*
 ****************************************************************
 * Unify *a with *b.  If record is true, then record bindings	*
 * in new_bindings.						*
 *								*
 * If unification succeeds, the unify_u returns TRUE, and the	*
 * bindings that were done are left in place.  If unification	*
 * fails, the all bindings that were done are undone, and	*
 * unify_u returns FALSE.					*
 *								*
 * See the remark under basic_unify_u about special behavior	*
 * for function types.						*
 ****************************************************************/

Boolean unify_u(TYPE **a, TYPE **b, Boolean record)
{
  LIST *mark;
  Boolean old_unify_seen_constraints;

# ifdef DEBUG
    if(trace_unify) {
      trace_s(30, *a, *b, record);
      print_two_types(*a, *b);
    }
# endif

  /*---------------------------------------*
   * Mark for undoing bindings on failure. *
   *---------------------------------------*/

  if(!use_binding_list) {
    bump_list(mark = new_bindings);
  }
  else {
    bump_list(mark = binding_list);
  }

  /*---------------------------------------------------------*
   * Perform unification without occur check. If unification *
   * fails, then undo the bindings and return FALSE.         *
   *---------------------------------------------------------*/

  old_unify_seen_constraints = unify_seen_constraints;
  unify_seen_constraints     = FALSE;
  if (!basic_unify_u(a, b, !use_binding_list, NIL)) {
#   ifdef DEBUG
      if(trace_unify) trace_s(31);
#   endif
    goto failout;  /* at bottom */
  }

  /*----------------------------------------------------*
   * Get rid of constraint cycles that do not go though *
   * structured lower bounds. 				*
   *----------------------------------------------------*/

  if(unify_seen_constraints) {
    if(!unify_cycles(*a, TRUE)) {
#     ifdef DEBUG
        if(trace_unify) trace_s(17);
#     endif
      goto failout;   /* at bottom */
    }
  }

  /*------------------------------------------------------------*
   * Perform the occur check. If a cycle has been generated, 	*
   * then the occur check fails, and we need to undo the     	*
   * bindings and return FALSE.					*
   *------------------------------------------------------------*/

  if (!acyclic_u(*a)) {
#   ifdef DEBUG
      if(trace_unify) {
        trace_s(32);
        trace_ty(*a);
        tracenl();
      }
#   endif
    goto failout;   /* at bottom */
  }

  /*---------------------------*
   * Clean up the constraints. *
   *---------------------------*/

  if(unify_seen_constraints) {
    remove_redundant_constraints(*a, record);
  }

# ifdef DEBUG
    if(trace_unify) {
      trace_s(33);
      trace_ty(*a);
      tracenl();
    }
# endif

  /*-----------------------------------------------------------*
   * We had basic_unify_u record bindings so that we could     *
   * undo them at a failure.  If we are not asked to record    *
   * bindings, now is the time to throw away what was recorded. *
   *-----------------------------------------------------------*/

  if(!use_binding_list) {
    if(!record) SET_LIST(new_bindings, mark);
  }
  drop_list(mark);
  unify_seen_constraints = old_unify_seen_constraints;
  return TRUE;

failout:
  if(use_binding_list) {
    SET_LIST(binding_list, mark);
  }
  else {
    undo_bindings_u(mark);
  }
  drop_list(mark);
  unify_seen_constraints = old_unify_seen_constraints;
  return FALSE;
}


/****************************************************************
 *			IS_MEMBER_TF				*
 ****************************************************************
 * Return true if species or family t is a member of the genus	*
 * or community described by class table entry ctc.		*
 *								*
 * This doesn't really belong here, except that it uses 	*
 * unification binding to do the job.				*
 ****************************************************************/

Boolean is_member_tf(TYPE *t, CLASS_TABLE_CELL *ctc)
{
  TYPE *V;
  Boolean result;

  bump_type(t = copy_type(t, 0));
  bump_type(V = tf_or_var_t(ctc));
  result = bind_u(V, t, FALSE, NULL);
  drop_type(V);
  drop_type(t);
  return result;
}


/****************************************************************
 *			DISJOINT				*
 ****************************************************************
 * Return true just when types A and B are disjoint.		*
 ****************************************************************/

Boolean disjoint(TYPE *A, TYPE *B)
{
  TYPE *acpy, *bcpy;
  Boolean result;

  if(A == NULL || B == NULL) return FALSE;

  bump_type(acpy = copy_type(A, 0));
  replace_null_vars(&acpy);
  bump_type(bcpy = copy_type(B, 0));
  replace_null_vars(&bcpy);

# ifdef DEBUG
    if(trace_overlap) trace_s(97);
# endif

  should_check_primary_constraints = FALSE;
  result = !UNIFY(acpy, bcpy, 0);
  should_check_primary_constraints = TRUE;
  drop_type(acpy);
  drop_type(bcpy);  

# ifdef DEBUG
    if(trace_overlap) trace_s(98, result);
# endif

  return result;
}


