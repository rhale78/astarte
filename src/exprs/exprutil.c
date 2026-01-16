/*********************************************************************
 * File:    exprs/exprutil.c
 * Purpose: Utilities for expressions.
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

/************************************************************************
 * This file contains assorted general and special purpose utilities	*
 * for expression trees.  Some utilities are specialized constructors,  *
 * some scan expressions, some modify expressions in certain ways, and  *
 * some classify expressions.  						*
 ************************************************************************/

#include <string.h>
#include "../misc/misc.h"
#include "../alloc/allocate.h"
#include "../exprs/expr.h"
#include "../utils/lists.h"
#include "../utils/strops.h"
#include "../classes/classes.h"
#include "../standard/stdtypes.h"
#include "../standard/stdids.h"
#include "../tables/tables.h"
#include "../clstbl/classtbl.h"
#include "../lexer/modes.h"
#include "../parser/parser.h"
#include "../error/error.h"
#include "../unify/unify.h"
#include "../evaluate/instruc.h"
#include "../ids/ids.h"
#include "../generate/generate.h"
#include "../generate/prim.h"
#include "../patmatch/patmatch.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif


/****************************************************************
 *		       SCAN_EXPR				*
 ****************************************************************
 * Execute f(a-addr) for each node a of expression *e, passing 	*
 * the address of the link to a to f.  Go into pattern		*
 * rules only if pr is true.  Skip addresses that contain NULL. *
 *								*
 * The traversal order is preorder.				*
 *								*
 * PAT_VAR_E nodes are treated as leaves.  No recursive call is *
 * done on the identifier beneath a PAT_VAR_E node.		*
 *                                                              *
 * The value returned by f indicates whether the children of *e *
 * should be scanned.  If f(e) = 0, then e's children are 	*
 * scanned.  Otherwise, they are not.				*
 ****************************************************************/

void scan_expr(EXPR **e, Boolean (*f)(EXPR **), Boolean pr)
{
  EXPR_TAG_TYPE kind;

  /*-------------------------------------*
   * The for-loop is for tail recursion. *
   *-------------------------------------*/

  for(;;) {
    if(e == NULL || *e == NULL) return;

    /*-----------------------------------------------------*
     * Call f at the root. Only continue with the children *
     * if f returns 0.					   *
     *-----------------------------------------------------*/

    kind = EKIND(*e);
    if(kind != BAD_E) if(f(e)) return;

    /*-------------------------*
     * Do the recursive calls. *
     *-------------------------*/

    switch(kind) {
      case CONST_E:
      case IDENTIFIER_E:
      case LOCAL_ID_E:
      case GLOBAL_ID_E:
      case OVERLOAD_E:
      case SPECIAL_E:
      case UNKNOWN_ID_E:
      case PAT_VAR_E:
      case BAD_E:
	return;

      case OPEN_E:
      case SINGLE_E:
      case RECUR_E:
      case SAME_E:
      case EXECUTE_E:
      case MANUAL_E:
      case LAZY_LIST_E:
	e = &((*e)->E1);
	break;                /* tail recur */

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
        scan_expr(&((*e)->E1), f, pr);
	e = &((*e)->E2);
	break;                     /* tail recur */

      case IF_E:
      case TRY_E:
      case FOR_E:
        scan_expr(&((*e)->E1), f, pr);
	scan_expr(&((*e)->E2), f, pr);
        e = &((*e)->E3);
	break;                     /* tail recur */

      case PAT_FUN_E:
#       ifdef NEVER
	  if(!pr) return;
#       endif
        e = &((*e)->E3);
	break;                     /* tail recur */

      case PAT_RULE_E: 
	if(!pr) return;
        scan_expr(&((*e)->E2), f, pr);
        scan_expr(&((*e)->E3), f, pr);
	e = &((*e)->E1);
	break;                    /* tail recur */

      default:
        die(10, (char *)tolong(EKIND(*e)));
    }
  }
}
    

/****************************************************************
 *		       SET_EXPR_LINE				*
 ****************************************************************
 * Change all of the line numbers in ex to line.		*
 ****************************************************************/

PRIVATE int set_expr_line_line;

PRIVATE Boolean set_expr_line_help(EXPR **e)
{
  (*e)->LINE_NUM = set_expr_line_line;
  return 0;
}

/*----------------------------------------------------------*/

void set_expr_line(EXPR *ex, int line)
{
  set_expr_line_line = line;
  scan_expr(&ex, set_expr_line_help, 0);
}


/****************************************************************
 *		       LET_EXPR_KIND				*
 ****************************************************************
 * Map attributes as follows.					*
 *    LET_ATT		=>	LET_E				*
 *    RELET_ATT		=>	LET_E				*
 *    DEFINE_ATT	=>	DEFINE_E			*
 ****************************************************************/

EXPR_TAG_TYPE let_expr_kind(ATTR_TYPE att)
{
  return att == DEFINE_ATT ? DEFINE_E : LET_E;
}


/****************************************************************
 *		       BIND_APPLY_EXPR				*
 ****************************************************************
 * Return (a b), except that NULL is treated as ().  Return 	*
 * NULL if both a and b are NULL.				*
 ****************************************************************/

EXPR* bind_apply_expr(EXPR *a, EXPR *b, int line)
{
  if(a == NULL) return b;
  if(b == NULL) return a;
  return apply_expr(a, b, line);
}


/****************************************************************
 *		       POSSIBLY_APPLY				*
 ****************************************************************
 * Return (a b), except if a = NULL or a = (), in which case 	*
 * return b.							*
 ****************************************************************/

EXPR* possibly_apply(EXPR *a, EXPR *b, int line)
{
  if(a == NULL || is_hermit_expr(a)) return b;
  return apply_expr(a,b, line);
}


/****************************************************************
 *			TAGGED_ID_P				*
 ****************************************************************
 * Return identifier id, tagged by type t.  Add a type 		*
 * assumption if appropriate.  t.type can be null, for no tag.  *
 *								*
 * id must be in the string table.				*
 ****************************************************************/

EXPR* tagged_id_p(char *id, RTYPE t, int line)
{
  EXPR *e;
  RTYPE assume_rt;

  e = new_expr1(IDENTIFIER_E, NULL_E, line);
  e->STR = id;

  /*-------------------------------*
   * Attach the type, if not null. *
   *-------------------------------*/

  if(t.type != NULL) {
    e = same_e(e, line);
    bump_type(e->ty = t.type);
    bump_role(e->role = t.role);
  }

  /*---------------------------------------*
   * If no type, then attach assumed type. *
   *---------------------------------------*/

  else {

    /*------------------------------------------------------------*
     * First try a local assumption.  If that fails, try a global *
     * assumption.						  *
     *------------------------------------------------------------*/

    assume_rt = get_local_assume_tm(id);
    if(assume_rt.type == NULL) assume_rt = assumed_rtype_tm(id);

    /*----------------------------------------*
     * If there is an assumption, install it. *
     *----------------------------------------*/

    if(assume_rt.type != NULL) {
      e = same_e(e, line);
      bump_type(e->ty = copy_type(assume_rt.type, 1));
      bump_role(e->role = assume_rt.role);
      e->SAME_MODE = 3;  /* Indicate assumption -- see expr.doc */
    }
  }
  return e;
}


/****************************************************************
 *			TAGGED_PAT_VAR_P			*
 ****************************************************************
 * Return pattern variable id, tagged by type t.  Add a type 	*
 * assumption if appropriate.  t.type can be null, for no tag.	*
 ****************************************************************/

EXPR* tagged_pat_var_p(char *id, RTYPE t, int line)
{
  EXPR *e, *pv;

  /*----------------------------------------------------------------*
   * Suppose that we are building ?x.  First build x. It might be a *
   * SAME_E node. 						    *
   *----------------------------------------------------------------*/

  bump_expr(e = tagged_id_p(id, t, line));

  /*----------------------------------------------------------------*
   * Now convert x to ?x.  Skip over the SAME_E nodes, and replace  *
   * the IDENTIFIER_E node by a PAT_VAR_E node.  Hang an identifier *
   * node beneath the PAT_VAR_E node.				    *
   *----------------------------------------------------------------*/

  e->pat   = 1;
  pv       = skip_sames(e);
  pv->kind = PAT_VAR_E;
  pv->pat  = 1;
  bump_expr(pv->E1 = id_expr(id, line));

  /*---------------------------------------------------------------*
   * If this is a procedure identifier, then tag it as a function  *
   * that produces ().						   *
   *---------------------------------------------------------------*/

  if(is_proc_id(id)) {
    SET_EXPR(e, same_e(e, line));
    bump_type(e->ty = function_t(NULL, hermit_type));
  }

  /*-----------*
   * Return e. *
   *-----------*/

  if(e != NULL) e->ref_cnt--;
  return e;
}


/****************************************************************
 *			ANON_PAT_VAR_P				*
 ****************************************************************
 * Return an anonymous pattern variable, tagged by type t.  	*
 * t.type can be null, for no tag.				*
 ****************************************************************/

EXPR* anon_pat_var_p(RTYPE t, int line)
{
  EXPR *e;

  bump_expr(e = new_expr1(PAT_VAR_E, NULL_E, line));
  e->pat = 1;
  e->STR = "#anon";
  bump_type(e->ty = t.type);
  bump_role(e->role = t.role);
  e->ref_cnt--;
  return e;
}


/****************************************************************
 *			NEW_PAT_VAR				*
 ****************************************************************
 * Return a new pattern variable named name.			*
 ****************************************************************/

EXPR* new_pat_var(char *name, int line)
{
  EXPR *p, *e;

  pat_vars_ok = TRUE;
  e           = new_expr1(LOCAL_ID_E, NULL_E, line);
  e->STR      = id_tb0(name);
  p           = new_expr1(PAT_VAR_E, e, line);
  p->pat      = 1;
  p->STR      = e->STR;
  pat_vars_ok = FALSE;
  return p;
}


/************************************************************************
 *				FRESH_ID_EXPR				*
 ************************************************************************
 * Return a new id with name s<n> where <n> is a number not used in	*
 * any other such id.							*
 ************************************************************************/

PRIVATE int next_fresh_id_num = 0;

EXPR* fresh_id_expr(char *s, int line)
{
  char name[MAX_ID_SIZE+4];

  sprintf(name, HIDE_CHAR_STR "%s%d", s, next_fresh_id_num++);
  return id_expr(name, line);
}


/****************************************************************
 *			FRESH_PAT_VAR				*
 ****************************************************************
 * Return a new pattern variable named nameN, where N is an     *
 * integer not used before in this declaration.			*
 ****************************************************************/

EXPR* fresh_pat_var(char *name, int line)
{
  char rname[MAX_ID_SIZE+4];

  sprintf(rname, "%s%d", name, next_fresh_id_num++);
  return new_pat_var(rname, line);
}


/****************************************************************
 *			VAR_TO_PAT_VAR				*
 ****************************************************************
 * id is an identifier.  Return pattern variable ?id.		*
 * The line number is the same as the line of id.		*
 ****************************************************************/

EXPR* var_to_pat_var(EXPR *id)
{
  EXPR* pv = new_expr1(PAT_VAR_E, id, id->LINE_NUM);
  pv->pat = 1;
  pv->STR = id->STR;
  return pv;
}


/****************************************************************
 *			TYPED_TARGET				*
 ****************************************************************
 * Return expression target, of type t, at line 'line'.		*
 *								*
 * Parameter patfun is the pattern function that this target    *
 * expression refers to.  It is the pattern function that is    *
 * being defined by the pattern declaration in which this 	*
 * target expression occurs.					*
 ****************************************************************/

EXPR* typed_target(TYPE *t, EXPR *patfun, int line)
{
  EXPR* target_expr;
  target_expr  	         = new_expr1t(SPECIAL_E, NULL_E, t, line);
  target_expr->E1        = patfun;  /* Not ref counted */
  target_expr->PRIMITIVE = PRIM_TARGET;
  return target_expr;
}


/****************************************************************
 *			ATTACH_TAG_P				*
 ****************************************************************
 * Return expression e with tag t attached to it.  Delete a	*
 * tag placed by an assumption if e is an identifier.		*
 ****************************************************************/

EXPR* attach_tag_p(EXPR *e, RTYPE t)
{
  EXPR **ep, *sse, *result;
  EXPR_TAG_TYPE kind;

  /*-------------------------------------------------*
   * Possibly delete assumptions from an identifier. *
   *-------------------------------------------------*/

  bump_expr(e);
  sse = skip_sames(e);
  kind = EKIND(sse);
  if(kind == IDENTIFIER_E || kind == GLOBAL_ID_E || kind == OVERLOAD_E
     || kind == LOCAL_ID_E || kind == PAT_FUN_E) {
    ep = &e;
    while(EKIND(*ep) == SAME_E) {
      if((*ep)->SAME_MODE == 3) {
	set_expr(ep, (*ep)->E1);
      }
      else ep = &((*ep)->E1);
    }
  }

  /*-------------------------------*
   * Add the tag in a SAME_E node. *
   *-------------------------------*/

  result = new_expr1t(SAME_E, e, t.type, e->LINE_NUM);
  bump_role(result->role = t.role);
  drop_expr(e);
  return result;
}


/****************************************************************
 *			BUILD_SPECIES_AS_VAL_EXPR		*
 ****************************************************************
 * Return an expression whose evaluation yields species T, as   *
 * a value.							*
 ****************************************************************/

EXPR* build_species_as_val_expr(TYPE *T, int line)
{
  EXPR* arg    = new_expr1t(SPECIAL_E, NULL, T, line);
  EXPR* result = new_expr1t(SPECIAL_E, arg, aspecies_type, line);
  result->PRIMITIVE = PRIM_SPECIES;
  return result;
}


/****************************************************************
 *			BUILD_STACK_EXPR			*
 ****************************************************************
 * Return a SPECIAL_E node with primitive PRIM_STACK.		*
 * Install the given type and line number.			*
 ****************************************************************/

EXPR* build_stack_expr(TYPE *ty, int line)
{
  EXPR* result = new_expr1t(SPECIAL_E, NULL_E, ty, line);
  result->PRIMITIVE = PRIM_STACK;
  result->STR = new_temp_var_name();
  return result;
}


/****************************************************************
 *			BUILD_CONS_P				*
 ****************************************************************
 * Return expression h :: t.					*
 ****************************************************************/

EXPR* build_cons_p(EXPR *h, EXPR *t)
{
  EXPR *cons, *result;
  int line = h->LINE_NUM;
  bump_expr(cons = 
	    typed_global_sym_expr(CONS_SYM, 
				 copy_type(cons_type,0), line));
  result = apply_expr(cons, new_expr2(PAIR_E, h, t, line), line);
  drop_expr(cons);
  return result;
}


/****************************************************************
 *			MAKE_WRAP				*
 ****************************************************************
 * Return <<e>>.						*
 ****************************************************************/

EXPR* make_wrap(EXPR *e, int line)
{
  EXPR* wrap = id_expr(std_id[WRAP_ID], line);
  return apply_expr(wrap, e, line);
}


/****************************************************************
 *			MAKE_AWAIT_EXPR				*
 ****************************************************************
 * Return expression 						*
 *                                                              *
 *    Await tests then body.					*
 *                                                              *
 * where the await is decorated with an & if nostore is 1.      *
 ****************************************************************/

EXPR* make_await_expr(EXPR *body, EXPR_LIST *tests, int nostore, int line)
{
  EXPR *test, *result;
  EXPR_LIST *testlist;
  if(tests == NIL) test = NULL;
  else {
    bump_list(testlist = 
	      append(tests, expr_cons(hermit_expr, NIL)));
    test = list_to_expr(testlist, 0);
  }
  result = new_expr2(AWAIT_E, test, body, line);
  if (test != NULL && expr_size(body) <= AWAIT_SIZE_CUTOFF) {
    result->DOUBLE_COMPILE = 1;
  }
  result->OFFSET = nostore;
  return result;
}


/****************************************************************
 *			MAKE_APPLY_P				*
 ****************************************************************
 * Make an application of name to the pair (e1, e2).		*
 ****************************************************************/

EXPR* make_apply_p(char *name, EXPR *e1, EXPR *e2)
{
  EXPR *id, *result;

  bump_expr(id = id_expr(name, e1->LINE_NUM));
  result = new_expr2(APPLY_E, id, 
	           new_expr2(PAIR_E, e1, e2, e1->LINE_NUM), 
	           e1->LINE_NUM);
  drop_expr(id);
  return result;  
}


/****************************************************************
 *			OP_EXPR_P				*
 ****************************************************************
 * Return expression e1 op e2, where op is a binary operator.	*
 * t is the type/role of the operator, or t.type = null if 	*
 * there is none given.						*
 ****************************************************************/

EXPR* op_expr_p(EXPR *e1, struct lstr op, RTYPE t, EXPR *e2)
{
  /*-----------------------*
   * Build the expression. *
   *-----------------------*/

  EXPR* fun    = tagged_id_p(op.name, t, op.line);
  EXPR* pair   = new_expr2(PAIR_E, e1, e2, e1->LINE_NUM);
  EXPR* result = apply_expr(fun, pair, e1->LINE_NUM);

  /*-------------------------------------------------*
   * Mark the pair open if this is an open operator. *
   *-------------------------------------------------*/

  Boolean open;
  operator_code_tm(op.name, &open);
  if(open) pair->SCOPE = 1;

  drop_rtype(t);
  drop_expr(e1);
  drop_expr(e2);
  return result;
}


/****************************************************************
 *			MAKE_ASK_EXPR				*
 ****************************************************************
 * Return the expr structure that represents 			*
 * 								*
 *    Ask agent => action %Ask					*
 *								*
 * If action is f x y, then the result is f agent x y.          *
 ****************************************************************/

EXPR* make_ask_expr(EXPR *agent, EXPR *action, int line)
{
  EXPR* result;
  EXPR_TAG_TYPE action_kind = EKIND(action);

  if(action_kind == APPLY_E) {
    result = apply_expr(make_ask_expr(agent, action->E1, line),
			action->E2,
			action->LINE_NUM);
  }
  else if(action_kind == SAME_E) {
    result = same_e(make_ask_expr(agent, action->E1, line), action->LINE_NUM);
    bump_type(result->ty = action->ty);
    
  }
  else {
    result = apply_expr(action, agent, line);
  }

  /*----------------------------------------------------------------*
   * Make the apply closed, since scope rules are that Ask ... %Ask *
   * should be closed.						    *
   *----------------------------------------------------------------*/

  result->SCOPE = 1;
  return result;
}


/****************************************************************
 *			BUILD_PROC_APPLY			*
 ****************************************************************
 * Return expression Proc a,b,c %Proc, where params is list     *
 * [a,b,c].  If there are no parameters, return (Proc)().	*
 ****************************************************************/

EXPR* build_proc_apply(EXPR *Proc, LIST *params)
{
  EXPR *result;
  LIST *rest_params;
  int line = Proc->LINE_NUM;

  skip_sames(Proc)->OFFSET = 0;

  if(params == NULL) {
    result = apply_expr(Proc, same_e(hermit_expr, line), line);
  }
  else {
    result = Proc;
    rest_params = params;
    while(rest_params != NULL) {
      result = apply_expr(result, rest_params->head.expr, line);
      rest_params = rest_params->tail;
    }
  }

  result->SCOPE = 1;  /* Set to closed apply */
  return result;
}


/****************************************************************
 *			REMOVE_APPLS_P				*
 ****************************************************************
 * Imagine a let or define expression Let f(?p) = b.		*
 * remove_appls_p(f(?p), b, basekind) returns an equivalent	*
 * let or define expr of the form Let f = c.  It handles	*
 * curried functions with any number of arguments. 		*
 *								*
 * Basekind is either LET_E or DEFINE_E, depending on the	*
 * expression kind (LET_E for a let, DEFINE_E for a define.)	*
 *--------------------------------------------------------------*
 * The work is done by remove_appls_help_p.  In that function,  *
 *								*
 * xfer is true if roles should transfer to id on lhs, and is	*
 * false if roles should not transfer, so the id should be	*
 * insulated from role transfer.				*
 *								*
 * did_conversion is true if a conversion of the definition has *
 * been done.  This is used to decide whether it should be 	*
 * allowed for the left-hand side to be a pattern variable.	*
 ****************************************************************/

PRIVATE EXPR* 
remove_appls_help_p(EXPR *heading, EXPR *body, int basekind, 
		    Boolean xfer, Boolean did_conversion);

/*---------------------------------------------------------*/

EXPR* remove_appls_p(EXPR *heading, EXPR *body, int basekind)
{
  return remove_appls_help_p(heading, body, basekind, 1, 0);
}

/*---------------------------------------------------------*/

PRIVATE EXPR* 
remove_appls_help_p(EXPR *heading, EXPR *body, int basekind, 
		    Boolean xfer, Boolean did_conversion)
{
  EXPR *fun, *result;
  EXPR_TAG_TYPE kind;

  bump_expr(heading);
  bump_expr(body);
  kind = EKIND(heading);

  /*------------------------------------------------------*
   * Transfer roles of function applications to the body. *
   *------------------------------------------------------*/

  if(kind == SAME_E) {
    Boolean new_xfer;
    int sskind = EKIND(skip_sames(heading));
    if(sskind == APPLY_E) {
      bump_expr(result = same_e(body, body->LINE_NUM));
      bump_type(result->ty = heading->ty);
      bump_role(result->role = heading->role);
      new_xfer = xfer && heading->SAME_MODE != 1 && heading->role == NULL;
      SET_EXPR(result, 
	       remove_appls_help_p(heading->E1, result, basekind, new_xfer,
				   did_conversion));
      goto out;
    }
    else kind = sskind;
  }

  /*--------------------------------------------------------------*
   * Handle non-applications by building the let or define expr. *
   *--------------------------------------------------------------*/

  if(kind != APPLY_E) {

    /*--------------------------------------------------------------------*
     * If the body is a function, set it to an ordinary function.  This   *
     * is necessary because it is set to a function that shares its	  *
     * environment when handling Let f(p) = ...				  *
     *--------------------------------------------------------------------*/

    if(EKIND(body) == FUNCTION_E) body->OFFSET = 0;

    /*--------------------------------------------------------------------*
     * Replace a pattern variable by a simple variable.  This allows	  *
     * Let ?x = ...  But complain if we did a function conversion and     *
     * the function is a pattern variable.				  *
     *--------------------------------------------------------------------*/

    if(kind == PAT_VAR_E) {
      if(did_conversion) {
	syntax_error(FUN_IS_PATVAR_ERR, heading->LINE_NUM);
      }
      SET_EXPR(heading, skip_sames(heading)->E1);
    }

    /*------------------------------------------------------*
     * Erase roles if we are not supposed to transfer them. *
     *------------------------------------------------------*/

    if(!xfer) {
      SET_EXPR(heading, same_e(heading, heading->LINE_NUM));
      heading->SAME_MODE = 1;
    }

    bump_expr(result = new_expr2(basekind, heading, body, heading->LINE_NUM));
  }

  /*-------------------------------------------------------------*
   * Handle applications by making the body into a function.	 *
   *-------------------------------------------------------------*/

  else {
    bump_expr(fun = 
	      new_expr2(FUNCTION_E, heading->E2, body, heading->LINE_NUM));
    fun->OFFSET = 1;
    bump_expr(result = 
	      remove_appls_help_p(heading->E1, fun, basekind, xfer, 1));
    drop_expr(fun);
  }

 out:
  drop_expr(heading);
  drop_expr(body);
  if(result != NULL) result->ref_cnt--;
  return result;
}


/****************************************************************
 *			FORCE_EVAL_P				*
 ****************************************************************
 * Return an expression that evaluates e and puts e into a 	*
 * variable, and set v to the variable.  If e is already a 	*
 * variable or constant, then return hermit_expr, with v = e.  	*
 * v is not ref-counted.					*
 ****************************************************************/

EXPR* force_eval_p(EXPR *e, EXPR **v)
{
  EXPR_TAG_TYPE kind;
  EXPR* sse;

  sse  = skip_sames(e);
  kind = EKIND(sse);
  if(kind == IDENTIFIER_E || kind == LOCAL_ID_E || kind == GLOBAL_ID_E
     || is_const_expr(sse)) {
    *v = e;
    return hermit_expr;
  }
  else {
    *v = fresh_id_expr("temp", e->LINE_NUM);
    return new_expr2(LET_E, *v, e, e->LINE_NUM);
  }
}


/****************************************************************
 *			IS_IRREGULAR_FUN			*
 ****************************************************************
 * Return true if e is an irregular function.			*
 ****************************************************************/

Boolean is_irregular_fun(EXPR *e)
{
  EXPR_TAG_TYPE kind;
  int prim;

  e     = skip_sames(e);
  kind  = EKIND(e);
  prim  = e->PRIMITIVE;

  return (kind == SPECIAL_E && (prim == PRIM_UNWRAP || prim == PRIM_DUNWRAP))
	 || (kind == GLOBAL_ID_E && e->irregular);
}


/****************************************************************
 *			IS_STAR					*
 ****************************************************************
 * Return true if e is expression *.                            *
 ****************************************************************/

Boolean is_star(EXPR *e)
{
  if(!is_id_p(e)) return FALSE;
  return e->STR == std_id[STAR_SYM];
}


/****************************************************************
 *			IS_BANG					*
 ****************************************************************
 * Return true if e is expression active or active'.            *
 ****************************************************************/

Boolean is_bang(EXPR *e)
{
  if(!is_id_p(e)) return FALSE;
  return e->STR == std_id[ACTIVE_ID] || e->STR == std_id[ACTIVE_PRIME_ID];
}


/****************************************************************
 *			IS_ANON_PAT_VAR				*
 ****************************************************************
 * Return true if e is an anonymous pattern variable.           *
 ****************************************************************/

Boolean is_anon_pat_var(EXPR *e)
{
  e = skip_sames(e);
  return EKIND(e) == PAT_VAR_E && e->E1 == NULL;
}


/****************************************************************
 *			IS_ORDINARY_PAT_VAR			*
 ****************************************************************
 * Return true if expr e is a pattern variable that is not 	*
 * anonymous.							*
 ****************************************************************/

Boolean is_ordinary_pat_var(EXPR *e)
{
  e = skip_sames(e);
  if(e == NULL) return FALSE;
  return EKIND(e) == PAT_VAR_E && e->STR != NULL && e->STR[0] != '#';
}
  

/****************************************************************
 *			IS_ID_P					*
 ****************************************************************
 * Return true if expr e is an identifier (local or global).	*
 ****************************************************************/

Boolean is_id_p(EXPR *e)
{
  EXPR_TAG_TYPE kind;

  e = skip_sames(e);
  if(e == NULL) return FALSE;
  kind = EKIND(e);
  return kind == IDENTIFIER_E || kind == LOCAL_ID_E || kind == GLOBAL_ID_E
	 || kind == OVERLOAD_E;
}
  

/****************************************************************
 *			IS_ID_OR_PATVAR_P			*
 ****************************************************************
 * Return true if expr e is an identifier (local or global)     *
 * or pattern variable.						*
 ****************************************************************/

Boolean is_id_or_patvar_p(EXPR *e)
{
  EXPR_TAG_TYPE kind;

  e = skip_sames(e);
  if(e == NULL) return FALSE;
  kind = EKIND(e);
  return kind == IDENTIFIER_E || kind == LOCAL_ID_E || kind == GLOBAL_ID_E
	 || kind == OVERLOAD_E || kind == PAT_VAR_E;
}
  

/****************************************************************
 *			IS_CONST_EXPR				*
 ****************************************************************
 * Return true just when expression e is a constant.  An 	*
 * expression of the form natconst(c) or ratconst(c), where	*
 * c is a plain constant, is considered a constant.		*
 ****************************************************************/

Boolean is_const_expr(EXPR *e)
{
  TYPE_TAG_TYPE kind;

  e = skip_sames(e);
  kind = EKIND(e);

  /*---------------------------*
   * Look for a bare constant. *
   *---------------------------*/

  if(kind == CONST_E) return TRUE;

  /*--------------------------------------*
   * Look for natconst(c) or ratconst(c). *
   *--------------------------------------*/

  if(kind == APPLY_E) {
    EXPR* a = e->E1;
    EXPR* b = e->E2;
    if(EKIND(b) != CONST_E || !is_id_p(a)) return FALSE;
    if(a->STR != std_id[NATCONST_ID] && a->STR != std_id[RATCONST_ID]) {
      return FALSE;
    }
    return TRUE;
  }

  return FALSE;
}


/****************************************************************
 *			IS_STACK_EXPR				*
 ****************************************************************
 * Return true if e is a SPECIAL_E with primitive PRIM_STACK.	*
 ****************************************************************/

Boolean is_stack_expr(EXPR *e)
{
  e = skip_sames(e);
  return e != NULL && EKIND(e) == SPECIAL_E && e->PRIMITIVE == PRIM_STACK;
}


/****************************************************************
 *			IS_FALSE_EXPR				*
 ****************************************************************
 * Return true just when e is expression 'false'.               *
 ****************************************************************/

Boolean is_false_expr(EXPR *e)
{
  register EXPR* sse = skip_sames(e);

  if(EKIND(sse) == CONST_E && 
     e->SCOPE == BOOLEAN_CONST && 
     e->STR == NULL)  return TRUE;

  else if(is_id_p(e) && e->STR == std_id[FALSE_ID] && is_boolean_type(e->ty)) {
    return TRUE;
  }
  else return FALSE;
}


/****************************************************************
 *			IS_DEFINITION				*
 ****************************************************************
 * Return true just when kind is one of DEFINE_E, LET_E, 	*
 * PAT_DCL_E or EXPAND_E expression.				*
 ****************************************************************/

Boolean is_definition_kind(EXPR_TAG_TYPE kind)
{
  return kind == DEFINE_E || kind == LET_E || 
         kind == PAT_DCL_E || kind == EXPAND_E;
}


/****************************************************************
 *			IS_HERMIT_EXPR				*
 ****************************************************************
 * Return true if e is the hermit.                              *
 ****************************************************************/

Boolean is_hermit_expr(EXPR *e)
{
  EXPR_TAG_TYPE kind;

  if(e == NULL_E) return FALSE;
  e = skip_sames(e);
  kind = EKIND(e);
  if(kind == CONST_E && e->SCOPE == HERMIT_CONST) return TRUE;
  return FALSE;
}


/****************************************************************
 *			IS_TARGET_EXPR				*
 ****************************************************************
 * Return true if e is expression target.                       *
 ****************************************************************/

Boolean is_target_expr(EXPR *e)
{
  e = skip_sames(e);
  return (EKIND(e) == SPECIAL_E && e->PRIMITIVE == PRIM_TARGET);
}


/****************************************************************
 *			BINDS_TYPE_VARIABLES			*
 ****************************************************************
 * The done field of EXPR nodes is set to 1 to indicate that    *
 * somewhere in the expression, a type variable might be 	*
 * dynamically bound.  This is only for non-global-id nodes.    *
 *								*
 * This function returns TRUE if e might dynamically bind type  *
 * variables.							*
 ****************************************************************/

Boolean binds_type_variables(EXPR *e)
{
  return EKIND(e) != GLOBAL_ID_E && e->done;
}


/****************************************************************
 *			GET_APPLIED_FUN				*
 ****************************************************************
 * Return the function that is being applied in expression e.   *
 * It is being applied, but is not itself an application.  	*
 * For example, in expression (f a b), f is the			*
 * function being applied.  To find the function being applied, *
 * we go to the left across APPLY_E nodes as far as possible.   *
 *								*
 * If report_err is true, then complain if the function 	*
 * is not an identifier, and return NULL.			*
 ****************************************************************/

EXPR* get_applied_fun(EXPR *e, Boolean report_err)
{
  EXPR *p;

  p = skip_sames(e);
  while(p != NULL && EKIND(p) == APPLY_E) p = skip_sames(p->E1);

  if(p != NULL && report_err) {
    EXPR_TAG_TYPE k = EKIND(p);
    if(k != IDENTIFIER_E && k != GLOBAL_ID_E && k != LOCAL_ID_E 
       && k != PAT_FUN_E && k != SPECIAL_E) {
      expr_error(BAD_PAT_ERR, e);
      return NULL;
    }
  }
  return p;
}


/****************************************************************
 *			GET_DEFINED_ID				*
 ****************************************************************
 * Return the identifier being defined by a DEFINE_E, LET_E,    *
 * PAT_DCL_E or EXPAND_E expression.                            *
 ****************************************************************/

EXPR* get_defined_id(EXPR *e)
{
  EXPR* defined_id = e->E1;
  if(EKIND(e) == EXPAND_E) defined_id = get_applied_fun(defined_id, FALSE);
  return skip_sames(defined_id);
}


/****************************************************************
 *			MAKE_LIST_MEM_EXPR			*
 ****************************************************************
 * Expression e occurs inside a list expression, such as	*
 * [..., e, ...].  Return what e should look like.		*
 * The cases are determined by the current top of list_expr_st.	*
 *								*
 *   top(list_expr_st)	return expr				*
 *   -----------------  ------------------------------ 		*
 *        0		e (unchanged)				*
 *	  1		f(e), where f is the expression on	*
 *			the top of list_map_st.			*
 *	  else 		(:e:)					*
 ****************************************************************/

EXPR* make_list_mem_expr(EXPR *e)
{
  int top = top_i(list_expr_st);
  if(top == 0) return e;
  else if(top == 1) {
     return apply_expr(top_expr(list_map_st), e, e->LINE_NUM);
  }
  else return new_expr2(AWAIT_E, NULL, e, e->LINE_NUM);
}


/********************************************************
 *			MAKE_BOX_EXPR			*
 ********************************************************
 * make_box_expr(content,mode) returns 			*
 *   <:content:>    if mode = 0				*
 *   <:#content#:>  if mode = SHARED_ATT		*
 *   <:%content%:>  if mode = NONSHARED_ATT. 		*
 ********************************************************/

EXPR* make_box_expr(EXPR *content, int mode)
{
  int line     = content->LINE_NUM;
  int id_index = (mode == SHARED_ATT)    ? SHAREDBOX_ID : 
                 (mode == NONSHARED_ATT) ? NONSHAREDBOX_ID
		                         : MAKEBOX_ID;
  TYPE* constructor_ty = function_t(NULL, fam_mem_t(box_fam, NULL));
  EXPR* constructor    = typed_global_sym_expr(id_index, constructor_ty, line);
  return apply_expr(constructor, content, line);
}


/****************************************************************
 *			LIST_TO_EXPR				*
 ****************************************************************
 * Convert list l to an expr, according to mode.                *
 * List l must be nonempty.  Suppose that l = [e1,...,en]	*
 * (as a list of expressions).					*
 *                                                              *
 *     mode         result                                      *
 *     ----         ______                                      *
 *                                                              *
 *      0           (e1,...,en)                                 *
 *                                                              *
 *      2           (||e1,....,en||)                            *
 *                                                              *
 *      3 	    [e1,...,en]  (that is, e1::...::en::nil)    *
 *                                                              *
 *      4           (:e1,...,en:), built as ((:e1:),...,(:en:)),*
 *                  with no outer (:...:) enclosing it.		*
 *                                                              *
 *      5           (:&e1,...,en:), with similar remark to 4.	*
 *                                                              *
 *      6           open (e1,e2,...,en)				*
 *                                                              *
 *      7           (e1,...,en), but mark each pair node with   *
 *                  extra = 1.					*
 ****************************************************************/

/*--------------------------------------------------------------*/

EXPR* list_to_expr(EXPR_LIST *l, int mode)
{
  EXPR *head, *result;

  if(l == NIL) return NULL_E;

  /*------------------------------------------------------*
   * Modify head for those forms that need head modified. *
   *------------------------------------------------------*/

  head = l->head.expr;
  if(mode == 4) {
    head = make_await_expr(head, NIL, 0, head->LINE_NUM);
  }
  else if(mode == 5) {
    head = make_await_expr(head, NIL, 1, head->LINE_NUM);
  }

  /*-------------------------*
   * Handle end of the list. *
   *-------------------------*/

  if(l->tail == NIL) return head;

  /*-----------------*
   * Recursive case. *
   *-----------------*/

  result = new_expr2(PAIR_E,head,list_to_expr(l->tail,mode),head->LINE_NUM);

  /*-------------------------------------------------------*
   * Modify the pair node for those forms that require it. *
   *-------------------------------------------------------*/

  if     (mode == 6) result->SCOPE = 1;   /* Open pair. */
  else if(mode == 7) result->extra = 1;   /* Curried pattern match. */
  else if(mode == 2) result->COROUTINE_FORM = 1;
  return result;
}


/****************************************************************
 *			SKIP_SAMES				*
 ****************************************************************
 * Remove leading SAME_E nodes from e, and return the result.   *
 ****************************************************************/

EXPR* skip_sames(EXPR *e)
{
  register EXPR* ee = e;

  if(ee == NULL_E) return NULL_E;
  while(ee != NULL_E && EKIND(ee) == SAME_E) ee = ee->E1;
  return ee;
}


/****************************************************************
 *			SKIP_SAMES_TO_DCL_MODE			*
 ****************************************************************
 * Remove leading SAME_E nodes from e, up to a node that either *
 * is not a SAME_E node or is a SAME_E node with SAME_MODE == 5 *
 * (a declaration mode holder).  Return the result.   		*
 ****************************************************************/

EXPR* skip_sames_to_dcl_mode(EXPR *e)
{
  register EXPR* ee = e;

  if(ee == NULL_E) return NULL_E;
  while(ee != NULL_E && EKIND(ee) == SAME_E && ee->SAME_MODE != 5) ee = ee->E1;
  return ee;
}


/****************************************************************
 *			ADD_EXPR				*
 ****************************************************************
 * Expr g is thought of as a chain, linked through the E1 field.*
 * Destructively put e at the end of that chain, and return the *
 * chain (g itself).                                            *
 ****************************************************************/

EXPR* add_expr(EXPR *g, EXPR *e)
{
  EXPR *p;

  if(g == NULL_E) return e;

  for(p = g; p->E1 != NULL_E; p = p->E1) {}
  p->E1 = e;
  return g;
}


/****************************************************************
 *			CLEAR_OFFSETS				*
 ****************************************************************
 * clear the offset, bound and temp fields in all LOCAL_ID_E 	*
 * and PAT_VAR_E nodes of e, and the done field in each 	*
 * node of e.							*
 ****************************************************************/

PRIVATE Boolean clear_offsets_help(EXPR **es)
{
  EXPR* e = *es;

 again:                  /* for tail recursion */
  e->done = 0;
  switch(EKIND(e)) {
    default:
        break;

    case LOCAL_ID_E:
        e->OFFSET = 0;
	e->bound  = 0;
	e->ETEMP  = 0;
	break;

    case PAT_VAR_E:
 	e->OFFSET = 0;
	e->bound  = 0;
	e         = e->E1;
	if(e == NULL) return 0;
	goto again;              /* tail recur on the id */
  }
  return 0;
}

/*--------------------------------------------------------*/

void clear_offsets(EXPR *e) 
{
 scan_expr(&e, clear_offsets_help, FALSE);
}


/****************************************************************
 *			CHECK_FOR_BARE_IDS			*
 ****************************************************************
 * Check that none of the ids in list l (which is a list of 	*
 * pat vars) occurs bare in e.					*
 ****************************************************************/

PRIVATE LIST *check_for_bare_ids_list;

PRIVATE Boolean check_for_bare_ids_help(EXPR **es)
{
  EXPR_LIST *p;
  EXPR *e;
  char *name;

  e = *es;
  if(EKIND(e) == LOCAL_ID_E) {
    for(p = check_for_bare_ids_list; p != NIL; p = p->tail) {
      name = e->STR;
      if(p->head.expr->STR == name) {
	semantic_error1(PATVAR_AS_NONPATVAR_ERR, display_name(name), 
			e->LINE_NUM);
      }
    }
  }
  return 0;
}

/*-----------------------------------------------------------*/

void check_for_bare_ids(EXPR *e, EXPR_LIST *l)
{
  bump_list(check_for_bare_ids_list = l);
  scan_expr(&e, check_for_bare_ids_help, 1);
  drop_list(l);
  check_for_bare_ids_list = NULL;
}


/****************************************************************
 *			EXPR_SIZE				*
 ****************************************************************
 * Return the number of nodes in e.                             *
 ****************************************************************/

PRIVATE int expr_size_num;

PRIVATE Boolean expr_size_help(EXPR **e_unused)
{
  expr_size_num++;
  return 0;
}

/*-----------------------------------------------------------*/

int expr_size(EXPR *e)
{
  expr_size_num = 0;
  scan_expr(&e, expr_size_help, 0);
  return expr_size_num;
}


/****************************************************************
 *			BUMP_EXPR_PARTS				*
 ****************************************************************
 * Bump the ref counts of the values in node e.                 *
 ****************************************************************/

void bump_expr_parts(EXPR *e)
{
  switch(EKIND(e)) {

    case OPEN_E:
      bump_expr(e->E1);
      bump_list(e->EL1);
      bump_list(e->EL2);
      break;

    case PAT_FUN_E:
      bump_expr(e->E3);
      break;

    case IF_E:
    case TRY_E:
    case FOR_E:
    case PAT_RULE_E:
      bump_expr(e->E3);
      /* No break - fall through */

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
    case TRAP_E:
    case WHERE_E:
    case PAT_DCL_E:
    case EXPAND_E:
      bump_expr(e->E2);
      /* No break - fall through */

    case SINGLE_E:
    case LAZY_LIST_E:
    case RECUR_E:
    case SAME_E:
    case EXECUTE_E:
    case PAT_VAR_E:
      bump_expr(e->E1);
      /* No break - fall through */

    default:
      bump_type(e->ty);
      break;

    case MANUAL_E:
      bump_expr(e->E1);
      bump_list(e->EL2);
      break;
  }
  bump_type(e->ty);
}


/****************************************************************
 *			DROP_HASH_EXPR				*
 ****************************************************************/

void drop_hash_expr(HASH2_CELLPTR h)
{
  drop_expr(h->val.expr);
}


/****************************************************************
 *			MARK_ALL_PAT_FUNS			*
 ****************************************************************
 * Mark all pattern functions and pattern constants in e.       *
 ****************************************************************/

PRIVATE Boolean mark_it(EXPR **e)
{
  int kind;

  kind = EKIND(*e);
  if(kind == MATCH_E || kind == FUNCTION_E || kind == FOR_E) {
    mark_pat_funs((*e)->E1, TRUE, 0);
  }
  return 0;
}

/*-------------------------------------------------------*/

void mark_all_pat_funs(EXPR *e)
{
  scan_expr(&e,mark_it,TRUE);
}


/************************************************************************
 *                         ID_OCCURRENCES                               *
 ************************************************************************
 * id_occurrences(id, e) returns the number of times identifier id 	*
 * occurs in expression e.  The following are special cases.		*
 *									*
 *    If id is a pattern variable, then occurrences of its 		*
 *    associated identifier are also counted.				*
 *									*
 *    If id is NULL, then occurrences of expression 'target' are	*
 *    counted.								*
 ************************************************************************/

PRIVATE int id_occurrences_num;
PRIVATE EXPR* id_occurrences_id;

PRIVATE Boolean id_occurrences_help(EXPR **e)
{
  register EXPR*         ee     = *e;
  register EXPR_TAG_TYPE e_kind = EKIND(ee);

  if(id_occurrences_id == NULL) {
    if(is_target_expr(ee)) {
      id_occurrences_num++;
    }
  }

  else if(id_occurrences_id == ee) id_occurrences_num++;

  return 0;
}

/*------------------------------------------------------------------*/

int id_occurrences(EXPR *id, EXPR *e)
{
  id_occurrences_num = 0;
  id_occurrences_id = (id != NULL && EKIND(id) == PAT_VAR_E) ? id->E1 : id;
  scan_expr(&e, id_occurrences_help, 1);
  return id_occurrences_num;
}


/****************************************************************
 *			IMAGE_TO_ID				*
 ****************************************************************
 * This function is used to put the target of a match into an   *
 * identifier, if necessary.  There are two possibilities.	*
 *								*
 * Case 1: Return a new identifier named ;target, and set 	*
 * preamble to an expression that binds that identifier to the	*
 * target (parameter image).					*
 *								*
 * Case 2. Return image, and set preamble to NULL.		*
 *								*
 * Selecting the case						*
 * -------------------						*
 *								*
 * Case 2 is chosen whenever image is a constant or identifier.	*
 *								*
 * Otherwise, case 1 is chosen if any of the following are true.*
 *								*
 *   (a) use_stack is true,					*
 *   (b) open is true,						*
 *   (c) 'target' occurs more than once in translation,		*
 *   (d) translation is NULL.					*
 *								*
 * Otherwise case 2 is taken.					*
 *								*
 * PREAMBLE is ref-counted.  					*
 *								*
 * If use_stack is true, then the target is actually in the	*
 * stack, and is got from there to bind it the identifier.	*
 ****************************************************************/

EXPR* image_to_id(EXPR *image, Boolean use_stack, EXPR *translation,
		  EXPR **preamble, Boolean open, int line)

{ int image_kind;
  EXPR *new_image;

  *preamble  = NULL_E;
  image_kind = EKIND(skip_sames(image));
  if(image_kind != CONST_E && image_kind != LOCAL_ID_E &&
     image_kind != GLOBAL_ID_E) {
    if(use_stack || open || translation == NULL ||
       id_occurrences(NULL,translation) > 1) {
      new_image        = new_expr1t(LOCAL_ID_E, NULL_E, image->ty, line);
      new_image->STR   = HIDE_CHAR_STR "target";
      new_image->SCOPE = -1;
      bump_expr(*preamble = new_expr2(LET_E, new_image, image, line));
      bump_type((*preamble)->ty = hermit_type);
      (*preamble)->SCOPE = open;
      return new_image;
    }
  }
  return image;
}


/****************************************************************
 *			USES_EXCEPTION				*
 ****************************************************************
 * Return true just when e contains expression 'exception'.     *
 ****************************************************************/

PRIVATE Boolean found_exception;

PRIVATE Boolean uses_exception_help(EXPR **e)
{
  if(EKIND(*e) == SPECIAL_E && (*e)->PRIMITIVE == PRIM_EXCEPTION) {
    found_exception = TRUE;
  }
  return 0;
}

/*-----------------------------------------------------------*/

Boolean uses_exception(EXPR *e)
{
  found_exception = FALSE;
  scan_expr(&e, uses_exception_help, 0);
  return found_exception;
}


/****************************************************************
 *			SIMPLIFY_EXPR				*
 ****************************************************************
 * Replace e by a possibly simplified version of e.             *
 ****************************************************************/

void simplify_expr(EXPR **e)
{
  register EXPR *stare = *e;

  if(stare == NULL) return;

  /*----------------------------------------------------------*
   * Skip over SAME_E nodes, but don't use skip_sames because *
   * want to keep address of the expr cell.		      *
   *----------------------------------------------------------*/

  while(EKIND(stare) == SAME_E) {
    e = &(stare->E1);
    stare = *e;
  }

  switch(EKIND(stare)) {
    default:

      /*---------------------------------*
       * The default is not to simplify. *
       *---------------------------------*/

      return;

    case GLOBAL_ID_E:

      /*---------------------------------------------*
       * Handle ids that are bound to true or false. *
       *---------------------------------------------*/

      {int prim, instr;
       PART *part;
       Boolean irregular;
       INT_LIST *selection_list;
       TYPE *ty;

       ty = find_u(stare->ty);
       if(is_boolean_type(ty)) {
         prim = get_prim_g(stare, &instr, &part, &irregular, &selection_list);
         if(prim == PRIM_CONST) {
           if(instr == NIL_I || instr == TRUE_I) {
	     set_expr(e, const_expr((instr == NIL_I) ? NULL : "", 
				    BOOLEAN_CONST, ty, stare->LINE_NUM));
	   }
	 }
       }
       return;
      }
 
    case LAZY_BOOL_E:

	/*-----------------------------------------*
	 * Calculate and, or, implies expr values. *
	 *-----------------------------------------*/

       {EXPR *e1, *e2;
	Boolean e1val, e2val, bool_result;
	int opn;

	simplify_expr(&(stare->E1));
	simplify_expr(&(stare->E2));
	e1 = skip_sames(stare->E1);
	e2 = skip_sames(stare->E2);
	if(EKIND(e1) == CONST_E && EKIND(e2) == CONST_E) {
	  e1val = (e1->STR == NULL) ? FALSE : TRUE;
	  e2val = (e2->STR == NULL) ? FALSE : TRUE;
	  opn = stare->LAZY_BOOL_FORM;
	  bool_result = (opn == AND_BOOL)       ? e1val & e2val :
	                (opn == OR_BOOL)        ? e1val | e2val
		      /*(opn == IMPLIES_BOOL)*/ : e1val <= e2val;
	  set_expr(e, const_expr(bool_result ? "" : NULL, 
				 BOOLEAN_CONST, boolean_type, 
				 stare->LINE_NUM));
	}
	return;
      }
	    
    case APPLY_E:

       /*--------------------------------*
        * Calculate primitive functions. *
        *--------------------------------*/

       {EXPR *e1, *e2;
	simplify_expr(&(stare->E1));
	simplify_expr(&(stare->E2));
	e1 = skip_sames(stare->E1);
	e2 = skip_sames(stare->E2);
	if(EKIND(e1) == GLOBAL_ID_E &&
	   strcmp(e1->STR, "not") == 0
	   && EKIND(e2) == CONST_E
	   && TKIND(e2->ty) == TYPE_ID_T
	   && e2->ty->ctc == boolean_type->ctc) {
	  set_expr(e, const_expr((e2->STR == NULL) ? "" : NULL,
				 BOOLEAN_CONST, boolean_type, 
				 stare->LINE_NUM));
	}
	return;
       }

    case IF_E:

      /*----------------------------------------------*
       * Simplify conditionals with known conditions. *
       *----------------------------------------------*/

      {EXPR *cond;
       simplify_expr(&(stare->E1));
       simplify_expr(&(stare->E2));
       simplify_expr(&(stare->E3));
       cond = skip_sames(stare->E1);
       if(EKIND(cond) == CONST_E) {
	 if(cond->STR == NULL) set_expr(e, stare->E3);
	 else set_expr(e, stare->E2);
       }
       return;
     }

    case SINGLE_E:
    case RECUR_E:
    case SAME_E:
    case EXECUTE_E:
    case OPEN_E:
    case LAZY_LIST_E:
       simplify_expr(&(stare->E1));
       return;

    case LET_E:
    case DEFINE_E:
    case SEQUENCE_E:
    case TEST_E:
    case STREAM_E:
    case AWAIT_E:
    case PAIR_E:
    case FUNCTION_E:
    case LOOP_E:
    case MATCH_E:
    case WHERE_E:
    case TRAP_E:
       simplify_expr(&(stare->E1));
       simplify_expr(&(stare->E2));
       return;

    case TRY_E:
    case FOR_E:
       simplify_expr(&(stare->E1));
       simplify_expr(&(stare->E2));
       simplify_expr(&(stare->E3));
       return;
  }
}     


/****************************************************************
 *			MAKE_CAST				*
 ****************************************************************
 * Return a special function that casts from type s to type t.  *
 ****************************************************************/

EXPR* make_cast(TYPE *s, TYPE *t, int line)
{
  EXPR* e      = new_expr1t(SPECIAL_E, NULL, function_t(s, t), line);
  e->PRIMITIVE = PRIM_CAST;
  e->STR       = stat_id_tb(HIDE_CHAR_STR "cast");
  return e;
}


/*****************************************************************
 *			IGNORE_EXPR				 *
 *****************************************************************
 * Return an expression that evaluates e and ignores the result. *
 * The value of this expression is ().				 *
 *****************************************************************/

EXPR* ignore_expr(EXPR *e, int line)
{
  TYPE* ignore_type = function_t(var_t(NULL), hermit_type);
  EXPR* result      = apply_expr(typed_global_sym_expr(IGNORE_ID,
					         ignore_type, line), 
		            e, line);
  UNIFY(ignore_type->TY1, e->ty, FALSE);
  bump_type(result->ty = hermit_type);
  return result;
}


/************************************************************************
 *			VAR_ICNSTR					*
 ************************************************************************
 * See the description of make_var_expr_p, below.  It defines functions *
 *   iconstr(flav,dims)							*
 *   aconstr(flav,dims)							*
 *									*
 * var_icnstr returns the following, depending on the parameters	*
 * kind and funselect.							*
 *									*
 *   kind     funselect       result					*
 *   ----     ----------      ------					*
 *    0       0               icnstr(shared,dims)			*
 *    1       0               icnstr(shared,dims)			*
 *    2       0               icnstr(nonshared,dims)			*
 *    0       1               acnstr(shared,dims)			*
 *    1       1               acnstr(shared,dims)			*
 *    2       1               acnstr(nonshared,dims)			*
 *									*
 ************************************************************************/

PRIVATE EXPR*
var_icnstr(int kind, Boolean funselect, LIST *dims, int line)
{
  EXPR *first_dim;

  if(dims == NULL) return NULL;
  first_dim = dims->head.expr;

  /*----------------------------------------------------*
   * The functions used here are defined on boxes.ast,  *
   * so be sure that it has been imported.		*
   *----------------------------------------------------*/

  check_imported("collect/boxes.ast", TRUE);

  /*--------------------------------*
   * Case dims = [n] or dims = [*]. *
   *--------------------------------*/

  if(dims->tail == NULL) {
    int cnstr_index;
    EXPR *flav, *cnstr_arg, *cnstr;

    flav = (kind == 2) ? nonshared_expr : shared_expr;
    if(is_star(first_dim)) {
      cnstr_index = funselect ? INFINITEARRAYNOINIT_ID : INFINITEARRAYINIT_ID;
      cnstr_arg   = same_e(hermit_expr, line);
    }
    else {
      cnstr_index = funselect ? ARRAYNOINIT_ID : ARRAYINIT_ID;
      cnstr_arg   = first_dim;
    }
    cnstr = id_expr(std_id[cnstr_index], line);
    return apply_expr(apply_expr(cnstr, flav, line), cnstr_arg, line);
  }

  /*----------------------------------*
   * Case dims = n::t or dims = *::t. *
   *----------------------------------*/

  else {
    EXPR *cnstr_arg, *cnstr, *rcnstr;

    if(is_star(first_dim)) {
      if(funselect) {
	cnstr     = id_expr(std_id[INFVARDUPNOINIT_ID], line);
        rcnstr    = var_icnstr(kind, 1, dims->tail, line);
      }
      else {
	cnstr  = id_expr(std_id[INFVARDUPINIT_ID], line);
        rcnstr = new_expr2(PAIR_E, 
			   var_icnstr(kind, 0, dims->tail, line),
			   var_icnstr(kind, 1, dims->tail, line),
			   line);
      }
      cnstr_arg = same_e(hermit_expr, line);
      return apply_expr(apply_expr(cnstr, rcnstr, line), cnstr_arg, line);
    }

    else {
      if(funselect) {
	cnstr     = id_expr(std_id[VARDUPNOINIT_ID], line);
        rcnstr    = var_icnstr(kind, 1, dims->tail, line);
      }
      else {
	cnstr  = id_expr(std_id[VARDUPINIT_ID], line);
        rcnstr = new_expr2(PAIR_E, 
			   var_icnstr(kind, 0, dims->tail, line),
			   var_icnstr(kind, 1, dims->tail, line),
			   line);
      }
      cnstr_arg = first_dim;
      return apply_expr(apply_expr(cnstr, rcnstr, line), cnstr_arg, line);
    }
  }
}


/************************************************************************
 *			VAR_CNSTR					*
 ************************************************************************
 * See the description of make_var_expr_p, below.  It defines functions *
 *   uconstr(flav,inf,dims)						*
 *   fcnstr(flav,inf,z,dims)						*
 *   zcnstr(flav,inf,dims)						*
 *									*
 * var_cnstr returns the following, depending on the parameters		*
 * init_val and kind.							*
 *									*
 *   kind     init_val           result					*
 *   ----     ----------         ------					*
 *    0       NULL               ucnstr(shared,inf,dims)		*
 *    1       NULL               ucnstr(shared,inf,dims)		*
 *    2       NULL               ucnstr(nonshared,inf,dims)		*
 *    4       NULL               zcnstr(shared,inf,dims)		*
 *    5       NULL               zcnstr(shared,inf,dims)		*
 *    6       NULL               zcnstr(nonshared,inf,dims)		*
 *    0       non NULL           fcnstr(shared,inf,init_val,dims)	*
 *    1       non NULL           fcnstr(shared,inf,init_val,dims)	*
 *    2       non NULL           fcnstr(nonshared,inf,init_val,dims)	*
 *									*
 ************************************************************************/

PRIVATE EXPR* 
var_cnstr(int kind, Boolean inf, EXPR *init_val, LIST *dims, int line)
{
  /*----------------------------*
   * Case where dims = []. 	*
   *----------------------------*/

  if(dims == NIL) {
    if(init_val == NULL) {
      int cnstr_index;
      switch(kind) {
	case 0:
 	case 1:
	  cnstr_index = inf ? INFINITESHAREDARRAY_ID : SHAREDARRAY_ID;
	  break;

	case 2:
	  cnstr_index = inf ? INFINITENONSHAREDARRAY_ID : NONSHAREDARRAY_ID;
	  break;

	case 4:
	case 5:
          check_imported("collect/boxes.ast", TRUE);
	  cnstr_index = inf ? INFINITESHAREDUNKNOWNARRAY_ID
			     : SHAREDUNKNOWNARRAY_ID;

	case 6:
          check_imported("collect/boxes.ast", TRUE);
	  cnstr_index = inf ? INFINITENONSHAREDUNKNOWNARRAY_ID
		            : NONSHAREDUNKNOWNARRAY_ID;
	  break;
      }
      return id_expr(std_id[cnstr_index], line);
    }

    else /* init_val != NULL */ {
      EXPR* flav = (kind & 2) ? nonshared_expr : shared_expr;
      int   cnstr_index = inf ? FULLINFINITEARRAY_ID : FULLARRAY_ID;
      EXPR* cnstr = id_expr(std_id[cnstr_index], line);

      check_imported("collect/boxes.ast", TRUE);
      return apply_expr(apply_expr(cnstr, same_e(flav, line), line), 
		        init_val, line);
    }
  }

  /*--------------------------------------------*
   * Case where dims = n::t or *::t. 		*
   *--------------------------------------------*/

  else {
    EXPR* first_dim   = dims->head.expr;
    int   cnstr_index = inf ? INFVARDUP_ID : VARDUP_ID;
    EXPR* duper       = id_expr(std_id[cnstr_index], line);
    EXPR *cnstr_arg, *rcnstr;
    Boolean newinf;

    check_imported("collect/boxes.ast", TRUE);
    if(is_star(first_dim)) {
      cnstr_arg = same_e(hermit_expr, line);
      newinf = TRUE;
    }
    else {
      cnstr_arg = first_dim;
      newinf = FALSE;
    }
    rcnstr = var_cnstr(kind, newinf, init_val, dims->tail, line);

    return apply_expr(apply_expr(duper, rcnstr, line), cnstr_arg, line);
  }
  
}


/************************************************************************
 *			TOPCNSTR					*
 ************************************************************************
 * See the description of make_var_expr_p, below.  It defines functions *
 *   utopconstr(flav,dims)						*
 *   ftopcnstr(flav,dims,v)						*
 *   ztopcnstr(flav,dims)						*
 *									*
 * topcnstr returns the following, depending on the parameters		*
 * init_val and kind.							*
 *									*
 *   kind     init_val           result					*
 *   ----     ----------         ------					*
 *    0       NULL               utopcnstr(shared,dims)			*
 *    1       NULL               utopcnstr(shared,dims)			*
 *    2       NULL               utopcnstr(nonshared,dims)		*
 *    4       NULL               ztopcnstr(shared,dims)			*
 *    5       NULL               ztopcnstr(shared,dims)			*
 *    6       NULL               ztopcnstr(nonshared,dims)		*
 *    0       non NULL           ftopcnstr(shared,dims,init_val)	*
 *    1       non NULL           ftopcnstr(shared,dims,init_val)	*
 *    2       non NULL           ftopcnstr(nonshared,dims,init_val)	*
 *									*
 ************************************************************************/

PRIVATE Boolean star_seen;

PRIVATE EXPR* 
topcnstr(int kind, LIST *dims, EXPR *init_val, int line)
{
  EXPR *fun, *first_dim, *cnstr_arg;
  Boolean inf;

  if(dims == NIL) return NULL;
  first_dim = dims->head.expr;
  if(is_star(first_dim)) {
    star_seen = TRUE;
    inf       = TRUE;
    cnstr_arg = same_e(hermit_expr, line);
  }
  else {
    inf       = FALSE;
    cnstr_arg = first_dim;
  }
  fun = var_cnstr(kind, inf, init_val, dims->tail, line);
  return apply_expr(fun, cnstr_arg, line);
}


/************************************************************************
 *			MAKE_VAR_EXPR_P					*
 ************************************************************************
 * Make a var expression or declation					*
 *									*
 *  Var x:A,y:B,z:C holding T := init.					*
 *									*
 *   L        is the list of members, each of the form [id t] 		*
 *	      (for just an id tagged by type t) or 			*
 *	      [id, t, e1, ... en] for a member of the form		*
 *	      id(e1,...,en) holding t.  id is a string.			*
 *            For example, in the above var expression, L is		*
 *	      [[x,A], [y,B], [z,C]].  In declaration 			*
 *	      Var x(3,4) holding T. , L is [[x,NULL,3,4]].		*
 *									*
 *   v_content_type is the content type from the holding phrase 	*
 *		    (or NULL if there is none).				*
 *									*
 *   init_val is the initial value, or is NULL if there is no 		*
 *	      initial value.						*
 *									*
 *   all      is TRUE if this is an "all :=" initializer, and  		*
 *	      is FALSE  if this is a ":=" initializer.			*
 *									*
 *   line     is the line number of the expression or 			*
 *	      declaration						*
 *									*
 *   mode     is the declaration mode.  The low order three bits        *
 *            of mode->define_mode contain the kind, as follows:	*
 *               0 for an untagged var expr (shared by default)		*
 *	         1 for a shared var expr, 				*
 *	         2 for a nonshared var expr				*
 *		 4 for an unknown of an unmarked kind (shared by 	*
 *		   default)						*
 *		 5 for a shared unknown expr				*
 *		 6 for a nonshared unknown expr				*
 *            The remaining bits of mode->define_mode hold other modes. *
 *            This function can change mode, and can put it into a data *
 *            structure.						*
 *									*
 * Return an expression that is the translated var expression.		*
 *									*
 * SPECIAL CASE: When this var expr is in a class 			*
 *               (top_i(var_context_st) == 1): 				*
 *									*
 *   1. Add information about it to class_vars.				*
 *									*
 *   2. Change the names of the variables in the expression that is	*
 *      returned, adding my- to the front of each.			*
 *									*
 * Examples:								*
 * ---------								*
 *    _______________________________________________________________   *
 *    ------------------------- uninitialized -----------------------	*
 *									*
 *    Var{shared} x holding T.						*
 *	result  = Let x:Box(T) = emptySharedBox().			*
 *									*
 *    Var{shared} x(5) holding T.					*
 *      result = Let x:[Box(T)] = sharedArray 5.			*
 *									*
 *    Var{shared} x(7,5) holding T.					*
 *      result = Let x:[[Box(T)]] = 					*
 *                 (varDup sharedArray 5) 7.				*
 *               %Let							*
 *									*
 *    Var{shared} x(9,7,5).						*
 *      result = Let x = (varDup (varDup sharedArray 5) 7) 9.   	*
 *									*
 *    Var{shared} x(*).							*
 *	result = Let x = infiniteSharedArray ().			*
 *									*
 *    Var{shared} x(*,*).						*
 *	result = Let x = (infVarDup infiniteSharedArray ()) ().		*
 *									*
 *    Var{shared} x(5,*).						*
 *      result = Let x = (varDup infiniteSharedArray ()) 5.		*
 *									*
 *    Var{shared} x(*,5).
 *      result = Let x = (infVarDup sharedArray 5) ()
 *									*
 * In general, Var{flav} x(dims), where dims is a list of indices and 	*
 * asterisks, is translated to						*
 *									*
 *    Let x = utopcnstr(flav,dims).					*
 *									*
 * where utopcnstr is defined as follows.  Here, n is an 		*
 * an expression, not *.						*
 *									*
 *    utopcnstr(flav,n::t) = (ucnstr(flav,false,t))(n)			*
 *    utopcnstr(flav,*::t) = (ucnstr(flav,true,t))()			*
 *									*
 *    ucnstr(shared,false,[])    = sharedArray				*
 *    ucnstr(nonshared,false,[]) = nonsharedArray			*
 *    ucnstr(shared,true,[])     = infiniteSharedArray			*
 *    ucnstr(nonshared,true,[])  = infiniteNonsharedArray		*
 *									*
 *    ucnstr(flav,false,n::t) = varDup(ucnstr(flav,false,t)) n    	*
 *    ucnstr(flav,false,*::t) = varDup(ucnstr(flav,true,t)) ()    	*
 *    ucnstr(flav,true,n::t)  = infVarDup(ucnstr(flav,false,t)) n 	*
 *    ucnstr(flav,true,*::t)  = infVarDup(ucnstr(flav,true,t)) () 	*
 *									*
 *    _______________________________________________________________   *
 *    ---------------- initialized simple box -----------------------	*
 *									*
 *    Var{shared} x holding T := i.					*
 *	result  = Let x:Box(T) = sharedBox(i).				*
 *									*
 *    _______________________________________________________________   *
 *    -------------- initialized array, using "all :=" --------------	*
 *									*
 *    Var{shared} x(5) holding T all := z.				*
 *      result = Let x:[Box(T)] = (fullArray shared z) 5.		*
 *									*
 *    Var{shared} x(7,5) all := z.					*
 *      result = Let x = (varDup (fullArray shared z) 5) 7		*
 *									*
 *    Var{shared} x(9,7,5) all := z.					*
 *      result = Let x = (varDup 					*
 *			   (varDup (fullArray shared z) 5) 		*
 *		            7) 9					*
 *		 %Let							*
 *									*
 *    Var{shared} x(*) all := z.					*
 *      result = Let x = (fullInfiniteArray shared z) ().		*
 *									*
 *    Var{shared} x(*,*) all := z.					*
 *      result = 							*
 *        Let x = infVarDup (fullInfiniteArray shared z) () (). 	*
 *									*
 * In general, Var{flav} x(dims) all := z is translated to		*
 *									*
 *   Let x = ftopcnstr(flav,dims,z).					*
 *									*
 * where								*
 *									*
 *   ftopcnstr(flav,z,n::t) = (fcnstr(flav,false,z,t))(n)		*
 *   ftopcnstr(flav,z,*::t) = (fcnstr(flav,true,z,t))()			*
 *									*
 *   fcnstr(flav,false,z,[]) = (fullArray flav z)			*
 *   fcnstr(flav,true,z,[]) = (fullInfiniteArray flav z)		*
 *									*
 *   fcnstr(flav,false,z,n::t) = varDup(fcnstr(flav,false,z,t)) n    	*
 *   fcnstr(flav,false,z,*::t) = varDup(fcnstr(flav,true,z,t)) ()    	*
 *   fcnstr(flav,true,z,n::t)  = infVarDup(fcnstr(flav,false,z,t)) n 	*
 *   fcnstr(flav,true,z,*::t)  = infVarDup(fcnstr(flav,true,z,t)) () 	*
 *									*
 *    _______________________________________________________________   *
 *    ----------------- array initialized from a list ---------------	*
 *									*
 *    Var{shared} x(5) := z.						*
 *      result =  Let x = (arrayInit shared 5) z.			*
 *									*
 *    Var{shared} x(7,5) := z.						*
 *	result = Let x = 						*
 *                 varDupInit 						*
 *                    (arrayInit shared 5, arrayNoinit shared 5) 	*
 *                    7 z						* 
 *               %Let							*
 *									*
 *    Var{shared} x(9,7,5) := z.					*
 *	result = Let x = 					   	*
 *                 (varDupInit					   	*
 *                   (varDupInit 				   	*
 *                      (arrayInit shared 5, arrayNoinit shared 5) 	*
 *                      7)					   	*
 *		     9 z					   	*
 *               %Let						   	*
 *									*
 *    Var{shared} x(*) := z.						*
 *      result = Let x = infiniteArrayInit shared () z.			*
 *									*
 *    Var{shared} x(*,*) := z.						*
 *      result = Let x = infVarDupInit					*
 *			   (infiniteArrayInit shared (),		*
 *			    infiniteArrayNoinit shared ())		*
 *			   () z						*
 *               %Let							*
 *									*
 * In general, Var{flav} x(dims) := z. is translated to			*
 *									*
 *   Let x = (icnstr(flav,dims)) z					*
 *									*
 * where								*
 *									*
 *    icnstr(flav,[n]) = arrayInit flav n				*
 *    icnstr(flav,[*]) = infiniteArrayInit flav ()			*
 *									*
 *    acnstr(flav,[n]) = arrayNoinit flav n				*
 *    acnstr(flav,[*]) = infiniteArrayNoinit flav ()			*
 *									*
 *    icnstr(flav,n::t) = varDupInit 					*
 *			   (icnstr(flav,t), acnstr(flav,t))		*
 * 			   n						*
 *									*
 *    icnstr(flav,*::t) = infVarDupInit					*
 *			    (icnstr(flav,t), acnstr(flav,t))		*
 *			    ()						*
 *									*
 *    acnstr(flav,n::t) = varDupNoInit (acnstr(flav,t))	n		*
 *									*
 *    acnstr(flav,*::t) = infVarDupNoinit (acnstr(flav,t)) ()		*
 *									*
 *    _______________________________________________________________   *
 *    ------------------------ Unknowns -----------------------------	*
 *									*
 * Unknowns are never initialized.  Here are forms.			*
 *									*
 *    Var{logic} x.							*
 *	result = Let x = nonsharedUnknown().				*
 *									*
 *    Var{logic} x(5).							*
 *	result = Let x = nonsharedUnknownArray 5.			*
 *									*
 *    Var{logic} x(*).							*
 *	result = Let x = infiniteNonsharedUnknownArray 5.		*
 *									*
 *    Var{logic} x(7,5).						*
 *	result = Let x = varDup nonsharedUnknownArray 5 7.      	*
 *									*
 *    Var{logic} x(*,*).						*
 *	result = Let x = (infVarDup 					*
 *			    infiniteNonsharedUnknownArray ()) ()	*
 *		 %Let							*
 *									*
 * In general, Var{unknown,flav} x(dims). (where dims is a nonempty 	*
 * list of dimensions) is translated to					*
 *									*
 *   Let x = ztopcnstr(flav,dims).					*
 *									*
 * where								*
 *									*
 *    ztopcnstr(flav,n::t) = (zcnstr(flav,false,t))(n)			*
 *    ztopcnstr(flav,*::t) = (zcnstr(flav,true,t))()			*
 *									*
 *    zcnstr(shared,false,[])    = sharedUnknownArray			*
 *    zcnstr(nonshared,false,[]) = nonsharedUnknownArray		*
 *    zcnstr(shared,true,[])     = infiniteSharedUnknownArray		*
 *    zcnstr(nonshared,true,[])  = infiniteNonsharedUnknownArray	*
 *									*
 *    zcnstr(flav,false,n::t) = varDup(zcnstr(flav,false,t)) n    	*
 *    zcnstr(flav,false,*::t) = varDup(zcnstr(flav,true,t)) ()    	*
 *    zcnstr(flav,true,n::t)  = infVarDup(zcnstr(flav,false,t)) n 	*
 *    zcnstr(flav,true,*::t)  = infVarDup(zcnstr(flav,true,t)) () 	*
 *									*
 ************************************************************************/

EXPR* make_var_expr_p(LIST *L, TYPE* v_content_type, EXPR *init_val, 
		      Boolean all, int line, MODE_TYPE *mode)
{
  EXPR* result        = NULL;
  TYPE* init_val_type = NULL;

  TYPE *tag;
  EXPR *id, *id_val, *bind_init;
  LIST *r, *r_head, *dims, *p;
  char *name, *real_name;

  int kind    = toint(get_define_mode(mode) & 7);
  int context = top_i(var_context_st);

  if(L == NULL) return NULL;

  /*------------------------------------------------------------*
   * Check that there is no initializer with an unknown array,  *
   * since none is allowed.					*
   *------------------------------------------------------------*/

  if(kind > 3 && init_val != NULL) {
    semantic_error(INIT_WITH_UNKNOWN_ARRAY_ERR, init_val->LINE_NUM);
    init_val = NULL;
    all = 0;
  }

  /*------------------------------------------------------------*
   * If L has more than one member and there is an initial	*
   * value, then be sure the initial value is computed only	*
   * once by putting it into an identifier.			*
   *------------------------------------------------------------*/

  bind_init = NULL;
  if(init_val != NULL && L->tail != NULL) {
    EXPR *init_var;
    bump_expr(bind_init = force_eval_p(init_val, &init_var));
    init_val = init_var;
  }

  /*-----------------------------------------------------*
   * Loop over all members of L, doing an expr for each. *
   * Accumulate all of the lets in result.		 *
   *-----------------------------------------------------*/

  for(r = L; r != NIL; r = r->tail) {
    r_head    = r->head.list;
    real_name = r_head->head.str;
    name      = (context == 1) 
                   ? my_name(real_name) 
                   : real_name;
    tag       = r_head->tail->head.type;
    dims      = r_head->tail->tail;
    id        = id_expr(name, line);
    bump_type(id->ty = tag);
    id        = same_e(id, line);

    /*----------------------------------------------------------*
     * Set up the type of id as it comes from v_content_type.   *
     *								*
     * If kind <= 3, then we need to use Box(v_content_type).	*
     * If kind > 3, then we use v_content_type directly.	*
     *								*
     * In the case of an array, this type is adjusted below.	*
     *----------------------------------------------------------*/

    if(kind > 3) {
      bump_type(id->ty = v_content_type);
    }
    else {
      TYPE *box_type;
      bump_type(box_type = box_t(v_content_type));
      replace_null_vars(&box_type);
      bump_type(init_val_type = box_type->TY1);
      SET_TYPE(id->ty, box_type);
      drop_type(box_type);
    }

    /*--------------------------------------------------------------*
     *** Get the expression id_val that creates the box or array. ***
     *--------------------------------------------------------------*/

    star_seen = FALSE;

    /*------------------------*
     * Case of a non-array.   *
     *------------------------*/

    if(dims == NIL) {
      EXPR *arg;
      char *cnstr;
      int cnstr_index;
      switch(kind) {
        case 0:
        case 1:
	  cnstr_index = (init_val == NULL) 
			  ? EMPTYSHAREDBOX_ID : SHAREDBOX_ID;
          break;

        case 2:
          cnstr_index = (init_val == NULL) 
			  ? EMPTYNONSHAREDBOX_ID : NONSHAREDBOX_ID;
          break;

	case 4:
        case 5:
          cnstr_index = SHAREDUNKNOWN_ID;
          break;

        case 6:
          cnstr_index = NONSHAREDUNKNOWN_ID;
          break;

      }
      cnstr = std_id[cnstr_index];
      arg   = (init_val == NULL) ? wsame_e(hermit_expr, line) : init_val;
      id_val = apply_expr(id_expr(cnstr, line), arg, line);
    }

    /*-------------------*
     * Case of an array. *
     *-------------------*/

    else {

      /*-------------------------------------*
       * Modify id->ty to reflect an array,  *
       * and also modify init_val_type.      *
       *------------------------------------ */

      for(p = dims; p != NULL; p = p->tail) {
	SET_TYPE(id->ty, list_t(id->ty));
	if(init_val_type != NULL) {
	  SET_TYPE(init_val_type, list_t(init_val_type));
	}
      }

      /*--------------------------------------------------------*
       * Now build the value to use in the let expression. 	*
       *							*
       * First case: an uninitialized array or an array that is *
       * initialized using all :=.				*
       *--------------------------------------------------------*/

      if(init_val == NULL || all) {
	id_val = topcnstr(kind, dims, init_val, line);
      }

      /*--------------------------------------------------------*
       * Second case: an array that is initialized from a list. *
       *--------------------------------------------------------*/

      else {
	id_val = apply_expr(var_icnstr(kind, 0, dims, line), init_val, line);
      }
    }

    /*-------------------------------------------------------------*
     * Build the let expression for this variable or array.  	   *
     *-------------------------------------------------------------*/

    result = bind_apply_expr(result,
			     new_expr2(LET_E, id, id_val, line),
			     line);

    /*-------------------------------------------------------------*
     * Add this variable or array to the list being built when     *
     * this is part of a class.					   *
     *-------------------------------------------------------------*/

    if(context == 1) {
      EXPR* node            = new_expr1t(SAME_E, NULL, id->ty, line);
      node->STR             = real_name;
      bump_mode(node->SAME_E_DCL_MODE = mode);
      if(mode != NULL) mode->define_mode &= ~7L;
      SET_LIST(class_vars, expr_cons(node, class_vars));
    }

  } /* end for(r = ...) */

  /*------------------------------------------------------------*
   * If we had to put the initial value into a variable, then	*
   * be sure to define that variable before defining the	*
   * boxes or arrays.						*
   *------------------------------------------------------------*/

  return possibly_apply(bind_init, result, line);
}


/****************************************************************
 *			MAKE_EXECUTE_P				*
 ****************************************************************
 * Return an execute declaration with body e.			*
 ****************************************************************/

EXPR* make_execute_p(EXPR *e, int line)
{
  return new_expr1(EXECUTE_E, 
		   new_expr1t(SAME_E, e, hermit_type, e->LINE_NUM),
		   line);
}


/****************************************************************
 *			ADD_IDS					*
 ****************************************************************
 * Add each id in list L to expression *ex in-place, creating   *
 * products, and adding in reverse order. But modify each	*
 * identifier by adding my- to its front.			*
 *								*
 * For example, if L contains ids [x,y,z] and variable *ex 	*
 * holds A initially, then after running this function *ex will *
 * hold (my-z, my-y, my-x, A).					*
 ****************************************************************/

void add_ids(EXPR **ex, EXPR_LIST *L, int line)
{
  EXPR_LIST *p;

  for(p = L; p != NIL; p = p->tail) {
    char* id = my_name(p->head.expr->STR);
    *ex = new_expr2(PAIR_E, id_expr(id, line), *ex, line);
  }
}


/****************************************************************
 *			ADD_IDS_AND_PATS			*
 ****************************************************************
 * Add each id in list L to expression *ex in-place, and add 	*
 * each as a pattern to expression *pat in-place, creating      *
 * products, and adding in reverse order.  But add my- to the	*
 * front of each identifier.					*
 *								*
 * For example, if list L contains ids [x,y,z], variable *ex	*
 * holds A initially and *pat holds B initially,		*
 * then after running this function 				*
 *    *ex   will hold (my-z, my-y, my-x, A).			*
 *    *pat  will hold (?my-z, ?my-y, ?my-x, B).			*
 ****************************************************************/

void add_ids_and_pats(EXPR **ex, EXPR **pat, EXPR_LIST *L, int line)
{
  EXPR_LIST *p;

  for(p = L; p != NIL; p = p->tail) {
    char* id = my_name(p->head.expr->STR);
    EXPR* pv = new_pat_var(id, line);
    *ex  = new_expr2(PAIR_E, pv->E1, *ex, line);
    *pat = new_expr2(PAIR_E, pv, *pat, line);
  }
}


/****************************************************************
 *		       MAKE_CLASS_CONSTRUCTOR_PARAM		*
 ****************************************************************
 * Build the parameter for a class constructor, where consts is *
 * the list of constants in the class and superclass is the     *
 * name of the superclass.					*
 *								*
 * Suppose that consts is the list [x,y,z].  Then		*
 *								*
 *  (1) If superclass is not Object, then the result expr is    *
 *       (my-z,my-y,my-x,super).				*
 *								*
 *  (2) If superclass is Object, then the result expr is	*
 *       (my-z,my-y,my-x,object!())				*
 ****************************************************************/

EXPR* make_class_constructor_param(EXPR_LIST *consts, char *superclass,
				   int line)
{
  EXPR *result;

  result = (superclass != std_type_id[OBJECT_TYPE_ID])
              ? id_expr("super", line)
              : apply_expr(id_expr(std_id[OBJECTCONSTR_ID], line),
			   same_e(hermit_expr, line), 
			   line);
  add_ids(&result, consts, line);
  return result;
}


/****************************************************************
 *			PERFORM_SIMPLE_DEFINE_P			*
 ****************************************************************
 * Return the expression for definition				*
 *   Define A = B.						*
 * or								*
 *   Let A = B.							*
 ****************************************************************/

EXPR* perform_simple_define_p(EXPR *A, EXPR *B)
{
  EXPR *body, *result;

  bump_expr(body = B);
			
  /*-------------------------------------------------*
   * Replace 					     *
   *						     *
   *   Let Ask T a => b. = c.			     *
   *						     *
   * by						     *
   *						     *
   *   Let Ask (?;this _also_ a) => b. =	     *
   *     Context T ;this 			     *
   *       c					     *
   *     %Context				     *
   *   %Let					     *
   *						     *
   * Note: in this case, the ask expression	     *
   * has already produced Ask (?;this _also_ a) => b.*
   *-------------------------------------------------*/

  if(EKIND(A) == SAME_E && A->SAME_MODE == 7) {
    SET_EXPR(body,
	     make_context_expr(A->STR, A->E3->E1, 
			       body, body->LINE_NUM, TRUE));
  }
 
  /*------------------------------------*
   * Replace 				*
   *					*
   *   Let f(?x) = e %Let 		*
   *					*
   * by					*
   *					*
   *   Let f = (?x => e) %Let		*
   *------------------------------------*/

  bump_expr(result = remove_appls_p(A, body, labs(top_i(deflet_st))));

  /*--------------------*
   * Replace 		*
   *			*
   *   Let ?x = e	*
   *			*
   * by			*
   *			*
   *   Let x = e	*
   *--------------------*/

  {EXPR** x = &(skip_sames(result)->E1);
   while(EKIND(*x) == SAME_E) x = &(*x)->E1;
   if(EKIND(*x) == PAT_VAR_E) set_expr(x, (*x)->E1);
  }

  drop_expr(body);

  if(result != NULL) result->ref_cnt--;
  return result;
}


/****************************************************************
 *			BUILD_CONSTRUCTOR_DEF_P			*
 ****************************************************************
 * Return the expression for definition				*
 *								*
 *   Define T constructor A = B then C.				*
 *								*
 * or a similar Let definition.					*
 *								*
 * If the constants of class C are a, b and c, and heading A is *
 * f(p), then the definition returned has the form		*
 *								*
 *   Define A =							*
 *       B							*
 *       Let result = ;construct-T!(my-a,my-b,my-c,super).	*
 *       Context T result =>					*
 *         C							*
 *       %Context						*
 *   %Define							*
 *								*
 * In the case where the superclass is Object, identifier super *
 * is replaced by expression object!().				*
 ****************************************************************/

EXPR* build_constructor_def_p(char *T, EXPR *A, EXPR *B, EXPR *C)
{
  char *basic_T_name, *construct_T_name;
  EXPR *result_id, *construct_T_params, *modified_C, *letbody, *let;
  EXPR *def, *body;

  int line = C->LINE_NUM;

  bump_expr(result_id = fresh_id_expr("result", line));
  basic_T_name       = concat_id(T, "!");
  construct_T_name   = attach_prefix_to_id(HIDE_CHAR_STR "construct-", 
					   basic_T_name, 1);
  construct_T_params = retrieve_class_info(T,TRUE);
  modified_C         = make_context_expr(T, result_id, C, line, FALSE);
  letbody            = apply_expr(id_expr(construct_T_name, line), 
				  construct_T_params, line);
  let                = new_expr2(LET_E, result_id, letbody, line);

  line = B->LINE_NUM;
  body = apply_expr(apply_expr(apply_expr(B, let, line), 
			       modified_C, line), 
		    result_id, line);
  bump_expr(def = remove_appls_p(A, body, labs(top_i(deflet_st))));
  drop_expr(result_id);
  if(def != NULL) def->ref_cnt--;
  return def;
}


/****************************************************************
 *			ROLE_MODIFY_EXPR			*
 ****************************************************************
 * l is a list of the form [r,a,s,b,...], where r,s,... are	*
 * names (identifiers) and a,b,... are expressions.  Produce	*
 * expression 							*
 *								*
 *      s'!!'(r'!!'(x,a),b)...					*
 ****************************************************************/

EXPR* role_modify_expr(EXPR *x, LIST *l, int line)
{
  EXPR* result = x;
  LIST* p = l;

  while(p != NIL) {
    char* r    = p->head.str;
    LIST* tl   = p->tail;
    EXPR* a    = tl->head.expr;
    char* newr = make_role_mod_id(new_name(r, TRUE));
    result     = apply_expr(id_expr(newr, line), 
			    new_expr2(PAIR_E, result, a, line),
			    line);
    result->ROLE_MOD_APPLY = 1;
    p          = tl->tail;
  }
  result = new_expr1(SINGLE_E, result, line);
  result->OFFSET = ROLE_MODIFY_ATT;
  return result;
}   
 

/****************************************************************
 *			ROLE_SELECT_EXPR			*
 ****************************************************************
 * l is a list of the form [r,a,s,b,...], where r,s,... are	*
 * names (identifiers) and a,b,... are expressions.  Produce	*
 * expression 							*
 *								*
 *    SINGLE_E[ROLE_SELECT_ATT] (r'??'(a), s'??'(b),...)	*
 ****************************************************************/

PRIVATE EXPR* role_select_expr_help(LIST *l, int line)
{
  if(l == NIL) return NULL;

  {LIST* tl    = l->tail;
   EXPR* rest  = role_select_expr_help(tl->tail, line);
   char* r     = new_name(l->head.str, TRUE);
   char* hasR  = make_role_sel_id(r);
   EXPR* a     = tl->head.expr; 
   EXPR* hasRa = apply_expr(id_expr(hasR, line), a, line);
   hasRa->ROLE_MOD_APPLY = 1;
   
   if(rest == NULL) return hasRa;
   return new_expr2(PAIR_E, hasRa, rest, line);
  }
}

/*----------------------------------------------------------*/

EXPR* role_select_expr(LIST *l, int line)
{
  EXPR* result = new_expr1(SINGLE_E, role_select_expr_help(l,line), line);
  result->OFFSET = ROLE_SELECT_ATT;
  return result;
}

