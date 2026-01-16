/**************************************************************
 * File:    classes/t_tyutil.c
 * Purpose: General utilities for types, used only by the
 *          translator.
 * Author:  Karl Abrahamson
 **************************************************************/

#ifdef COPYRIGHT
/************************************************************************
 * Copyright (c) 2000 Karl Abrahamson					*
 * All rights reserved.							*
 * See file COPYRIGHT.txt for details.					*
 ************************************************************************/
#endif

#include "../misc/misc.h"
#include "../classes/classes.h"
#include "../exprs/expr.h"
#include "../standard/stdtypes.h"
#include "../standard/stdids.h"
#include "../tables/tables.h"
#include "../clstbl/classtbl.h"
#include "../clstbl/meettbl.h"
#include "../utils/lists.h"
#include "../utils/strops.h"
#include "../utils/hash.h"
#include "../generate/generate.h"
#include "../unify/unify.h"
#include "../unify/constraint.h"
#include "../parser/tokens.h"
#include "../alloc/allocate.h"
#include "../error/error.h"
#include "../dcls/dcls.h"
#include "../infer/infer.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

PRIVATE TYPE_LIST* lefts_t(TYPE_LIST *l);
PRIVATE TYPE_LIST* rights_t(TYPE_LIST *l);


/****************************************************************
 *			IS_BOOLEAN_TYPE				*
 ****************************************************************
 * Return true if T is type Boolean.				*
 ****************************************************************/

Boolean is_boolean_type(TYPE *t)
{
  t = find_u(t);
  if(t == NULL_T || t->ctc == NULL) return FALSE;
  if(TKIND(t) == TYPE_ID_T && t->ctc == boolean_type->ctc) return TRUE;
  return FALSE;
}


/****************************************************************
 *			COUNT_OCCURRENCES_1			*
 ****************************************************************
 * Put, in the seenTimes field of each type of T, the number of *
 * times that node occurs in T.  The largest value counted 	*
 * to is 2 -- anything above that is just kept at 2.  Only	*
 * PAIR_T, FUNCTION_T and FAM_MEM_T nodes are done.		*
 *								*
 * Go into lower bounds in the search.				*
 *								*
 * Return TRUE if any type has multiple occurrences, and FALSE  *
 * otherwise.							*
 ****************************************************************/

PRIVATE Boolean count_occurrences_multiple;

PRIVATE void count_occurrences_1_help(TYPE *t)
{
  TYPE_TAG_TYPE t_kind;

 tail_recur:
  if(t == NULL) return;

  t      = find_u(t);
  t_kind = TKIND(t);
  if(IS_STRUCTURED_T(t_kind)) {
    int old_seenTimes = t->seenTimes;
    if(old_seenTimes == 0) {
      t->seenTimes = 1;
      count_occurrences_1_help(t->TY1);
      t = t->TY2;
      goto tail_recur;
    }
    else if(old_seenTimes == 1) {
      t->seenTimes = 2;
      count_occurrences_multiple = TRUE;
    }
  }
  else if(IS_VAR_T(t_kind) && !t->seen) {
    TYPE_LIST *p;
    mark_seen(t);
    for(p = t->LOWER_BOUNDS; p != NIL; p = p->tail) {
      count_occurrences_1_help(p->head.type);
    }
  }
}

/*----------------------------------------------------------------*/

Boolean count_occurrences_1(TYPE *t)
{
  count_occurrences_multiple = FALSE;
  count_occurrences_1_help(t);
  reset_seen();
  return count_occurrences_multiple;
}


/****************************************************************
 *			COUNT_OCCURRENCES_2			*
 ****************************************************************
 * Similar to count_occurrences_1, but does all types in the 	*
 * type of each expr in list L, not just one type.  It replaces *
 * each type in list L by its reduction first.			*
 *								*
 * XREF: Called in genglob.c.					*
 ****************************************************************/

Boolean count_occurrences_2(EXPR_LIST *l)
{
  EXPR_LIST *p;

  count_occurrences_multiple = 0;
  for(p = l; p != NIL; p = p->tail) {
    SET_TYPE(p->head.expr->ty, reduce_type(p->head.expr->ty));
    count_occurrences_1_help(p->head.expr->ty);
  }
  reset_seen();
  return count_occurrences_multiple;
}


/****************************************************************
 *			CLEAR_SEENTIMES_1			*
 ****************************************************************
 * Clear the seenTimes field for each node in T.		*
 *								*
 * This function does go into lower bounds.			*
 ****************************************************************/

PRIVATE void clear_seenTimes_1_help(TYPE *t)
{
  TYPE_TAG_TYPE t_kind;

 tail_recur:
  if(t == NULL) return;
  t      = find_u(t);
  t_kind = TKIND(t);
  if(IS_STRUCTURED_T(t_kind)) {
    t->seenTimes = 0;
    clear_seenTimes_1_help(t->TY1);
    t = t->TY2;
    goto tail_recur;
  }
  else if(IS_VAR_T(t_kind) && !t->seen) {
    TYPE_LIST *p;
    mark_seen(t);
    for(p = t->LOWER_BOUNDS; p != NIL; p = p->tail) {
      clear_seenTimes_1_help(p->head.type);
    }
  }
}

/*-------------------------------------------------------------*/

void clear_seenTimes_1(TYPE *t)
{
  clear_seenTimes_1_help(t);
  reset_seen();
}


/****************************************************************
 *			CLEAR_SEENTIMES_2			*
 ****************************************************************
 * Similar to clear_seenTimes, but do it on each type in each   *
 * expr in list L.  Only the type of the top EXPR node is done, *
 * not types in subexpressions.					*
 *								*
 * XREF: Called in genglob.c.					*
 ****************************************************************/

void clear_seenTimes_2(EXPR_LIST *l)
{
  EXPR_LIST *p;

  for(p = l; p != NIL; p = p->tail) {
    clear_seenTimes_1_help(p->head.expr->ty);
  }
  reset_seen();
}


/****************************************************************
 *			MARK_NO_SPLIT				*
 ****************************************************************
 * Mark all variables in T as non-splittable.  Do not go into   *
 * lower bounds.						*
 ****************************************************************/

void mark_no_split(TYPE *t)
{
  TYPE_TAG_TYPE t_kind;

 tail_recur:
  if(t == NULL) return;

  t      = find_u(t);
  t_kind = TKIND(t); 
  if(IS_STRUCTURED_T(t_kind)) {
    mark_no_split(t->TY1);
    t = t->TY2;
    goto tail_recur;
  }

  else if(IS_VAR_T(t_kind)) {
    t->nosplit = 1;
  }
}


/****************************************************************
 *			CLEAR_LOCS_T	 			*
 ****************************************************************
 * Clear the TOFFSET field in each variable in T, including	*
 * those in lower bounds.			.		*
 ****************************************************************/

PRIVATE void clear_locs_help_t(TYPE *t)
{
  TYPE_TAG_TYPE t_kind;

tail_recur:
  if(t == NULL || t->copy == 0) return;

  t      = find_u(t);
  t_kind = TKIND(t);
  if(IS_STRUCTURED_T(t_kind)) {
    clear_locs_help_t(t->ty1);
    t = t->TY2;
    goto tail_recur;
  }
  else if(IS_VAR_T(t_kind)) {
    t->TOFFSET = 0;
    if(!t->seen) {
      TYPE_LIST *p;
      mark_seen(t);
      for(p = lower_bound_list(t); p != NIL; p = p->tail) {
        if(LKIND(p) != TYPE2_L) {
	  clear_locs_help_t(p->head.type);
	}
      }
    }
  }
}

/*-------------------------------------------------------*/

void clear_locs_t(TYPE *t)
{
  clear_locs_help_t(t);
  reset_seen();
}


/************************************************************************
 *			RESET_FREES	 				*
 ************************************************************************
 * Sets free fields of the nodes of type T correctly.  The conditions   *
 * for setting the free field of a subtree S are as follows.		*
 *									*
 *   free = 1	       If S contains at least one free variable. A	*
 *		       free variable is	one that is not currently	*
 *		       in list glob_bound_vars.				*
 *		       (runtime bound variables are considered free.)	*
 *									*
 *   free = 0          Otherwise.					*
 ************************************************************************/

PRIVATE void reset_frees_help(TYPE *t)
{
  TYPE *ty1, *ty2;

  if(t == NULL) return;

  switch(TKIND(t)) {
    /*-------------------------------------------------------*/
    case FUNCTION_T:
    case PAIR_T:
    case FAM_MEM_T:

      /*----------------------------------------------------------------*
       * Note: a null subtree indicates an anonymous variable, which, 	*
       * of course, must be free since it cannot have been put into  	*
       * any table.							*
       *----------------------------------------------------------------*/

      ty1 = t->ty1;
      ty2 = t->TY2;
      reset_frees_help(ty1);
      reset_frees_help(ty2);
      t->free = 0;
      if(ty1 == NULL || ty1->free == 1 || ty2 == NULL || ty2->free == 1) {
	t->free = 1;
      }
      return;

    /*-------------------------------------------------------*/
    case TYPE_ID_T:
    case FAM_T:
    case FICTITIOUS_TYPE_T:
    case FICTITIOUS_FAM_T:
    case WRAP_TYPE_T:
    case WRAP_FAM_T:
    case FICTITIOUS_WRAP_TYPE_T:
    case FICTITIOUS_WRAP_FAM_T:
      t->free = 0;
      return;

    /*-------------------------------------------------------*/
    case MARK_T:
      ty1 = t->ty1;
      reset_frees_help(ty1);
      t->free = 0;
      if(ty1 == NULL || ty1->free == 1) t->free = 1;
      return;

    /*-------------------------------------------------------*/
    case TYPE_VAR_T:
    case FAM_VAR_T:
    case PRIMARY_TYPE_VAR_T:
    case PRIMARY_FAM_VAR_T:
    case WRAP_TYPE_VAR_T:
    case WRAP_FAM_VAR_T:

      ty1 = t->ty1;
      if(ty1 != NULL_T) {
        reset_frees_help(ty1);
        t->free = (ty1 == NULL) ? 1 : ty1->free;
        return;
      }
      else {
        t->free = (class_mem(t,glob_bound_vars) == NULL);
	if(!t->seen) {
	  TYPE_LIST *p;
	  mark_seen(t);
	  for(p = lower_bound_list(t); p != NIL; p = p->tail) {
	    if(LKIND(p) != TYPE2_L) {
	      reset_frees_help(p->head.type);
	    }
	  }
	}
        return;
      }

    /*-------------------------------------------------------*/
    default: {}
  }
}

/*-------------------------------------------------------------------*/

void reset_frees(TYPE *t)
{
   reset_frees_help(t);
   reset_seen();
}


/*****************************************************************
 *			BASIC_CONTAINING_CLASS			 *
 *****************************************************************
 * Return a type that contains all members of list L, or  	 *
 * NULL if the only such type is `a.  List L must be nonempty.   *
 *								 *
 * This is a restricted version of containing_class, where each  *
 * member of list L is either an id or a variable, and none of   *
 * the TYPE_VAR_T variables in list L ranges over ANY.		 *
 *								 *
 * It is possible for the members of list L to be families or	 *
 * family variables.						 *
 *****************************************************************/

PRIVATE TYPE* basic_containing_class(TYPE_LIST *l)
{
  CLASS_TABLE_CELL *head_ctc, *ctcp;
  TYPE_LIST *p;
  int head_code, join;
  TYPE_TAG_TYPE join_code;
  TYPE *head_type;
  Boolean all_wrapped, all_primary, one_is_null;

  head_type = l->head.type;
  head_ctc  = head_type->ctc;
  head_code = (head_ctc == NULL) ? GENUS_ID_CODE : head_ctc->code;

  /*-------------------------------------------------------------------*
   * If all types in list l are the same, then return the common type. *
   *-------------------------------------------------------------------*/

  for(p = l->tail; p != NIL; p = p->tail) {
    TYPE* this_type = find_u(p->head.type);
    if(!full_type_equal(this_type, head_type)) {
      goto general; /* Just below */
    }
  }
  return copy_type(head_type, 0);

 general:

  /*-------------------------------------------------------*
   * General case: First check the list of types, setting  *
   *							   *
   *   one_is_null to TRUE just when at least one of the   *
   *                  types has a NULL ctc field.	   *
   *							   *
   *   all_wrapped to TRUE just when all of the members    *
   *                  have tags that are among WRAP_TYPE_T,*
   * 		      WRAP_FAM_T, WRAP_TYPE_VAR_T,	   *
   *		      WRAP_FAM_VAR_T, FICTITIOUS_WRAP_TYPE_T*
   *		      and FICTITIOUS_WRAP_FAM_T.	   *
   *							   *
   *   all_primary to TRUE just when all of the members	   *
   *		      have tags that are among FAM_T,	   *
   *		      TYPE_ID_T, PRIMARY_TYPE_VAR_T,	   *
   *		      PRIMARY_FAM_VAR_T, 		   *
   *		      FICTITIOUS_TYPE_T and		   *
   *		      FICTITIOUS_FAM_T.			   *
   *-------------------------------------------------------*/

  one_is_null = FALSE;
  all_wrapped = all_primary = TRUE;
  for(p = l; p != NIL; p = p->tail) {
    TYPE* this_type = find_u(p->head.type);
    register TYPE_TAG_TYPE p_kind = TKIND(this_type);
    if(!MEMBER(p_kind, wrap_tkind_set)) {
      all_wrapped = FALSE;
    }
    if(!MEMBER(p_kind, primary_tkind_set)) {
      all_primary = FALSE;
    }
    if(this_type->ctc == NULL) one_is_null = TRUE;
  }

  /*-----------------------------------------*
   * Find the join of all members of list l. *
   *-----------------------------------------*/

  join = ctc_num(head_ctc);
  for(p = l->tail; p != NIL; p = p->tail) {
    ctcp = find_u(p->head.type)->ctc;
    join = full_get_join(ctcs[join], ctcp);
  }

  /*---------------------------------------------*
   * If the join is ANY and the desired variable *
   * kind is ordinary, then return NULL.    	 *
   *---------------------------------------------*/

  if(join == 0 && !all_primary && !all_wrapped) return NULL;

  /*------------------------------------------------------*
   * If the join is a genus, but the things in list a	  *
   * are families or family variables, then return NULL.  *
   *------------------------------------------------------*/

  join_code = ctcs[join]->code;
  if(join_code == GENUS_ID_CODE && 
     (head_code == COMM_ID_CODE || head_code == FAM_ID_CODE)) {
    return NULL;
  }

  /*------------------------------------------------------------------*
   * If all of the types in list l are wrap types or variables, then  *
   * return a wrap variable.  					      *
   *								      *
   * If all of the types in list l are primary types or variables,    *
   * then return a primary variable.				      *
   *								      *
   * Otherwise, return an ordinary variable. 			      *
   *------------------------------------------------------------------*/

  {TYPE* result = copy_type(ctcs[join]->ty, 0);
   if(IS_VAR_T(TKIND(result))) {
     if(all_wrapped) {
       result->kind = (head_code == FAM_ID_CODE || head_code == COMM_ID_CODE)
	               ? WRAP_FAM_VAR_T 
                       : WRAP_TYPE_VAR_T;
     }
     if(all_primary) {
       result->kind = (head_code == FAM_ID_CODE || head_code == COMM_ID_CODE)
	               ? PRIMARY_FAM_VAR_T 
                       : PRIMARY_TYPE_VAR_T;
     }
   }

   return result;
  }
}

    
/****************************************************************
 *			CONTAINING_CLASS			*
 ****************************************************************
 * Return a type expr that contains all members of list A.      *
 *								*
 * Where member types S and T have common types, the upper	*
 * bound has the same type, including places where S and T have *
 * a common variable.  Where S and T differ, the result has a	*
 * type with new variables that includes both types. 		*
 *								*
 * If the answer has the form (`a), then return NULL instead.   *
 * Also return NULL if list A is empty.				*
 *								*
 * This function is not guaranteed to produce the best 		*
 * possible containing type.  It does the best that it can.	*
 * It should be used when such a result is acceptable.		*
 ****************************************************************/

TYPE* containing_class(TYPE_LIST *a)
{
  TYPE_TAG_TYPE kind, this_kind;
  TYPE_LIST *p;
  TYPE *result, *t;

  if(a == NIL) return NULL;

# ifdef DEBUG
    if(trace_infer > 1) {
      trace_t(9);
      for(p = a; p != NIL; p = p->tail) {
	fprintf(TRACE_FILE, "  ");
	trace_ty(p->head.type);
	tracenl();
      }
    }
# endif

  /*----------------------------------------------------*
   * NULL, or an ordinary variable that ranges over	*
   * NULL, is a variable that ranges over ANY.  Its	*
   * presence in list a forces a NULL result.		*
   *----------------------------------------------------*/

  t = find_u(a->head.type);
  if(t == NULL) return NULL;

  kind = TKIND(t);
  if(kind == BAD_T) return NULL;
  if(kind == TYPE_VAR_T && t->ctc == NULL) return NULL;

  for(p = a->tail; p != NIL; p = p->tail) {
    t = find_u(p->head.type);
    if(t == NULL) return NULL;

    /*----------------------------------------------------------*
     * If we see a type in list a that has a kind that		*
     * is incompatible with previous kinds, then we 		*
     * just return NULL, indicating that the types in list	*
     * a are so general that we accept `x as the containing	*
     * polymorphic type.					*
     *----------------------------------------------------------*/

    this_kind = TKIND(t);
    if(this_kind == BAD_T) return NULL;
    if(this_kind != kind) {
      if(IS_STRUCTURED_T(kind)) return NULL;
      if(IS_STRUCTURED_T(this_kind)) return NULL;
    }

    if(this_kind == TYPE_VAR_T && t->ctc == NULL) return NULL;
  }

  /*------------------------------------------------------------*
   * If we get here, and kind is one of the structured kinds	*
   * FUNCTION_T, PAIR_T or FAM_MEM_T, then the previous		*
   * loop guarantees that all members of list a have the same	*
   * kind.  So we can recur on the parts of each member.  	*
   *								*
   * On the other hand, if kind is not a structured kind,	*
   * then it is either a variable or a type/family id, and all	*
   * members of a are also variables or type/family ids.  So	*
   * we use basic_containing_class to find the result.		*
   *------------------------------------------------------------*/

  if(IS_STRUCTURED_T(kind)) {

    /*-----------------------------------------------------------*
     * In the case of FAM_MEM_T, the right member of each is the *
     * family.  We need to have it to build the result.	         *
     *-----------------------------------------------------------*/

    TYPE* rcont = containing_class(rights_t(a));
    if(rcont == NULL && kind == FAM_MEM_T) result = NULL;
    else result = new_type2(kind, containing_class(lefts_t(a)), rcont);
  }

  else {
    result = basic_containing_class(a);
  }

# ifdef DEBUG
    if(trace_infer) {
      trace_t(10);
      trace_ty(result);
      trace_t(11);
      for(p = a; p != NIL; p = p->tail) {
        fprintf(TRACE_FILE, "  ");
        trace_ty(p->head.type);
	tracenl();
      }
    }
# endif

  return result;
}


/****************************************************************
 *			CONTAIN_TYPE				*
 ****************************************************************
 * contain_type(A,B) = containing_class([A,B]), except that     *
 * NULL will not be returned.  Instead of NULL, we return	*
 * `a, where `a is a new variable.				*
 ****************************************************************/

TYPE* contain_type(TYPE *A, TYPE *B)
{
  TYPE *result;
  TYPE_LIST* l = type_cons(A, type_cons(B, NIL));
  bump_list(l);

  result = containing_class(l);
  if(result == NULL) result = var_t(NULL);
  else replace_null_vars(&result);
  bump_type(result);
  drop_list(l);
  result->ref_cnt--;
  return result;
}


/****************************************************************
 *			INTERSECT_TYPE_LISTS			*
 ****************************************************************
 * Lists A and B are either lists of variables, or lists of     *
 * pair types, where the left-hand member of each pair is a	*
 * variable.							*
 *								*
 * In the case where lists A and B are lists of variables, 	*
 * return the list of all variables occur in both A and B.  	*
 * Variables are compared using ==, so we are comparing		*
 * pointers.							*
 *								*
 * In the cases where lists A and B are lists of pairs, find 	*
 * all pairs (V,X) in list A such that there is a pair (V,Y)	*
 * in list B, with the same variable V.  If X = Y, then pair	*
 * (V,X) is placed in the result list.  If X and Y are 		*
 * different, then pair (V,R) is placed in the result list,	*
 * where R is a type that is an upper bound on X and Y.		*
 ****************************************************************/

TYPE_LIST* intersect_type_lists(TYPE_LIST *a, TYPE_LIST *b)
{
  TYPE_LIST *result, *p, *q;
  TYPE *p_var, *q_var, *q_entry, *add_to_result;
  Boolean p_is_pair, q_is_pair = FALSE;

  result = NIL;
  for(p = a; p != NIL; p = p->tail) {
    p_var     = p->head.type;
    p_is_pair = FALSE;
    if(TKIND(p_var) == PAIR_T) {
      p_var     = p_var->TY1;
      p_is_pair = TRUE;
    }

    /*----------------------------------------------------------*
     * Find the entry q_entry in b that has the same variable	*
     * as the current entry (p) in list a (or NULL if none).	*
     *----------------------------------------------------------*/

    q_entry = NULL;
    for(q = b; q != NIL; q = q->tail) {
      q_var     = q->head.type;
      q_is_pair = FALSE;
      if(TKIND(q_var) == PAIR_T) {
	q_var     = q_var->TY1;
        q_is_pair = TRUE;
      }
      if(q_var == p_var) {
	q_entry = q->head.type;
	break;
      }
    }

    /*------------------------------------------------------------*
     * If we found a matching entry, then add an entry to result. *
     * Be sure to include the correct right-hand side when the    *
     * list entries are pairs.					  *
     * -----------------------------------------------------------*/

    if(q_entry != NULL) {
      if(p_is_pair && q_is_pair) {
        TYPE* p_entry = p->head.type;
        TYPE* p_rhs = p_entry->TY2;
	TYPE* q_rhs = q_entry->TY2;

	if(full_type_equal(p_rhs, q_rhs)) add_to_result = p_entry;
        else {
	  add_to_result = pair_t(p_var, contain_type(p_rhs, q_rhs));
	}
      }
      else add_to_result = p_var;

      result = type_cons(add_to_result, result);
    }

  } /* end for(p = ...) */

  return result;
}

/********************************************************
 *			LEFTS_T				*
 ********************************************************
 * Return the list of TY1 fields of types in L.		*
 ********************************************************/

PRIVATE TYPE_LIST* lefts_t(TYPE_LIST *l)
{
  if(l == NIL) return NIL;
  return type_cons(find_u(l->head.type)->TY1, lefts_t(l->tail));
}


/********************************************************
 *			RIGHTS_T			*
 ********************************************************
 * Return the list of TY2 fields of types in L.		*
 ********************************************************/

PRIVATE TYPE_LIST* rights_t(TYPE_LIST *l)
{
  if(l == NIL) return NIL;
  return type_cons(find_u(l->head.type)->TY2, rights_t(l->tail));
}


/*******************************************************************
 *			BIND_FOR_NO_FICTITIOUS		 	   *
 *******************************************************************
 * Replace variables in T as follows, and return the resulting 	   *
 * type.							   *
 *								   *
 * If T is a variable G`x or G``x then return G (a wrap species). *
 *								   *
 * If T is a variable G`*x ranging over an empty genus G, then	   *
 * return NULL.  Otherwise return a canonical member of G.	   *
 *								   *
 * If T has no variable, return T.				   *
 *******************************************************************/

TYPE* bind_for_no_fictitious(TYPE *t)
{
  TYPE_TAG_TYPE t_kind;
  if(t == NULL) return NULL;

  IN_PLACE_FIND_U_NONNULL(t);
  t_kind = TKIND(t);

  if(IS_STRUCTURED_T(t_kind)) {
    TYPE *ty1, *ty2;
    ty1 = bind_for_no_fictitious(t->TY1);
    if(ty1 == NULL) return NULL;
    ty2 = bind_for_no_fictitious(t->TY2);
    if(ty2 == NULL) return NULL;
    if(ty1 == t->TY1 && ty2 == t->TY2) return t;
    else return new_type2(t_kind, ty1, ty2);
  }

  else if(IS_VAR_T(t_kind)) {
    if(IS_PRIMARY_VAR_T(t_kind)) {
      CLASS_TABLE_CELL *mem_ctc;
      if(t->ctc != NULL || !t->ctc->nonempty) return NULL;
      mem_ctc = ctcs[t->ctc->mem_num];
      if((mem_ctc->code == FAM_ID_CODE || mem_ctc->code == PAIR_CODE) &&
         t->ctc->code == GENUS_ID_CODE) return t;
      else return mem_ctc->ty;
    }
    else {
      return wrap_tf(t->ctc);
    }
  }

  else return t;
}


/*******************************************************************
 *			HAS_HIDDEN_ID_T		 		   *
 *******************************************************************
 * Return true just when T contains an identifier that has the     *
 * form package-name:id-name outside a lower bound.  Such a form   *
 * indicates an identifier that is hidden in the body of a package.*
 *******************************************************************/

Boolean has_hidden_id_t(TYPE *t)
{
  TYPE_TAG_TYPE t_kind;

tail_recur:
  if(t == NULL) return FALSE;

  IN_PLACE_FIND_U_NONNULL(t);
  t_kind = TKIND(t);
  if(IS_STRUCTURED_T(t_kind)) {
    if(has_hidden_id_t(t->TY1)) return TRUE;
    t = t->TY2;
    goto tail_recur;
  }
  else if(t_kind != BAD_T) {
    return t->ctc == NULL ? FALSE : is_hidden_id(t->ctc->name);
  }
  else return FALSE;
}


/****************************************************************
 *			PUT_VARS_T				*
 ****************************************************************
 * Put the variables in T into hash table TBL.  Do not go into  *
 * lower bounds.						*
 ****************************************************************/

void put_vars_t(TYPE *t, HASH1_TABLE **tbl)
{ 
  HASH1_CELLPTR h;
  HASH_KEY u;
  TYPE_TAG_TYPE t_kind;

tail_recur:
  t = find_u(t);
  if(t == NULL) return;
  t_kind = TKIND(t);

  if(IS_STRUCTURED_T(t_kind)) {
    put_vars_t(t->TY1, tbl);
    t = t->TY2;
    goto tail_recur;
  }

  else if(IS_VAR_T(t_kind)) {
    u.num = tolong(t);
    h     = insert_loc_hash1(tbl, u, inthash(u.num), eq);
    if(h->key.num == 0) {
      h->key = u;
    }
  }
}


/****************************************************************
 *			PUT_VARS2_T				*
 ****************************************************************
 * Put the variables in T into hash table TBL, each paired with *
 * itself.  So the key and value of each entry are the same.    *
 * The value field has a reference count, but the key field     *
 * does not.  Do not go into lower bounds.			*
 ****************************************************************/

void put_vars2_t(TYPE *t, HASH2_TABLE **tbl)
{ 
  HASH2_CELLPTR h;
  HASH_KEY u;
  TYPE_TAG_TYPE t_kind;

tail_recur:
  t = find_u(t);
  if(t == NULL) return;

  t_kind = TKIND(t);
  if(IS_STRUCTURED_T(t_kind)) {
    put_vars2_t(t->TY1, tbl);
    t = t->TY2;
    goto tail_recur;
  }

  else if(IS_VAR_T(t_kind)) {
    u.num = tolong(t);
    h     = insert_loc_hash2(tbl, u, inthash(u.num), eq);
    if(h->key.num == 0) {
      h->key = u;
      bump_type(h->val.type = t);
    }
  }
}


/*********************************************************************
 *			COPY_VARS_TO_LIST_T			     *
 *********************************************************************
 * Copy the variables in T to the front of list L, and return the    *
 * resulting list.  Do not copy variables that are already in L, and *
 * do not copy variables that are in list SKIP.  Use class_mem to    *
 * test membership in L and SKIP.				     *
 *								     *
 * Do not go into lower bounds.					     *
 *								     *
 * By copying a variable to a list, we mean only that the pointer to *
 * the variable is copied to the list.  The variable itself is not   *
 * copied.							     *
 *********************************************************************/

TYPE_LIST* copy_vars_to_list_t(TYPE *t, TYPE_LIST *l, TYPE_LIST *skip)
{
  TYPE_TAG_TYPE t_kind;

 tail_recur:
  IN_PLACE_FIND_U(t);
  if(t == NULL) return l;
  t_kind = TKIND(t);

  if(IS_STRUCTURED_T(t_kind)) {
    TYPE_LIST* l1 = copy_vars_to_list_t(t->TY1, l, skip);

    /*---------------------------------------------------------------*
     * Following does: return copy_vars_to_list_t(t->TY2, l1, skip); *
     *---------------------------------------------------------------*/

    l = l1;
    t = t->TY2;
    goto tail_recur;
  }

  else if(IS_VAR_T(t_kind)) {
    if(class_mem(t, l) || class_mem(t, skip)) return l;
    else return type_cons(t, l);
  }

  else return l;
}


/****************************************************************
 *			COUNT_VARS_T				*
 ****************************************************************
 * Put the variables in T into hash table TBL, as keys, each	*
 * paired with a count of how many times it occurs.  		*
 *								*
 * Variables in lower bound lists are counted, but only once, 	*
 * regardless of how many times the variable holding that list	*
 * occurs.  For example, if variable `a has constraint		*
 * `a >= `b, and `a occurs twice, then `b is still just		*
 * counted once for the lower bound occurrence. 		*
 ****************************************************************/

PRIVATE void count_vars_help_t(TYPE *t, HASH2_TABLE **tbl)
{ 
  HASH2_CELLPTR h;
  HASH_KEY u;
  TYPE_TAG_TYPE t_kind;

tail_recur:
  t = find_u(t);
  if(t == NULL) return;

  t_kind = TKIND(t);
  if(IS_STRUCTURED_T(t_kind)) {
    count_vars_help_t(t->TY1, tbl);
    t = t->TY2;
    goto tail_recur;
  }

  else if(IS_VAR_T(t_kind)) {
    u.num = tolong(t);
    h     = insert_loc_hash2(tbl, u, inthash(u.num), eq);
    if(h->key.num == 0) {
      h->key     = u;
      h->val.num = 1;
    }
    else h->val.num++;

    if(!t->seen && lower_bound_list(t) != NULL) {
      TYPE_LIST* p;
      mark_seen(t);
      for(p = lower_bound_list(t); p != NULL; p = p->tail) {
	count_vars_help_t(p->head.type, tbl);
      }
    }
  }
}

/*------------------------------------------------------------*/

void count_vars_t(TYPE *t, HASH2_TABLE **tbl)
{
  count_vars_help_t(t, tbl);
  reset_seen();
}


/****************************************************************
 *			MARKED_VARS_T				*
 ****************************************************************
 * Return a list of the marked variables in T, followed by list *
 * REST.  Do not go into lower bounds.				*
 ****************************************************************/

TYPE_LIST* marked_vars_t(TYPE *t, TYPE_LIST *rest)
{
  TYPE_TAG_TYPE t_kind;
  Boolean marked;

  if(t == NULL) return rest;

  t      = find_mark_u(t, &marked);
  t_kind = TKIND(t);

  if(IS_STRUCTURED_T(t_kind)) {
    TYPE_LIST* r1 = marked_vars_t(t->TY1, rest);
    return marked_vars_t(t->TY2, r1);
  }

  else if(marked && IS_VAR_T(t_kind)) {
    return type_mem(t, rest, 0) ? rest : type_cons(t, rest);
  }

  else return rest;
}


/****************************************************************
 *			DISPATCHABLE_COUNT_T			*
 ****************************************************************
 * Return the number of times that variable V occurs in a	*
 * dispatchable context in T.  Such a context is in the domain	*
 * of a function, where a function will have the value to look	*
 * at.  That is, it represents a parameter to the function.	*
 *								*
 * Parameter context tells the context in which T occurs.	*
 *								*
 *   context = 0	Right context.  This is what should be  *
 *			used when is_dispatchable_t is called	*
 *			from outside.				*
 *								*
 *   context = 1	Left context.  We are looking at a	*
 *			function parameter type.		*
 ****************************************************************/

int dispatchable_count_t(TYPE *V, TYPE *t, int context)
{
  t = find_u(t);
  switch(TKIND(t)) {

    /*----------------------------------------------------------*
     * A family member is only dispatchable if the family	*
     * is dispatchable.  We can't look at the parameter.	*
     *----------------------------------------------------------*/

    case FAM_MEM_T:
      if(context == 0) return 0;
      else {
        return dispatchable_count_t(V, t->TY2, 1);
      }

    case PAIR_T:
      if(context == 0) return 0;
      else {
        return dispatchable_count_t(V, t->TY1, 1) +
	       dispatchable_count_t(V, t->TY2, 1);
      }

    case FUNCTION_T:
      if(context == 1) return 0;
      else {
        return dispatchable_count_t(V, t->TY1, 1) +
	       dispatchable_count_t(V, t->TY2, 0);
      }

    default: return 0;

    case TYPE_VAR_T:
    case FAM_VAR_T:
    case PRIMARY_TYPE_VAR_T:
    case PRIMARY_FAM_VAR_T:
    case WRAP_TYPE_VAR_T:
    case WRAP_FAM_VAR_T:
      if(context == 1 && t == V) return 1;
      else return 0;
  }
}


/************************************************************************
 *			EXTENSIBLE_VAR_IN	 			*
 ************************************************************************
 * This function has two modes.						*
 *									*
 * In the first mode, C is nonnull. Class table cell C describes a 	*
 * genus or community X.  If there is exactly one variable in T that 	*
 * is marked (occurs immediately beneath a MARK_T node) and that ranges *
 * over X,  or over an explicitly extensible genus or community		*
 * that contains X, then return that variable.  If there is no such     *
 * variable in T, return NULL.  If there are two or more such variables,*
 * return (TYPE *) 1. 							*
 *									*
 * Note that we are counting variables here, not variable occurrences.  *
 * A single variable can occur several times, and still be returned.    *
 * A variable is considered to be marked if any of its occurrences	*
 * are marked.								*
 *									*
 * In the second mode, C is NULL.  If there is just one variable	*
 * in T that is marked, and that ranges over an explicitly extensible   *
 * genus or community, return that variable.  (Any genus or community   *
 * will do in this case.)  Return NULL if there are no such variables,  *
 * and (TYPE*) 1 if there are two or more such variables.		*
 ************************************************************************/

TYPE* extensible_var_in(TYPE *t, CLASS_TABLE_CELL *C)
{

tail_recur:

  /*------------------------------*
   * Just in case, test for NULL. *
   *------------------------------*/

  if(t == NULL) return NULL;

  switch(TKIND(t)) {
    /*-------------------------------------------------------*/
    case PAIR_T:
    case FAM_MEM_T:
    case FUNCTION_T:
      {TYPE *a, *b;

       a = extensible_var_in(t->TY1, C);
       if(a == (TYPE *) 1) return (TYPE *) 1;

       b = extensible_var_in(t->TY2, C);
       if(b == NULL || a == b) return a;
       else if(a == NULL) return b;
       else return (TYPE *) 1;
      }

    /*-------------------------------------------------------*/
    case MARK_T:

      /*--------------------------------------------------------*
       * If we see a marked variable, then check ancestry with	*
       * respect to C, if appropriate.  This might be the	*
       * variable to return.					*
       *--------------------------------------------------------*/

      {register TYPE* t1 = find_u(t->TY1);

       if(IS_VAR_T(TKIND(t1))) {
	 register CLASS_TABLE_CELL* ctc = t1->ctc;
	 if(ctc != NULL && ctc->extensible == 2 &&
	    (C == NULL || ctc == C || ancestor_tm(ctc, C))) {
	   return t1;
	 }
       }

       /*-------------------------------------------------------*
	* If this is not an appropriate variable, then search	*
	* in it for a variable.  Just tail recur.		*
	*-------------------------------------------------------*/

       t = t1;
       goto tail_recur;
      }

    /*-------------------------------------------------------*/
    case TYPE_VAR_T:
    case FAM_VAR_T:
    case PRIMARY_TYPE_VAR_T:
    case PRIMARY_FAM_VAR_T:
    case WRAP_TYPE_VAR_T:
    case WRAP_FAM_VAR_T:

      /*--------------------------------------------------------*
       * At a bound variable, follow the binding link.  At	*
       * an unbound variable, we must return NULL.		*
       *--------------------------------------------------------*/

      {register TYPE* t1 = t->TY1;
       if(t1 != NULL) {
	 t = t1;
         goto tail_recur;
       }
       /* else fall through and return NULL. */
      }

    /*-------------------------------------------------------*/
    default:
      return NULL;
  }
}


/****************************************************************
 *			FIND_VAR_T				*
 ****************************************************************
 * Return an unbound variable that occurs in T but is not in 	*
 * TBL, or NULL if there is none.  Do not go into lower bounds.	*
 *								*
 * There can be several variables in T that work.  Any one can  *
 * be returned.							*
 ****************************************************************/

TYPE* find_var_t(TYPE *t, HASH1_TABLE *tbl)
{ 
  HASH1_CELLPTR h;
  HASH_KEY u;
  TYPE *result;
  TYPE_TAG_TYPE t_kind;

 tail_recur:
  t = find_u(t);
  if(t == NULL) return NULL;
  t_kind = TKIND(t);

  if(IS_STRUCTURED_T(t_kind)) {
    result = find_var_t(t->TY1, tbl);
    if(result != NULL) return result;
    t = t->TY2;
    goto tail_recur;
  }

  else if(IS_VAR_T(t_kind)) {
    u.num = tolong(t);
    h = locate_hash1(tbl, u, inthash(u.num), eq);
    if(h->key.num == 0) return t;
    else return NULL;
  }

  else return NULL;
}

 
/****************************************************************
 *			ANY_OVERLAP		 		*
 ****************************************************************
 * Return TRUE if T overlaps (i.e. has a nonempty intersection  *
 * with) any of the types in list L.        			*
 ****************************************************************/

Boolean any_overlap(TYPE *t, TYPE_LIST *l)
{
  TYPE_LIST *p;

  for(p = l; p != NIL; p = p->tail) {
    if(!disjoint(t, p->head.type)) return TRUE;
  }
  return FALSE;
}


/****************************************************************
 *			ADD_TYPE_TO_LIST	 		*
 ****************************************************************
 * Add type T to list L, with list tag tag, but			*
 *								*
 *   Don't add T it if it is entirely subsumed by an existing	*
 *   type in list L whose tag is not larger than tag.		*
 *								*
 *   If there are any types in list L that are subsumed by T,   *
 *   and whose tags are not less than tag, then delete them.    *
 *								*
 * Return the resulting list.  List L is not changed, but is	*
 * copied where changes are necessary.  List L might be shared  *
 * by the result list.						*
 ****************************************************************/

TYPE_LIST* add_type_to_list(TYPE *t, TYPE_LIST *L, int tag)
{
  TYPE_LIST *p, *r;
  TYPE *s;
  int ov, ptag;

  bump_list(L);

  /*------------------------------------------------------*
   * Accumulate (in r) the types from L that should stay. *
   *------------------------------------------------------*/

  r = NIL;
  for(p = L; p != NIL; p = p->tail) {
    s    = p->head.type;
    ptag = LKIND(p);
    ov   = overlap_u(t,s);
    switch(ov) {
      /*-------------------------------------------------------*/
      case CONTAINS_OV:

	/*-------------------------------------------------------*
	 * Type s is contained in t, so it only goes into the    *
	 * result list if ptag < tag.				 *
	 *-------------------------------------------------------*/

	if(ptag >= tag) break;

	/*----------------------------------------*
	 * No break -- fall through to next case. *
	 *----------------------------------------*/

      /*-------------------------------------------------------*/
      case DISJOINT_OV:
      case BAD_OV:

	/*-------------------------------*
	 * This type goes into the list. *
	 *-------------------------------*/

        {register HEAD_TYPE ht;
	 ht.type = s;
         SET_LIST(r, general_cons(ht,r,ptag));
	 break;
	}

      /*-------------------------------------------------------*/
      case EQUAL_OV:

	/*---------------------------------------------------------*
	 * Since t is equal to a member of L, the result of this   *
         * function is just L, provided the tag is acceptable.	   *
         * Save space by returning L itself.  			   *
	 *---------------------------------------------------------*/

	if(ptag <= tag) {SET_LIST(r, L); goto out1;}
	break;

      /*-------------------------------------------------------*/
      case CONTAINED_IN_OV:

	/*-----------------------------------------------------------*
	 * Since t is contained in a member of L, the result is just *
	 * L, provided the tag does not need to be updated.          *
	 *-----------------------------------------------------------*/

	if(ptag <= tag) {SET_LIST(r, L); goto out1;}
	else {
	  register HEAD_TYPE ht;
	  ht.type = s;
	  SET_LIST(r, general_cons(ht, r, ptag));
	  break;
	}

      }
  }

  /*---------------------------------------------------------*
   * If the loop exits normally, then we need to put t in r. *
   *---------------------------------------------------------*/

  {register HEAD_TYPE ht;
   ht.type = t;
   SET_LIST(r, general_cons(ht,r,tag));
  }

  /*-----------*
   * Return r. *
   *-----------*/

 out1:
  if(r != NIL) r->ref_cnt--;
  drop_list(L);
  return r;
} 


/****************************************************************
 *			REDUCE_TYPE_LIST			*
 ****************************************************************
 * Return the list of all types in list L that have a nonempty  *
 * overlap with type T.						*
 ****************************************************************/

TYPE_LIST* reduce_type_list(TYPE_LIST* l, TYPE* t)
{
  Boolean deleted_some = FALSE;
  TYPE_LIST* p;
  TYPE_LIST* result = NIL;
 
  for(p = l; p != NIL; p = p->tail) {
    if(!disjoint(t, p->head.type)) {
      SET_LIST(result, type_cons(p->head.type, result));
    }
    else deleted_some = TRUE;
  }

  if(!deleted_some) {
    drop_list(result);
    return l;
  }
  else {
    SET_LIST(result, reverse_list(result));
    if(result != NIL) result->ref_cnt--;
    return result;
  }
}


/****************************************************************
 *			IS_POLYMORPHIC				*
 ****************************************************************
 * Return true just when type T contains an unbound variable.   *
 ****************************************************************/

Boolean is_polymorphic(TYPE* t)
{
  return find_var_t(t, NULL) != NULL;
}


/****************************************************************
 *			VAR_IN_CONTEXT				*
 ****************************************************************
 * Return true if T has a variable that occurs in a             *
 * context that is						*
 *   left  if SIDE is 0,					*
 *   right if SIDE is 1.					*
 * It is presumed that we are looking at T in a right context.  *
 *								*
 * Do not go into lower bounds.					*
 ****************************************************************/

Boolean var_in_context(TYPE *t, int side)
{
  for(;;) {
    t = find_u(t);
    switch(TKIND(t)) {
      case FAM_MEM_T:
        t = t->TY1;
        break;

      case FUNCTION_T:
        if(var_in_context(t->TY1, 1-side)) return TRUE;
        t = t->TY2;
        break;

      case PAIR_T:
        if(var_in_context(t->TY1, side)) return TRUE;
        t = t->TY2;
        break;

      case TYPE_VAR_T:
      case FAM_VAR_T:
      case PRIMARY_TYPE_VAR_T:
      case PRIMARY_FAM_VAR_T:
      case WRAP_TYPE_VAR_T:
      case WRAP_FAM_VAR_T:
        return side;

      default: return FALSE;
    }
  }
}


/****************************************************************
 *		       RESTRICT_FAMILIES_T			*
 ****************************************************************
 * For each family application F(X) in T, unify X with a copy   *
 * of the formal parameter of F in the declaration of F.  That  *
 * is, if F is defined by					*
 *								*
 *    Species F(A) = B.						*
 *								*
 * then unify X with a copy of A. Return true if the		*
 * unifications succeeded, false if a unification failed.  	*
 *								*
 * Record bindings done by unification, so that they can be 	*
 * undone later.						*
 ****************************************************************/

Boolean restrict_families_t(TYPE *t)
{
 tail_recur:
  t = find_u(t);
  switch(TKIND(t)) {
    /*------------------------------------------------*/
    default: 
      return TRUE;

    /*------------------------------------------------*/
    case FAM_MEM_T:
      {TYPE* f = find_u(t->TY2), *arg_type;
       Boolean result;

       if(!restrict_families_t(t->TY1)) return FALSE;

       if(TKIND(f) == FAM_T) {

         /*-----------------------------------------------------*
          * Be careful - if f is a pseudo-family, then there	*
          * is no ctc to look at.				*
          *-----------------------------------------------------*/

	 if(f->ctc != NULL) {
	   bump_type(arg_type = copy_type(f->ctc->CTC_REP_TYPE->TY1, 0));
	   result = unify_u(&(t->TY1), &(arg_type), TRUE);
	   drop_type(arg_type);
	   return result;
	 }
	 else return TRUE;
       }
       else return TRUE;
      }

    /*------------------------------------------------*/
    case PAIR_T:
    case FUNCTION_T:
      if(!restrict_families_t(t->TY1)) return FALSE;
      t = t->TY2;
      goto tail_recur;
  }
}


/****************************************************************
 *			COMMIT_RESTRICT_FAMILIES		*
 ****************************************************************
 * Restrict families in T, with bindings committed to.		*
 * Otherwise, this is the same as restrict_families_t.		*
 ****************************************************************/

Boolean commit_restrict_families_t(TYPE *t)
{
  LIST* mark;
  bump_list(mark = finger_new_binding_list());
  if(!restrict_families_t(t)) {
    drop_list(mark);
    return FALSE;
  }
  else {
    commit_new_binding_list(mark);
    drop_list(mark);
    return TRUE;
  }
}


/****************************************************************
 *			UNIFY_WITH_EQ				*
 ****************************************************************
 * Unify type *T with EQ`x.  Return TRUE on success, FALSE on	*
 * failure.							*
 ****************************************************************/

Boolean unify_with_eq(TYPE **t)
{
  TYPE *eq_var;
  Boolean result;

  bump_type(eq_var = var_t(EQ_ctc));
  result = unify_u(t, &eq_var, FALSE);
  drop_type(eq_var);
  return result;
}


/****************************************************************
 *			EQ_RESTRICT				*
 ****************************************************************
 * If T is not a family member, do nothing and return TRUE.	*
 * If T is a family member F(A), then unify A with EQ`x.	*
 * If unification fails, return FALSE.  If successful, return 	*
 * TRUE.							*
 ****************************************************************/

Boolean eq_restrict(TYPE *t)
{
  t = find_u(t);
  if(TKIND(t) != FAM_MEM_T) return TRUE;
  return unify_with_eq(&(t->TY1));
}


/******************************************************************
 *			SUBSTITUTE_AT_POSN_T			  *
 ******************************************************************
 * Return the type obtained by substituting SUBST at the position *
 * given by POSNLIST in T.  POSNLIST is a list of LEFT_SIDE and	  *
 * RIGHT_SIDE values.  [L,L,R] indicates that SUBST should be 	  *
 * substituted at the left of the left of the right of T.	  *
 *								  *
 * Function subst_help does the work.  Notice that it reverses    *
 * the sense of the list, so that [L,L,R] indicates the right of  *
 * the left of the left of T to subst_help.			  *
 ******************************************************************/

PRIVATE TYPE* subst_help(TYPE *t, TYPE *subst, INT_LIST *posnlist)
{
  if(posnlist == NIL) return subst;
  t = find_u(t);
  if(TKIND(t) != PAIR_T) die(156);

  if(posnlist->head.i == LEFT_SIDE) {
    return pair_t(subst_help(t->TY1, subst, posnlist->tail), t->TY2);
  }
  else {
    return pair_t(t->TY1, subst_help(t->TY2, subst, posnlist->tail));
  }
}

/*--------------------------------------------------------------*/

TYPE* substitute_at_posn_t(TYPE *t, TYPE *subst, INT_LIST *posnlist)
{
  INT_LIST *rl;
  TYPE *result;

  bump_list(rl = reverse_list(posnlist));
  result = subst_help(t, subst, rl);
  drop_list(rl);
  return result;
}


/****************************************************************
 *			LIST_TO_TYPE_EXPR			*
 ****************************************************************
 * Parameters TYPES and ROLES are parallel lists of types	*
 * and roles.  The n-th role belongs with the n-th type.  	*
 *								*
 * Parameter TYPES must be non-null.  Parameter ROLES can	*
 * be null -- a null role list is treated like a list of null   *
 * roles.							*
 *								*
 * Suppose that TYPES is [A,B,C].  				*
 *								*
 *   If MODE is 0, return (A,B,C).	  			*
 *   If MODE is 1, return (<:A:>,<:B:>,<:C:>). 			*
 *								*
 * Install roles appropriately, from list ROLES.		*
 * Roles of A, B and C are brought outside the boxes, so that	*
 * if TYPES if [A,B,C] and ROLES is [R,S,T], and MODE is 1, 	*
 * the result is (R~> <:A:>, S~> <:B:>, T~> <:C:>).		*
 ****************************************************************/

RTYPE list_to_type_expr(TYPE_LIST *types, ROLE_LIST *roles, int mode)
{
  RTYPE result;
  TYPE *this_type;

  this_type = (mode == 0) 
                ? types->head.type 
                : fam_mem_t(box_fam, types->head.type);

  if(types->tail == NIL) {
    result.role = roles == NIL ? NULL : roles->head.role;
    result.type = this_type;
  }
  else {
    ROLE_LIST* roles_tail = roles == NIL ? NIL : roles->tail;
    RTYPE rest = list_to_type_expr(types->tail, roles_tail, mode);
    result.role = pair_role(roles->head.role, rest.role);
    result.type = pair_t(this_type, rest.type);
  }
  return result;
}


/*********************************************************************
 *		CHECK_FOR_CONSTRAINED_DYNAMIC_VAR		     *
 *********************************************************************
 * Give a warning if type T contains a variable with both a >>= and  *
 * a >= constraint.						     *
 *								     *
 * If a warning is issued, THE_FUN is the responsible function.	     *
 *********************************************************************/

void check_for_constrained_dynamic_var(TYPE *t, EXPR *the_fun)
{
  TYPE_TAG_TYPE t_kind;

 tail_recur:
  t = find_u(t);
  if(t == NULL) return;
  t_kind = TKIND(t);

  if(IS_STRUCTURED_T(t_kind)) {
    check_for_constrained_dynamic_var(t->TY1, the_fun);
    t = t->TY2;
    goto tail_recur;
  }

  else if(IS_VAR_T(t_kind)) {
    TYPE_LIST* t_lwbs = lower_bound_list(t);
    if(t_lwbs != NIL) {
      TYPE_LIST *p;
      Boolean has_ge = FALSE, has_gge = FALSE;
      for(p = t_lwbs; p != NIL; p = p->tail) {
	int pkind = LKIND(p);
	if(pkind == TYPE_L) has_ge = TRUE;
	else if(pkind == TYPE1_L) has_gge = TRUE;
      }
      if(has_ge && has_gge) {
	constrained_dyn_var_err(t, the_fun);
      }
    }
  }
}


/****************************************************************
 *			GET_TRUE_LOWER_BOUNDS			*
 ****************************************************************
 * List L should be a lower bound list of a variable.		*
 *								*
 * Return a list of the cells in list L that have list tag	*
 * TYPE_L.  These are the >= lower bounds in L.  (The >>= lower *
 * bounds are not included.)					*
 ****************************************************************/

TYPE_LIST* get_true_lower_bounds(LIST *L)
{
  if(L == NIL) return NIL;
  else {
    TYPE_LIST* rest = get_true_lower_bounds(L->tail);
    if(LKIND(L) != TYPE_L) return rest;
    else if(rest == L->tail) return L;
    else return type_cons(L->head.type, rest);
  }
}


/****************************************************************
 *			CLASS_MEM				*
 ****************************************************************
 * If type V is in list L or is the value that some member of 	*
 * list L is bound to, then return that member of L.  		*
 * Otherwise, return NULL.					*
 *								*
 * Class_mem will look for V either as a member of L or as the  *
 * left-hand member of a pair in list L.  So if (V,A) occurs in *
 * L, then V will be returned.					*
 *								*
 * All members of list L should be (possibly bound) variables,  *
 * or should be PAIR_T types whose left-hand members are 	*
 * (possibly bound) variables.					*
 ****************************************************************/

TYPE* class_mem(TYPE *V, TYPE_LIST *l)
{
  TYPE_LIST *p;
  TYPE *q, *q_orig;
  TYPE_TAG_TYPE kind;

  for(p = l; p != NIL; p = p->tail) {
    q_orig = p->head.type;
    kind = TKIND(q_orig);  /* Should be PAIR_T or a variable kind. */

    /*------------------------------------------------------*
     * Get the variable out of a pair, if q_orig is a pair. *
     *------------------------------------------------------*/

    if(kind == PAIR_T) q_orig = q_orig->TY1;

    /*-------------------------------------------------------------*
     * Check each variable to which q is bound, so see if it is V. *
     *-------------------------------------------------------------*/

    q = q_orig;
    do {
      if(q == V) return q_orig;
      q = q->TY1;
    } while(q != NULL && TKIND(q) == kind);

    if(q == V) return q_orig;
  }

  return NULL;
}


/****************************************************************
 *			REDUCE_TYPE				*
 ****************************************************************
 * Return a type equivalent to T, but with duplicate types	*
 * replaced by the same node.  For example, if (Natural,Integer)*
 * occurs twice, then both occurrences will be made to point    *
 * to the same PAIR_T node.					*
 *								*
 * Reduce_type_help is similar, but takes a list SEEN of types  *
 * that have been encountered earlier, and that should be used  *
 * to replace identical types in T.				*
 ****************************************************************/

PRIVATE TYPE* reduce_type_help(TYPE *t, TYPE_LIST **seen)
{
  TYPE *find, *a, *b, *result;
  TYPE_TAG_TYPE t_kind;

  if(t == NULL) return NULL;

  t      = find_u(t);
  t_kind = TKIND(t);

  if(IS_STRUCTURED_T(t_kind)) {

    /*----------------------------------------------------------*
     * If this type has been seen before, then just return the 	*
     * previous node.						*
     *----------------------------------------------------------*/

    find = type_mem(t, *seen, 0);
    if(find != NULL) return find;

    /*-----------------------------------------------------------*
     * Otherwise, try to reduce each type, and build the result. *
     * If the reductions of the parts are identical nodes to	 *
     * the parts themselves, then don't build a new node.	 *
     * Add this node to seen.					 *
     *-----------------------------------------------------------*/

    a = reduce_type_help(t->TY1, seen);
    b = reduce_type_help(t->TY2, seen);
    result = (a == t->TY1 && b == t->TY2) ? t : new_type2(t_kind, a, b);
    set_list(seen, type_cons(result, *seen));
    return result;
  }

  else if(IS_VAR_T(t_kind)) {
    if(!t->seen) {
      TYPE_LIST* p;

      mark_seen(t);
      for(p = lower_bound_list(t); p != NIL; p = p->tail) {
        if(LKIND(p) != TYPE2_L) {
	  SET_TYPE(p->head.type, reduce_type_help(p->head.type, seen));
        }
      }
    }
    return t;
  }

  else {

    /*----------------------------------------------------------*
     * If type t has no structure (so it is a type, family or	*
     * variable) then there is no point in putting it in seen. 	*
     * Just return it.					 	*
     *----------------------------------------------------------*/

    return t;
  }
}

/*------------------------------------------------------------*/

TYPE* reduce_type(TYPE *t)
{
  TYPE_LIST* seen = NIL;
  TYPE *result;

  bump_type(result = reduce_type_help(t, &seen));
  reset_seen();
  drop_list(seen);
  if(result != NULL) result->ref_cnt--;
  return result;
}
  

/******************************************************************
 *			MEMBER_OF_CTC		 		  *
 ******************************************************************
 * Return a species or family that is a member of the genus or    *
 * community whose class table entry is CTC, or NULL if there	  *
 * is none.  See member_of_class, below, for more details.  This  *
 * function helps that function.				  *
 * 								  *
 * KIND tells what kind of member to return.  If KIND is a wrap-  *
 * variable kind, then return a wrapped member.  Otherwise, 	  *
 * return a primary member.					  *
 ******************************************************************/

PRIVATE TYPE* member_of_ctc(CLASS_TABLE_CELL *ctc, TYPE_TAG_TYPE kind)
{
  int mem_num;
  CLASS_TABLE_CELL *mem_ctc;
  TYPE *mem_ty;

  /*-------------------------------------------------------*
   * For ANY, let the species be ().  (A variable with 	   *
   * NULL ctc field ranges over ANY).  If we must return   *
   * a wrapped species, return ANY.			   *
   *-------------------------------------------------------*/

  if(ctc == NULL) {
    return IS_NOT_WRAP_VAR_T(kind) ? hermit_type : WrappedANY_type; 
  }

  /*-------------------------------------------------------*
   * If this genus or community is not marked as nonempty, *
   * then nothing has been placed in it.  Return NULL.     *
   *-------------------------------------------------------*/

  if(!(ctc->nonempty)) return NULL;

  /*----------------------------------------------------------*
   * Find a member from the mem_num field in the class table. *
   *----------------------------------------------------------*/

  mem_num = ctc->mem_num;
  mem_ctc = ctcs[mem_num];
  mem_ty  = mem_ctc->ty;
  switch(mem_ctc->code) {
    /*-------------------------------------------------------*/
    case TYPE_ID_CODE:
      return IS_WRAP_VAR_T(kind) ? wrap_tf(ctc) : mem_ty;

    /*-------------------------------------------------------*/
    case GENUS_ID_CODE:

      /*---------------------------------------------------------*
       * If the standard member is a genus, get a member of that *
       * genus.						         *
       *---------------------------------------------------------*/

      return member_of_class(mem_ctc->ty);

    /*-------------------------------------------------------*/
    case PAIR_CODE:
    case FUN_CODE:

      /*-------------------------------------------------------*
       * If the standard member is a pair or function, build a *
       * pair or function type with appropriate components.    *
       *-------------------------------------------------------*/

      {LPAIR lbl;
       TYPE *component1, *component2, *result;

       lbl = get_link_label_tm(ctc_num(ctc), ctc_num(mem_ctc));
       component1 = member_of_class(ctcs[lbl.label1]->ty);
       if(component1 == NULL) return NULL;

       bump_type(component1);
       component2 = member_of_class(ctcs[lbl.label2]->ty);
       if(component2 != NULL) {
	 result = (mem_ctc->code == PAIR_CODE) 
	            ? pair_t(component1, component2)
	            : function_t(component1, component2);
       }
       else result = NULL;
       drop_type(component1);
       return result;
      }

    /*-------------------------------------------------------*/
    case FAM_ID_CODE:
      {LPAIR lbl;
       TYPE *component_type, *result;

       /*----------------------------------------------------------*
	* The standard member is a family.  If we are asking for a *
	* member of a community, just return the family.  If we    *
	* are asking for a member of a genus, then build F(T) for  *
	* an appropriate species T, where T is determined from the *
	* link label.						   *
	*----------------------------------------------------------*/
	    
       bump_type(result = IS_WRAP_VAR_T(kind) ? wrap_tf(ctc) : mem_ty);
       if(ctc->code != COMM_ID_CODE) {
	 lbl = get_link_label_tm(ctc_num(ctc), ctc_num(mem_ctc));
	 component_type = member_of_class(ctcs[lbl.label1]->ty);
	 if(component_type != NULL) {
	   SET_TYPE(result, fam_mem_t(result, component_type));
	 }
	 else SET_TYPE(result, NULL);
       }
       if(result != NULL) result->ref_cnt--;
       return result;
      }

    /*-------------------------------------------------------*/
    case COMM_ID_CODE:
      {LPAIR lbl;
       TYPE *component_type, *fam, *result;

       /*-------------------------------------------------------*
	* Handle communities somewhat like families.  If we are *
	* asked for a member of a community, then just get the  *
	* member of this community.  If asked for a member of   *
	* a class, build it.				        *
	*-------------------------------------------------------*/

       fam = member_of_class(mem_ty);
       if(fam == NULL) return NULL;

       if(ctc->code == COMM_ID_CODE) return fam; 

       bump_type(fam);
       lbl = get_link_label_tm(ctc_num(ctc), ctc_num(mem_ctc));
       component_type = member_of_class(ctcs[lbl.label1]->ty);
       if(component_type != NULL) {
	 result = fam_mem_t(fam, component_type);
       }
       else result = NULL;

       drop_type(fam);
       return result;
      }

    default:
      die(12);
      return NULL;  
  } /* end switch(mem_ctc->code) */
}


/******************************************************************
 *			MEMBER_OF_CLASS		 		  *
 ******************************************************************
 * Return a species that is a member of polymorphic type T, or    *
 * NULL if there is none.  T can be structured.  For example, if  *
 * T is (`a, REAL`b) then member_of_class(T) might return 	  *
 * ((),Natural).						  *
 *								  *
 * Note: Every genus G has a member G.  Such a member is not      *
 * returned for G`a.  Instead, a primary member is 	  	  *
 * returned.  If G has no primary member, then NULL is	  	  *
 * returned.  For G``a, G is returned, but only if G has    	  *
 * a primitive member.  Otherwise, NULL is returned.		  *
 ******************************************************************/

TYPE* member_of_class(TYPE *t)
{
  TYPE_TAG_TYPE t_kind;
  TYPE* result = NULL;  /* default */

  /*----------------------------------------------------------------*
   * Scan to the end of chain of bound variables, and check kind of *
   * type at the end of that chain.				    *
   *----------------------------------------------------------------*/

  t      = find_u(t);
  t_kind = TKIND(t);
  
  /*---------------------------------------------------------------*
   * A constructed type is handled by doing its parts recursively. *
   *---------------------------------------------------------------*/

  if(IS_STRUCTURED_T(t_kind)) {
    TYPE *t1,*t2;

    t1 = member_of_class(t->TY1);
    if(t1 == NULL)  goto out; /* result is NULL */

    bump_type(t1);
    t2 = member_of_class(t->TY2);
    if(t2 != NULL) {
      result = new_type2(t_kind, t1, t2);
    }

    drop_type(t1);
  }

  /*-----------------------------------------------------------------*
   * A variable is handled by extracting the member type or family   *
   * from the class table entry.  In the class table, field nonempty *
   * is true if the genus or community is nonempty.  When nonempty   *
   * is true, mem_num gives the index in the table of the member     *
   * type or family.						 *
   *-----------------------------------------------------------------*/

  else if(IS_VAR_T(t_kind)) {
    result = member_of_ctc(t->ctc, t_kind);
  }

  /*-----------------------------------------------------*
   * Anything else is handled by returning it unchanged. *
   *-----------------------------------------------------*/

  else {
    result = t;
  }

out:

# ifdef DEBUG
    if(trace_missing) {
      trace_t(8);
      trace_ty( t);
      fprintf(TRACE_FILE, ") = ");
      trace_ty( result);
      tracenl();
    }
# endif

  return result;
}
