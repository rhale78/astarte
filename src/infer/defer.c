/**********************************************************************
 * File:    infer/defer.c
 * Purpose: Functions to handle deferred type inference.
 * Author:  Karl Abrahamson
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
 * This file contains functions that perform the late stages of	type	*
 * inference.  These functions must backtrack through various 		*
 * possibilities.  Since recursion is used to handle the backtracking,	*
 * these functions are rather ugly.					*
 *									*
 * This file also contains support for error handling.  When type 	*
 * inference fails in the late stages, the compiler only gets an	*
 * indication that the backtracking could not find a solution.  It	*
 * needs to retrace the backtracking to the "most promising" point, so	*
 * that it can give a reasonable error message indicating why inference *
 * failed.								*
 ************************************************************************/

#include "../misc/misc.h"
#include "../utils/lists.h"
#include "../utils/hash.h"
#include "../utils/strops.h"
#include "../error/error.h"
#include "../tables/tables.h"
#include "../parser/tokens.h"
#include "../parser/parser.h"
#include "../lexer/modes.h"
#include "../ids/open.h"
#include "../ids/ids.h"
#include "../unify/unify.h"
#include "../infer/infer.h"
#include "../alloc/allocate.h"
#include "../classes/classes.h"
#include "../exprs/expr.h"
#include "../dcls/dcls.h"
#include "../generate/generate.h"
#include "../generate/prim.h"
#include "../patmatch/patmatch.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/************************************************************************
 *			PUBLIC VARIABLES				*
 ************************************************************************/

/************************************************************************
 *			version_list					*
 ************************************************************************
 * version_list provides information for a declaration of the form	*
 *    Let species T species S ... %Let					*
 *									*
 * where								*
 *									*
 *   version_list.types		is the list of types following species,	*
 *   version_list.roles		is a parallel list of roles		*
 *   version_list.descrip	is the description in the definition.	*
 ************************************************************************/

struct with_info version_list = {NULL,NULL, NULL};

/************************************************************************
 *			unification_defer_types				*
 *			unification_defer_exprs				*
 *			unification_defer_holds				*
 ************************************************************************
 * When unify_type_tc in t_unify.c sees an overloaded identifier id	*
 * whose species must be t, it adds pair (id,t) to the parallel lists	*
 * unification_defer_exprs, unification_defer_types, for handling later *
 * by handle_deferrals_tc.						*
 *									*
 * In cases where the type of an overloaded id must be unified with	*
 * the type of a cell that is currently NULL, unification_defer_holds	*
 * gets a type pushed onto it that is needed to keep track of reference	*
 * counts.								*
 ************************************************************************/

LIST *unification_defer_types;
LIST *unification_defer_exprs;
LIST *unification_defer_holds;

/************************************************************************
 *			suppress_using_local_expectations		*
 ************************************************************************
 * suppress_using_local_expectations is true if handle_expects_tc	*
 * should just go directly to handle_deferrals_tc, without trying	*
 * local expectations as assumptions.  It is set when defining		*
 * automatic symbols that should not be subject to local expectations.	*
 ************************************************************************/

Boolean suppress_using_local_expectations = FALSE;


/************************************************************************
 *			GET_LOCAL_EXPECTS				*
 ************************************************************************
 * Set *expects to a list of the local expectations of definition e,    *
 * where name is the name of the identifier being defined.		*
 * Also set *roles to the list of roles of the identifier being defined.*
 *									*
 * Neither roles nor expects is reference counted.			*
 ************************************************************************/

PRIVATE void get_local_expects(EXPR *e, char *name, TYPE_LIST **expects,
			      ROLE_LIST **roles)
{
  if(version_list.types != NIL) {
    *expects = version_list.types;
    *roles   = version_list.roles;
  }
  else if(EKIND(e) == SAME_E && e->SAME_MODE == 5 && e->EL1 != NIL) {
    *expects = e->EL1;
    *roles   = e->EL2;
  }
  else {
    *expects = local_expectation_list_tm(name,0);
    *roles = NIL;
  }
}


/************************************************************************
 *			HANDLE_EXPECTS_TC				*
 ************************************************************************
 * This should be called after the initial phase of type inference,	*
 * done by infer_types_tc.						*
 *									*
 * This function finishes type inference and initial processing on	*
 * e_orig by 								*
 *									*
 *   1. using expectations or versions as assumptions, 			*
 *   2. handling overloaded identifiers, 				*
 *   3. handling pattern matches, 					*
 *   4. handling defaults						*
 *									*
 * This function itself is responsible for handling local expectations	*
 * or versions as assumptions.   It tries all local expectations or     *
 * versions as assumptions on expresssions e and rest, then does the	*
 * remainder of inference on e_orig.  After handle_expects_tc is done	*
 * handling local expectations, handle_deferrals_tc is called for the	*
 * remainder of the processing.						*
 *									*
 * When called from outside, e should be the same as e_orig, and rest	*
 * should be NULL.  Parameters e and rest are used for recursive	*
 * calls.  e_orig is maintained, through recursive calls, as the	*
 * original expression on which we are doing inference.  e is the	*
 * expression on which handle_expects_tc is currently working, and	*
 * rest is some expression on which handle_expects_tc must work		*
 * later.  These are used for teams, where there are several		*
 * definitions, and each must be processed by handle_expects_tc.	*
 *									*
 * This function returns the number of successes.			*
 *									*
 * If suppress_using_local_expectations is true, then local 		*
 * expectations are ignored.  (This is used in handling automatically	*
 * defined things, like type constructors.)				*
 *									*
 * MODE is the mode of the definition, and kind is the kind of		*
 * definition.  It is a safe pointer: it does not live longer than	*
 * this function call.							*
 *									*
 * Variables *DL, *LDL, *EDL and *LEDL are used for returning to	* 
 * the most promising point after an error occurs during this		*
 * stage of type inference.  Variables *DL and *LDL are used for the    *
 * case	where the error is found in handle_deferrals_tc.  		*
 * They are just passed through handle_expects_tc to 			*
 * handle_deferrals_tc.  Variables *EDL and *LEDL are used here.	*
 * *EDL is a "direction" list, and *LEDL holds its length.		*
 * The idea is that the i-th entry tells which expectation is		*
 * the best one to use for the i-th variable that has local		*
 * expectations.  Specifically, the i-th entry of *EDL is		*
 * the index in the expectation list for the i-th variable of the	*
 * species to use for that variable.  (Actually, *EDL is stored in	*
 * reverse order, since we add each new direction to the front of it,	*
 * so it needs to be reversed.)						*
 ************************************************************************/

int handle_expects_tc(EXPR *e_orig, EXPR *e, EXPR *rest, int kind, 
		      MODE_TYPE *mode, INT_LIST **EDL, int *LEDL,
		      INT_LIST **DL, int *LDL)
{
  EXPR *sse;
  EXPR_TAG_TYPE ekind;

  *LEDL = *LDL = 0;
  *EDL  = *DL  = NIL;

  /*------------------------------------------------------------------*
   * There are only local expectations for let and define 	      *
   * declarations.  Also, e is NULL if we are done processing local   *
   * expectations, and it is time to go to handle_deferrals_tc to     *
   * complete this phase of of type inference.			      *
   *------------------------------------------------------------------*/

  sse = skip_sames(e);
  if(e == NULL_E
     || suppress_using_local_expectations
     || kind == EXECUTE_E 
     || EKIND(sse) == PAT_DCL_E 
     || EKIND(sse) == EXPAND_E
     || (kind == MANUAL_E && e->E1 == NULL_E)) {

    int result = handle_deferrals_tc(unification_defer_exprs,
				     unification_defer_types,
			     	     e_orig, kind, mode, DL, LDL, 0);
#   ifdef DEBUG
      if(trace_infer > 1) {
	trace_t(28);
	trace_t(278, result);
      }
#   endif

    return result;
  }

  /*-------------------------------------------------------------*
   * Here, we must attach local expectations to definition e, if *
   * there are any appropriate local expectations.		 *
   *-------------------------------------------------------------*/

  /*--------------------------------------------------------------------*
   * If e is an APPLY_E then e is a team.  Handle expectations 		*
   * for each definition in the team.  That is done by moving		*
   * definitions to be handled later into the rest parameter.		*
   *--------------------------------------------------------------------*/

  ekind = EKIND(sse);
  if(ekind == APPLY_E) {
    EXPR *new_rest;
    int num_successes;

    bump_expr(new_rest = (rest == NULL) 
	      		    ? sse->E2 
			    : apply_expr(sse->E2, rest, sse->E2->LINE_NUM));
    num_successes = handle_expects_tc(e_orig, sse->E1, new_rest, 
				      kind, mode, EDL, LEDL, DL, LDL);
    drop_expr(new_rest);
    return num_successes;
  }

  /*------------------------------------------------------*
   * If e is not an application, then it is a definition  *
   * or (). 						  *
   *------------------------------------------------------*/

  else {
    TYPE_LIST *expects, *p;
    LIST *mark, *pr;
    TYPE **this_type, *s;
    int direction;

    int       num_successes = 0;
    int       LDLT 	    = 0;
    int       LEDLT 	    = 0;
    INT_LIST* DLT 	    = NIL;
    INT_LIST* EDLT 	    = NIL;
    EXPR*     id 	    = NULL;
    char*     name          = NULL;
    LIST*     roles 	    = NULL;

    /*----------------------------------------------------------*
     * If this is a definition, then id is the function being   *
     * applied.  (If e = f a b c, then id is f.)		*
     *								*
     * Also get expects and roles, telling what to install as	*
     * local expectations.  It comes from version_list if	*
     * version_list.types != NIL, from a heading SAME_E node if *
     * there is one in e and that node gives versions, and from	*
     * local_expectation_list_tm otherwise.			*
     *----------------------------------------------------------*/

    if(ekind == CONST_E) expects = NIL;  /* e = () */
    else {
      id = skip_sames(e->E1);
#     ifdef NEVER
        if(ekind == EXPAND_E) id = get_applied_fun(id, TRUE);
#     endif
      name = new_name(id->STR, TRUE);
      get_local_expects(e, name, &expects, &roles);
      bump_list(expects);
      bump_list(roles);
    }

#   ifdef DEBUG
      if(trace_infer > 1) {
	trace_t(28);
	trace_t(273, nonnull(name));
	for(p = expects; p != NIL; p = p->tail) {
	  fprintf(TRACE_FILE, " ");
	  trace_ty(p->head.type);
	  tracenl();
	}
      }
#   endif

    /*--------------------------------------------------------------*
     * Skip ids without local expectations.  (That is, just pass    *
     * them on to the next stage.)  This is done by moving 	    *
     * expression rest up to where it will be processed in the      *
     * recursive call.						    *
     *								    *
     * Note: no need to drop expects and roles, since both will be  *
     * NIL.							    *
     *--------------------------------------------------------------*/

    if(expects == NIL) {
      return handle_expects_tc(e_orig, rest, NULL,
			       kind, mode, EDL, LEDL, DL, LDL);
    }

    /*------------------------------------------------------------*
     * Here, there is at least one local expectation.  First, 	  *
     * check for overlaps in the local expectation list.  A bad   *
     * overlap should be reported, since it can cause definitions *
     * to be compiled twice.  If a bad overlap is reported, or if *
     * none is found, then the list tag on the first list cell    *
     * is set to TYPE1_L, to suppress further tests or reports.   *
     * So if the kind has already been set to TYPE1_L, then don't *
     * do the overlap test now.					  *
     *								  *
     * Before checking of overlaps, reduce the list to those	  *
     * expectations that are consistent with the type of id as	  *
     * inferred so far.  Only those expectations are of interest  *
     * anyway.							  *
     *								  *
     * Do not do this check if the mode has DEFAULT_MODE and      *
     * either OVERRIDES_MODE or CHECK_UNDERRIDES_MODE, since then *
     * there will be no problem with multiple definitions.        *
     *------------------------------------------------------------*/

    if(!(has_mode(mode, DEFAULT_MODE) &&
         (has_mode(mode, OVERRIDES_MODE) || 
	  has_mode(mode, UNDERRIDES_MODE))) &&
       LKIND(expects) != TYPE1_L) {
      TYPE_LIST *q, *reduced_expects;
      bump_list(reduced_expects = reduce_type_list(expects, id->ty));
      for(p = reduced_expects; p != NIL; p = p->tail) {
	for(q = p->tail; q != NIL; q = q->tail) {
	  if(overlap_u(p->head.type, q->head.type) == BAD_OV) {
	    char* spec_name = display_name(name);
	    warn1(LOCAL_EXPECT_ERR, spec_name, e_orig->LINE_NUM);
	    err_print(SPACE2_ERR);
	    err_print(SPACE2_STR_ERR, spec_name);
	    err_print_ty(p->head.type);
	    err_nl();
	    err_print(SPACE2_ERR);
	    err_print(SPACE2_STR_ERR, spec_name);
	    err_print_ty(q->head.type);
	    err_nl();
	    expects->kind = TYPE1_L;
	  }
	} /* end for(q = ...) */
      } /* end for(p = ...) */

      drop_list(reduced_expects);

    } /* end if(LKIND(expects) != TYPE1_L) */

    /*-----------------------------*
     * Try each local expectation. *
     *-----------------------------*/

    this_type = &(id->ty);
    pr = roles;
    for(p = expects, direction = 0; p != NIL; p = p->tail, direction++) {
      bump_list(mark = finger_new_binding_list());
      bump_type(s = copy_type(p->head.type, 0));

#     ifdef DEBUG
	if(trace_infer) {
	  trace_t(28);
	  trace_t(274);
	  trace_ty( s);
	  trace_t(275, name);
	}
#     endif

      /*-----------------------------------------------------------*
       * Is this expectation consistent with what we have already? *
       *-----------------------------------------------------------*/

      if(unify_u(&s, this_type, TRUE)) {

#       ifdef DEBUG
	  if(trace_infer) {
	    trace_t(28);
	    trace_t(276, name);
	  }
#       endif

	/*-----------------------------------------------------------*
	 * The unification above installs this local expectation.    *
         * Install the role if there is one.			     *
	 *							     *
         * After trying this local expectation, we must handle those *
	 * in expression rest.					     *
         *-----------------------------------------------------------*/

	if(pr != NULL && pr->head.role != NULL) {
	  SET_ROLE(id->role, checked_meld_roles(id->role, pr->head.role, id));
	}

	num_successes += handle_expects_tc(e_orig, rest, NULL_E, kind,
					   mode, &EDLT, &LEDLT,
					   &DLT, &LDLT);

	/*-----------------------------------------------------------*
         * Update the direction lists.  The most promising direction *
	 * is presumed to be the one with the most successes.  So    *
 	 * long direction lists are preferred.		             *
	 *-----------------------------------------------------------*/

	if(*LEDL + *LDL <= LEDLT + LDLT) {
	  set_list(EDL, int_cons(direction, EDLT));
	  *LEDL = LEDLT + 1;
	  set_list(DL, DLT);
	  *LDL = LDLT;
	}
	drop_list(EDLT);
	drop_list(DLT);

      } /* end if(unify...) */

      undo_bindings_u(mark);
      drop_list(mark);
      drop_type(s);
      if(pr != NIL) pr = pr->tail;

    } /* end for(p = expects...) */

    drop_list(expects);
    drop_list(roles);

#   ifdef DEBUG
      if(trace_infer > 1) {
	if(num_successes == 0) {
	  trace_t(28);
          trace_t(277);
          print_str_list_nl(*EDL);
          print_str_list_nl(*DL);
        }
	trace_t(28);
	trace_t(278, num_successes);
      }
#   endif

    return num_successes;

  } /* end if(ekind != APPLY_E) */

}



/************************************************************************
 *			HANDLE_DEFERRALS_TC				*
 ************************************************************************
 * During type inference, unifications involving overloaded		*
 * identifiers are deferred.  This function finishes type inference,	*
 * and then issues declaration main_e, of kind 'kind' 			*
 * (and mode MODE for a define).  kind should be one of			*
 * FIRST_PASS, DEFINE_E, LET_E, PAT_DCL_E, EXECUTE_E, TEAM_E or		*
 * RECOVER.  								*
 *									*
 * expr_defers is the list of overload expressions, and type_defers 	*
 * is the corresponding list of types.  Each member of expr_defers 	*
 * should have the type in the corresponding position in type_defers.	*
 * 									*
 * LDL and DL are reference parameters, used for error reporting.	*
 * DL is set to a list of directions to take to reach the most		*
 * promising failed type inference, and LDL is set to the		*
 * length of DL.  The first member of DL tells, by number, which	*
 * entry in the global id table to use for the first member of		*
 * expr_defers, etc.  DL ref count is bumped.  				*
 *									*
 * num_prior_successes is the number of successes on the current	*
 * declaration before this instantiation of handle_deferrals_tc was	*
 * called.  It is used for execute expressions, which should not	*
 * have multiple types.							*
 *									*
 * Return the number of successful versions of the declaration that	*
 * were issued.								*
 *									*
 * MODE is a safe pointer.  It only lives as long as this		*
 * function call.							*
 ************************************************************************/

int handle_deferrals_tc(EXPR_LIST *expr_defers, 
			LIST *type_defers, EXPR *main_e, int kind,
			MODE_TYPE *mode, INT_LIST **DL, int *LDL, 
			int num_prior_successes)
{
  GLOBAL_ID_CELL *gic;
  EXPECTATION *this_exp;
  TYPE **this_type, *expected_type, *this_expr_original_type;
  EXPR *this_expr;
  ROLE *this_expr_original_role;
  LIST *mark;
  INT_LIST *DLT;
  int num_successes, def_kind, LDLT, direction;

  /*------------------------------------------------------------*
   * Set defaults, and check for case where there has already	*
   * been an error.						*
   *------------------------------------------------------------*/

  *LDL = 0;
  *DL = NIL;
  if(local_error_occurred && kind != RECOVER) return 0;

  /*------------------------------------------------------------*
   * An execute-declaration cannot have more than one success.  *
   * If there have already been more than one, then don't 	*
   * continue to look for more.					*
   *------------------------------------------------------------*/

  if(kind == EXECUTE_E && num_prior_successes > 1) return 0;

  bump_expr(main_e);

  /*----------------------------------------------------*
   * Check if deferrals are all done.  If so, finish up *
   * with end_initial_stage.				*
   *----------------------------------------------------*/

  if(expr_defers == NIL) {
    num_successes =  end_initial_stage(kind, main_e, mode,
				       num_prior_successes, FALSE);
    drop_expr(main_e);
    return num_successes;
  }

  /*--------------------------------*
   * There are deferrals to handle. *
   *--------------------------------*/

  bump_list(expr_defers);
  bump_list(type_defers);
  num_successes = 0;
  this_expr     = expr_defers->head.expr;
  gic           = this_expr->GIC;
  def_kind      = LKIND(type_defers);
  this_type     = (def_kind == STYPE_L) ?  type_defers->head.stype
			                :  &(type_defers->head.type);
  bump_type(this_expr_original_type = this_expr->ty);
  bump_role(this_expr_original_role = this_expr->role);

# ifdef DEBUG
    if(trace_infer) {
      trace_t(450);
      trace_t(279, num_prior_successes);
      print_expr(this_expr, 1);
      fprintf(TRACE_FILE, " "); trace_ty(*this_type);
      tracenl();
    }
# endif

  /*--------------------------------------------------------------*
   * Fork on overloaded identifier types, trying all expectations *
   * in the global id table.				     	  *
   *--------------------------------------------------------------*/

  if(EKIND(this_expr) == OVERLOAD_E) {
    for(this_exp = gic->expectations, direction = 1; 
	this_exp != NULL; 
	this_exp = this_exp->next, direction++) {

      /*------------------------------------*
       * Stop as soon as there is an error. *
       *------------------------------------*/

      if(local_error_occurred && kind != RECOVER) {
	num_successes = 0;
	goto out;
      }

      /*-------------------------------------------------------------*
       * Stop immediately when an execute declaration is overloaded. *
       *-------------------------------------------------------------*/

      if(kind == EXECUTE_E && num_successes + num_prior_successes > 1) {
	goto out;
      }

      /*--------------------------------------------------------*
       * Cannot use expectation this_exp if this_exp invisible. *
       * So try the next one.					*
       *--------------------------------------------------------*/

      if(!visible_expectation(current_package_name, this_exp)) {
	continue;
      }

      /*---------------------------*
       * Try expectation this_exp. *
       *---------------------------*/

      bump_type(expected_type = copy_type(this_exp->type, 0));
      bump_list(mark = finger_new_binding_list());

#     ifdef DEBUG
	if(trace_infer) {
	  trace_t(450);
	  trace_t(280, this_expr->STR, num_successes + num_prior_successes);
	  trace_ty(expected_type); tracenl();
	}
#     endif

      if(unify_u(&expected_type, this_type, TRUE) 
	 && unify_u(&(this_expr->ty), &expected_type, TRUE)) {

	/*---------------------------------------------------------*
	 * The PRIMITIVE field of an OVERLOAD_E expr is PAT_FUN if *
	 * this is an overloaded pattern function, PAT_CONST if    *
	 * this is an overloaded pattern constant, and is -1	   *
	 * otherwise. 						   *
	 *---------------------------------------------------------*/

	int prim = this_expr->PRIMITIVE;

#       ifdef DEBUG
	  if(trace_infer) {
	    trace_t(450);
	    trace_t(281);
	  }
#       endif

	/*----------------------------------------------------*
	 * This expectation has worked out, so set up for it, *
	 * altering main_e to reflect this expectation.  Note *
	 * that the type of the expectation has already been  *
	 * installed by the successful unification above.     *
	 *----------------------------------------------------*/

	if(prim == PAT_FUN || prim == PAT_CONST) {

	  /*------------------------------------------------------*
	   * A pattern function or constant must have a type      *
	   * that is consistent with the largest pattern function *
	   * polymorphic type at this global id table node.	  *
	   * get_patfun_class_tm returns that polymorphic type.	  *
	   *------------------------------------------------------*/

	  TYPE* pf_type   = get_patfun_expectation_tm(gic, expected_type);
	  this_expr->kind = PAT_FUN_E;

#         ifdef DEBUG
	    if(trace_infer) {
	      trace_t(450);
	      trace_t(494, this_expr->STR);
	      trace_ty(pf_type);
	      tracenl();
	    }
#	  endif

	  if(!unify_u(&(this_expr->ty), &pf_type, TRUE)) {
	    goto reject_this_expectation;
	  }
	}
	else { /* Not a pattern function or constant */
	  this_expr->kind      = GLOBAL_ID_E;
	  this_expr->irregular = this_exp->irregular;
	}

	/*------------------------------------------------------*
	 * Install the role for this expectations.		*
	 *------------------------------------------------------*/

	bump_role(this_expr->role = get_role_tm(this_expr, NULL));

	/*---------------------------------------------------------*
	 * Recur on the rest of the deferral list.		   *
	 *---------------------------------------------------------*/

	num_successes += 
	  handle_deferrals_tc(expr_defers->tail,
			      type_defers->tail,
			      main_e, kind, mode, &DLT, &LDLT,
			      num_successes + num_prior_successes);

	/*-----------------------------------------------------------*
	 * Set the direction list if this is the most promising path *
	 * so far. 						     *
	 *-----------------------------------------------------------*/

	if(*LDL <= LDLT) {
	  set_list(DL, int_cons(direction, DLT));
	  *LDL = LDLT + 1;
	}
	drop_list(DLT);

#       ifdef DEBUG
	  if(trace_infer) {
	    trace_t(450);
	    trace_t(282, num_successes, num_prior_successes);
	    print_str_list_nl(*DL); tracenl();
	  }
#       endif
      } /* end if(unify...) */

      /*--------------------------------------------------------*
       * Restore the expression for the next expectation, 	*
       * which might also work. 				*
       *--------------------------------------------------------*/

    reject_this_expectation:
      this_expr->kind = OVERLOAD_E;
      drop_type(expected_type);
      undo_bindings_u(mark);
      drop_list(mark);
      SET_TYPE(this_expr->ty, this_expr_original_type);
      SET_ROLE(this_expr->role, this_expr_original_role);

    } /* end for(this_exp = ...) */

#   ifdef DEBUG
      if(trace_infer && main_context != IMPORT_CX) {
	trace_t(450);
        trace_t(283, this_expr->STR);
      }
#   endif

    goto out;

  } /* end if(EKIND(this_expr) == OVERLOAD_E) */

  /*--------------------------------------------------------------*
   * If not an overloaded id, skip -- might be a duplicate entry. *
   *--------------------------------------------------------------*/

# ifdef DEBUG
    if(trace_infer && main_context != IMPORT_CX) {
      trace_t(450);
      trace_t(284);
    }
# endif

  num_successes = handle_deferrals_tc(
			    expr_defers->tail, type_defers->tail, 
			    main_e, kind, mode, &DLT, &LDLT,
			    num_successes + num_prior_successes);
  *LDL = LDLT + 1;
  set_list(DL, int_cons(0, DLT));
  drop_list(DLT);

# ifdef DEBUG
    if(trace_infer && main_context != IMPORT_CX) {
      trace_t(450);
      trace_t(285);
      print_str_list_nl(*DL); tracenl();
    }
# endif

 out:
  drop_list(expr_defers);
  drop_list(type_defers);
  drop_type(this_expr_original_type);
  drop_role(this_expr_original_role);
  drop_expr(main_e);
  return num_successes;
}


/************************************************************************
 *			RETRACE_EXPECTS_TC				*
 ************************************************************************
 * An error has occurred in the later stages of type inference.		*
 * Retrace the steps indicated by list EDL for expression e.  These	*
 * were recorded during handle_expects_tc.				*
 *									*
 * Return true if reported error, false otherwise.			*
 ************************************************************************/

PRIVATE Boolean retrace_expects_tc(int kind, EXPR *e, EXPR *rest, LIST *EDL)
{
  EXPR *sse;
  EXPR_TAG_TYPE ekind;

  /*--------------------------------------------------------------------*
   * handle_expects_tc does not do anything under certain circumstances *
   * Just ignore e then.					        *
   *--------------------------------------------------------------------*/

  sse = skip_sames(e);
  if(e == NULL_E 
     || suppress_using_local_expectations
     || kind == EXECUTE_E
     || EKIND(sse) == PAT_DCL_E
     || EKIND(sse) == EXPAND_E
     || (kind == MANUAL_E && e->E1 == NULL)) {

    return FALSE;
  }

  /*------------------------------------------------*
   * An apply indicates a team. Handle similarly to *
   * handle_expects_tc.				    *
   *------------------------------------------------*/

  ekind = EKIND(sse);
  if(ekind == APPLY_E) {
    EXPR *new_rest;
    Boolean result;

    bump_expr(new_rest = (rest == NULL) 
			    ? sse->E2
			    : apply_expr(sse->E2, rest, sse->E2->LINE_NUM));
    result = retrace_expects_tc(kind, sse->E1, new_rest, EDL);
    drop_expr(new_rest);
    return result;
  }

  /*---------------------------------------------------------------*
   * EKIND(sse) != APPLY_E. Handle similarly to handle_expects_tc. *
   *---------------------------------------------------------------*/

  else {
    int d, this_dir;
    TYPE_LIST *expects, *q;
    LIST* roles;
    TYPE **this_type, *s;
    EXPR *sse1;
    char *name;

    sse1 = skip_sames(e->E1);
    name = new_name(sse1->STR, TRUE);
    get_local_expects(e, name, &expects, &roles);
    bump_list(expects);

    /*-------------------------------------*
     * Skip over ids without expectations. *
     *-------------------------------------*/

   if(expects == NIL) {
     return retrace_expects_tc(kind, rest, NULL, EDL);
   }

   this_type = &(sse1->ty);

   /*-----------------------------------------------------------*
    * If EDL is NIL, then this is where the problem was. 	*
    * (We have not reached the end of what handle_expects_tc	*
    * was supposed to do, but we have reached the end of the	*
    * direction list indicating successes.)			*
    *-----------------------------------------------------------*/

   if(EDL == NIL) {
     char* dname = display_name(name);
     semantic_error1(EXPECT_ASSUMPTION_ERR, dname, 0);
     err_print(THIS_DCL_ERR, dname);
     err_print_ty(*this_type);
     err_nl();
     err_print(EXPECTATIONS_ERR);
     for(q = expects; q != NIL; q = q->tail) {
       err_print_str("   %s: ", dname);
       err_print_ty(q->head.type);
       err_nl();
     }
     drop_list(expects);
     return TRUE;
   }

   /*------------------------------------------*
    * Otherwise, do a unification and move on. *
    *------------------------------------------*/

   this_dir = (int)(EDL->head.i);
   for(d = 0, q = expects; d < this_dir; d++, q = q->tail) /* Nothing */;
   bump_type(s = copy_type(q->head.type, 0));

#  ifdef DEBUG
     if(trace_infer) {
       trace_t(298, name);
       trace_ty(s); 
       tracenl();
     }
#  endif

   unify_u(&s, this_type, 0);
   drop_list(expects);
   drop_type(s);

   return retrace_expects_tc(kind, rest, NULL, EDL->tail);
 }
}


/************************************************************************
 *			REDO_DEFERRALS_TC				*
 ************************************************************************
 * An error has occurred during the later stages of type inference.	*
 * Redo the most promising steps of handle_deferrals_tc, returning to	*
 * the most promising location in deferred type inference, as		*
 * indicated by list DL.						*
 *									*
 * REST_ED and REST_TD are set to the remainders of the expr and type	*
 * deferral lists.  Other parameters are as for handle_deferrals_tc.	*
 * If REST_ED is set to NIL, then there is no useful deferral		*
 * information.  This can happen because this function reports an error.*
 *									*
 * MODE is a safe pointer.  It only lives as long as this		*
 * function call.							*
 ************************************************************************/

void redo_deferrals_tc(int kind, EXPR *e, INT_LIST *EDL, INT_LIST *DL, 
		       EXPR_LIST **rest_ed, 
		       LIST **rest_td, EXPR_LIST **rest_fe, MODE_TYPE *mode)
{
  EXPR_LIST *expr_defers;
  LIST *type_defers;
  GLOBAL_ID_CELL *gic;
  EXPECTATION *this_exp;
  TYPE **this_type, *expected_type;
  EXPR *this_expr;
  int direction, i, def_kind;

# ifdef DEBUG
    if(trace_infer) {
      trace_t(299); 
      print_str_list_nl(EDL);
      print_str_list_nl(DL);
    }
# endif

  /*------------------------------------*
   * The out parameters default to NIL. *
   *------------------------------------*/

  *rest_ed = *rest_td = *rest_fe = NIL;

  /*----------------------------------------------------*
   * Retrace steps of handle_expects_tc. If an error is *
   * found there, don't look for more errors.		*
   *----------------------------------------------------*/

  if(retrace_expects_tc(kind, e, NULL_E, EDL)) return;

  /*----------------------------------------------------*
   * If retrace_expects_tc did not report an error,	*
   * then retrace steps of handle_deferrals_tc.		*
   *----------------------------------------------------*/

  bump_list(expr_defers = unification_defer_exprs);
  bump_list(type_defers = unification_defer_types);

  while(DL != NIL && expr_defers != NIL) {
    this_expr     = expr_defers->head.expr;
    gic           = this_expr->GIC;
    def_kind      = LKIND(type_defers);
    this_type     = (def_kind == STYPE_L) ?  type_defers->head.stype 
					  :  &(type_defers->head.type);
    direction = top_i(DL);

#   ifdef DEBUG
      if(trace_infer) {
	trace_t(300, direction, this_expr->STR);
      }
#   endif

    if(EKIND(this_expr) == OVERLOAD_E) {

      /*--------------------------------------------------*
       * Scan to the correct expectation, as given by DL. *
       *--------------------------------------------------*/

      for(this_exp = gic->expectations, i = 1; 
	  this_exp != NULL && i < direction; 
	  this_exp = this_exp->next, i++) {
	/* Nothing */
      }

      /*---------------------------------------------*
       * If DL is exhausted, something is wrong.     *
       * Give up on this approach.		     *
       *---------------------------------------------*/

      if(this_exp == NULL) return;

      bump_type(expected_type = copy_type(this_exp->type, 0));

#     ifdef DEBUG
        if(trace_infer) {
	  trace_t(301);
	  print_expr(this_expr, 1);
	  fprintf(TRACE_FILE, " "); 
	  trace_ty(expected_type); 
	  tracenl();
	}
#     endif

      /*----------------------------------------------------*
       * Now that we are at the most promising expectation, *
       * unify the types and try the next item in the       *
       * deferral list.					    *
       *----------------------------------------------------*/

      unify_u(&expected_type, this_type, FALSE);
      unify_u(&expected_type, &(this_expr->ty), FALSE);
      this_expr->kind = (this_expr->PRIMITIVE == PAT_FUN ||
			 this_expr->PRIMITIVE == PAT_CONST) 
			? PAT_FUN_E : GLOBAL_ID_E;

      SET_LIST(expr_defers, expr_defers->tail);
      SET_LIST(type_defers, type_defers->tail);
      drop_type(expected_type);
    }

    else { /* if EKIND(this_expr) != OVERLOAD_E */
      SET_LIST(expr_defers, expr_defers->tail);
      SET_LIST(type_defers, type_defers->tail);
    }
  
    DL = DL->tail;

  } /* end while(DL != NULL ...) */

  *rest_ed = expr_defers;  /* Inherits ref from expr_defers */
  *rest_td = type_defers;  /* Ihherits ref from type_defers */

  /*------------------------------------------------------------*
   * If the error happened before end of handle_deferrals then	*
   * it has been reported.  If not, continue to finish the	*
   * initial stage of type inference.				*
   *------------------------------------------------------------*/

  if(expr_defers == NIL) {
    end_initial_stage(kind, e, mode, 0, TRUE);  
  }
}


/*****************************************************************
 *			PRINT_OVERLOAD_INFO			 *
 *****************************************************************
 * Print the type of each identifier in unification_defer_exprs. *
 * Expression e is the expression that has caused problems.	 *
 *****************************************************************/

PRIVATE Boolean print_overload_info_help(EXPR **ee)
{
  GLOBAL_ID_CELL *gic;
  EXPECTATION    *exp;

  EXPR* e         = *ee;
  char* spec_name = display_name(e->STR);

  if(EKIND(e) == OVERLOAD_E) {
    err_print(OVERLOAD_POSSIBILITIES_ERR, toint(e->LINE_NUM), spec_name);
    err_print_ty(e->ty);
    err_nl();
    gic = get_gic_tm(e->STR, TRUE);
    if(gic != NULL) {
      for(exp = gic->expectations; exp != NULL; exp = exp->next) {
	if(visible_expectation(current_package_name, exp)) {
	  err_print(SPACE2_ERR);
	  err_print(COLON_ERR);
	  err_print_ty(exp->type);
          err_nl();
          err_print(SPACE3_ERR);
          err_print(PACKAGE_STR_ERR, exp->package_name);
	}
      }
    }
  }
  return 0;
}

/*-------------------------------------------------------------*/

void print_overload_info(EXPR *e)
{
  scan_expr(&e, print_overload_info_help, TRUE);
}
