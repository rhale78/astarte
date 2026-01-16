/*****************************************************************
 * File:    dcls/dclutil.c
 * Purpose: Provide utilities for declaring entity-identifiers
 *          in the outer environment.
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

#include <string.h>
#include "../misc/misc.h"
#include "../alloc/allocate.h"
#include "../utils/lists.h"
#include "../utils/strops.h"
#include "../utils/hash.h"
#include "../unify/unify.h"
#include "../unify/constraint.h"
#include "../parser/tokens.h"
#include "../parser/parser.h"
#include "../lexer/modes.h"
#include "../exprs/expr.h"
#include "../classes/classes.h"
#include "../standard/stdtypes.h"
#include "../standard/stdids.h"
#include "../dcls/dcls.h"
#include "../evaluate/instruc.h"
#include "../tables/tables.h"
#include "../error/error.h"
#include "../patmatch/patmatch.h"
#include "../generate/prim.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif


/****************************************************************
 *			CUC_REP_TYPE				*
 ****************************************************************
 * Return the representation type as described by a class-union-*
 * cell entry.							*
 ****************************************************************/

TYPE* cuc_rep_type(CLASS_UNION_CELL *cuc)
{
  return (cuc->tok == TYPE_LIST_TOK)
            ? list_to_type_expr(cuc->CUC_TYPES, cuc->CUC_ROLES, 0).type
            : cuc->CUC_TYPE;
}


/****************************************************************
 *			DEFINE_BY_CAST				*
 *			DEFINE_BY_CAST_FROM_ID			*
 ****************************************************************
 * define_by_cast defines new_id: new_type as a cast from       *
 * old_expr, where the ty field of old_expr holds the type of   *
 * old_expr.  Casting indicates that the two values have the    *
 * same internal representation.				*
 *								*
 * Defer the declaration if defer is true.			*
 *								*
 * MODE gives the mode of the declaration.  It is a safe	*
 * pointer: it does not live longer than this function call.	*
 *								*
 * LINE is the line where the declaration occurs.		*
 *								*
 *--------------------------------------------------------------*
 * define_by_cast_from_id defines new_id:new_type as a cast	*
 * from old_id:old_type.  That is, it does			*
 *								*
 *   Let new_id:new_type = cast(old_id:old_type).		*
 *								*
 *--------------------------------------------------------------*
 * define_by_cast_from_nat defines new_id:new_type as a cast    *
 * from natural number val.					*
 *								*
 * XREF: 							*
 *   Called in dclclass.c and dclcnstr.c to create automatic	*
 *   definitions.						*
 ****************************************************************/

void define_by_cast(char *new_id, TYPE *new_ty,
		    EXPR *old_expr, 
		    MODE_TYPE *mode, Boolean defer, int line)
{
  TYPE* old_type    = old_expr->ty;
  EXPR* caster      = make_cast(old_type, new_ty, line);
  EXPR* new_id_expr = typed_id_expr(new_id, new_ty, line);
  EXPR* val         = apply_expr(caster, old_expr, line);
  EXPR* def         = new_expr2(LET_E, new_id_expr, val, line);

  bump_type(val->ty = new_ty);
  bump_type(def->ty = hermit_type);
  bump_expr(def);
  if(defer) defer_issue_dcl_p(def, LET_E, mode);
  else issue_define_p(def, mode, TRUE);
  drop_expr(def);
}

/*------------------------------------------------------------------*/

void define_by_cast_from_id(char *new_id, TYPE *new_ty,
		            char *old_id, TYPE *old_type, 
		            MODE_TYPE *mode, Boolean defer, int line)
{
  EXPR* old_expr = typed_id_expr(old_id, old_type, line);
 
  define_by_cast(new_id, new_ty, old_expr, mode, defer, line);
}


/*------------------------------------------------------------------*/

void define_by_cast_from_nat(char *new_id, TYPE *new_ty, long val,
		             MODE_TYPE *mode, Boolean defer, int line)
{
  char val_as_str[16];
  EXPR* val_as_expr;

  sprintf(val_as_str, "%ld", val);
  val_as_expr = const_expr(string_const_tb0(val_as_str), NAT_CONST, 
			   natural_type, line);
  define_by_cast(new_id, new_ty, val_as_expr, mode, defer, line);
}


/************************************************************************
 *			DEFINE_BY_COMPOSE				*
 ************************************************************************
 * Issue definition							*
 *									*
 *   Let a = b _o_ c.							*
 *									*
 * where a: X -> Z,							*
 *       b: Y -> Z,							*
 *       c: X -> Y.							*
 *									*
 * The definition is deferred if defer is true.				*
 ************************************************************************/

void define_by_compose(char *a, char *b, char *c, 
		       TYPE *X, TYPE *Y, TYPE *Z, 
		       Boolean defer, MODE_TYPE *mode, int line)
{
  EXPR *a_id, *b_id, *c_id, *compose, *compose_arg, *def;

  a_id         = typed_id_expr(a, function_t(X, Z), line);
  b_id         = typed_id_expr(b, function_t(Y, Z), line);
  c_id         = typed_id_expr(c, function_t(X, Y), line);

  compose      = global_sym_expr(COMPOSE_ID, line);
  compose_arg  = new_expr2(PAIR_E, b_id, c_id, line);
  def          = new_expr2(LET_E, a_id, 
			   apply_expr(compose, compose_arg, line),
			   line);
  
  bump_expr(def);
  (defer ? defer_issue_dcl_p : issue_dcl_p)(def, LET_E, mode);
  drop_expr(def);
}


/************************************************************************
 *			PAD_SYM_DEF					*
 ************************************************************************
 * Issue a missing declaration for std_id[sym].  sym must		*
 * be one of								*
 *   EQ_SYM								*
 *   DOLLAR_SYM 							*
 *   PULL_ID								*
 *   COPY_ID								*
 *   DBL_HAT_SYM							*
 *   DOWNCAST_SYM							*
 * and t should be of the form F(-), where F is a family; 		*
 * otherwise nothing is done.  						*
 *									*
 * If necessary, one of the following will be done.			*
 *									*
 *   (a) If F is transparent and sym is EQ_SYM, then equality will be   *
 *       declared missing of type (F(EQ`a), F(EQ`a)) -> Boolean.	*
 *									*
 *   (b) If F is opaque and sym is EQ_SYM, then equality will be	*
 *       declared missing of type (F(`a), F(`a)) -> Boolean.		*
 *									*
 *   (c) If sym is DOLLAR_SYM then $ will be declared missing of type	*
 *       F(`a) -> String.						*
 *									*
 *   (d) If sym is PULL_ID then pull will be declared missing of type   *
 *       String -> (F(`a), String).					*
 *									*
 *   (e) If sym is COPY_ID then copy will be declared missing of type   *
 *       CopyFlavor -> F(`a) -> F(`a).					*
 *									*
 *   (f) If sym is DBL_HAT_SYM then ^^ is
 *       will be declared missing of species F(`a) -> F(`a).		*
 *									*
 * The missing declaration is deferred.  (This should only be called    *
 * from inside an extension.)						*
 *									*
 * XREF: Called in dclcnstr.c to pad definitions.			*
 ************************************************************************/

void pad_sym_def(int sym, TYPE *t)
{
  TYPE *ty1, *gt, *pad_type, *fam;
  Boolean is_opaque;
  
  /*----------------------------------------------------------------------*
   * Check whether t is a family member.  If not, there is nothing to do. *
   * If so, get the family and determine whether it is opaque.		  *
   *----------------------------------------------------------------------*/

  t = find_u(t);
  if(TKIND(t) != FAM_MEM_T) return;

  fam       = find_u(t->TY2);
  is_opaque = fam->ctc->opaque;

  /*----------------------------------------------------*
   * If t is already sufficiently general, then there 	*
   * is no need to do any missing declaration.		*
   *----------------------------------------------------*/

  ty1 = find_u(t->TY1);
  if(ty1 == NULL || 
     (TKIND(ty1) == TYPE_VAR_T && 
      (ty1->ctc == NULL || 
       (sym == EQ_SYM && !is_opaque && ty1->ctc == EQ_ctc)))) {
    return;
  }

  /*---------------------------*
   * Build the type to pad to. *
   *---------------------------*/

  if(sym == EQ_SYM) {
    gt       = fam_mem_t(fam, is_opaque ? any_type : any_EQ);
    pad_type = function_t(pair_t(gt, gt), boolean_type);
  }
  else if(sym == DOLLAR_SYM) {
    gt       = fam_mem_t(fam, any_type);
    pad_type = function_t(gt, string_type);
  }
  else if(sym == PULL_ID) {
    gt       = fam_mem_t(fam, any_type);
    pad_type = function_t(string_type, pair_t(gt, string_type));
  }
  else if(sym == COPY_ID) {
    gt       = fam_mem_t(fam, any_type);
    pad_type = function_t(copyflavor_type, function_t(gt, gt));
  }
  else if(sym == DBL_HAT_SYM || sym == DOWNCAST_ID) {
    TYPE* dom_var = var_t(NULL);
    TYPE* cod_var = var_t(NULL);
    TYPE* dom     = fam_mem_t(fam, dom_var);
    TYPE* cod     = fam_mem_t(fam, cod_var);
    pad_type       = function_t(dom, cod);
    if(sym == DBL_HAT_SYM) CONSTRAIN(cod_var, dom_var, 0, 0);
    else CONSTRAIN(dom_var, cod_var, 0, 0);
  }
  else return;
  bump_type(pad_type);

  /*-----------------------*
   * Issue the definition. *
   *-----------------------*/

  defer_issue_missing_tm(std_id[sym], pad_type, 0);
  drop_type(pad_type);
}


/****************************************************************
 *			AUTO_PAT_CONST_P			*
 ****************************************************************
 * Declare id of type id_type to be a pattern constant with	*
 * given tester of type tester_type.  That is, issue declaration*
 *								*
 *   Pattern id:id_type => {tester(target)}.			*
 *								*
 * If tester == NULL, then the pattern constant matches		*
 * everything.  So the definition in that case is		*
 *								*
 *   Pattern id:id_type => ().					*
 *								*
 * If defer is true, then put the declaration in deferred_dcls. *
 * Otherwise, do the declation now.				*
 *								*
 * XREF:							*
 *   Called in dclcnstr.c to create automatic pattern constants *
 *   for enumerated species.					*
 *								*
 *   Called in tables/primtbl.c to create pattern constants	*
 *   for primitive exceptions.					*
 ****************************************************************/

void auto_pat_const_p(char *id, TYPE *id_type, char *tester, 
		      TYPE *tester_type, Boolean defer)
{
  int line = current_line_number;
  EXPR *patfun, *rule, *pat_dcl, *test;

  bump_type(id_type);
  bump_type(tester_type);

# ifdef DEBUG
    if(trace_defs) {
      trace_t(35, id);
      trace_ty(id_type); fnl(TRACE_FILE);
    }
# endif

  /*-----------------------------*
   * Build the pattern constant. *
   *-----------------------------*/

  bump_expr(patfun	= new_expr1t(PAT_FUN_E, NULL_E, id_type, line));
  patfun->STR 		= id;

  /*--------------------------------------------------------------------*
   * Build the test expression {tester(target)}, but handle the case 	*
   * of the tester being NULL by setting the test to (). 		*
   *--------------------------------------------------------------------*/

  if(tester != NULL) {
    EXPR *image_expr, *tester_expr, *appl_expr;

    image_expr              = typed_target(id_type, patfun, line);
    tester_expr 	    = typed_id_expr(tester, tester_type, line);
    appl_expr		    = apply_expr(tester_expr, image_expr, line);
    bump_type(appl_expr->ty = find_u(tester_type)->TY2);
    test		    = new_expr1(TEST_E, appl_expr, line);
  }
  else {
    test = same_e(hermit_expr, line);
  }
  bump_type(test->ty	= hermit_type);

  /*-------------------------*
   * Build the pattern rule. *
   *-------------------------*/

  bump_expr(rule        = new_expr1(PAT_RULE_E, NULL_E, line));
  bump_expr(rule->E2    = patfun);
  bump_expr(rule->E3 	= test);
  rule->PAT_RULE_MODE   = 0;
  rule->ETAGNAME	= id;

  /*----------------------------------*
   * Build and issue the declaration. *
   *----------------------------------*/

  bump_expr(pat_dcl	= new_expr2(PAT_DCL_E, patfun, rule, line));
  if(defer) defer_issue_dcl_p(pat_dcl, PAT_DCL_E, 0);
  else issue_dcl_p(pat_dcl, PAT_DCL_E, 0);

  drop_expr(pat_dcl);
  drop_expr(rule);
  drop_expr(patfun);
  drop_type(id_type);
  drop_type(tester_type);
}


/****************************************************************
 *			BASIC_PAT_FUN_P				*
 ****************************************************************
 * Declare function constr_name as a pattern function of type	*
 * constr_type, where function destr_name:destr_type implements	*
 * the inverse function.  That is, issue declaration		*
 *								*
 *   Pattern constr_name(p) =>					*
 *     match p = destr_name(target).				*
 *   %Pattern							*
 *								*
 * Defer the declaration if defer is true.			*
 *								*
 * XREF: Called in tables/primtbl.c to create pattern functions *
 * for primitive exceptions.					*
 ****************************************************************/

void basic_pat_fun_p(char *constr_name, TYPE *constr_type, 
		     char *destr_name, TYPE *destr_type, Boolean defer)
{
  EXPR *destr;

  bump_expr(destr = typed_id_expr(destr_name, destr_type, 0));
  auto_pat_fun_p(constr_name, constr_type, NULL, destr, defer);
  drop_expr(destr);
}


/****************************************************************
 *			AUTO_PAT_FUN_P				*
 *			LONG_AUTO_PAT_FUN_P			*
 ****************************************************************
 * Auto_pat_fun_p declares function constr_name as a pattern 	*
 * function of type constr_type and role r, where function 	*
 * destructor implements the inverse function.  The declaration	*
 * has the form							*
 *								*
 *    Pattern constr_name(p) => 				*
 *      match p = destructor(target).				*
 *    %Pattern							*
 *								*
 * The declaration is deferred if defer is true.		*
 *								*
 * The constructor name stored in the constructor list (for	*
 * reporting that choices using pattern matching are not	*
 * exhaustive) is obtained from constr_name by removing any	*
 * leading periods.						*
 *								*
 * Long_auto_pat_fun_p is similar, but sets primitive info for	*
 * the pattern function if set_prims is true.  The primitive	*
 * info will be placed in the global id table, so that it can	*
 * be retrieved when identifier constr_name is used.  The 	*
 * primitive info is given by posn_list, discr and irregular.   *
 * The primitive is always PRIM_SELECT when set_prims is true.	*
 *								*
 * XREF:							*
 *   Called in dclclass.c and dclcnstr.c to create automatic	*
 *   pattern functions at species definitions.			*
 ****************************************************************/

void long_auto_pat_fun_p(char *constr_name, TYPE *constr_type, 
				ROLE *r, EXPR  *destructor, Boolean defer,
				Boolean set_prims, INT_LIST *posn_list,
				int discr, Boolean irregular)
{
  EXPR *patfun, *target_expr, *rule, *pat_dcl, *patvar;
  int line = current_line_number;

# ifdef DEBUG
    if(trace_defs) {
      trace_t(36, constr_name);
      trace_ty(constr_type); fnl(TRACE_FILE);
    }
# endif

  /*---------------------------------------------------------------*
   * Get the type, and build the head of the declaration.  Install *
   * primitive information in the head if called for. 		   *
   *---------------------------------------------------------------*/

  bump_type(constr_type  = copy_type(constr_type, 0));
  bump_expr(patfun       = new_expr1t(PAT_FUN_E, NULL_E, constr_type, line));
  patfun->STR 	         = constr_name;
  bump_role(patfun->role = r);
  if(set_prims) {
    bump_list(patfun->EL3 = posn_list);
    patfun->PRIMITIVE     = discr;
    patfun->irregular     = irregular;
    patfun->patfun_prim  = 1;
  }

  /*-----------------------------------------------------------*
   * Build target, and the right-hand-side of the declaration. *
   *-----------------------------------------------------------*/

  bump_expr(target_expr = typed_target(constr_type->TY2, patfun, line));
  bump_expr(rule        = new_expr1(PAT_RULE_E, NULL_E, line));
  patvar		= new_pat_var(HIDE_CHAR_STR "z", line);
  bump_expr(rule->E2    = apply_expr(patfun, patvar, line));
  bump_expr(rule->E3 	= new_expr2(MATCH_E, patvar, 
		             apply_expr(destructor, target_expr, line), line));
  rule->PAT_RULE_MODE   = 0;
  rule->ETAGNAME	= remove_dots(constr_name);
  if(destructor->PRIMITIVE == PRIM_UNWRAP) {
    rule->E3->SCOPE = 1;  /* Make an open match, to preserve type in env.*/
  }

  /*----------------------------------*
   * Build and issue the declaration. *
   *----------------------------------*/

  bump_expr(pat_dcl	= new_expr2(PAT_DCL_E, patfun, rule, line));
  (defer ? defer_issue_dcl_p : issue_dcl_p)(pat_dcl, PAT_DCL_E, 0);

  drop_expr(pat_dcl);
  drop_expr(rule);
  drop_expr(target_expr);
  drop_expr(patfun);
  drop_type(constr_type);
}

/*---------------------------------------------------------------*/

void auto_pat_fun_p(char *constr_name, TYPE *constr_type, 
		    ROLE *r, EXPR  *destructor, Boolean defer)
{
  long_auto_pat_fun_p(constr_name, constr_type, r, destructor, defer,
		      FALSE, NIL, 0, 0);
}

