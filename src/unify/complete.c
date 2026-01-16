/************************************************************************
 * File:    unify/complete.c
 * Purpose: Utilities for finding a type that is in one polymorphic
 *          type, but not in others.
 * Author:  Karl Abrahamson
 ************************************************************************/

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
 * This file is responsible for comparing a polymorphic type to a list  *
 * of polymorphic types.  The main function is missing_type, which	*
 * finds a type that belongs to one polymorphic type, but not to any of	*
 * a list of others, or says that no such type exists.			*
 * 									*
 * We need to do the same flavor of computation to determine whether a	*
 * given set of patterns covers all possible values, when doing		*
 * exhaustiveness tests for choose-matching constructs.  So we arrange	*
 * to make it possible to use these functions for patterns as well as	*
 * for types.  That is done by converting patterns to "pseudo-types".	*
 * That conversion is not done here, but is done in patmatch/pmcompl.c.	*
 * Here, we handle pseudo-types as well as normal types.		*
 *									*
 *		IMPORTANT NOTE						*
 *	 								*
 *   THE ALGORITHM THAT IS IMPLEMENTED HERE IS INCORRECT.               *
 *   I DO NOT KNOW WHETHER THIS PROBLEM IS COMPUTABLE.			*
 *									*
 * This algorithm does not report all possible missing types. 		*
 * Sometimes it reports no missing types even though some do exist.	*
 * If you are reading this, you are invited to find (and implement)	*
 * a better algorithm, or prove that no correct algorithm exists.	*
 ************************************************************************/

#include "../misc/misc.h"
#include "../parser/tokens.h"
#include "../utils/hash.h"
#include "../utils/lists.h"
#include "../alloc/allocate.h"
#include "../classes/classes.h"
#include "../clstbl/classtbl.h"
#include "../clstbl/meettbl.h"
#include "../standard/stdtypes.h"
#include "../unify/unify.h"
#include "../error/error.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

PRIVATE TYPE* main_missing_type(TYPE_LIST *source, TYPE *t, int split_depth);
PRIVATE TYPE* missing_type_with_split(TYPE_LIST *source, TYPE *t, TYPE *V,
				      int split_depth);

/****************************************************************
 *		missing_type_use_fictitious_types		*
 ****************************************************************
 * missing_type_use_fictitious_types is true if missing_type    *
 * should consider fictitious types when looking for types.     *
 ****************************************************************/

Boolean missing_type_use_fictitious_types = TRUE;

/****************************************************************
 *			MISSING_TYPE				*
 ****************************************************************
 * Return a type (an expression representing a set of		*
 * species) that is disjoint with all of the types in list	*
 * source, but that is contained in the type t.  If no		*
 * such species can be found, return NULL. 			*
 *								*
 * t should not be null or contain any null types.		*
 *								*
 * NOTE: The algorithm that is currently employed sometimes 	*
 * returns NULL even when an appropriate type exists.		*
 *								*
 * Note: it is possible for the result to contain a variable 	*
 * that ranges over a genus that currently has no members.  	*
 * If that is not desired, then some action should be taken to  *
 * avoid that, by checking the result.				*
 *								*
 * XREF:							*
 *   Used in t_compl.c to handle expectation completness test.  *
 *								*
 *   Used in pmcompl.c to handle pattern match exhaustiveness	*
 *   testing.							*
 *								*
 *   Used in tables/globtbl.c to check for hidden expectations. *
 *								*
 *   Used in missing.c to check for use of missing definitions. *
 ****************************************************************/

TYPE* missing_type(TYPE_LIST *source, TYPE *t)
{
  TYPE *result;

# ifdef DEBUG
    int old_trace_unify = trace_unify;
    if(trace_missing_type > 0) {
      trace_unify = trace_overlap = trace_missing_type - 1;
    }
# endif

  /*--------------------------------------------*
   * Copy t, and restrict families in it.	*
   * (We should not find a missing type that    *
   * contains F(T) where T makes no sense for   *
   * family F.)					*
   *						*
   * If the restriction fails, then t is empty, *
   * so return NULL.   				*
   *--------------------------------------------*/

  bump_type(t);
  SET_TYPE(t, copy_type(t, 0));
  if(!commit_restrict_families_t(t)) {
    drop_type(t);
    return NULL;
  }
    
# ifdef DEBUG
    if(trace_missing_type || trace_pat_complete) tracenl();
# endif

  /*--------------------------------------------------------------------*
   * main_missing_type does the work.  With this algorithm, the		*
   * initial split_depth (the third parameter to main_missing_type)	*
   * needs to be quite small, since the cost is exponential in that	*
   * parameter, with a fairly large base.  Currently, it is set to 0.	*
   *--------------------------------------------------------------------*/

  bump_type(result = main_missing_type(source, t, 0));
  drop_type(t);

# ifdef DEBUG
    if(trace_missing_type || trace_pat_complete) tracenl();
    trace_unify = old_trace_unify;
# endif

  if(result != NULL) result->ref_cnt--;
  return result;
}


/****************************************************************
 *			MISSING_DEBUG1				*
 *			MISSING_DEBUG2				*
 *			MISSING_DEBUG3				*
 ****************************************************************
 * Print traces.						*
 ****************************************************************/

#ifdef DEBUG
PRIVATE void 
missing_debug1(TYPE_LIST *source, TYPE *t, int split_depth)
{
  trace_t(446);
  trace_t(82, split_depth);
  trace_ty(t); tracenl();
  trace_t(552);
  print_type_list_separate_lines(source, "  ");
}

/*---------------------------------------------------------*/

PRIVATE void 
missing_debug2(TYPE_LIST *newsource)
{
  trace_t(446);
  trace_t(88);
  print_type_list_separate_lines(newsource, "  ");
}

/*---------------------------------------------------------*/

PRIVATE void 
missing_debug3(TYPE_LIST *source, TYPE *t, TYPE *V)
{
  trace_t(447);
  trace_t(90);
  trace_ty(V); tracenl();
  trace_t(553);
  trace_ty(t); tracenl();
  trace_t(552);
  print_type_list_separate_lines(source, "  ");
  if(V == NULL) {
    trace_t(447);
    trace_t(91);
  }
  V = find_u(V);
  if(!IS_VAR_T(TKIND(V)) || V->TY1 != NULL) {
    trace_t(447);
    trace_t(92);
  }
}
#endif


/****************************************************************
 *			MAIN_MISSING_TYPE			*
 ****************************************************************
 * Same as missing_type, but does not copy t or restrict 	*
 * families.							*
 *								*
 * This function cuts off splitting prematurely.		*
 * (See missing_type_with_split.)  Parameter split_depth tells  *
 * how many times to split a non-splittable variable before	*
 * cutting off the search.					*
 ****************************************************************/

PRIVATE TYPE* 
main_missing_type(TYPE_LIST *source, TYPE *t, int split_depth)
{
  int ov;
  TYPE_LIST *p, *newsource;
  TYPE *s, *result, *V;

  if(t == NULL) return NULL;         /* Just in case */

  bump_list(source);
  bump_type(t);

# ifdef DEBUG
    if(trace_missing_type || trace_pat_complete) {
      missing_debug1(source, t, split_depth);
    }
# endif

  /*---------------------------------------------------------------*
   * Compare t to each type in list source.  If t is found to be   *
   * a subset of a member of source, then return NULL.  Otherwise, *
   * produce the list newsource of members of source that are	   *
   * not disjoint with t. 					   *
   *---------------------------------------------------------------*/

  newsource = NIL;
  for(p = source; p != NIL; p = p->tail) {
    s = p->head.type;

#   ifdef DEBUG
      if(trace_missing_type || trace_pat_complete) {
	trace_t(446);
        trace_t(83);
        print_two_types(s, t);
      }
#   endif

    ov = half_overlap_u(t, s);

    /*--------------------------------------------------------------*
     * If s and t are disjoint, ignore s -- it is not a constraint  *
     * on our choice. 						    *
     *--------------------------------------------------------------*/

    if(ov == DISJOINT_OV) {
#     ifdef DEBUG
        if(trace_missing_type || trace_pat_complete) {
	  trace_t(446);
	  trace_t(84);
	}
#     endif
    }

    /*----------------------------------------------------*
     * If t is a subset of s, then we should return NULL, *
     * since t is contained in just one of the types in   *
     * the source list.					  *
     *----------------------------------------------------*/

    else if(ov == EQUAL_OR_CONTAINED_IN_OV) {
#     ifdef DEBUG
        if(trace_missing_type || trace_pat_complete) {
	  trace_t(446);
	  trace_t(85);
	}
#     endif
      drop_type(t);
      drop_list(source);
      drop_list(newsource);
      return NULL;
    }

    /*-----------------------------------------------------------*
     * Otherwise, t and s overlap, and either t contains s or    *
     * neither contains the other.  Add s to the newsource list. *
     *-----------------------------------------------------------*/

    else {
#     ifdef DEBUG
        if(trace_missing_type || trace_pat_complete) {
	  trace_t(446);
          trace_t(86);
	  trace_ty(s);
	  trace_t(87);
        }
#     endif
      SET_LIST(newsource, type_cons(s, newsource));
    }
  }

# ifdef DEBUG
    if(trace_missing_type || trace_pat_complete) {
      missing_debug2(newsource);
    }
# endif

  /*----------------------------------------------------------------*
   * If control reaches here, and there are no types in newsource,  *
   * then t is disjoint from all types in list source. So t itself  *
   * is the answer.  						    *
   *								    *
   * Note: it is possible for t to contain a variable that ranges   *
   * over a genus that currently has no members.  		    *
   *----------------------------------------------------------------*/

  if(newsource == NIL) {
    drop_list(source);
    if(t != NULL) t->ref_cnt--;
    return t;
  }

  /*---------------------------------------------------------------*
   * If control reaches here, then t is too difficult to deal with *
   * directly.  Unify t with the first member of newsource, and	   *
   * get a variable V that is nontrivially bound during		   *
   * unification.  Try each possible concrete binding of V as a	   *
   * possible value.  This will reduce the number of variables in  *
   * t, except in the case where V gets bound to a structured type.*
   *---------------------------------------------------------------*/

  half_overlap_u(t, newsource->head.type);
  bump_type(V = get_restrictively_bound_var()); 
  if(V != NULL) {
    SET_TYPE(V, FIND_U_NONNULL(V));
    bump_type(result = 
      missing_type_with_split(newsource, t, V, split_depth));
  }
  else {
    /*----------------------*
     * Should not get here. *
     *----------------------*/
#   ifdef DEBUG
      if(trace) trace_t(89);
#   endif
    result = NULL;
  }

  drop_type(V);
  drop_type(t);
  drop_list(newsource);
  drop_list(source);
  if(result != NULL) result->ref_cnt--;
  return result;
}


/****************************************************************
 *			STRUCTURED_BINDING_INDICATOR		*
 ****************************************************************
 * Return an indication of the kinds of structured types that   *
 * are in positions in type src that correspond to positions	*
 * occupied by variable V in type target.			*
 * The return value is a combination of the following bits.	*
 *								*
 *  COR_FAM	V occurs in a position in target occupied by a	*
 *		family member in src.				*
 *								*
 *  COR_PAIR	V occurs in a position in target occupied by a	*
 *		product type in src.				*
 *								*
 *  COR_FUN	V occurs in a position in taget occupied by a	*
 *		function type in src.				*
 *								*
 * V should not be a bound variable.				*
 ****************************************************************/

#define COR_FAM  1
#define COR_PAIR 2
#define COR_FUN  4

PRIVATE int 
structured_binding_indicator(TYPE *V, TYPE *target, TYPE *src)
{
  TYPE_TAG_TYPE target_kind, src_kind;

  /*------------------------------------------------------------*
   * When target and src have different kinds, the only case of	*
   * interest is where target is V and src is structured.	*
   *------------------------------------------------------------*/

  IN_PLACE_FIND_U(src);
  IN_PLACE_FIND_U(target);
  src_kind    = TKIND(src);
  target_kind = TKIND(target);

  if(target_kind != src_kind) {
    if(target == V && IS_STRUCTURED_T(src_kind)) {
      if(src_kind == FAM_MEM_T) return COR_FAM;
      else if(src_kind == FUNCTION_T) return COR_FUN;
      else return COR_PAIR;
    }
    else return 0;
  }

  /*------------------------------------------------------------*
   * If src and target are both of the same structured kind	*
   * then handle the corresponding components.			*
   * Note that we only get here when src_kind == target_kind.   *
   *------------------------------------------------------------*/

  else if(IS_STRUCTURED_T(src_kind)) {
    return structured_binding_indicator(V, target->TY1, src->TY1) |
           structured_binding_indicator(V, target->TY2, src->TY2);
  }

  /*------------------------------------------------------------*
   * If src and target have the same kind, and they are not	*
   * structured, then there is nothing interesting here.	*
   *------------------------------------------------------------*/

  else return 0; 
}


/****************************************************************
 *		CHECK_FOR_STRUCTURED_BINDING			*
 ****************************************************************
 * Return an indication of the kinds of structured types that   *
 * are in positions, in some member of list L, that correspond	*
 * to positions occupied by variable V in type target.		*
 * The return value is a combination of the following bits.	*
 *								*
 *  COR_FAM	V occurs in target in a position occupied by a	*
 *		family member in some member of list source.	*
 *								*
 *  COR_PAIR	V occurs in target in a position occupied by a	*
 *		product type in some member of list source.	*
 *								*
 *  COR_FUN	V occurs in target in a position occupied by a	*
 *		function type in some member of list source.	*
 ****************************************************************/

PRIVATE int
check_for_structured_binding(TYPE *V, TYPE *target, TYPE_LIST *source)
{
  int result;
  TYPE_LIST *p;

  IN_PLACE_FIND_U(V);
  IN_PLACE_FIND_U(target);
  result = 0;
  for(p = source; p != NIL; p = p->tail) {
    result |= structured_binding_indicator(V, target, p->head.type);
  }
  return result;
}


/****************************************************************
 *			MAKE_STRUCTURED_BINDING_TABLE		*
 ****************************************************************
 * Put into table tbl an entry for each unbound variable in     *
 * type t.  The entry for variable V tells what kinds of        *
 * types occur in members of list source in positions that 	*
 * correspond to positions of variable V in target, as an OR of *
 * bits COR_FAM (a family member), COR_PAIR (a product) and 	*
 * COR_FUN (a function).					*
 *								*
 * The table should be NULL on entry from outside.  If there is *
 * already an entry for a variable, no further information is   *
 * added for that variable.					*
 * 								*
 * The keys in the table are not reference counted.		*
 ****************************************************************/

PRIVATE void 
make_structured_binding_table(HASH2_TABLE **tbl, TYPE *t, TYPE *target, 
			      TYPE_LIST *source)
{
  TYPE_TAG_TYPE t_kind;

 tail_recur:
  if(t == NULL) return;
  else {
    IN_PLACE_FIND_U(t);
    t_kind = TKIND(t);
    if(IS_STRUCTURED_T(t_kind)) {
      make_structured_binding_table(tbl, t->TY1, target, source);
      t = t->TY2;
      goto tail_recur;
    }

    else if(IS_VAR_T(t_kind)) {
      HASH_KEY u;
      HASH2_CELLPTR h;
      u.num = (LONG) t;
      h = insert_loc_hash2(tbl, u, inthash(u.num), eq);
      if(h->key.num == 0) {
	h->key.num = u.num;
        h->val.num = check_for_structured_binding(t, target, source);
      }
    }
  }
}


/****************************************************************
 *		       MARK_EXTRA_NOSPLIT			*
 ****************************************************************
 * For each bound variable X that occurs in t, do the		*
 * following.							*
 *								*
 * 1. If X is marked nonsplittable then mark all of the 	*
 *    variables in the type to which X is bound nonsplittable	*
 *    as well.							*
 *								*
 * 2. If X has an entry in table tbl and X is bound to a kind	*
 *    of type that is not mentioned in the value of X in tbl, 	*
 *    then mark all variables in the type that X is bound to as	*
 *    nonsplittable.  						*
 *								*
 *    For example, if X is bound to a family member in t, but	*
 *    the value stored with X in table tbl does not have its	*
 *    COR_FAM bit set, then all of the variables in the type to *
 *    which X is bound will be marked nonsplittable.		*
 *								*
 * The value associated with a variable in table tbl is an OR   *
 * of COR_FAM, COR_PAIR and COR_FUN bits.			*
 ****************************************************************/

PRIVATE void
mark_extra_nosplit(HASH2_TABLE *tbl, TYPE *t)
{
 tail_recur:
  if(t == NULL) return;
  else {
    TYPE_TAG_TYPE t_kind = TKIND(t);
    if(IS_STRUCTURED_T(t_kind)) {
      mark_extra_nosplit(tbl, t->TY1);
      t = t->TY2;
      goto tail_recur;
    }

    else if(t_kind >= FIRST_BOUND_T) {

      /*---------------------------------------------------*
       * We are really only interested in bound variables  *
       * and MARK_T nodes, which have a nonnull TY1 field. *
       *---------------------------------------------------*/

      if(t->TY1 != NULL) {

	/*--------------------------------------------------------------*
         * If this is a bound variable then check whether its binding   *
         * needs to be marked nonsplittable.				*
         *--------------------------------------------------------------*/

	if(IS_VAR_T(t_kind)) {
	  if(t->nosplit) {
	    mark_no_split(t);
	    return;
	  }
	  else {
	    HASH_KEY u;
	    HASH2_CELLPTR h;
	    u.num = (LONG) t;
	    h = locate_hash2(tbl, u, inthash(u.num), eq);
	    if(h->key.num != 0) {
	      int allowed_kinds = h->val.num;
	      TYPE* bound_to = find_u(t);
	      TYPE_TAG_TYPE bound_to_kind = TKIND(bound_to);
	      if((bound_to_kind == FAM_MEM_T && 
		  (allowed_kinds & COR_FAM) == 0) ||
		 (bound_to_kind == PAIR_T && 
		  (allowed_kinds & COR_PAIR) == 0) ||
		 (bound_to_kind == FUNCTION_T && 
		  (allowed_kinds & COR_FUN) == 0)) {
		mark_no_split(bound_to);
		return;
	      }
	    }
	  }
	}

	/*------------------------------------------------------*
	 * If we did not do the nosplit mark on this bound 	*
	 * variable, then tail recur on the binding of this	*
	 * variable.         					*
	 *------------------------------------------------------*/

	t = t->TY1;
	goto tail_recur;
      }
    }
  }
}


/****************************************************************
 *			TRY_BINDING				*
 ****************************************************************
 * Same as missing_type, but before proceding, bind V to	*
 * bind_to.							*
 *								*
 * It is possible, due to constraints, that a variable other    *
 * than V gets bound when V is bound.  If any such variable X   *
 * is bound to a structured type, and variable X does not       *
 * occur in a position occupied by a similar structured type    *
 * in the source list, then all variables in the type that X is *
 * bound to are marked nonsplittable.  This kind of thing is    *
 * necessary to prevent an infinite recursion.			*
 ****************************************************************/

PRIVATE TYPE* 
try_binding(TYPE *bind_to, TYPE_LIST *source, TYPE *t, TYPE *V, 
	    int split_depth)
{
  LIST *mark;
  TYPE *result;

  HASH2_TABLE* tbl                = NULL;
  Boolean      V_has_lower_bounds = FALSE;

  /*------------------------------------------------------------*
   * If V has lower bounds, then make the table that associates *
   * with each variable an indication of the kinds of things	*
   * that it corresponds to in the source list.  This table is  *
   * needed below, but must be computed before binding V.	*
   *------------------------------------------------------------*/

  if(V->LOWER_BOUNDS != NIL) {
    V_has_lower_bounds = TRUE;
    make_structured_binding_table(&tbl, t, t, source);
  } 

  /*--------------------------------------------*
   * Now do the unification of V to bind_to.	*
   *--------------------------------------------*/

  bump_list(mark = finger_new_binding_list());
  if(UNIFY(V, bind_to, TRUE)) {

#   ifdef DEBUG
      if(trace_missing_type || trace_pat_complete) {
	trace_t(447);
	trace_t(97, V);
	trace_ty(bind_to); tracenl();
      }
#   endif

    /*----------------------------------------------------------*
     * If V has lower bounds then it is possible that binding   *
     * V causes other variables to become bound.  If such a     *
     * variable X was marked nonsplittable, then any variables  *
     * in its binding must also be nonsplittable.  If such an X *
     * got bound to a structured type, then the variables in    *
     * that structured type might have to be marked		*
     * nonsplittable even if X is splittable.			*
     *----------------------------------------------------------*/

    if(V_has_lower_bounds) {
      mark_extra_nosplit(tbl, t);
      free_hash2(tbl);
    }

    /*----------------------------------------------------*
     * Try to find a missing type under this binding.  If *
     * successful, return the result.  If not successful, *
     * undo the binding.				  *
     *----------------------------------------------------*/

    bump_type(result = main_missing_type(source, t, split_depth));
    if(result != NULL) {
      drop_list(mark);
      result->ref_cnt--; 
      return result;
    }
    else {
      undo_bindings_u(mark);
    }
  }
  drop_list(mark);
  return NULL;
}


/****************************************************************
 *		   TRY_FICTITIOUS_PRIMARY_SPECIES		*
 ****************************************************************
 * Try to bind find a missing type for list source in type t by *
 * binding V to a fictitious primary member of the genus or     *
 * community describe by class table entry ctc.			*
 *								*
 * Return the type found, or NULL if none is found.		*
 *								*
 * This function tries two different fictitious primaries.  But *
 * it only tries the second one if t already contains the first *
 * one.								*
 ****************************************************************/

PRIVATE TYPE* 
try_fictitious_primary_species(TYPE_LIST *source, TYPE *t, TYPE *V, 
			       CLASS_TABLE_CELL *ctc, int split_depth)
{
  TYPE *this_type, *result;

  /*---------------------------------------------*
   * Try the first fictitious species or family. *
   *---------------------------------------------*/

  bump_type(this_type = fictitious_tf(ctc, 1));
  bump_type(result = try_binding(this_type, source, t, V, split_depth));
  if(result != NULL) {
    drop_type(this_type);
    result->ref_cnt--;
    return result;
  }

  /*----------------------------------------------------*
   * Try the second fictitious species or family, but   *
   * only if the first occurs in t.			*
   *----------------------------------------------------*/

  if(occurs_in(this_type, t)) {
    drop_type(this_type);
    bump_type(this_type = fictitious_tf(ctc, 2));
    bump_type(result = try_binding(this_type, source, t, V, split_depth));
    drop_type(this_type);
    if(result != NULL) {
      result->ref_cnt--;
      return result;
    }
  }
  else drop_type(this_type);
  return NULL;
}


/****************************************************************
 *			MISSING_TYPE_SIMPLE_BINDINGS		*
 ****************************************************************
 * Same as missing_type_with_split, below, but only tries       *
 * bindings of V to primary nonfictitious species or families.  *
 * It does not try structured or wrapped species.		*
 ****************************************************************/

PRIVATE TYPE* 
missing_type_simple_bindings(TYPE_LIST *source, TYPE *t, TYPE *V,
			     int split_depth)
{
  int i, t_code;
  TYPE *result;

  TYPE_TAG_TYPE    V_kind = TKIND(V);
  CLASS_TABLE_CELL* V_ctc = V->ctc;

  /*---------------------------------------------------------------*
   * Here, handle genuine species or family variables (as opposed  *
   * to pseudo-variables used for pattern match exhaustiveness     *
   * tests). 							   *
   *---------------------------------------------------------------*/

  if(!V->special) {
    t_code = (MEMBER(V_kind, fam_tkind_set)) ? FAM_ID_CODE : TYPE_ID_CODE;

#   ifdef DEBUG
      if(trace_missing_type || trace_pat_complete) {
	trace_t(447);
	trace_t(547);
      }
#   endif

    for(i = 0; i < next_class_num; i++) {
      CLASS_TABLE_CELL* ctci  = ctcs[i];
      int               codei = ctci->code;
      TYPE *this_type;

      if(codei == t_code && ancestor_tm(V_ctc, ctci)) {

	bump_type(this_type = ctci->ty);
	bump_type(result = 
		  try_binding(this_type, source, t, V, split_depth));
	drop_type(this_type);
	if(result != NULL) {
	  result->ref_cnt--;
	  return result;
	}
      }
    }
  } /* end if(!V->special) */

  /*----------------------------------------------------*
   * Here, handle pseudo-variables (for pattern match   *
   * exhaustiveness check). 				*
   *----------------------------------------------------*/

  else { /* V->special */
    TYPE *this_t;

    TYPE_TAG_TYPE this_t_kind = (V_kind == TYPE_VAR_T) ? TYPE_ID_T : FAM_T;
    int                     n = V->TOFFSET;   /* Number of parts */

    for(i = 0; i < n; i++) {
      this_t = new_type(this_t_kind, NULL);
      this_t->special = 1;
      this_t->TOFFSET = i;
      bmp_type(V->TY1 = this_t);

#     ifdef DEBUG
	if(trace_missing_type || trace_pat_complete) {
	  trace_t(447);
	  trace_t(94, i);
	}
#     endif

      result = main_missing_type(source, t, split_depth);
      if(result != NULL) return result;
      else SET_TYPE(V->TY1, NULL_T);
    }

  } /* end for(i = ...) */

  return NULL;
}


/****************************************************************
 *	      MISSING_TYPE_SIMPLE_FICTITIOUS_BINDINGS		*
 ****************************************************************
 * Same as missing_type_with_split, below, but only tries       *
 * bindings of V to primary fictitious species or families.     *
 * It does not try structured or wrapped species.		*
 ****************************************************************/

PRIVATE TYPE* 
missing_type_simple_fictitious_bindings
  (TYPE_LIST *source, TYPE *t, TYPE *V, int split_depth)
{
  int i, g_code;
  TYPE *result;

  /*------------------------------------------------------------*
   * Fictitious species or families are not used for 		*
   * pseudo-types, so only handle for genuine variables.	*
   *------------------------------------------------------------*/

  if(!V->special) {
    TYPE_TAG_TYPE    V_kind = TKIND(V);
    CLASS_TABLE_CELL* V_ctc = V->ctc;

    g_code = (MEMBER(V_kind, fam_tkind_set)) ? COMM_ID_CODE : GENUS_ID_CODE;

#   ifdef DEBUG
      if(trace_missing_type || trace_pat_complete) {
        trace_t(447);
        trace_t(573);
      }
#   endif

    for(i = 0; i < next_cg_num; i++) {
      CLASS_TABLE_CELL* ctci  = vctcs[i];
      int               codei = ctci->code;

      if(codei == g_code && ancestor_tm(V_ctc, ctci)) {
	if(ctci->extensible > 1) {
	  result = try_fictitious_primary_species(source, t, V, ctci, 
						  split_depth);
	  if(result != NULL) return result;
	}
      }
    }
  }

  return NULL;
}


/****************************************************************
 *		TRY_FICTITIOUS_WRAPPED_SPECIES			*
 ****************************************************************
 * Try to bind find a missing type for list source in type t by *
 * binding V to a wrap-type for a fictitious subgenus or	*
 * subcommunity of the genus or community describe by class     *
 * table entry ctc.						*
 *								*
 * Return the type found, or NULL if none is found.		*
 *								*
 * This function tries two different fictitious types.  But     *
 * it only tries the second one if t already contains the first *
 * one.								*
 ****************************************************************/

PRIVATE TYPE* 
try_fictitious_wrapped_species(TYPE_LIST *source, TYPE *t, TYPE *V, 
			       CLASS_TABLE_CELL *ctc, int split_depth)
{
  TYPE *this_type, *result;

  /*---------------------------------------------*
   * Try the first fictitious species or family. *
   *---------------------------------------------*/

  bump_type(this_type = fictitious_wrap_tf(ctc, 1));
  bump_type(result = try_binding(this_type, source, t, V, split_depth));
  if(result != NULL) {
    drop_type(this_type);
    result->ref_cnt--;
    return result;
  }

  /*----------------------------------------------------*
   * Try the second fictitious species or family, but	*
   * only if the first occurs in t.			*
   *----------------------------------------------------*/

  if(occurs_in(this_type, t)) {
    drop_type(this_type);
    bump_type(this_type = fictitious_wrap_tf(ctc, 2));
    bump_type(result = try_binding(this_type, source, t, V, split_depth));
    drop_type(this_type);
    if(result != NULL) {
      result->ref_cnt--;
      return result;
    }
  }
  else drop_type(this_type);

  return NULL;
}


/****************************************************************
 *			MISSING_TYPE_WRAP_BINDINGS		*
 ****************************************************************
 * Same as missing_type_with_split, below, but only tries       *
 * bindings of V to wrapped nonfictitious species or families.	*
 ****************************************************************/

PRIVATE TYPE* 
missing_type_wrap_bindings(TYPE_LIST *source, TYPE *t, TYPE *V, 
			   int split_depth)
{
  int i, g_code;
  TYPE *bind_to, *result;

  /*------------------------------------------------------------*
   * Wrapped species or families are not used for pseudo-types, *
   * so only handle for genuine variables.			*
   *------------------------------------------------------------*/

  if(!V->special) {
    TYPE_TAG_TYPE     V_kind = TKIND(V);
    CLASS_TABLE_CELL* V_ctc  = V->ctc;

#   ifdef DEBUG
      if(trace_missing_type || trace_pat_complete) {
        trace_t(447);
        trace_t(194);
      }
#   endif

    g_code = (MEMBER(V_kind, fam_tkind_set)) ? COMM_ID_CODE : GENUS_ID_CODE;

    for(i = 0; i < next_cg_num; i++) {
      CLASS_TABLE_CELL* ctci  = vctcs[i];
      int               codei = ctci->code;

      if(codei == g_code && 
	 (V_ctc == ctci || ancestor_tm(V_ctc, ctci))) {
	bmp_type(bind_to = wrap_tf(ctci));
	bump_type(result = try_binding(bind_to, source, t, V, split_depth));
	drop_type(bind_to);
	if(result != NULL)  {
	  result->ref_cnt--;
	  return result;
	}
      }
    }
  } /* end if(!V->special) */

  return NULL;
}


/****************************************************************
 *		MISSING_TYPE_WRAP_FICTITIOUS_BINDINGS	        *
 ****************************************************************
 * Same as missing_type_with_split, below, but only tries       *
 * bindings of V to wrapped fictitious species or families.	*
 ****************************************************************/

PRIVATE TYPE* 
missing_type_wrap_fictitious_bindings
  (TYPE_LIST *source, TYPE *t, TYPE *V, int split_depth)
{
  int i, g_code;
  TYPE *result;

  /*------------------------------------------------------------*
   * Wrapped species or families are not used for pseudo-types, *
   * so only handle for genuine variables.			*
   *------------------------------------------------------------*/

  if(!V->special) {
    TYPE_TAG_TYPE     V_kind = TKIND(V);
    CLASS_TABLE_CELL* V_ctc  = V->ctc;

#   ifdef DEBUG
      if(trace_missing_type || trace_pat_complete) {
        trace_t(447);
        trace_t(266);
      }
#   endif

    g_code = (MEMBER(V_kind, fam_tkind_set)) ? COMM_ID_CODE : GENUS_ID_CODE;

    for(i = 0; i < next_cg_num; i++) {
      CLASS_TABLE_CELL* ctci  = vctcs[i];
      int               codei = ctci->code;

      if(codei == g_code && 
	 (V_ctc == ctci || ancestor_tm(V_ctc, ctci))) {
	if(ctci->extensible > 1) {
	  result = try_fictitious_wrapped_species(source, t, V, ctci, 
						  split_depth);
	  if(result != NULL) return result;
	}
      }
    }
  } /* end if(!V->special) */

  return NULL;
}


/****************************************************************
 *			BIND_FAM_HELP				* 
 ****************************************************************
 * Same as missing_type_with_split, but only tries  		*
 * binding of V to family fam(T), where T is an appropriate type*
 * for family fam.  Parameter i is the ctc_num of the class     *
 * table entry for F, and is used to determine T.		*
 *								*
 * try_kinds contains bit COR_FAM if the argument of fam 	*
 * should be a splittable variable.  If it does not contain bit *
 * COR_FAM, then the argument is a nonsplittable variable.	*
 ****************************************************************/

PRIVATE TYPE* 
bind_fam_help(TYPE *fam, int i, TYPE_LIST *source, 
	      TYPE *t, TYPE *V, int try_kinds, int split_depth)

{
  TYPE *bind_to, *result;
  int arg_num;
  TYPE *ty1;

  CLASS_TABLE_CELL* V_ctc = V->ctc;

  /*-----------------------------------------------*
   * We are going to bind V to fam(ty1).  Get ty1. *
   * Then build bind_to = fam(ty1).		   *
   *-----------------------------------------------*/

  arg_num = (V_ctc == NULL) ? 0 : get_link_label_tm(V_ctc->num, i).label1;
  ty1 = var_t(ctcs[arg_num]);
  bump_type(bind_to = fam_mem_t(fam, ty1));

  /*------------------------------------*
   * Restrict the family if necessary.	*
   * Mark the variables in ty1 as	*
   * nonsplittable if appropriate.	*
   *------------------------------------*/

  if(!commit_restrict_families_t(bind_to)) {
    drop_type(bind_to);
    return NULL;
  }
  if(!(try_kinds & COR_FAM)) {
    mark_no_split(ty1);
  }

  /*------------------*
   * Try the binding. *
   *------------------*/

  bump_type(result = try_binding(bind_to, source, t, V, split_depth));
  drop_type(bind_to);
  if(result != NULL) result->ref_cnt--;
  return result;
}


/****************************************************************
 *		   MISSING_TYPE_PRIMARY_FAMILY_BINDINGS		*
 ****************************************************************
 * Same as missing_type_with_split, below, but only tries       *
 * bindings of V to family members where the family is primary	*
 * and not fictitious.						*
 *								*
 * try_kinds contains bit COR_FAM if general family bindings 	*
 * should be tried.  If it does not contain bit COR_FAM, then	*
 * family bindings are still tried, but their variables are	*
 * marked nonsplittable.					*
 ****************************************************************/

PRIVATE TYPE* 
missing_type_primary_family_bindings(TYPE_LIST *source, TYPE *t, TYPE *V, 
				     int try_kinds, int split_depth)
{
  int i;
  TYPE *result, *this_fam;

  CLASS_TABLE_CELL* V_ctc = V->ctc;

# ifdef DEBUG
    if(trace_missing_type || trace_pat_complete) {
      trace_t(447);
      trace_t(241);
    }
# endif

  for(i = 1; i < next_class_num; i++) {
    CLASS_TABLE_CELL* ctci  = ctcs[i];
    int               codei = ctci->code;

    /*--------------------------------------------------------*
     * For each family F, we must try binding V to F(X).      *
     *--------------------------------------------------------*/

    if(codei == FAM_ID_CODE && ancestor_tm(V_ctc, ctci)) {
      bump_type(this_fam = ctci->ty);
      bump_type(result = 
		bind_fam_help(this_fam, i, source, t, V, try_kinds, 
			      split_depth));
      drop_type(this_fam);
      if(result != NULL) {
	result->ref_cnt--;
	return result;
      }
    }  
  } /* end for */

  return NULL;
}


/****************************************************************
 *	     MISSING_TYPE_PRIMARY_FICTITIOUS_FAMILY_BINDINGS	*
 ****************************************************************
 * Same as missing_type_with_split, below, but only tries       *
 * bindings of V to family members where the family is primary	*
 * and fictitious.						*
 *								*
 * try_kinds contains bit COR_FAM if general family bindings 	*
 * should be tried.  If it does not contain bit COR_FAM, then	*
 * family bindings are still tried, but variables are marked	*
 * nonsplittable.						*
 ****************************************************************/

PRIVATE TYPE* 
missing_type_primary_fictitious_family_bindings
(TYPE_LIST *source, TYPE *t, TYPE *V, int try_kinds, int split_depth)
{
  int i;
  TYPE *result, *this_fam;

  CLASS_TABLE_CELL* V_ctc = V->ctc;

# ifdef DEBUG
    if(trace_missing_type || trace_pat_complete) {
      trace_t(447);
      trace_t(574);
    }
# endif

  for(i = 1; i < next_cg_num; i++) {
    CLASS_TABLE_CELL* ctci  = vctcs[i];
    int               codei = ctci->code;

    /*--------------------------------------------------------*
     * For each fictitious family F, we must try binding V to *
     * F(X).      					      *
     *--------------------------------------------------------*/

    if(codei == COMM_ID_CODE && ancestor_tm(V_ctc, ctci) && ctci->extensible) {

      /*--------------------------*
       * First fictitious family. *
       *--------------------------*/

      bump_type(this_fam = fictitious_tf(ctci, 1));
      bump_type(result = 
		bind_fam_help(this_fam, ctci->num, source, t, V, try_kinds,
			      split_depth));
      if(result != NULL) {
	drop_type(this_fam);
	result->ref_cnt--;
	return result;
      }

      /*--------------------------------------*
       * Second fictitious family -- but only *
       * if first already occurs in t.        *
       *--------------------------------------*/

      if(occurs_in(this_fam, t)) {
	drop_type(this_fam);
	bump_type(this_fam = fictitious_tf(ctci, 2));
	bump_type(result = 
		  bind_fam_help(this_fam, ctci->num, source, t, V, 
				try_kinds, split_depth));
	drop_type(this_fam);
	if(result != NULL) {
	  result->ref_cnt--;
	  return result;
	}
      }
      else drop_type(this_fam);
    }
  } /* end for */

  return NULL;
}


/****************************************************************
 *		   MISSING_TYPE_WRAP_FAMILY_BINDINGS		*
 ****************************************************************
 * Same as missing_type_with_split, below, but only tries       *
 * bindings of V to family members where the family is wrapped 	*
 * and not fictitious.						*
 *								*
 * try_kinds contains bit COR_FAM if general family bindings 	*
 * should be tried.  If it does not contain bit COR_FAM, then	*
 * family bindings are still tried, but variables are marked	*
 * nonsplittable.						*
 ****************************************************************/

PRIVATE TYPE* 
missing_type_wrap_family_bindings(TYPE_LIST *source, TYPE *t, TYPE *V, 
				  int try_kinds, int split_depth)
{
  int i;
  TYPE *result, *this_fam;

  CLASS_TABLE_CELL* V_ctc = V->ctc;

# ifdef DEBUG
    if(trace_missing_type || trace_pat_complete) {
      trace_t(447);
      trace_t(95);
    }
# endif

  for(i = 1; i < next_cg_num; i++) {
    CLASS_TABLE_CELL* ctci  = vctcs[i];
    int               codei = ctci->code;

    /*------------------------------------------------*
     * For each community C, we must try binding V to *
     * C(X) (using wrapped family C).  		      *
     *------------------------------------------------*/

    if(codei == COMM_ID_CODE && ancestor_tm(V_ctc, ctci)) {
      bump_type(this_fam = wrap_tf(ctci));
      bump_type(result = 
		bind_fam_help(this_fam, ctci->num, source, t, V, 
			      try_kinds, split_depth));
      drop_type(this_fam);
      if(result != NULL) {
	result->ref_cnt--;
	return result;
      }
    }
  } /* end for */

  return NULL;
}


/****************************************************************
 *	    MISSING_TYPE_WRAP_FICTITIOUS_FAMILY_BINDINGS	*
 ****************************************************************
 * Same as missing_type_with_split, below, but only tries       *
 * bindings of V to family members where the family is wrapped 	*
 * and fictitious.						*
 *								*
 * try_kinds contains bit COR_FAM if general family bindings 	*
 * should be tried.  If it does not contain bit COR_FAM, then	*
 * family bindings are still tried, but variables are marked	*
 * nonsplittable.						*
 ****************************************************************/

PRIVATE TYPE* 
missing_type_wrap_fictitious_family_bindings
  (TYPE_LIST *source, TYPE *t, TYPE *V, int try_kinds, int split_depth)
{
  int i;
  TYPE *result, *this_fam;

  CLASS_TABLE_CELL* V_ctc = V->ctc;

# ifdef DEBUG
    if(trace_missing_type || trace_pat_complete) {
      trace_t(447);
      trace_t(575);
    }
# endif

  for(i = 1; i < next_cg_num; i++) {
    CLASS_TABLE_CELL* ctci  = vctcs[i];
    int               codei = ctci->code;

    /*------------------------------------------------*
     * For each community C, we must try binding V to *
     * FC1(X) and FC2(X), where FC1 and FC2 are two   *
     * wrapped families for two fictitious communities*
     * that are beneath C.			      *
     *------------------------------------------------*/

    if(codei == COMM_ID_CODE && ancestor_tm(V_ctc, ctci) && ctci->extensible) {

      /*---------*
       * FC1(X)  *
       *---------*/

      bump_type(this_fam = fictitious_wrap_tf(ctci, 1));
      bump_type(result = 
		bind_fam_help(this_fam, ctci->num, source, t, V, 
			      try_kinds, split_depth));
      if(result != NULL) {
	drop_type(this_fam);
	result->ref_cnt--;
	return result;
      }

      /*-------------------------------------------------------*
       * FC2(X).  Only do a second one if the first one occurs *
       * in t.  					       *
       *-------------------------------------------------------*/

      if(occurs_in(this_fam, t)) {
	drop_type(this_fam);
	bump_type(this_fam = fictitious_wrap_tf(ctci, 2));
	bump_type(result = 
		  bind_fam_help(this_fam, ctci->num, source, t, V, 
				try_kinds, split_depth));
	drop_type(this_fam);
	if(result != NULL) {
	  result->ref_cnt--;
	  return result;
	}
      }
      else drop_type(this_fam);
    }
  } /* end for */

  return NULL;
}


/****************************************************************
 *			MISSING_TYPE_PAIR_BINDINGS		*
 ****************************************************************
 * Same as missing_type_with_split, below, but only tries       *
 * bindings of V to products and functions.  ctci should be	*
 * either Pair_ctc or Function_ctc.  It tells which kind of 	*
 * binding to do.						*
 *								*
 * try_kinds contains bit COR_PAIR if general pair bindings	*
 * should be tried, and COR_FUN if general function bindings	*
 * should be tried.  When it does not contain the appropriate	*
 * bit, pair or function bindings are still tried, but          *
 * variables are marked nonsplittable.				*
 ****************************************************************/

PRIVATE TYPE* 
missing_type_pair_bindings_help(CLASS_TABLE_CELL *ctci,
			        TYPE_LIST *source, TYPE *t, 
				TYPE *V, int try_kinds, int split_depth)
{
  CLASS_TABLE_CELL* V_ctc = V->ctc;
  int codei = ctci->code;

  if(ancestor_tm(V_ctc, ctci)) {
    TYPE *ty1, *ty2, *bind_to, *result;
    TYPE_TAG_TYPE pkind;
    int poly;

    if(codei == PAIR_CODE) {
      pkind = PAIR_T;
      poly  = try_kinds & COR_PAIR;
    }
    else {
      pkind = FUNCTION_T;
      poly  = try_kinds & COR_FUN;
    }

#   ifdef DEBUG
      if(trace_missing_type || trace_pat_complete) {
	trace_t(447);
	trace_t(96, codei);
      }
#   endif

    ty1 = var_t(V_ctc);
    ty2 = var_t(V_ctc);
    bump_type(bind_to = new_type2(pkind, ty1, ty2));
    if(!poly) {
      mark_no_split(ty1);
      mark_no_split(ty2);
    }
    bump_type(result = try_binding(bind_to, source, t, V, split_depth));
    drop_type(bind_to);
    if(result != NULL) result->ref_cnt--;
    return result;
  }

  return NULL;
}

/*-----------------------------------------------------------*/

PRIVATE TYPE* 
missing_type_pair_bindings(TYPE_LIST *source, TYPE *t, TYPE *V, int try_kinds,
			   int split_depth)
{
  TYPE *result;

  result = missing_type_pair_bindings_help(Pair_ctc, source, t, V, 
					   try_kinds, split_depth);
  if(result != NULL) return result;

  return missing_type_pair_bindings_help(Function_ctc, source, t, V,
					 try_kinds, split_depth);
}


/****************************************************************
 *			MISSING_TYPE_WITH_SPLIT			*
 ****************************************************************
 * This is almost the same as missing_type.			*
 *								*
 * Return a type (or expression representing a set of		*
 * species) that is disjoint with all of the types in		*
 * list source, but that is contained in the set of species 	*
 * represented by expression t.  If there is			*
 * no such species, return NULL.				*
 *								*
 * Before doing any direct tests, bind variable V in all	*
 * possible ways.  						*
 ****************************************************************/

PRIVATE TYPE* 
missing_type_with_split(TYPE_LIST *source, TYPE *t, TYPE *V, int split_depth)
{
  TYPE *result; 
  TYPE_TAG_TYPE V_kind;
  int V_is_fam;
  Boolean V_is_primary, V_is_wrap;

  if(V == NULL) return NULL;   /* Just in case - should not happen. */

  V_kind       = TKIND(V);
  V_is_fam     = MEMBER(V_kind, fam_tkind_set);
  V_is_primary = IS_PRIMARY_VAR_T(V_kind);
  V_is_wrap    = IS_WRAP_VAR_T(V_kind);

# ifdef DEBUG
    if(trace_missing_type || trace_pat_complete) {
      missing_debug3(source, t, V);
    }
# endif

  /*-------------------------------------------------------*
   * Try each simple nonfictitious species or family.  If  *
   * successful, no need to look further.		   *
   *-------------------------------------------------------*/

  if(!V_is_wrap) {
    result = missing_type_simple_bindings(source, t, V, split_depth);
    if(result != NULL)  return result;
  }

  /*--------------------------------------------------------*
   * Try each wrapped nonfictitious species or family.  If  *
   * successful, no need to look further.		    *
   *--------------------------------------------------------*/

  if(!V_is_primary) {
    result = missing_type_wrap_bindings(source, t, V, split_depth);
    if(result != NULL) return result;
  }

  if(missing_type_use_fictitious_types) {

    /*-------------------------------------------------------*
     * Try each simple fictitious species or family.  If     *
     * successful, no need to look further.		     *
     *-------------------------------------------------------*/

    if(!V_is_wrap) {
      result = missing_type_simple_fictitious_bindings
	        (source, t, V, split_depth);
      if(result != NULL) return result;
    }

    /*----------------------------------------------------------*
     * Try each wrapped fictitious species or family.  If  	*
     * successful, no need to look further.		    	*
     *----------------------------------------------------------*/

    if(!V_is_primary) {
      result = missing_type_wrap_fictitious_bindings
                 (source, t, V, split_depth);
      if(result != NULL) return result;
    }
  }

  /*-------------------------------------------------------------*
   * Now we try structured bindings of V.			 * 
   *								 *
   * The remaining bindings are only done for species variables, *
   * not for family variables, and not for pseudo-types.	 *
   *								 *
   * Also, the following are only done when V is not marked	 *
   * as nonsplittable, unless split_depth is positive.  If we    *
   * split a variable marked nosplit, then split_depth is	 *
   * decreased.							 *
   *-------------------------------------------------------------*/

  if(!V_is_fam && !V->special && (split_depth > 0 || !V->nosplit)) {

    int try_kinds;

    if(V->nosplit) split_depth--;

    try_kinds = check_for_structured_binding(V, t, source);

    /*-------------------------------------------------------------*
     * Try family bindings for V.  For example, we might want      *
     * to bind V to a species such as [Natural].  		   *
     *-------------------------------------------------------------*/

    if(!V_is_wrap) {
      result = missing_type_primary_family_bindings(source, t, V, try_kinds,
						    split_depth);
      if(result != NULL) return result;
    }
    if(!V_is_primary) {
      result = missing_type_wrap_family_bindings(source, t, V, try_kinds,
						 split_depth);
      if(result != NULL) return result;
    }
    if(missing_type_use_fictitious_types) {
      if(!V_is_wrap) {
	result = missing_type_primary_fictitious_family_bindings
	  (source, t, V, try_kinds, split_depth);
	if(result != NULL) return result;
      }
      if(!V_is_primary) {
	result = missing_type_wrap_fictitious_family_bindings
	  (source, t, V, try_kinds, split_depth);
	if(result != NULL) return result;
      }
    }
    
    /*-------------------------------------------------------------*
     * Try product and function bindings for V.  For example, we   *
     * might want to bind V to a species such as (Natural, 	   *
     * Natural).  						   *
     *-------------------------------------------------------------*/

    if(!V_is_wrap) {
      result = missing_type_pair_bindings(source, t, V, try_kinds,
					  split_depth);
      if(result != NULL) return result;
    }
  }

  /*------------------------------------------*
   * If no binding for V worked, return NULL. *
   *------------------------------------------*/

# ifdef DEBUG
    if(trace_missing_type || trace_pat_complete) {
      trace_t(447);
      trace_t(98);
    }
# endif

  return NULL;
}


/****************************************************************
 *			REDUCE_TYPE_LIST_WITH	       		*
 ****************************************************************
 * Return a list of types whose union is the same as the	*
 * union of the types in lists l and keep, but possibly with 	*
 * fewer entries. The types in keep will be kept in the list,	*
 * but those in l that are subsumed by members of keep or by    *
 * other members of l will not be kept.				*
 *								*
 * XREF: Called in t_compl.c to reduce list to search.		*
 ****************************************************************/

TYPE_LIST* reduce_type_list_with(TYPE_LIST *keep, TYPE_LIST *l)
{
  TYPE *t;
  TYPE_LIST *p;
  int ov;

 tail_recur:
  if(l == NIL) return keep;

  t = l->head.type;

  /*----------------------------------------------------------------*
   * See if the head of l is subsumed by a member of the tail of l. *
   *----------------------------------------------------------------*/

  for(p = l->tail; p != NIL; p = p->tail) {
    ov = half_overlap_u(t, p->head.type);
    if(ov == EQUAL_OR_CONTAINED_IN_OV) {
      /*------------------------------------------------*
       * return reduce_type_list_with(keep, l->tail);	*
       *------------------------------------------------*/
      l = l->tail;
      goto tail_recur;
    }
  }

  /*-------------------------------------------------------*
   * See if the head of l is subsumed by a member of keep. *
   *-------------------------------------------------------*/

  for(p = keep; p != NIL; p = p->tail) {
    ov = half_overlap_u(t, p->head.type);
    if(ov == EQUAL_OV) {
      /*------------------------------------------------*
       * return reduce_type_list_with(keep, l->tail);	*
       *------------------------------------------------*/
      l = l->tail;
      goto tail_recur;
    }
  }

  /*--------------------------------------*
   * Keep the head of l, if not subsumed. *
   *--------------------------------------*/

  /*------------------------------------------------------------*
   * return reduce_type_list_with(type_cons(t, keep), l->tail); *
   *------------------------------------------------------------*/

  l    = l->tail;
  keep = type_cons(t, keep);
  goto tail_recur;
}

