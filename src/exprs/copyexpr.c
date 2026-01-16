/*********************************************************************
 * File:    exprs/copyexpr.c 
 * Purpose: Copy expressions.
 * Author:  Karl Abrahamson
 *********************************************************************/

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

/****************************************************************
 * Copying an expression copies all of its nodes and also       *
 * copies the types to which those nodes refer.			*
 ****************************************************************/

#include "../misc/misc.h"
#include "../alloc/allocate.h"
#include "../utils/hash.h"
#include "../utils/lists.h"
#include "../classes/classes.h"
#include "../error/error.h"
#include "../exprs/expr.h"
#include "../patmatch/patmatch.h"
#include "../generate/prim.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

PRIVATE EXPR* copy_expr1(EXPR *e, HASH2_TABLE **ex_b, HASH2_TABLE **ty_b);
PRIVATE EXPR_LIST* copy_expr_list1(EXPR_LIST *l, HASH2_TABLE **ex_b, 
			   HASH2_TABLE **ty_b);


/****************************************************************
 *			PRIVATE VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			ec_loop_st				*
 ****************************************************************
 * LOOP_E nodes point to an expression that contains, somewhere *
 * inside it, RECUR_E nodes that point back to this LOOP_E	*
 * node.  When an expression is being copied, and a RECUR_E	*
 * node is encountered, we need to know where the copy of the   *
 * LOOP_E node is located.  It will always be found on the top	*
 * of ec_loop_st.  (When a loop is entered, its root node is	*
 * copied, and a pointer to that copy is pushed onto 		*
 * ec_loop_st.)							*
 ****************************************************************/

PRIVATE EXPR_STACK ec_loop_st;


/****************************************************************
 *			COPY_GLOBAL_ID_EXPR			*
 ****************************************************************
 * Return a copy of expression e, which must be a GLOBAL_ID_E   *
 * or SPECIAL_E node.  The type is not copied.			*
 ****************************************************************/

EXPR* copy_global_id_expr(EXPR *e)
{
  EXPR* result = allocate_expr();
  *result = *e;
  result->ref_cnt = 1;
  bump_type(result->ty);
  bump_list(result->EL3);
  return result;
}


/****************************************************************
 *			COPY_EXPR				*
 ****************************************************************
 * Return a copy of expression e, with all of its types also 	*
 * copied. Each local id and pattern variable in e is copied.   *
 ****************************************************************/

EXPR* copy_expr(EXPR *e)
{
  /*------------------------------------------------------------*
   * When a local id or pattern variable is encountered, it is 	*
   * assigned a number, and is bound (at that number) in 	*
   * ex_bindings to a copy of itself.   That way, each time	*
   * a local id is copied, we can get a pointer to the same 	*
   * node.							*
   *								*
   * ty_b is similar, but binds type and family variables, so	*
   * that each copy of them is replaced by the same variable.	*
   *------------------------------------------------------------*/

  EXPR *ecpy;
  HASH2_TABLE *ex_bindings = NULL, *ty_bindings = NULL;

  ec_loop_st = NIL;

  /*--------------*
   * Do the copy. *
   *--------------*/

  bump_expr(ecpy = copy_expr1(e, &ex_bindings, &ty_bindings));

  /*--------------------------------------------*
   * Drop refs in ex_bindings and ty_bindings,  *
   * and free tables. 				*
   *--------------------------------------------*/

  scan_and_clear_hash2(&ex_bindings, drop_hash_expr);
  scan_and_clear_hash2(&ty_bindings, drop_hash_type);

  /*-------------------------------------------------------*
   * Drop the reference count for result, and return ecpy. *
   *-------------------------------------------------------*/

  if(ecpy != NULL) ecpy->ref_cnt--;
  return ecpy;
}


/*******************************************************************
 *			COPY_EXPR1				   *
 *******************************************************************
 * Return a copy of expression e, subject to the bindings in ex_b  *
 * and ty_b.  Clear the done field in SPECIAL_E nodes.  Copies	   *
 * will be recorded in ex_b, so that the copy of e can be found    *
 * by looking under e in table ex_b.  Requires ec_loop_st to be    *
 * set, as in copy_expr.					   *
 *******************************************************************/

PRIVATE EXPR* 
copy_expr1(EXPR *e, HASH2_TABLE **ex_b, HASH2_TABLE **ty_b)
{
  EXPR *ecpy;
  EXPR_TAG_TYPE kind;
  HASH2_CELLPTR h;
  HASH_KEY u;

  if(e == NULL_E) return NULL_E;

  /*------------------------------------------------------------*
   * If e is already in the table, just get it.  Otherwise, 	*
   * find out where to put it. 					*
   *------------------------------------------------------------*/

  u.num = tolong(e);
  h     = insert_loc_hash2(ex_b, u, inthash(u.num), eq);
  if(h->key.num != 0) return h->val.expr;

  /*-----------------------------------------*
   * Make a copy, and install in hash table. *
   *-----------------------------------------*/

  ecpy  = allocate_expr();
  *ecpy = *e;
  ecpy->ref_cnt = 1;
  h->key.num = tolong(e);
  bump_expr(h->val.expr = ecpy);

  /*--------------------------------*
   * Bump/copy the parts as needed. *
   *--------------------------------*/

  kind = EKIND(e);
  switch(kind) {
    case OVERLOAD_E:
    case IDENTIFIER_E:
    case CONST_E: 
    case UNKNOWN_ID_E:
    case BAD_E:

        /*----------------------*
         * No copy/bump needed. *
         *----------------------*/

	break;

    case LOCAL_ID_E:

        /*-----------------------------------------------------*
         * Only need to bump the EL0 field.  It is not copied. *
         *-----------------------------------------------------*/

	bump_list(ecpy->EL0);
	break;

    case SPECIAL_E:    /* Target does not need pat fun copied */
      	if(ecpy->PRIMITIVE == PRIM_SPECIES) {
	  bump_expr(ecpy->E1 = copy_expr1(e->E1, ex_b, ty_b));
      	}
 	ecpy->done = 0;

	/*--------------------------------------------------------------*
	 * No break - fall through to handle a SPECIAL_E the same as    *
         * a GLOBAL_ID_E.						*
	 *--------------------------------------------------------------*/

    case GLOBAL_ID_E:
	bump_list(ecpy->EL3);
        break;

    case PAT_FUN_E:
	bump_expr(ecpy->E3 = copy_expr1(e->E3, ex_b, ty_b));
	bump_list(ecpy->EL3);
	break;

    case PAT_RULE_E:
    case IF_E:
    case TRY_E:
    case FOR_E:
	bump_expr(ecpy->E3 = copy_expr1(e->E3, ex_b, ty_b));
        /*-------------------------*
         * No break - fall through *
         *-------------------------*/

    case LET_E:
    case APPLY_E:
    case SEQUENCE_E:
    case STREAM_E:
    case AWAIT_E:
    case PAIR_E:
    case MATCH_E:
    case LAZY_BOOL_E:
    case DEFINE_E:
    case FUNCTION_E:
    case WHERE_E:
    case TRAP_E:
    case TEST_E:
	bump_expr(ecpy->E2 = copy_expr1(e->E2, ex_b, ty_b));
	/*--------------------------*
	 * No break - fall through. *
	 *--------------------------*/

    case LAZY_LIST_E:
    case SINGLE_E:
    case EXECUTE_E:
    case PAT_VAR_E:
	bump_expr(ecpy->E1 = copy_expr1(e->E1, ex_b, ty_b));
	break;

    case SAME_E:
	bump_expr(ecpy->E1 = copy_expr1(e->E1, ex_b, ty_b));
	if(ecpy->SAME_MODE == 5) {
	  bump_list(ecpy->EL1);
	  bump_list(ecpy->EL2);
	}
	break;

    case MANUAL_E:
	bump_expr(ecpy->E1 = copy_expr1(e->E1, ex_b, ty_b));
	bump_list(ecpy->EL2);
	break;

    case LOOP_E:

        /*-------------------------------------------------------*
         * At a loop, we need to push the copy of the root onto  *
         * ec_loop_st, so that it can be found at RECUR_E nodes. *
	 * Then copy E1 and E2, and pop ec_loop_st.		 *
         *-------------------------------------------------------*/

        push_expr(ec_loop_st, ecpy);
	bump_expr(ecpy->E2 = copy_expr1(e->E2, ex_b, ty_b));
	bump_expr(ecpy->E1 = copy_expr1(e->E1, ex_b, ty_b));
	bump_list(ecpy->EL2);
        pop(&ec_loop_st);
	break;

    case RECUR_E:
	bump_expr(ecpy->E1 = copy_expr1(e->E1, ex_b, ty_b));
        ecpy->E2 = top_expr(ec_loop_st);  /* Not ref counted -- cyclic link */
	break;

    case OPEN_E:
	bump_expr(ecpy->E1 = copy_expr1(e->E1, ex_b, ty_b));
	bump_list(ecpy->EL1 = copy_expr_list1(ecpy->EL1, ex_b, ty_b));
	bump_list(ecpy->EL2 = copy_expr_list1(ecpy->EL2, ex_b, ty_b));
	break;

    case EXPAND_E:
    case PAT_DCL_E:
	bump_expr(ecpy->E2 = copy_expr1(e->E2, ex_b, ty_b));
	bump_expr(ecpy->E1 = copy_expr1(e->E1, ex_b, ty_b));
	goto out;
  }

  /*----------------*
   * Copy the type. *
   *----------------*/

  bump_type(ecpy->ty = copy_type1(e->ty, ty_b, 1));
  bump_role(ecpy->role);

 out:
  ecpy->ref_cnt--;
  return ecpy;
}


/****************************************************************
 *			COPY_EXPR_LIST1				*
 ****************************************************************
 * Copy each of the expressions in list l, and return a list	*
 * of the copies, in the same order as l.  Use bindings in	*
 * ex_b and ty_b.  (All expressions in the copy list will have	*
 * the same type variables and expression nodes, if they are	*
 * shared in the originals in list l.				*
 *								*
 * This function is used in copy_expr1, above.			*
 ****************************************************************/

PRIVATE EXPR_LIST* 
copy_expr_list1(EXPR_LIST *l, HASH2_TABLE **ex_b, HASH2_TABLE **ty_b)
{
  if(l == NIL) return NIL;

  return expr_cons(copy_expr1(l->head.expr, ex_b, ty_b), 
		   copy_expr_list1(l->tail, ex_b, ty_b));
}


/****************************************************************
 *			COPY_EXPR_LIST				*
 ****************************************************************
 * Return a list of copies of the members of list l.  Each	*
 * expression is copied separately, so that no nodes or type	*
 * variables will be shared among them.				*
 ****************************************************************/

EXPR_LIST* copy_expr_list(EXPR_LIST *l)
{ 
  if(l == NIL) return NIL;
  return expr_cons(copy_expr(l->head.expr), copy_expr_list(l->tail));
}


/*****************************************************************
 *			COPY_THE_PAT_LISTS			 *
 *****************************************************************
 * pls is a list of lists of expressions and possibly strings.   *
 * Return a similar list where all of the expressions are        *
 * replaced by their copies as found in ex_b.			 *
 *****************************************************************/

PRIVATE EXPR_LIST* copy_the_pat_list(LIST *pl, HASH2_TABLE *ex_b)
{
  HASH2_CELLPTR h;
  HASH_KEY u;

  if(pl == NIL) return NIL;

  if(LKIND(pl) == EXPR_L) {
    u.num = tolong(pl->head.expr);
    h     = locate_hash2(ex_b, u, inthash(u.num), eq);
    if(h->key.num == 0) die(9);
    return expr_cons(h->val.expr, copy_the_pat_list(pl->tail, ex_b));
  }
  else {
    return general_cons(pl->head, copy_the_pat_list(pl->tail, ex_b),
			LKIND(pl));
  }
}

/*------------------------------------------------------------*/

PRIVATE LIST* copy_the_pat_lists(LIST *pls, HASH2_TABLE *ex_b)
{
  if(pls == NIL) return NIL;
  return list_cons(copy_the_pat_list(pls->head.list, ex_b), 
		   copy_the_pat_lists(pls->tail, ex_b));
}


/****************************************************************
 *			COPY_EXPR_AND_CHOOSE_MATCHING_LISTS	*
 ****************************************************************
 * Return a copy of e, but also set copy_choose_matching_lists  *
 * to a copy of choose_matching_lists, sharing the expressions  *
 * in the copy of e.						*
 ****************************************************************/

EXPR* copy_expr_and_choose_matching_lists(EXPR *e)
{
  EXPR *ecpy;
  HASH2_TABLE *ex_bindings, *ty_bindings;

  /*------------------------------------*
   * Set up copy tables and loop stack. *
   *------------------------------------*/

  ex_bindings = ty_bindings = NULL;
  ec_loop_st = NIL;

  /*--------------*
   * Do the copy. *
   *--------------*/

  bump_expr(ecpy = copy_expr1(e, &ex_bindings, &ty_bindings));

  /*-----------------------------*
   * Copy choose_matching_lists. *
   *-----------------------------*/

  bump_list(copy_choose_matching_lists = 
	    copy_the_pat_lists(choose_matching_lists, ex_bindings));

  /*--------------------------------------------*
   * Drop refs in ex_bindings, and free tables. *
   *--------------------------------------------*/

  scan_and_clear_hash2(&ex_bindings, drop_hash_expr);
  scan_and_clear_hash2(&ty_bindings, drop_hash_type);

  if(ecpy != NULL) ecpy->ref_cnt--;
  return ecpy;
}


