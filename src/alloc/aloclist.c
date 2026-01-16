/**************************************************************
 * File:    alloc/aloclist.c
 * Purpose: Implement free space management routines for lists.
 * Author:  Karl Abrahamson
 **************************************************************/

#ifdef COPYRIGHT
/************************************************************************
 * Copyright (c) 2000 Karl Abrahamson					*
 * All rights reserved.							*
 * See file COPYRIGHT.txt for details.					*
 ************************************************************************/
#endif

/************************************************************************
 * Lists are managed by reference counts.  Lists are allocated		*
 * with a ref count of 0.  Use                                          *
 *									*
 *   bump_list(l)   to increment ref count of l                         *
 *									*
 *   drop_list(l)   to decrement ref count of l, and possibly free,     *
 *									*
 *   SET_LIST(x,l)  to set list variable x to l, when x already has     *
 *		    a value.                                            *
 *									*
 *   free_list_node(l) to free list node l, without looking inside it.  *
 ************************************************************************/

#include "../misc/misc.h"
#include "../alloc/allocate.h"
#include "../utils/lists.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif


/********************************************************
 *			FREE SPACE LIST			*
 ********************************************************/

PRIVATE LIST* free_lists = NULL;


/********************************************************
 *			ALLOCATE_LIST			*
 ********************************************************
 * Return a pointer to a new list cell.  The reference  *
 * count and marks are both 0.		                *
 ********************************************************/

LIST* allocate_list(void)
{
  LIST* e = free_lists;

  if(e != NIL) free_lists = e->tail;
  else {
    e = (LIST*) alloc_small(sizeof(LIST));
  }
  e->mark = 0;
  e->ref_cnt = 0;

# ifdef DEBUG
    allocated_lists++;
# endif

  return e;
}


/************************************************************************
 *	     			SET_LIST				*
 ************************************************************************
 * Set variable X to T, managing reference counts.                      *
 * (If T is the special address 1, do not bump its reference count.)	*
 ************************************************************************/

void set_list(LIST **x, LIST *t)
{
  if(t != NIL && t != (LIST *) 1) t->ref_cnt++;
  drop_list(*x);
  *x = t;
}


/****************************************************************
 *	      		BUMP_LIST				*
 ****************************************************************
 * Increment the reference count of L.  L can be NULL.          *
 * (If L is the special address 1, do not bump its reference	*
 *  count.)							*
 ****************************************************************/

void bump_list(LIST *l)
{
  if(l != NIL && l != (LIST *) 1) l->ref_cnt++;
}

/****************************************************************
 *	      		DROP_LIST				*
 ****************************************************************
 * Decrement the reference count of LL, and free LL if the count*
 * goes to 0.  Do recursive drops where necessary.		*
 * List LL can be NULL.  Ignore LL if it is the special pointer	*
 * 1.								*
 ****************************************************************/

void drop_list(LIST *ll)
{
# ifndef GCTEST
    HEAD_TYPE u;
# endif
  register LIST* p;
  register LIST* l = ll;

  if(l == (LIST *) 1) return;

  while(l != NIL) {
#   ifdef GCTEST
      LKIND(l);  /* check the ref count. */
#   endif

    if(--(l->ref_cnt) <= 0) {

      /*------------------*
       * Recursive drops. *
       *------------------*/

#     ifndef GCTEST
        u.str = NULL;
        set_head(l,0,u);
#     else
        drop_head(LKIND(l), l->head);
#     endif

      /*-----------------*
       * Free this node, *
       *-----------------*/

      p = l->tail;
#     ifdef DEBUG
        free_list_node(l);
#     else
        l->tail    = free_lists;
        free_lists = l;
#     endif
      l = p;
    }

    else break;
  }
} 


/****************************************************************
 *	      		FREE_LIST_NODE				*
 ****************************************************************
 * Free node L, without looking inside it.                      *
 ****************************************************************/

void free_list_node(LIST *l)
{
# ifdef DEBUG
    if(trace_frees) {
      trace_s(0, l);
      if(l->kind == STR_L) 
        trace_s(1, l->head.str, tolong(l->ref_cnt));
      else 
        trace_s(2, toint(l->kind), tolong(l->ref_cnt));
    }
    allocated_lists--;
# endif

# ifdef GCTEST
    LKIND(l);
    l->ref_cnt = -100;
# else
    l->tail    = free_lists;
    free_lists = l;
# endif
}

