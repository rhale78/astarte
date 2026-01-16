/*****************************************************************
 * File:    ids/lateids.c
 * Purpose: Functions for handling identifiers late in the
 *          compilation process.
 * Author:  Karl Abrahamson
 *****************************************************************/

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
 * This file contains functions to handle identifiers after type	*
 * inference, and just before code generation.  Here, we recognize	*
 * which scope each identifier occurs in.				*
 ************************************************************************/

#include <string.h>
#include "../misc/misc.h"
#include "../classes/classes.h"
#include "../tables/tables.h"
#include "../exprs/expr.h"
#include "../utils/hash.h"
#include "../utils/strops.h"
#include "../utils/lists.h"
#include "../alloc/allocate.h"
#include "../error/error.h"
#include "../parser/parser.h"
#include "../ids/open.h"
#include "../ids/ids.h"
#include "../dcls/dcls.h"
#include "../patmatch/patmatch.h"
#include "../generate/prim.h"
#include "../standard/stdids.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif


/************************************************************************
 *                     DO_PROCESS_SCOPES                              	*
 ************************************************************************
 * Initialize for the declaration, and do process_scopes.  See 		*
 * process_scopes, below.						*
 ************************************************************************/

void do_process_scopes(EXPR *e)
{
  init_dcl_tm(outer_scope_num);
  process_scopes(e, 1);
}

/************************************************************************
 *                     PROCESS_SCOPES                              	*
 ************************************************************************
 * Move into a new scope.  Set *hold_scope to the current scope.	*
 ************************************************************************/

PRIVATE void push_scope(int *hold_scope)
{

  /*----------------------------------------------------------------*
   * Put the old scope level into *hold_scope, and push a new scope *
   * into the scopes array.					    *
   *----------------------------------------------------------------*/

  scopes[next_scope].pred     = current_scope;
  scopes[next_scope].nonempty = 0;
  *hold_scope = current_scope;

  /*--------------------------------------------*
   * Reallocate the scopes array if necesssary. *
   *--------------------------------------------*/

  if(next_scope >= scopes_size) {
    int new_scopes_size;
    new_scopes_size = 2*scopes_size;
    scopes = (struct scopes_struct *) 
              reallocate((char *) scopes, 
			 scopes_size*sizeof(struct scopes_struct),
			 new_scopes_size*sizeof(struct scopes_struct), TRUE);
    scopes_size = new_scopes_size;
  }

  current_scope = next_scope++;
}


/************************************************************************
 *                     PROCESS_SCOPES                              	*
 ************************************************************************
 * Sets the SCOPE field of local ids and expressions (FUNCTION_E,	*
 * LAZY_LIST_E, DEFINE_E, AWAIT_E) in e that need their own scopes.	*
 * A scope of -1 in a LAZY_LIST_E, DEFINE_E or AWAIT_E node indicates	*
 * that no local scope is needed.  Context is 1 for a declaration and  	*
 * 0 for an expression.							*
 *									*
 * This function also sets up the scopes array, described in 		*
 * tables/loctbl.c.							*
 ************************************************************************/

void process_scopes(EXPR *e, int context)
{
  EXPR_TAG_TYPE kind;
  EXPR *e1, *e2;

  if(e == NULL_E) return;
  kind = EKIND(e);
  e1 = e->E1;	/* Shared for below */
  e2 = e->E2;

# ifdef DEBUG
    if(trace_ids) {
      trace_t(255, e, current_scope, expr_kind_name[kind]);
      if(kind == IDENTIFIER_E) {
	trace_t(251, e->STR, e->STR);
      }
      tracenl();
    }
# endif

  switch(kind) {
    case SPECIAL_E:
    case GLOBAL_ID_E:
    case OVERLOAD_E:
    case PAT_FUN_E:
    case CONST_E:
    case LOCAL_ID_E:
    case PAT_VAR_E:
    case MANUAL_E:
    case PAT_DCL_E:
    case EXPAND_E:
    case BAD_E:
      break;

    case FOR_E:
      scopes[current_scope].nonempty = 1;
      /* No break - fall through to next case. */

    case IF_E:
    case TRY_E:
      process_scopes(e1, 0);
      process_scopes(e2, 0);
      process_scopes(e->E3, 0);
      break;

    case PAIR_E:
    case LAZY_BOOL_E:
    case WHERE_E:
    case TEST_E:
    case SEQUENCE_E:
    case APPLY_E:
    case STREAM_E:
    case TRAP_E:
      process_scopes(e1, 0);
      process_scopes(e2, 0);
      break;

    case SINGLE_E:
    case SAME_E:
    case RECUR_E:
    case EXECUTE_E:
    case OPEN_E: 
      process_scopes(e1, 0);
      break;

    case LOOP_E:
      if(e1 != NULL) {
        process_scopes(e1->E2, 0);
      }
      process_scopes(e2, 0);
      break;

    case LAZY_LIST_E:

      /*-----------------------------------------*
       * A LAZY_LIST_E node needs its own scope. *
       *-----------------------------------------*/

      {int hold_scope;
       push_scope(&hold_scope);
       process_scopes(e1, 0);
       e->SCOPE = current_scope;
       current_scope = hold_scope;
       break;
      }

    case AWAIT_E:

      /*-----------------------------------------------------------*
       * A normal AWAIT_E node, representing expression await e1   *
       * then e2 %await, needs its own scope, for executing e2.    *
       * The extra field is 0 in a normal await.  Some await       *
       * exprs are compiled twice, once for lazy evaluation and    *
       * once for eager evaluation.  The eager part does not use   *
       * its own scope -- it shares the embedding scope.  So if    *
       * the current expression is to be double-compiled,  	   *
       * then if the separate scope has any identifiers      	   *
       * defined in it, the current scope will also have some      *
       * identifiers defined in it, at the eager parts.		   *
       *-----------------------------------------------------------*/

      {int hold_scope, body_scope;

       process_scopes(e1, 0);
       if(e->extra == 0) push_scope(&hold_scope);
       process_scopes(e2, 0);
       if(e->extra == 0) {
         body_scope = e->SCOPE = current_scope;
         if(e->DOUBLE_COMPILE == 1 && scopes[body_scope].nonempty != 0) {
	   scopes[current_scope].nonempty = 1;
         }
         current_scope = hold_scope;
       }
       break;
      }

    case FUNCTION_E:

      /*------------------------------------------------------------*
       * A FUNCTION_E node needs its own scope, provided it is not  *
       * flagged to share the scope of the embedding function.  The *
       * SCOPE field of this node tells whether it is to share its  *
       * embedding scope.					    *
       *------------------------------------------------------------*/

      {int hold_scope;

       if(e->OFFSET == 0) push_scope(&hold_scope);
       e->SCOPE = current_scope;
       scopes[current_scope].nonempty = 1;
       process_scopes(e1, context);
       process_scopes(e2, context);
       if(e->OFFSET == 0) current_scope = hold_scope;
       break;
      }

    case LET_E:

      /*--------------------------------------------------*
       * A let-declaration needs a scope in which to run. *
       * A let-expression does not.  If this is a let	  *
       * expression, the current scope should be marked   *
       * nonempty, since we are defining an id.		  *
       *--------------------------------------------------*/

      {int hold_scope;

       if(context) push_scope(&hold_scope);
       else scopes[current_scope].nonempty = 1;
       process_scopes(e2, 0);
       if(context) current_scope = hold_scope;
       e1 = skip_sames(e->E1);

       /*--------------------------------------------------*
        * Set the SCOPE field of the id being defined.  At *
        * a relet, that is not necessary, since the id is  *
        * actually defined elsewhere.  But check that the  *
        * current scope is the same as the scope of the    *
        * id being relet, since relets must be to ids in   *
	* the most local scope only.			   *
        *--------------------------------------------------*/

       if(e->LET_FORM == 0) { /* A let */
         if(EKIND(e1) == LOCAL_ID_E) e1->SCOPE = current_scope;
       }
       else { /* A relet */
	 if(EKIND(e1) != LOCAL_ID_E || e1->SCOPE != current_scope) {
	   semantic_error1(NONLOCAL_RELET_ERR, display_name(e1->STR), 
			   e->LINE_NUM);
	 }
       }
       break;
     }
      
    case DEFINE_E:

      /*--------------------------------------------------------------*
       * Define expressions are handled similarly to let expressions. *
       * The main difference is that the body is lazy, and needs      *
       * its own scope.						      *
       *--------------------------------------------------------------*/

      {EXPR *id;
       int hold_scope;

       id = skip_sames(e1);
       if(!context) {
	 id->SCOPE = current_scope;
         scopes[current_scope].nonempty = 1;
       }
       push_scope(&hold_scope);
       e->SCOPE = current_scope;
       process_scopes(e2, 0);
       current_scope = hold_scope;
       break;
      }

    default: 
      die(166, (char *) ((LONG) kind));
      break;
  }

# ifdef DEBUG
    if(trace_ids) trace_t(256, e);
# endif
}


/****************************************************************
 *			MARK_FINAL_IDS				*
 ****************************************************************
 * Mark the local ids in e that are last uses, by setting the	*
 * extra field of the SAME_E node above them.  in_used_ids is a *
 * list of the ids that are used after e, and in_sig is true if *
 * there are any calls of non-primitive functions after e.  	*
 * Returns a list of the ids that are used in e, and sets 	*
 * out_sig true if there are any calls of non-primitive		*
 * functions in e.  If no_final is true, then do not mark any	*
 * of the ids as final, but do the other computations.		*
 *								*
 * The implentation assigns a number to each id, storing it in  *
 * the ETEMP field.  It then scans the expression, keeping	*
 * track of used ids.						*
 ****************************************************************/

PRIVATE int temp_id_for_final;

/*------------------------------------------------------------*
 * set_temp_field sets e->ETEMP to the next available number. *
 *------------------------------------------------------------*/

PRIVATE void set_temp_field(EXPR *e)
{
  EXPR* sse = skip_sames(e);
  if(EKIND(sse) == LOCAL_ID_E && sse->ETEMP == 0) {

#   ifdef DEBUG
      if(trace_gen > 2) {
	trace_t(157, sse, temp_id_for_final);
      }
#   endif

    sse->ETEMP = temp_id_for_final++;
  }
}

/*----------------------------------------------------------*
 * Ids that are the same due to open ifs, etc. must get the *
 * same temp numbers assigned to them.  set_open_temps does *
 * that.						    *
 *----------------------------------------------------------*/

PRIVATE void set_open_temps(EXPR_LIST *l1, EXPR_LIST *l2)
{
  EXPR_LIST *p,*q;
  UBYTE *ptemp, *qtemp;

  for(p = l1, q = l2; p != NIL && q != NIL; p = p->tail, q = q->tail) {
    ptemp = &(p->head.expr->ETEMP);
    qtemp = &(q->head.expr->ETEMP);
    if(*ptemp != 0) {
      if(*qtemp != 0) {
	if(*ptemp != *qtemp) die(29);
      }
      else *qtemp = *ptemp;
    }
    else if(*qtemp != 0) *ptemp = *qtemp;
    else *ptemp = *qtemp = temp_id_for_final++;

#   ifdef DEBUG
      if(trace_gen > 2) {
	trace_t(158, p->head.expr, *ptemp, q->head.expr, *qtemp);
      }
#   endif
  }
}

/*--------------------------------------------------*/

PRIVATE Boolean set_all_temps_help(EXPR **e) 
{
  EXPR_TAG_TYPE kind;

  kind = EKIND(*e);
  if(kind == LET_E || kind == DEFINE_E) set_temp_field((*e)->E1);
  else if(kind == OPEN_E) set_open_temps((*e)->EL1, (*e)->EL2);
  return 0;
}

/*--------------------------------------------------*/

PRIVATE void set_all_temps(EXPR *e) 
{
  scan_expr(&e, set_all_temps_help, FALSE);
}


/*--------------------------------------------------------------*
 * mark_final_ids1 is similar to mark_final_ids, but requires   *
 * initialization done by mark_final_ids.			*
 *--------------------------------------------------------------*/

PRIVATE INT_LIST *mark_final_ids1(EXPR *e, INT_LIST *in_used_ids, 
				  Boolean *out_sig, Boolean in_sig, 
				  Boolean no_final)
{
  EXPR *e1, *e2;
  INT_LIST *result;

# ifdef DEBUG
    if(trace_gen > 2) {
      trace_t(159, e, in_sig, no_final);
      print_str_list_nl(in_used_ids);
    }
# endif

  result   = NIL;    /* default */
  *out_sig = FALSE;  /* default */

 tail_recur:
  if(e == NULL) return NIL;
  e1 = e->E1;
  e2 = e->E2;
  switch(EKIND(e)) {
    default:
      goto out;  /* Return NIL */

    case OPEN_E:
    case SINGLE_E:
    case RECUR_E:
    case EXECUTE_E:
    case MANUAL_E:
      e = e1;
      goto tail_recur;

    case LAZY_LIST_E:
      bump_list(result = mark_final_ids1(e1, NIL, out_sig, FALSE, FALSE));
      *out_sig = FALSE;
      goto out;

    case FUNCTION_E:
      bump_list(result = mark_final_ids1(e2, NIL, out_sig, FALSE, FALSE));
      *out_sig = FALSE;
      goto out;
    
    case AWAIT_E:
      {INT_LIST *in_used1, *out_used1, *out_used2;

       bump_list(out_used2 = mark_final_ids1(e2, NIL, out_sig, FALSE, FALSE));
       bump_list(in_used1 = append_without_dups(out_used2, in_used_ids));
       bump_list(out_used1 = mark_final_ids1(e1, in_used1, out_sig, in_sig,
					     no_final));
       bump_list(result = append_without_dups(out_used1, out_used2));
       drop_list(in_used1);
       drop_list(out_used1);
       drop_list(out_used2);
       goto out;
      }

    case LET_E:
      bump_list(result = mark_final_ids1(e2, in_used_ids, out_sig, in_sig, 
					no_final));
      goto out;

    case DEFINE_E:
      bump_list(result = mark_final_ids1(e2, NIL, out_sig, FALSE, FALSE));
      *out_sig = FALSE;
      goto out;

    case APPLY_E:
      if(is_hermit_type(e1->ty)) goto two_exprs;

      {EXPR *temp, *fun;
       INT_LIST *in_used1, *out_used1, *out_used2;
       Boolean out_sig1, out_sig2, in_sig2, nonprim, irregular_prim;
       int prim, instr;
       PART *part;
       INT_LIST *sel_list;

       /*-------------------------------------------------------*
        * An id as a function will be fetched after its 	*
	* argument is evaluated. 				*
        *-------------------------------------------------------*/

       fun = skip_sames(e1);
       prim = get_prim_g(fun, &instr, &part, &irregular_prim, &sel_list);
       nonprim = (prim == 0);
       in_sig2 = nonprim | in_sig;
       if(EKIND(fun) == LOCAL_ID_E) {
	 temp = e1;
	 e1 = e2;
	 e2 = temp;
       }

       bump_list(out_used2 = mark_final_ids1(e2, in_used_ids, &out_sig2, 
					    in_sig2, no_final));
       bump_list(in_used1 = append_without_dups(out_used2, in_used_ids));
       bump_list(out_used1 = mark_final_ids1(e1, in_used1, &out_sig1,
					    in_sig2 | out_sig2, no_final));
       bump_list(result = append_without_dups(out_used1, out_used2));
       *out_sig = nonprim | out_sig1 | out_sig2;
       drop_list(in_used1);
       drop_list(out_used1);
       drop_list(out_used2);
       goto out;
      }

    case STREAM_E:
      if(e->STREAM_MODE == MIX_ATT) { /* A mix expr */
        INT_LIST *in_used, *out_used1, *out_used2;
	Boolean out_sig1, out_sig2;

        bump_list(out_used1 = 
		  mark_final_ids1(e1, NIL, &out_sig1, FALSE, TRUE));
        bump_list(out_used2 = 
		  mark_final_ids1(e2, NIL, &out_sig2, FALSE, TRUE));
	bump_list(in_used = append_without_dups(out_used1, in_used_ids));
	SET_LIST(in_used, append_without_dups(out_used2, in_used));
	SET_LIST(out_used1, mark_final_ids1(e1, in_used, &out_sig1, in_sig, 
					   no_final));
        SET_LIST(out_used2, mark_final_ids1(e2, in_used, &out_sig2, in_sig,
					   no_final));
	bump_list(result = append_without_dups(out_used1, out_used2));
	*out_sig = out_sig1 | out_sig2;
	drop_list(out_used1);
	drop_list(out_used2);
	drop_list(in_used);
	goto out;
      }
      /*--------------------------------------*
       * Else handle like sequence: no break. *
       *--------------------------------------*/
    case SEQUENCE_E:
    case TEST_E:
    case PAIR_E:
    case MATCH_E:
    case LAZY_BOOL_E:
    case WHERE_E:
    case TRAP_E:
    two_exprs:
      {INT_LIST *in_used1, *out_used1, *out_used2;
       Boolean out_sig1, out_sig2;

       bump_list(out_used2 = mark_final_ids1(e2, in_used_ids, &out_sig2, 
					    in_sig, no_final));
       bump_list(in_used1 = append_without_dups(out_used2, in_used_ids));
       bump_list(out_used1 = mark_final_ids1(e1, in_used1, &out_sig1,
					    in_sig | out_sig2, no_final));
       bump_list(result = append_without_dups(out_used1, out_used2));
       *out_sig = out_sig1 | out_sig2;
       drop_list(in_used1);
       drop_list(out_used1);
       drop_list(out_used2);
       goto out;
      }

    case IF_E:
    case TRY_E:
      {INT_LIST *in_used1, *in_used2, *out_used1, *out_used2, *out_used3;
       INT_LIST *out_used23;
       Boolean out_sig1, out_sig2, out_sig3;

       bump_list(out_used3 = mark_final_ids1(e->E3, in_used_ids, &out_sig3,
					    in_sig, no_final));
       bump_list(in_used2 = (EKIND(e) == IF_E) 
		              ? in_used_ids
		              : append_without_dups(out_used3, in_used_ids));
       bump_list(out_used2 = mark_final_ids1(e2, in_used2, &out_sig2,
					    in_sig, no_final));
       bump_list(out_used23 = append_without_dups(out_used2, out_used3));
       bump_list(in_used1 = append_without_dups(out_used23, in_used_ids));
       bump_list(out_used1 = 
		 mark_final_ids1(e1, in_used1, &out_sig1,
				in_sig | out_sig2 | out_sig3,no_final));
       bump_list(result = append_without_dups(out_used1, out_used23));
       *out_sig = out_sig1 | out_sig2 | out_sig3;
       drop_list(in_used1);
       drop_list(in_used2);
       drop_list(out_used1);
       drop_list(out_used2);
       drop_list(out_used3);
       goto out;
      }

    case LOCAL_ID_E:
      bump_list(result = int_cons(e->ETEMP, NIL));
      goto out;

    case SAME_E:
      {EXPR* sse1 = skip_sames(e1);

#      ifdef DEBUG
         if(trace_gen > 2) {
	   trace_t(160, no_final, EKIND(sse1));
	 }
#      endif

       /*---------------------------------------------------------*
	* If this is a SAME_E node above a local id, and this is  *
        * a final use of that id, then mark the SAME_E indicating *
        * so by setting its extra field.			  *
        *---------------------------------------------------------*/

       if(!no_final && in_sig && EKIND(sse1) == LOCAL_ID_E) {
	 if(int_member(sse1->ETEMP, in_used_ids)) {
	   e->extra = 0;
	 }
         else {
#          ifdef DEBUG
	     if(trace_gen > 2) {
	       trace_t(161, sse1);
	     }
#          endif

	   e->extra = 1;
	 }
         bump_list(result = int_cons(sse1->ETEMP, NIL));
	 goto out;
       }

       /*--------------------------------------------------------*
        * The default for a SAME_E node is just to tail recur on *
	* its subexpression.					 *
        *--------------------------------------------------------*/

       else {
	 e = sse1;
	 goto tail_recur;
       }
      }

    case FOR_E:
      {INT_LIST *in_used2, *out_used1, *out_used2, *out_used3, *out_used13;
       Boolean out_sig1, out_sig2, out_sig3;

       bump_list(out_used1 = mark_final_ids1(e1, NIL, &out_sig1, FALSE, TRUE));
       bump_list(out_used3 = 
		 mark_final_ids1(e->E3, NIL, &out_sig3, FALSE, TRUE));
       bump_list(out_used13 = append_without_dups(out_used1, out_used3));
       bump_list(in_used2 = append_without_dups(out_used13, in_used_ids));
       bump_list(out_used2 = 
		 mark_final_ids1(e2, in_used2, &out_sig2, TRUE, no_final));
       bump_list(result = append_without_dups(out_used2, out_used13));
       drop_list(out_used1);
       drop_list(out_used2);
       drop_list(out_used3);
       drop_list(out_used13);
       drop_list(in_used2);
       *out_sig = TRUE;
       goto out;
      }
      
    case LOOP_E:
      {INT_LIST *out_used1, *out_used2, *in_used1;
       Boolean out_sig1, out_sig2;

       bump_list(out_used2 = mark_final_ids1(e2, NIL, &out_sig2, FALSE, TRUE));
       if(e1 != NULL) {
	 bump_list(in_used1 = append_without_dups(out_used2, in_used_ids));
	 bump_list(out_used1 = mark_final_ids1(e1->E2, in_used1, &out_sig1,
					      TRUE, no_final));
	 bump_list(result = append_without_dups(out_used1, out_used2));
	 drop_list(out_used1);
	 drop_list(in_used1);
	 drop_list(out_used2);
       }
       else {
	 result = out_used2;   /* Ref from out_used2 */
       }
       *out_sig = TRUE;
       goto out;
      }
  }

 out:

#  ifdef DEBUG
     if(trace_gen > 2) {
       trace_t(162, e, *out_sig);
       print_str_list_nl(result);
     }
#  endif

   if(result != NIL) result->ref_cnt--;
   return result;
}


/*-----------------------------------------------------*/

void mark_final_ids(EXPR *e)
{
  INT_LIST *result;
  Boolean out_sig;

  temp_id_for_final = 1;
  set_all_temps(e);
  bump_list(result = mark_final_ids1(e, NIL, &out_sig, FALSE, FALSE));
  drop_list(result);

# ifdef DEBUG
    if(trace_gen > 1 && trace_exprs > 1) {
      trace_t(163);
      print_expr(e, 0);
    }
# endif
}


