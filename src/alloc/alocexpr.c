/*****************************************************************
 * File:    alloc/alocexpr.c
 * Purpose: Storage management for EXPRs
 * Author:  Karl Abrahamson
 *****************************************************************/

#ifdef COPYRIGHT
/************************************************************************
 * Copyright (c) 2000 Karl Abrahamson					*
 * All rights reserved.							*
 * See file COPYRIGHT.txt for details.					*
 ************************************************************************/
#endif

/************************************************************************
 * Exprs are managed by reference counts.  Exprs are allocated		*
 * with a ref count of 0.  Use						*
 *									*
 *   bump_expr(e)   to increment ref count of e				*
 *									*
 *   drop_expr(e)   to decrement ref count of e, and possibly free,	*
 *									*
 *   SET_EXPR(x,e)  to set expr variable x to e, when x already has a 	*
 *		    value.						*
 ************************************************************************/

#include <memory.h>
#include "../misc/misc.h"
#include "../alloc/allocate.h"
#include "../exprs/expr.h"
#include "../generate/prim.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/********************************************************
 *			FREE SPACE LIST			*
 ********************************************************/

PRIVATE EXPR* free_exprs = NULL;


/********************************************************
 *			ALLOCATE_EXPR			*
 ********************************************************
 * Return a new expr node, zeroed out.  The ref count   *
 * is 0.				                *
 ********************************************************/

EXPR* allocate_expr(void)
{
  EXPR* e = free_exprs;

  if(free_exprs != NULL_E) free_exprs = e->E1;
  else e = (EXPR*) alloc_small(sizeof(EXPR));

  memset(e, 0, sizeof(EXPR));

# ifdef DEBUG
    allocated_exprs++;
# endif

  return e;
}


/************************************************************************
 *	     			SET_EXPR				*
 ************************************************************************
 * Set variable X to T, keeping track of reference counts.		*
 ************************************************************************/

void set_expr(EXPR **x, EXPR *t)
{
  EXPR* starx = *x;

  if(t != NULL_E) t->ref_cnt++;

  if(starx != NULL_E) {
#   ifdef GCTEST
      EKIND(starx);
#   endif
    if(--(starx->ref_cnt) <= 0) free_expr(starx);
  }

  *x = t;
}


/************************************************************************
 *	     			BUMP_EXPR				*
 ************************************************************************
 * Increment the reference count of X.  X can be NULL.                  *
 ************************************************************************/

void bump_expr(EXPR *x)
{
  if(x != NULL_E) x->ref_cnt++;
}


/************************************************************************
 *	     			DROP_EXPR				*
 ************************************************************************
 * Decrement the reference count of X, possibly freeing.  Handle	*
 * recursive drops to things pointed to by X.  X can be NULL.           *
 ************************************************************************/

void drop_expr(EXPR *x)
{
  if(x != NULL_E) {
#   ifdef GCTEST
      EKIND(x);  /* Check ref count */
#   endif
    if(--(x->ref_cnt) <= 0) free_expr(x);
  }
}  


/************************************************************************
 *			FREE_EXPR 					*
 ************************************************************************
 * Return EE to the free space list, but first drop reference counts     *
 * to the parts of EE.                                                   *
 ************************************************************************/

void free_expr(EXPR *ee)
{
  /*---------------------------------------------------------*
   * We will tail recur where possible to avoid deep stacks. *
   *---------------------------------------------------------*/

  EXPR* e = ee;	     /* Official param, changed at tail recursive calls. */
  EXPR* newe = NULL; /* Next to do recursive call on. */

 tail_recur:

# ifdef DEBUG
    if(trace_frees) trace_t(0, e);
# endif

  /*------------------*
   * Recursive drops. *
   *------------------*/

  switch(EKIND(e)){
    case OPEN_E:
      drop_list(e->EL1);
      drop_list(e->EL2);
      drop_expr(e->E1);
      break;

    case LOCAL_ID_E:
      drop_list(e->EL0);
      break;

    case SPECIAL_E:
      if(e->PRIMITIVE == PRIM_SPECIES) drop_expr(e->E1);
      /* No break: fall through to next case. */

    case GLOBAL_ID_E:
      drop_list(e->EL3);
      break;

    case IF_E:
    case TRY_E:
      drop_expr(e->E1);
      drop_expr(e->E2);

      /*-------------------------------------------------------------------*
       * The following is drop_expr(e->E3), inlined to get tail recursion. *
       * The call to free_expr becomes an assignment to newe.              *
       *-------------------------------------------------------------------*/

      {EXPR* x = e->E3;
       if(x != NULL_E) {
#        ifdef GCTEST
           EKIND(x);
#        endif
         if(--(x->ref_cnt) <= 0) newe = x;
       }
      }

      break;

    case FOR_E:
    case PAT_RULE_E:
      drop_expr(e->E3);
      /* No break - fall through */

    case LET_E:
    case DEFINE_E:
    case SEQUENCE_E:
    case TEST_E:
    case STREAM_E:
    case AWAIT_E:
    case PAIR_E:
    case APPLY_E:
    case FUNCTION_E:
    case LOOP_E:
    case MATCH_E:
    case LAZY_BOOL_E:
    case WHERE_E:
    case PAT_DCL_E:
    case TRAP_E:
    case EXPAND_E:
      drop_expr(e->E2);
      /* No break - fall through */

    case SINGLE_E:
    case LAZY_LIST_E:
    case EXECUTE_E:
    case PAT_VAR_E:
    case RECUR_E:
    drop_e1:

      /*----------------------------------------------------------*
       * The following is drop_expr(e->E1), inlined.  The call to *
       * free_expr becomes an assignment to newe.                 *
       *----------------------------------------------------------*/

      {EXPR* x = e->E1;
       if(x != NULL_E) {
#        ifdef GCTEST
           EKIND(x);
#        endif
         if(--(x->ref_cnt) <= 0) newe = x;
       }
      }
      break;

    case SAME_E:
      {int m = e->SAME_MODE;
       if(m == 5) {
	 drop_list(e->EL1);
	 drop_list(e->EL2);
	 drop_mode(e->SAME_E_DCL_MODE);
       }
       else if(m == 7) {
	 drop_expr(e->E3);
       }
       else if(m == EXPECT_ATT || m == ANTICIPATE_ATT) {
	 drop_mode(e->SAME_E_DCL_MODE);
       }
       goto drop_e1;  /* Preceding case */
      }

    default: 
      break;

    case MANUAL_E:
      drop_expr(e->E1);
      drop_list(e->EL2);
      break;
  }

# ifndef GCTEST
    SET_TYPE(e->ty, NULL_T);
# else
    drop_type(e->ty);
# endif

  /*-----------------*
   * Free this node. *
   *-----------------*/

# ifdef DEBUG
    allocated_exprs--;
# endif

# ifdef GCTEST
    e->ref_cnt = -100;
# else
    e->E1      = free_exprs;
    free_exprs = e; 
# endif

  /*----------------------------------------*
   * Tail recur on newe, if it is not null. *
   *----------------------------------------*/

  if(newe != NULL) {
    e = newe;
    newe = NULL;
    goto tail_recur;
  }
}
