/*********************************************************************
 * File:    utils/listutil.c
 * Purpose: Miscellaneous functions for lists.
 * Author:  Karl Abrahamson
 *********************************************************************/

#ifdef COPYRIGHT
/************************************************************************
 * Copyright (c) 2000 Karl Abrahamson					*
 * All rights reserved.							*
 * See file COPYRIGHT.txt for details.					*
 ************************************************************************/
#endif

/************************************************************************
 * This file provides a collection of operations on lists, including	*
 *   list copy								*
 *   list subscripting							*
 *   list length							*
 *   list reversal							*
 *   sorting								*
 *   append								*
 *   membership tests							*
 *   deleting items							*
 *   set intersection							*
 *   set union								*
 *   set difference							*
 ************************************************************************/

#include "../misc/misc.h"
#include "../utils/lists.h"
#include "../tables/tables.h"
#include "../alloc/allocate.h"
#include "../classes/classes.h"
#include "../unify/unify.h"
#include "../ids/open.h"
#include "../exprs/expr.h"
#include "../generate/generate.h"
#include <string.h>
#ifdef DEBUG
# include "../debug/debug.h"
#endif


/*==============================================================*
 *		UTILITIES FOR COMPILER AND INTERPRETER		*
 *==============================================================*/

/****************************************************************
 *			  REVERSE_LIST  			*
 ****************************************************************
 * Return the reversal of L.					*
 ****************************************************************/

LIST* reverse_list(LIST *l)
{
  LIST *p, *res;

  res = NIL;
  for(p = l; p != NIL; p = p->tail) {
    res = general_cons(p->head, res, LKIND(p));
  }
  return res;
}


/****************************************************************
 *				APPEND	 			*
 ****************************************************************
 * Return L1 ++ L2.						*
 ****************************************************************/

LIST* append(LIST *l1, LIST *l2)
{
  if(l1 == NIL) return l2;
  return general_cons(l1->head, append(l1->tail, l2), toint(LKIND(l1)));
}


/****************************************************************
 *		       APPEND_WITHOUT_DUPS	 		*
 ****************************************************************
 * Append, removing duplicates.  The lists are assumed to hold  *
 * integers.  This can be used for other types, but you must	*
 * be aware that equality comparisons are done using ==.	*
 ****************************************************************/

INT_LIST* append_without_dups(INT_LIST *l1, INT_LIST *l2)
{
  LIST *rest;

  if(l1 == NIL) return l2;
  rest = append_without_dups(l1->tail, l2);
  if(int_member(l1->head.i, l2)) return rest;
  else return general_cons(l1->head, rest, toint(LKIND(l1)));
}


/****************************************************************
 *			COPY_LIST 				*
 ****************************************************************
 * Returns a copy of list L.  Only the list structure is copied.*
 ****************************************************************/

LIST* copy_list(LIST *l)
{
  if(l == NIL) return NIL;
  else return general_cons(l->head, copy_list(l->tail), l->kind);
}


/****************************************************************
 *			LIST_SUBSCRIPT 				*
 ****************************************************************
 * Returns L sub N, where subscripting is from 1.		*
 *								*
 * What is actually returned is the list obtained by removing   *
 * the first N-1 members of L.  To get L sub N, take the head   *
 * of that list.						*
 ****************************************************************/

LIST* list_subscript(LIST *l, int n)
{
  LIST *p;
  int i;

  for(p = l, i = n; p != NIL && i > 1; p = p->tail, i--){}
  return p;
}


/****************************************************************
 *			LIST_LENGTH 				*
 ****************************************************************
 * Returns the length of list L.				*
 ****************************************************************/

int list_length(LIST *l)
{
  LIST *p;
  int n;

  n = 0;
  for(p = l; p != NIL; p = p->tail) n++;
  return n;
}


/****************************************************************
 *			DROP_HASH_LIST				*
 ****************************************************************
 * Drop the val.list field of cell H.				*
 ****************************************************************/

void drop_hash_list(HASH2_CELLPTR h)
{
  drop_list(h->val.list);
}


/****************************************************************
 *				STR_MEMBER 			*
 ****************************************************************
 * Return the index in list L of string S (numbering from 1), 	*
 * or 0 if S does not occur in l.  Uses strcmp to compare	*
 * strings.							*
 ****************************************************************/

int str_member(CONST char *s, STR_LIST *l)
{
  int n;
  STR_LIST *p;

  if(s == NULL) return 0;

  n = 1;
  for(p = l; p != NIL; p = p->tail) {
    if(LKIND(p) == STR_L && p->head.str != NULL && 
       strcmp(p->head.str, s) == 0) return n;
    else n++;
  }
  return 0;
}


/****************************************************************
 *				STR_MEMQ 			*
 ****************************************************************
 * Return true if string S occurs in list L.			*
 * Uses == to compare strings.					*
 ****************************************************************/

Boolean str_memq(CONST char *s, STR_LIST *l)
{
  STR_LIST *p;

  for(p = l; p != NIL; p = p->tail) {
    if(p->head.str == s) {
      return TRUE; 
    }
  }
  return FALSE;
}


/****************************************************************
 *				INT_MEMBER 			*
 ****************************************************************
 * Returns true if N is a member of list L.			*
 ****************************************************************/

Boolean int_member(LONG n, INT_LIST *l)
{
  INT_LIST *p;
  for(p = l; p != NIL; p = p->tail) {
    if(p->head.i == n) {
      return TRUE;
    }
  }
  return FALSE;
}


/****************************************************************
 *			DELETE_STRING 				*
 ****************************************************************
 * Return the list obtained by deleting the first occurrence of	*
 * S from list L.  Uses strcmp to compare strings.		*
 ****************************************************************/

LIST* delete_string(char *name, LIST *l)
{
  if(l == NIL) return NIL;

  if(strcmp(l->head.str, name) == 0) return l->tail;

  return str_cons(l->head.str, delete_string(name, l->tail));
}


/****************************************************************
 *			DELETE_STR 				*
 ****************************************************************
 * Return the list obtained by deleting the first occurrence of	*
 * S from list L.  Uses == to compare strings.			*
 ****************************************************************/

LIST* delete_str(char *name, LIST *l)
{
  if(l == NIL) return NIL;

  if(l->head.str == name) return l->tail;

  return str_cons(l->head.str, delete_str(name, l->tail));
}


/*****************************************************************
 *			STR_LIST_INTERSECT			 *
 *****************************************************************
 * This function returns the intersection of lists A and B 	 *
 * (those items that are members of both).  It uses == to	 *
 * compare list members.  					 *
 *								 *
 * This function saves space by returning A if all members of A  *
 * are in B.  As a consequence, it is possible to test whether 	 *
 * A is a subset of B by asking whether str_list_intersect(A,B)  *
 * == A.  THIS CHARACTERISTIC IS USED IN GLOBTBL.C, SO	 	 *
 * DON'T CHANGE IT.						 *
 *****************************************************************/

STR_LIST* str_list_intersect(STR_LIST *a, STR_LIST *b)
{
  STR_LIST *p, *result;
  Boolean nonb_seen = FALSE;

  if(b == NIL) return NIL;

  result = NIL;
  for(p = a; p != NIL; p = p->tail) {
    if(str_memq(p->head.str, b)) {
      SET_LIST(result, str_cons(p->head.str, result));
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
 *			STR_LIST_DIFFERENCE			*
 ****************************************************************
 * This function returns the difference of lists A and B 	*
 * (those strings that are in A but not in B).  It uses == to	*
 * compare list members.  					*
 *								*
 * This function saves space by returning A if no members of 	*
 * B are in A.  So you can test that A and B are disjoint by	*
 * testing that str_list_difference(A,B) == A.  THIS 		*
 * CHARACTERISTIC MIGHT BE USED, SO DON'T CHANGE IT.		*
 ****************************************************************/

STR_LIST* str_list_difference(STR_LIST *a, STR_LIST *b)
{
  STR_LIST *p, *result;
  Boolean b_seen = FALSE;

  if(b == NIL) return a;

  result = NIL;
  for(p = a; p != NIL; p = p->tail) {
    if(!str_memq(p->head.str, b)) {
      SET_LIST(result, str_cons(p->head.str, result));
    }
    else b_seen = TRUE;
  }

  if(!b_seen) {
    drop_list(result);
    return a;
  }
  else {
    if(result != NIL) result->ref_cnt--;
    return result;
  }
}


/****************************************************************
 *			STR_LIST_DISJOINT			*
 ****************************************************************
 * Return true if lists A and B are disjoint.  Use == to 	*
 * do comparisons.						*
 ****************************************************************/

Boolean str_list_disjoint(STR_LIST *a, STR_LIST *b)
{
  STR_LIST *d;
  Boolean result;

  bump_list(d = str_list_intersect(a,b));
  result = d == NIL;
  drop_list(d);
  return result;
}


/****************************************************************
 *			STR_LIST_UNION				*
 ****************************************************************
 * Return the union of lists A and B.  Use  == to compare	*
 * list members.  						*
 *								*
 * This function returns A if all members of B are in A.  That  *
 * way, it is possible to test whether B is a subset of A by	*
 * testing whether str_list_union(A,B) == B.  THIS 		*
 * CHARACTERISTIC MIGHT BE USED, SO DON'T CHANGE IT.		*
 ****************************************************************/

STR_LIST* str_list_union(STR_LIST *a, STR_LIST *b)
{
  STR_LIST *p, *result;

  if(a == NIL) return b;

  result = a;
  for(p = b; p != NIL; p = p->tail) {
    if(!str_memq(p->head.str, a)) {
      result = str_cons(p->head.str, result);
    }
  }
  return result;
}


/****************************************************************
 *			STR_LIST_SUBSET				*
 ****************************************************************
 * Return true if A is a subset of B.  Strings are compared	*
 * using ==.							*
 ****************************************************************/

Boolean str_list_subset(STR_LIST *a, STR_LIST *b)
{
  STR_LIST *u;
  Boolean result;

  bump_list(u = str_list_union(b,a));
  result = u == b;
  drop_list(u);
  return result;  
}


/*==============================================================*
 *		UTILITIES FOR COMPILER ONLY			*
 *==============================================================*/

#ifdef TRANSLATOR

/****************************************************************
 *			STR_LIST_EQUAL_SETS			*
 ****************************************************************
 * Return true if A and B contain the same members.  Strings	*
 * are compared using ==.					*
 ****************************************************************/

Boolean str_list_equal_sets(STR_LIST *a, STR_LIST *b)
{
  return str_list_subset(a, b) && str_list_subset(b, a);
}


/****************************************************************
 *			MERGE_ID_LISTS				*
 ****************************************************************
 * Return merged sorted lists S and T.  This is a destructive 	*
 * operation -- it destroys lists S and T. 			*
 *								*
 * The members of lists S and T are presumed to be EXPR nodes   *
 * that are identifiers.  Sorting is alphabetic by name.	*
 ****************************************************************/

EXPR_LIST* merge_id_lists(EXPR_LIST *s, EXPR_LIST *t)
{
  if(s == NIL) return t;
  if(t == NIL) return s;

  if(strcmp(s->head.expr->STR, t->head.expr->STR) < 0) {
    SET_LIST(s->tail, merge_id_lists(s->tail, t));
    return s;
  }
  else {
    SET_LIST(t->tail, merge_id_lists(s, t->tail));
    return t;
  }
}


/****************************************************************
 *			SORT_ID_LIST				*
 ****************************************************************
 * List L should be a list of EXPR nodes that are identifiers.	*
 *								*
 * Destructively sort the first K members of list *L into	*
 * alphabetic order by name, and remove them from *L, setting   *
 * *L to the suffix (after the first K members). 		*
 ****************************************************************/

EXPR_LIST* sort_id_list(EXPR_LIST **l, int k)
{
  int halfk;
  EXPR_LIST *s, *t, *result;

  if(k == 0) return NIL;

  /*----------------------------------------------------*
   * When k = 1, we are asked to sort only the first 	*
   * member.  If *l = [a,b,c], then the result is     	*
   * [a], and *l should be set to [b,c].		*
   *----------------------------------------------------*/

  if(k == 1) {
    bump_list(t = *l);
    SET_LIST(*l, (*l)->tail);
    SET_LIST(t->tail, NIL);
    t->ref_cnt--;
    return t;
  }

  /*-----------------------------------------------------*
   * For larger k, first get the first k/2 members, then *
   * the next k - k/2 members, and merge.		 *
   *-----------------------------------------------------*/

  else {
    halfk = k/2;
    bump_list(s = sort_id_list(l, halfk));
    bump_list(t = sort_id_list(l, k - halfk));
    bump_list(result = merge_id_lists(s,t));
    drop_list(s);
    drop_list(t);
    result->ref_cnt--;
    return result;
  }
}


/****************************************************************
 *			INTERSECT_ID_LISTS			*
 ****************************************************************
 * Return two lists LA and LB.  LA is the list of identifiers	*
 * in L1 that have correspondingly-named identifiers in L2.  LB *
 * is the list of identifiers in L2 that have correspondingly-	*
 * named identifiers in L1.  The name lists of LA and LB are	*
 * identical, but the expression nodes will in general be 	*
 * different.  LA and LB are ref-counted.			*
 *								*
 * Special handling: an identifier that starts with char code 1 *
 * is interpreted as representing all ids, thus suppressing	*
 * intersection.						*
 *								*
 * Requires that L1 and L2 be sorted by name.  			*
 *								*
 *		-------------------				*
 * 		DESTROYS L1 AND L2.				*
 *		-------------------				*
 *								*
 ****************************************************************/

void intersect_id_lists(EXPR_LIST *l1, EXPR_LIST *l2, 
			EXPR_LIST **la, EXPR_LIST **lb)
{
  EXPR_LIST *s, *t;
  int c;
  char *name1, *name2;

  if(l1 == NIL || l2 == NIL) {
    *la = *lb = NIL;
    return;
  }

  name1 = l1->head.expr->STR;
  name2 = l2->head.expr->STR;
  if(name1[0] == 1) {
    bump_list(*la = l2);
    bump_list(*lb = l2);
    return;
  }

  if(name2[0] == 1) {
    bump_list(*la = l1);
    bump_list(*lb = l1);
    return;
  }

  c = strcmp(name1, name2);
  if(c == 0) {
    intersect_id_lists(l1->tail, l2->tail, &s, &t);
    SET_LIST(l1->tail, s);
    SET_LIST(l2->tail, t);
    bump_list(*la = l1);
    bump_list(*lb = l2);
    drop_list(s);
    drop_list(t);
  }
  else if(c < 0) {
    intersect_id_lists(l1->tail, l2, la, lb);
  }
  else { /* c > 0 */
    intersect_id_lists(l1, l2->tail, la, lb);
  }
}

#endif
