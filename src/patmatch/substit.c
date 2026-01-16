/******************************************************************
 * File:    patmatch/substit.c
 * Purpose: Perform substitution for pattern matching and expansion
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
 * This file contains functions that perform substitutions into		*
 * expressions, in support of pattern match and expand replacements.	*
 ************************************************************************/

#include "../misc/misc.h"
#include "../utils/hash.h"
#include "../utils/lists.h"
#include "../utils/strops.h"
#include "../alloc/allocate.h"
#include "../classes/classes.h"
#include "../exprs/expr.h"
#include "../evaluate/instruc.h"
#include "../tables/tables.h"
#include "../standard/stdtypes.h"
#include "../standard/stdids.h"
#include "../generate/generate.h"
#include "../generate/prim.h"
#include "../patmatch/patmatch.h"
#include "../error/error.h"
#include "../ids/open.h"
#include "../unify/unify.h"
#include "../infer/infer.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

PRIVATE EXPR *
match_bind_pm(EXPR *this_formal, EXPR *this_actual, EXPR *translation,
	      Boolean expand, HASH2_TABLE **ex_b, HASH2_TABLE **ty_b,
	      int line);
PRIVATE EXPR *
do_match_bindings_pm(EXPR *formals, EXPR *actuals, EXPR *translation,
		     Boolean expand,
		     HASH2_TABLE **ex_b,
		     HASH2_TABLE **ty_b, int line);
PRIVATE EXPR *
fresh_id_pm(EXPR *e, EXPR *image,
	    HASH2_TABLE **ex_b, HASH2_TABLE **ty_b, int line);
PRIVATE EXPR *
do_subst_pm(EXPR *form, EXPR *image,
	    HASH2_TABLE **ex_b, HASH2_TABLE **ty_b,
	    int unif, Boolean lazy, int line);
#ifdef DEBUG
  PRIVATE void show_binding_pm(HASH2_CELLPTR h);
#endif

/****************************************************************
 *			PRIVATE VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			pm_loop_st				*
 ****************************************************************
 * pm_loop_st is used to allow a RECUR_E to find the LOOP_E to  *
 * which it belongs. Its top is the current LOOP_E expr.	*
 ****************************************************************/

PRIVATE EXPR_STACK pm_loop_st;


/****************************************************************
 *			SUBSTITUTE_PM				*
 ****************************************************************
 * Return the result of substituting expression `actual' for 	*
 * formal parameter `formals' in expression `ftranslation'.	*
 * If actual and formals are each pairs or function 		*
 * applications, then substitute  corresponding parts.		*
 * Also replace each local id in ftranslation by a new id, to	*
 * prevent conflict with other ids.				*
 *								*
 * `image' is the expression to substitute for the expression	*
 * 'target' in a pattern match.  If image is NULL, then this    *
 * is an expansion, not a pattern match substitution.		*
 *								*
 * If unif is true, match expressions in ftranslation get a ! 	*
 * attached to their patterns as part of the substitution.	*
 * So, for example, if  Match pt = z %Match is part of 		*
 * ftranslation,  then the result of substitution will include 	*
 * Match !(pt') = z' %Match, where pt' and z' are the results	*
 * of doing substitutions on pt and z.				*
 *								*
 * If lazy is true, then embedded matches are made lazy.	*
 * For example, if Match pt = z %Match is part of ftranslation, *
 * then the result of substitutin will include			*
 * Match (:pt':) = z' %Match where pt' and z' are the results	*
 * of doing substitutions on pt and z.				*
 *								*
 * If do_infer is true, then do type inference on the result of *
 * the substitution.  Otherwise, do not do such type inference.	*
 *								*
 * Open is true if the resulting expression should have an open *
 * scope, and false if it should have a closed scope.		*
 *								*
 * Line is the line number for new expressions.                 *
 *								*
 ****************************************************************/

EXPR* substitute_pm(EXPR *formals, EXPR *actuals, EXPR *ftranslation,
		    EXPR *image, int unif, Boolean lazy,
		    Boolean open, Boolean do_infer, int line)
{
  EXPR *result, *bind_expr;
  Boolean expand = (image == NULL);

  /*---------------------------------------------------------------*
   * Types will be copied from the translation, and we want all to *
   * share the same vars. Table ty_b contain variables that have   *
   * been encountered.				 	   	   *
   *---------------------------------------------------------------*/

  HASH2_TABLE* ty_b = NULL;

  /*---------------------------------------------------------*
   * Match the parameters, putting copies in the ex_b table. *
   *---------------------------------------------------------*/

  HASH2_TABLE* ex_b = NULL;
  if(formals != NULL) {

#   ifdef DEBUG
      if(trace_pm > 1 || trace_open_expr > 1) {
	trace_t(413);
      }
#   endif

    bind_expr = do_match_bindings_pm(formals, actuals, ftranslation,
				     expand, &ex_b, &ty_b, line);
  }
  else bind_expr = NULL;

# ifdef DEBUG
    if(trace_pm > 1 || trace_open_expr > 1) {
      trace_t(414);
        print_expr(formals, 1);
        trace_t(415);
        print_expr(actuals, 1);
        trace_t(416);
        scan_hash2(ex_b, show_binding_pm);
        if(image != NULL) {
          trace_t(417);
          print_expr(image, 1);
	}
        trace_t(418);
        print_expr(ftranslation, 1);
    }
# endif	     

  /*---------------------------*
   * Perform the substitution. *
   *---------------------------*/

  pm_loop_st = NIL;  /* Loop handling */
  bump_expr(result = 
	  do_subst_pm(ftranslation, image, &ex_b, &ty_b, unif, lazy, line));
  if(bind_expr != NULL) {
    SET_EXPR(result, apply_expr(bind_expr, result, line));
  }

  /*------------------------------------------------------------*
   * Redo id handling in the translation.  If might be		*
   * necessary to convert some PAT_FUN_E nodes to		*
   * GLOBAL_ID_E nodes.  (If we had f(?x) in the translation,	*
   * and ?x got bound to a term without pattern variables,	*
   * then we need to make f a global id.)			*
   *------------------------------------------------------------*/

  mark_all_pat_funs(result);

  /*------------------------------*
   * Handle open exprs in result. *
   *------------------------------*/

  if(open) {
    set_open_lists(result);
  }

# ifdef DEBUG
    if(trace_pm > 1 || trace_open_expr > 1) {
      trace_t(419);
      print_expr(result, 1);
    }
# endif

  /*-------------------------------------------------------------*
   * Do type inference on result.  Don't defer any unifications. *
   *-------------------------------------------------------------*/

  if(do_infer) {
    if(!do_patmatch_infer_pm(result, PAT_SUBST_TYPE_ERR, actuals->LINE_NUM)) {
      SET_EXPR(result, bad_expr);
      goto out;
    }
  }

  /*------------------------------------------------------------*
   * Drop refs in ex_b and ty_b, and free free the hash tables. *
   *------------------------------------------------------------*/
  
  scan_and_clear_hash2(&ex_b, drop_hash_expr);
  scan_and_clear_hash2(&ty_b, drop_hash_type);

 out:
  if(result != NULL) result->ref_cnt--;
  return result;
}


/****************************************************************
 *			MATCH_BIND_PM				*
 ****************************************************************
 * Bind this_formal to this_actual for pattern matching.  	*
 * ex_b and ty_b hold bindings for identifiers and type 	*
 * variables.   They are updated to hold the new binding of	*
 * this_formal to this_actual.                                  *
 *								*
 * If this_actual is a complex expression, and it occurs more   *
 * than once in translation, then it will be evaluated and put  *
 * into the environment.  The returned expr is NULL if there 	*
 * are no bindings to do, and it an expression that performs 	*
 * the bindings if there are bindings to do.			*
 *								*
 * If expand is true, then when binding a formal ?x,            *
 * put the identifier x in the table, rather than ?x itself.    *
 * Expand should be true when doing an expand, and false when   *
 * doing a pattern match.					*
 *								*
 * line is the line number to use when building new expression  *
 * nodes.							*
 ****************************************************************/

PRIVATE EXPR *
match_bind_pm(EXPR *this_formal, EXPR *this_actual, EXPR *translation,
	      Boolean expand, HASH2_TABLE **ex_b, HASH2_TABLE **ty_b,
	      int line)
{
  EXPR_TAG_TYPE this_actual_kind, this_formal_kind;
  EXPR *copy, *ssthis_formal, *ssthis_actual;
  TYPE *t;
  HASH_KEY u;
  HASH2_CELLPTR h;

  EXPR* result = NULL_E;

  ssthis_formal    = skip_sames(this_formal);
  this_formal_kind = EKIND(this_formal);

  /*-------------------------------------------------------------*
   * No need to bind when the formal is a global id or constant. *
   * Such formals are only used in selecting whether to use this *
   * expansion or pattern match rule.				 *
   *-------------------------------------------------------------*/

  if(this_formal_kind == GLOBAL_ID_E || this_formal_kind == CONST_E) {
    return NULL_E;
  }

# ifdef DEBUG
    if(trace_pm > 1) {
      trace_t(422);
      trace_t(420, expand);
      print_expr(this_formal, 1);
      print_expr(this_actual, 1);
    }
# endif

  /*--------------------------------------------------------------*
   * Decide whether to use the actual parameter unchanged or      *
   * whether to put it in an identifier.  (Leave identifiers and  *
   * constants unchanged.)  When putting in an identifier,        *
   * add a let expr to the result. 				  *
   *--------------------------------------------------------------*/

  ssthis_actual         = skip_sames(this_actual);
  ssthis_formal->OFFSET = 0;
  this_actual_kind      = EKIND(ssthis_actual);
  if (ssthis_actual->pat ||
      this_actual_kind == LOCAL_ID_E || 
      this_actual_kind == GLOBAL_ID_E ||
      this_actual_kind == CONST_E ||
      id_occurrences(ssthis_formal, translation) == 1) {

#   ifdef DEBUG
      if(trace_pm > 1) {
	trace_t(422);
	trace_t(421);
      }
#   endif

    bump_expr(copy = this_actual);
  }
  else {

#   ifdef DEBUG
      if(trace_pm > 1) {
	trace_t(422);
	trace_t(423);
      }
#   endif

    bump_expr(copy = new_expr1t(LOCAL_ID_E, NULL_E, this_actual->ty, line));
    copy->STR    = new_temp_var_name();
    copy->SCOPE  = -1;   /* Indicate local temporary. */
    result = new_expr2(LET_E, copy, this_actual, line);
  }

  /*------------------------------------------------------------------*
   * Enter the binding into the table.  If expand is true, and        *
   * the formal is a pattern variable ?x, then bind the identifier x. *
   *------------------------------------------------------------------*/

  if(expand && EKIND(ssthis_formal) == PAT_VAR_E) {
    ssthis_formal = ssthis_formal->E1;
  }
  u.num       = tolong(ssthis_formal);
  h           = insert_loc_hash2(ex_b, u, inthash(u.num), eq);
  h->key.num  = u.num;
  h->val.expr = copy;   /* Ref from copy, so don't drop copy */
  bump_type(t = copy_type1(this_formal->ty, ty_b, 1));
  if(!UNIFY(copy->ty, t, FALSE)) {
    semantic_error(PAT_SUBST_TYPE_ERR, 0);
  }
  drop_type(t);
  return result;
}


/****************************************************************
 *			DO_MATCH_BINDINGS_PM			*
 ****************************************************************
 * Perform all bindings of formals to actuals.  Handle as for   *
 * expansions if expand is true, and as for pattern 		*
 * translations if expand is false.  Parameter 'translation' is *
 * the translation of the match or expansion -- it is used to   *
 * count occurrences of formals.  				*
 *								*
 * Each formal is bound either to its corresponding actual, or  *
 * to an identifier that is bound to the corresponding actual.  *
 * The choice is made as follows.				*
 *								*
 *    If the actual is an identifier or constant, or if the     *
 *    formal occurs in the translation only once, then the      *
 *    formal is bound directly to the actual.			*
 *								*
 *    Otherwise, an identifier is created and is bound to	*
 *    the actual, and the formal is bound to that identifier.   *
 *								*
 * The returned expression is the one that does the identifier  *
 * bindings, if any are done, and is NULL if no bindings are    *
 * needed.  New expression nodes have line 'line'.		*
 *								*
 * ex_b tells parameter bindings, and ty_b tells type variable  *
 * bindings.  New bindings are put into ex_b.			*
 ****************************************************************/

PRIVATE EXPR *
do_match_bindings_pm(EXPR *formals, EXPR *actuals, EXPR *translation,
		     Boolean expand, HASH2_TABLE **ex_b,
		     HASH2_TABLE **ty_b, int line)
{
  EXPR *copy1, *copy2, *bind_expr1, *bind_expr2;
  EXPR *stack1, *stack2, *let1, *let2, *lets;
  EXPR_TAG_TYPE formals_kind;
  EXPR* result = NULL;

  if(formals == NULL) return NULL;

  formals      = skip_sames(formals);
  formals_kind = EKIND(formals);
  actuals      = skip_sames(actuals);

  /*---------------------------------*
   * Default is to do match_bind_pm. *
   *---------------------------------*/

  if(formals_kind != PAIR_E && formals_kind != APPLY_E) {
    bump_expr(result =
	      match_bind_pm(formals, actuals, translation,
			    expand, ex_b, ty_b, line));
  }

  else if(formals_kind == PAIR_E) {

    /*-----------------------------------------------------------------*
     * If both the formals and the actuals are pair expressions, then  *
     * it is more efficient to bind the left parts together and the    *
     * right parts together.					       *
     *-----------------------------------------------------------------*/

    if(EKIND(actuals) == PAIR_E) {
      bump_expr(bind_expr1 =
	do_match_bindings_pm(formals->E1, actuals->E1, translation,
			     expand, ex_b, ty_b, line));
      bump_expr(bind_expr2 =
	do_match_bindings_pm(formals->E2, actuals->E2, translation,
			     expand, ex_b, ty_b, line));
      bump_expr(result = bind_apply_expr(bind_expr1, bind_expr2, line));
      drop_expr(bind_expr1);
      drop_expr(bind_expr2);
    }

    /*---------------------------------------------------------------*
     * If the formal expr is a pair but the actual is not, then just *
     * split the actual.					     *
     *---------------------------------------------------------------*/

    else {

#     ifdef DEBUG
	if(trace_pm > 1) {
	  trace_t(424);
	}
#     endif

      if(actuals->ty == NULL || TKIND(actuals->ty) != PAIR_T) {
	semantic_error(PM_SPLITS_NONPAIR_ERR, actuals->LINE_NUM);
      }
      else {
        bump_expr(copy1 = new_expr1(LOCAL_ID_E, NULL_E, line));
        bump_expr(copy2 = new_expr1(LOCAL_ID_E, NULL_E, line));
	copy1->STR   = new_temp_var_name();
	copy2->STR   = new_temp_var_name();
        copy1->SCOPE = copy2->SCOPE = -1;
        bump_type(copy1->ty = actuals->ty->TY1);
        bump_type(copy2->ty = actuals->ty->TY2);
	stack1 = build_stack_expr(copy1->ty, line);
	stack2 = build_stack_expr(copy2->ty, line);
        let1 = new_expr2(LET_E, copy1, stack1, line);
        let2 = new_expr2(LET_E, copy2, stack2, line);
        lets = apply_expr(let2, let1, line);
        bump_expr(result = new_expr2(SEQUENCE_E, actuals, lets, line));
        result->STR3 = stack1->STR;
        result->STR4 = stack2->STR;
        bump_expr(bind_expr1 = 
	  do_match_bindings_pm(formals->E1, copy1, translation,
			       expand, ex_b, ty_b, line));
	bump_expr(bind_expr2 =
	  do_match_bindings_pm(formals->E2, copy2, translation,
			       expand, ex_b, ty_b, line));
	SET_EXPR(result,
		 bind_apply_expr(result,
			     bind_apply_expr(bind_expr1, bind_expr2, line),
			     line));
	drop_expr(bind_expr1);
	drop_expr(bind_expr2);
	drop_expr(copy1);
	drop_expr(copy2);
      }
    }
  }

  else /* formals_kind == APPLY_E */ {

    /*-----------------------------------------------------------*
     * If formals and actuals are both APPLY_E nodes, then match *
     * corresponding parts.  Otherwise, just do the default      *
     * match_bind_pm.						 *
     *-----------------------------------------------------------*/

    if(EKIND(skip_sames(formals->E1)) == APPLY_E) {
      bump_expr(bind_expr1 =
	do_match_bindings_pm(formals->E1, actuals->E1, translation,
			     expand,ex_b,ty_b,line));
      bump_expr(bind_expr2 =
	do_match_bindings_pm(formals->E2, actuals->E2, translation,
			     expand,ex_b,ty_b,line));
      bump_expr(result = bind_apply_expr(bind_expr1, bind_expr2, line));
      drop_expr(bind_expr1);
      drop_expr(bind_expr2);
    }
    else {
      bump_expr(result =
		do_match_bindings_pm(formals->E2, actuals->E2, translation,
				     expand, ex_b, ty_b, line));
    }
  } /* end else(formals_kind == APPLY_E) */

  if(result != NULL) result->ref_cnt--;
  return result;
}


/*****************************************************************
 *			FRESH_ID_PM		       		 *
 *****************************************************************
 * Return a copy of e, at line line.  ex_b and ty_b give current *
 * bindings.  image is for convenience, to pass to do_subst.     *
 * It is not used by do_subst.					 *
 *****************************************************************/

PRIVATE EXPR *
fresh_id_pm(EXPR *e, EXPR *image,
	    HASH2_TABLE **ex_b, HASH2_TABLE **ty_b, 
	    int line)
{
  EXPR *newE1, *newE;
  HASH2_CELLPTR h;
  HASH_KEY u;

  bump_expr(newE1 = (EKIND(e) == PAT_VAR_E)
	      ? do_subst_pm(e->E1, image, ex_b, ty_b, 0, 0, line)
	      : NULL_E);
  u.num = tolong(e);
  h     = insert_loc_hash2(ex_b, u, inthash(u.num), eq);

  /*---------------------------------------------------------------*
   * If this identifier is not yet in the table.  Build a node for *
   * it, and associate the copy with it.  Put this identifier      *
   * in the table.			                           *
   *---------------------------------------------------------------*/

  if(h->key.num == 0) {

#   ifdef DEBUG
      if(trace_locals) {
	trace_t(425, e->STR, e);
      }
#   endif

    newE =  new_expr1t(EKIND(e), newE1, copy_type1(e->ty, ty_b, 1), line);
    newE->STR       = e->STR;
    newE->PRIMITIVE = e->PRIMITIVE;
    h->key = u;
    bump_expr(h->val.expr = newE);
  }

  /*------------------------------------------------------------*
   * If this identifier is already in the table, then we have   *
   * serious trouble.  This should never happen.		*
   *------------------------------------------------------------*/

  else {
    die(130);
    return NULL;
  }

  drop_expr(newE1);
  return newE;
}


/****************************************************************
 *			DO_SUBST_PM				*
 ****************************************************************
 * Return a copy of form in which formal parameters have been 	*
 * bound to actual parameters, and expression `target' has been *
 * replaced by image.  Also copy all locally defined		*
 * identifiers. 						*
 *								*
 * ex_b gives bindings of formals to actuals.			*
 *								*
 * ty_b gives bindings of type variables.			*
 ****************************************************************/

PRIVATE EXPR *
do_subst_pm(EXPR *form, EXPR *image,
	    HASH2_TABLE **ex_b, HASH2_TABLE **ty_b,
	    int unif, Boolean lazy, int line)
{
  EXPR *form1, *form2, *id, *cpy, *result;
  EXPR_TAG_TYPE kind;

  for(;;) {
    if(form == NULL_E) return NULL_E;

    /*---------------------------------------------------------------*
     * Variables form1 and form2 do not make sense in all cases, but *
     * it is still safe to select them once here.                    *
     *---------------------------------------------------------------*/

    form1 = form->E1;
    form2 = form->E2;

    kind = EKIND(form);
    switch(kind) {

      case OPEN_E:
	{EXPR *f1;

	 bump_expr(f1 = do_subst_pm(form1, image, ex_b, ty_b, unif, 
				    lazy, line));
	 bump_expr(result = open_if(f1));
	 process_new_open(result);
	 drop_expr(f1);
	 if(result != NULL) result->ref_cnt--;
	 goto out;
	}

      case IF_E:
      case TRY_E:
      case FOR_E:
	result = new_expr3 (kind,
		  do_subst_pm(form1,    image, ex_b, ty_b, unif, lazy, line),
		  do_subst_pm(form2,    image, ex_b, ty_b, unif, lazy, line),
		  do_subst_pm(form->E3, image, ex_b, ty_b, unif, lazy, line),
		  line);
	result->mark     = form->mark;  /* See expr.doc */
        result->TRY_KIND = form->TRY_KIND;
	goto out;

      case APPLY_E:
      case SEQUENCE_E:
      case STREAM_E:
      case FUNCTION_E:
      case PAIR_E:
      case LAZY_BOOL_E:
      case AWAIT_E:
      case TRAP_E:
      case WHERE_E:
      case TEST_E:
	{EXPR *res1, *res2;
	 result           = new_expr1(kind, NULL, line);
	 *result          = *form;
	 result->LINE_NUM = line;
	 result->ref_cnt  = 0;
	 res1 = do_subst_pm(form1, image, ex_b, ty_b, unif, lazy, line);
	 res2 = do_subst_pm(form2, image, ex_b, ty_b, unif, lazy, line);
	 bump_expr(result->E1 = res1);
	 bump_expr(result->E2 = res2);
	 goto out;
	}

      case LOOP_E:
	/*---------------------------------------------------------*
	 * Partially build this loop expression and push a pointer *
	 * to it onto pm_loop_st, so that a RECUR_E expression can *
	 * find it.						   *
	 *---------------------------------------------------------*/

	result = new_expr1(kind, NULL_E, line);
	push_expr(pm_loop_st, result);

	/*-------------------------------*
	 * Now finish building the loop. *
	 *-------------------------------*/

	bump_expr(result->E1 = do_subst_pm(form1, image, ex_b, ty_b,
					   unif, lazy, line));
	bump_expr(result->E2 = do_subst_pm(form2, image, ex_b, ty_b,
					   unif, lazy, line));
	pop(&pm_loop_st);
	goto out;

      case RECUR_E:
	result = new_expr1(kind,
		   do_subst_pm(form1, image, ex_b, ty_b, unif, lazy, line),
		   line);
	result->E2 = top_expr(pm_loop_st);  /* Not ref counted -- cyclic link*/
	goto out;

      case MATCH_E:
	{EXPR *s1, *s2, *spec;
	 TYPE *s1_type;

	 s1 = do_subst_pm(form1, image, ex_b, ty_b, unif, lazy, line);
	 s2 = do_subst_pm(form2, image, ex_b, ty_b, unif, lazy, line);

	 /*-------------------------------------------------------*
	  * Attach active or active' to pattern if in unify mode. *
	  *-------------------------------------------------------*/

	 if(unif) {
	   s1_type         = s1->ty;
	   spec            = new_expr1(SPECIAL_E, NULL, line);
	   spec->STR       = std_id[(ACTIVE_ID - 1) + unif];
	   spec->PRIMITIVE = PRIM_CAST;
	   bump_type(spec->ty = function_t(s1_type, s1_type));
	   s1 = apply_expr(spec, s1, line);
	   s1->PRIMITIVE = PAT_FUN;
	   bump_type(s1->ty = s1_type);
	 }

	 /*-------------------------*
	  * Make lazy in lazy mode. *
	  *-------------------------*/

	 if(lazy) {
	   s1_type = s1->ty;
	   s1      = new_expr2(AWAIT_E, NULL, s1, line);
	   bump_type(s1->ty = s1_type);
	 }

	 result = new_expr2(kind, s1, s2, line);
	 result->SCOPE = form->SCOPE;
	 goto out;
       }

      case LAZY_LIST_E:
	result = new_expr1(kind,
			   do_subst_pm(form1, image, ex_b,
				       ty_b, unif, lazy, line),
			   line);
	result->SCOPE = form->SCOPE;
	result->OFFSET = form->OFFSET;
	goto out;

      case SINGLE_E:
	result = new_expr1(SINGLE_E,
			   do_subst_pm(form1, image, ex_b,
				       ty_b, unif, lazy, line),
			   line);
	result->SINGLE_MODE = form->SINGLE_MODE;
	result->SCOPE       = form->SCOPE;
	goto out;

      case SAME_E:
	result = new_expr1(SAME_E,
			   do_subst_pm(form1, image, ex_b,
				       ty_b, unif, lazy, line),
			   line);
	result->SAME_MODE = form->SAME_MODE;
	result->SCOPE     = form->SCOPE;
	goto out;

      case PAT_VAR_E:
      case LOCAL_ID_E:
      case IDENTIFIER_E:
	{HASH_KEY u;
	 HASH2_CELLPTR h;
	 if(form->bound == 0) {
	   u.num = tolong(form);
	   h     = locate_hash2(*ex_b, u, inthash(u.num), eq);

#          ifdef DEBUG
	     if(trace_locals) {
	       trace_t(426, form->STR, form, expr_kind_name[EKIND(form)]);
	       if(h->key.num != 0) trace_t(427);
	       else                trace_t(428);
	     }
#          endif

	   if(h->key.num != 0) {
	     result = wsame_e(h->val.expr, line);
	     goto out;
	   }
	   return fresh_id_pm(form, image, ex_b, ty_b, line);
	 }
	}
	/*------------------------------------------------------------*
	 * No break: continue with next form when form_bound is true. *
	 *------------------------------------------------------------*/

      case CONST_E:
      case GLOBAL_ID_E:
      case PAT_FUN_E:
      copy_id:
	result  = new_expr1(kind, NULL_E, line);
	*result = *form;
	result->LINE_NUM = line;
	result->ref_cnt  = 0;
	goto out;

      case LET_E:
      case DEFINE_E:
	id  = fresh_id_pm(form1, image, ex_b, ty_b, line);
	cpy = new_expr2(kind, id,
		  do_subst_pm(form2, image, ex_b, ty_b, unif, lazy, line),
		  line);
	cpy->SCOPE    = form->SCOPE;
	cpy->LET_FORM = form->LET_FORM;
	bump_type(cpy->ty = hermit_type);
	return cpy;

      case SPECIAL_E:
	if(form->PRIMITIVE != PRIM_TARGET) goto copy_id;
	return image;  /* Don't install old type */

      default:
	semantic_error1(BAD_EXPR_KIND_ERR, (char *) tolong(kind), line);
	return bad_expr;
    } /* end switch */
  } /* end for */

 out:
  bump_type(result->ty = copy_type1(form->ty, ty_b, 1));
  return result;
}


/****************************************************************
 *			SHOW_BINDING_PM				*
 ****************************************************************
 * Show the binding in cell h.  This is strictly for debugging. *
 ****************************************************************/

#ifdef DEBUG

PRIVATE void show_binding_pm(HASH2_CELLPTR h)
{
  trace_t(429, h->key.expr);
  print_expr(h->val.expr, 1);
}

#endif
