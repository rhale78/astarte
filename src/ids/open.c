/*******************************************************************
 * File:    ids/open.c
 * Purpose: Handle open conditionals
 * Author:  Karl Abrahamson
 *******************************************************************/

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

/*******************************************************************
 * This file contains functions for creating and handling open     *
 * if and open try expressions.  An open if or try is represented  *
 * as an OPEN_E node,						   *
 *								   *
 *		OPEN_E(l1,l2)					   *
 *		  |						   *
 *		IF_E						   *
 *            /   |   \						   *
 *         cond   t    e					   *
 *								   *
 * where cond is the condition, t the then-part and e the 	   *
 * else-part.  Identically named identifiers in t and e have	   *
 * different nodes.  The OPEN_E node is responsible for binding    *
 * the different nodes together.  If t has node N1 for symbol x,   *
 * and e has node N2 for that same symbol x, then lists l1 and l2  *
 * will have N1 and N2 at corresponding locations.  For example,   *
 * list L1 might go [N1, ...], while l2 goes [N2, ...], where N1   *
 * and N2 occupy the first positions.				   *
 *								   *
 * The functions here construct l1 and l2 at an OPEN_E node, and   *
 * manage OPEN_E nodes.						   *
 *******************************************************************/

#include "../misc/misc.h"
#include "../utils/lists.h"
#include "../tables/tables.h"
#include "../parser/parser.h"
#include "../alloc/allocate.h"
#include "../classes/classes.h"
#include "../unify/unify.h"
#include "../ids/open.h"
#include "../standard/stdids.h"
#include "../exprs/expr.h"
#include "../generate/generate.h"
#include <string.h>
#ifdef DEBUG
# include "../debug/debug.h"
#endif


/****************************************************************
 *			PUBLIC VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			seen_open_expr				*
 ****************************************************************
 * seen_open_expr is set true when the id processor encounters  *
 * an open expression.  This lets us skip processing of open	*
 * exprs in an expression that has no open exprs.		*
 ****************************************************************/

Boolean seen_open_expr;

/****************************************************************
 *			ALL_IDS					*
 ****************************************************************
 * Return a list representing all ids in a list of ids in an	*
 * expression.  Such a list is used to represent the ids	*
 * that are bound by an expression that clearly always fails,	*
 * so it can be said that if you get through it, any id at all	*
 * is bound.  The returned list begins with an identifier named *
 * "^A" (control-A).						*
 ****************************************************************/

PRIVATE EXPR_LIST* all_ids(void)
{
  char name[2];
  name[0] = 1;
  name[1] = 0;
  return expr_cons(id_expr(id_tb0(name), 0), NIL);
}


/****************************************************************
 *			PAIR_IDS				*
 ****************************************************************
 * Return a list of the ids defined in e->E1 or e->E2.		*
 * Parameter pattern1 is the pattern parameter for finding ids	*
 * in e->E1, and pattern2 is the pattern parameter for finding	*
 * bindings in e->E2.						*
 ****************************************************************/

PRIVATE EXPR_LIST* pair_ids(EXPR *e, Boolean pattern1, Boolean pattern2)
{ 
  EXPR_LIST *e1list, *e2list, *result;

  bump_list(e1list = get_id_list(e->E1, pattern1));
  bump_list(e2list = get_id_list(e->E2, pattern2));

  /*-------------------------------------------------------------*
   * If the second list represents all ids, then return all ids. *
   *-------------------------------------------------------------*/

  if(e2list != NIL) {
    EXPR *h = e2list->head.expr;
    if(EKIND(h) == IDENTIFIER_E && h->STR[0] == 1) {
      bump_list(result = e2list);
      goto out;
    }
  }

  /*---------------------------------------------------------------*
   * Otherwise, just append all e1list and e2list.  Note that if   *
   * e1list represents all ids, then the result will also represent*
   * all ids, since its head will be an identifier that starts	   *
   * with ^A.							   *
   *---------------------------------------------------------------*/

  bump_list(result = append(e1list, e2list));

 out:

  /*--------------------------------------*
   * Clean up and return the result list. *
   *--------------------------------------*/

  drop_list(e1list);
  drop_list(e2list);
  if(result != NIL) result->ref_cnt--;
  return result;
}


/****************************************************************
 *			GET_ID_LIST				*
 ****************************************************************
 * Get the list of identifiers defined in e.  			*
 * Pattern is true if e is part of a pattern; in that case,	*
 * pair expressions (a,b) should be treated as open.		*
 ****************************************************************/

EXPR_LIST* get_id_list(EXPR *e, Boolean pattern)
{
 tail_recur:
  if(e == NULL_E) return NIL;

  switch(EKIND(e)) {
    case IDENTIFIER_E:
    case LOCAL_ID_E:
    case GLOBAL_ID_E:
    case OVERLOAD_E:
    case SPECIAL_E:
    case UNKNOWN_ID_E:
    case STREAM_E:
    case AWAIT_E:
    case LAZY_LIST_E:
    case IF_E:
    case TRY_E:
    case FUNCTION_E:
    case PAT_FUN_E:
    case FOR_E:
    case MANUAL_E:
    case BAD_E:
      return NIL;

    case PAIR_E:
    case LAZY_BOOL_E:

      /*--------------------------------------------------------*
       * Pairs and lazy boolean expressions normally hide their *
       * internal bindings.  In patterns, however, they do not. *
       *--------------------------------------------------------*/

      if(pattern) return pair_ids(e, TRUE, TRUE);
      else return NIL;

    case LOOP_E:

      /*--------------------------------------------------------*
       * A closed loop hides its bindings.  An open loop 	*
       * (e->OPEN_LOOP == 1) does not hide its bindings.	*
       *--------------------------------------------------------*/

      if(e->OPEN_LOOP == 0) return NIL;
      return pair_ids(e, pattern, pattern);

    case LET_E:
    case DEFINE_E:

      {/*-------------------------------------------------------*
        * Define expressions and closed let expressions do not 	*
	* export any ids from their bodies.			*
        *-------------------------------------------------------*/

       LIST* rest = (EKIND(e) == DEFINE_E || e->SCOPE == 0) 
	              ? NIL : get_id_list(e->E2, pattern);

       /*------------------------------------------------------*
        * Define and let expressions do define one identifier, *
	* namely e->E1.					       *
        *------------------------------------------------------*/

       EXPR* a = skip_sames(e->E1);
       return expr_cons(a, rest);
      }

    case SINGLE_E:

      /*------------------------------------------------------------*
       * A closed SINGLE_E expression does not export its bindings. *
       *------------------------------------------------------------*/

      if(e->SCOPE == 0) return NIL;
      return get_id_list(e->E1, pattern);

    case PAT_VAR_E:

      /*---------------------------------------------------------*
       * A pattern variable should be thought of as a binding of *
       * its associated identifier.  An anonymous variable, 	 *
       * however, should be ignored.				 *
       *---------------------------------------------------------*/

      {LIST* result = NIL;
       EXPR* a = skip_sames(e->E1);
       if(a != NULL) {
	 /*-----------------------------------------*
	  * This pattern variable is not anonymous. *
	  *-----------------------------------------*/
         result = expr_cons(a, result);
       }
       return result;
      }

    case APPLY_E:
      {EXPR*         fun  = skip_sames(e->E1);
       EXPR_TAG_TYPE kind = EKIND(fun);

      /*----------------------------------------------------------------*
       * If this expression is an application of a function that 	*
       * is known always to fail, then return a list that represents 	*
       * all ids. 							*
       *----------------------------------------------------------------*/

       if((kind == IDENTIFIER_E || kind == GLOBAL_ID_E || kind == OVERLOAD_E)
	  && (strcmp(fun->STR, std_id[FAIL_ID]) == 0 
	      || strcmp(fun->STR, std_id[FAILPROC_ID]) == 0)) {
	return all_ids(); 
       }

       /*----------------------------------------------------------*
        * If this is a closed application, it exports no bindings. *
        *----------------------------------------------------------*/

       else if(e->SCOPE == 1) return NIL;

       /*-------------------------------------------------------*
        * An open application exports the bindings of its parts *
        *-------------------------------------------------------*/

       else return pair_ids(e, pattern, pattern);
      }

    case TEST_E:

      /*--------------------------------------------------------*
       * Expression {false} always fails, so it binds all ids.	*
       * Otherwise, {a else b} binds the ids bound in a or b.	*
       *--------------------------------------------------------*/

      if(is_false_expr(e->E1)) return all_ids();
      else return pair_ids(e, pattern, pattern);

    case SEQUENCE_E:

      /*--------------------------------------------------------------*
       * A SEQUENCE_E node can be open or closed on its E1 field, but *
       * always exports the bindings done by its E2 field.	      *
       *--------------------------------------------------------------*/

      if(e->SCOPE == 0) {
	/*---------------*
	 * Closed on e1. *
	 *---------------*/
	return get_id_list(e->E2, pattern);
      }
      else return pair_ids(e, pattern, pattern);

    case TRAP_E:

      /*----------------------------------------------------------------*
       * A closed TRAP_E expression exports no bindings.  Otherwise, it *
       * returns the bindings done by its parts, just as WHERE_E	*
       * expressions do.					*
       *----------------------------------------------------------------*/

      if(e->SCOPE == 0) return NIL;
      /* No break - fall through to next case. */

    case WHERE_E:
      return pair_ids(e, pattern, pattern);

    case MATCH_E:

      /*--------------------------------------------------------*
       * A close match exports only those bindings done in the  *
       * pattern.  An open match additionally exports bindings	*
       * done in the target expression.				*
       *--------------------------------------------------------*/

      if(e->SCOPE == 0) return get_id_list(e->E1, TRUE);
      else              return pair_ids(e, TRUE, FALSE);

    case SAME_E:

      /*---------------------------------------------------------*
       * If this expression is marked closed, then it exports no *
       * bindings.  Otherwise, treat like the next case.	 *
       *---------------------------------------------------------*/

      if(e->SAME_CLOSE) return NIL;

      /* else fall through to next case. */

    case EXECUTE_E:
      /*-----------------------------------------------*
       * Simulate  return get_id_list(e->E1, pattern). *
       *-----------------------------------------------*/

      e = e->E1;
      goto tail_recur;

    case CONST_E:
      /*----------------------------------------------------------------*
       * A constant normally exports no bindings. If the constant if 	*
       * the special FAIL_CONST, however, then it always fails, so	*
       * it binds all identifiers.					*
       *----------------------------------------------------------------*/

      if(e->SCOPE != FAIL_CONST) return NIL;
      /* No break - fall through to next case. */

    case RECUR_E:

       /*---------------------------------------------------------------*
        * A continue-phrase at a loop should bind all ids, since the	*
	* until-cases are the ones that determine which ids are bound.	*
        *---------------------------------------------------------------*/

       return all_ids();

    case OPEN_E:
      return e->EL1;

    default:
      die(128, (char *) tolong(EKIND(e)));
  }

  return NIL; /* To suppress warning */
}
      

/******************************************************************
 *			GET_OPEN_LISTS				  *
 ******************************************************************
 * get_open_lists(e, la, lb) returns the lists la and lb of 	  *
 * identifiers that are defined in both branches of expression e, *
 * which must be a TRY_E, IF_E or STREAM_E expression.		  *
 * The names in la and lb are the same, but the expr nodes are	  *
 * different.  e should be a stream, if or try expression.  la 	  *
 * and lb are ref-counted.  					  *
 ******************************************************************/

PRIVATE void get_open_lists(EXPR *e, EXPR_LIST **la, EXPR_LIST **lb)
{
  EXPR_TAG_TYPE kind;
  EXPR_LIST *a, *b, *e1_list, *e2_list;

  kind = EKIND(e);
  bump_list(e1_list = get_id_list(e->E1, FALSE));
  bump_list(e2_list = get_id_list(e->E2, FALSE));

  /*------------------------------------------------------------*
   * List a consist of ids bound on one branch, and list b	*
   * consists of those bound on the other branch.		*
   *------------------------------------------------------------*/

  /*---------------------------------------------------------------*
   * At Try e1 then e2 else e3 %Try, the then branch gets bindings *
   * done in e1 or e2, and the else branch includes only those	   *
   * bindings done in e3.					   *
   *---------------------------------------------------------------*/

  if(kind == TRY_E) {
    bump_list(a = append(e1_list, e2_list));
    bump_list(b = get_id_list(e->E3, FALSE));
  }

  /*--------------------------------------------------------------------*
   * At If e1 then e2 else e3 %If, the then branch gets bindings  	*
   * done in e1 or e2, and the else branch gets bindings done in	*
   * e1 or e3.							 	*
   *--------------------------------------------------------------------*/

  else if(kind == IF_E) {
    bump_list(a = append(e1_list, e2_list));
    bump_list(b = append(e1_list, get_id_list(e->E3, FALSE)));
  }

  /*--------------------------------------------------------------------*
   * At Stream e1 then e2 %Stream, or a similar mix expression, the	*
   * first branch is just e1, and the second branch is just e2.		*
   *--------------------------------------------------------------------*/

  else if(kind == STREAM_E) {
    bump_list(a = e1_list);
    bump_list(b = e2_list);
  }
  else {
    die(129, (char *) ((LONG) kind));
  }

# ifdef DEBUG
    if(trace_locals) {
      trace_t(257);
      print_str_list_nl(a);
      print_str_list_nl(b);
    }
# endif

  /*--------------------------------------------------------------------*
   * Sort the lists.  In addition to making it easy to intersect the	*
   * lists, this has the effect of moving a node that indicates all ids *
   * (having name ^A) to the front of the list, so that the list is 	*
   * recognized as representing all ids. 				*
   *--------------------------------------------------------------------*/

  SET_LIST(a, sort_id_list(&a, list_length(a)));
  SET_LIST(b, sort_id_list(&b, list_length(b)));

# ifdef DEBUG
    if(trace_locals) {
      trace_t(258);
      print_str_list_nl(a);
      print_str_list_nl(b);
    }
# endif

  /*--------------------------------------------------------------------*
   * Now intersect the lists, putting the results (using different	*
   * identifier nodes) in la and lb. 					*
   *--------------------------------------------------------------------*/

  intersect_id_lists(a, b, la, lb);

  /*----------------------*
   * Clean up and return. *
   *----------------------*/

  drop_list(a);
  drop_list(b);
  drop_list(e1_list);
  drop_list(e2_list);
}


/************************************************************************
 *                     MARK_ALTS_USED                                 	*
 ************************************************************************
 * Expression e should have kind OPEN_E.				*
 *									*
 * Mark identifiers in the EL2 field as used.				*
 ************************************************************************/

void mark_alts_used(EXPR *e)
{
  EXPR_LIST *p;
  EXPR *id;

  for(p = e->EL2; p != NIL; p = p->tail) {
    if(LKIND(p) == EXPR_L && EKIND(id = p->head.expr) == LOCAL_ID_E) {
      id->used_id = 1;  /* id is set in the if-statement above. */
    }
  }
}


/****************************************************************
 *			PROCESS_NEW_OPEN			*
 ****************************************************************
 * A new OPEN_E expression e has just been created during 	*
 * expansion.  Process it, setting its open lists and fixing up *
 * its identifiers.						*
 ****************************************************************/

void process_new_open(EXPR *e)
{
  set_open_lists(e);
  mark_alts_used(e);
}


/****************************************************************
 *			SET_OPEN_LISTS				*
 ****************************************************************
 * Set the open lists EL1 and EL2 at OPEN_E nodes in e.		*
 ****************************************************************/

void set_open_lists(EXPR *e)
{

  /*-------------------------------------------------------------*
   * This for-loop is for tail-recursion.  A break in the switch *
   * is a tail recursive call.					 *
   *-------------------------------------------------------------*/

  for(;;) {
    if(e == NULL) return;
    switch(EKIND(e)) {
      case OPEN_E:
	{EXPR_LIST *la, *lb;

	 /*-------------------------------------------------------------*
	  * Set the open lists in the body if, try or stream expr	*
   	  * recursively first.						*
	  *-------------------------------------------------------------*/

	 set_open_lists(e->E1);

	 /*---------------------------------------------------------------*
	  * Get the two open lists, and install them in this OPEN_E node. *
	  *---------------------------------------------------------------*/

	 get_open_lists(skip_sames(e->E1), &la, &lb);
	 e->EL1 = la;   /* Ref from la */
         e->EL2 = lb;   /* Ref from lb */

#        ifdef DEBUG
           if(trace_locals) {
             trace_t(259);
             print_str_list_nl(la);
             print_str_list_nl(lb);
           }
#        endif
	 return;
        }

      /*-------------------------------------------------------------*
       * Handle other kinds of expressions by calling set_open_lists *
       * on their parts.					     *
       *-------------------------------------------------------------*/

      case IF_E:
      case TRY_E:
      case FOR_E:
      case PAT_RULE_E: 
        set_open_lists(e->E3);
        /* No break - fall through to next case. */

      case LET_E:
      case DEFINE_E:
      case APPLY_E:
      case SEQUENCE_E:
      case TEST_E:
      case STREAM_E:
      case AWAIT_E:
      case PAIR_E:
      case FUNCTION_E:
      case LOOP_E:
      case MATCH_E:
      case LAZY_BOOL_E:
      case WHERE_E:
      case TRAP_E:
      case PAT_DCL_E:
      case EXPAND_E:
        set_open_lists(e->E2);
	/* No break - fall through to next case. */

      case SINGLE_E:
      case RECUR_E:
      case SAME_E:
      case EXECUTE_E:
      case MANUAL_E:
      case LAZY_LIST_E:

	/*----------------------*
	 * Tail recur on e->E1. *
	 *----------------------*/

        e = e->E1;
        break;

      default:
        return;
    }
  }
}


/****************************************************************
 *			OPEN_IF					*
 ****************************************************************
 * Return an open version of if- or try-expression e.		*
 ****************************************************************/

EXPR* open_if(EXPR *e)
{
  EXPR *a, *sse;

  sse = skip_sames(e);
  if(sse == bad_expr) return bad_expr;
  seen_open_expr = TRUE;

  a = new_expr1(OPEN_E, e, e->LINE_NUM);
  return a;
}


/****************************************************************
 *			OPEN_CHOOSE				*
 ****************************************************************
 * Return an open version of a choose expression e.		*
 * This function is destructive -- it replaces subexpressions	*
 * of e by open versions.					*
 ****************************************************************/

EXPR* open_choose(EXPR *e)
{
  EXPR_TAG_TYPE kind;

  if(e == bad_expr || e == NULL) return e;

  /*------------------------------------------------------------*
   * If e is tagged as a choose expression, then do not open it.*
   * It must be a choose expression that is embedded within	*
   * another one that is being opened.	But if e is not an	*
   * embedded choose, then skip over any SAME_E nodes.		*
   *------------------------------------------------------------*/

  {EXPR* new_e = e;
   while(EKIND(new_e) == SAME_E) {
     if(new_e->SAME_MODE == 6) return e;
     new_e = new_e->E1;
   }
   e = new_e;
  }

  kind = EKIND(e);

  /*-------------------------------------------------------------*
   * An APPLY_E node is a do phrase.  Just open up the rest of   *
   * the cases, and return this expression.			 *
   *-------------------------------------------------------------*/

  if(kind == APPLY_E) {
    SET_EXPR(e->E2, open_choose(e->E2));
    return e;
  }

  /*-------------------------------------------------------------*
   * Do the top if or try expression, and also do the else-part, *
   * making if's and try's there open.  The mark field is 	 *
   *   0 for an if or try that is not a case in a choose expression *
   *   1 for a case of a choose expression			 *
   *   2 for the top level of a subcases part.			 *
   *-------------------------------------------------------------*/

  else if((kind == IF_E || kind == TRY_E) && e->mark != 0) {

    /*-------------------------------------------------------------*
     * Make the else part open as well.  This handles other cases. *
     *-------------------------------------------------------------*/

    SET_EXPR(e->E3, open_choose(e->E3));

    /*----------------------------------------------------------*
     * If the then-part is a subcases, then it must also be	*
     * made open.						*
     *----------------------------------------------------------*/

    if(e->E2->mark == 2) SET_EXPR(e->E2, open_choose(e->E2));

    /*-------------------------------------------*
     * Now handle if or try expression e itself. *
     *-------------------------------------------*/

    return open_if(e);
  }

  /*-----------------------------------------------------*
   * A case of a choose-all-expression can be a stream,  *
   * For a stream, handle the top level and also any     *
   * embedded parts that belong to the same open stream. *
   * The mark field is as for an if or try.		 *
   *-----------------------------------------------------*/

  else if(kind == STREAM_E && e->mark != 0) {

    /*-----------------------------------------*
     * Handle opening up any embedded streams. *
     *-----------------------------------------*/

    SET_EXPR(e->E2, open_choose(e->E2));

    /*----------------------------------------*
     * There can be subcases, so handle them. *
     *----------------------------------------*/

    if(e->E1->mark == 2) SET_EXPR(e->E1, open_choose(e->E1));

    /*---------------------------------*
     * Now handle the top level stream *
     *---------------------------------*/

    return open_if(e);
  }

  else return e;
}


/****************************************************************
 *			OPEN_LOOP				*
 ****************************************************************
 * Return an open version of loop expression e.			*
 ****************************************************************/

EXPR* open_loop(EXPR *e)
{

  /*--------------------------------------------*
   * The body is a choose expression.  Open it. *
   *--------------------------------------------*/

  SET_EXPR(e->E2, open_choose(e->E2));

  /*-------------------------------------------------------*
   * The match expression needs to be opened, and the loop *
   * marked as open. 					   *
   *-------------------------------------------------------*/

  e->E1->SCOPE = 1;   /* Open match */
  e->OPEN_LOOP = 1;   /* Open loop  */
  return e;
}


/****************************************************************
 *			OPEN_STREAM				*
 ****************************************************************
 * Return an open version of a stream or mix expression e.	*
 ****************************************************************/

EXPR* open_stream(EXPR *e)
{
  EXPR_TAG_TYPE kind;

  e = skip_sames(e);
  if(e == bad_expr) return bad_expr;

  kind = EKIND(e);
  if(kind == STREAM_E) {

    /*----------------------------------------------------------*
     * If the second part of the stream was implicitly build,	*
     * it is open too.						*
     *----------------------------------------------------------*/

    if(e->E2->mark == 3) SET_EXPR(e->E2, open_stream(e->E2));

    /*---------------------------*
     * Open the top-level stream *
     *---------------------------*/

    return open_if(e);
  }

  /*---------------------------------------------------------*
   * If this is not a stream, leave it alone. It must be the *
   * non-stream at the end of the chain of STREAM_E nodes.   *
   *---------------------------------------------------------*/

  else return e;
}
