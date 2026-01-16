/*****************************************************************
 * File:    exprs/choose.c
 * Purpose: Functions for building if/try/choose/loop/for
 *          expressions.
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
 * This file contains functions that construct expressions that make	*
 * choices.  Functions for building the more primitive form of a 	*
 * definition by cases are here, for example, as are functions that 	*
 * build parts of choose expressions for the parser.			*
 ************************************************************************/

#include <string.h>
#include "../misc/misc.h"
#include "../machdata/except.h"
#include "../machstrc/machstrc.h"
#include "../classes/classes.h"
#include "../tables/tables.h"
#include "../standard/stdtypes.h"
#include "../standard/stdids.h"
#include "../exprs/expr.h"
#include "../utils/hash.h"
#include "../utils/strops.h"
#include "../utils/lists.h"
#include "../alloc/allocate.h"
#include "../error/error.h"
#include "../parser/parser.h"
#include "../parser/tokens.h"
#include "../patmatch/patmatch.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

PRIVATE void get_succeed_guard_p	(int try, EXPR *gd,
				 int *succeed, EXPR **guard,
				 EXPR *when, int line);

/*********************************************************************
 *			PUBLIC VARIABLES			     *
 *********************************************************************/

/*********************************************************************
 *			formal_num				     *
 *********************************************************************
 * formal_num is used to compute the next automatic formal parameter *
 * name in a function definition by cases.  If formal_num is <n>,    *
 * then the next formal is called 'formal<n>'.  formal_num is set to *
 * 1 at the start of each declaration.  It is used below in 	     *
 * make_def_by_cases_p.						     *
 *********************************************************************/

int formal_num = 1;

/****************************************************************
 *			ATTACH_CUT				*
 ****************************************************************
 * Return expression 						*
 *								*
 *     Cut(). 							*
 *     body							*
 ****************************************************************/

PRIVATE EXPR* attach_cut(EXPR *body)
{
  int   line = body->LINE_NUM;
  TYPE *cut_type;
  EXPR *cut_id, *cut_expr;

  cut_type = function_t(hermit_type, hermit_type);
  cut_id   = typed_global_sym_expr(CUT_ID, cut_type, line);
  cut_expr = apply_expr(cut_id, same_e(hermit_expr, line), line);
  return apply_expr(cut_expr, body, line);
}


/****************************************************************
 *			ATTACH_COMMIT				*
 ****************************************************************
 * If the top frame of choose_st has a nonnull status_var	*
 * field, then return expression 				*
 *								*
 *     ChooseCommit(status_var). 				*
 *     body							*
 *								*
 * Otherwise, just return body.					*
 ****************************************************************/

PRIVATE EXPR* attach_commit(EXPR *body)
{
  int   line = body->LINE_NUM;
  TYPE *commit_type;
  EXPR *commit_id, *commit_expr, *status_var;

  status_var = top_choose_info(choose_st)->status_var;
  if(status_var == NULL) return body;
  else {
    commit_type = function_t(box_t(hermit_type), hermit_type);
    commit_id   = typed_global_sym_expr(CHOOSECOMMIT_ID, commit_type, line);
    commit_expr = apply_expr(commit_id, same_e(status_var, line), line);
    return apply_expr(commit_expr, body, line);
  }
}


/****************************************************************
 *			ALL_CASE_P				*
 ****************************************************************
 * Return an expression representing a part of a choose-all or	*
 * choose-all-mixed or choose-one-mixed expression.  		*
 *								*
 * When try = 0, the returned expression implements 		*
 *								*
 *   case cond => body						*
 *   rest (remaining cases)					*
 *								*
 * rest can be NULL if there are no other cases.		*
 *								*
 * If try is nonzero, the returned expression implements	*
 *								*
 *   case do cond => body					*
 *   rest (remaining cases)					*
 *								*
 * When try is nonzero, it is the control kind (TRY_F, etc.) to *
 * use for the try that implements the test.			*
 *								*
 *--------------------------------------------------------------*
 * The resulting expression is as follows, depending on mode,   *
 * try, rest and the value of the status_var field in the top	*
 * frame of stack choose_st.  If status_var is nonnull, then	*
 * commit is							*
 *								*
 *	ChooseCommit(status_var).				*
 *								*
 * If status_var is null, then commit is omitted.		*
 * 								*
 * mode		try	rest	result				*
 * ----		---	----	------				*
 * ALL_ATT	0	expr	Stream 				*
 *				  {cond}			*
 *				  commit			*
 *				  body 				*
 *				then				*
 *				  rest 				*
 *				%Stream				*
 *								*
 * ALL_ATT	0	NULL	{cond}				*
 *				commit				*
 *				body				*
 *								*
 * ALL_ATT	TRY_F	expr	Stream				*
 *				  cond				*
 *				  commit			*
 *				  body				*
 *				then				*
 *				  rest				*
 *				%Stream				*
 *								*
 * ALL_ATT	TRY_F   NULL	cond				*
 *				commit				*
 *				body				*
 *								*
 * ALL_MIXED_ATT 0	expr	Mix				*
 *				  {cond}			*
 *				  commit			*
 *				  body				*
 *				with				*
 *				  rest				*
 *				%Mix				*
 *								*
 * ALL_MIXED_ATT 0	NULL	{cond}				*
 *				commit				*
 *				body				*
 *								*
 * ALL_MIXED_ATT TRY_F	expr	Mix				*
 *				  cond				*
 *				  commit			*
 *				  body				*
 *				with				*
 *				  rest				*
 *				%Mix				*
 *								*
 * ALL_MIXED_ATT TRY_F  NULL	cond 				*
 *				commit				*
 *				body				*
 *								*
 * ONE_MIXED_ATT 0	expr	Mix				*
 *				  {cond}			*
 *				  Cut().			*
 *				  body				*
 *				with				*
 *				  rest				*
 *				%Mix				*
 *								*
 * ONE_MIXED_ATT 0	NULL	{cond} 				*
 *				Cut(). 				*
 *				body				*
 *								*
 * ONE_MIXED_ATT TRY_F	expr	Mix				*
 *				  cond				*
 *				  Cut().			*
 *				  body				*
 *				with				*
 *				  rest				*
 *				%Mix				*
 *								*
 * ONE_MIXED_ATT TRY_F	NULL	cond				*
 *				Cut(). 				*
 *				body				*
 *								*
 *--------------------------------------------------------------*
 *								*
 * In the special case where cond is NULL, treat cond as if it  *
 * is always true.  cond == NULL is only acceptable if try = 0.	*
 *								*
 ****************************************************************/

PRIVATE EXPR* all_case_p(ATTR_TYPE which, int try, EXPR *cond, 
		        EXPR *body, EXPR *rest)
{
  EXPR *this_case, *result, *augmented_body, *test;

  int line = cond == NULL ? body->LINE_NUM : cond->LINE_NUM;

  /*--------------------------------------*
   * Check that try is either 0 or TRY_F. *
   *--------------------------------------*/

  if(try > TRY_F) {
    warn0(ALL_CASE_WITH_MODE_ERR, line);
  }

  /*--------------------------------------------------------------------*
   * Build the body as it will occur in the result.  This is the	*
   * body possibly preceded by commit a choose-all-mixed or choose-all  *
   * expression, and is the body preceded by a Cut() for a 		*
   * choose-one-mixed expression.					*
   *--------------------------------------------------------------------*/

  augmented_body = (which != ONE_MIXED_ATT) 
	             ? attach_commit(body) 
		     : attach_cut(body);

  /*----------------------------------------------------------------------*
   * Now build the left-hand part of the stream.  It is called this_case. *
   *----------------------------------------------------------------------*/

  if(cond == NULL) {
    this_case = augmented_body;
  }
  else {
    test = same_e(try ? cond : new_expr1(TEST_E, cond, line), line);
    test->SAME_MODE = 2;  /* mark choose-condition -- see expr.doc */
    this_case = apply_expr(test, augmented_body, line);
  }

  /*---------------------------------------------------*
   * Now build the stream, or skip it if rest is NULL. *
   *---------------------------------------------------*/

  if(rest == NULL) result = this_case;
  else {
    result = new_expr2(STREAM_E, this_case, rest, line);
    result->STREAM_MODE = (which != ALL_ATT) ? MIX_ATT : STREAM_ATT;
    result->mark = 1;
  }

  /*-----------------------------------*
   * Mark as mix expr if mode says to. *
   *-----------------------------------*/

  return result;
}


/************************************************************************
 *			FIRST_CASE_P					*
 ************************************************************************
 * Return an expression representing a part of a choose-first or	*
 * choose-one expression.  The case has the form			*
 *									*
 *   case cond => body							*
 *   rest (remaining cases)						*
 *									*
 * if try is 0, and has the form					*
 *									*
 *   case do cond => body						*
 *   rest (remaining cases)						*
 *									*
 * if try is nonzero.  In the latter case, try is the control kind	*
 * (TRY_F, etc.) to use in the try.					*
 *									*
 * The result is 							*
 *									*
 *   If cond then body else rest %If					*
 *									*
 * in the former case, or						*
 *									*
 *   Try cond then body else rest %Try 					*
 *									*
 * in the latter case.							*
 ************************************************************************/

PRIVATE EXPR* first_case_p(ATTR_TYPE w, int try, EXPR *cond, 
		          EXPR *body, EXPR *rest)
{
  EXPR *result;

  if(!try) {
    result = new_expr3(IF_E, cond, body, rest, cond->LINE_NUM);
    if(w == ONE_ATT) result->IF_MODE = 2;
  }
  else {
    result = try_expr(cond, body, rest, try, cond->LINE_NUM);
  }

  /*---------------------------------*
   * Indicate part of a choose expr. *
   *---------------------------------*/

  result->mark = 1;
  return result;
}


/****************************************************************
 *			MAKE_CASE_P				*
 ****************************************************************
 * Return an expression that represents a case in a choose 	*
 * expression.  The case is					*
 *								*
 *   case cond => body						*
 *   rest (remaining cases)					*
 *								*
 * if try is 0, and is						*
 *								*
 *   case do cond => body					*
 *   rest							*
 *								*
 * if try is nonzero.  When try is nonzero, it is the control	*
 * kind (TRY_F, etc.) to use when building the try.		*
 *								*
 * branch is used to force the choose mode. If it is nonzero,   *
 * it is adopted as the choose mode.  Otherwise, the choose     *
 * mode is taken from the top of stack choose_st.		*
 ****************************************************************/

PRIVATE EXPR* make_case_p(int branch, int try, EXPR *cond, 
			 EXPR *body, EXPR *rest)
{
  ATTR_TYPE w = top_choose_info(choose_st)->which;
  if(branch != 0) w = branch;

  if(w >= ONE_MIXED_ATT) {
    return all_case_p(w, try, cond, body, rest);
  }
  else {
    return first_case_p(w, try, cond, body, rest);
  }
}


/****************************************************************
 *			RECORD_ELSE_EXPR			*
 ****************************************************************
 * Record expression body as an else expression in a choose	*
 * or loop in the top frame of choose_st, and install a status	*
 * variable there if appropriate.				*
 ****************************************************************/

PRIVATE void
record_else_expr(EXPR *body, int line)
{
  CHOOSE_INFO* inf = top_choose_info(choose_st);
  ATTR_TYPE   mode = inf->which;

  bump_expr(inf->else_exp = body);
  if(mode >= ALL_ATT) {
    bump_expr(inf->status_var = id_expr(HIDE_CHAR_STR "status", line));
  }
}


/****************************************************************
 *			MAKE_WHILE_CASES_P			*
 ****************************************************************
 * Return an expression that represents a loopcase in a loop.   *
 * The case has the form					*
 *								*
 *   loopcase cond1						*
 *   ...							*
 *   loopcase condn => caseBody continue cont [when 'when']	*
 *   cases (remaining cases)					*
 *								*
 * where cond1,...,condn are contained in guards.		*
 * Parameter when is NULL if the when-phrase is omitted.	*
 * Arrow overrides the choose-mode, if not 0.			*
 ****************************************************************/

EXPR* make_while_cases_p(CLASS_UNION_CELL *guards, int arrow, EXPR *caseBody, 
			 EXPR *cont, EXPR *when, EXPR *cases, int line)
{
  EXPR *guard, *result, *rest, *body;
  int succeed;
  ATTR_TYPE case_kind;

  if(guards->u.next != NULL) {
    rest = make_while_cases_p(guards->u.next, arrow, caseBody, cont, 
			      when, cases, line);
  }
  else rest = cases;

  body = possibly_apply(caseBody, cont, line);
  case_kind = (ATTR_TYPE) guards->tok;
  if(case_kind == WHILE_ELSE_ATT) {
    CHOOSE_INFO* inf = top_choose_info(choose_st);
    ATTR_TYPE   mode = inf->which;
    if(when != NULL) {
      syntax_error(BAD_WHEN_ERR, when->LINE_NUM);
    }
    if(mode >= ONE_MIXED_ATT) {
      record_else_expr(body, line);
      result = NULL;
    }

    result = body;
  }
  else {
    get_succeed_guard_p(guards->special, guards->CUC_EXPR, &succeed, 
			&guard, when, line);
    result = make_case_p(arrow, succeed, guard, body, rest);
  }
  return result;
}


/****************************************************************
 *			MAKE_CASES_P				*
 ****************************************************************
 * Return an expression that represents a case in a choose or 	*
 * loop expression.  (For a loop, this is an exitcase.) 	*
 * The case has the form					*
 *								*
 *   case cond1							*
 *   ...							*
 *   case condn => caseBody [when when]				*
 *   cases (remaining cases)					*
 *								*
 * (or similar form with exitcase in a loop)			*
 * where cond1,...,condn are guards, stored in a chain of	*
 * class-union-cells.						*
 *								*
 * The choose mode information is found on the top of 		*
 * stack choose_st.						*
 *								*
 * Expression when is NULL if the when-phrase is omitted	*
 *								*
 * Arrow overrides the choose-mode, if not 0.			*
 ****************************************************************/

EXPR* make_cases_p(CLASS_UNION_CELL *guards, int arrow, EXPR *caseBody, 
		   EXPR *when, EXPR *cases, int line)
{
  EXPR *guard, *result, *rest;
  int succeed;
  ATTR_TYPE case_kind;

  /*---------------------------------------------------------------*
   * The cases are built separately for now.  Make the cases that  *
   * follow the first case.					   *
   *---------------------------------------------------------------*/

  if(guards->u.next != NULL) {
    rest = make_cases_p(guards->u.next, arrow, caseBody, when, cases, line);
  }
  else rest = cases;

  /*---------------------------------------------------------------*
   * Now build the first of the cases (with the first guard).	   *
   * First, make sure there is not a when phrase with an else case.*
   *---------------------------------------------------------------*/

  case_kind = (ATTR_TYPE) guards->tok;
  if(case_kind == ELSE_ATT || case_kind == UNTIL_ELSE_ATT) {
    CHOOSE_INFO* inf = top_choose_info(choose_st);
    ATTR_TYPE   mode = inf->which;
    if(when != NULL) {
      syntax_error(BAD_WHEN_ERR, when->LINE_NUM);
    }
    if(mode >= ONE_MIXED_ATT) {
      record_else_expr(caseBody, line);
      result = NULL;
    }
    else result = caseBody;
  }
  else {
    get_succeed_guard_p(guards->special, guards->CUC_EXPR, &succeed, 
			&guard, when, line);
    result = make_case_p(arrow,succeed,guard,caseBody,rest);
  }

  return result;
}


/****************************************************************
 *			MAKE_DEFINE_ELSE_CASE_P			*
 ****************************************************************
 * Return the code for						*
 *								*
 *   else lhs = rhs						*
 *								*
 * in a definition by cases.  If appropriate, also put 		*
 * information in the top frame of choose_st.			*
 ****************************************************************/

EXPR* make_define_else_case_p(EXPR *lhs, EXPR *rhs)
{
  CHOOSE_INFO* inf = top_choose_info(choose_st);

  if(inf->which >= ONE_MIXED_ATT) {
    EXPR* this_case = make_define_case_p(lhs, rhs, NULL, NULL);
    if(this_case != NULL) record_else_expr(this_case, this_case->LINE_NUM);
    return NULL;
  }
  else {
    EXPR* rest = make_else_expr_p(inf->which, NO_CASE_EX, current_line_number);
    return make_define_case_p(lhs, rhs, NULL, rest);
  }
}


/****************************************************************
 *			MAKE_ELSE_EXPR_P			*
 ****************************************************************
 * Make an implicit else expression for a choose- or 		*
 * loop-expression with mode n.	 Parameter exc_num is the	*
 * number of the exception to fail with if the else is reached,	*
 * in the case where the mode is first or one or one mixed.	*
 *								*
 * When the mode n is ALL_ATT or ALL_MIXED_ATT, the result is   *
 * NULL, since in those cases there is no implicit else.	*
 ****************************************************************/

EXPR* make_else_expr_p(ATTR_TYPE n, int exc_num, int line)
{
  EXPR *result;

  if(n >= ALL_ATT) return NULL;

  if(n == PERHAPS_ATT) return same_e(hermit_expr, line);

  result         = new_expr1(CONST_E, NULL_E, line);
  result->SCOPE  = FAIL_CONST;
  result->OFFSET = exc_num;
  return result;
}


/****************************************************************
 *			MAKE_EMPTY_ELSE_P			*
 ****************************************************************
 * Return the default else case for the current choose/loop	*
 * expression.  The top of choose_st describes this choose/loop,* 
 * and the top of case_kind_st tells what kind of case the	*
 * immediately preceding case was.				*
 ****************************************************************/

EXPR* make_empty_else_p(void)
{
  CHOOSE_INFO* inf = top_choose_info(choose_st);
  int ck = top_i(case_kind_st);
  ATTR_TYPE mode = inf->which;
  EXPR *else_ex, *result;

  result = NULL;   /* default */
  if(mode == ONE_MIXED_ATT) {
    bump_expr(inf->else_exp = 
	      make_else_expr_p(mode, NO_CASE_EX, current_line_number));
  }
  else if(mode < ALL_ATT) {
    if(ck != ELSE_ATT && ck != UNTIL_ELSE_ATT &&
       ck != WHILE_ELSE_ATT) {
      result = make_else_expr_p(mode, NO_CASE_EX, current_line_number);
    }
  }
  return result;
}


/****************************************************************
 *		       CASE_CUC_P 		      		*
 ****************************************************************
 * Build and return a class-union-cell in a form appropriate	*
 * for make_cases_p. The case has the form			*
 *								*
 *   case guard =>						*
 *								*
 * if succeed is 0, or						*
 *								*
 *   case do mode guard =>					*
 *								*
 * if succeed is nonzero.  In the latter case, succeed is the	*
 * control kind (TRY_F, etc.) that indicates how to do the	*
 * try.								*
 *								*
 * case_kind tells the kind of case (such 			*
 * as CASE_ATT, UNTIL_ATT, WHILE_ATT, etc.).			*
 ****************************************************************/

CLASS_UNION_CELL* case_cuc_p(ATTR_TYPE case_kind, int succeed, 
			     EXPR *guard, int line)
{
  CLASS_UNION_CELL *cuc;

  cuc 		= allocate_cuc();
  bump_expr(cuc->CUC_EXPR = guard);
  cuc->special 	= succeed;
  cuc->tok 	= (int) case_kind;
  cuc->u.next 	= NULL;
  cuc->line 	= line;
  return cuc;
}


/****************************************************************
 *		       ATTACH_DEFINE_CASE_TAG_P       		*
 ****************************************************************
 * A let expression has the form 				*
 *								*
 *    Let e:t ?a ?b ... = body.					*
 *								*
 * where rest is the list [?a,?b,...].  Move the tag t to	*
 * the members of rest and to body, as appropriate, returning 	*
 * the new list for rest, and setting outbody to the new body.  *
 * Line is the line for error reporting.  outbody is not ref 	*
 * counted.							*
 ****************************************************************/

PRIVATE EXPR_LIST *
attach_define_case_tag_p(TYPE *t, EXPR_LIST *rest, EXPR *body,
		         EXPR **outbody, int line)
{
  EXPR *e1;

  if(rest == NIL) {

    /*-------------------------*
     * Have define e:t = body. *
     *-------------------------*/

    *outbody = same_e(body, body->LINE_NUM);  
    bump_type((*outbody)->ty = t);
    return NIL;
  }

  if(TKIND(t) != FUNCTION_T) {
    semantic_error(BAD_DEF_HEAD_ERR, line);
    *outbody = body;
    return rest;
  }
  
  e1 = same_e(rest->head.expr, rest->head.expr->LINE_NUM);
  bump_type(e1->ty = t->TY1);
  return expr_cons(e1, attach_define_case_tag_p(t->TY2, rest->tail, body,
						outbody, line));
}


/****************************************************************
 *		       GET_DEFINE_CASE_PARTS_P       		*
 ****************************************************************
 * Return a list [?a,?b,?c & rest] if heading is f ?a ?b ?c.	*
 * Set outbody to the new body, obtained by moving tags from 	*
 * the heading to the body where appropriate.  outbody is 	*
 * ref counted.  Also check that the main function has the 	*
 * right name (top_i(defining_id_st)).				*
 ****************************************************************/

EXPR_LIST* 
get_define_case_parts_p(EXPR *heading, EXPR_LIST *rest, EXPR *body,
			EXPR **outbody)
{
  EXPR_TAG_TYPE kind;
  EXPR_LIST *result;
  EXPR *newoutbody;

  bump_list(rest);
  bump_expr(*outbody = body);

# ifdef DEBUG
    if(trace_exprs > 1) {
      trace_t(588);
      print_expr(heading, 1);
    }
# endif

  /*-----------------------------------------------------------*
   * Pull type tags off the heading, and put them on the body. *
   *-----------------------------------------------------------*/

  while(EKIND(heading) == SAME_E) {
    SET_LIST(rest, attach_define_case_tag_p(heading->ty, rest, *outbody,
					    &newoutbody, heading->LINE_NUM));
    set_expr(outbody, newoutbody);
    heading = heading->E1;
  }

  kind = EKIND(heading);

  /*------------------------------------------------------------*
   * If this is a definition of a function, then add the last 	*
   * parameter to the front of rest, and recur on the function	*
   * being applied.  This will get all of the parameters into	*
   * rest.							*
   *------------------------------------------------------------*/

  if(EKIND(heading) == APPLY_E) {
    result = get_define_case_parts_p(heading->E1, 
				     expr_cons(heading->E2, rest),
				     *outbody, &newoutbody);
    set_expr(outbody, newoutbody);
    drop_expr(newoutbody);
  }

  /*--------------------------------------------------------------------*
   * If the heading is not an APPLY_E, then it should be an identifier. *
   * If not, complain.							*
   *--------------------------------------------------------------------*/

  else {
    char* fun_name = top_str(defining_id_st);
    if(kind != LOCAL_ID_E && kind != GLOBAL_ID_E && kind != IDENTIFIER_E
       || heading->STR != fun_name) {
      syntax_error(BAD_DEF_HEAD_ERR, heading->LINE_NUM);
      if(fun_name != NULL && heading->STR != fun_name) {
        syntax_error1(NAME_MISMATCH_ERR, fun_name, heading->LINE_NUM);
      }
    }
    result = rest;
  }
  bump_list(result);
  drop_list(rest);
  if(result != NULL) result->ref_cnt--;
  return result;
}


/****************************************************************
 *			MAKE_DEFINE_CASE_P	       		*
 ****************************************************************
 * Return an expression that represents a define or let case    *
 * of the form							*
 *								*
 *   case 'heading' = 'body' when 'when'			*
 *   rest (remaining cases)					*
 *								*
 * If the choice mode (from the top of choose_st) is ALL_ATT,	*
 * ALL_MIXED_ATT or ONE_MIXED_ATT, then rest can be NULL,	*
 * indicating that there are no remaining cases.		* 
 ****************************************************************/

EXPR* make_define_case_p(EXPR *heading, EXPR *body, EXPR *when, EXPR *rest)
{
  ATTR_TYPE w;
  int l;
  EXPR *match = NULL, *pat = NULL, *newbody = NULL, *result;
  EXPR_LIST *pat_list;
  CHOOSE_INFO *inf;

  bump_expr(body);
  bump_expr(heading);
  bump_expr(rest);
  bump_expr(when);

  /*--------------------------------------------------------------------*
   * Get the pattern and the body, where the pattern is given as a	*
   * list.  For example, if the definition is		 		*
   *									*
   *    case f (?x,?y) ?z = body					*
   *									*
   * then pat_list will be [(?x,?y), ?z] and newbody will be body.	*
   * In general, new_body will be the same as body, but type tags on 	*
   * the patterns are transfered to the body where necessary.  		*
   *									*
   * If the definition is 						*
   *									*
   *   case x = ..., 							*
   *									*
   * then pat_list will be nil, and new_body will be body.		*
   *--------------------------------------------------------------------*/

  bump_list(pat_list = get_define_case_parts_p(heading, NIL, body, &newbody));
  l = list_length(pat_list);

  /*------------------------------------------------------------*
   * If the number of parameters in this case does not match 	*
   * number in preceding cases, then complain.  If this is the  *
   * first case, then num_params will be -1 (set in parser.c).  *
   *------------------------------------------------------------*/

  if(num_params >= 0 && l != num_params) {
    syntax_error(DEF_CASE_PARAM_MISMATCH_ERR, heading->LINE_NUM);
    drop_expr(body);
    drop_expr(heading);
    drop_expr(rest);
    drop_expr(when);
    return bad_expr;
  }

  /*--------------------------------------------------------------------*
   * Record the number of parameters for the next case. Get the		*
   * kind w of choice (FIRST_ATT, ONE_ATT, ALL_ATT, ALL_MIXED_ATT, 	*
   * etc.								*
   *--------------------------------------------------------------------*/

  num_params = l;
  inf = top_choose_info(choose_st);
  w   = inf->which;

  /*----------------------------------------------------*
   * If this definition has the form			*
   *							*
   *   case x = body					*
   *							*
   * (not definining a function)			*
   * then build one of the following.  			*
   *----------------------------------------------------*
   * Case where w = FIRST_ATT or ONE_ATT:		*
   *   then build expression				*
   *							*
   *     If when then body else rest %If		*
   *							*
   *   provided when != NULL, and build			*
   *							*
   *     body						*
   *							*
   *   if when is NULL.					*
   *----------------------------------------------------*
   * Case where w == ALL_ATT:				*
   *   build expression					*
   *							*
   *     Stream commit body then rest %Stream		*
   *							*
   *   provided when is NULL, and			*
   *							*
   *     Stream						*
   *         {when} commit body				*
   *       then rest					*
   *     %Stream					*
   *							*
   *   if when is not NULL.  See the description of	*
   *   all_case_p for the meaning of commit. 		*
   *   If rest is NULL, then don't build the stream,	*
   *   only what is shown as the first part of the	*
   *   stream.						*
   *----------------------------------------------------*
   * Case where w == ALL_MIXED_ATT:			*
   *   Same as for ALL_ATT,but use a Mix instead of	*
   *   a Stream.					*
   *----------------------------------------------------*
   * Case where w == ONE_MIXED_ATT:			*
   *   build expression					*
   *							*
   *     Cut(). body					*
   *							*
   *   provided when is NULL, and			*
   *							*
   *     Mix						*
   *         {when} Cut(). body				*
   *       with rest					*
   *     %Mix						*
   *							*
   *   if when is not NULL.  If rest is NULL, then	*
   *   don't build the mix.				*
   *----------------------------------------------------*/

  if(l == 0) {
    if(w >= ONE_MIXED_ATT) {
      result = all_case_p(w, 0, when, body, rest);
    }
    else if(when == NULL) result = body;
    else {
      result = first_case_p(w, 0, when, body, rest);
    }
  }

  /*----------------------------------------------------*
   * If this is a definition of a function, of the form *
   *							*
   *   case f(p) = body					*
   *							*
   * then build the following.				*
   *----------------------------------------------------*
   * If w = FIRST_ATT or ONE_ATT:			*
   *							*
   *   Try Match p = target. 				*
   *     then body 					*
   *     else rest 					*
   *   %Try						*
   *							*
   * where target is the expression on the top of	*
   * choose_from_st.  However, if when is not NULL, 	*
   * then replace pattern p by				*
   *							*
   *   p _where_ when.					*
   *							*
   *----------------------------------------------------*
   * In the case where w = ALL_ATT or ALL_MIXED_ATT	*
   * or ONE_MIXED_ATT, build a stream or mix instead	*
   * of a try.						*
   *							*
   * In this case, it is possible for rest to be NULL.  *
   * Then don't build a stream or mix, just use the	*
   * pattern match and the body.			*
   *----------------------------------------------------*/

  else {

    /*------------------------------------------*
     * Build the pattern from the pattern list. *
     *------------------------------------------*/

    bump_expr(pat = list_to_expr(pat_list, 7));
    if(when != NULL) {
      SET_EXPR(pat, new_expr2(WHERE_E, pat, when, pat->LINE_NUM));
    }

    /*----------------------------------------------------------*
     * Add the pattern to working_choose_matching_list for the  *
     * pattern match completeness checker. 			*
     *----------------------------------------------------------*/

    {LIST** wpl_head = &(inf->working_choose_matching_list);
     set_list(wpl_head, expr_cons(pat, *wpl_head));
    }

    /*-----------------------------*
     * Build the match expression. *
     *-----------------------------*/

    match = new_expr2(MATCH_E, pat, 
		      wsame_e(inf->choose_from, heading->LINE_NUM), 
		      pat->LINE_NUM);
  
    /*-----------------*
     * Build the case. *
     *-----------------*/

    w = inf->which;
    if(w >= ONE_MIXED_ATT) {
      result = all_case_p(w, TRY_F, match, newbody, rest);
    }
    else {
      result = first_case_p(w, TRY_F, match, newbody, rest);
    }
  }

  /*------------------*
   * Drop and return. *
   *------------------*/

  bump_expr(result);
  drop_expr(heading);
  drop_expr(body);
  drop_expr(rest);
  drop_expr(when);
  drop_expr(newbody);
  drop_list(pat_list);
  drop_expr(pat);
  if(result != NULL) result->ref_cnt--;
  return result;
}


/****************************************************************
 *			ADD_THEN_TO_BODY			*
 ****************************************************************
 * Parameter then_expr is a pair (pat,action).  Build the	*
 * following expression, and return it.				*
 *								*
 *     open Let .result = body_expr.				*
 *     Try							*
 *       Match pat = .result.					*
 *       action:()						*
 *     %Try							*
 *     .result							*
 ****************************************************************/

EXPR* add_then_to_body(EXPR *body_expr, EXPR *then_expr)
{
  int   body_line     = body_expr->LINE_NUM;
  int   then_line     = then_expr->LINE_NUM;
  EXPR* result_id     = id_expr(HIDE_CHAR_STR "result", body_line);
  EXPR* let           = new_expr2(LET_E, result_id, body_expr, body_line);
  EXPR* pat           = then_expr->E1;
  EXPR* action        = then_expr->E2;
  EXPR* match         = new_expr2(MATCH_E, pat, result_id, body_line);
  EXPR* tagged_action = same_e(action, action->LINE_NUM);
  EXPR* herm          = same_e(hermit_expr, then_line);
  EXPR* try_exp       = try_expr(apply_expr(match, tagged_action, then_line), 
				 herm, herm, TRY_F, then_line);
  EXPR* result        = apply_expr(let, 
				   apply_expr(try_exp, result_id, then_line),
				   body_line);

  let->SCOPE = 1;      /* Open the scope */
  bump_type(tagged_action->ty = hermit_type);
  return result;
}


/****************************************************************
 *			MAKE_DEF_BY_CASES_P			*
 ****************************************************************
 * Make an expression of the form				*
 *								*
 *   Define hd by ...						*
 *								*
 * or								*
 *								*
 *   Let hd by ...						*
 *								*
 * that defines a function, where				*
 *								*
 *   hd         is the identifier being defined, or is an       *
 * 		application of the function being defined to    *
 *		formals.					*
 *								*
 *   image 	is the target to match against			*
 *								*
 *   defkind 	is LET_E or DEFINE_E				*
 *								*
 *   body 	is the list of cases				*
 *								*
 *   num_params is global, and must be set to the number of 	*
 *		(curried) parameters in each case.		*
 *								*
 *   the top of choose_st tells the selection kind 		*
 *   		(first, one, all, ...)				*
 ****************************************************************/

EXPR* make_def_by_cases_p(EXPR *hd, EXPR *image, EXPR *body, int defkind)
{
  EXPR *let_expr, *body_expr, *patvar, *extra_formals, *sshd, *defn;
  EXPR_LIST *formals, *reverse_formals, *l;
  int i;
  char name[12];
			 
  /*----------------------------------*
   * Build the formal parameter list. *
   *----------------------------------*/

  formals = NIL;
  for(i = 0; i < num_params; i++) {
    sprintf(name, "%s%d", HIDE_CHAR_STR "formal", formal_num++);
    formals = expr_cons(id_expr(name, hd->LINE_NUM), formals);
  }
  bump_list(formals);
  bump_list(reverse_formals = reverse_list(formals));

  /*---------------------------------------------------------------*
   * Build the target of the pattern match, and attach the builder *
   * to the front of body. 					   *
   *---------------------------------------------------------------*/

  let_expr  = new_expr2(LET_E, image, list_to_expr(reverse_formals, 7), 
		        hd->LINE_NUM);
  body_expr = build_choose_p(let_expr, body, FALSE, hd->LINE_NUM);

  /*---------------------------------------------------------------*
   * When id is an application, we will need to bind its formals.  *
   * extra_formals points to the application whose argument is the *
   * next formal to bind, going inside out.  If extra_formals is   *
   * not an application, there is an error.  If extra_formals      *
   * is NULL, there are no extra formals.			   *
   *---------------------------------------------------------------*/

  sshd          = skip_sames(hd);
  extra_formals = (EKIND(sshd) == IDENTIFIER_E) ? NULL : sshd;

  /*------------------------------------------------------------*
   * Build the function body, by adding the pattern matches	*
   * to the front of the cases. 				*
   *------------------------------------------------------------*/

  for(i = 0, l = formals; i < num_params; i++, l = l->tail) {

    /*-----------------------------------------------------------*
     * Get the pattern variable for this function.  It is one of *
     * the .formal variables.					 *
     *-----------------------------------------------------------*/

    patvar = var_to_pat_var(l->head.expr);
    patvar->LINE_NUM = hd->LINE_NUM;

    /*----------------------------------------------*
     * If there is an extra match to do, do it now. *
     *----------------------------------------------*/

    if(extra_formals != NULL) {
      if(EKIND(extra_formals) != APPLY_E) {
        syntax_error(DEF_CASE_PARAM_MISMATCH_ERR, extra_formals->LINE_NUM);
      }
      else {
        EXPR* match   = new_expr2(MATCH_E, extra_formals->E2, patvar->E1, 
			          extra_formals->LINE_NUM);
        body_expr     = apply_expr(match, body_expr, match->LINE_NUM);
	extra_formals = skip_sames(extra_formals->E1);
      }
    }    

    /*--------------------------------------------------------------*
     * Build the function. It shares an environment with enclosing  *
     * function, unless it is the outermost function.  The 	    *
     * outermost function is patched to have OFFSET 0 just before   *
     * returning.           					    *
     *--------------------------------------------------------------*/

    body_expr = new_expr2(FUNCTION_E, patvar, body_expr, hd->LINE_NUM);
    body_expr->OFFSET = 1;  /* Share environment */
  }

  /*-----------------------*
   * Build the definition. *
   *-----------------------*/

  {EXPR* id = hd;
   EXPR* ssid = sshd;
   while(EKIND(ssid) == APPLY_E) {
     id   = ssid->E1;
     ssid = skip_sames(id);
   }
   if(EKIND(ssid) != IDENTIFIER_E) {
     syntax_error(BAD_DEF_HEAD_ERR, hd->LINE_NUM);
   }
   if(EKIND(body_expr) == FUNCTION_E) body_expr->OFFSET = 0;
   defn = new_expr2(defkind, id, body_expr, hd->LINE_NUM);
  }

  /*------------------*
   * Drop and return. *
   *------------------*/

  drop_list(formals);
  drop_list(reverse_formals);
  return defn;
}


/****************************************************************
 *			CHECK_FOR_CASES				*
 ****************************************************************
 * Complain if e consists of just an implicit else case.	*
 ****************************************************************/

void check_for_cases(EXPR *e, int line)
{
  if(e == NULL || 
     (EKIND(e) == CONST_E && e->SCOPE == FAIL_CONST) ||
     is_hermit_expr(e)) {
    syntax_error(NO_CASES_ERR, line);
  }
}


/****************************************************************
 *			SHORT_CONTINUE_P			*
 ****************************************************************
 * Make an expression for 'continue', without any following 	*
 * expr.							*
 ****************************************************************/

EXPR* short_continue_p()
{
  EXPR  *loop, *result;

  loop = top_choose_info(choose_st)->loop_ref;
  if(loop->E1 != NULL_E) {
    semantic_error(EMPTY_CONT_ERR, 0);
  }

  result      = new_expr1(RECUR_E, NULL_E, current_line_number);
  result->E2  = loop;  /* Not ref counted -- cyclic link */
  result->pat = 0;
  return result;
}


/****************************************************************
 *			GET_SUCCEED_GUARD_P			*
 ****************************************************************
 * Set *guard to the guard expression of a choose or loop 	*
 * expression of the form					*
 *								*
 *   case gd =>							*
 *								*
 * if try is 0, or						*
 *								*
 *   case do mode gd =>						*
 *								*
 * if try is nonzero.  In the latter case, try is the control	*
 * kind indicated by the mode (TRY_F, etc.).			*
 *								*
 * If this is in a choose-matching or 				*
 * loop-matching expression, need to make a match.		*
 *								*
 * Set *succeed to 0 if expression *guard should be tested	*
 * using an if, and to a control mode (TRY_F, etc.) if *guard	*
 * should be tested using a try.				*
 *								*
 * Build the guard having line line.				*
 ****************************************************************/

PRIVATE void 
get_succeed_guard_p(int try, EXPR *gd, int *succeed, EXPR **guard,
		    EXPR *when, int line)
{
  CHOOSE_INFO* inf = top_choose_info(choose_st);
  if(inf->match_kind) {
    if(when != NULL) {
      gd = new_expr2(WHERE_E, gd, when, gd->LINE_NUM);
    }
    *guard = new_expr2(MATCH_E, gd, 
		      wsame_e(inf->choose_from, line),
		      line);
    *succeed = TRY_F;
  }
  else {
    *succeed = try;
    *guard = gd;
    if(when != NULL) {
      syntax_error(BAD_WHEN_ERR, when->LINE_NUM);
    }
  }
}


/****************************************************************
 *			MAKE_DO_DBLARROW_CASE_P			*
 ****************************************************************
 * Make a case 							*
 *    do a => b							*
 *    rest 							*
 * in a choose-matching expr.					*
 ****************************************************************/

EXPR* make_do_dblarrow_case_p(EXPR *a, EXPR *b, EXPR *rest, int line)
{
  CHOOSE_INFO* inf = top_choose_info(choose_st);
  EXPR* match = 
    new_expr2(MATCH_E, a, 
	      wsame_e(inf->choose_from,line),
	      line);
  EXPR* do_part = apply_expr(match, b, line);
  return apply_expr(do_part, rest, line);
}


/****************************************************************
 *			MAKE_UNIFY_PATTERN			*
 ****************************************************************
 * heading should be an application of a function, such as	*
 * (f p) or (f p q), etc.					*
 *								*
 * If eqkind is ARROW1_ATT then destructively replace each of   *
 * the patterns by an application of active to that pattern.    *
 * For example, (f p) becomes (f (active(p)), 			*
 * (f p q) becomes (f (active(p)) (active(q))).			*
 *								*
 * If eqkind is anything else, then destructively replace	*
 * each pattern p by active?(p)					*
 *								*
 * If heading is not an application of a function, do not 	*
 * change it.							*
 ****************************************************************/

void make_unify_pattern(EXPR *heading, int eqkind, int line)
{
  int id_index = 
    (eqkind == ARROW1_ATT)
      ? ACTIVE_ID
      : ACTIVE_PRIME_ID;
  EXPR* ssheading = skip_sames(heading);
  EXPR* arg, *newarg;

  while(EKIND(ssheading) == APPLY_E) {
    arg    = ssheading->E2;
    newarg = apply_expr(id_expr(std_id[id_index], line), arg, line);
    SET_EXPR(ssheading->E2, newarg);
    ssheading = skip_sames(ssheading->E1);
  }
}


/****************************************************************
 *			MAKE_FOR_PART_P				*
 ****************************************************************/

PRIVATE EXPR* make_for_part_p(EXPR_LIST *iterators, EXPR *body)
{
  EXPR* e     = iterators->head.expr;
  EXPR* inner = new_expr3(FOR_E, e->E1, e->E2, body, e->E1->LINE_NUM);

  if(iterators->tail == NIL) return inner;
  return make_for_part_p(iterators->tail, inner);
}


/****************************************************************
 *			MAKE_FOR_EXPR_P				*
 ****************************************************************
 * Make a for-expression.					*
 ****************************************************************/

EXPR* make_for_expr_p(EXPR_LIST *iterators, EXPR_LIST *body)
{
  EXPR *rest, *this;

  rest = make_for_part_p(iterators, body->head.expr);
  if(body->tail == NIL) return rest;
  this = make_for_expr_p(iterators, body->tail);
  return apply_expr(this, rest, this->LINE_NUM);
}


/****************************************************************
 *			ADD_LIST_TO_CHOOSE_MATCHING_LISTS_P	*
 ****************************************************************
 * Let choose_from be top_choose_info(choose_st)->choose_from.  *
 *								*
 * Add list pl, prefixed by fun and choose_from, to		*
 * choose_matching_lists.  For example, if pl is [p1,p2] and    *
 * choose_from is t, then list [fun,t,p1,p2] is			*
 * pushed onto the front of choose_matching_lists.		*
 *								*
 * See patmatch/pmcompl.c for a discussion of why this is being *
 * done.  		       					*
 ****************************************************************/

void add_list_to_choose_matching_lists_p(EXPR *fun, EXPR_LIST *pl)
{
  char* fun_name = (fun == NULL) ? NULL : skip_sames(fun)->STR;
  CHOOSE_INFO* inf = top_choose_info(choose_st);

  SET_LIST(choose_matching_lists,
	   list_cons(
	     str_cons(fun_name, expr_cons(inf->choose_from, pl)),
	     choose_matching_lists));
}


/****************************************************************
 *			BUILD_CHOOSE_P				*
 ****************************************************************
 * Build a choose or loop expression from the given parts.  The *
 * parameters are as follows.					*
 *								*
 *  target_builder	This is either NULL or is a let expr 	*
 *			that binds an identifier to the target  *
 *			of the matches that are done in the 	*
 *			choose or loop.				*
 *								*
 *  cases		This is the body of the choose, which	*
 *			performs the cases.			*
 *								*
 *  subcase		This is true if this choose is actually	*
 *			from a subcases construct.		*
 ****************************************************************/

EXPR* build_choose_p(EXPR *target_builder, EXPR *cases, Boolean subcase, 
		     int line)
{
  EXPR* result;
  CHOOSE_INFO* inf = top_choose_info(choose_st);

  bump_expr(result = cases);

  /*----------------------------------------------------*
   * If there is a deferred else expression, then   	*
   * add it.  A deferred else is added as an		*
   * alternative in a stream.  So set result to		*
   *							*
   *   Stream						*
   *     result 					*
   *   then						*
   *     else_exp					*
   *   %Stream						*
   *							*
   * But if the status_var for this choose is nonnull,  *
   * then instead use					*
   *							*
   *   Stream						*
   *     result 					*
   *   then						*
   *     ChooseTest(status_var).			*
   *     else_exp					*
   *   %Stream						*
   *----------------------------------------------------*/

  if(inf->else_exp != NULL) {
    EXPR* else_exp = inf->else_exp;

    if(inf->status_var != NULL) {
      TYPE* choosetest_type = function_t(box_t(hermit_type), hermit_type);
      EXPR* choosetest_id = 
            typed_global_sym_expr(CHOOSETEST_ID, choosetest_type, line);
      EXPR* dotest = apply_expr(choosetest_id, inf->status_var, line);
      else_exp = apply_expr(dotest, else_exp, line);
    }

    SET_EXPR(result, new_expr2(STREAM_E, result, else_exp, line));
    result->STREAM_MODE = STREAM_ATT;
    result->mark = 1;
  }

  /*-----------------------------------*
   * Complain if there are no cases.   *
   *-----------------------------------*/

  check_for_cases(result, line);

  /*------------------------------------------------*
   * If this is a choose-one-mixed expression, then *
   * wrap it inside a CutHere construct.	    *
   *------------------------------------------------*/

  if(inf->which  == ONE_MIXED_ATT) {
    SET_EXPR(result, cuthere_expr(result));
  }

  /*------------------------------------------------------------*
   * If there is a status variable, then create it.  Replace	*
   * result by							*
   *								*
   *   Let status_var = emptySharedBox().			*
   *   result							*
   *------------------------------------------------------------*/

  if(inf->status_var != NULL) {
    EXPR* box_cnstr = global_sym_expr(EMPTYSHAREDBOX_ID, line);
    EXPR* thebox    = apply_expr(box_cnstr, hermit_expr, line);
    EXPR* let       = new_expr2(LET_E, inf->status_var, thebox, line);
    SET_EXPR(result, apply_expr(let, result, line));
  }

  /*------------------------------------------------------------*
   * If there is a target to build, then add code to build it.	*
   *------------------------------------------------------------*/

  SET_EXPR(result, possibly_apply(target_builder, result, line));

  /*----------------------------------------------*
   * Tag the choose expression as a choose 	  *
   * expression, provided it does not come from   *
   * subcases.					  *
   *----------------------------------------------*/

  if(!subcase) {
    SET_EXPR(result, same_e(result, line));
    result->SAME_MODE  = 6;
    result->SAME_CLOSE = 1;
  }

  if(result!= NULL) result->ref_cnt--;
  return result;
}


