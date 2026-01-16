/**************************************************************
 * File:    unify/constraint.c
 * Purpose: Handling of constraints in types
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

/************************************************************************
 * This file provides operations for installing and checking		*
 * constraints on variables and general types.				*
 ************************************************************************/

#include "../misc/misc.h"
#include "../parser/tokens.h"
#include "../utils/lists.h"
#include "../utils/hash.h"
#include "../classes/classes.h"
#include "../clstbl/classtbl.h"
#include "../clstbl/meettbl.h"
#include "../standard/stdtypes.h"
#include "../unify/unify.h"
#include "../unify/constraint.h"
#include "../error/error.h"
#include "../alloc/allocate.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif


/****************************************************************
 *		    should_check_primary_constraints		*
 ****************************************************************
 * When should_check_primary_constraints is TRUE, >>= 		*
 * constraints on primary variables are checked for		*
 * consistency with other constraints.  No such check		*
 * is done when should_check_primary_constraints is FALSE.	*
 ****************************************************************/

Boolean should_check_primary_constraints = TRUE;

/****************************************************************
 *			LOWER_BOUND_LIST			*
 ****************************************************************
 * Return the LOWER_BOUNDS list of variable V.  It is obtained  *
 * from the LOWER_BOUNDS field of V, or from the binding_list,  *
 * depending on which is currently being used for bindings.	*
 ****************************************************************/

TYPE_LIST* lower_bound_list(TYPE *V)
{
  register LIST *p;

  if(!use_binding_list) return V->LOWER_BOUNDS;

  /*----------------------------------------------------------*
   * When use_binding_list is in effect, find the most        *
   * recent binding of the lower bounds in binding_list.  If  *
   * there are none, then take the LOWER_BOUNDS field of V.   *
   *----------------------------------------------------------*/

  for(p = binding_list; p != NIL; p = p->tail) {
    if(LKIND(p) == LIST_L) {
      register TYPE_LIST* w = p->head.list;
      if(w->head.type == V) return w->tail;
    }
  }
  return V->LOWER_BOUNDS;
}


/****************************************************************
 *			INSTALL_LWBS				*
 ****************************************************************
 * Install new_lb as the new lower bound list of variable V.    *
 * old_lb is the current lower bound list.  If new_lb = old_lb, *
 * do not change anything.					*
 *								*
 * Record bindings if record is true.				*
 ****************************************************************/

PRIVATE void
install_lwbs(TYPE *V, TYPE_LIST *old_lb, TYPE_LIST *new_lb, Boolean record)
{
  if(new_lb != old_lb) {
    if(use_binding_list) {
      SET_LIST(binding_list, 
	       list_cons(type_cons(V, new_lb), 
			 binding_list));
    }
    else {
      if(record) have_replaced_lower_bounds(V, old_lb);
      SET_LIST(V->LOWER_BOUNDS, new_lb);
    }
  }
}


/****************************************************************
 *			 ATTACH_LOWER_BOUND			*
 ****************************************************************
 * Install lower bound lb into variable v, if it is not		*
 * already a known lower bound.	 This function does not check   *
 * for consistency of the lower bound, it just installs it	*
 * blindly.							*
 *								*
 * If record is true, then changes to lower bounds are recorded *
 * for later undoing.						*
 *								*
 * If gg is true, this is a >>=  lower bound.  Otherwise, 	*
 * it is a >= lower bound.					*
 ****************************************************************/

PRIVATE void 
attach_lower_bound(TYPE *v, TYPE *lb, Boolean record, Boolean gg)
{
  int tag = gg ? TYPE1_L : TYPE_L;
  TYPE_TAG_TYPE v_kind = TKIND(v);
  TYPE_LIST*    v_lwbs = lower_bound_list(v);

  if(!type_mem(lb, v_lwbs, tag)) {
    HEAD_TYPE hd;

    /*---------------------------*
     * Install this lower bound. *
     *---------------------------*/

    hd.type = lb;
    install_lwbs(v, v_lwbs, general_cons(hd, v_lwbs, tag), record);

    /*----------------------------------------------------------*
     * If v is a primary variable and lb is a variable, then    *
     * install a back pointer into the lower bound list for lb. *
     * The back pointer allows us to check constraint v >= lb   *
     * when lb becomes bound later.				*
     *----------------------------------------------------------*/

    if(IS_PRIMARY_VAR_T(v_kind) && IS_VAR_T(TKIND(lb))) {
      TYPE_LIST* lb_lwbs = lower_bound_list(lb);
      hd.type = v;
      install_lwbs(lb, lb_lwbs, general_cons(hd, lb_lwbs, TYPE2_L), record);
    }
  }
}


/****************************************************************
 *			 REMOVE_CONSTRAINT			*
 ****************************************************************
 * Remove lower bound lb from the lower bound list of ub, if    *
 * it is there.  Members are checked using full_type_equal.	*
 ****************************************************************/

PRIVATE TYPE_LIST* 
remove_type(TYPE *t, TYPE_LIST *L)
{
  if(L == NIL) return NIL;
  else if(full_type_equal(t, L->head.type)) return L->tail;
  else {
    TYPE_LIST* r = remove_type(t, L->tail);
    if(r == L->tail) return L;
    else return type_cons(L->head.type, r);
  }
}

/*-----------------------------------------------------------------*/

PRIVATE void 
remove_constraint(TYPE *ub, TYPE *lb, Boolean record)
{
  TYPE_LIST* old_lbs = lower_bound_list(ub);
  TYPE_LIST* new_lbs = remove_type(lb, old_lbs);
  if(new_lbs != old_lbs) {
    install_lwbs(ub, old_lbs, new_lbs, record);
  }
}


/****************************************************************
 *			 REDUCE_PRIMARY_VAR			*
 ****************************************************************
 * Primary variable ub has just received >= lower bound lb.	*
 * Perform unifications that are called for.  They are as 	*
 * follows, if ub = X.						*
 *								* 
 *  X >= (A,B)    => X = (U,V), U >= A, V >= B			*
 *								* 
 *  X >= (A -> B) => X = U -> V, A >= U, V >= B			*
 *								* 
 *  X >= F(A)     => X = U(V), U >= F, V >= A (U ranges over 	*
 *                   OPAQUE or TRANSPARENT)			*
 *								* 
 *  X >= T        => X = T (t a simple species or family)	*
 *								* 
 *  X >= V        => Get two new primary variables Y and Z,     *
 *		     each of whose domains is the intersection  *
 *		     of the domains of X and V. Unify X with Y  *
 *		     and V with Z.				*
 *								*
 * Record bindings if record is true.				*
 * 								*
 * Return true on success, false on failure.			*
 ****************************************************************/

PRIVATE Boolean 
reduce_primary_var(TYPE *ub, TYPE *lb, Boolean record)
{
  TYPE_TAG_TYPE lb_kind;

  lb      = find_u(lb);
  lb_kind = TKIND(lb);
  switch(lb_kind) {
    case FAM_MEM_T:
      {TYPE *v1, *v2, *val, *fam;
       Boolean result;
       fam = lb->TY2;
       if(fam->ctc->opaque) {
         v1 = var_t(NULL);
         v2 = var_t(OPAQUE_ctc);
       }
       else {
         v1 = var_t(ub->ctc);
         v2 = var_t(TRANSPARENT_ctc);
       }
       bump_type(v1);
       bump_type(v2);
       if(!CONSTRAIN(v1, lb->TY1, record, 0)) result = FALSE;
       else if(!CONSTRAIN(v2, fam, record, 0)) result = FALSE;
       else {
         bump_type(val = fam_mem_t(v2, v1));
         result = UNIFY(ub, val, record);
         drop_type(val);
       }
       drop_type(v1);
       drop_type(v2);
       return result;
      }

    case FUNCTION_T:
      {TYPE *v1, *v2, *val;
       Boolean result;
       if(ub->ctc != NULL) return FALSE;
       bump_type(v1 = var_t(NULL));
       bump_type(v2 = var_t(NULL));
       if(!CONSTRAIN(lb->TY1, v1, record, 0)) result = FALSE;
       else if(!CONSTRAIN(v2, lb->TY2, record, 0)) result = FALSE;
       else {
         bump_type(val = function_t(v1, v2));
         result = UNIFY(ub, val, record);
         drop_type(val);
       }
       drop_type(v1);
       drop_type(v2);
       return result;
      }

    case PAIR_T:
      {TYPE *v1, *v2, *val;
       Boolean result;
       bump_type(v1 = var_t(ub->ctc));
       bump_type(v2 = var_t(ub->ctc));
       if(!CONSTRAIN(v1, lb->TY1, record, 0)) result = FALSE;
       else if(!CONSTRAIN(v2, lb->TY2, record, 0)) result = FALSE;
       else {
         bump_type(val = pair_t(v1, v2));
         result = UNIFY(ub, val, record);
         drop_type(val);
       }
       drop_type(v1);
       drop_type(v2);
       return result;
      }

    case TYPE_ID_T:
    case FAM_T:
    case FICTITIOUS_TYPE_T:
    case FICTITIOUS_FAM_T:
      return UNIFY(ub, lb, record);

    case WRAP_TYPE_T:
    case WRAP_FAM_T:
    case FICTITIOUS_WRAP_TYPE_T:
    case FICTITIOUS_WRAP_FAM_T:
      return FALSE;

    case TYPE_VAR_T:
    case FAM_VAR_T:
    case PRIMARY_TYPE_VAR_T:
    case PRIMARY_FAM_VAR_T:
      {TYPE *v;
       Boolean result;
       CLASS_TABLE_CELL* ctc = intersection_ctc_tm(ub->ctc, lb->ctc);

       if(ctc == NULL) result = FALSE;
       else {
	 bump_type(v = primary_var_t(ctc));
	 result = UNIFY(lb, v, record);
	 drop_type(v);
	 if(result && ctc != ub->ctc) {
	   bump_type(v = primary_var_t(ctc));
	   result = UNIFY(ub, v, record);
	   drop_type(v);
	 }
       }
       return result;
      }

    case WRAP_TYPE_VAR_T:
    case WRAP_FAM_VAR_T:
      return FALSE;

    default: 
      return TRUE;
  }
}
 

/****************************************************************
 *		   BIND_PRIMARY_TO_STRUCTURED			*
 ****************************************************************
 * We are adding a constraint X >= Y or X >>= Y, where X is a   *
 * primary variable and Y is a family or family variable or the *
 * special family PAIR.	 Parameter gg is 1 for a >>= constraint *
 * and 0 for a >= constraint.					*
 *								*
 * For constraint X >= Y:					*
 *								*
 * If Y is PAIR, then check that PAIR is in the domain of X,    *
 * and bind X to a pair (U,V), where U and V are new variables. *
 *								*
 * If Y is a family or family variable, bind X to F(V) where    *
 * F and V are variables.					*
 *								*
 * For constraint X >>= Y:					*
 *								*
 * If Y is PAIR, then check that PAIR is in the domain of X.	* 
 * That is done by unifying pair (U,V) with a new variable Z    *
 * that has the same domain as X.				*
 *								*
 * If Y is a family or family variable F, then bind F(V) to a   *
 * new variable that has the same domain as X, where V is a	*
 * new variable.						* 
 *								*
 * Record bindings if record is true.				*
 *								*
 * Return true on success, false on failure.			*
 ****************************************************************/

PRIVATE Boolean 
bind_primary_to_structured(TYPE *X, TYPE *Y, Boolean record, Boolean gg)
{
  TYPE *U, *V, *p;
  Boolean result;

  V = var_t(NULL);
  if(Y->ctc == Pair_ctc) {
    U = var_t(NULL);
    p = pair_t(U,V);
  }
  else {
    U = gg                     ? Y :
        var_t((Y->ctc->opaque) ? OPAQUE_ctc 
                               : TRANSPARENT_ctc);
    p = fam_mem_t(U,V);
  }
  bump_type(U);
  bump_type(p);

  if(!gg) {
    result = UNIFY(X, p, record);
  }
  else {
    TYPE* Z;
    bump_type(Z = var_t(X->ctc));
    result = UNIFY(Z, p, record);
    drop_type(Z);
  }
  
  drop_type(U);
  drop_type(p);
  return result;
}


/****************************************************************
 *		   CHECK_CONSTRAINT				*
 ****************************************************************
 * ub must be a variable or type or family.			*
 *								*
 * Check that constraint ub >= lb or ub >>= lb is consistent by *
 * performing required unifications.  Parameter gg is 1 for	*
 * a >>= constraint, and 0 for a >= constraint.			*
 *								*
 * The actions are as follows.					*
 *								*
 *  (1) If ub is an ordinary or secondary variable with domain  *
 *      D, then create a new ordinary variable X that has the   *
 *      same domain D as ub, and unify X with lb.  This ensures *
 *      that, no matter what lb is bound to in the future, as   *
 *      long as ub does not receive any more bindings, we can   *
 *      satisfy this inequality by binding ub to D (a secondary *
 *      species).						*
 *								*
 *  (2) If ub is a primary variable then checking consistency   *
 *      is a little more involved, because the option of	*
 *      binding ub to its domain D is not available.  If        *
 *      a primary variable P has two constraints P >= X and     *
 *      P >= Y, then subsequent bindings of X and Y to		*
 *      different species render these inequalities inconsistent*
 *      with one another, even though variable P does not       *
 *      receive any bindings.  Also, the rules for how to	*
 *      handle lower bounds on primary variables are a little   *
 *      different.  Here is how these two problems are dealt    *
 *      with.							*
 *								*
 *      (a) If X >= Y is a constraint, where X and Y are        *
 *          primary, then X is placed in the lower bound        *
 *          list of Y with tag TYPE2_L, indicating a back	*
 *	    pointer.  This back pointer makes it possible to    *
 *	    check constraint X >= Y when Y is bound.  (Note:    *
 *	    due to the rules from part (b) below, when X is     *
 *	    primary, Y will necessarily be primary as well.)	*
 *								*
 *      (b) When X >= Y is checked here, with X primary, the	*
 *	    intersection I of the domains of X and Y is found,  *
 *	    and two new variables U and V, each with domain I,  *
 *	    are created.  X is unified with U, and Y is unified *
 *	    with V.  This can be done because X >= Y can only   *
 *	    be satisfied when X is primary if either X = Y or   *
 *          both X and Y are the same kind of structured type.  *
 *          In either case, restricting both to their the	*
 *	    intersection of their domains does not hurt.	*
 *								*
 *      (c) Constraints of the form X >>= Y, with X primary,    *
 *          are handled differently.  Here, just unify Y with   *
 *          a variable whose domain is the same as the domain	*
 *	    of X.						*
 *								*
 *  3. If ub is a species, then					*
 *       (a) If ub is primary, unify ub with lb.  They must be  *
 *           equal in this case.				*
 *								*
 *	 (b) If ub is secondary, then unify lb with a variable  *
 *	     whose domain is ub.				*
 *								*
 * There is a special case where ub is a species or species	*
 * variable and lb is a family or family variable or special    *
 * family indicating PAIR.  In this case, check that this	*
 * constraint can be satisfied, without doing any unifications  *
 * that unify a species with a family.  (Such unifications are  *
 * not allowed.)						*
 *								*
 * If record is true, record any bindings that are done in the	*
 * unification.							*
 *								*
 * Return true on success, false on failure.			*
 ****************************************************************/

PRIVATE Boolean 
check_constraint(TYPE *ub, TYPE *lb, Boolean record, Boolean gg)
{
  TYPE *new_var;
  Boolean result, ub_is_primary;
  TYPE_TAG_TYPE lb_kind, ub_kind;

# ifdef DEBUG
    if(trace_unify > 1) {
      trace_s(108, gg);
      trace_ty(ub);
      trace_s(109);
      trace_ty(lb);
      tracenl();
    }
# endif

  IN_PLACE_FIND_U_NONNULL(lb);
  IN_PLACE_FIND_U_NONNULL(ub);
  lb_kind = TKIND(lb);
  ub_kind = TKIND(ub);
  ub_is_primary = IS_PRIMARY_T(ub_kind);
  bump_type(lb);

  /*------------------------------------------------------------*
   * Handle the special case where ub is a species or species	*
   * variable and lb is a family or family variable or the	*
   * special family PAIR.					*
   *								*
   *   (1) If ub is a primary variable, then this can only be   *
   *       satisfied if ub is a family member (or, when the     *
   *       lower bound is PAIR, when ub is a pair).  So just    *
   *       bind ub.						*
   *								*
   *   (2) When ub is not a primary varible, do one of the      *
   *       following.						*
   *								*
   *       (a) In the case where lb is a family (or PAIR), just *
   *           check that ub's domain contains that family (or  *
   *           PAIR).						*
   *								*
   *       (b) In the case where lb is a family variable,	*
   *           replace lb by an appropriately constructed type  *
   *           and proceed to the general case below.		*
   *------------------------------------------------------------*/

  if(!IS_FAM_OR_FAM_VAR_T(ub_kind) && IS_FAM_OR_FAM_VAR_T(lb_kind)) {
    if(ub_is_primary) {
      result = bind_primary_to_structured(ub, lb, record, gg);
      goto out;
    }
    else if(IS_FAM_T(lb_kind)) {
      result = ancestor_tm(ub->ctc, lb->ctc);
      goto out;
    }
    else {
      SET_TYPE(lb, fam_mem_t(lb, NULL));
    }
  }

  /*------------------------------------------------------------*
   * When ub is a primary species or family, unify ub with lb.  *
   *------------------------------------------------------------*/

  if(IS_PRIMARY_TYPE_T(ub_kind)) {
    result = UNIFY(ub, lb, record);
  }

  /*------------------------------------------------------------*
   * A primary upper bound variable with a >= lower bound is	*
   * handled by reduce_primary_var, above.			*
   *------------------------------------------------------------*/

  if(ub_is_primary && !gg) {
    result = reduce_primary_var(ub, lb, record);
  }

  /*------------------------------------------------------------*
   * Perform the unification of lb with a variable.		*
   *								*
   * The unification will have no effect, and we can skip it,   *
   * if lb is a variable or wrap species or family with the same*
   * domain as ub, as long as ub is not a fictitious species or *
   * family.  There are other cases that will have no effect,	*
   * but they are less easy to test for.			*
   *------------------------------------------------------------*/

  else if(!IS_VAR_OR_WRAP_TYPE_T(lb_kind) || lb->ctc != ub->ctc) {
    bump_type(new_var = var_t(ub->ctc));
    result = UNIFY(new_var, lb, record);
    drop_type(new_var);
    if(!result) {
#     ifdef DEBUG
        if(trace_unify > 1) trace_s(18);
#     endif
    }
  }

  else result = TRUE;

 out:

# ifdef DEBUG
    if(trace_unify > 1) trace_s(110, result);
# endif

  drop_type(lb);
  return result;
}


/****************************************************************
 *			 CHECK_PRIMARY_CONSTRAINTS		*
 ****************************************************************
 * V is a primary variable.  					*
 *								*
 * If V has two or more >>= constraints, return FALSE.  This 	*
 * indicates that there are two different places in the         *
 * program where to (possibly different) secondary items are    *
 * unwrapped, and both need their intrinsic species to be V.	*
 * The second one might fail, since the intrinsic species of	*
 * two secondary items might be different.			*
 *								*
 * If V has a >>= constraint and a >= constraint (V >= U), then *
 * unify V and U, and return FALSE if the unification fails.	*
 * This is because the constraint on V might prevent V from	*
 * binding to what its >>= lower bound unwraps to, unless V = U,*
 * so that the constraint is surely satisfied.			*
 *								*
 * Record bindings if record is true.				*
 *								*
 * Return TRUE if no problems are detected with the 		*
 * constraints on V.						*
 ****************************************************************/

PRIVATE Boolean 
check_primary_constraints(TYPE *V, Boolean record)
{
  TYPE_LIST *p;
  TYPE *gg_lwb;
  int gg_count;
  TYPE_LIST* lwbs = lower_bound_list(V);

  /*--------------------------------------------*
   * Count the number of >>= constraints on V.	*
   * gg_lwb keeps track of the first >>= lower	*
   * bound seen.  The only important values 	*
   * for gg_count are 0, 1 or >1.		*
   *--------------------------------------------*/

  gg_lwb = NULL;
  gg_count = 0;
  for(p = lwbs; p != NIL; p = p->tail) {
    if(LKIND(p) == TYPE1_L) {
      if(gg_lwb == NULL) {
	gg_lwb = find_u(p->head.type);
	gg_count++;
      }
      else if(!full_type_equal(p->head.type, gg_lwb)) {
	gg_count++;
      }
    }
  }

  /*---------------------------------------------------------*
   * If there are two or more >>= constraints, return FALSE. *
   * If there is exactly one >>= constraint, the find all of *
   * the >= constraints and unify their sides together.      *
   *---------------------------------------------------------*/

  if(gg_count == 0) return TRUE;
  if(gg_count >= 2) return FALSE;
  for(p = lwbs; p != NIL; p = p->tail) {
    if(LKIND(p) == TYPE_L) {
      if(!UNIFY(V, p->head.type, record)) return FALSE;
    }
  }
  return TRUE;
}


/****************************************************************
 *			 FAM_FOR_CONSTRAINT			*
 ****************************************************************
 * Suppose constraint F(A) >= F'(B) must be enforced.  To do so,*
 * it is necessary to find a family or family variable X to     *
 * unify with F'.  This function returns such an X.  The 	*
 * parameter fam is F.						*
 ****************************************************************/

PRIVATE TYPE* fam_for_constraint(TYPE *fam)
{
  TYPE_TAG_TYPE fam_kind;

  IN_PLACE_FIND_U_NONNULL(fam);
  fam_kind = TKIND(fam);
  return IS_PRIMARY_TYPE_T(fam_kind) ? fam : var_t(fam->ctc);
}


/****************************************************************
 *			 CONSTRAIN				*
 ****************************************************************
 * Install constraint ub >= lb or ub >>= lb, where ub is *ubp   *
 * and lb is *lbp.  The pointers ubp and lbp are only used to   *
 * replace null variables by real variables, if necessary.	*
 * If a null is replaced, the reference count is bumped.	*
 *								*
 * If gg is true, this is a >>= type lower bound.  Otherwise, 	*
 * it is a >= type lower bound.					*
 *								*
 * In the process of installing this constraint, bindings are   *
 * done to ensure that any future bindings that only restrict   *
 * lb cannot invalidate the constraint.  Later bindings that    *
 * affect ub will need to be checked when they are done.	*
 *								*
 * Record the bindings if record is true, for later unbinding.	*
 *								*
 * Return TRUE on success, FALSE on failure.  (It fails if	*
 * there are no bindings of variables that would satisfy this   *
 * constraint, and that are consistent with prior information.)	*
 *								*
 * Macro CONSTRAIN is defined in constraint.h so that		*
 * CONSTRAIN(ub, lb, r, gg) = constrain(&(ub), &(lb), r, gg). 	*
 ****************************************************************/

Boolean constrain(TYPE **ubp, TYPE **lbp, Boolean record, Boolean gg)
{
  Boolean result = FALSE;          /* default */
  TYPE_TAG_TYPE ub_kind, lb_kind;
  TYPE* ub = *ubp;
  TYPE* lb = *lbp;

  /*------------------------------------------------------*
   * There must be no drops below for these bumps, since  *
   * they replace NULLs.				  *
   *------------------------------------------------------*/

  if(ub == NULL) bump_type(ub = *ubp = var_t(NULL));
  if(lb == NULL) bump_type(lb = *lbp = var_t(NULL));

# ifdef DEBUG
    if(trace_unify > 1 ) {
      trace_s(4, gg);
      print_two_types(ub, lb);
    }
# endif

  IN_PLACE_FIND_U_NONNULL(lb);
  IN_PLACE_FIND_U_NONNULL(ub);
  ub_kind = TKIND(ub);
  lb_kind = TKIND(lb);

  switch(ub_kind) {

    /*---------------------------------------------*/
    case FAM_MEM_T:

     /*---------------------------------------------------------*
      * Here ub has the form F(A), where F is a family		*
      * or family variable.  We need lb to be a family member 	*
      * F'(B), where F >= F' and 				*
      *   (1) (F transparent) A >= B or				*
      *   (2) (F opaque)      A = B	  			*
      *---------------------------------------------------------*/

     {Boolean opaque;
      TYPE* new_fam = fam_for_constraint(ub->TY2);
      TYPE* new_t   = fam_mem_t(new_fam, var_t(NULL));
      bump_type(new_t);
      opaque = new_fam->ctc->opaque;

      if(!UNIFY(new_t, lb, record)) {
	drop_type(new_t);
        /* result is false */
      }
      else {
        IN_PLACE_FIND_U_NONNULL(lb);
        drop_type(new_t);
        result = opaque
	          ? UNIFY(ub->TY1, lb->TY1, record) 
                  : CONSTRAIN(ub->TY1, lb->TY1, record, gg);
      }
      break;
     } 

    /*---------------------------------------------*/
    case FUNCTION_T:
    case PAIR_T:

      /*---------------------------------------------------------*
       * First make sure that lb is of the same kind as ub.      *
       * We can only handle (A,B) >= (C,D) or 			 *
       * (A -> B) >= (C -> D).					 *
       *---------------------------------------------------------*/

      if(lb_kind != ub_kind) {
	TYPE* structured = new_type2(ub_kind, var_t(NULL), var_t(NULL));
        bump_type(structured);
	if(!UNIFY(lb, structured, record)) {
	  drop_type(structured);
          break;   /* return FALSE */
	}
        IN_PLACE_FIND_U_NONNULL(lb);
	drop_type(structured);
      }

      /*--------------------------------------------------------*
       * Now do constraints on the components. 			*
       *							*
       * (A,B) >= (C,D) is the same as A >= C and B >= D.	*
       *							*
       * (A -> B) >= (C -> D) is the same as C >= A and B >= D. *
       * Notice the transposition in the domains, which is 	*
       * required.						*
       *--------------------------------------------------------*/

      if(ub_kind == PAIR_T) {
	if(!CONSTRAIN(ub->TY1, lb->TY1, record, gg)) {
	  break;  /* return FALSE */
	}
      }
      else {
	if(!CONSTRAIN(lb->TY1, ub->TY1, record, gg)) {
          break;  /* return FALSE */
        }
      }
      if(!CONSTRAIN(ub->TY2, lb->TY2, record, gg)) {
        break;  /* return FALSE */
      }
      result = TRUE;
      break;

    /*---------------------------------------------*/
    case TYPE_ID_T:
    case FAM_T:
    case FICTITIOUS_TYPE_T:
    case FICTITIOUS_FAM_T:
    case WRAP_TYPE_T:
    case WRAP_FAM_T:
    case FICTITIOUS_WRAP_TYPE_T:
    case FICTITIOUS_WRAP_FAM_T:
      result = check_constraint(ub, lb, record, gg);
      break;

    /*---------------------------------------------*/
    case TYPE_VAR_T:
    case FAM_VAR_T:
      if(!check_constraint(ub, lb, record, gg)) {
	break;  /* Return false */
      }
      attach_lower_bound(ub, lb, record, gg);
      result = TRUE;
      break;

    case WRAP_TYPE_VAR_T:
    case WRAP_FAM_VAR_T:

      /*------------------------------------------------------------------*
       * When the upper bound is a secondary variable, structured	  *
       * lower bounds can be eliminated in favor of a collection	  *
       * of simpler lower bounds.					  *
       *								  *
       *  V >= (A,B)    becomes  V >= PAIR, V >= A, V >= B		  *
       *								  *
       *  V >= (A -> B) becomes  V = ANY				  *
       *								  *
       *  V >= F(A)     becomes  V >= F, V >= A   (when F is transparent) *
       *								  *
       *  V >= F(A)     becomes  V >= F           (when F is opaque)	  *
       *								  *
       * This is only done for >= constraints, not >>= constraints.	  *
       * (This simplification is required by the overlap tester, since it *
       * is used to get rid of structured lower bounds.)		  *
       *------------------------------------------------------------------*/

      if(gg || !IS_STRUCTURED_T(lb_kind)) {
	if(!check_constraint(ub, lb, record, gg)) {
	  break;  /* Return false */
	}
	attach_lower_bound(ub, lb, record, gg);
	result = TRUE;
      }
      else {
	remove_constraint(ub, lb, record);
	switch(lb_kind) {
	  case FAM_MEM_T:
	    {TYPE* fam = lb->TY2;
	     if(!CONSTRAIN(ub, fam, record, FALSE)) break; /* return FALSE */
	     if(!fam->ctc->opaque) {
	       result = CONSTRAIN(ub, lb->TY1, record, FALSE);
	     }
	     else result = TRUE;
	     break;
	    }

	  case PAIR_T:
	    {TYPE* pr = new_type(FAM_T, NULL);
	     pr->ctc = Pair_ctc;
	     bump_type(pr);
	     if(!CONSTRAIN(ub, pr, record, FALSE)) {
	       drop_type(pr);
	       break;  /* return FALSE */
	     }
	     drop_type(pr);
	     if(!CONSTRAIN(ub, lb->TY1, record, FALSE)) break;
	     result = CONSTRAIN(ub, lb->TY2, record, FALSE);
	     break;
	    }

	  case FUNCTION_T:
	    result = UNIFY(ub, WrappedANY_type, record);
	    break;

	  default:
	    die(183);
	}
      }
      break;

    case PRIMARY_TYPE_VAR_T:
    case PRIMARY_FAM_VAR_T:

      /*-----------------------------------------------------------------*
       * If ub is a primary variable, perform unifications as needed  	*
       * for a primary variable.  					*
       *-----------------------------------------------------------------*/

      if(!check_constraint(ub, lb, record, gg)) {
	break;  /* return FALSE */
      }
      attach_lower_bound(ub, lb, record, gg);
      if(should_check_primary_constraints &&
	 !check_primary_constraints(ub, record)) {
	break;  /* return FALSE */
      }
      result = (!gg) ? reduce_primary_var(ub, lb, record) : TRUE;
      break;

    /*---------------------------------------------*/
    default:
      die(5, ub_kind);
  }

# ifdef DEBUG
    if(trace_unify > 1 ) trace_s(105);
# endif

  return result;
}


/****************************************************************
 *			BIND_LOWER_BOUNDS_U			*
 ****************************************************************
 * Variable a has just become bound, and lwb is its LOWER_BOUNDS*
 * list.  Perform any additional bindings that are required by	*
 * the lower_bounds list of a.					*
 *								*
 * Record bindings if record is true.				*
 *								*
 * This function should not reorder the lower bounds, only      *
 * perform unifications on them.  The completeness checker      *
 * (complete.c) wants the order to be preserved.		*
 *								*
 * Return TRUE on success, FALSE on failure.			*
 ****************************************************************/

Boolean bind_lower_bounds_u(TYPE *a, LIST *lwb, Boolean record)
{
  int lwb_kind;
  if(lwb != NIL) {
    if(!bind_lower_bounds_u(a, lwb->tail, record)) return FALSE;
    lwb_kind = LKIND(lwb);
    if(lwb_kind != TYPE2_L) {
      return CONSTRAIN(a, lwb->head.type, record, lwb_kind == TYPE1_L);
    }
    else {
      return reduce_primary_var(find_u(lwb->head.type), find_u(a), record);
    }
  }
  return TRUE;
}


/****************************************************************
 *			UNIFY_CYCLES				*
 ****************************************************************
 * Find all lower bound cycles X1 >= X2, X2 >= X3, ..., Xn >= X1*
 * in type t, where all Xi are variables, and unify all of the  *
 * variables on the cyle with one another.  			*
 *								*
 * >>= constraints are ignored, as are family lower bounds on   *
 * species variable.						*
 *								*
 * Record bindings if record is true.				*
 *								*
 * Return true if the unifications all succeed, false		*
 * otherwise.							*
 *								*
 *--------------------------------------------------------------*
 *								*
 * find_components_from is the classical strongly connected 	*
 * component algorithm, from a start vertex v.  It puts the	*
 * non-singleton strongly connected components into list	*
 * components, each as a list of variables.			*
 *								*
 * find_all_components runs find_components_from starting at 	*
 * each unsearched variable in t.				*
 ****************************************************************/

PRIVATE int        dfcount;	/* Current depth-first order number */

PRIVATE TYPE_LIST* dfstack;      /* Stack for finding components     */

PRIVATE LIST_LIST* components;   /* List of all non-singleton	    *
				 * strongly connected components    */

/*-------------------------------------------------------------*/

PRIVATE void find_components_from(TYPE *v)
{
  TYPE_LIST *p, *v_lwb;
  Boolean v_is_fam;

  /*------------------------------------------------------*
   * This is the classical strongly connected components  *
   * algorithm.  It uses the dfsmark field of a type node *
   * to tell whether the type is in the stack.		  *
   *------------------------------------------------------*/

  mark_seen(v);
  v->dfnum = v->lowlink = dfcount++;
  v_lwb    = lower_bound_list(v);

  if(v_lwb == NIL) return;          /* singleton component */

  SET_LIST(dfstack, type_cons(v, dfstack));
  v->dfsmark = 1;
  v_is_fam = IS_FAM_VAR_T(TKIND(v));

  /*----------------------------------------------*
   * Recursive traversal.  Get the lowlink value. *
   *----------------------------------------------*/

  for(p = v_lwb; p != NIL; p = p->tail) {

    /*--------------------------------------------------*
     * Get the lower bound wo on v.  Skip w if it is a	*
     * not a >= lower bound or if w is a family lower	*
     * bound on a species variable.			*
     *--------------------------------------------------*/

    if(LKIND(p) == TYPE_L) {
      TYPE* w = find_u(p->head.type);
      if(!v_is_fam && IS_FAM_OR_FAM_VAR_T(TKIND(w))) {
	if(IS_VAR_T(TKIND(w))) {
	  if(!w->seen){
	    find_components_from(w);
	    if(w->lowlink < v->lowlink) {
	      v->lowlink = w->lowlink;
	    }
	  }
	  else if(w->dfnum < v->dfnum && w->dfsmark) {
	    if(w->dfnum < v->lowlink) {
	      v->lowlink = w->dfnum;
	    }
	  }
	}
      }
    }
  }

  /* ------------------------------------------------------------*
   * If we are at the root of a strongly connected component,    *
   * then remove the entire component from the top of the stack. *
   *-------------------------------------------------------------*/

  if(v->lowlink == v->dfnum) {

    /*---------------------------------------------*
     * If the component is singleton, just pop it. *
     *---------------------------------------------*/
     
    if(dfstack->head.type == v) {
      v->dfsmark = 0;
      SET_LIST(dfstack, dfstack->tail);
    }

    /*----------------------------------------------------------*
     * If the component is not singleton, add it to components. *
     *----------------------------------------------------------*/

    else {
      TYPE_LIST* this_component = NIL;
      register TYPE *x;
      do {
	x = dfstack->head.type;
	this_component = type_cons(x, this_component);
	x->dfsmark = 0;
	SET_LIST(dfstack, dfstack->tail);
      } while(x != v && dfstack != NIL);
      SET_LIST(components, list_cons(this_component, components));
    }
  }
}

/*-------------------------------------------------------------*/

PRIVATE void find_all_components(TYPE *t)
{
  TYPE_TAG_TYPE t_kind;

 tail_recur:

  if(t == NULL) return;
  IN_PLACE_FIND_U_NONNULL(t);
  t_kind = TKIND(t);
  if(IS_STRUCTURED_T(t_kind)) {
    find_all_components(t->TY1);
    t = t->TY2;
    goto tail_recur;
  }
  else if(IS_VAR_T(t_kind) && t->seen == 0) {
    find_components_from(t);
  }
}

/*-------------------------------------------------------------*/

Boolean unify_cycles(TYPE *t, Boolean record)
{
  Boolean result;
  LIST_LIST *comp, *p;

# ifdef DEBUG
    if(trace_unify > 1) trace_s(45);
# endif

  /*------------------------------------------------------*
   * Find the strongly connected components, putting them *
   * into list 'components'.				  *
   *------------------------------------------------------*/

  dfcount    = 1;
  dfstack    = NIL;
  components = NIL;
  find_all_components(t);
  reset_seen();

  /*----------------------------------------------------*
   * Now unify the members of each strongly connected	*
   * component.  It is important to pull the list out of*
   * global variable components, since unify can recur	*
   * back to here, and hence ends up reusing that global*
   * variable.						*
   *----------------------------------------------------*/

  comp       = components;   /* ref from components */
  components = NIL;
  for(p = comp; p != NIL; p = p->tail) {
    TYPE_LIST *q;
    TYPE_LIST* this_component = p->head.list;
    TYPE* V = this_component->head.type;
    for(q = this_component->tail; q != NIL; q = q->tail) {
      if(!UNIFY(V, q->head.type, record)) {
	drop_list(comp);
	result = FALSE;
	goto out;
      }
    }
  }
  drop_list(comp);
  result = TRUE;

 out:

# ifdef DEBUG
    if(trace_unify > 1) trace_s(46);
# endif

  return result;
}


/****************************************************************
 *			TYPE_LUB				*
 ****************************************************************
 * Return the join of all of the unstructured, nonvariable	*
 * types in list L, or NULL if there are no such types.		*
 *								*
 * Only consider members of list L that occur with tag TYPE_L.	*
 ****************************************************************/

PRIVATE CLASS_TABLE_CELL* type_lub(TYPE_LIST *L)
{
  CLASS_TABLE_CELL* result = NULL;
  TYPE_LIST *p;

  for(p = L; p != NIL; p = p->tail) {
    if(LKIND(p) == TYPE_L) {
      TYPE*         head_type = find_u(p->head.type);
      TYPE_TAG_TYPE head_kind = TKIND(head_type);
      if(IS_TYPE_T(head_kind)) {
        if(result == NULL) {
	  result = head_type->ctc;
	  if(result == NULL) result = ctcs[0];
	}
        else result = ctcs[full_get_join(result, head_type->ctc)];
      }
    }
  }
  return result;
}


/************************************************************************
 *			REMOVE_DUPLICATE_CONSTRAINTS			*
 ************************************************************************
 * remove_duplicate_constraints(l,V,lbt) returns a sublist of l.	*
 * The types that are deleted are as follows.				*
 *									*
 *  (1) There is only one occurrence with any given list tag (TYPE_L,	*
 *      TYPE1_L or TYPE2_L) of each type in the result list; duplicates *
 *      are deleted.							*
 *									*
 *  (2) Any occurrences of V are deleted.				*
 *									*
 *  (3) All members with tag TYPE_L that are not variables or 		*
 *      structured types are deleted.					*
 *									*
 *  (4) If lbt is not NULL, then all variable members whose domains	*
 *      are <= lbt are deleted, as long as their list tag is not 	*
 *      TYPE2_L.							*
 *----------------------------------------------------------------------*/

PRIVATE TYPE_LIST* 
remove_duplicate_constraints(TYPE_LIST *l, TYPE *V, CLASS_TABLE_CELL *lbt)
{
  TYPE_LIST *rest;
  TYPE *head_type;
  TYPE_TAG_TYPE head_kind;
  int l_kind;

  if(l == NIL) return NIL;
  l_kind = LKIND(l);

  rest      = remove_duplicate_constraints(l->tail, V, lbt);
  head_type = find_u(l->head.type);
  head_kind = TKIND(head_type);

  /*-------------------------*
   * Remove any copies of V. *
   *-------------------------*/

  if(head_type == V) return rest;

  /*-------------------------------------------*
   * Remove duplicates with the same list tag. *
   *-------------------------------------------*/

  if(type_mem(head_type, rest, l_kind)) return rest;

  /*-------------------------------------------------*
   * Remove things that are beneath or equal to lbt. *
   *-------------------------------------------------*/

  if(lbt != NULL && 
     l_kind != TYPE2_L &&
     IS_VAR_T(head_kind) && 
     (head_type->ctc == lbt || 
      ancestor_tm(lbt, head_type->ctc))) return rest;

  /*-------------------------------------------------------------*
   * Remove simple types and families that have list tag TYPE_L. *
   *-------------------------------------------------------------*/

  if(l_kind == TYPE_L && IS_TYPE_T(head_kind)) return rest;

  /*-----------------------------------------------------------*
   * If the current node is not to be deleted, add it to rest. *
   *-----------------------------------------------------------*/

  if(rest == l->tail) return l;
  else {
    HEAD_TYPE hd;
    hd.type = head_type;
    return general_cons(hd, rest, LKIND(l));  
  }
}


/****************************************************************
 *			REMOVE_REDUNDANT_CONSTRAINTS		*
 ****************************************************************
 * Make modifications on lower bounds of variables.		*
 *								*
 *  (1) Make sure that each lower bound occurs only once.	*
 *								*
 *  (2) If a variable has itself as a lower bound, delete the	*
 *      lower bound.						*
 *								*
 *  (3) If there are two or more lower bounds that are not	*
 *      structured and not variables, then replace them by	*
 *      their join.						*
 *								*
 *  (4) V >= U is removed when U is a variable that ranges over *
 *      some genus H such that V >= G is another constraint, 	*
 *      and G >= H.						*
 *								*
 * Additionally, if G`x >= G is a constraint, then G`x is 	*
 * unified with	G.  That is because the only possible value of  *
 * G`x is G.							*
 *								*
 * Bindings are recorded if record is true.			*
 *								*
 * This function returns true on success, false on failure.	*
 * Failure indicates that type t is really empty.  It should    *
 * never occur.							*
 ****************************************************************/

/*----------------------------------------------------------*/

PRIVATE Boolean remove_redundant_constraints_help(TYPE *t, Boolean record)
{
  TYPE_TAG_TYPE t_kind;

 tail_recur:
  if(t == NULL) return TRUE;

  IN_PLACE_FIND_U_NONNULL(t);
  t_kind = TKIND(t);

  /*-----------------------------------------------------*
   * Handle structured types by handling the components. *
   *-----------------------------------------------------*/

  if(IS_STRUCTURED_T(t_kind)) {
    if(!remove_redundant_constraints_help(t->TY1, record)) return FALSE;
    t = t->TY2;
    goto tail_recur;
  }  

  /*-----------------------------------------------------*
   * Handle variables by removing constraints on that    *
   * variable that are of no interest.  This is done in  *
   * such a way that any lower bound that is a named or  *
   * wrap species ends up at the front of the lower	 *
   * bound list.  Subsequent calls recognize the type at *
   * the front of the list, and avoid changing anything  *
   * if no real change is made.  			 *
   *-----------------------------------------------------*/

  else if(IS_VAR_T(t_kind) && !t->seen) {
    TYPE_LIST* t_lb = lower_bound_list(t);
    if(t_lb != NIL) {
      CLASS_TABLE_CELL *lbt1, *lbt;
      LIST *new_lb, *p;
      TYPE *head_lb;
      TYPE_TAG_TYPE head_lb_kind;

      /*-----------------------------------------*
       * Recur on the variables in lower bounds. *
       *-----------------------------------------*/

      mark_seen(t);
      for(p = t_lb; p != NIL; p = p->tail) {
        if(!remove_redundant_constraints_help(p->head.type, record)) {
	  return FALSE;
        }
      }

      /*-----------------------------------------------------------------*
       * Break the lower bound list t_lb into its head H and its tail T. *
       * Find								 *
       *    lbt1 = the least upper bound of the types in T		 *
       *    lbt  = the least upper bound of the types in t_lb.		 *
       * where we are only looking at non-structured, non-variable types.*
       * Getting lbt1 allows us to do things a little more efficiently   *
       * below.								 *
       * 								 *
       * Each of lbt and lbt1 is NULL if there are no nonstructured, 	 *
       * non-variable types to consider.				 *
       *								 *
       * Note: Only >= lower bounds are considered here.  >>= lower      *
       * bounds are ignored.						 *
       *-----------------------------------------------------------------*/

      head_lb      = find_u(t_lb->head.type);
      head_lb_kind = TKIND(head_lb);
      lbt1         = type_lub(t_lb->tail);
      if(LKIND(t_lb) == TYPE_L && IS_TYPE_T(head_lb_kind)) {
	if(lbt1 == NULL) {
	  lbt = head_lb->ctc;
	  if(lbt == NULL) lbt = ctcs[0];
	}
	else lbt = ctcs[full_get_join(head_lb->ctc, lbt1)];
      }
      else {
        lbt = lbt1;
      }

      /*---------------------------------------------------------*
       * If lbt is the same as the domain of t, then unify t	 *
       * with a wrap-type with domain lbt.			 *
       *---------------------------------------------------------*/

      if(lbt != NULL &&
	 (lbt == t->ctc || (t->ctc == NULL && lbt->num == 0))) {
        TYPE* w;
        Boolean result;

        bump_type(w = wrap_tf(t->ctc));

#       ifdef DEBUG
          if(trace_unify > 1) {
	    trace_s(68);
	    trace_ty(t);
	    trace_s(90);
	    trace_ty(w);
	    tracenl();
	  }
#       endif

        result = UNIFY(t, w, record);
        drop_type(w);
        return result;
      }

      /*---------------------------------------------------------*
       * If lbt is not the same as the domain of t, then compute *
       * the new lower bound list new_lb.  Try to make new_lb be *
       * the same as t_lb when they are the same list. 		 *
       *---------------------------------------------------------*/

      bump_list(new_lb = remove_duplicate_constraints(t_lb, t, lbt));
      if(lbt != NULL) {
        if(lbt1 == NULL && new_lb == t_lb->tail) {
	  SET_LIST(new_lb, t_lb);
        }
        else {
	  SET_LIST(new_lb, type_cons(wrap_tf(lbt), new_lb));
	}
      }

      /*---------------------------------------*
       * Now install the new lower bound list. *
       *---------------------------------------*/

      if(t_lb != new_lb) {
#       ifdef DEBUG
          if(trace_unify > 1) {
	    trace_s(91);
	    trace_ty_without_constraints(t);
	    tracenl();
	    print_type_list_without_constraints(new_lb);
	    tracenl();
	  }
#       endif

	install_lwbs(t, t_lb, new_lb, record);
      }
      drop_list(new_lb);
    }
  }
  return TRUE;
}

/*---------------------------------------------------------------*/

Boolean remove_redundant_constraints(TYPE *t, Boolean record)
{
  Boolean result;

# ifdef DEBUG
    if(trace_unify > 1) {
      trace_s(56, record);
      trace_ty(t);
      tracenl();
    }
# endif

  result = remove_redundant_constraints_help(t, record);
  reset_seen();

# ifdef DEBUG
    if(trace_unify > 1) {
      trace_s(63);
      trace_ty(t);
      tracenl();
    }
# endif

  return result;
}


/*==============================================================*
 *			TRANSLATOR ONLY				*
 *==============================================================*/

#ifdef TRANSLATOR

/****************************************************************
 *			 FULL_LOWER_BOUNDS			*
 ****************************************************************
 * Return the list of all lower bounds of v that follow from	*
 * transitivity.  If v >= u >= x is known, then x will be in 	*
 * the result list.						*
 *								*
 * The result list does not, in general, include lower bounds	*
 * that follow from v >= c >= x, where c is a constant (such	*
 * as a species).  Only variables are used as intermediates.	*
 *								*
 * Only >= lower bounds are considered.  >>= lower bounds are 	*
 * left in the list, but those implied by transitivity are	*
 * not added.							*
 ****************************************************************/

PRIVATE LIST* full_lower_bounds_of_var(TYPE *v)
{
  TYPE_LIST *p, *q, *new_lb, *v_lb;

  v_lb   = lower_bound_list(v);
  new_lb = v_lb;
  for(p = v_lb; p != NIL; p = p->tail) {
    if(LKIND(p) == TYPE_L) {
      TYPE* w = find_u(p->head.type);
      if(IS_VAR_T(TKIND(w))) {
	TYPE_LIST *w_lbs;
	bump_list(w_lbs = full_lower_bounds_of_var(w));
	for(q = w_lbs; q != NIL; q = q->tail) {
	  if(LKIND(q) == TYPE_L) {
	    TYPE* q_head = find_u(q->head.type);
	    if(!type_mem(q_head, new_lb, TYPE_L)) {
              new_lb = type_cons(q_head, new_lb);
	    }
	  }
	}
	drop_list(w_lbs);
      }
    }
  }
  return new_lb;
}


/****************************************************************
 *			 LIFT_LOWER_BOUNDS			*
 ****************************************************************
 * Lift lower bounds up in t, so that if X >= Y and Y >= Z then *
 * X >= Z is explicitly stored.  Only do this for >= lower	*
 * bounds.							*
 *								*
 * Results that follow from transitivity due to an intermediate	*
 * constant (species, family, etc.) are not considered.  That 	*
 * is, if we see X >= G >= X, then X >= Z is not added to the   *
 * lower bound list when G is a species, and not a variable.	*
 *								*
 * If record is true, record changes to lower bounds so that    *
 * the changes can be undone.					*
 ****************************************************************/

PRIVATE void lift_lower_bounds(TYPE *t, Boolean record)
{
  TYPE_TAG_TYPE t_kind;

 tail_recur:
  IN_PLACE_FIND_U_NONNULL(t);
  t_kind = TKIND(t);
  if(IS_STRUCTURED_T(t_kind)) {
    lift_lower_bounds(t->TY1, record);
    t = t->TY2;
    goto tail_recur;
  }
  else if(IS_VAR_T(t_kind)) {
    TYPE_LIST *new_lb, *t_lb;
    t_lb = lower_bound_list(t);
    bump_list(new_lb = full_lower_bounds_of_var(t));
    if(t_lb != new_lb) {
      install_lwbs(t, t_lb, new_lb, record);
    }
    drop_list(new_lb);
  }
}


/****************************************************************
 *			 LIFT_ALL_LOWER_BOUNDS			*
 ****************************************************************
 * For each variable V in list L, replace the lower bound list  *
 * of V by its transitive closure, as is computed by		*
 * full_lower_bounds_of_var. 					*
 *								*
 * Descend into lower bounds, recursively closing the lower	*
 * bound lists.							*
 *								*
 * Only consider members of list L that have list tag TYPE_L.	*
 *								*
 * Record all changes to lower bound lists.			*
 ****************************************************************/

PRIVATE void lift_all_lower_bounds_help(TYPE_LIST *L)
{
  TYPE_LIST *p;
  for(p = L; p != NIL; p = p->tail) {
    if(LKIND(p) == TYPE_L) {
      TYPE* V = p->head.type;
      IN_PLACE_FIND_U(V);
      if(!V->seen) {
        if(IS_VAR_T(TKIND(V))) {
	  TYPE_LIST *new_lbs, *V_lbs;
	  mark_seen(V);
          V_lbs   = lower_bound_list(V);
	  lift_all_lower_bounds(V_lbs);
	  bump_list(new_lbs = full_lower_bounds_of_var(V));
	  if(new_lbs != V_lbs) {
	    install_lwbs(V, V_lbs, new_lbs, TRUE);
	  }
	  drop_list(new_lbs);
	}
      }
    }
  }
}

/*--------------------------------------------------------------*/

void lift_all_lower_bounds(TYPE_LIST *L)
{
  lift_all_lower_bounds_help(L);
  reset_seen();
}


/****************************************************************
 *			REDUCE_CONSTRAINT_LIST			*
 ****************************************************************
 * reduce_constraint_list takes a list of lower bounds and a 	*
 * hash table that associates an occurrence count (in the whole *
 * type t) with each variable.  				*
 *								*
 * reduce_constraint_list returns a list of the types in lwbs	*
 * except those that are variables with domain ctc that have    *
 * an occurrence count of <= 1, and that have no lower bounds   *
 * themselves.							*
 ****************************************************************/

PRIVATE TYPE_LIST* 
reduce_constraint_list(CLASS_TABLE_CELL *ctc, TYPE_LIST *lwbs, 
		       HASH2_TABLE *tbl)
{
  HASH2_CELLPTR h;
  HASH_KEY u;
  TYPE_LIST *rest_result;
  TYPE *head_type;
  TYPE_TAG_TYPE head_kind;

  if(lwbs == NULL) return NULL;

  rest_result = reduce_constraint_list(ctc, lwbs->tail, tbl);
  head_type   = find_u(lwbs->head.type);
  head_kind   = TKIND(head_type);

  /*-------------------------------------------------------------*
   * If the first type in list lwbs is not a variable that	 *
   * has the domain ctc.					 *
   *-------------------------------------------------------------*/

  if(!IS_VAR_T(head_kind) || head_type->ctc != ctc) {
    if(rest_result == lwbs->tail) return lwbs;
    else return type_cons(head_type, rest_result);
  }
  
  /*------------------------------------------------------------*
   * If the first type in list lwbs is a variable with a count  *
   * of >1, then we must keep it.				*
   *------------------------------------------------------------*/

  u.num = tolong(head_type);
  h     = locate_hash2(tbl, u, inthash(u.num), eq);
  if(h->key.num != 0 && h->val.num > 1) {
    if(rest_result == lwbs->tail) return lwbs;
    else return type_cons(head_type, rest_result);
  }

  /*----------------------------------------------------*
   * If we get here, then this variable can be deleted. *
   *----------------------------------------------------*/

  else {
    return rest_result;
  }
}


/****************************************************************
 *			REDUCE_CONSTRAINTS			*
 ****************************************************************
 * Remove any constraints from t that are of no interest when   *
 * t is being used only to describe a set of species.           *
 * Uninteresting constraints are those that do not restrict	*
 * that set. 							*
 *								*
 * The following modifications are done to constraints.		*
 *								*
 *  (1) Constraints are lifted, so that if X >= Y and Y >= Z,   *
 *      then X >= Z is explicitly stored.  			*
 *								*
 *  (2) V >= L is of no interest if L is a variable that has	*
 *     the same domain as V, that has no lower bounds itself,   *
 *     and that does not occur anywhere else, including in	*
 *     constraints, in t.  It is deleted.			*
 *								*
 * Bindings are recorded if record is true.			*
 ****************************************************************/

/*------------------------------------------------------*
 * reduce_constraints_help does the computation of      *
 * reduce_constraints, but requires that tbl has 	*
 * been set up to hold the counts of number of		*
 * occurrences of variables.				*
 *------------------------------------------------------*/

PRIVATE void
reduce_constraints_help(TYPE *t, HASH2_TABLE *tbl, Boolean record)
{
  TYPE_TAG_TYPE t_kind;

 tail_recur:
  if(t == NULL) return;

  IN_PLACE_FIND_U_NONNULL(t);
  t_kind = TKIND(t);

  if(IS_STRUCTURED_T(t_kind)) {
    reduce_constraints_help(t->TY1, tbl, record);
    t = t->TY2;
    goto tail_recur;
  }  

  else {
    TYPE_LIST* t_lb = lower_bound_list(t);
    if(IS_VAR_T(t_kind) && t_lb != NIL && !t->seen) {
      TYPE_LIST *p;
      mark_seen(t);
      for(p = t_lb; p != NIL; p = p->tail) {
        if(LKIND(p) == TYPE_L) {
	  reduce_constraints_help(p->head.type, tbl, record);
	}
      }
      install_lwbs(t, t_lb, reduce_constraint_list(t->ctc, t_lb, tbl), record);
    }
  }
}

/*----------------------------------------------------------*/

void reduce_constraints(TYPE *t, Boolean record)
{
  HASH2_TABLE* tbl = NULL;

# ifdef DEBUG
    if(trace_unify > 1) {
      trace_s(92);
      trace_ty(t);
      tracenl();
    }
# endif

  bump_type(t);
  lift_lower_bounds(t, record);
  count_vars_t(t, &tbl);
  reduce_constraints_help(t, tbl, record);
  reset_seen();
  free_hash2(tbl);
  drop_type(t);

# ifdef DEBUG
    if(trace_unify > 1) {
      trace_s(94);
      trace_ty(t);
      tracenl();
    }
# endif

}


/****************************************************************
 *			 ADD_LOWER_BOUND			*
 ****************************************************************
 * Add lb as a lower bound to variable v.  Restrict lb to be    *
 * an acceptable bound for v.  Bindings done during the 	*
 * restriction are not recorded.				*
 *								*
 * Both v and lb must be nonnull.				*
 *								*
 * If gg is true, this is a >>= type lower bound.  Otherwise,	*
 * it is a >= type lower bound.					*
 *								*
 * If the lower bound addition is not successful, then		*
 * complain.							*
 ****************************************************************/

void add_lower_bound(TYPE *v, TYPE *lb, Boolean gg, int line)
{
  if(!CONSTRAIN(v, lb, FALSE, gg)) {
    semantic_error(LOWER_BOUND_FAIL_ERR, line);
  }
} 


/****************************************************************
 *			 MAXIMAL_LOWER_BOUNDS			*
 ****************************************************************
 * Return a list of the members of list L that have tag TYPE_L  *
 * and that are not directly bounded above by other members of  *
 * list L.							*
 ****************************************************************/

TYPE_LIST* maximal_lower_bounds(TYPE_LIST *L)
{
  TYPE_LIST* result   = NIL;
  TYPE_LIST* known_lb = NIL;
  TYPE_LIST *p, *q;

  for(p = L; p != NIL; p = p->tail) {
    if(LKIND(p) == TYPE_L) {
      TYPE* w = find_u(p->head.type);
      if(!type_mem(w, result, 0) && !type_mem(w, known_lb, 0)) {
	SET_LIST(result, type_cons(w, result));
	if(IS_VAR_T(TKIND(w))) {
	  for(q = lower_bound_list(w); q != NIL; q = q->tail) {
	    if(LKIND(q) == TYPE_L) {
	      TYPE* x = find_u(q->head.type);
	      if(!type_mem(x, result, 0) && !type_mem(x, known_lb, 0)) {
		SET_LIST(known_lb, type_cons(x, known_lb));
	      }
	    }
	  }
	}
      }
    }
  }

  drop_list(known_lb);
  if(result != NIL) result->ref_cnt--;
  return result;
}

#endif


