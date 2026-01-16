/**********************************************************************
 *    File:    alloc/aloctype.c
 *    Purpose: Manage storage for TYPE nodes
 *    Author:  Karl Abrahamson
 **********************************************************************/

#ifdef COPYRIGHT
/************************************************************************
 * Copyright (c) 2000 Karl Abrahamson					*
 * All rights reserved.							*
 * See file COPYRIGHT.txt for details.					*
 ************************************************************************/
#endif

/************************************************************************
 * Types are managed by reference counts.  Types are allocated		*
 * with a ref count of 0.  Use						*
 *									*
 *   bump_type(e)   to increment ref count of e				*
 *									*
 *   drop_type(e)   to decrement ref count of e, and possibly free,	*
 *									*
 *   SET_TYPE(x,e)  to set type variable x to e, when x already has a	*
 * 		    value.						*
 ************************************************************************/

#include <memory.h>
#include "../misc/misc.h"
#include "../alloc/allocate.h"
#include "../classes/classes.h"
#include "../utils/lists.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/********************************************************
 *			FREE SPACE LIST			*
 ********************************************************/

PRIVATE TYPE* free_types = NULL;


/********************************************************
 *			ALLOCATE_TYPE			*
 ********************************************************
 * Return a new type node, zeroed out.  The ref count	*
 * is 0.				                *
 ********************************************************/

TYPE* allocate_type(void)
{
  TYPE* t = free_types;

  if(t != NULL_T) free_types = t->ty1;
  else {
    t = (TYPE*) alloc_small(sizeof(TYPE));
  }

  memset(t, 0, sizeof(TYPE));

# ifdef DEBUG
    allocated_types++;
# endif

  return t;
}


/******************************************************************
 *			REMOVE_BACK_POINTERS_FROM_ALL		  *
 ******************************************************************
 * REMOVE_BACK_POINTERS_FROM_ALL removes any back pointers to	  *
 * node V from the lower bound list of every variable in list L.  *
 *								  *
 * REMOVE_BACK_PTR and REMOVE_BACK_POINTERS are helper functions. *
 ******************************************************************/

PRIVATE LIST* remove_back_ptr(TYPE *V, LIST *L)
{
  LIST *rest;

  if(L == NIL) return NIL;

  rest = remove_back_ptr(V, L->tail);
  if(LKIND(L) == TYPE2_L && L->head.type == V) return rest;
  else if(rest == L->tail) return L;
  else {
    HEAD_TYPE hd;
    hd.type = L->head.type;
    return general_cons(hd, rest, LKIND(L));
  }
}

/*--------------------------------------------------------*/

PRIVATE void remove_back_pointers(TYPE *V, TYPE *X)
{
  while(X != NULL && IS_VAR_T(TKIND(X))) {
    if(X->LOWER_BOUNDS != NIL) {
      SET_LIST(X->LOWER_BOUNDS, remove_back_ptr(V, X->LOWER_BOUNDS));
    }
    X = X->TY1;
  }
}

/*--------------------------------------------------------*/

PRIVATE void remove_back_pointers_from_all(TYPE *V, LIST *L)
{
  LIST *p;
  for(p = L; p != NIL; p = p->tail) {
    remove_back_pointers(V, p->head.type);
  }
}


/******************************************************************
 *			FREE_TYPE 				  *
 ******************************************************************
 * Return T to the free space list, first dropping any references *
 * that it has.	 T should not be NULL.				  *
 ******************************************************************/

void free_type(TYPE *t)
{
# ifdef DEBUG
    if(trace_frees) {
      trace_s(3, t);
      trace_ty( t);
      fnl(TRACE_FILE);
    }
# endif

  /*---------------------------------------------------------------*
   * The seen field is used to break cycles in the type structure. *
   * It is possible that we are freeing a cyclic type.		   *
   *---------------------------------------------------------------*/

  if(t->seen == 0) {
    t->seen = 1;

    /*------------------*
     * Recursive frees. *
     *------------------*/

    switch(t->kind){
      case FAM_MEM_T:
      case PAIR_T:
      case FUNCTION_T:
        drop_type(t->TY2);
	drop_type(t->TY1);
        break;

      default: 
	break;

      case MARK_T:
	drop_type(t->TY1);
	break;

      case TYPE_VAR_T:
      case FAM_VAR_T:
      case WRAP_TYPE_VAR_T:
      case WRAP_FAM_VAR_T:
	drop_list(t->LOWER_BOUNDS);
	drop_type(t->TY1);
        break;

      case PRIMARY_TYPE_VAR_T:
      case PRIMARY_FAM_VAR_T:
	/*-------------------------------------------------------*
	 * There might be back pointers in the lower bound list. *
         * Delete them.  They are not ref-counted.		 *
         *-------------------------------------------------------*/

	if(t->LOWER_BOUNDS != NIL) {
	  remove_back_pointers_from_all(t, t->LOWER_BOUNDS);
	  drop_list(t->LOWER_BOUNDS);
	}
	drop_type(t->TY1);
        break;
    }

    /*-----------------*
     * Free this cell. *
     *-----------------*/

#   ifdef DEBUG
      allocated_types--;
#   endif

#   ifdef GCTEST
      t->ref_cnt = -100;
#   else
      t->ty1     = free_types;
      free_types = t;
#   endif
  }
}


/************************************************************************
 *	     			SET_TYPE				*
 ************************************************************************
 * Set variable X to T, keeping track of reference counts.              *
 ************************************************************************/

void set_type(TYPE **x, TYPE *t)
{
  register TYPE *starx;

  /*--------------------------------------*
   * First bump the reference count of t. *
   *--------------------------------------*/

  if(t != NULL_T) t->ref_cnt++;

  starx = *x;

  /*--------------------------------------------------------------------*
   * Now drop the reference count of *x.  				*
   * 									*
   * If *x has a nonzero freeze field, then do not free it.  But only   *
   * check that in the interpreter.					*
   *--------------------------------------------------------------------*/

  if(starx != NULL_T) {
#   ifdef GCTEST
      TKIND(starx);  /* Check if starx was already freed. */
#   endif

#   ifdef MACHINE
      if(starx->ref_cnt > 1) starx->ref_cnt--;
      else if(!starx->freeze) { 
        free_type(starx);
      }
#   else
      if(--(starx->ref_cnt) <= 0) free_type(starx);
#   endif
  }

  /*--------------------------------------*
   * Now set *x = t to do the assignment. *
   *--------------------------------------*/

  *x = t;
}


/****************************************************************
 *			BUMP_TYPE				*
 ****************************************************************
 * Increment the reference count of T.  T can be NULL.          *
 ****************************************************************/

void bump_type(TYPE *t)
{
  if(t != NULL_T) {
#   ifdef GCTEST
      TKIND(t);  /* Check if already freed. */
#   endif

    t->ref_cnt++;
  }
}


/****************************************************************
 *			DROP_TYPE				*
 ****************************************************************
 * Decrement the reference count of T, possibly freeing.        *
 * Do recursive drops if freed.  T can be NULL.			*
 ****************************************************************/

void drop_type(TYPE *t)
{
  /*--------------------------------------------------------------------*
   * If t has a nonzero freeze field, then it is marked for		*
   * preservation.  Do not drop the reference count.  This is only	*
   * done in the interpreter.						*
   *--------------------------------------------------------------------*/

  if(t != NULL) {
#   ifdef GCTEST
      TKIND(t);
#   endif

#   ifdef MACHINE
      if(t->ref_cnt > 1) t->ref_cnt--;
      else if(!t->freeze) {
	free_type(t);
      }
#   else
      if(--(t->ref_cnt) <= 0) free_type(t);
#   endif

  }
}  

