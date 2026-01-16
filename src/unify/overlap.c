/**************************************************************
 * File:    unify/overlap.c
 * Purpose: Overlap tests for types
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
 * This file implements tests that determine how types overlap.         *
 * Possible overlaps include disjoint, equal, one contained in the	*
 * other, etc.								*
 ************************************************************************/

#include <memory.h>
#ifdef USE_MALLOC_H
#  include <malloc.h>
#else
#  include <stdlib.h>
#endif
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

PRIVATE int overlap_help_u
  (TYPE *A, TYPE *B, Boolean full);
PRIVATE Boolean restrictive_binding_done
  (TYPE *t1, TYPE *t2, HASH2_TABLE *tbl, TYPE_LIST *t1_vars);
PRIVATE Boolean restrictive1
  (HASH2_TABLE *tbl, TYPE_LIST *t1_vars, Boolean *has_lower_bounds);
PRIVATE Boolean restrictive2
  (HASH2_TABLE *tbl, TYPE_LIST *t1_vars);
PRIVATE Boolean restrictive3
  (TYPE *t1, TYPE *t2, HASH2_TABLE *tbl, TYPE_LIST *correspondences,
   TYPE_LIST *t1_vars);
PRIVATE Boolean restrictive4
  (TYPE *t1, TYPE *t2);
PRIVATE Boolean restrictive4_help
  (TYPE *t1, TYPE *t2, TYPE_LIST *t2_vars, HASH2_TABLE *t2_tbl, 
   TYPE_LIST *correspondences);
PRIVATE Boolean restrictive5
  (TYPE *t1, TYPE *t2, TYPE_LIST *t2_vars, 
   HASH2_TABLE *t2_tbl, TYPE_LIST *correspondences);
PRIVATE TYPE_LIST* build_var_table
  (TYPE *t1, TYPE *t2, HASH2_TABLE **tbl, TYPE_LIST *rest);
PRIVATE Boolean try_structured_binding
  (TYPE *t1, TYPE *t2, TYPE *Z1, TYPE *lb, 
   HASH2_TABLE *tbl, TYPE_LIST *correspondences,
   TYPE_LIST *t1_vars);
PRIVATE Boolean try_secondary
  (TYPE *t1, TYPE *t2, TYPE *Z1, TYPE *lb, HASH2_TABLE *tbl, 
   TYPE_LIST *correspondences, TYPE_LIST *t1_vars);
PRIVATE Boolean check_t2_structured_binding
  (TYPE *t1, TYPE *t2, TYPE *X2, TYPE *lb, TYPE_LIST *t2_vars, 
   HASH2_TABLE *t2_tbl, TYPE_LIST *correspondences);
PRIVATE Boolean check_simple_constraints
  (TYPE_LIST *t2_vars, HASH2_TABLE *t2_tbl, TYPE_LIST *correspondences);
PRIVATE Boolean check_clusters
  (TYPE_LIST *t2_vars, HASH2_TABLE *t2_tbl, TYPE_LIST *correspondences);
PRIVATE TYPE* cover_var
  (TYPE *V);


/*==============================================================*
 *==============================================================*
 * The overlap testers work by performing a unification and   	*
 * then looking to see what variables became restrictively    	*
 * bound.							*
 *								*
 * The following functions manage what is remembered about	*
 * restrictively bound variables.				*
 *==============================================================*/


/****************************************************************
 *              restrictively_bound_vars			*
 ****************************************************************
 * When an overlap test is done, variables that receive 	*
 * restrictive bindings are identified.  They are kept in	*
 * structure restrictively_bound_vars.  Four kinds of variables *
 * are kept.				 			*
 *								*
 *   splittable_no_lb:    a variable that is not marked nosplit,*
 *                        and that has no lower bound.		*
 *								*
 *   splittable_lb:       a variable that is not marked nosplit,*
 *                        and that has a lower bound.		*
 *								*
 *   nonsplittable_no_lb: a variable that is marked nosplit,    *
 *                        and that has no lower bound.		*
 *								*
 *   nonsplittable_lb:    a variable that is marked nosplit,    *
 *                        and that has a lower bound.		*
 *								*
 * Each field is NULL if there is no such restrictively bound   *
 * variable.							*
 ****************************************************************/

PRIVATE struct restrictively_bound_vars_struct {
  TYPE* splittable_no_lb;
  TYPE* splittable_lb;
  TYPE* nonsplittable_no_lb;
  TYPE* nonsplittable_lb;
} restrictively_bound_vars;



/****************************************************************
 *			ADD_RESTRICTIVELY_BOUND_VAR		*
 ****************************************************************
 * Add variable v to restrictively_bound_vars, in the		*
 * appropriate field.						*
 ****************************************************************/

PRIVATE void add_restrictively_bound_var(TYPE *v)
{
  IN_PLACE_FIND_U(v);
  if(v->nosplit) {
    if(v->LOWER_BOUNDS == NIL) {
      restrictively_bound_vars.nonsplittable_no_lb = v;
    }
    else restrictively_bound_vars.nonsplittable_lb = v;
  }
  else {
    if(v->LOWER_BOUNDS == NIL) {
      restrictively_bound_vars.splittable_no_lb = v;
    }
    else restrictively_bound_vars.splittable_lb = v;
  }
}


/****************************************************************
 *		GET_RESTRICTIVELY_BOUND_VAR			*
 ****************************************************************
 * Return a variable that was restrictively bound in the last	*
 * half_overlap, or NULL if the result was EQUAL_OV.		*
 *								*
 * Preference is given to a splittable variable.  If there is   *
 * no splittable variable, a nonsplittable one is chosen.	*
 *								*
 * Among splittable or nonsplittable variables, preference is   *
 * given to a variable that has a nonempty LOWER_BOUNDS list.   *
 ****************************************************************/

TYPE* get_restrictively_bound_var(void)
{
  if(restrictively_bound_vars.splittable_lb != NULL) {
    return restrictively_bound_vars.splittable_lb;
  }
  else if(restrictively_bound_vars.splittable_no_lb != NULL) {
    return restrictively_bound_vars.splittable_no_lb;
  }
  if(restrictively_bound_vars.nonsplittable_lb != NULL) {
    return restrictively_bound_vars.nonsplittable_lb;
  }
  else return restrictively_bound_vars.nonsplittable_no_lb;
}


/****************************************************************
 *		CLEAR_RESTRICTIVELY_BOUND_VARS			*
 ****************************************************************
 * Set all fields of restrictively_bound_vars to NULL.		*
 ****************************************************************/

PRIVATE void clear_restrictively_bound_vars(void)
{
  memset(&restrictively_bound_vars, 0, 
	 sizeof(struct restrictively_bound_vars_struct));
}


/*==============================================================*
 *==============================================================*
 * The following are the overlap testers.			*
 *==============================================================*/

/****************************************************************
 *			OVERLAP_U				*
 ****************************************************************
 * Check how A and B overlap.  Return one of the following.	*
 *								*
 *   DISJOINT_OV	A and B are disjoint.			*
 *								*
 *   CONTAINS_OV	B is a proper subset of A.		*
 *								*
 *   CONTAINED_IN_OV	A is a proper subset of B.		*
 *								*
 *   EQUAL_OV		A and B contain exactly the same 	*
 *			species.				*
 *								*
 *   BAD_OV		A and B have a nonempty intersection,   *
 *			but neither is a subset of the other.	*
 ****************************************************************/

int overlap_u(TYPE *A, TYPE *B)
{
  int result;

  should_check_primary_constraints = FALSE;
  result = overlap_help_u(A, B, TRUE);
  should_check_primary_constraints = TRUE;
  return result;
}


/****************************************************************
 *			HALF_OVERLAP_U				*
 ****************************************************************
 * Check how A and B overlap.  					*
 *								*
 *   DISJOINT_OV	A and B are disjoint.			*
 *								*
 *   CONTAINS_OR_BAD_OV	B is a proper subset of A, or A and B   *
 * 			have a nonempty intersection, but 	*
 *			neither is a subset of the other.	*
 *			This includes both CONTAINS_OV and	*
 *			BAD_OV of overlap_u.			*
 *								*
 *   EQUAL_OR_CONTAINED_IN_OV					*
 *			A is a (possibly improper) subset of B. *
 *			This includes both EQUAL_OV and		*
 *			CONTAINED_IN_OV of overlap_u.		*
 *								*
 * Note that EQUAL_OR_CONTAINED_IN_OV is the same as EQUAL_OV,	*
 * and CONTAINS_OR_BAD_OV is the same as CONTAINS_OV.		*
 *								*
 * If the result is CONTAINS_OR_BAD_OV or DISJOINT_OV, 		*
 * variables in restrictively_bound_vars are set to variables	*
 * in A that get restrictively bound when A and B are unified. 	*
 * (This is only guaranteed to be done if A is not NULL, and 	*
 * contains no null variables, and does not represent the empty *
 * set.)							*
 *								*
 * If the result is EQUAL_OV, then variables in			*
 * restrictively_bound_vars are set to NULL.			*
 ****************************************************************/

int half_overlap_u(TYPE *A, TYPE *B)
{
  int result;

  clear_restrictively_bound_vars();
  should_check_primary_constraints = FALSE;
  result = overlap_help_u(A, B, FALSE);
  should_check_primary_constraints = TRUE;
  return result;
}


/*==============================================================*
 *==============================================================*
 * The following are the main helpers for the overlap testers.  *
 *==============================================================*/

/****************************************************************
 *		RESTRICTIVE_BINDING_DONE			*
 ****************************************************************
 * Types t1 and t2 started out as copies of one another. 	*
 * Table tbl was created, associating with each variable in t1  *
 * the corresponding variable in t2.  Then t2 was unified 	*
 * with another type.  						*
 *								*
 * List t1_vars contains all variables that occur in t1.	*
 *								*
 * Return true if some variable in t2 became restrictively	*
 * bound, so that t2 describes a smaller set of species than	*
 * t1, and return false otherwise.				*
 *								*
 * If true is returned, add to restrictively_bound_vars at 	*
 * least one variable in t1 that corresponds to a restrictively *
 * bound variable in t2, according to the description of	*
 * restrictively_bound_vars.					*
 ****************************************************************/

PRIVATE Boolean
restrictive_binding_done(TYPE *t1, TYPE *t2, HASH2_TABLE *tbl, 
			 TYPE_LIST *t1_vars)
{
  Boolean result = TRUE;   /* default */
  Boolean has_lower_bounds;

  /*-----------------------------------------------------*
   * If there are no variables in t1, then there are	 *
   * certainly no restrictions.				 *
   *-----------------------------------------------------*/

  if(t1_vars == NIL) return FALSE;

  /*----------------------------------------------------*
   * Look for an individual variable that got 		*
   * restricted.  This also sets has_lower_bounds to 	*
   * true just when one of the variables in t1 or t2 	*
   * has a lower bound.					*
   *----------------------------------------------------*/

  if(restrictive1(tbl, t1_vars, &has_lower_bounds)) {
    goto out;  /* return TRUE */
  }

  /*----------------------------------------------------*
   * Look for two different variables that got unified 	*
   * with one another.					*
   *----------------------------------------------------*/

  if(restrictive2(tbl, t1_vars)) {
    goto out;  /* return TRUE */
  }

  /*----------------------------------------------------*
   * The next case checks for restrictions due to	*
   * new lower bounds.  It is only done if there	*
   * is a lower bound.					*
   *----------------------------------------------------*/

  if(has_lower_bounds) {
#   ifdef DEBUG
      if(trace_overlap) trace_t(565);
#   endif

    if(restrictive3(t1, t2, tbl, NIL, t1_vars)) {
      goto out;  /* return TRUE */
    }
  }

  result = FALSE;

 out:
  return result;
}


/****************************************************************
 *			OVERLAP_HELP_U				*
 ****************************************************************
 * Check how A and B overlap.  If full is true, then return one *
 * of the following.						*
 *								*
 *   DISJOINT_OV	A and B are disjoint.			*
 *								*
 *   CONTAINS_OV	B is a proper subset of A.		*
 *								*
 *   CONTAINED_IN_OV	A is a proper subset of B.		*
 *								*
 *   EQUAL_OV		A and B contain exactly the same 	*
 *			species.				*
 *								*
 *   BAD_OV		A and B have a nonempty intersection,   *
 *			but neither is a subset of the other.	*
 *								*
 * If full is false, then return one of the following.		*
 *								*
 *   DISJOINT_OV	A and B are disjoint.			*
 *								*
 *   CONTAINS_OV	B is a proper subset of A, or A and B   *
 * 			have a nonempty intersection, but 	*
 *			neither is a subset of the other.	*
 *								*
 *   EQUAL_OV		A is a (possibly improper) subset of B. *
 *								*
 * In case where full is false and the result is CONTAINS_OV	*
 * or DISJOINT_OV, variables in restrictively_bound_vars 	*
 * are set to variables in A that get restrictively bound when  *
 * A and B are unified.  (This is only done if A is not NULL,   *
 * and contains no null variables, and does not represent the 	*
 * empty set.)							*
 ****************************************************************/

PRIVATE int 
overlap_help_u(TYPE *A, TYPE *B, Boolean full)
{
  TYPE *acpy, *bcpy;
  HASH2_TABLE *a_tbl, *b_tbl;
  TYPE_LIST *a_var_list, *b_var_list;
  Boolean a_restrictive, b_restrictive;
  int result;

  IN_PLACE_FIND_U(A);
  IN_PLACE_FIND_U(B);

  /*----------------------------------------------------*
   * Handle nulls.  NULL is an anonymous variable that	*
   * ranges over ANY.					*
   *----------------------------------------------------*/

  if(A == NULL) {
    if(B == NULL || (TKIND(B) == TYPE_VAR_T && B->ctc == NULL)) {
      return EQUAL_OV;
    }
    else return CONTAINS_OV;
  }
  else if(B == NULL) {
    if(!full || (TKIND(A) == TYPE_VAR_T && A->ctc == NULL)) return EQUAL_OV;
    else return CONTAINED_IN_OV;
  }

  /*----------------------------------------------------*
   * We will work with copies of A and B.		*
   *----------------------------------------------------*/
  
  bump_type(A);
  replace_null_vars(&A);
  bump_type(B);
  replace_null_vars(&B);

  bump_type(acpy = copy_type(A, 0));
  bump_type(bcpy = copy_type(B, 0));

# ifdef DEBUG
    if(trace_overlap) {
      trace_s(34, full);
      print_two_types(acpy, bcpy);
    }
# endif

  /*--------------------------------------------*
   * Reduce constraints in the copies, so that  *
   * we see an accurate and near-minimal 	*
   * picture of the constraints.		*
   *--------------------------------------------*/

  reduce_constraints(acpy, FALSE);
  reduce_constraints(bcpy, FALSE);

  /*----------------------------------------------------*
   * Build a table associating variables in A with the	*
   * corresponding variables in acpy,  and a similar	*
   * table for B.  Also get a list of the variables in  *
   * acpy and bcpy.					*
   *----------------------------------------------------*/

  a_tbl = b_tbl = NULL;
  bump_list(a_var_list = build_var_table(A, acpy, &a_tbl, NIL));
  if(full) bump_list(b_var_list = build_var_table(B, bcpy, &b_tbl, NIL));
  else b_var_list = NIL;

  /*------------------------------------------------------------*
   * Unify the copies.  If unification fails, then A and B	*
   * must be disjoint.						*
   *------------------------------------------------------------*/

  if(!UNIFY(acpy, bcpy, 0)) {result = DISJOINT_OV; goto out;}

  /*----------------------------------------------------*
   * Now find out where restrictive bindings were done. *
   *----------------------------------------------------*/  

  a_restrictive = restrictive_binding_done(A, acpy, a_tbl, a_var_list);
  if(full) {
    b_restrictive = restrictive_binding_done(B, bcpy, b_tbl, b_var_list);
  }
  else b_restrictive = FALSE;

  if(!a_restrictive) {
    if(!b_restrictive) {result = EQUAL_OV; goto out;}
    else {result = CONTAINED_IN_OV; goto out;}
  }

  if(!b_restrictive) {result = CONTAINS_OV; goto out;}

  result = BAD_OV;

 out:

# ifdef DEBUG
    if(trace_overlap) {
      trace_s(35); 
      trace_s(36 + result);
      tracenl();
    }
# endif

  drop_list(a_var_list);
  drop_list(b_var_list);
  drop_type(acpy);
  drop_type(bcpy);
  drop_type(A);
  drop_type(B);
  free_hash2(a_tbl);
  free_hash2(b_tbl);
  return result;
}


/*==============================================================*
 *==============================================================*
 * The following are some basic tools for the functions		*
 * that find restrictively bound variables.			*
 *==============================================================*/

/****************************************************************
 *			COVER_VAR				*
 ****************************************************************
 * Return a variable that ranges over ANY if V is not a family  *
 * sort of thing, or a variable that ranges over TRANSPARENT if *
 * V is a transparent family or family variable, or that	*
 * ranges over OPAQUE if V is an opaque family or family	*
 * variable.							*
 ****************************************************************/

PRIVATE TYPE*
cover_var(TYPE *V)
{
  CLASS_TABLE_CELL *X_ctc, *V_ctc;
  int V_code;
  TYPE_TAG_TYPE V_kind = TKIND(V);

  /*---------------------------------------------*
   * Get X_ctc, the domain for the new variable. *
   *---------------------------------------------*/

  if(IS_STRUCTURED_T(V_kind)) X_ctc = NULL;
  else { 
    V_ctc  = V->ctc;
    V_code = (V_ctc == NULL) ? GENUS_ID_CODE : V_ctc->code;
    if(MEMBER(V_code, fam_codes)) {
      X_ctc = (V_ctc->opaque) ? OPAQUE_ctc : TRANSPARENT_ctc;
    }
    else X_ctc = NULL;
  }

  /*--------------------------*
   * And return the variable. *
   *--------------------------*/

  return var_t(X_ctc);
}


/****************************************************************
 *			BUILD_VAR_TABLE				*
 ****************************************************************
 * t1 and t2 are two copies of a type.				*
 *								*
 * Set tbl to a table that associates with each variable in t1  *
 * the corresponding variable in t2.  The keys and values  	*
 * are not reference-counted.					*
 *								*
 * Lower bounds on variables are not explored.  Only those 	*
 * variables that occur outside of lower bounds are put into	*
 * the table.							*
 *								*
 * tbl should be NULL on entry from outside.  The new pairs 	*
 * will be added to any existing pairs.				*
 *								*
 * Return a list of the variables in t1, followed by list rest. *
 ****************************************************************/

PRIVATE TYPE_LIST* 
build_var_table(TYPE *t1, TYPE *t2, HASH2_TABLE **tbl, TYPE_LIST *rest)
{
  TYPE_TAG_TYPE t1_kind;

  IN_PLACE_FIND_U(t1);
  t1_kind = TKIND(t1);
  IN_PLACE_FIND_U(t2);
  if(IS_STRUCTURED_T(t1_kind)) {
    TYPE_LIST *tylist1, *tylist2;
    tylist1 = build_var_table(t1->TY1, t2->TY1, tbl, rest);
    tylist2 = build_var_table(t1->TY2, t2->TY2, tbl, tylist1);
    return tylist2;
  }

  else if(IS_VAR_T(t1_kind)) {
    HASH_KEY u;
    HASH2_CELLPTR h;

    u.type = t1;
    h      = insert_loc_hash2(tbl, u, inthash((LONG)(u.type)), eq);
    if(h->key.num == 0) {
      h->key.type = t1;
      h->val.type = t2;
      return type_cons(t1, rest);
    }
    else return rest;
  }

  else return rest;
}


/****************************************************************
 *			GET_CORRESPONDING_VAR			*
 ****************************************************************
 * List correspondences is a list of pairs (X,Y) associating    *
 * with a variable X a corresponding variable Y.  Table tbl is  *
 * similar, but it is a hash table associating pairs (X,Y).	*
 *								*
 * Find a pair (V,Y) in either correspondences or tbl, and	*
 * return Y.  The list is searched first, then the table.	*
 *								*
 * If there is no corresponding variable for V, NULL is		*
 * returned.							*
 ****************************************************************/

PRIVATE TYPE*
get_corresponding_var(TYPE *V, HASH2_TABLE *tbl, TYPE_LIST *correspondences)
{
  HASH_KEY u;
  HASH2_CELLPTR h;
  TYPE_LIST *p;

  if(!IS_VAR_T(TKIND(V))) return NULL;

  for(p = correspondences; p != NIL; p = p->tail) {
    TYPE* this_pair = p->head.type;
    if(V == find_u(this_pair->TY1)) return find_u(this_pair->TY2);
  }

  u.type = V;
  h      = locate_hash2(tbl, u, inthash((LONG) u.type), eq);
  if(h->key.num != 0) return find_u(h->val.type);
  return NULL;
}


/*==============================================================*
 *==============================================================*
 * The following functions work with two types t1 and t2 that	*
 * started out as copies of one another.  After the copy was	*
 * done, some bindings were done on t2, and the goal is to	*
 * determine whether the bindings are restrictive.  They are	*
 * restrictive if there is a type to which t1 can bind but to	*
 * which t2 cannot bind.					*
 *								*
 * It is important to keep track of where the variables are.	*
 * The following naming conventions are used.			*
 *								*
 *   X1, Y1, Z1: variables in t1 that occur outside of 		*
 *		 lower bounds.					*
 *								*
 *   X2, Y2, Z2: variables in t2 that correspond to variables	*
 *		 in t1 that occur outside of lower bounds.  	*
 *		 (X2 is a copy of X1, etc.)			*
 *								*
 *   U2,V2:      variables in t2 that do not correspond to	*
 *		 variables in t1 that occur outside lower	*
 *		 bounds.					*
 *								*
 *   A,B,X,Y:	other variables.				*
 *==============================================================*/

/*==============================================================*
 *==============================================================*
 * The first group of functions is responsible for finding	*
 * restrictions that are caused by domain restrictions on	*
 * variables, and by binding variables to one another.	These	*
 * have nothing to do with lower bounds.			*
 *==============================================================*/

/****************************************************************
 *			RESTRICTIVE1				*
 ****************************************************************
 * Parameter tbl is as for restrictive_binding_done, 	 	*
 * and t1_vars is a list of its keys.				*
 *								*
 * Consider t1 and t2 as described for restrictive_binding_done.*
 * So t2 started out as a copy of t1, but had subsequent 	*
 * bindings done on its variables.				*
 *								*
 * Return true just when t2 has a variable that has a smaller	*
 * domain than the corresponding variable in t1, or that has	*
 * been bound to a nonvariable.  For example, if t1 contains    *
 * variable ANY`a in a place where t2 contains variable REAL`b, *
 * then restrictive1 returns true since the domain of REAL`b is *
 * smaller than the domain of ANY`a.				*
 *								*
 * If true is returned, add to restrictively_bound_vars any 	*
 * variables in t1 that correspond to restrictively bound    	*
 * variables in t2, according to the description of		*
 * restrictively_bound_vars.					*
 *								*
 * Set *has_lower_bounds to true just when there is a variable	*
 * in t1 that has a lower bound, or a corresponding variable in	*
 * t2 that has a lower bound (and that is still an unbound	*
 * variable after the additional bindings.)			*
 ****************************************************************/

PRIVATE Boolean
restrictive1(HASH2_TABLE *tbl, TYPE_LIST *t1_vars, Boolean *has_lower_bounds)
{
  /*----------------------------------------------------*
   * For each variable X1 in t1 (not in a lower bound),	*
   * look at the corresponding variable X2 in t2.  	*
   * If X2 is bound to a nonvariable, or is an unbound	*
   * variable that has a smaller domain than X1, then	*
   * bindings done in t2 are restrictive, so put X1 in	*
   * restrictively_bound_vars.				*
   *----------------------------------------------------*/

  Boolean result;
  LIST *p;
  TYPE *X1, *X2;

# ifdef DEBUG
    if(trace_overlap) trace_t(13);
# endif

  result = FALSE;
  *has_lower_bounds = FALSE;
  for(p = t1_vars; p != NIL; p = p->tail) {
    X1 = p->head.type;
    X2 = get_corresponding_var(X1, tbl, NIL);

    if(TKIND(X1) != TKIND(X2) || X1->ctc != X2->ctc) {
      add_restrictively_bound_var(X1);
      result = TRUE;

#     ifdef DEBUG
        if(trace_overlap) {
	  trace_t(34);
          trace_ty(X1); tracenl();
	}
#     endif
    }

    if(lower_bound_list(X1) != NIL || lower_bound_list(X2) != NIL) {
      *has_lower_bounds = TRUE;
    }
  }

# ifdef DEBUG
    if(trace_overlap && !result) trace_t(561);
# endif

  return result;
}
 

/****************************************************************
 *			RESTRICTIVE2				*
 ****************************************************************
 * Parameter tbl is as for restrictive_binding_done,	 	*
 * and t1_vars is a list of its keys.				*
 *								*
 * Consider t1 and t2 as described for restrictive_binding_done.*
 * So t2 started out as a copy of t1, but had subsequent 	*
 * bindings done on its variables.				*
 *								*
 * Return true just when two distinct variables in t2 became    *
 * bound to one another during the unification.  This is a	*
 * restrictive binding.  For example, if t1 contains type	*
 * (`a,`b) in a place where t2 contains (`c,`c), then		*
 * restrictive2 returns true.					*
 *								*
 * If true is returned, add to restrictively_bound_vars any 	*
 * variable in t1 that corresponds to a restrictively bound     *
 * variable in t2, according to the description of		*
 * restrictively_bound_vars.					*
 ****************************************************************/

PRIVATE Boolean restrictive2(HASH2_TABLE *tbl, TYPE_LIST *t1_vars)
{
  /*----------------------------------------------------*
   * For each pair of distinct variables X1, Y1 in t1 	*
   * (not in lower bounds), find the corresponding 	*
   * variables X2 and Y2 in t2.  If X2 and Y2 are the 	*
   * same, then there is a restriction.  X1 and Y1 are  *
   * both restrictively bound, by being bound to one    *
   * another.						*
   *----------------------------------------------------*/

  Boolean result;
  LIST *p, *q;
  TYPE *X1, *Y1, *X2, *Y2;

# ifdef DEBUG
    if(trace_overlap) trace_t(562);
# endif

  result = FALSE;
  for(p = t1_vars; p != NIL && p->tail != NIL; p = p->tail) {
    X1 = find_u(p->head.type);
    X2 = get_corresponding_var(X1, tbl, NIL);
    for(q = p->tail; q != NIL; q = q->tail) {
      Y1 = find_u(q->head.type);
      Y2 = get_corresponding_var(Y1, tbl, NIL);

      if(X2 == Y2) {
        add_restrictively_bound_var(X1);
        add_restrictively_bound_var(Y1);
        result = TRUE;

#       ifdef DEBUG
          if(trace_overlap) {
	    trace_t(563);
	    print_two_types(X1, Y1);
	  }
#       endif
      }
    }
  }

# ifdef DEBUG
    if(trace_overlap && !result) trace_t(564);
# endif

  return result;
}


/*==============================================================*
 *==============================================================*
 * The next group of functions is responsible for finding	*
 * restrictions that are caused by lower bounds.		*
 *								*
 * Functions restrictive3 and restrictive4 are responsible for	*
 * getting rid of structured lower bounds.			*
 *==============================================================*/

/****************************************************************
 *			RESTRICTIVE3				*
 ****************************************************************
 * Parameter tbl is as for restrictive_binding_done,	 	*
 * and t1_vars is a list of its keys.  List correspondences is  *
 * a list of pair types that augments tbl.  If pair (X1,X2) is 	*
 * in correspondences, then variable X1 in t1 corresponds to	*
 * variable X2 in t2.						*
 *								*
 * Consider t1 and t2 as described for restrictive_binding_done.*
 * So t2 started out as a copy of t1, but had subsequent 	*
 * bindings done on its variables.				*
 *								*
 * Return true if t2 is more restrictive than t1 due to the     *
 * presence of new lower bounds.  That is, return true when 	*
 * there is some type to which t1 can bind, but t2 cannot bind. *
 *								*
 * If true is returned, add to restrictively_bound_vars a 	*
 * variable in t1 that corresponds to a restrictively bound     *
 * variable in t2, according to the description of		*
 * restrictively_bound_vars.					*
 *								*
 * It is required for this function that restrictive1 and	*
 * restrictive2 each returns false on types t1 and t2, so that	*
 * each variable X1 in t1 corresponds to a variable X2 in t2	*
 * that has the same domain as X1, and distinct variables in	*
 * t1 correspond to distinct variables in t2.			*
 ****************************************************************/

PRIVATE Boolean 
restrictive3(TYPE *t1, TYPE *t2, HASH2_TABLE *tbl, TYPE_LIST *correspondences, 
	     TYPE_LIST *t1_vars)
{
  TYPE_LIST *p, *q;

 /*---------------------------------------------------------------------*
  * First, get rid of the structured lower bounds in t1, since		*
  * they are awkward to deal with otherwise.  If there is		*
  * a lower bound of the form X1 >= (A,B), where X1 is in t1, then	*
  * there are two possibilities: either X1 is a pair (X,Y), or		*
  * X1 is secondary.  Try both possibilities.  If either one yields	*
  * true, then there must be a type to which t1 can bind, but t2	*
  * cannot, so return true.						*
  *									*
  * Other structured lower bounds are handled similarly.		*
  *---------------------------------------------------------------------*/

  for(p = t1_vars; p != NIL; p = p->tail) {
    TYPE* X1 = find_u(p->head.type);
    if(IS_VAR_T(TKIND(X1))) {
      for(q = X1->LOWER_BOUNDS; q != NIL; q = q->tail) {
	if(LKIND(q) == TYPE_L) {
	  TYPE*         lb      = find_u(q->head.type);
	  TYPE_TAG_TYPE lb_kind = TKIND(lb);
	  if(IS_STRUCTURED_T(lb_kind)) {
#           ifdef DEBUG
	      if(trace_overlap) {
		trace_t(566, X1);
		trace_ty(lb);
		tracenl();
	      }
#           endif

	    return try_structured_binding(t1, t2, X1, lb, tbl, 
					  correspondences, t1_vars);
	  }
	}
      }
    }
  }

  /*--------------------------------------------------------------*
   * If we get here, then there are no structured lower bounds on *
   * variables in t1_vars.  Handle that case.			  *
   *--------------------------------------------------------------*/

  return restrictive4(t1, t2);
}


/****************************************************************
 *			TRY_SECONDARY				*
 ****************************************************************
 * Bind variable Z1 a secondary variable or type and then	*
 * return the same result as restrictive3(t1, t2, tbl,		*
 * correspondences, t1_vars).					*
 *								*
 * If lb is a pair or family member then bind Z1 to ``a.	*
 * If lb is a function then bind Z1 to ANY.			*
 *								*
 * Z1 can be any kind of variable.  See restrictive3.		*
 ****************************************************************/

PRIVATE Boolean 
try_secondary(TYPE *t1, TYPE *t2, TYPE *Z1, TYPE *lb, HASH2_TABLE *tbl, 
	      TYPE_LIST *correspondences, TYPE_LIST *t1_vars)
{
  TYPE *X1, *X2, *Z2;
  TYPE_TAG_TYPE lb_kind;
  LIST *mark;

  IN_PLACE_FIND_U(lb);
  lb_kind = TKIND(lb);
  bump_type(X1 = (lb_kind == FUNCTION_T) 
			? WrappedANY_type 
			: wrap_var_t(NULL));
  bump_list(mark = finger_new_binding_list());

# ifdef DEBUG
    if(trace_overlap) {
      trace_t(567, Z1);
      trace_ty(X1);
      tracenl();
    }
# endif

  if(UNIFY(Z1,X1,TRUE)) {
    Boolean result;
    Z2 = get_corresponding_var(Z1, tbl, correspondences);
    bump_type(X2 = (lb_kind == FUNCTION_T) 
			? WrappedANY_type 
			: wrap_var_t(NULL));
    if(Z2 != NULL && UNIFY(Z2,X2,TRUE)) {
      TYPE_LIST *new_correspondences;
      bump_list(new_correspondences = 
		(lb_kind == FUNCTION_T) 
		  ? correspondences
		  : type_cons(pair_t(X1, X2), correspondences));
      result = restrictive3(t1, t2, tbl, new_correspondences, t1_vars);
      drop_list(new_correspondences);
      if(result) {
        undo_bindings_u(mark);
        drop_list(mark);
        drop_type(X1);
	drop_type(X2);
        return TRUE;
      }
    }
    drop_type(X2);
    undo_bindings_u(mark);
  }

# ifdef DEBUG
    if(trace_overlap) {
      trace_t(568, Z1);
      trace_ty(X1);
      tracenl();
    }
# endif

  drop_list(mark);
  drop_type(X1);
  return FALSE;
}


/****************************************************************
 *		TRY_DIRECT_STRUCTURED_BINDING			*
 ****************************************************************
 * Bind Z1 to a structured type of the same kind as lb, with	*
 * two new variables as its children.  Then return the result	*
 * of restrictive3(t1, t2, tbl, correspondences', t1_vars')	*
 * where t1_vars' and correspondences' are modified versions of	*
 * t1_vars and correspondences that take new variables into	*
 * account.							*
 ****************************************************************/

PRIVATE Boolean
try_direct_structured_binding(TYPE *t1, TYPE *t2, TYPE *Z1, TYPE *lb,
			      HASH2_TABLE *tbl, 
			      TYPE_LIST *correspondences,
			      TYPE_LIST *t1_vars)
{
  TYPE *T1, *T2, *X1, *X2, *Y1, *Y2, *Z2;
  TYPE_TAG_TYPE lb_kind;
  LIST *mark;

  IN_PLACE_FIND_U(lb);
  lb_kind = TKIND(lb);  
  X1 = var_t(NULL);
  Y1 = cover_var(lb->TY2);
  bump_type(T1 = new_type2(lb_kind, X1, Y1));
  bump_list(mark = finger_new_binding_list());

# ifdef DEBUG
    if(trace_overlap) {
      trace_t(567, Z1);
      trace_ty(T1);
      tracenl();
    }
# endif

  if(UNIFY(Z1, T1, TRUE)) {
    X2 = var_t(NULL);
    Y2 = cover_var(lb->TY2);
    bump_type(T2 = new_type2(lb_kind, X2, Y2));
    Z2 = get_corresponding_var(Z1, tbl, correspondences);
    if(Z2 != NULL && UNIFY(Z2, T2, TRUE)) {
      LIST *new_t1_vars, *new_correspondences;
      Boolean result;
      bump_list(new_t1_vars = type_cons(X1, type_cons(Y1, t1_vars)));
      bump_list(new_correspondences = 
		   type_cons(pair_t(X1,X2),
			     type_cons(pair_t(Y1, Y2),
				       correspondences)));
      result = restrictive3(t1, t2, tbl, new_correspondences, new_t1_vars);
      drop_list(new_correspondences);
      drop_list(new_t1_vars);
      if(result) {
	undo_bindings_u(mark);
	drop_list(mark);
	drop_type(T1);
	drop_type(T2);
	return TRUE;
      }
    }
    undo_bindings_u(mark);
    drop_type(T2);
  }

# ifdef DEBUG
    if(trace_overlap) {
      trace_t(568, Z1);
      trace_ty(T1);
      tracenl();
    }
# endif

  drop_list(mark);
  drop_type(T1);
  return FALSE;
}


/****************************************************************
 *			TRY_STRUCTURED_BINDING			*
 ****************************************************************
 * This is a helper function for restrictive3.  See that	*
 * function for details.					*
 *								*
 * Bind variable Z1 as follows.					*
 *								*
 * If lb is a product type (A,B), then try two possibilities.	*
 *   (1) Z1 = (X,Y), X >= A, Y >= B				*
 *   (2) Z1 is secondary, Z1 >= A, Z1 >= B			*
 *								*
 * If lb is a function type A -> B, then try two possibilities.	*
 *   (1) Z1 = (X -> Y), A >= X, Y >= B				*
 *   (2) Z1 = ANY						*
 *								*
 * If lb is a family member, then try two possibilities.	*
 *   (1) Z1 = X(Y)						*
 *   (2) Z1 is secondary					*
 *								*
 * Note that the constraints will automatically be		*
 * added as a result of binding Z1, so just do the binding.	*
 ****************************************************************/

PRIVATE Boolean 
try_structured_binding(TYPE *t1, TYPE *t2, TYPE *Z1, TYPE *lb, 
		       HASH2_TABLE *tbl, TYPE_LIST *correspondences,
		       TYPE_LIST *t1_vars)
{

  /*---------------------------------------*
   * Case 1: Bind Z1 to a structured type. *
   *---------------------------------------*/

  if(try_direct_structured_binding(t1, t2, Z1, lb, tbl, 
				   correspondences, t1_vars)) {
    return TRUE;
  }

  /*------------------------------------------*
   * Case 2: Bind Z1 to a secondary variable. *
   *------------------------------------------*/

  return try_secondary(t1, t2, Z1, lb, tbl, correspondences, t1_vars);
}


/****************************************************************
 *			RESTRICTIVE4				*
 ****************************************************************
 * This is the same as restrictive3, but it presumes     	*
 * that none of the variables in t1 have any structured lower	*
 * bounds.							*
 ****************************************************************/

/*--------------------------------------------------------------*
 * The plan is to get rid of any structured lower bounds on	*
 * variables in t2, and then to use restrictive5, which works	*
 * when there are no structured lower bounds at all.		*
 *								*
 * Suppose that X2 >= (A,B) is a constraint in t2.  We want 	*
 * to know whether it is possible to violate this while 	*
 * satisfying all of the constraints in t1.  There are two 	*
 * ways to do that.						*
 *								*
 *  1. X2 = (U,V) and either U >= A is violated or V >= B is	*
 *     violated.						*
 *								*
 *  2. One of X2 >= PAIR  or X2 >= A or X2 >= B is violated.	*
 *     where PAIR is the thing that stands in for products.	*
 *								*
 * Other structured lower bounds (functions and family members) *
 * are handled similarly.					*
 *								*
 * Function restrictive4_help does the work, with restrictive4  *
 * just getting a table for it to use.				*
 *--------------------------------------------------------------*/

PRIVATE Boolean 
restrictive4(TYPE *t1, TYPE *t2)
{
  /*-------------------------------------------------------------*
   * Before doing anything we need to get a reverse table that	 *
   * associates variables t1 with corresponding variables in t2, *
   * but is keyed on the t2 variables.				 *
   *-------------------------------------------------------------*/

  Boolean result;
  TYPE_LIST *t2_vars;
  HASH2_TABLE* t2_tbl = NULL;
  bump_list(t2_vars = build_var_table(t2, t1, &t2_tbl, NIL));

  /*----------------------------------*
   * Now do the work with this table. *
   *----------------------------------*/

  result = restrictive4_help(t1, t2, t2_vars, t2_tbl, NIL);
  free_hash2(t2_tbl);
  drop_list(t2_vars);
  return result;
}

/*--------------------------------------------------------*/

PRIVATE Boolean 
restrictive4_help(TYPE *t1, TYPE *t2, TYPE_LIST *t2_vars,
		 HASH2_TABLE *t2_tbl, TYPE_LIST *correspondences)
{
  TYPE_LIST *p, *q;

  /*----------------------------------------------------*
   * Find all of the structured lower bounds in t2.	*
   *----------------------------------------------------*/
  
  for(p = t2_vars; p != NIL; p = p->tail) {
    TYPE* X2 = find_u(p->head.type);
    if(IS_VAR_T(TKIND(X2))) {
      for(q = X2->LOWER_BOUNDS; q != NIL; q = q->tail) {
	if(LKIND(q) == TYPE_L) {
	  TYPE*         lb      = find_u(q->head.type);
	  TYPE_TAG_TYPE lb_kind = TKIND(lb);
	  if(IS_STRUCTURED_T(lb_kind)) {
	    return check_t2_structured_binding(t1, t2, X2, lb, t2_vars, 
					       t2_tbl, correspondences);
	  }
	}
      }
    }
  }

  /*--------------------------------------------------------------*
   * If we get here, then there are no structured lower bounds on *
   * variables in t2_vars.  Handle that case.			  *
   *--------------------------------------------------------------*/

  return restrictive5(t1, t2, t2_vars, t2_tbl, correspondences);
}


/****************************************************************
 *			TRY_CONSTRAINT				*
 ****************************************************************
 * This is a helper for check_t2_structured_binding.  It adds   *
 * constraint ub >= lb to the t2-constraints, and then tries to *
 * find a solution that satisfies the t1 constraints but 	*
 * violates the t2-constraints.	 				*
 *								*
 * The return value is true on success, false on failure.	*
 ****************************************************************/

PRIVATE Boolean 
try_constraint(TYPE *ub, TYPE *lb, TYPE *t1, TYPE *t2, 
	       TYPE_LIST *t2_vars, HASH2_TABLE *t2_tbl, 
	       TYPE_LIST *correspondences)
{
  LIST *mark2;

# ifdef DEBUG
    if(trace_overlap) {
      trace_t(570);
      print_two_types(ub,lb);
    }
# endif

  bump_list(mark2 = finger_new_binding_list());
  if(CONSTRAIN(ub, lb, TRUE, FALSE)) {
    if(restrictive4_help(t1, t2, t2_vars, t2_tbl, correspondences)) {
      undo_bindings_u(mark2);
      drop_list(mark2);
      return TRUE;
    }
    undo_bindings_u(mark2);
  }
  drop_list(mark2);

# ifdef DEBUG
    if(trace_overlap) {
      trace_t(571);
      print_two_types(ub,lb);
    }
# endif

  return FALSE;
}


/****************************************************************
 *			CHECK_T2_STRUCTURED_BINDING		*
 ****************************************************************
 * This is a helper function for restrictive4.  X2 >= lb is	*
 * a constraint in t2, and lb is structured.  Return the	*
 * same result as restrictive4, but with this constraint	*
 * removed.  							*
 *								*
 * t2_vars is a list of the variables in t2.  t2_tbl and	*
 * correspondences associate variables in t2 and their		*
 * corresponding variables in t1.				*
 ****************************************************************/

PRIVATE Boolean 
check_t2_structured_binding(TYPE *t1, TYPE *t2, TYPE *X2, TYPE *lb, 
                            TYPE_LIST *t2_vars, HASH2_TABLE *t2_tbl, 
			    TYPE_LIST *correspondences)
{
  TYPE_TAG_TYPE lb_kind;
  TYPE* X1 = get_corresponding_var(X2, t2_tbl, correspondences);

# ifdef DEBUG
    if(trace_overlap) {
      trace_t(569, X2);
      trace_ty(lb);
      tracenl();
    }
# endif

  IN_PLACE_FIND_U(lb);
  lb_kind = TKIND(lb);

  /*------------------------------------------------------------*
   * Case 1. 							*
   *								*
   *  lb = (A,B)   : X2 = (U,V) and either U >= A is violated 	*
   *                 or V >= B is violated.			*
   *								*
   *  lb = (A -> B): X2 = (U -> V) and either A >= U is		*
   *                 violated or V >= B is violated.		*
   *								*
   *  lb = B(A)    : X2 = V(U) and				*
   *                   A opaque     : V >= B is violated	*
   *                   A transparent: either V >= B or		*
   *		                      U >= A is violated.	*
   *------------------------------------------------------------*/

  {LIST *mark;

   /*-----------------------------------------------------------*
    * First unify X2 and its corresponding variable X1 with a	*
    * structured type of an appropriate form.			*
    *-----------------------------------------------------------*/

   TYPE* U2 = var_t(NULL);
   TYPE* V2 = cover_var(lb->TY2);
   TYPE* X2_val = new_type2(lb_kind, U2, V2);
   bump_type(X2_val);
   bump_list(mark = finger_new_binding_list());
   if(UNIFY(X2, X2_val, TRUE)) {
     TYPE* U1 = var_t(NULL);
     TYPE* V1 = cover_var(lb->TY2);
     TYPE* X1_val = new_type2(lb_kind, U1,V1);
     bump_type(X1_val);
     if(UNIFY(X1, X1_val, TRUE)) {
       TYPE *ub1, *ub2, *lb1, *lb2;
       TYPE_LIST *new_correspondences;
       bump_list(new_correspondences = 
		 type_cons(pair_t(U2, U1),
			   type_cons(pair_t(V2, V1), correspondences)));
 
       /*-----------------------------------------------------------*
        * Determine the two constraints ub1 >= lb1 and ub2 >= lb2   *
	* to try.						    *
        *-----------------------------------------------------------*/

       switch(lb_kind) {
	 case PAIR_T:
	   ub1 = U2;
	   lb1 = lb->TY1;
	   ub2 = V2;
	   lb2 = lb->TY2;
	   break;

	 case FUNCTION_T:
	   ub1 = lb->TY1;
	   lb1 = U2;
	   ub2 = V2;
	   lb2 = lb->TY2;
	   break;

	 case FAM_MEM_T:
	   ub2 = V2;
	   lb2 = lb->TY2;
	   if(V2->ctc->opaque) ub1 = lb1 = NULL;
	   else {
	     ub1 = U2;
	     lb1 = lb->TY1;
	   }
	   break;

	 default:
	   die(182);
       }

       /*-----------------------------------------------------------*
	* Try constraint ub1 >= lb1.  If it yields a solution, then *
	* answer true.						    *
        *-----------------------------------------------------------*/

       if(ub1 != NULL) {
	 if(try_constraint(ub1, lb1, t1, t2, t2_vars, t2_tbl, 
			   new_correspondences)) {
	   drop_list(new_correspondences);
	   undo_bindings_u(mark);
	   drop_list(mark);
	   drop_type(X1_val);
	   drop_type(X2_val);
	   return TRUE;
	 }
       }

       /*-----------------------------------------------------------*
	* Try constraint ub2 >= lb2.  If it yields a solution, then *
	* answer true.						    *
        *-----------------------------------------------------------*/

       if(ub2 != NULL) {
	 if(try_constraint(ub2, lb2, t1, t2, t2_vars, t2_tbl, 
			   new_correspondences)) {
	   drop_list(new_correspondences);
	   undo_bindings_u(mark);
	   drop_list(mark);
	   drop_type(X1_val);
	   drop_type(X2_val);
	   return TRUE;
	 }
       }

       drop_list(new_correspondences);
       undo_bindings_u(mark);
     }
     drop_type(X1_val);
   }
   drop_type(X2_val);
   drop_list(mark);  
  }

  /*------------------------------------------------------------* 
   * Case 2. 							*
   *								*
   *  lb = (A,B)   : X2 >= PAIR or X2 >= A or X2 >= B is	*
   *		     violated.					*
   *								*
   *  lb = (A -> B): X2 >= ANY is violated.			*
   *								*
   *  lb = A(B)    : A opaque     : X2 >= A is violated.	*
   *                 A transparent: either X2 >= A or X2 >= B	*
   *                                is violated.		*
   *------------------------------------------------------------*/

  {TYPE *lb1, *lb2, *lb3;

   switch(lb_kind) {
     case PAIR_T:
       lb1 = new_type(FAM_T, NULL);
       lb1->ctc = Pair_ctc;
       lb2 = lb->TY1;
       lb3 = lb->TY2;
       break;

     case FUNCTION_T:
       lb1 = WrappedANY_type;
       lb2 = lb3 = NULL;
       break;

     case FAM_MEM_T:
       lb1 = lb->TY2;
       lb2 = (lb->TY2->ctc->opaque) ? NULL : lb->TY1;
       lb3 = NULL;
       break;

     default:
       die(182);
   }   

   if(lb1 != NULL) {
     if(try_constraint(X2, lb1, t1, t2, t2_vars, t2_tbl, correspondences)) {
       return TRUE;
     }
   }

   if(lb2 != NULL) {
     if(try_constraint(X2, lb2, t1, t2, t2_vars, t2_tbl, correspondences)) {
       return TRUE;
     }
   }

   if(lb3 != NULL) {
     if(try_constraint(X2, lb3, t1, t2, t2_vars, t2_tbl, correspondences)) {
       return TRUE;
     }
   }
  }

  return FALSE;
}


/*==============================================================*
 *==============================================================*
 * The next group of functions is also part of finding		*
 * restrictions that are caused by lower bounds.		*
 *								*
 * Function restrictive5 responsible for the case where there	*
 * are no structured lower bounds.				*
 *==============================================================*/

/****************************************************************
 *			RESTRICTIVE5				*
 ****************************************************************
 * This is the same as restrictive3, but it presumes that there *
 * are no structured lower bounds in either t1 or t2.  See      *
 * restrictive3.						*
 *								*
 * t2_vars is a list of the variables in t2.	 		*
 *								*
 * Table t2_tbl is a table that associates to each variable in	*
 * t2 the corresponding variable in t1.  correspondences is a	*
 * list of pairs (X2,X1) that provides additional 		*
 * correspondences that are not found in the table.		*
 ****************************************************************/

 /*---------------------------------------------------------------------*
  * Our job is to decide whether there is a binding of the variables    *
  * that satisfies the constraints in t1 (the t1-constraints) but does 	*
  * not satisfy the constraints in t2.	A solution to the t1-constraints*
  * is called a t1-solution.  A t1-solution that does not satisfy the	*
  * t2-constraints is called an overall solution.			*
  *									*
  * Note: types t1 and t2 have shared variables (those that were 	*
  * present when t1 was copied) and can also have variables that do	*
  * not occur in the other (those that occur only in lower bounds.)	*
  * Variables that are shared by t1 and t2 are called main variables.	*
  * Variables that occur only in constraints in t2 (and not in t1) are	*
  * called aux variables.						*
  *									*
  * What we are looking for is a t1-solution that cannot be extended to *
  * a t2-solution by selecting values of the aux variables.		*
  * The idea is as follows.   We examine the t2-constraints.		*
  *									*
  * 1. (Type 1 constraints)						*
  *    First check constraints of the forms X >= C and X >= Y, where X	*
  *    and Y are main variables and C is a constant (a genus or species,*
  *    for example).  These t2-constraints can be handled individually.	*
  *    If any solution to the t1-constraints is found that fails	*
  *    to satisfy one of these constraints, then there is an overall	*
  *    solution,0 and we answer yes.  If not, then all solutions of the	*
  *    t1-constraints must satisfy this inequality, so it can be	*
  *    dropped from the t2-constraints.					*
  *									*
  *    Note that there can be no interesting t2-constraints of the form	*
  *    C >= X, since the presence of such a constraint that resulted in	*
  *    a restriction would have been detected by function restrictive1.	*
  *									*
  * 2. (Type 2 constraints)						*
  *    Next consider t2-constraints of the form X >= V, V >= X, 	*
  *    U >= V, C >= V and V >= C where C is a constant, X is a main 	*
  *    variable and U, V are aux variables.    				*
  *									*
  *    Handle them by selecting one aux variable at a time.  If V is an	*
  *    aux-variable, take all of the t2-constraints that involve V and	*
  *    no other aux-variable, and call that collection of		*
  *    t2-constraints a *cluster*.					* 
  *									*
  *    Two things need to be observed.					*
  *									*
  *      A. It is possible to check each cluster (for aux-variable V) 	*
  *         independently to see whether, for every t1-solution, there	*
  *         is a value of V that satisfies this cluster.  If there is	*
  *	    no such V, then there is an overall solution, and we	*
  *	    answer yes.							*
  *									*
  *      B. If each cluster is satisfiable independently, then for	*
  *         every t1-solution, there is a solution to the		*
  *	    t2-constraints.						*
  *									*
  * Argument for fact (A).						*
  * ----------------------						*
  * It is important that the constraints have been transitively closed, *
  * except that relationships X >= Y that follow from X >= C >= Y,	*
  * where C is a constant, are not needed.  Before running trying	*
  * any constraints, the transitive closure is done.			*
  *									*
  * Also, the type (2) constraints are only checked after all type	*
  * (1) constraints were checked and found to be satisfied by all	*
  * t1-solutions.  That way, all constraints of type (1) that		*
  * follow by transitivity have been checked.				*
  * 									*
  * Consider a cluster for variable V.  Suppose that V has both upper	*
  * and lower bounds in the cluster.  For any selection of values for 	*
  * the main variables, take the least upper bound of the lower bounds  *
  * of V in the cluster.  This least upper bound must be less than or	*
  * equal to all of the upper bounds of V since all relationships	*
  * between upper and lower bounds were already checked under type (1).	*
  * So this is a solution for V, and the t2-constraints in this cluster *
  * must be satisfiable.						*
  *									*
  * The same kind of argument holds when V has only lower bounds.  The	*
  * interesting case is when V has only upper bounds.  We can't bind   	*
  * V to the greatest lower bound of its upper bounds when that		*
  * greatest lower bound is bottom.  So the idea is to determine	*
  * whether there is a solution to the t1-constraints where the upper	*
  * bounds of V have bottom as their greatest lower bound.  That will	*
  * give an overall solution, since it will be a t1-solution that does	*
  * not satisfy the constraints in this cluster.  The details are given *
  * under function check_cluster.					*
  *									*
  * Argument for fact (B).						*
  * ----------------------						*
  * Clearly, if any variable V has a cluster that is not		*
  * satisfiable by a t1-solution, then there is a solution.  But	*
  * suppose that all t1-solutions also satisfy each cluster (or,	*
  * more precisely, make each cluster satisfiable since there is a	*
  * value of V that satisfies the cluster).  Can all of the clusters,   *
  * and the constraints among aux-variables that are not part of	*
  * any clusters, be satisfied simulataneously?  Yes.			*
  *									*
  * Given a t1-solution, select a value for each aux-variable that	*
  * satisfies its cluster.  Now look at relationships among		*
  * aux-variables.  Suppose that U >= V is such a t2-constraint.	*
  * If this constraint is not already satisfied, then it suffices to	*
  * set U = lub(U,V) and V = glb(U,V).  These new values will still	*
  * have to satisfy their clusters, and they also satisfy U >= V.  	*
  * (This is because, by transitivity, the cluster of V shares all of	*
  * U's upper bounds, and the cluster of U shares all			*
  * of V's lower bounds.)						*
  *									*
  * Continue in this fashion until all of the t2-constraints that	*
  * relate aux variables to one another have been satisfied.		*
  *---------------------------------------------------------------------*/
  
PRIVATE Boolean 
restrictive5(TYPE *t1, TYPE *t2, TYPE_LIST *t2_vars, HASH2_TABLE *t2_tbl, 
	     TYPE_LIST *correspondences)
{
  /*--------------------------------------------------------------------*
   * 1. Do a transitive closure on the constraints.  This is needed	*
   * for the algorithm to work.						*
   *--------------------------------------------------------------------*/

  lift_all_lower_bounds(t2_vars);

# ifdef DEBUG
    if(trace_overlap) {
      trace_t(583);
      print_two_types(t1, t2);
    }
# endif

  /*--------------------------------------------------------------------*
   * 2. Check constraints of the form X >= C and X >= Y, where X	*
   * and Y are main variables and C is a constant.			*
   *--------------------------------------------------------------------*/

  if(check_simple_constraints(t2_vars, t2_tbl, correspondences)) {
    return TRUE;
  }

  /*------------------------------------*
   * 3. Next check each cluster.	*
   *------------------------------------*/

  return check_clusters(t2_vars, t2_tbl, correspondences);
}


/****************************************************************
 *			CHECK_SIMPLE_CONSTRAINT			*
 ****************************************************************
 * Return true if constraint X1 >= Y1 can fail to be satisfied	*
 * by a solution to the constraints on the variables in list	*
 * t1_vars.  							*
 *								*
 * It is required that either 					*
 *								*
 *   (a) X1 and Y1 are two variables that occur in t1_vars, or	*
 *								*
 *   (b) X1 is such a variable and Y1 is a constant (genus,	*
 *       species, etc.). 					* 
 *								*
 * This function requires that none of the lower bounds on X1   *
 * are structured.						*
 *								*
 * Note: although it is convenient to imagine t1_vars, it turns *
 * out that we do not need to refer to that list, so it is	*
 * not a parameter.						*
 ****************************************************************/

/*--------------------------------------------------------------*
 * To check whether constraint X1 >= Y1 can be violated, we need*
 * to compare it to the constraints in t1.  			*
 *								*
 * 1. If X1 >= Y1 is a constraint on X1 in t1, then there is	*
 *    clearly no t1-solution that violates X1 >= Y1.		*
 *								*
 * 2. If X1 >= C >= D >= Y1 are all in t1, then there is 	*
 *    clearly no t1-solution that violates X1 >= Y1.  These	*
 *    constraints will not cause X1 >= Y1 to show up in t1 	*
 *    explicitly by transitivity, so they must be checked for	*
 *    explicitly.						*
 *								*
 * If X1 >= Y1 does not follow from the t1-constraints, then	*
 * new genera can be added to mimic the entire structure of the *
 * constraints in t1, and a solution exists that does not	*
 * satisfy X1 >= Y1.						*
 *--------------------------------------------------------------*/

PRIVATE Boolean 
check_simple_constraint(TYPE *X1, TYPE *Y1)
{
  TYPE_LIST *p;

# ifdef DEBUG
    if(trace_overlap) {
      trace_t(584);
      print_two_types(X1, Y1);
    }
# endif

  for(p = X1->LOWER_BOUNDS; p != NIL; p = p->tail) {
    TYPE* lb = find_u(p->head.type);
    if(lb == Y1) return FALSE;
    if(!IS_VAR_T(TKIND(lb))) {
      if(lb->ctc == Y1->ctc || ancestor_tm(lb->ctc, Y1->ctc)) {
#       ifdef DEBUG
          if(trace_overlap) trace_t(585);
#       endif
	return FALSE;
      }
    }
  }

# ifdef DEBUG
  if(trace_overlap) trace_t(586, X1);
# endif
  add_restrictively_bound_var(X1);
  return TRUE;
}


/****************************************************************
 *			CHECK_SIMPLE_CONSTRAINTS		*
 ****************************************************************
 * This function is part of the work of restrictive5.  It	*
 * returns true if there is any simple (type 1) constraint	*
 * in t2 that can be violated while satisfying the		*
 * t1-constraints.  See restrictive5 for details on what it 	*
 * does.							*
 *								*
 * t2_vars is a list of the main variables in t2, and t2_tbl	*
 * is a table that associates with each variable in t2_vars	*
 * the corresponding variable in t2.				*
 ****************************************************************/

PRIVATE Boolean 
check_simple_constraints(TYPE_LIST *t2_vars, HASH2_TABLE *t2_tbl,
			 TYPE_LIST *correspondences)
{
  LIST *p, *q;
  for(p = t2_vars; p != NIL; p = p->tail) {
    TYPE* X2 = find_u(p->head.type);
    TYPE* X1 = get_corresponding_var(X2, t2_tbl, correspondences);
    for(q = X2->LOWER_BOUNDS; q != NIL; q = q->tail) {
      if(LKIND(q) == TYPE_L) {
	TYPE* Y2 = find_u(q->head.type);
	TYPE* Y1 = get_corresponding_var(Y2, t2_tbl, correspondences);
	if(Y1 != NULL) {
	  if(check_simple_constraint(X1,Y1)) return TRUE;
	}
        else if(IS_TYPE_T(TKIND(Y2))) {
	  if(check_simple_constraint(X1,Y2)) return TRUE;
	}
      }
    }
  }

  return FALSE;
}


/****************************************************************
 *			CHECK_CLUSTER				*
 ****************************************************************
 * List vars is a list of variables.  Return true if it is	*
 * is possible to bind those variables in such a way that	*
 * their greatest lower bound is bottom (the empty set).	*
 *								*
 * It is required that none of the lower bounds on variables	*
 * in list vars are structured.  The lower bounds are either	*
 * variables or species or families.				*
 ****************************************************************/

/*--------------------------------------------------------------*
 * See the description of restrictive5 for terminology.		*
 *								*
 * The variables in list vars, together with their lower bounds *
 * and their domains, describe a partial order.  The question	*
 * is whether it is possible to embed that partial order in	*
 * an extension of the current genus/species hierarchy in such	*
 * a way that the constant correspondences are respected (so C  *
 * means the same C in both cases) and such that the variables  *
 * in list vars have bottom as their greatest lower bound.  The *
 * variables can all become new things in the hierarchy.	*
 *								*
 * This will always be possible provided that the variables 	*
 * in list vars do not share a common lower bound.  So just	*
 * look for such a lower bound.					*
 *--------------------------------------------------------------*/

PRIVATE Boolean 
check_cluster(TYPE_LIST *vars)
{
  Boolean result;
  int k, num_vars;
  TYPE *overall_const_lb, **const_var_lb;
  TYPE_LIST *lb_vars, *common_var_list, *p, *q, **var_lb_list;

# ifdef DEBUG
    if(trace_overlap) {
      TYPE_LIST *pp;
      trace_t(587);
      for(pp = vars; pp != NIL; pp = pp->tail) {
	trace_ty(pp->head.type);
	tracenl();
      }
    }
# endif

  /*------------------------------------------------------------*
   * If there are not at least two variables, then we must	*
   * clearly return false.					*
   *------------------------------------------------------------*/

  num_vars = list_length(vars);
  if(num_vars <= 1) return FALSE;

  /*-----------------------------------------------------------*
   * Create and initialize the list and type arrays used below. *
   *-----------------------------------------------------------*/

  var_lb_list  = (TYPE_LIST**) BAREMALLOC(num_vars*sizeof(TYPE_LIST*));
  const_var_lb = (TYPE**) BAREMALLOC(num_vars*sizeof(TYPE*));

  lb_vars = NIL;
  for(k = 0; k < num_vars; k++) {
    var_lb_list[k]  = NIL;
    const_var_lb[k] = NULL;
  }

  /*--------------------------------------------------------------------*
   * Phase 1. Accumulate the following.					*
   *									*
   *    a) For each variable Xi in list vars, a list var_lb_list[i] of	*
   *       the variables that are lower bounds of Xi.			*
   *									*
   *    b) For each variable Xi in list vars, a type const_var_lb[i] 	*
   *       that is a constant (genus/species, etc.) lower bound of Xi.	*
   *       It is the largest known such lower bound on Xi, or is NULL	*
   *       if no constant lower bound is known.				*
   *									*
   *    c) A list lb_vars of all variables that are encountered as	*
   *       lower bounds.						*
   *--------------------------------------------------------------------*/

  for(p = vars, k = 0; p != NIL; p = p->tail, k++) {
    TYPE* X = find_u(p->head.type);
    if(IS_VAR_T(TKIND(X))) {
      for(q = X->LOWER_BOUNDS; q != NIL; q = q->tail) {
	if(LKIND(q) == TYPE_L) {
          TYPE* Y = find_u(q->head.type);
	  if(IS_VAR_T(TKIND(Y))) {
	    SET_LIST(var_lb_list[k], type_cons(Y, var_lb_list[k]));
	    SET_LIST(lb_vars, add_var_to_list(Y, lb_vars));
	  }
	  else {
	    if(const_var_lb[k] == NULL) {
	      bump_type(const_var_lb[k] = Y);
	    }
	    else {
	      SET_TYPE(const_var_lb[k], type_join(Y, const_var_lb[k]));
	    }
	  }
	}
      }
    }
  }

  /*--------------------------------------------------------------------*
   * Phase 2. Now try to find a constant that is a lower bound of all	*
   * varibles in list vars.  If any exists, then we must return		*
   * false, since there cannot be any values of those variables		*
   * whose meet is bottom.						*
   *									*
   * First check that all of the variables have constant lower bounds.  *
   * Then, if they do, take the meet of those lower bounds.		*
   *--------------------------------------------------------------------*/

  for(k = 0; k < num_vars; k++) {
    if(const_var_lb[k] == NULL) goto phase3;
  }

  bump_type(overall_const_lb = const_var_lb[0]);
  for(k = 1; k < num_vars; k++) {
    SET_TYPE(overall_const_lb, type_meet(overall_const_lb, const_var_lb[k]));
    if(overall_const_lb == NULL) goto phase3;
  }
  drop_type(overall_const_lb);
  result = FALSE;
  goto out;

 phase3:

  /*------------------------------------------------------------*
   * Phase 3. Possibly add more variables to the lower bound 	*
   * lists var_lb_list[k].  If X >= C >= Y, then add Y as a	*
   * lower bound to X.  These relationships have not been added	*
   * yet.							*
   *------------------------------------------------------------*/

  for(k = 0; k < num_vars; k++) {
    TYPE* C = const_var_lb[k];
    if(C != NULL) {
      for(p = lb_vars; p != NIL; p = p->tail) {
        TYPE* L = p->head.type;
	if(L->ctc == C->ctc || ancestor_tm(C->ctc, L->ctc)) {
	  SET_LIST(var_lb_list[k], add_var_to_list(L, var_lb_list[k]));
	}
      }
    }
  }

  /*--------------------------------------------------------------------*
   * Phase 4. Find the intersection of all of the var_lb_list lists.	*
   * If it is nonempty, then we must return false, since the common	*
   * member is a common lower bound of all of the variables in 		*
   * list vars.								*
   *--------------------------------------------------------------------*/

  k = 0;
  bump_list(common_var_list = var_lb_list[0]);
  if(common_var_list == NIL) goto add_bound_var;  /* below */
  for(k = 1; k < num_vars; k++) {
    SET_LIST(common_var_list, 
	     var_list_intersect(common_var_list, var_lb_list[k]));
    if(common_var_list == NIL) goto add_bound_var; /* below */
  }
  drop_list(common_var_list);
  result = FALSE;
  goto out;

 add_bound_var:

  {TYPE* restr_var = list_subscript(vars, k+1)->head.type;
#  ifdef DEBUG
     if(trace_overlap) trace_t(586, restr_var);
#  endif
   add_restrictively_bound_var(restr_var);
   result = TRUE;
  }

 out:

  /*------------------------------*
   * Clean up and return result.  *
   *------------------------------*/

  for(k = 0; k < num_vars; k++) {
    drop_list(var_lb_list[k]);
    drop_type(const_var_lb[k]);
  }
  drop_list(lb_vars);
  FREE(var_lb_list);
  FREE(const_var_lb);
  return result;
}


/****************************************************************
 *			CHECK_CLUSTERS				*
 ****************************************************************
 * Return true if there is any cluster in t2 that can be	*
 * violated while satisfying all of the constraints in t1.	*
 * This function helps restrictive5.  See that function for	*
 * details.							*
 *								*
 * t2_vars is a list of the main variables in t2, and t2_tbl	*
 * is a table that associates with each variable in t2_vars	*
 * the corresponding variable in t2.				*
 ****************************************************************/

PRIVATE Boolean 
check_clusters(TYPE_LIST *t2_vars, HASH2_TABLE *t2_tbl,
	       TYPE_LIST *correspondences)
{
  TYPE_LIST *p, *q, *r;
  for(p = t2_vars; p != NIL; p = p->tail) {
    TYPE* V2 = find_u(p->head.type);
    TYPE* V1 = get_corresponding_var(V2, t2_tbl, NIL);
    if(V1 == NULL) {
       
      /*----------------------------------------------------------*
       * V2 is an aux-variable.  Get its upper bounds in t2, but  *
       * use the corresponding variables in t1 when building the  *
       * list.  Those are the upper part of this cluster, and it  *
       * is only the upper part that needs to be checked.	   *
       *----------------------------------------------------------*/

      TYPE_LIST* upbs = NIL;
      for(q = t2_vars; q != NIL; q = q->tail) {
	TYPE* Y2 = find_u(q->head.type);
	TYPE* Y1 = get_corresponding_var(Y2, t2_tbl, correspondences);
	if(Y1 != NULL) {
	  for(r = Y2->LOWER_BOUNDS; r != NULL; r = r->tail) {
	    if(LKIND(r) == TYPE_L && find_u(r->head.type) == V2) {
	      upbs = type_cons(Y1, upbs);
	      break;
	    }
	  }
	}
      }

      if(upbs != NIL) {
	bump_list(upbs);
	if(check_cluster(upbs)) {
	  drop_list(upbs);
	  return TRUE;
	}
	drop_list(upbs);
      }
    }
  }

  return FALSE;
}
