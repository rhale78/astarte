/*****************************************************************
 * File:    dcls/expnddcl.c
 * Purpose: Handling of pattern and expand declarations.
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
 * This file issues expand and pattern declarations.			*
 ************************************************************************/

#include <string.h>
#include "../misc/misc.h"
#include "../utils/lists.h"
#include "../utils/hash.h"
#include "../utils/strops.h"
#include "../utils/rdwrt.h"
#include "../error/error.h"
#include "../tables/tables.h"
#include "../parser/parser.h"
#include "../lexer/modes.h"
#include "../unify/unify.h"
#include "../alloc/allocate.h"
#include "../classes/classes.h"
#include "../standard/stdtypes.h"
#include "../standard/stdfuns.h"
#include "../standard/stdids.h"
#include "../exprs/expr.h"
#include "../dcls/dcls.h"
#include "../evaluate/instruc.h"
#include "../generate/generate.h"
#include "../generate/prim.h"
#include "../patmatch/patmatch.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif


/****************************************************************
 *			ISSUE_PATEXP_DCL_P			*
 ****************************************************************
 * Issue a pattern or expand declaration e. 			*
 *								*
 * mode		is the mode of the declaration.			*
 *		It is a safe pointer: it will not be kept	*
 *		longer than the lifetime of this function call.	*
 *								*
 * XREF: Called by dcl.c:issue_dcl_p to issue a pattern or	*
 * expand declaration after doing type inference.		*
 ****************************************************************/

Boolean issue_patexp_dcl_p(EXPR *e, MODE_TYPE *mode)
{
  if(!should_take(e)) return 0;

  if(EKIND(skip_sames(e)) == EXPAND_E) {
    return issue_expand_dcl_p(e, mode);
  }
  else return issue_pattern_dcl_p(e, mode);
}


/****************************************************************
 *			COPY_RULES_P				*
 ****************************************************************
 * Copy the parts in list part, declaring each for PAT_FUN_E	*
 * patfun.  Issue the declarations with kind KIND (either	*
 * PAT_DCL_E or EXPAND_E) with the given mode.  This is used	*
 * in handling  declarations of the forms			*
 *								*
 *    Pattern{mode} f = g.					*
 *    Expand{mode} f = g.					*
 *								*
 * to copy rules for g to f.					*
 *								*
 * MODE is a safe pointer: it does not live longer than this	*
 * function call.						*
 ****************************************************************/

PRIVATE void copy_rules_p(EXPR *patfun, PART *part, EXPR_TAG_TYPE kind,
			  MODE_TYPE *mode, int line)
{
  LIST *binding_mark;
  STR_LIST *who_sees;
  PART *pt;

  bump_list(who_sees = get_visible_in(mode, patfun->STR));

  for(pt = part; pt != NULL; pt = pt->next) {
    SET_EXPR(patfun->E3, pt->u.rule);
    bump_list(binding_mark = finger_new_binding_list());
    if(unify_u(&patfun->ty, &pt->ty, 1)) {
      if(!local_error_occurred) {
	drop_list(define_global_id_tm(patfun, NULL, 0, mode, 
				      0, kind, line, who_sees, NULL));
      }
    }
    undo_bindings_u(binding_mark);
    drop_list(binding_mark);
  }

  drop_list(who_sees);
}


/****************************************************************
 *			REPORT_EXPAND_AUX			*
 ****************************************************************
 * Parameter formals is the formal part of an expand or		*
 * pattern declaration of identifier fname.			*
 *								*
 * Report the types of ids in formals whose types are not	*
 * determined by the type of the function being defined.	*
 *								*
 * Parameter level is one of the following.			*
 *								*
 *   0	indicates that this call is on the top level of the	*
 *	formals expression.  So the formals expression should	*
 *	be an application of function fname.			*
 *								*
 *   1	indicates that formals is part of the formal expression,*
 *	but identifiers should not be reported until an 	*
 *	embedded application is seen.				*
 *								*
 *   2	indicates that we are inside an embedded function 	*
 *	application, and that any identifiers that are seen	*
 *	should be reported.					*
 ****************************************************************/

PRIVATE void 
report_expand_aux(char *fname, EXPR *formals, int level)
{
  EXPR_TAG_TYPE kind;

  formals = skip_sames(formals);
  if(formals == NULL) return;

  kind = EKIND(formals);
  if(is_id_p(formals) || is_ordinary_pat_var(formals)) {
    if(level == 2) {
      report_dcl_aux_p(fname, EXPAND_AUX_E, 0, formals->ty, formals->role, 
		       formals->STR, NULL, NULL_LP);
    }
  }
  else if(kind == PAIR_E) {
    report_expand_aux(fname, formals->E1, level);
    report_expand_aux(fname, formals->E2, level);
  }
  else if(kind == APPLY_E) {

    /*------------------------------------------------------------------*
     * If we are at level 0, then just process the applied		*
     * function expression at level 0 and its argument at level 1.	*
     * If we are at a level higher than 0, then process the		*
     * function expression at level 1 to prevent showing the		*
     * function identifier itself, and the argument expression		*
     * at level 2.							*
     *									*
     * Note: an application of active or active' should not be treated  *
     * as a function application for the purposes of this function.	*
     *------------------------------------------------------------------*/

    if(!is_bang(formals->E1)) {
      int left_level, right_level;
      if(level == 0) {
        left_level = 0;
        right_level = 1;
      }
      else {
        left_level = 1;
        right_level = 2;
      }
      report_expand_aux(fname, formals->E1, left_level);
      report_expand_aux(fname, formals->E2, right_level);
    }
    else report_expand_aux(fname, formals->E2, level);
  }
}


/****************************************************************
 *			ISSUE_PATTERN_DCL_P			*
 ****************************************************************
 * Issue pattern declaration e, with given mode.  e must have	*
 * tag PAT_DCL_E.						*
 *								*
 * MODE is a safe pointer: it does not live longer than this	*
 * function call.						*
 *								*
 * XREF: Called by issue_patexp_dcl_p above, and in dclutil.c	*
 * to declare an automatic pattern function.			*
 ****************************************************************/

Boolean issue_pattern_dcl_p(EXPR *e, MODE_TYPE *mode)
{
  EXPR *patfun, *e1, *e2;
  LIST *who_sees;

  e1 = skip_sames(e->E1);
  e2 = skip_sames(e->E2);
  e1->kind = PAT_FUN_E;
  bump_list(who_sees =  get_visible_in(mode, e1->STR));

  if(!local_error_occurred) {

#   ifdef DEBUG
      if(trace_exprs) {
	trace_t(47);
	print_expr(e,1);
      }
#   endif

    bump_expr(e);
    bump_expr(patfun = copy_expr(e1));

    /*--------------------------*
     * Show types if requested. *
     *--------------------------*/

    if(show_types || force_show) do_show_types(e);

    /*--------------------------*
     * Perform the declaration. *
     *--------------------------*/

    if(e->PAT_DCL_FORM == 0) {

      /*---------------------------*
       * A declaration with rules. *
       *---------------------------*/

      SET_EXPR(patfun->E3, copy_expr(e2));

      /*---------------*
       * Put in table. *
       *---------------*/

      if(!local_error_occurred) {
	drop_list(define_global_id_tm(patfun, NULL, 0, mode, 0, PAT_DCL_E,
				      e->LINE_NUM, who_sees, NULL));
      }

      /*--------------------------------------------------------*
       * Report the types of formals whose type does not show 	*
       * up in the type of the function.  e2 is the PAT_RULE_E  *
       * rule, and e2->E2 is the formal part. 			*
       *--------------------------------------------------------*/

      report_expand_aux(patfun->STR, e2->E2, 0);

      /*------------------------------------------*
       * Mark recursive uses of pattern function. *
       *------------------------------------------*/

      mark_all_pat_funs(patfun->E3);
    }

    else {

      /*------------------------------*
       * A declaration Pattern f = g. *
       *------------------------------*/

      if(EKIND(e2) != PAT_FUN_E) {
	semantic_error(RHS_NOT_PATFUN_ERR, e->E2->LINE_NUM);
      }
      else {
	PART *pt;
	EXPAND_INFO *pi;

	/*--------------------------------------------------------------*
	 * If g is not undefinedPatternFunction, then just do a normal 	*
         * copy. 							*
	 *--------------------------------------------------------------*/

	if(strcmp(e2->STR, std_id[UNDEFINEDPATTERNFUNCTION_ID]) != 0) {
	  pi = e2->GIC->expand_info;
	  if(pi == NULL || (pt = pi->patfun_rules) == NULL) {
	    semantic_error1(NO_PAT_PARTS_ERR, e2->STR, e->E2->LINE_NUM);
	  }
	  else copy_rules_p(patfun, pt, PAT_DCL_E, mode, e->LINE_NUM);
	}

	/*------------------------------------------------------*
	 * Handle undefinedPatternFunction by defining with	*
	 * a null translation.				 	*
	 *------------------------------------------------------*/

	else { /* g is undefinedPatternFunction */
	  STR_LIST *who_sees1;

	  bump_list(who_sees1 = get_visible_in(mode, patfun->STR));
	  patfun->E3 = NULL;
	  drop_list(define_global_id_tm(patfun, NULL, 0, mode, 0, 
					PAT_DCL_E, e->LINE_NUM, who_sees1,
					NULL));
	  drop_list(who_sees1);
	}
      }
    }
    drop_expr(patfun);
    drop_expr(e);
  }
  drop_list(who_sees);
  return !local_error_occurred;
}


/****************************************************************
 *			ISSUE_EXPAND_DCL_P			*
 **************************************************************** 
 * Issue expand declaration e.					*
 *								*
 * mode		is the mode of the declaration.			*
 *		It is a safe pointer: it will not be kept	*
 *		longer than the lifetime of this function call.	*
 *								*
 * XREF: Called by issue_patexp_p above.			*
 ****************************************************************/

Boolean issue_expand_dcl_p(EXPR *e, MODE_TYPE *mode)
{
  EXPR *e1, *e2, *def_e, *rules;
  STR_LIST *who_sees;
  int line;

  if(!local_error_occurred) {
    bump_expr(e);
    e1          = skip_sames(e->E1);
    e2          = skip_sames(e->E2);
    def_e       = get_applied_fun(e1, TRUE);
    def_e->kind = PAT_FUN_E;
    line        = e->LINE_NUM;
    bump_list(who_sees =  get_visible_in(mode, def_e->STR));

#   ifdef DEBUG
      if(trace_exprs) {
        trace_t(48);
        print_expr(e,1);
      }
#   endif

    /*--------------------------*
     * Show types if requested. *
     *--------------------------*/

    if(show_types || force_show) do_show_types(e);

    if(e->EXPAND_FORM == 0) {

      /*----------------------------------------*
       * A declaration Expand e1 => e2 %Expand, *
       *----------------------------------------*/

      bump_expr(rules = new_expr2(PAT_RULE_E, NULL, e->E1, line));
      bump_expr(rules->E3 = e->E2);
      rules->ETAGNAME = NULL;
      SET_EXPR(def_e->E3, copy_expr(rules));

      /*------------------------*
       * Declare the expansion. *
       *------------------------*/

      drop_list(define_global_id_tm(def_e,NULL,0,mode,0,
				    EXPAND_E,line,who_sees,NULL));

      /*--------------------------------------------------------*
       * Report the types of formals whose type does not show 	*
       * up in the type of the function.			*
       *--------------------------------------------------------*/

      report_expand_aux(def_e->STR, e1, 0);

    }
    else {

      /*-------------------------------------*
       * A declaration Expand f = g %Expand. *
       *-------------------------------------*/

      if(EKIND(e2) != GLOBAL_ID_E) {
	semantic_error(RHS_NO_EXPAND_ERR, line);
      }
      else {
	EXPAND_INFO* ei = e2->GIC->expand_info;
	PART *pt = NULL;
	if(ei != NULL) pt = ei->expand_rules;

        if(pt == NULL) {
	  semantic_error(RHS_NO_EXPAND_ERR, line);
	}
	else copy_rules_p(def_e, pt, EXPAND_E, mode, line);
      }  
    }
    drop_expr(e);
    drop_list(who_sees);
  }
  return !local_error_occurred;
}

