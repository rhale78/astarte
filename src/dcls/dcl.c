/*****************************************************************
 * File:    dcls/dcl.c
 * Purpose: Functions for issuing declarations that require
 *          doing type inference.
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
 * This file contains the top level processing of declarations that	*
 * involve type inference (let, execute, pattern, etc.).  Here, we	*
 * coordinate the phases of processing such declarations.		*
 *									*
 * Routines for showing the results of type inference are also here.	*
 ************************************************************************/

#include <string.h>
#include "../misc/misc.h"
#include "../utils/lists.h"
#include "../utils/strops.h"
#include "../error/error.h"
#include "../tables/tables.h"
#include "../parser/tokens.h"
#include "../parser/parser.h"
#include "../lexer/modes.h"
#include "../utils/hash.h"
#include "../unify/unify.h"
#include "../infer/infer.h"
#include "../alloc/allocate.h"
#include "../classes/classes.h"
#include "../exprs/expr.h"
#include "../exprs/prtexpr.h"
#include "../dcls/dcls.h"
#include "../ids/ids.h"
#include "../ids/open.h"
#include "../generate/prim.h"
#include "../patmatch/patmatch.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

PRIVATE void explain_show(void);


/************************************************************************
 *			PUBLIC VARIABLES				*
 ************************************************************************/

/************************************************************************
 *			main_id_being_defined				*
 ************************************************************************
 * main_id_being_defined is the identifier being defined while 		*
 * processing a let declaration, and is NULL while processing other	*
 * declarations.							*
 ************************************************************************/

char* main_id_being_defined = NULL;

/************************************************************************
 *			max_overloads					*
 *			main_max_overloads				*
 ************************************************************************
 * max_overloads is the maximum number of nonparametrically overloaded  *
 * definitions a single declaration can yield.				*
 *									*
 * main_max_overloads is the same, but is the value that max_overloads  *
 * is normally.  It is used when an 					*
 *    Advise overloads K one.						*
 * declaration is done, and we need to restore max_overloads afterwards.*
 ************************************************************************/

int max_overloads      = INIT_MAX_OVLDS;
int main_max_overloads = INIT_MAX_OVLDS;

/************************************************************************
 *			in_pat_dcl					*
 ************************************************************************
 * True when processing a pattern or expand declaration.  Used to       *
 * control error reporting.						*
 ************************************************************************/

Boolean in_pat_dcl;

/************************************************************************
 *			do_show_at_type_err				*
 ************************************************************************
 * True if should do a show at a type inference error.  Set in 		*
 * compiler.c.								*
 ************************************************************************/

Boolean do_show_at_type_err = FALSE;

/************************************************************************
 *			force_show					*
 ************************************************************************
 * This variable is set here to cause another module to do a show	*
 * operation.								*
 ************************************************************************/

Boolean force_show = FALSE;

/****************************************************************
 *			show_types				*
 ****************************************************************
 * show_types is						*
 *   1  if should show the full results of type inference after *
 *       the current declaration.				*
 *   2  if should show only the types of locals after the	*
 *      current	declaration.					*
 *   0  if should do neither of those things.			*
 ****************************************************************/

UBYTE show_types = 0;

/****************************************************************
 *			echo_expr				*
 ****************************************************************
 * echo_expr is							*
 *   1  if the translated form of the current declaration	*
 *      should be printed at the end of the declaration.	*
 *								*
 *   0  otherwise.						*
 ****************************************************************/

Boolean echo_expr = 0;

/****************************************************************
 *			solution_count				*
 ****************************************************************
 * solution_count is used to count the number of solutions to   *
 * type inference during the first pass.			*
 ****************************************************************/

int solution_count = 0;

/****************************************************************
 *			viable_expr				*
 ****************************************************************
 * viable_expr is set, during the first pass of type inference, *
 * to the only solution when solution_count is 1.  It is NULL   *
 * if no viable expression has been recorded.			*
 ****************************************************************/

EXPR* viable_expr = NULL;


/************************************************************************
 *			PRIVATE VARIABLES				*
 ************************************************************************/

/****************************************************************
 *			explained_show				*
 ****************************************************************
 * When the first type inference error occurs, an explanation	*
 * of how to use show is printed.  Variable explain_show	*
 * indicates whether the explanation has already been printed.  *
 ****************************************************************/

PRIVATE Boolean explained_show = FALSE;      

/****************************************************************
 *			define_overlap				*
 ****************************************************************
 * Variable define_overlap is set when it appears that a 	*
 * definition overlaps itself, so that there are conflicting 	*
 * solutions to type inference. 				* 
 ****************************************************************/

PRIVATE Boolean define_overlap;              


/************************************************************************
 *			ISSUE_DCL_P					*
 ************************************************************************
 * Issue declaration e, of kind 'kind' (either DEFINE_E, LET_E, 	*
 * PAT_DCL_E, EXECUTE_E, TEAM_E or MANUAL_E).				*
 *									*
 * (Note: EXPAND_E declarations have kind PAT_DCL_E, since they are 	*
 * handled similarly to pattern dcls.)					*
 *									*
 * MODE is the mode (options).	It is a safe pointer: it does not live  *
 * longer than this function call.					*
 *									*
 * Perform identifier handling and type inference first.		*
 *									*
 * Return TRUE on success, FALSE on failure.				*
 *									*
 * XREF: 								*
 *   Called by dclutil.c for issuing automatic pattern declarations.	*
 *									*
 *   Called by dclcnstr.c for declaring automatic constructors, 	*
 *   destructors.							*
 *									*
 *   Called by deferdcl.c for handling deferred declarations.		*
 *									*
 *   Called by parser.y for handling execute and let/define		*
 *   declarations.							*
 ************************************************************************/

Boolean issue_dcl_p(EXPR *e, int kind, MODE_TYPE *mode)
{
  Boolean ok;
  int num_successes, LDL, LEDL;
  HEAD_TYPE h;
  TYPE *t;
  INT_LIST *DL, *EDL;
  EXPR_LIST *rest_ed, *rest_fe;
  TYPE_LIST *rest_td;

  /*--------------------------------------------------------------*
   * If there has already been an error on this declaration, just *
   * skip this declaration.                                       *
   *--------------------------------------------------------------*/

  if(local_error_occurred) return FALSE;

  force_show = FALSE;
  bump_expr(e);
  in_pat_dcl = (kind == PAT_DCL_E);

# ifdef DEBUG
    if(trace && main_context != IMPORT_CX) {
      trace_t(15);
      if(kind == DEFINE_E) fprintf(TRACE_FILE, "define");
      else if(kind == LET_E) fprintf(TRACE_FILE, "let");
      else if(kind == PAT_DCL_E) fprintf(TRACE_FILE, "pattern or expand");
      else if(kind == EXECUTE_E) fprintf(TRACE_FILE, "execute\n");
      else if(kind == TEAM_E) fprintf(TRACE_FILE, "team\n");
      else if(kind == MANUAL_E) fprintf(TRACE_FILE, "description\n");
      else fprintf(TRACE_FILE, "%d\n", kind);
      if(kind == DEFINE_E || kind == LET_E || kind == PAT_DCL_E) {
        fprintf(TRACE_FILE, ", id %s\n", skip_sames(e->E1)->STR);
      }
      if(trace_exprs) {
        fprintf(TRACE_FILE, "Expr =\n\n");
        print_expr(e,1);
      }
    }
# endif

  /*----------------------------------------------------------------*
   * Check for Let{copy}.  If this is a Let{copy} dcl, then the     *
   * left-hand side and right-hand side must each be an identifier. *
   *----------------------------------------------------------------*/

  if(has_mode(mode, COPY_MODE) && (kind == DEFINE_E || kind == LET_E)) {
    if(EKIND(skip_sames(e->E1)) != IDENTIFIER_E ||
       EKIND(skip_sames(e->E2)) != IDENTIFIER_E) {
      semantic_error(BAD_LET_COPY_ERR, 0);
      goto err_out;
    }
  }

  /*------------------------------------------------------------*
   * For a let, store the name of the identifier being defined	*
   * so that we can give a better error message this identifier	*
   * shows up as unknown in the body.  (That would appear to	*
   * be a recursive definition using Let.)			*
   *------------------------------------------------------------*/

  main_id_being_defined = (kind == LET_E) ? get_defined_id(e)->STR : NULL;

  /*------------------------------------------------------------*
   * Handle identifiers in e. This involves distinguishing      *
   * local from global identifiers, and replacing each copy of  *
   * a local identifier by the same node. It also handles open  *
   * conditionals, so that ids that are really the same are     *
   * recognized as the same.                                    *
   *------------------------------------------------------------*/

  if(kind == DEFINE_E || kind == LET_E || kind == EXECUTE_E
     || kind == PAT_DCL_E || kind == TEAM_E || kind == MANUAL_E) {

#   ifdef DEBUG
      if(trace) {trace_t(138); trace_t(16);}
#   endif

    do_process_ids(e, kind);

#   ifdef DEBUG
      if(trace_ids || trace_infer) {
         trace_t(17);
         print_expr(e, 0);
       }
#   endif
  }

  if(local_error_occurred) goto err_out;

  /*-----------------------------------------------------------*
   * Perform initial type inference, deferring overloaded ids. *
   * Only one solution is possible from this phase.            *
   *-----------------------------------------------------------*/

# ifdef DEBUG
    if(trace) {trace_t(138); trace_t(18);}
# endif

  unification_defer_types =
    unification_defer_exprs =
    unification_defer_holds = NIL;
  ok = infer_type_tc(e);

# ifdef DEBUG
    if(trace) {
      if(!ok) {trace_t(138); trace_t(19);}
    }
# endif

  /*------------------------------------------*
   * If didn't pass main inference, quit now. *
   *------------------------------------------*/

  if(!ok) {

    /*----------------------------------------------------*
     * If, for some strange reason, nothing was reported, *
     * report an error now.                               *
     *----------------------------------------------------*/

    if(!local_error_occurred) syntax_error(TYPE_ERR, 0);

    /*-------------------------------------------*
     * If a show is called for, do the show now. *
     *-------------------------------------------*/

    if(show_types || do_show_at_type_err) {do_show_types(e);}
    else explain_show();
    if(echo_expr) do_echo_expr(e, 0);
    goto err_out;
  }

# ifdef DEBUG
    if(trace_infer && main_context != IMPORT_CX) {
      tracenl();
      trace_t(138);
      trace_t(20);
      print_expr(e,1);
    }
# endif

  /*--------------------------------------------------------------------*
   * Complete type inference and issue the declaration.  This is all    *
   * done within handle_expects_tc (and functions that it calls,        *
   * directly or indirectly) since we need to use recursion to          *
   * simulate backtracking. It eventually calls end_initial_stage,      *
   * below, when type inference is complete.                            *
   * DL is a direction-list for deferals, and EDL is a direction-list   *
   * for choosing local expectations.  They indicate the most promising *
   * direction to go in case none of the options works out.             *
   * LDL is set to the length of DL, and LEDL is set to the length of   *
   * EDL.                                                               *
   *                                                                    *
   * For define, let, pattern and team declarations, a first pass is    *
   * done in which no declarations are issued, but types are tried.     *
   * The purpose of this pass is to determine whether any solutions     *
   * contribute to expectations.  If so, then solutions that do not     *
   * contribute to expectations should be ignored.                      *
   * For a team, only solutions that contribute to expectations in as   *
   * many components as possible are kept.                              *
   *                                                                    *
   * If there is only one solution, it will be put into viable_expr     *
   * during the first pass, thus obviating a second pass.  If there is  *
   * more than one soltion, it will be necessary to redo the            *
   * backtracking to find all solutions again.                          *
   *--------------------------------------------------------------------*/

# ifdef DEBUG
    if(trace) {trace_t(138); trace_t(21);}
# endif

  num_successes = 0;
  define_overlap = FALSE;
  DL = EDL = NIL;

  /*--------------------------------------------------------------------*
   * For a let, define, patttern function dcl or team, find out if the	*
   * declaration contributes to any expectations.  That involves doing	*
   * type inference with kind = FIRST_PASS.                      	*
   *									*
   * handle_expects_tc will set viable_expr, expectation_contributions  *
   * and solution_count.                                        	*
   *--------------------------------------------------------------------*/

  if(kind == LET_E || kind == DEFINE_E ||
     kind == PAT_DCL_E || kind == TEAM_E) {

    viable_expr               = NULL;
    expectation_contributions = NIL;
    solution_count            = 0;
    self_overlap_table        = NULL;
    if(ok) {
      handle_expects_tc(e, e, NULL_E, FIRST_PASS, mode,
			&EDL, &LEDL, &DL, &LDL);
#     ifdef DEBUG
	if(trace_infer) {
	  trace_t(138);
	  trace_t(481,solution_count, viable_expr);
	  print_str_list_nl(expectation_contributions);
	}
#     endif

      num_successes = solution_count;
      if(num_successes == 0) ok = FALSE;
      check_for_self_overlaps(e, mode);
    }
  }

# ifdef DEBUG
    if(trace) {
      trace_t(138);
      if(kind != EXECUTE_E) trace_t(22, num_successes, viable_expr);
    }
# endif

  /*-------------------------------------------------*
   * Now either do a second pass or use viable_expr. *
   *-------------------------------------------------*/

  if(ok) {
    SET_LIST(DL, NIL);
    SET_LIST(EDL, NIL);

    /*---------------------------------------------------------------*
     * If a single viable solution was found, use that solution.     *
     * In this case, we must do role checking here, since it was     *
     * not done on the first pass, and there will be no second pass. *
     * Also, the role obtained for the id(s) being defined must be   *
     * copied into viable_expr, since viable_expr is a copy of e.    *
     *---------------------------------------------------------------*/

    if(num_successes == 1 && viable_expr != NULL) {

#     ifdef DEBUG
	if(trace_exprs) {
	  tracenl();
	  trace_t(138);
	  trace_t(23);
	  if(trace_exprs > 1) print_expr(viable_expr, 0);
	}
#     endif

      do_role_check(kind, e);
      copy_main_roles(e, viable_expr);
      ok = final_issue_dcl_p(viable_expr, kind, mode, 0);
    }

    /*------------------------------------------------------------*
     * If multiple solutions were found, then redo type inference *
     * with the actual kind.                                      *
     *------------------------------------------------------------*/

    else {

#     ifdef DEBUG
	if(trace_infer) {
	  trace_t(138);
	  trace_t(482);
	}
#     endif

      num_successes = handle_expects_tc(e, e, NULL_E, kind, mode,
					&EDL, &LEDL, &DL, &LDL);
    }
  } /* end if(ok) */

  SET_EXPR(viable_expr, NULL);

# ifdef DEBUG
    if(trace) {trace_t(138); trace_t(24, num_successes);}
# endif

  /*---------------------------------------------------------------------*
   * If there were no successes, return to the most promising collection *
   * of bindings, and report an error there.                             *
   *---------------------------------------------------------------------*/

  if(num_successes == 0) {
    redo_deferrals_tc(kind, e, EDL, DL, &rest_ed, &rest_td, &rest_fe, 
		      mode);
    if(rest_ed != NIL) {
      EXPR *r;
      LIST *mark;
      h = rest_td->head;
      t = LKIND(rest_td) == TYPE_L ? h.type : *(h.stype);
      bump_list(mark = finger_new_binding_list());
      r = rest_ed->head.expr;
      if((EKIND(r) == PAT_FUN_E || r->PRIMITIVE == PAT_FUN 
	  || r->PRIMITIVE == PAT_CONST) &&
	 UNIFY(r->ty, t, 1)) {
	no_pat_parts_error(r);
      }
      else {
        undo_bindings_u(mark);
        type_error(r, t);
      }
      drop_list(mark);
    } /* end if(rest_ed != NIL) */

    if(!local_error_occurred) semantic_error(TYPE_ERR, 0);
    drop_list(rest_ed);
    drop_list(rest_td);
    drop_list(rest_fe);
    if(show_types || do_show_at_type_err) {do_show_types(e);}
    else explain_show();
    if(echo_expr) do_echo_expr(e, 1);
    ok = FALSE;
  } /* end if(num_successes == 0) */

  /*-------------------------------------------------------------*
   * An execute expression should not have unresolved overloads. *
   *-------------------------------------------------------------*/

  if(kind == EXECUTE_E && num_successes > 1 || define_overlap) {
    if(kind == EXECUTE_E) semantic_error(EXECUTE_OVLD_ERR, 0);
    print_overload_info(e);
    ok = FALSE;
  }

  /*-----------------------------------*
   * Show the expression if requested. *
   *-----------------------------------*/

  if(echo_expr) do_echo_expr(e, 1);

  /*----------------------*
   * Clean up and return. *
   *----------------------*/

  drop_list(DL);
  drop_list(EDL);
  drop_list(unification_defer_types);
  drop_list(unification_defer_exprs);
  drop_list(unification_defer_holds);
  drop_expr(e);
  unification_defer_exprs = 
    unification_defer_types = 
    unification_defer_holds = NIL; /* dropped above*/
  main_id_being_defined = NULL;
  return ok;

err_out:
  
# ifdef DEBUG
    if(trace) {trace_t(138); trace_t(26);}
# endif

  drop_expr(e);
  ok = FALSE;
  main_id_being_defined = NULL;
  return ok;
}


/************************************************************************
 *			END_INITIAL_STAGE				*
 ************************************************************************
 * Complete the initial stage of declaring main_e.  kind is the kind	*
 * of declaration, as in issue_dcl_p, mode is the mode.			*
 *									*
 * num_prior_successes is the number of successful solutions to type    *
 * inference that have been found before this one.			*
 *									*
 * report_errs is true if errors should be reported in the listing.	*
 * If report_errs is false, errors cause this solution to be ignored,	*
 * without comment.							*
 *									*
 * The return value is the number of successful declarations made by	*
 * this call to end_inital_stage.					*
 *									*
 * The operations to be performed are as follows.  For some kinds of	*
 * declarations, type inference makes two passes over the full process. *
 * Some of the following are only done on one of the passes.  For	*
 * other kinds of declarations, only one pass is made.			*
 *    									*
 *   1. Check roles. (second pass or only pass)				*
 *									*
 *   2. Do pattern match and expand substitutions.  (all passes) 	*
 *									*
 *   3. Perform exhaustiveness checks for choose-matching and 		*
 *      definition by cases.  (first pass or only pass)			*
 *									*
 *   4. Assign scopes to local identifiers.  That is, determine which	*
 *      environments they will be allocated into.  (all passes)		*
 *									*
 *   5. Not all definitions will be taken.  (If any of the solutions	*
 *      contributes to an expectation, then solutions that do not	*
 *      contribute to an expectation are skipped.)  Record information	*
 *      to see the full picture (only in the first pass) so that, in	*
 *      the second pass it will be possible to decide what to take and	*
 *      what to skip.							*
 *									*
 *   6. Issue the declaration by generating code, making table entries,	*
 *      etc. (second pass or only pass)					*
 *									*
 *									*
 * MODE is a safe pointer.  It only lives as long as this		*
 * function call.							*
 ************************************************************************/

int end_initial_stage(int kind, EXPR *main_e, MODE_TYPE *mode, 
		      int num_prior_successes, Boolean report_errs)
{
  EXPR *new_e;
  int result;
  EXPR_TAG_TYPE e_kind;

# ifdef DEBUG
    if(trace) {
      if(!report_errs) {
	trace_t(450);
        trace_t(286, kind);
	if(trace_exprs) {
	  trace_t(287); 
	  print_expr(main_e, 0);
	}
      }
      else {
	trace_t(288, toint(kind));
      }
    }
# endif

  result = 0;
  e_kind = EKIND(skip_sames(main_e));

  /*-------------------------------------------------------------*
   * If we are trying to recover from an error, just return now. *
   *-------------------------------------------------------------*/

  if(kind == RECOVER) return 1;

  bump_expr(main_e);
  new_e = NULL;
  copy_choose_matching_lists = NIL;

  /*--------------------------------------------------------------------*
   * Operations from here on tend to do destructive operations on the 	*
   * expression and its types.  Copy everything, so that this branch	*
   * of handling overloads will not affect other branches. 		*
   *--------------------------------------------------------------------*/

# ifdef DEBUG
    if(trace_exprs > 1) {
      trace_t(290);
      print_expr(main_e, 0);
    }
# endif

  bump_expr(new_e = copy_expr_and_choose_matching_lists(main_e));

# ifdef DEBUG
    if(trace_pm > 1 || trace_exprs > 1) {
      trace_t(291);
      print_expr(new_e, 0);
    }
# endif

  /*--------------------------------------------------------------*
   * Check roles.  Only check when not in first pass, and when we *
   * should take this declaration (if a define, let, etc.) 	  *
   *--------------------------------------------------------------*/

  if(kind == EXECUTE_E ||
     ((kind == DEFINE_E || kind == LET_E ||
       kind == PAT_DCL_E || kind == TEAM_E) && should_take(new_e))) {
    do_role_check(kind, new_e);
  }

  /*------------------------------------------------------------*
   * Do pattern match substitutions and expansions.		*
   * Don't do the substitutions if this is an imported package,	*
   * and not generating code.					*
   *------------------------------------------------------------*/

  if(main_context != IMPORT_CX && e_kind != PAT_DCL_E) {
    SET_EXPR(new_e, translate_matches_pm(new_e, report_errs, 0));
  }

# ifdef DEBUG
    if(trace_pm > 1 || trace_exprs) {
      if(new_e != NULL) {
	trace_t(292);
        print_expr(new_e, 0);
      }
      else trace_t(293);
    }
# endif

  if(local_error_occurred || new_e == NULL) goto out;

  /*---------------------------------------------------------------*
   * Check for completeness of pattern matching, but only on first *
   * pass, and only if not importing. 				   *
   *---------------------------------------------------------------*/

  if((kind == FIRST_PASS || kind == EXECUTE_E) 
     && main_context != IMPORT_CX
     && !should_suppress_warning(err_flags.suppress_pm_incomplete_warnings)) {
    check_pm_completeness();
  }

  /*-------------------------------------------------------*
   * Assign scopes to local identifiers, if not importing. *
   *-------------------------------------------------------*/

  if(main_context != IMPORT_CX && kind != PAT_DCL_E && kind != RECOVER) {
    do_process_scopes(new_e);
    pop_scope_to_tm(NIL);

#   ifdef DEBUG
      if(trace_ids) {
	trace_t(72);
	print_expr(new_e, 0);
      }
#   endif

    if(local_error_occurred) goto out;
  }

  /*------------------------*
   * Issue the declaration. *
   *------------------------*/

  result = final_issue_dcl_p(new_e, kind, mode, num_prior_successes);

 out:
  drop_expr(new_e);
  drop_expr(main_e);
  SET_LIST(copy_choose_matching_lists, NIL);
  return result;
}


/************************************************************************
 *			FINAL_ISSUE_DCL_P				*
 ************************************************************************
 * Issue declaration e, of kind kind and mode MODE, as for		*
 * issue_dcl_p, but assuming that type inference is completed.		*
 *									*
 * If this is the first pass, then complete the first pass, setting	*
 * viable_expr, solution_count and expectation_contributions		*
 * appropriately.							*
 *									*
 * If this is not the first pass, then perform the declaration.		*
 * This same declaration previously lead to num_prior_successes 	*
 * successful definitions, if this is a define or let declaration.	*
 * Return the number of successes.					*
 *									*
 * MODE is a safe pointer.  It only lives as long as this		*
 * function call.							*
 *									*
 * XREF: Called by infer/defer.c at the end of type inference.  That    *
 * is where type inference really wants to return to issue_dcl_p.	*
 * Unfortunately, we are inside a recursion that simulates backtracking	*
 * over all possible overloadings of identifiers, so it is not 		*
 * possible to return yet without destroying the backtrack stack.  So	*
 * defer.c gets back by calling final_issue_dcl_p.			*
 ************************************************************************/

int final_issue_dcl_p(EXPR *e, int kind, MODE_TYPE *mode, 
		      int num_prior_successes)
{
  int num_successes = 0;

  /*-----------------------*
   * Check for first pass. *
   *-----------------------*/

  if(kind == FIRST_PASS) {
    do_first_pass_tm(e);
    return 1;
  }

  /*-------------------------------------*
   * Second pass: Issue the declaration. *
   *-------------------------------------*/

# ifdef DEBUG
    if(trace) trace_t(30, kind);
# endif

  if(kind == DEFINE_E || kind == LET_E) {
    if(num_prior_successes > 0 && has_mode(mode, ASSUME_MODE)) {
      semantic_error(ASSUME_OVERLOAD_ERR, 0);
    }
    if(num_prior_successes == max_overloads) {
      semantic_error1(TOO_MANY_OVLDS_ERR, skip_sames(e->E1)->STR, 0);
      return 0;
    }
    num_successes = issue_define_p(e, mode, TRUE);
    if(num_successes == 0 && num_prior_successes > 0
       && local_error_occurred) define_overlap = TRUE;
  }

  else if(kind == PAT_DCL_E) num_successes = issue_patexp_dcl_p(e, mode);
  else if(kind == EXECUTE_E) num_successes = do_execute_p(e, mode);
  else if(kind == TEAM_E)    num_successes = issue_team_p(e);
  else if(kind == MANUAL_E)  num_successes = issue_description_p(e, mode);

  else {
    die(20);
  }

# ifdef DEBUG
    if(trace) trace_t(31);
# endif

  return num_successes;
}


/****************************************************************
 *			DO_ECHO_EXPR				*
 ****************************************************************
 * Print expression e in pretty format in the listing.		*
 * Also set echo_expr to 0 to prevent more than one echo.	*
 *								*
 * Parameter context should be 0 if this is called before	*
 * pattern match translation and 1 if called after pattern	*
 * match translation.						*
 ****************************************************************/

void do_echo_expr(EXPR *e, int context)
{
  FILE *f;

  f = LISTING_FILE;
  if(f == NULL) f = ERR_FILE;
  if(f == NULL) return;
  
  fprintf(f, "\n\nInternal form of expression:\n\n");
  pretty_print_expr(f, e, 0, context);
  fnl(f);
  echo_expr = FALSE;
}


/****************************************************************
 *			DO_SHOW_TYPES				*
 ****************************************************************
 * If show_types is not 2:					*
 *								*
 * Print the identifiers in e with their types and behaviors.   *
 * Also print assumptions about the identifiers in e, and 	*
 * expectations for the identifier being defined.		*
 *								*
 * If show_types is 2:						*
 *								*
 * Show the types of the locals where they are defined.  Do	*
 * not show other information.					*
 ****************************************************************/

PRIVATE LIST *assumed_ids;

void do_show_types(EXPR *e)
{
  PRINT_TYPE_CONTROL ctl;
  GLOBAL_ID_CELL *gic;
  EXPR_TAG_TYPE kind;
  Boolean is_defn;

  char* name     = NULL;
  char* spec_name = NULL;

  /*--------------------------------*
   * Leading stuff for a full show. *
   *--------------------------------*/

  err_nl(); err_nl();
  if(show_types != 2) {

    /*--------------------*
     * Print the heading. *
     *--------------------*/

    load_error_strings();
    err_print(DASHES_ERR);
    err_nl();
    err_print(SHOW_RES_ERR);
    err_nl();

    /*-----------------------------------------------------------------*
     * Show the expectations that are visible in the currrent package. *
     *-----------------------------------------------------------------*/

    kind    = EKIND(e);
    is_defn = is_definition_kind(kind);

    /*------------------------------------------------*
     * Print the expectation header for a definition. *
     *------------------------------------------------*/

    if(is_defn) {
      name      = get_defined_id(e)->STR;
      spec_name = display_name(name);
      gic       = get_gic_tm(name, TRUE);
      if(gic != NULL && gic->expectations != NULL) {
	EXPECTATION *exp;
	err_print(EXPECTATIONS_FOR_ERR, spec_name);
	for(exp = gic->expectations; exp != NULL; exp = exp->next) {
	  if(visible_expectation(current_package_name, exp)) {
	    err_print(SPACE2_STR_ERR, spec_name);
	    err_print_ty_with_constraints_indented(exp->type, 
						   strlen(spec_name) + 5);
	    err_print(PACKAGE_LINE_ERR,
		      exp->package_name, toint(exp->line_no));
	  }
	}
	err_nl();
      }
      else err_print(NO_EXPECTATIONS_FOR_ERR, spec_name);
    } /* end if(is_defn) */

    /*--------------------------------------*
     * Show the local expectations, if any. *
     *--------------------------------------*/

    if(is_defn) {
      LIST *local_exp, *p;
      bump_list(local_exp = local_expectation_list_tm(name, FALSE));
      if(local_exp != NIL) {
	err_print(EXPECTATIONS_THIS_PACKAGE_ERR);
	for(p = local_exp; p != NIL; p = p->tail) {
	  err_print(SPACE2_STR_ERR, spec_name);
	  err_print_ty_with_constraints_indented(p->head.type,
						 strlen(spec_name) + 5);
	  err_nl();
	}
	err_nl();
      }
      drop_list(local_exp);
    } /* end if(is_defn) */

    err_print(INFER_RESULTS_ERR);
  }

  /*------------------------------------------------------------*
   * Print the types of identifiers. All identifiers in this	*
   * and subsequent sections of the show are printed using the	*
   * same variable name tables.					*
   *------------------------------------------------------------*/

# ifdef DEBUG
    if(trace_exprs) print_expr(e, 0);
# endif

  begin_printing_types(ERR_FILE, &ctl);
  assumed_ids = NIL;
  do_show_types1(e, &ctl, show_types);

  /*----------------------------------*
   * Trailing stuff, for a full show. *
   *----------------------------------*/

  if(show_types != 2) {

    /*--------------------------------------*
     * Show the assumptions that were used. *
     *--------------------------------------*/

    if(assumed_ids != NIL) {
      LIST *p;
      char *p_name;
      err_print(ASSUMPTIONS_USED_ERR);
      for(p = assumed_ids; p != NIL; p = p->tail) {
	char *spec_pname;
	p_name     = p->head.str;
	spec_pname = display_name(p_name);
	err_print(SPACE2_STR_ERR, spec_pname);
	err_print_ty_with_constraints_indented(assumed_rtype_tm(p_name).type,
					       strlen(spec_pname) + 5);
	err_nl();
      }
      drop_list(assumed_ids);
    }

    /*-----------------------------------*
     * Show the defaults that were done. *
     *-----------------------------------*/

    if(default_bindings != NULL) {
      EXPR_LIST *p;
      err_print(DEFAULTS_DONE_ERR);
      for(p = default_bindings; p != NULL; p = p->tail) {
	EXPR* this_expr = p->head.expr;
	err_print_str("%4d ", this_expr->LINE_NUM);
	err_short_print_expr(this_expr);
	err_nl();
      }
    }

    /*--------------------*
     * Print the trailer. *
     *--------------------*/

    err_print(DASHES_ERR);
    err_nl();
    err_nl();
  }

  /*-----------------*
   * Finish up types *
   *-----------------*/

  end_printing_types(&ctl);
}


/****************************************************************
 *			SHOW_AVAIL_TYPES			*
 ****************************************************************
 * show_avail_types(id) prints the available types for 		*
 * global-id id.						*
 ****************************************************************/

PRIVATE void show_avail_types(EXPR *id)
{
  EXPECTATION *p;
  GLOBAL_ID_CELL* gic = get_gic_tm(id->STR, TRUE);
  PRINT_TYPE_CONTROL c;

  if(gic != NULL) {
    err_print(AVAIL_TYPES_ERR, display_name(id->STR));
    for(p = gic->expectations; p != NULL; p = p->next) {
      err_print(SPACE3_ERR); err_print(SPACE2_ERR);
      begin_printing_alt_types(ERR_FILE, &c);
      err_print_rt1_with_constraints_indented(p->type, get_role_tm(id, NULL), 
					      &c, 6);
      end_printing_types(&c);
      err_nl();
    }
  }
}


/*****************************************************************
 *			DO_SHOW_TYPES1				 *
 *****************************************************************
 * When show_mode < 2:				 	 	 *
 *								 *
 * do_show_types1 prints the types of identifiers, including the *
 * available types of global ids, in e.  It also puts all 	 *
 * assumptions that were used in assumed_ids. Types are all 	 *
 * shown in a common name space, controled by ctl.	 	 *
 *								 *
 * When show_mode == 2:					 	 *
 *								 *
 * do_show_types1 prints the types of local identifiers where	 *
 * they are defined.						 *
 *								 *
 * When show_mode == 3:						 *
 *								 *
 * do_show_types1 prints the types of pattern variables only.	 *
 *								 *
 * XREF: called by do_show_types here and by patmatch/translt.c	 *
 * to show types in restrictive pattern rule errors.		 *
 *****************************************************************/

void do_show_types1(EXPR *e, PRINT_TYPE_CONTROL *ctl, int show_mode)
{
  int line;

  for(;;) {
    if(e == NULL) return;
    line = e->LINE_NUM;

  cont_from_same:
    switch(EKIND(e)) {
      case SPECIAL_E:
      case UNKNOWN_ID_E:
      case BAD_E:
	return;

      case CONST_E:
	if(e->SCOPE != NAT_CONST && e->SCOPE != REAL_CONST) return;
        /* No break - fall through to next case. */

      case IDENTIFIER_E:
      case LOCAL_ID_E:
      case GLOBAL_ID_E:
      case OVERLOAD_E:
      case PAT_VAR_E:
      case PAT_FUN_E:

	/*------------------------------------------------------------*
         * Show this id and its type, but not if the id starts with a *
	 * HIDE_CHAR, since then it is an internally generated id.    *
         * Only do the show if show_mode is not 2, and, if show_mode  *
         * is 3, only do if a pattern variable.		      	      *
	 *------------------------------------------------------------*/

	if(show_mode != 2 && (show_mode != 3 || EKIND(e) == PAT_VAR_E)) {
          char* spec_name;
	  char* name = e->STR; 
	  if(name == NULL) name = HIDE_CHAR_STR "?";
	  spec_name = display_name(name);
	  if(e->ty != NULL && spec_name[0] != HIDE_CHAR) {
	    if(show_types == 2) err_print_str("-->");
	    err_print(LINE_STR_ERR, line, spec_name);
	    err_print_rt1_with_constraints_indented
	      (e->ty, e->role, ctl, strlen(spec_name) + 8);
	    err_nl();
	  }

	  if(show_mode < 2) {

	    /*------------------------------------------*
	     * If a global, show the available types.   *
	     * This is suppressed when show_types is 2, *
	     * to suppress printing of avail types for  *
	     * id being defined at a let.		*
	     *------------------------------------------*/

	    if(show_types != 2 && EKIND(e) == GLOBAL_ID_E) show_avail_types(e);

	    /*------------------------------------------------*
	     * If an assumption was used, add to assumed_ids. *
	     *------------------------------------------------*/

	    if(assumed_rtype_tm(name).type != NULL
	       && !str_member(name, assumed_ids)) {
	      SET_LIST(assumed_ids, str_cons(name, assumed_ids));
	    }
	  }
        }
	return;

      case PAT_RULE_E: 
	do_show_types1(e->E2, ctl, show_mode);
	do_show_types1(e->E3, ctl, show_mode);
	/* No break -- fall through */

      case SINGLE_E:
      case LAZY_LIST_E:
      case RECUR_E:
      case MANUAL_E:
      case EXECUTE_E:
      case OPEN_E:
	e = e->E1;
	break;

      case SAME_E:
        e = e->E1;
        if(e == NULL) return;
        goto cont_from_same;

      case LET_E:
      case DEFINE_E:
        do_show_types1(e->E1, ctl, 1);
	e = e->E2;
	break;

      case MATCH_E:
      case FUNCTION_E:
        do_show_types1(e->E1, ctl, (show_mode == 2) ? 3 : show_mode);
	e = e->E2;
	break;

      case APPLY_E:
      case SEQUENCE_E:
      case STREAM_E:
      case TEST_E:
      case AWAIT_E:
      case PAIR_E:
      case LOOP_E:
      case LAZY_BOOL_E:
      case TRAP_E:
      case WHERE_E:
      case PAT_DCL_E:
      case EXPAND_E:
        do_show_types1(e->E1, ctl, show_mode);
	e = e->E2;
	break;

      case IF_E:
      case TRY_E:
      case FOR_E:
        do_show_types1(e->E1, ctl, show_mode);
        do_show_types1(e->E2, ctl, show_mode);
	e = e->E3;
	break;
    } /* end switch */
  } /* end for */
}


/****************************************************************
 *			EXPLAIN_SHOW				*
 ****************************************************************
 * Print a message explaining that programmer can use show 	*
 * option to see results of inference.				*
 ****************************************************************/

PRIVATE void explain_show(void)
{
  if(!explained_show) {
    err_print(USE_SHOW_ERR);
  }
  explained_show = TRUE;
}
