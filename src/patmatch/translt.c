/******************************************************************
 * File:    patmatch/translt.c
 * Purpose: Perform pattern match translation and expansion
 *          on an expr
 * Author:  Karl Abrahamson
 ******************************************************************/

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
 * This file contains functions that perform translations of		*
 * expressions to handle pattern matching and expansion.		*
 ************************************************************************/

#include "../misc/misc.h"
#include "../utils/hash.h"
#include "../utils/lists.h"
#include "../utils/strops.h"
#include "../alloc/allocate.h"
#include "../classes/classes.h"
#include "../exprs/expr.h"
#include "../exprs/prtexpr.h"
#include "../evaluate/instruc.h"
#include "../tables/tables.h"
#include "../standard/stdtypes.h"
#include "../standard/stdids.h"
#include "../patmatch/patmatch.h"
#include "../dcls/dcls.h"
#include "../error/error.h"
#include "../ids/open.h"
#include "../unify/unify.h"
#include "../infer/infer.h"
#include "../generate/generate.h"
#include "../generate/prim.h"
#include "../generate/selmod.h"
#include "../parser/parser.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

PRIVATE EXPR* translate_patmatch_test_pm(EXPR *pattern, EXPR *image, int unif,
				 int line, Boolean report_errs, int depth);
PRIVATE EXPR* translate_pair_match(EXPR *pattern, EXPR *image, Boolean open,
				   int unif, Boolean lazy, int line,
				   Boolean report_errs, int depth);
PRIVATE EXPR* translate_apply_match(EXPR *pattern, EXPR *image, Boolean open,
				    int unif, Boolean lazy, Boolean use_stack,
				    int line,
				    Boolean report_errs, int depth);
PRIVATE EXPR* do_patmatch_subst
	(EXPR *formals, EXPR *sspattern, EXPR *translation,
 	 EXPR *image, EXPR *preamble, int unif,
	 Boolean lazy, int extra,
	 int line, Boolean report_errs, int depth, char *pack_name);


/************************************************************************
 *			VARIABLES					*
 ************************************************************************/

/************************************************************************
 *			should_show_patmatch_subst			*
 *			always_should_show_patmatch_subst		*
 ************************************************************************
 * should_show_patmatch_subst is true if each substitution that is done *
 * in expansion and pattern match substitution should be shown in the	*
 * listing.								*
 *									*
 * always_should_show_patmatch_subst is the value that 			*
 * should_show_patmatch_subst reverts to at the beginning of a		*
 * declaration.								*
 ************************************************************************/

Boolean should_show_patmatch_subst = 0;
Boolean always_should_show_patmatch_subst = 0;

/************************************************************************
 *			warn_on_compare					*
 ************************************************************************
 * warn_on_compare is TRUE if the pattern match translator should print *
 * a warning when it does an equality comparison.			*
 ************************************************************************/

Boolean warn_on_compare = FALSE;


/************************************************************************
 *			pm_loop_broken					*
 *			max_patmatch_subst_depth			*
 ************************************************************************
 * max_patmatch_subst_depth is the depth of substitution at which the   *
 * translator will break off translating, and set pm_loop_broken true.  *
 *									*
 * pm_loop_broken is set TRUE when the pattern match translator breaks  *
 * off translating because the depth of substitutions has become too    *
 * high.								*
 ************************************************************************/

PRIVATE Boolean pm_loop_broken = FALSE;
LONG max_patmatch_subst_depth  = INIT_MAX_PATMATCH_SUBST_DEPTH;


/************************************************************************
 *			SHOW_PATMATCH_SUBST				*
 ************************************************************************
 * Show in the listing file a message that pattern is being replaced 	*
 * using rule formals => translation.  Parameter image is the target 	*
 * value.  								*
 *									*
 * Parameters lazy and unif tell the lazy and unification modes, and 	*
 * line is the line to report that translation at.			*
 ************************************************************************/

PRIVATE void 
show_patmatch_subst(EXPR *formals, EXPR *pattern, EXPR *translation, 
		    EXPR *image, Boolean unif, Boolean lazy, 
		    int line, char *pack_name)
{
  FILE* f = LISTING_FILE;
  if(f == NULL) f = ERR_FILE;
  if(f == NULL) return;

  fprintf(f, 
          "\n--------------------------------------------------\n"
          "Performing pattern match substitution at line %d\n"
          "Pattern:\n", 
          line);
  pretty_print_expr(f, pattern, 0, 0);
  fprintf(f, "\nTarget:\n");
  pretty_print_expr(f, image, 0, 0);
  fprintf(f, "\nRule (from package %s, line %d):\n", 
	  pack_name, formals->LINE_NUM);
  pretty_print_pat_rule(f, formals, translation, 1, 0);
}


/************************************************************************
 *			SHOW_EXPAND_SUBST				*
 ************************************************************************
 * Show in the listing file a message that expression src is being 	*
 * replaced by translation (after substitutions into translation).  	*
 * Parameter line to report that translation at.			*
 ************************************************************************/

PRIVATE void 
show_expand_subst(EXPR *src, EXPR *translation, int line, char *pack_name)
{
  FILE* f = LISTING_FILE;
  if(f == NULL) f = ERR_FILE;
  if(f == NULL) return;

  fprintf(f, 
          "\n-------------------------------------------\n"
	  "Performing expand substitution at line %d\n"
	  "Expression:\n",
	  line);
  pretty_print_expr(f, src, 0, 0);
  fprintf(f, "\nTranslation (from package %s, line %d):\n", 
	  pack_name, translation->LINE_NUM);
  pretty_print_expr(f, translation, 0, 0);
}


/************************************************************************
 *			SHOW_EXPAND_RESULT				*
 ************************************************************************
 * Show the result (RESULT) of an expand substitution.			*
 ************************************************************************/

PRIVATE void 
show_expand_result(EXPR *result)
{
  FILE* f = LISTING_FILE;
  if(f == NULL) f = ERR_FILE;
  if(f == NULL) return;

  fprintf(f, "\nResult of substitution:\n");
  pretty_print_expr(f, result, 0, 0);
}


/************************************************************************
 *			TRANSLATE_MATCHES_PM				*
 ************************************************************************
 * Replace all match expressions in e by their translations, perform	*
 * expansions and return the resulting expr.				*
 *									*
 * depth is the depth of matching at which this translation is being    *
 * done, and is used to break off really deep substitutions. 		*
 * (Global pm_loop_broken is set true when substitution is too deep.) 	*
 *									*
 * Returns NULL on failure.  If report_errs is true, it also reports	*
 * errors in the listing on failure.					*
 ************************************************************************/

EXPR* translate_matches_pm(EXPR *e, Boolean report_errs, int depth)
{
  EXPR_TAG_TYPE kind;
  EXPR *new;

  pm_loop_broken = FALSE;

  /*--------------------------------------------------------*
   * If there are no match expressions in e, just return e. *
   *--------------------------------------------------------*/

  if(e == NULL || e->nomatches) return e;

  /*-------------------------------------------------*
   * Break off translation if the depth is too high. *
   *-------------------------------------------------*/

  if(depth > max_patmatch_subst_depth) {
    if(report_errs) {
      semantic_error(PATMATCH_DEPTH_ERR, e->LINE_NUM);
      pm_loop_broken = TRUE;
    }
    return NULL;
  }

  /*----------------------------*
   * Do each according to kind. *
   *----------------------------*/

  kind = EKIND(e);
  switch(kind) {
    case IF_E:
    case TRY_E:

      /*---------------------------------------------------------------*
       * These expressions want their E1, E2 and E3 fields translated. *
       *---------------------------------------------------------------*/

      if(e->E3 != NULL) {
        new = translate_matches_pm(e->E3, report_errs, depth);
        if(new == NULL) return NULL;
        SET_EXPR(e->E3, new);
      }
      /* No break: fall through to next case. */

    case LET_E:
    case DEFINE_E:
    case SEQUENCE_E:
    case TEST_E:
    case STREAM_E:
    case AWAIT_E:
    case PAIR_E:
    case LAZY_BOOL_E:
    case WHERE_E:
    case TRAP_E:
    twoparts:

      /*-----------------------------------------------------------*
       * These expressions want their E1 and E2 fields translated. *
       *-----------------------------------------------------------*/

      if(e->E2 != NULL) {
        new = translate_matches_pm(e->E2, report_errs, depth);
        if(new == NULL) return NULL;
        SET_EXPR(e->E2, new);
      }
      /* No break: fall through to next case. */

    case SINGLE_E:
    case RECUR_E:
    case SAME_E:
    case LAZY_LIST_E:
    case EXECUTE_E:
    case OPEN_E:
    case MANUAL_E:

      /*----------------------------------------------------*
       * These expressions want their E1 fields translated. *
       *----------------------------------------------------*/

      if(e->E1 != NULL) {
        new = translate_matches_pm(e->E1, report_errs, depth);
        if(new == NULL) return NULL;
        SET_EXPR(e->E1, new);
      }
      e->nomatches = 1;
      return e;

    case APPLY_E:

      /*--------------------------------------*
       * Expand an application, if necessary. *
       *--------------------------------------*/
      
      {EXPR *expanded;
       expanded = translate_expand_pm(e, report_errs, e->LINE_NUM, depth);
       if(expanded == NULL) return NULL;
       if(expanded == e) {
	 if(e->E1 != NULL) {
	   new = translate_matches_pm(e->E1, report_errs, depth);
	   if(new == NULL) return NULL;
	   SET_EXPR(e->E1, new);
	 }
	 if(e->E2 != NULL) {
	   new = translate_matches_pm(e->E2, report_errs, depth);
	   if(new == NULL) return NULL;
	   SET_EXPR(e->E2, new);
	 }
       }
       expanded->nomatches = 1;
       return expanded;
      }

    case MATCH_E:

       /*--------------------------------------------*
        * Handle a match expression by expanding it. *
        *--------------------------------------------*/

       new = translate_match_pm(e,FALSE,FALSE,e->LINE_NUM,report_errs,depth);
       if(new == NULL) return NULL;
       new->nomatches = 1;
       return new;

    case FUNCTION_E:
    case LOOP_E:
    case FOR_E:

      /*-------------------------------------------------------------*
       * Function, loop and for expressions all contain patterns.    *
       * We need to do translations with them.	For for and function *
       * expression, the pattern is e->E1.  For a loop expression,   *
       * e->E1 is a match expression.				     *
       *-------------------------------------------------------------*/

      {EXPR *stack_expr, *new_body, *match, *pattern, *old_body;
       
       pattern = e->E1;
       if(pattern == NULL) goto twoparts;  /* Above, one of the cases. */
       if(kind == LOOP_E) pattern = pattern->E1;

       bump_expr(e);
       bump_expr(pattern);

       /*----------------------------------------------------*
        * Build a match that does a match against the stack. *
        *----------------------------------------------------*/

       bump_expr(stack_expr = build_stack_expr(pattern->ty, 
					       pattern->LINE_NUM));
       match = new_expr2(MATCH_E, pattern, stack_expr, pattern->LINE_NUM);
       bump_type(match->ty = hermit_type);

       /*---------------------------*
        * Build a loop or function. *
        *---------------------------*/

       if(kind != FOR_E) {
	 if(kind == LOOP_E) e->STR4 = stack_expr->STR;
	 old_body = e->E2;
	 bump_expr(new_body = apply_expr(match, old_body, 
					 old_body->LINE_NUM));
	 bump_type(new_body->ty = old_body->ty);
         SET_EXPR(new_body, translate_matches_pm(new_body,report_errs,depth));
	 if(new_body == NULL) {
	   drop_expr(new_body);
       fun_return_null:
	   drop_expr(e);
	   drop_expr(pattern);
	   drop_expr(stack_expr);
	   return NULL;
	 }
         bump_type(new_body->ty = old_body->ty);
 	 if(kind != LOOP_E) SET_EXPR(e->E1, stack_expr);
	 SET_EXPR(e->E2, new_body);
	 drop_expr(new_body);
       }

       /*-------------------------*
        * Build a for expression. *
        *-------------------------*/

       else {
	 e->STR4 = stack_expr->STR;
	 new = translate_matches_pm(match, report_errs, depth);
	 if(new == NULL) goto fun_return_null;  /* Above, under this case. */
	 SET_EXPR(e->E1, new);
	 if(e->E2 != NULL) {
	   new = translate_matches_pm(e->E2, report_errs, depth);
	   if(new == NULL) goto fun_return_null; /* Above, under this case. */
	   SET_EXPR(e->E2, new);
	 }
	 if(e->E3 != NULL) {
	   new = translate_matches_pm(e->E3, report_errs, depth);
	   if(new == NULL) goto fun_return_null;  /* Above, under this case.*/
	   SET_EXPR(e->E3, new);
	 }
	 if(is_simple_pattern(pattern)) e->extra = 1;  /* Can't fail. */
       }

       e->nomatches = 1;
       drop_expr(stack_expr);
       drop_expr(pattern);
       e->ref_cnt--;
       return e;
      }

    default: return e;
  }
}


/*******************************************************************
 *			REPLACE_PATVARS				   *
 *******************************************************************
 * Replace pattern variables in *es by copies of themselves.  pv_b *
 * is a table of previous bindings within same replace, so that	   *
 * the same pattern variable is replaced by the same replacement   *
 * throughout.							   *
 *******************************************************************/

PRIVATE void replace_patvars(EXPR **es, HASH2_TABLE **pv_b)
{
  EXPR *e;
  e = *es;

  /*---------------------------------*
   * The loop is for tail recursion. *
   *---------------------------------*/

  for(;;) {
    switch(EKIND(e)) {
      case PAT_VAR_E:
        {EXPR *pv;
	 HASH_KEY u;
	 HASH2_CELLPTR h;

	 /*--------------------------------------------------*
	  * There is nothing to do at an anonymous variable. *
	  *--------------------------------------------------*/

	 if(is_anon_pat_var(e)) return;

	 /*------------*
	  * Look up e. *
	  *------------*/

	 u.expr = e;
	 h = insert_loc_hash2(pv_b, u, inthash(tolong(e)), eq);

	 /*-------------------------------------------------------*
	  * If e is found, just get the new value from the table. *
	  *-------------------------------------------------------*/

	 if(h->key.num != 0) pv = h->val.expr;

	 /*-------------------------------------------------------------*
	  * If e is not found, make a copy and enter it into the table. *
	  *-------------------------------------------------------------*/

	 else {
	   pv = allocate_expr();
	   *pv = *e;
	   pv->ref_cnt = 1;
	   bump_expr(pv->E1);
	   bump_type(pv->ty);
	   h->key.num = tolong(e);
	   h->val.expr = pv; /* ref from pv */
	 }
	 set_expr(es, pv);
	 return;
        }
 
      /*--------------------------------------------------------*
       * The following just run replace_patvars recursively on  *
       * subexpressions. 					*
       *--------------------------------------------------------*/

      case IF_E:
      case TRY_E:
      case FOR_E:
        replace_patvars(&(e->E3), pv_b);
	/* No break: fall through to next case. */

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
        replace_patvars(&(e->E2), pv_b);
	/* No break: fall through to next case. */

      case SINGLE_E:
      case RECUR_E:
      case SAME_E:
      case EXECUTE_E:
      case MANUAL_E:
      case LAZY_LIST_E:
      case OPEN_E: 
        /*----------------------*
         * Tail recur on e->E1. *
         *----------------------*/

        es = &(e->E1);
	e = *es;
        break;		

      default: 
        return;
    }
  }
}


/************************************************************************
 *			TRANSLATE_MATCH_PM				*
 ************************************************************************
 * Return the result of pattern match translation on expression		*
 * e.  e is a match expr.  						*
 *									*
 * If unif is nonzero, then do unifications instead of equality tests.  *
 * (Use ==! if unif = 1, and ==!' if unif = 2.) 			*
 *									*
 * If lazy is true, then do translation in lazy mode.  			*
 *									*
 * Mark built expr nodes as having line 'line'.  			*
 *									*
 * Perform type inference necessary for first possible translation rule.*
 *									*
 * Return NULL if no translation rule can be found, and report any 	*
 * errors if report_errs is true.					*
 ************************************************************************/

EXPR* translate_match_pm(EXPR *e, int unif, Boolean lazy, 
			 int line, Boolean report_errs, int depth)
{
  EXPR *pattern, *sspattern, *pattern_id, *image, *ssimage;
  EXPR_TAG_TYPE pattern_kind;
  TYPE *image_type;
  Boolean use_stack;

  Boolean open           = e->SCOPE;
  EXPR*   pat_const_rule = NULL;
  EXPR*   result         = NULL;

  /*-----------------------------------------------------------------*
   ********************** Initialization *****************************
   *-----------------------------------------------------------------*/

  /*---------------------------------------------------------------*
   * Get the target and target type. The target is called 'image'. *
   * Translate the target.					   *
   *								   *
   * If the target is the special expression PRIM_STACK, then	   *
   * set use_stack true to cause the contents of the operand stack *
   * to be used as the target.					   *
   *---------------------------------------------------------------*/

  bump_expr(e);
  bump_expr(image = translate_matches_pm(e->E2, report_errs, depth));
  if(image == NULL) {
    drop_expr(e); 
    return NULL;
  }

  ssimage = skip_sames(image);
  bump_type(image_type = find_u(image->ty));
  use_stack = FALSE;
  if(EKIND(ssimage) == SPECIAL_E && ssimage->PRIMITIVE == PRIM_STACK) {
    use_stack = TRUE;
  }

  /*------------------------------------------------------------*
   * Get the pattern. If the pattern is a SINGLE_E pattern with *
   * attribute ROLE_SELECT_ATT, then translate it first. 	*
   *------------------------------------------------------------*/

  pattern      = e->E1;
  sspattern    = skip_sames(pattern);
  pattern_kind = EKIND(sspattern);
  if(pattern_kind == SINGLE_E && sspattern->SINGLE_MODE == ROLE_SELECT_ATT) {
    SET_EXPR(e->E1, 
	     get_role_select_structure(sspattern->E1, image_type));
    pattern      = e->E1;
    sspattern    = skip_sames(pattern);
    pattern_kind = EKIND(sspattern);
  }

  /*-------------------------------------------------------------*
   * Check for an application of active or active?.  Such 	 *
   * applications are handled by changing the unification mode.	 *
   *-------------------------------------------------------------*/

  if(pattern_kind == APPLY_E) {
    EXPR *patfun    = skip_sames(sspattern->E1);
    EXPR_TAG_TYPE k = EKIND(patfun);
    if(k == SPECIAL_E || k == GLOBAL_ID_E || k == PAT_FUN_E) {
      if(patfun->STR == std_id[ACTIVE_ID]) unif = 1;
      else if(patfun->STR == std_id[ACTIVE_PRIME_ID]) unif = 2;
      else goto after_unification_check; /* Just below. */

      pattern      = sspattern->E2;
      sspattern    = skip_sames(pattern);
      pattern_kind = EKIND(sspattern);
    }
  }

after_unification_check:

  /*----------------------------------------------------------------------*
   ********************* Equality Check or Unification ********************
   *----------------------------------------------------------------------*/

  /*-------------------------------------------------------------------*
   * If the pattern has no pattern variables, is not an application of *
   * a pattern function, is not a pair and is not a pattern constant,  *
   * do an equality check or unification. 			       *
   *-------------------------------------------------------------------*/

  if(is_fixed_pat(sspattern)) {

#   ifdef DEBUG
      if(trace_pm > 2) trace_t(559, pattern);
#   endif

    /*-------------------------------*
     * Check for a pattern constant. *
     *-------------------------------*/

    {EXPR *formals;
     int status;
     char *pack_name;
     if(pattern_kind == GLOBAL_ID_E
       && get_expand_type_and_rule_tm(sspattern, sspattern, unif, lazy, 1,
				      &formals, &pat_const_rule, &status,
				      &pack_name)) {
       sspattern->kind = PAT_FUN_E;
       sspattern->pat = 1;
       goto do_pat_const;  /* below, under switch */
     }
    }

    /*------------------------------------------*
     * Not a pattern constant.  		*
     * Do the equality check or unification.	*
     *------------------------------------------*/

#   ifdef DEBUG
      if(trace_pm) {
        if(!unif) {
	  trace_t(430);
	}
	else {
	  trace_t(431);
	  trace_ty(image_type);
	  tracenl();
	}
      }
#   endif

    /*---------------------------------------------*
     * If the pattern is (), then no need to test. *
     * Otherwise, do the test.			   *
     *---------------------------------------------*/

    if(is_hermit_expr(pattern)) {
      SET_EXPR(result, ignore_expr(image, line));
      goto out;
    }
    SET_EXPR(result, translate_patmatch_test_pm(pattern,image,unif,
						line,report_errs, depth));
    goto out;
  }

  /*-------------------------------------------------------------------*
   ************************ General Case *******************************
   *-------------------------------------------------------------------*/

  /*--------------------------------------------*
   * The pattern is one of the following.  	*
   *   A pattern variable such as ?x		*
   *   A pair, such as (a,b)			*
   *   A pattern constant			*
   *   An application of a pattern function	*
   *   A lazy pattern (:a:)			*
   *   A 'where' pattern a where b		*
   *   A SAME_E node referring to one of these  *
   *     kinds of patterns.			*
   *--------------------------------------------*/

  for(;;) {  /* Loop at SAME_E */

#   ifdef DEBUG
      if(trace_pm > 2) trace_t(557, pattern, expr_kind_name[pattern_kind]);
#   endif

    switch(pattern_kind) {

      /*--------------------------------------------------------*
       ******************** Pairs *******************************
       *--------------------------------------------------------*/

      case PAIR_E:

        if(sspattern->SCOPE) open = TRUE;
	SET_EXPR(result, 
		 translate_pair_match(sspattern, image, open, unif, lazy,
				      line, report_errs, depth));
	goto out;


      /*------------------------------------------------------------------*
       **************************** Awaits ********************************
       *------------------------------------------------------------------*/

      case AWAIT_E:
	{EXPR *new_match;

	 bump_expr(new_match = new_expr2(MATCH_E, sspattern->E2, image, line));
	 bump_type(new_match->ty = hermit_type);
	 SET_EXPR(result, 
	   translate_match_pm(new_match,unif,TRUE,line,report_errs,depth));
	 drop_expr(new_match);
	 goto out;
        }
					    

      /*----------------------------------------------------------*
       ******************* Applications ***************************
       *----------------------------------------------------------*/

      case APPLY_E:
	SET_EXPR(result,
		 translate_apply_match(pattern, image, open, unif, lazy,
				       use_stack, line, report_errs, depth));
	goto out;


      /*------------------------------------------------------------------*
       ********************** Pattern Constants ***************************
       *------------------------------------------------------------------*/

      do_pat_const:
      case PAT_FUN_E:
	{EXPR *formals, *translation, *preamble;
	 int status;
	 char *pack_name;

	 /*-------------------------------------------------------------*
	  * Get the type and translation rule, if not obtained already. *
	  *-------------------------------------------------------------*/
	
	 if(pat_const_rule == NULL &&
	    !get_expand_type_and_rule_tm(sspattern, sspattern, unif, lazy, 1,
					&formals, &pat_const_rule, &status,
				        &pack_name)) {
	   if(report_errs) {
	     expr_error(NO_RULE_ERR, sspattern);
	     report_pat_avail(sspattern);
           }
	   goto out;
	 }

	 formals     = NULL;
	 translation = pat_const_rule->E3;

	 /*----------------------------------------------*
	  * Put the image in an identifier if necessary. *
	  *----------------------------------------------*/

	 SET_EXPR(image, image_to_id(image,use_stack,translation,&preamble,
				     open,line));
	
	 /*------------------------------------*
	  * Rule found -- perform translation. *
	  *------------------------------------*/

	 SET_EXPR(result, do_patmatch_subst(formals, sspattern, translation,
					    image, preamble, unif, lazy,
					    0, line, report_errs, depth,
					    pack_name));
	 if(pm_loop_broken) {
	   err_print(STR_ERR, display_name(sspattern->STR));
	 }
	 drop_expr(preamble);
         goto out;
       }
	 

      /*------------------------------------------------------------------*
       ********************** Pattern Variables ***************************
       *------------------------------------------------------------------*/

      case PAT_VAR_E:

        /*--------------------------------------------------------------*
         * The bound field of the pattern variable is 0 if this 	*
	 * pattern var has not yet been given a value.  		*
         *--------------------------------------------------------------*/

	if(is_anon_pat_var(sspattern)) {

	  /*-------------------------------------------------------------*
	   * An anonymous pattern variable.  Just do ignore(image).      *
	   * It is important to evaluate the image, since its evaluation *
	   * might fail, and that can affect what happens.      	 *
	   *-------------------------------------------------------------*/

	  SET_EXPR(result, ignore_expr(image, line));
	  goto out;
	}

        /*--------------------------------------------------------------*
         * pattern_id is the local identifier associated with pattern   *
	 * variable e1. 						*
         *--------------------------------------------------------------*/

        pattern_id = skip_sames(sspattern->E1);

        /*------------------------------------------*
         * If pattern_id has no value, give it one. *
         *------------------------------------------*/

        if(sspattern->bound == 0) {
	  int kind = lazy ? DEFINE_E : LET_E;
	  SET_EXPR(result, new_expr2(kind, pattern_id, image, line));
	  bump_type(result->ty = hermit_type);
	  pattern_id->bound = sspattern->bound = 1;
	}

        /*-----------------------------------------------------------------*
         * pattern_id already has a value, so do an equality test, failing *
	 * not equal. 							   *
         *-----------------------------------------------------------------*/

	else {
	 SET_EXPR(result, translate_patmatch_test_pm(pattern_id, image,
					      unif, line, report_errs, depth));
	}
	goto out;


      /*----------------------------------------------------*
       ******************** Same ****************************
       *----------------------------------------------------*/

      case SAME_E:
	pattern      = sspattern->E1;
	sspattern    = skip_sames(pattern);
	pattern_kind = EKIND(sspattern);
	break;

      /*---------------------------------------------------------*
       ******************** Where ********************************
       *---------------------------------------------------------*/

      case WHERE_E:
	{EXPR *match, *test;
	 bump_expr(match = new_expr2(MATCH_E, sspattern->E1, image, line));
	 bump_type(match->ty = hermit_type);
	 SET_EXPR(match, translate_matches_pm(match, report_errs, depth));
	 if(match == NULL) {
	   drop_expr(match); 
	   goto out;
	 }
	 test = translate_matches_pm(sspattern->E2, report_errs, depth);
	 if(test == NULL) {
	   drop_expr(match); 
	   goto out;
	 }
	 test = new_expr1t(TEST_E, test, hermit_type, line);
	 SET_EXPR(result, apply_expr(match, test, line));
	 bump_type(result->ty = hermit_type);
	 drop_expr(match);
	 goto out;
	}

      default:

#	ifdef DEBUG
	  if(trace) trace_t(433, pattern_kind);
#	endif

	if(report_errs) expr_error(BAD_PAT_ERR, pattern);
	goto out;
    }

#   ifdef DEBUG
      if(trace_pm > 2) trace_t(558, pattern);
#   endif
  }

 out:
   drop_expr(image);
   drop_type(image_type);
   drop_expr(e);
   if(result != NULL) result->ref_cnt--;
   return result;
}


/****************************************************************
 *			TRY_EQUALITY_BY_PATFUN			*
 ****************************************************************
 * Return an expression that is the translation of		*
 * Match pattern = image %Match.  This is for the case where	*
 * the match would normally be translated to a unification or	*
 * an equality test, but the type of the pattern does not	*
 * support an equality test.					*
 *								*
 * Try instead to use a pattern function.			*
 *								*
 * If not possible, return NULL.				*
 ****************************************************************/

PRIVATE EXPR* 
try_equality_by_patfun(EXPR *pattern, EXPR *image, int line, int depth)
{
  EXPR* sspattern = skip_sames(pattern);
  if(EKIND(sspattern) == APPLY_E) {
    EXPR* fun = get_applied_fun(sspattern, TRUE);
    if(EKIND(fun) == GLOBAL_ID_E) {
      fun->kind = PAT_FUN_E;
      return translate_apply_match(pattern, image, FALSE, FALSE,
				   FALSE, FALSE, line, FALSE, depth);
    }
  }

  return NULL;
}


/************************************************************************
 *			TRANSLATE_PATMATCH_TEST_PM		        *
 ************************************************************************
 * We are translating Match pattern = image %Match.  Return the		*
 * result of the translation.						*
 *									*
 * Normally, create an expression 					*
 *     {pattern == image}     or 					*
 *     Unify(pattern, image). or 					*
 *     Unify'(pattern, image).						*
 * depending on whether unif is 0, 1 or 2, repectively.  		*
 *									*
 * Exception: If the pattern is of a type that does not support an	*
 * equality check, and it is a pattern constant or an application of a	*
 * pattern function, then try to use the pattern constant or function	*
 * instead.								*
 *									*
 * Return NULL on failure (and show an error message).		    	*
 * If report_errs is true, also report errors in the listing on failure.*
 ************************************************************************/

PRIVATE EXPR *
translate_patmatch_test_pm(EXPR *pattern, EXPR *image, int unif,
                           int line, Boolean report_errs, int depth)
{
  char *fun_name;
  EXPR *fun_expr, *call, *pair, *new1, *new2; 
  TYPE *codomain_type, *fun_type, *image_type;

  /*-----------------------------------------------------------------*
   * Get the name and type of the function that does the comparison. *
   *-----------------------------------------------------------------*/

  image_type = image->ty;
  if(unif == 0) {
    codomain_type = boolean_type;
    fun_name = std_id[EQ_SYM];
    if(warn_on_compare) {
      warn0(EQ_IN_MATCH_ERR, line);
    }
  }
  else {
    codomain_type = hermit_type;
    fun_name = std_id[(unif == 1) ? UNIFY_ID : UNIFYPRIME_ID];
  }     
  fun_type = function_t(pair_t(image_type, image_type), codomain_type);

  /*--------------------------------*
   * Build the comparison function. *
   *--------------------------------*/

  fun_expr = typed_global_id_expr(fun_name, fun_type, line);
  if(fun_expr == NULL) {
    if(report_errs) expr_error(PATMAT_EQ_ERR, pattern);
    return NULL;
  } 

  /*--------------------------------------------------------------------*
   * Get the image and the pattern on the stack.			*
   * Note: image must occur before pattern in pair, since image might	*
   * be on the stack. 							*
   *--------------------------------------------------------------------*/

  new1 = translate_matches_pm(image, report_errs, depth);
  if(new1 == NULL) return NULL;
  new2 = translate_matches_pm(pattern, report_errs, depth);
  if(new2 == NULL) return NULL;
  pair = new_expr2(PAIR_E, new1, new2, line);
  bump_type(pair->ty = pair_t(image_type, pattern->ty));

  /*-----------------------*
   * Build the comparison. *
   *-----------------------*/

  call = apply_expr(fun_expr, pair, line);
  if(unif == 0) {
    bump_type(call->ty = boolean_type);
    call = new_expr1t(TEST_E, call, hermit_type, line);
  }
  else {
    bump_type(call->ty = hermit_type);
  }

  /*-------------------------------------------------*
   * Do type inference on the comparison. If the     *
   * inference succeeds, then return call.  But if   *
   * the inference fails, try using a pattern 	     *
   * function instead.				     *
   *-------------------------------------------------*/

  bump_expr(call);
  if(!do_patmatch_infer_pm(call, 0, line)) {
    EXPR* newcall = try_equality_by_patfun(pattern, image, line, depth);
    if(newcall != NULL) SET_EXPR(call, newcall);
    else {
      if(report_errs) expr_error(PATMAT_EQ_ERR, pattern);
      SET_EXPR(call, bad_expr);
    }
  }
  if(call != NULL) call->ref_cnt--;
  return call;
}


/****************************************************************
 *			DO_PATMATCH_SUBST			*
 ****************************************************************
 * Return the result of doing pattern match substitution on	*
 * expression 							*
 *								*
 *   Match sspattern = image.					*
 *								*
 * with a rule that has	given formals and translation.  Add	*
 * preamble to the start of the result, by returning 		*
 * (preamble result). 						*
 *								*
 * unif gives the unification mode.  extra is nonzero of the 	*
 * resulting expr should have an open scope. 			*
 *								*
 * pack_name is the name of the package that provides this	*
 * substitution.						*
 *								*
 * Report any errors encountered only if report_errs is true.	*
 *								*
 ****************************************************************/

PRIVATE EXPR*
do_patmatch_subst
	(EXPR *formals, EXPR *sspattern, EXPR *translation,
	 EXPR *image, EXPR *preamble, int unif,
	 Boolean lazy, int extra,
	 int line, Boolean report_errs, int depth,
	 char *pack_name)
{
  EXPR* result = NULL;

# ifdef DEBUG
    if(trace_pm) trace_t(434);
# endif

  if(should_show_patmatch_subst) {
    show_patmatch_subst(formals, sspattern, translation, image,
			unif, lazy, line, pack_name);
  }

  if(!UNIFY(image->ty, sspattern->ty, FALSE)) {
    if(report_errs) semantic_error(PAT_SUBST_TYPE_ERR, 0);
    return NULL;
  }

  SET_EXPR(result, substitute_pm(formals, sspattern, translation, 
				 image, unif, lazy, extra, 1, line));
  if(preamble != NULL) {
    SET_EXPR(result, apply_expr(preamble, result, line));
    bump_type(result->ty = result->E2->ty);
  }

  if(should_show_patmatch_subst) {
    show_expand_result(result);
  }

  SET_EXPR(result, translate_matches_pm(result, report_errs, depth + 1));
  if(result != NULL) result->ref_cnt--;
  return result;
}


/****************************************************************
 *			DO_EXPAND_SUBST				*
 ****************************************************************
 * Return the result of doing expasion substitution on		*
 * expression e, with a rule that has				*
 * given formals and translation.  Report			*
 * any errors encountered only if report_errs is true.		*
 *								*
 * depth is used to cut off deep recursive translations, and	*
 * should be 0 when called from outside.			*
 ****************************************************************/

PRIVATE EXPR*
do_expand_subst(EXPR *formals, EXPR *e, EXPR *translation,
		int line, Boolean report_errs, int depth, char *pack_name)
{
  EXPR *result;

# ifdef DEBUG
    if(trace_pm) trace_t(435);
# endif

  if(should_show_patmatch_subst) {
    show_expand_subst(e,translation,line, pack_name);
  }

  bump_expr(result = substitute_pm(formals, e, translation, 
				   NULL, 0, 0, 0, 1, line));

  if(should_show_patmatch_subst) {
    show_expand_result(result);
  }

  SET_EXPR(result, translate_matches_pm(result, report_errs, depth + 1));
  if(result != NULL) result->ref_cnt--;
  return result;
}


/************************************************************************
 *			DO_PATMATCH_INFER_PM				*
 ************************************************************************
 * Do type inference on expression e, reporting any error to have	*
 * kind err_kind, and to occur at line line.  Return true on success,	*
 * false on failure.							*
 *									*
 * If err_kind is 0, do not report an error on failure, just return     *
 * false.								*
 ************************************************************************/

Boolean do_patmatch_infer_pm(EXPR *e, int err_kind, int line)
{
  deferral_ok = FALSE;

# ifdef DEBUG
    if(trace_infer || trace_pm > 1 || trace_open_expr > 1) {
      trace_t(436);
    }
# endif

  if(!infer_type_tc(e)) {
    if(err_kind != 0) semantic_error(err_kind, line);
    deferral_ok = TRUE;
    return FALSE;
  }
  deferral_ok = TRUE;
  return TRUE;
}


/************************************************************************
 *			TRANSLATE_EXPAND_PM				*
 ************************************************************************
 * Return the result of doing expansion on expression e, which must	*
 * be an APPLY_E expression.  Mark the new nodes as having line 'line'.	*
 *									*
 * Perform type inference necessary for the expansion, but refuse	*
 * to do restrictive bindings.  					*
 *									*
 * Report any errors if report_errs is true.				*
 ************************************************************************/

EXPR* translate_expand_pm(EXPR *e, Boolean report_errs, int line, int depth)
{
  EXPR *appl_fun, *result, *formals, *rule, *translation;
  int status;
  char *pack_name;

  /*--------------------------------------------------------------------*
   * Get the function being applied. We can only translate this		*
   * expansion if the expression being applied is a global identifier.	*
   *--------------------------------------------------------------------*/

  bump_expr(appl_fun = get_applied_fun(e, FALSE));
  if(appl_fun == NULL) return e;
  if(EKIND(appl_fun) != GLOBAL_ID_E) return e;

  /*------------------------------------------------------------*
   * Get the type and translation rule. If no rule is found	*
   * or if a restrictive rule is found, then just return e	*
   * unchanged.							*
   *------------------------------------------------------------*/

# ifdef DEBUG
    if(trace_pm) {
      tracenl();
      trace_t(437, appl_fun->STR);
      if(trace_pm > 1) {
	trace_t(438);
	short_print_expr(TRACE_FILE, e);
      }
    }
# endif

  if(!get_expand_type_and_rule_tm(appl_fun, e, 1, 1, 2, &formals, 
				  &rule, &status, &pack_name) || status) {
    return e;
  }
  translation = rule->E3;

  /*------------------------------------*
   * Rule found -- perform translation. *
   *------------------------------------*/

  result = do_expand_subst(formals, e, translation, 
			   line, report_errs, depth, pack_name);

  /*-------------------------------------------------*
   * If a loop was broken, finish the error message. *
   *-------------------------------------------------*/

  if(pm_loop_broken) {
    err_print(STR_ERR, display_name(appl_fun->STR));
  }

  drop_expr(appl_fun);

# ifdef DEBUG
    if(trace_pm > 1) {
      trace_t(439);
      print_expr(result, 0);
    }
# endif

  return result;
}


/****************************************************************
 *			BUILD_PARTIAL_PAIR_MATCH		*
 ****************************************************************
 * Build a match expr 						*
 *								*
 *   Match p = f(i).						*
 *								*
 * Return its translation.					*
 ****************************************************************/

PRIVATE EXPR*
build_partial_pair_match
  (EXPR *p, char *f, EXPR *i, int line, int open, Boolean unif,
   Boolean lazy, Boolean report_errs, int depth)
{
  EXPR *new_target, *f_expr, *result;
          
  f_expr = typed_global_id_expr(f, function_t(i->ty, p->ty), line);
  new_target = apply_expr(f_expr, i, line);
  bump_type(new_target->ty = p->ty);
  bump_expr(result = new_expr2(MATCH_E, p, new_target, line));
  bump_type(result->ty = hermit_type);
  result->SCOPE = open;
  SET_EXPR(result, 
	   translate_match_pm(result, unif, lazy, line, report_errs, depth));
  if(result == NULL) return NULL;
  result->ref_cnt--;
  return result;
}


/****************************************************************
 *			TRANSLATE_PAIR_MATCH			*
 ****************************************************************
 * Return the translation of 					*
 *								*
 *    Match pattern = image.					*
 *								*
 * where pattern has kind PAIR_E.  So we are really doing	*
 *								*
 *    Match (a,b) = image.					*
 *								*
 * This is a special case of translate_match_pm, and its	*
 * parameters are similar to that function's.			*
 ****************************************************************/

PRIVATE EXPR* 
translate_pair_match(EXPR *pattern, EXPR *image, Boolean open,
		     int unif, Boolean lazy, int line,
		     Boolean report_errs, int depth)
{
  EXPR *ssimage;

  /*---------------------------------------------------------------*
   * If the left-hand pattern is an anonymous variable, just       *
   * match the right-hand pattern against the right of the target. *
   *---------------------------------------------------------------*/

  if(is_anon_pat_var(pattern->E1)) { 
    return build_partial_pair_match(pattern->E2, std_id[RIGHT_ID], image,
				    line, open, unif, lazy, 
				    report_errs, depth);
  }

  /*-------------------------------------------------------------*
   * If the right-hand pattern is an anonymous variable, just    *
   * match the left-hand pattern against the left of the target. *
   *-------------------------------------------------------------*/

  if(is_anon_pat_var(pattern->E2)) {
    return build_partial_pair_match(pattern->E1, std_id[LEFT_ID], image,
				    line, open, unif, lazy,
				    report_errs, depth);
  }
		    
  /*-----------------------------------------------------------------*
   * If the target is a pair expression or if this is a lazy match,  *
   * then just do two separate matches.				     *
   *-----------------------------------------------------------------*/

  ssimage = skip_sames(image);
  if(EKIND(ssimage) == PAIR_E || lazy) {
    EXPR *match1, *match2, *image_left, *image_right;
    EXPR *new_image, *prelude, *result;

    /*-------------------------------------------------------------*
     * Get the left and right parts of the image. We might need to *
     * split the image to get them, in which case there is prelude *
     * stuff that does the splitting.				   *
     *-------------------------------------------------------------*/

    if(EKIND(ssimage) == PAIR_E) {
      prelude     = NULL;
      image_left  = ssimage->E1;
      image_right = ssimage->E2;
    }
    else {

      /*------------------------------------------------------------*
       * For a lazy match, get lazyleft and lazyright of the image. *
       *------------------------------------------------------------*/

      EXPR *lazyleft, *lazyright;
      TYPE *lazyleft_type, *lazyright_type, *image_ty;
      
      image_ty 	     = find_u(image->ty);
      lazyleft_type  = function_t(image_ty, image_ty->TY1);
      lazyright_type = function_t(image_ty, image_ty->TY2);
      lazyleft       = typed_global_sym_expr(LAZYLEFT_ID, 
					    lazyleft_type, line);
      lazyright      = typed_global_sym_expr(LAZYRIGHT_ID, 
					    lazyright_type, line);
      new_image      = image_to_id(image, 0, NULL, &prelude, FALSE, line);
      image_left     = apply_expr(lazyleft, new_image, line);
      bump_type(image_left->ty = image_ty->TY1);
      image_right    = apply_expr(lazyright, new_image, line);
      bump_type(image_right->ty = image_ty->TY2);
    }

    /*-----------------------------------------------*
     * Build the two match exprs and translate them. *
     *-----------------------------------------------*/

    bump_expr(match1 = 
	      new_expr2(MATCH_E, pattern->E1, image_left, line));
    bump_type(match1->ty = hermit_type);
    match1->SCOPE = open;
    SET_EXPR(match1, 
	     translate_match_pm(match1, unif, lazy, line, report_errs, depth));
    if(match1 == NULL) {drop_expr(prelude); return NULL;}

    bump_expr(match2 = 
	      new_expr2(MATCH_E, pattern->E2, image_right, line));
    bump_type(match2->ty = hermit_type);
    match2->SCOPE = open;
    SET_EXPR(match2, 
	     translate_match_pm(match2, unif, lazy, line, report_errs, depth));
    if(match2 == NULL) {
      drop_expr(match1); 
      drop_expr(prelude); 
      return NULL;
    }

    /*------------------------------------------------------------------*
     * Build the combined match, including the prelude if there is one. *
     *------------------------------------------------------------------*/

    result = apply_expr(match1, match2, line);
    bump_type(result->ty = hermit_type);
    result = possibly_apply(prelude, result, line);
    bump_type(result->ty = hermit_type);

    drop_expr(prelude);
    drop_expr(match1);
    drop_expr(match2);
    return result;
  }

  /*----------------------------------------------------------------*
   * If the target is not a pair expression, and this is not a lazy *
   * match, then compute the target and split it, doing two matches *
   * on what is left in the stack after splitting. 		    *
   *----------------------------------------------------------------*/

  else {
    EXPR *match1, *match2, *result;

    TYPE* image_ty = find_u(image->ty);
    EXPR* stack1   = build_stack_expr(image_ty->TY1, line);
    EXPR* stack2   = build_stack_expr(image_ty->TY2, line);

    /*------------------------*
     * Build the first match. *
     *------------------------*/

    bump_expr(match1 = new_expr2(MATCH_E, pattern->E1, stack1, line));
    bump_type(match1->ty = hermit_type);

    /*-------------------------*
     * Build the second match. *
     *-------------------------*/

    bump_expr(match2 = new_expr2(MATCH_E, pattern->E2, stack2, line));
    bump_type(match2->ty = hermit_type);

    /*-----------------------------*
     * Translate the second match. *
     *-----------------------------*/

    SET_EXPR(match2, 
	     translate_match_pm(match2,unif,lazy,line,report_errs,depth));
    if(match2 == NULL) {drop_expr(match1); return NULL;}

    /*----------------------------*
     * Translate the first match. *
     *----------------------------*/

    SET_EXPR(match1, 
	     translate_match_pm(match1,unif,lazy,line,report_errs,depth));
    if(match1 == NULL) {drop_expr(match2); return NULL;}

    /*------------------------------------------------------------------*
     * Build the combined expression, including splitting, which is 	*
     * done by a SEQUENCE node.						*
     *------------------------------------------------------------------*/

    result = apply_expr(match2, match1, line);
    bump_type(result->ty = hermit_type);
    result = new_expr2(SEQUENCE_E, image, result, line);
    result->STR3 = stack1->STR;
    result->STR4 = stack2->STR;
    bump_type(result->ty = hermit_type);
    result->SCOPE = open;
    drop_expr(match1);
    drop_expr(match2);
    return result;
  }
}


/****************************************************************
 *			TRANSLATE_APPLY_MATCH			*
 ****************************************************************
 * Return the translation of expression				*
 *								*
 *   Match pattern = image					*
 *								*
 * where pattern (or more precisely, skip_sames(pattern)) is	*
 * an APPLY_E node.						*
 *								*
 * On failure, return NULL and report an error if report_errs	*
 * is true.							*
 *								*
 * This is a special case of translate_match_pm, and its 	*
 * parameters are similar to that function's.			*
 ****************************************************************/

PRIVATE EXPR*
translate_apply_match(EXPR *pattern, EXPR *image, Boolean open,
                      int unif, Boolean lazy, Boolean use_stack,
 		      int line, Boolean report_errs, int depth)
{
  EXPR *patfun, *formals, *translation, *preamble, *rule;
  TYPE *patfun_type;
  EXPR_TAG_TYPE patfun_kind;
  int status;
  char *pack_name;

  EXPR* result = NULL;
  EXPR* sspattern = skip_sames(pattern);

  bump_expr(pattern);
  bump_expr(image);

  /*-----------------------------------------------------------*
   * Get pattern function, and check whether it is acceptable. *
   *-----------------------------------------------------------*/

  bump_expr(patfun = get_applied_fun(sspattern, TRUE));
  if(patfun == NULL) goto out;
  patfun_kind = EKIND(patfun);
  patfun_type = find_u(patfun->ty);
  if(patfun_kind != SPECIAL_E && patfun_kind != PAT_FUN_E) {
    if(report_errs) {
      expr_error(BAD_PAT_ERR, pattern);
      if(patfun_kind == GLOBAL_ID_E || patfun_kind == LOCAL_ID_E) {
        no_pat_parts_error(patfun);
      }
    }
    goto out;
  }

  /*------------------------------*
   * Check for a special pattern. *
   *------------------------------*/
  
  if(patfun_kind == SPECIAL_E) {
    EXPR* inverted_patfun = new_expr1t(SPECIAL_E, NULL_E,
				       function_t(patfun_type->TY2, 
						  patfun_type->TY1), 
				       line);
    inverted_patfun->PRIMITIVE = invert_prim_val(patfun->PRIMITIVE);
    inverted_patfun->SCOPE     = patfun->SCOPE;
    inverted_patfun->irregular = patfun->irregular;
    inverted_patfun->STR       = concat_id("un", patfun->STR);

    SET_EXPR(image, apply_expr(inverted_patfun, image, line));
    bump_type(image->ty = patfun_type->TY1);
    SET_EXPR(result, new_expr2(MATCH_E, sspattern->E2, image, line));
    bump_type(result->ty = hermit_type);
    result->SCOPE = open;
    SET_EXPR(result, 
	     translate_match_pm(result,unif,lazy,line,report_errs,depth));
    goto out;
  }

  /*------------------------------------*
   * Get the type and translation rule. *
   *------------------------------------*/

# ifdef DEBUG
    if(trace_pm) {
      tracenl();
      trace_t(432, nonnull(patfun->STR), unif);
      short_print_expr(TRACE_FILE,sspattern);
    }
# endif

  if(!get_expand_type_and_rule_tm(patfun, sspattern, unif, lazy, 0,
				  &formals, &rule, &status, &pack_name)) {

    /*------------------------------------------------------------------*
     * No rule could be found.  If a restrictive rule exists, then	*
     * ask for a tag.  If the pattern function is wrap, note that the   *
     * reason might be the safety of wrap.				*
     *------------------------------------------------------------------*/

    if(report_errs) {
      expr_error(NO_RULE_ERR, sspattern);
      err_print(PATFUN_IS_ERR);
      err_short_print_expr(patfun);
      report_pat_avail(patfun);
      if(patfun_kind == PAT_FUN_E && patfun->STR == std_id[WRAP_ID]) {
	err_print(WRAP_PATFUN_ERR);
      }
      if(status == 1) {
	PRINT_TYPE_CONTROL ctl;
        err_print(RESTRICTIVE_PATRULE_ERR);
	begin_printing_types(ERR_FILE, &ctl);
        do_show_types1(sspattern, &ctl, 1);
        end_printing_types(&ctl);
      }
    }
    goto out;
  }

  translation = rule->E3;

  /*----------------------------------------------*
   * Put the image in an identifier if necessary. *
   *----------------------------------------------*/

  SET_EXPR(image, image_to_id(image, use_stack, translation,
			      &preamble, open, line));

  /*---------------------------------------------------------------*
   * In open mode, replace pattern variables in the actual params. *
   * There are separate replacement tables for the two parts.      *
   *---------------------------------------------------------------*/

  if(rule->extra) {
    HASH2_TABLE *pv_b;
    EXPR* ssact = skip_sames(sspattern->E2);

    if(EKIND(ssact) != PAIR_E) die(131);
    pv_b = NULL;
    replace_patvars(&(ssact->E1), &pv_b);
    scan_and_clear_hash2(&pv_b, drop_hash_expr);
    replace_patvars(&(ssact->E2), &pv_b);
    scan_and_clear_hash2(&pv_b, drop_hash_expr);
  }
  
  /*------------------------------------*
   * Rule found -- perform translation. *
   *------------------------------------*/
  
  SET_EXPR(result, do_patmatch_subst(formals, sspattern, translation,
				     image, preamble, unif, lazy,
				     rule->extra, line, 
				     report_errs, depth, pack_name));

  /*-------------------------------------------------*
   * If a loop was broken, finish the error message. *
   *-------------------------------------------------*/

  if(pm_loop_broken) {
    err_print(STR_ERR, display_name(patfun->STR));
  }
  drop_expr(preamble);

 out:
  drop_expr(pattern);
  drop_expr(image);
  drop_expr(patfun);
  if(result != NULL) result->ref_cnt--;
  return result;
}
