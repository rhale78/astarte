/**********************************************************************
 * File:    infer/infer.c
 * Purpose: Perform main pass of type inference.
 * Author:  Juan Du and Karl Abrahamson
 **********************************************************************/

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
 * This file contains a function that performs the first phase of	*
 * type inference -- the phase before thinking about overloaded 	*
 * identifiers.  This function should not generally be used by itself,  *
 * since overloaded ids need to be handled.  During inference, 		*
 * equations that cannot be handled until overloaded ids are handled	*
 * are deferred here by putting them into a deferral list.		*
 *									*
 * File dcls/dcl.c contains the top level type inference handler that   *
 * calls this one.							*
 ************************************************************************/

#include "../misc/misc.h"
#include "../utils/lists.h"
#include "../utils/strops.h"
#include "../exprs/expr.h"
#include "../classes/classes.h"
#include "../standard/stdtypes.h"
#include "../unify/unify.h"
#include "../infer/infer.h"
#include "../error/error.h"
#include "../generate/prim.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

PRIVATE Boolean infer_for_role_select(EXPR *e);


/************************************************************************
 *			PRIVATE VARIABLES				*
 ************************************************************************/

/************************************************************************
 *			pat_fun_type					*
 *			image_type					*
 ************************************************************************
 * Variables pat_fun_type and image_type are used in processing pattern *
 * declarations.  image_type will hold the type that expression		*
 * 'target' should have. pat_fun_type holds type type that the pattern  *
 * function has.							*
 ************************************************************************/

PRIVATE TYPE** pat_fun_type = NULL;
PRIVATE TYPE** image_type   = NULL;


/************************************************************************
 *			INFER_TYPE_TC					*
 ************************************************************************
 * This function performs the first phase of type inference on		*
 * expression e.							*
 *									*
 * It can put some unifications on the deferal list. Those deferred	*
 * inferences must be handled after this phase completes.		*
 *									*
 * This function returns TRUE if it encounters no difficulties, but	*
 * that does not guarantee that all variables that must be bound were	*
 * in fact bound.  do_all_defaults_tc should be called to bind the	*
 * remaining variables.							*
 *									*
 * Remark:  Function application is handled in a special way in		*
 * infer_type.  In (a b), the type of a is presumed to be a function	*
 * type.  However, the hermit in the type cell is set to 1 to indicate	*
 * that this function can unify with type ().  Thus, a can receive	*
 * type ().  Function basic_unify_u in unify.c takes this into account.	*
 ************************************************************************/

Boolean infer_type_tc(EXPR *e)
{
  EXPR *e1, *e2, *e3, *e1a;
  TYPE *t, **etype;
  Boolean result;

  /*----------------------------------------------------------*
   * Ignore null expressions -- they represent unused fields. *
   *----------------------------------------------------------*/

  if(e == NULL_E) {
#   ifdef DEBUG
      if(trace) trace_t(302);
#   endif
    return FALSE;
  }

# ifdef DEBUG
    if(trace_infer) {
      trace_t(303, e, expr_kind_name[EKIND(e)]);
    }
# endif

  /*--------------------------------------------------------------------*
   * Get the fields here, to avoid having to do it over and over again. *
   *--------------------------------------------------------------------*/

  e1     = e->E1;
  e2     = e->E2;
  e3     = e->E3;
  etype  = &(e->ty);
  
  /*------------------------------------------------------------*
   * Set result to FALSE; at failed unifications, just jump to  *
   * out, without need to set result. 				*
   *------------------------------------------------------------*/

  result = FALSE;

  /*------------------------------*
   * Try each kind of expression. *
   *------------------------------*/

  switch(EKIND(e)) {

    case APPLY_E:

      /*------------------------------------------------*
       * Expression e is e1(e2). 			*
       * Perform inference on the subexpressions first. *
       *------------------------------------------------*/

      if(!infer_type_tc(e1)) {goto out;}
      if(!infer_type_tc(e2)) {goto out;}

      /*------------------------------------------------------*
       * Check for hermit being applied -- really sequencing. *
       * When e1 is (), the type of e must be the type of e2. *
       *------------------------------------------------------*/
      
      if (EKIND(e1) != OVERLOAD_E && is_hermit_type(e1->ty)) {
	result = unify_type_tc(e2, etype, NULL, 1);
        goto out;
      }

      /*-----------------------------------------------------------------*
       * If e1 is not (), then the type of e1 should be a function type. *
       * (There is still the possibility that e1 will turn out to be	 *
       * () -- that possibility is taken into account below.) 		 *
       *-----------------------------------------------------------------*/

      bump_type(t = function_t(NULL_T, NULL_T));

      /*-----------------------------------------------------------*
       * Actual parameter type should match formal parameter type. *
       *-----------------------------------------------------------*/

      if(!unify_type_tc(e2, &(t->ty1), t, 0)) {
	appl_type_error(e1, e2);
	drop_type(t);
	goto out;
      }

      /*------------------------------------------------------------*
       * Output type of function should match type of expression e. *
       *------------------------------------------------------------*/

      if(!unify_u(etype, &(t->TY2), FALSE)) {
	type_error(e, t->TY2);
	drop_type(t);
	goto out;
      }

      /*------------------------------------------------------------*
       * Want a function being applied. This must be after previous *
       * unifications, since the function type t might be converted *
       * to () here. Might still convert to () if use of function   *
       * fails. We set t->hermit_f = 1 to permit type t (the type of*
       * e1) to become () during unification.  			    *
       *------------------------------------------------------------*/

      t->hermit_f = 1;
      if(!unify_type_tc(e1, &t, t, 0)) {

	/*------------------------------------------------------*
	 * e1 will not unify with the function type t.  Try to  *
	 * make the type of e1 be ().  If this is not possible, *
	 * undo any temporary bindings, since they will only    *
	 * confuse the programmer.  				*
	 *							*
	 * Can only let e1 have type () if the type of e2 will  *
	 * unify with the type of e. 				*
	 *------------------------------------------------------*/

	LIST *binding_list_mark;
	bump_list(binding_list_mark = finger_new_binding_list());
	if(unify_u(&(e2->ty), etype, TRUE)) {

	  /*--------------------------------------------------*
	   * Ok, so try unifying the type of e1 with type (). *
	   *--------------------------------------------------*/

	  if(unify_type_tc(e1, &hermit_type, NULL, 0)) {
	    drop_type(t);
	    drop_list(binding_list_mark);
	    result = TRUE;
	    goto out;
	  }

	  /*----------------------------------------------*
	   * At failure to unify type of e1 with (), undo *
	   * unification of type of e2 with type of e.    *
	   *----------------------------------------------*/
	   
	  else {
	    undo_bindings_u(binding_list_mark);
	  }
	}
	
	/*------------------------------------------------------*
	 * Couldn't unify e1 with t or make e1 have type (), so *
	 * complain. 						*
	 *------------------------------------------------------*/

	drop_list(binding_list_mark);
	type_error(e1, t);
	drop_type(t);
	goto out;
      }

      /*-----------------------------*
       * All is well -- return true. *
       *-----------------------------*/

      drop_type(t);
      result = TRUE; 
      goto out;

    case SEQUENCE_E:

      /*-----------------------------------------------------------*
       * e is an expression that calls for doing e1, splitting the *
       * result, then doing e2.  This is only used when both e and *
       * e2 must have type (). 	   				   *
       *-----------------------------------------------------------*/

      if(!infer_type_tc(e1)) goto out;
      if(!infer_type_tc(e2)) goto out;
      if(!unify_type_tc(e2, &hermit_type, NULL, 1)) goto out;
      if(!unify_type_tc(e, &hermit_type, NULL, 1)) goto out;
      result = TRUE;
      goto out;

    case AWAIT_E:

      /*------------------------------------------------------------*
       * e is await e1 then e2 %await.  e1 = NULL if it is missing. *
       * The type of e is the same as the type of e2. 		    *
       *------------------------------------------------------------*/

      if(e1 != NULL && !infer_type_tc(e1)) {goto out;}
      if(!infer_type_tc(e2)) {goto out;}
      if(!unify_type_tc(e2, etype, NULL, 1)) {goto out;}
      result = TRUE; 
      goto out;

    case SINGLE_E:
      if(e->SINGLE_MODE == ROLE_SELECT_ATT) {
	result = infer_for_role_select(e);
	break;
      }
      /* No break - fall through to next case. */

    case SAME_E:
    case EXECUTE_E:

      /*----------------------------------------------------------------*
       * SINGLE_E expressions are normally used where the type of e1 is *
       * the same as the type of e. The other forms also require	*
       * that e have the same type as e1. 				*
       *----------------------------------------------------------------*/

      if(!infer_type_tc(e1)) {goto out;}
      if(!unify_type_tc(e1, etype, NULL, 1)) {goto out;}
      result = TRUE; 
      goto out;

    case TEST_E:

      /*------------------------------------------------------------*
       * e is {e1 else e2}.  e1 must have type Boolean, and e2 must *
       * have type ExceptionSpecies.  e itself has type ().	    *
       *------------------------------------------------------------*/

      if(!infer_type_tc(e1)) {goto out;}
      if(!unify_u(etype, &hermit_type, FALSE)) {
	type_error(e, hermit_type);
        goto out;
      }
      if(!unify_type_tc(e1, &boolean_type, NULL, 1)){goto out;}
      if(!unify_type_tc(e2, &exception_type, NULL, 1)){goto out;}
      result = TRUE; 
      goto out;

    case STREAM_E:

      /*--------------------------------------------------------*
       * e is stream e1 then e2 %stream or mix e1 then e2 %mix. *
       * e, e1 and e2 must all have the same type. 		*
       *--------------------------------------------------------*/

      if(!infer_type_tc(e1)) {goto out;}
      if(!infer_type_tc(e2)) {goto out;}
      if(!unify_type_tc(e1, etype, NULL, 1)) {goto out;}
      if(!unify_type_tc(e2, etype, NULL, 1)) {goto out;}
      result = TRUE; 
      goto out;

    case LAZY_LIST_E:

      /*----------------------------------------------------*
       * e is [:e1:].  e has type [T], where e1 has type T. *
       *----------------------------------------------------*/

      bmp_type(t = list_t(NULL_T));
      if(!unify_u(etype, &t, FALSE)) {
	type_error(e, t);
	drop_type(t);
        goto out;
      }
      if(!infer_type_tc(e1)) {goto out;}
      if(!unify_type_tc(e1, &(t->ty1), t, 1)) {
	drop_type(t);
	goto out;
      }
      drop_type(t);
      result = TRUE; 
      goto out;

    case CONST_E: 
    case LOCAL_ID_E:
    case OVERLOAD_E:
    case GLOBAL_ID_E:
    case PAT_FUN_E:

      /*----------------------------------------------------------------*
       * Types for global ids are assigned by old_id_tm, when they	*
       * are fetched from the table.  Types for constants are assigned	*
       * when the constants are created.  Types for local ids are	*
       * left open, and will be assigned according to context at	*
       * other cases.  Types for overloaded ids will be handled		*
       * later in the type inference process.  So do nothing here. 	*
       *----------------------------------------------------------------*/

      result = TRUE; 
      goto out;

    case UNKNOWN_ID_E:

      /*-----------------------------*
       * e is an unknown identifier. *
       *-----------------------------*/

      semantic_error1(LATE_UNKNOWN_ERR, display_name(e->STR), e->LINE_NUM);
      goto out;

    case PAT_VAR_E:

      /*----------------------------------------------------------------*
       * e is a pattern variable.  e1 is the associated identifier, or	*
       * is NULL if e is an anonymous pattern variable.  If e1 is	*
       * not NULL, then e and e1 must have the same type. 		*
       *----------------------------------------------------------------*/

      result = (e1 == NULL) ? TRUE : unify_type_tc(e1, etype, NULL, 1);
      goto out;

    case SPECIAL_E:

      /*-------------------------------------*
       * e is one of various special things. *
       *-------------------------------------*/

      switch(e->PRIMITIVE) {
        case PRIM_EXCEPTION:

	  /*---------------------------------------------------------*
	   * e is 'exception', so the type of e is ExceptionSpecies. *
	   *---------------------------------------------------------*/

	  if(!unify_u(etype, &exception_type, FALSE)) {
	    type_error(e, exception_type);
	    goto out;
          }
	  result = TRUE; 
          goto out;

	case PRIM_TARGET:

	  /*------------------------------------------------------*
	   * e is 'target'. Its type must be image_type, which is *
 	   * set at PAT_RULE_E 					  *
	   *------------------------------------------------------*/

	   if(image_type != NULL) {
	     if(!unify_u(etype, image_type, FALSE)) {
	       type_error(e, *image_type);
	       goto out;
	     }
	   }
	   /*-----------------------------------------------------------*
	    * No break -- handle like default if don't have image_type. *
	    *-----------------------------------------------------------*/
	default:

	  /*--------------------------------------------------------*
	   * Other special forms have been handled when introduced. *
	   *--------------------------------------------------------*/

	  result = TRUE; 
	  goto out;
      }

    case MATCH_E:
    case DEFINE_E:
    case LET_E:

      /*----------------------------------------------------*
       * e is let e1 = e2 %let or define e1 = e2 %define or *
       * match e1 = e2 %match.  In all cases, e must have   *
       * type () and e1 and e2 must have the same type.	    *
       *----------------------------------------------------*/

      if(!infer_type_tc(e1)) {goto out;}
      if(!infer_type_tc(e2)) {goto out;}
      if(!unify_u(etype, &hermit_type, FALSE)) {
	type_error(e, hermit_type);
        goto out;
      }

      /*------------------------------------------------------------*
       * Either of e1 and e2 might be an overload.  unify_type_g_tc *
       * handles overloads, so we need to call it twice, to give    *
       * each expression a chance to have its unification deferred. *
       *------------------------------------------------------------*/

      result = unify_type_g_tc(e2, &(e1->ty), NULL, 1, e1);
      if(result) unify_type_tc(e1, &e1->ty, NULL, 1);
      goto out;

    case IF_E:
    case TRY_E:

      /*-----------------------------------------------------------------*
       * e is if e1 then e2 else e3 %if, or try e1 then e2 else e3 %try. *
       * After recursive inferences, unify the types of e, e1 and e2.    *
       * For an if, e1 has type Boolean.  For a try, e1 has type (). 	 *
       *-----------------------------------------------------------------*/

      {TYPE **test_type;
       if(!infer_type_tc(e1)) {goto out;}
       if(!infer_type_tc(e2)) {goto out;}
       if(!infer_type_tc(e3)) {goto out;}
       if(!unify_type_tc(e2, etype, NULL, 1)) {goto out;}
       if(!unify_type_tc(e3, etype, NULL, 1)) {goto out;}
       test_type = (EKIND(e) == TRY_E) ? &hermit_type : &boolean_type;
       result = unify_type_tc(e1, test_type, NULL, 1);
       goto out;
      }

    case TRAP_E:

       /*-------------------------------------------------------*
        * e is trap e1 => e2 %trap or untrap e1 => e2 %untrap.	*
        * e1 is NULL for forms trap all => e2 %trap and		*
        * untrap all => e2 %untrap.  e1 has type Exception if	*
        * not null, and e has the same type as e2. 		*
        *-------------------------------------------------------*/

       if(e1 != NULL && !infer_type_tc(e1)) {goto out;}
       if(!infer_type_tc(e2)) {goto out;}
       if(e1 != NULL && !unify_type_tc(e1, &exception_type, NULL, 1)) goto out;
       result = unify_type_tc(e2, etype, NULL, 1);
       goto out;

    case OPEN_E:

      /*----------------------------------------------------------------*
       * e is an open if or open try.  The lists e->EL1 and e->EL2	*
       * are parallel lists of corresponding identifiers from the	*
       * different branches of the if or try expression.  (So, if	*
       * e->EL1 begins with expression x, and e->EL2 begins with	*
       * expression x', then x and x' are two different nodes that	*
       * must represent the same identifier.)  So unify the types	*
       * of corresponding identifiers. 					*
       *								*
       * There is an important exception.  If either list e->EL1 or	*
       * e->EL2 begins with an expression whose name starts with	*
       * character code 1, then that list indicates a branch that	*
       * is a forced failure, and the two lists should be ignored	*
       * here. 								*
       *----------------------------------------------------------------*/

      {EXPR_LIST* p = e->EL1;
       EXPR_LIST* q = e->EL2;
       result = TRUE;
       if(p != NIL && p->head.expr->STR[0] == 1) goto out;
       if(q != NIL && q->head.expr->STR[0] == 1) goto out;
       for(;p != NIL; p = p->tail, q = q->tail) {
	 if(!UNIFY(p->head.expr->ty, q->head.expr->ty, FALSE)) {
	   result = FALSE;
	   goto out;
	 }
       }
       if(!infer_type_tc(e1)) {result = FALSE; goto out;}
       result = unify_type_tc(e1, etype, NULL, 1);
       goto out;
     }

    case PAIR_E:
    case FUNCTION_E:

      /*--------------------------------------------------------------------*
       * e is (e1, e2) or (e1 => e2).  Unify the type of e with a pair type *
       * or function type holding the types of e1 and e2.  		    *
       *--------------------------------------------------------------------*/

      if(!infer_type_tc(e1)) {goto out;}
      if(!infer_type_tc(e2)) {goto out;}
      bmp_type(t = (EKIND(e) == PAIR_E) 
	              ? pair_t(NULL_T, NULL_T)
		      : function_t(NULL_T, NULL_T));
      if(!unify_u(etype, &t, FALSE)) {
	type_error(e, t);
	drop_type(t);
	goto out;
      }
      if(!unify_type_tc(e1, &(t->ty1), t, 1)) {
	drop_type(t);
	goto out;
      }
      if(!unify_type_tc(e2, &(t->TY2), t, 1)){
	drop_type(t);
	goto out;
      }
      drop_type(t);
      result = TRUE; 
      goto out;

    case LAZY_BOOL_E:

      /*---------------------------------------------*
       * e is (a and b) or (a or b) or (a implies b) *
       *---------------------------------------------*/

      if(!infer_type_tc(e1)) {goto out;}
      if(!infer_type_tc(e2)) {goto out;}
      if(!unify_u(etype, &boolean_type, FALSE)) {
	type_error(e, boolean_type);
	goto out;
      }
      if(!unify_type_tc(e1, &boolean_type, NULL, 1)) {goto out;}
      if(!unify_type_tc(e2, &boolean_type, NULL, 1)) {goto out;}
      result = TRUE; 
      goto out;
   
    case WHERE_E:

      /*------------------*
       * e is e1 where e2 *
       *------------------*/

      if(!infer_type_tc(e1)) {goto out;}
      if(!infer_type_tc(e2)) {goto out;}
      if(!unify_type_tc(e1, etype, NULL, 1)) {goto out;}
      if(!unify_type_tc(e2, &boolean_type, NULL, 1)) {goto out;}
      result = TRUE;
      goto out;

    case FOR_E:

      /*----------------------------------------------------------*
       * e is for e1 from e2 do e3 %for.  e and e3 must each have *
       * type ().  e2 must have type [T], where e1 has type T.    *
       *----------------------------------------------------------*/

      if(!infer_type_tc(e1)) {goto out;}
      if(!infer_type_tc(e2)) {goto out;}
      if(!infer_type_tc(e3)) {goto out;}
      if(!unify_u(etype, &hermit_type, FALSE)) {
	type_error(e, hermit_type);
	goto out;
      }
      if(!unify_type_tc(e3, &hermit_type, NULL, 1)) {goto out;}
      bump_type(t = copy_type(list_t(any_type), 0));
      if(!unify_type_tc(e2, &t, t, 1)) {drop_type(t); goto out;}
      if(!unify_type_tc(e1, &(t->TY1), t, 1)) {drop_type(t); goto out;}
      drop_type(t);
      result = TRUE;
      goto out;

    case LOOP_E:

      /*------------------------------------------*
       * e is loop p = a MODE e2 %loop, where	  *
       *   e1   is expression match p = a %match, *
       *        or is NULL if p=a is missing,	  *
       *   MODE tells the loop mode.		  *
       * The types of e and e2 must be the same.  *
       *------------------------------------------*/
	 
      if(e1 != NULL_E && !infer_type_tc(e1)) {goto out;}
      if(!infer_type_tc(e2)) {goto out;}
      if(!unify_u(etype, &(e2->ty), FALSE)) {
	type_error(e, e2->ty);
	goto out;
      }
      result = TRUE; 
      goto out;

    case RECUR_E:

      /*----------------------------------------------------------------*
       * e is a 'continue e1' in a loop expression. e2 refers back	*
       * to the loop expression in which this continue occurs.  So	*
       * e1 and e2->E1->E2 must have the same type. That is, if the	*
       * loop has the form loop p = a MODE body, and body says		*
       * continue x, then x and a must have the same type.  		*
       * The type of e is the same as the type of the loop, since	*
       * it represents a recursive call.  So e and e2 must have		*
       * the same type. 						*
       *----------------------------------------------------------------*/

      if(e1 != NULL && !infer_type_tc(e1)) {goto out;}
      if(e2->E1 != NULL_E) {
        if(!unify_type_tc(e1, &(e2->E1->E2->ty), NULL, 1)) {goto out;}
      }
      if(!unify_u(etype, &(e2->ty), FALSE)) {
	type_error(e, e2->ty);
	goto out;
      }
      result = TRUE; 
      goto out;

    case MANUAL_E:

      /*---------------------------------------------------------*
       * e is a description of identifier e1.  Make its type (). *
       *---------------------------------------------------------*/

      if(e1 != NULL) {
	if(!infer_type_tc(e1)) {goto out;}
      }
      if(!unify_type_tc(e, &hermit_type, NULL, 1)) {goto out;}
      result = TRUE; 
      goto out;

    case EXPAND_E:

      /*----------------------*
       * e is expand e1 => e2 *
       *----------------------*/

      if(!infer_type_tc(e1)) goto out;
      if(!infer_type_tc(e2)) goto out;

      /*------------------------------------------------------------*
       * Unify_typ is responsible for deferring overloads.  We need *
       * to give it a chance to defer e1 or e2, so call it twice.   *
       *------------------------------------------------------------*/

      if(!unify_type_tc(e2, &(e1->ty), NULL, 1)) goto out;
      unify_type_tc(e1, &(e1->ty), NULL, 1);
      result = TRUE;
      goto out;

    case PAT_DCL_E:

      /*----------------------------------------------------------------*
       * e is pattern a => b %pattern, or pattern a = b %pattern,	*
       * where a and b are stored in the pattern rule e2 in the		*
       * case of pattern a => b, and e1 = a, e2 = b in the case		*
       * of pattern a = b.  For the case pattern a => b, 		*
       * image_type is set (at PAT_RULE_E) to the type that expression	*
       * 'target' should have in b, and pat_fun_type is set to the type	*
       * of the pattern function.  (pat_fun_type was used for		*
       * consistency checking, but is not really needed any more.)      *
       *----------------------------------------------------------------*/

      if(!infer_type_tc(e1)) goto out;

      /*-----------------------*
       * Handle pattern f = g. *
       *-----------------------*/

      if(e->PAT_DCL_FORM == 1) {
	e1a = skip_sames(e2);
	e1a->kind = PAT_FUN_E;

	/*--------------------------------------------------------*
	 * Call unify_type_tc twice, to give it a chance to defer *
         * either e1 or e2, if one is an overload.		  *
	 *--------------------------------------------------------*/

        if(!unify_type_tc(e2, &(e1->ty), NULL, 1)) {goto out;}
	unify_type_tc(e1, &(e1->ty), NULL, 1);
	if(!infer_type_tc(e2)) {goto out;}
	result = TRUE; 
	goto out;
      }

      /*---------------------------------------*
       * Handle pattern pat => translation ... *
       *---------------------------------------*/

      pat_fun_type = &(e1->ty);
      if(!infer_type_tc(e2)) goto out;
      pat_fun_type = image_type = NULL;
      result = TRUE; 
      goto out;

    case PAT_RULE_E:

      /*--------------------------------------------------------*
       * e is a pattern translation or expansion rule e2 => e3. *
       *--------------------------------------------------------*/

      {EXPR *param;
       if(!infer_type_tc(e2)) {goto out;}
       image_type = &(e2->ty);

       /*---------------------------------------------------------------*
        * Check that this rule has the same type as the main pattern 	*
        * function.  This is really for an older style of pattern	*
        * declaration, but it is left in, since it doesn't seem		*
        * to matter much. 						*
        *---------------------------------------------------------------*/

       param = e2;
       while(param != NULL && EKIND(param) == APPLY_E) param = param->E1;
       if(param != NULL && pat_fun_type != NULL) {
         if(!unify_u(&(param->ty), pat_fun_type, 0)) goto out;
         push_type(unification_defer_holds, param->ty);
       }

       if(!unify_type_tc(e->E3, &hermit_type, NULL, 1)) {goto out;}
       if(!infer_type_tc(e->E3)) {goto out;}
       if(e1 != NULL_E && !infer_type_tc(e1)) {goto out;}
       result = TRUE; 
       goto out;
      }

    case IDENTIFIER_E:
    case BAD_E:
      /*--------------------------------------------------------*
       * IDENTIFIER_E nodes should have been eliminated before  *
       * type inference is done 				*
       *--------------------------------------------------------*/

      goto out;

   default:
      die(13, (char *)(long) EKIND(e));
  }

  out:

# ifdef DEBUG
    if(trace_infer) {
      trace_t(304, toint(result), e);
    }
# endif

  return result;
}


/****************************************************************
 *			INFER_FOR_ROLE_SELECT			*
 ****************************************************************
 * Perform type inference on a SINGLE_E expression e with tag	*
 * ROLE_SELECT_ATT.						*
 ****************************************************************/

PRIVATE Boolean infer_for_role_select(EXPR *e)
{
  EXPR* p;
  EXPR* e1 = e->E1;

  /*----------------------------*
   * Do inference on each part. *
   *----------------------------*/

  for(p = e1; EKIND(p) == PAIR_E; p = p->E2) {
    if(!infer_type_tc(p->E1)) return FALSE;
  }
  if(!infer_type_tc(p)) return FALSE;

  /*---------------------------------------------------------------*
   * Unify the types of the parts together, and the type of e with *
   * the type of its parts. 					   *
   *---------------------------------------------------------------*/

  {TYPE** t = &(e->ty);
   for(p = e1; EKIND(p) == PAIR_E; p = p->E2) {
     if(!unify_type_tc(p->E1, t, NULL, 1)) return FALSE;
   }
   if(!unify_type_tc(p, t, NULL, 1)) return FALSE;
  }

  return TRUE;
}
