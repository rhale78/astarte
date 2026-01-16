/*****************************************************************
 * File:    dcls/dclcnstr.c
 * Purpose: Declare constructors and destructors for types and families
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
 * The functions in this file are responsible for creating		*
 * things that are automatically created when a species or family	*
 * is declared.  They include constructors, destructors, field 		*
 * selectors, pattern functions/constants, and definitions of == and $.	*
 *									*
 * These functions do the specific definitions.  The high level 	*
 * decisions about doing definitions are made in dclclass.c.		*
 ************************************************************************/

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
#include "../machstrc/machstrc.h"
#include "../machdata/except.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

PRIVATE EXPR* 
make_special_prim_p(int prim, INT_LIST *posnlist, 
		     int discr, Boolean irregular, char *name, int line);

/********************************************************************
 *			suppress_role_extras			    *
 ********************************************************************
 * If suppress_role_extras is true, then the role functions	    *
 * ;role_name'??' and ;role_name'!!' are not defined when defining  *
 * a new species.						    *
 ********************************************************************/

Boolean suppress_role_extras = FALSE;

/********************************************************************
 *			DECLARE_EQUALITY_BY_CAST		    *
 ********************************************************************
 * Declare == :(t',t')->Boolean as a cast from == :(r,r)->Boolean,  *
 * where 							    *
 *    t' is t if t is a type identifier, and 			    *
 *    t' is F(A) if t is F(B), where A is obtained from B by 	    *
 *       unifying with EQ`x.     				    *
 * One of the following declarations will be issued.		    *
 *								    *
 *  (1) Let{default} ==:(t,t)->Boolean = (==:(r,r)->Boolean).	    *
 *								    *
 *  (2) Let{default} ==:(F(A),F(A))->Boolean = (==:(r,r)->Boolean). *
 *								    *
 * If t is F(s) for a transparent family F and an s that does not   *
 * include all of EQ, then == is padded to 			    *
 * (F(EQ`a), F(EQ`a)) -> Boolean, to prevent an	    		    *
 * incomplete-definition warning.				    *
 *								    *
 * If t is F(s) for an opaque family F and an s that does not       *
 * include all of ANY, then == is padded to 			    *
 * (F(`a), F(`a)) -> Boolean, to prevent an	    		    *
 * incomplete-definition warning.				    *
 *								    *
 * If type r does not support an equality test, then this cast	    *
 * cannot be done.  In that case, issue a warning and return 0.  On *
 * success, return 1.						    *
 *								    *
 * XREF: Called in dclclass.c to declare == at a species	    *
 *       declaration.						    *
 ********************************************************************/

Boolean declare_equality_by_cast(TYPE *t, TYPE *r)
{
  TYPE *eq_type1, *eq_type2;
  MODE_TYPE *mode;
  int line = current_line_number;

  /*------------------------------------------------------------------------*
   * Get the types.  Copy, since they can be altered during type inference. *
   *------------------------------------------------------------------------*/

  {HASH2_TABLE* ty_b = NULL;
   bump_type(t);
   bump_type(r);
   SET_TYPE(t, copy_type1(t, &ty_b, 0));
   SET_TYPE(r, copy_type1(r, &ty_b, 0));
   scan_and_clear_hash2(&ty_b, drop_hash_type);
  }

  /*-----------------*
   * Restrict to EQ. *
   *-----------------*/

  if(!eq_restrict(t) || !unify_with_eq(&r)) {
    warn0(CANNOT_DEFINE_EQUAL_ERR, 0);
    drop_type(t);
    drop_type(r);
    return 0;
  }

  /*--------------------------------------------------*
   * Get the types of == to cast from and to cast to. *
   *--------------------------------------------------*/

  bump_type(eq_type1 = function_t(pair_t(t,t), boolean_type));
  bump_type(eq_type2 = function_t(pair_t(r,r), boolean_type));

  /*------------------------------------*
   * Build the definition and issue it. *
   *------------------------------------*/

  mode = simple_mode(DEFAULT_MODE);      /* ref cnt is 1. */
  define_by_cast_from_id(std_id[EQ_SYM], eq_type1, 
		         std_id[EQ_SYM], eq_type2, 
		         mode, TRUE, line);
  drop_mode(mode);

  /*-------------------------------------------------------------------*
   * Possibly generalize ==.  This is necessary when defining equality *
   * for a family F(C) where C includes types that are not in EQ.      *
   *-------------------------------------------------------------------*/

  pad_sym_def(EQ_SYM, t);

  /*------------------*
   * Drop and return. *
   *------------------*/

  drop_type(eq_type1);
  drop_type(eq_type2);
  drop_type(t);
  drop_type(r);
  return 1;
}


/****************************************************************
 *			DECLARE_EQUALITY_BY_QEQ			*
 ****************************************************************
 * Declare ==:(t',t')->Boolean for a coproduct type defined by  *
 *								*
 *    Species t = l %Species					*
 *								*
 * where l is the list of members.  Obtain			*
 * t' from t as for function declare_equality_by_cast.		*
 * The declaration is as follows.  Suppose that type t is 	*
 * declared by							*
 *								*
 *   Species t = d1 | d2(R) | ...				*
 *								*
 *   Let ==(?x,?y) =						*
 *     Choose first						*
 *       case do (unwrap1 x) => (unwrap1 y) true		*
 *       case do Let a = unwrap2(x). =>				*
 *         Try Let b = unwrap2(y). 				*
 *           then a == b					*
 *           else false						*
 *         %Try							*
 *       ...							*
 *       else => false						*
 *     %Choose							*
 *   %Let							*
 *								*
 * where unwrap1 tests whether its argument has the same	*
 * tag as d1 (failing it not), and unwrap2 tests whether its	*
 * argument has the same tag as d2, and returns its argument	*
 * with the tag stripped.  For irregular parts, the unwrappers	*
 * strip off the type-tag as well.				*
 *								*
 * XREF: Called in dclclass.c to declare == at a species	*
 *       declaration.						*
 ****************************************************************/

void declare_equality_by_qeq(TYPE *t, LIST *l)
{
  EXPR *cases, *fun, *def = NULL,
       *eq_id, *pv1, *pv2, *param1, *param2,
       *false_expr, *true_expr;
  TYPE *eq_type;
  int line, n;
  LIST *p;
  MODE_TYPE *mode;
  HASH2_TABLE* ty_b = NULL;

  bump_list(l);
  line = current_line_number;

  /*------------------------------------------------------------*
   * Get the type of equality.  Copy, since type inference will *
   * do bindings.  Restrict it to EQ.				*
   *------------------------------------------------------------*/

  bump_type(t);
  SET_TYPE(t, copy_type1(t, &ty_b, 0));
  if(!eq_restrict(t)) {
    warn0(CANNOT_DEFINE_EQUAL_ERR, 0);
    goto out1;
  }
  bmp_type(eq_type = function_t(pair_t(t,t), boolean_type));

  /*------------------------------------*
   * Build the definition and issue it. *
   *------------------------------------*/

  bmp_expr(pv1 = new_pat_var(HIDE_CHAR_STR "x", line));
  bmp_expr(pv2 = new_pat_var(HIDE_CHAR_STR "y", line));
  param1 = pv1->E1;
  param2 = pv2->E1;

  bmp_expr(false_expr = const_expr(NULL, BOOLEAN_CONST, boolean_type, line));
  bmp_expr(true_expr  = const_expr("", BOOLEAN_CONST, boolean_type, line));

  /*------------------------------------------------------------*
   * Variable n keeps the tag as we go through each part	*
   * of the species.						*
   *------------------------------------------------------------*/

  bmp_expr(cases = false_expr);
  for(p = l, n = 0; p != NULL; p = p->tail, n++) {
    EXPR *testeq, *this_eq_expr, *unwrapper, *x, *y, *getp1, *getp2;
    TYPE *rept, *this_eq_type;
    CLASS_UNION_CELL *cuc;
    int prim;

    /*------------------------------------------------------------------*
     * Get the representation type rept of this entry, and check 	*
     * that equality is defined for that type.  We can't define		*
     * equality for the new type in terms of equality in the rep	*
     * type unless equality is is supported by the rep type.		*
     *------------------------------------------------------------------*/

    cuc = p->head.cuc;
    if(cuc->tok == TYPE_ID_TOK) {
      rept = copy_type1(cuc->CUC_TYPE, &ty_b, 0);
    }
    else {
      RTYPE rt;
      rt   = list_to_type_expr(cuc->CUC_TYPES, cuc->CUC_ROLES, 0);
      rept = rt.type;
    }
    bump_type(rept);
    if(!unify_with_eq(&rept)) {
      warn0(CANNOT_DEFINE_EQUAL_ERR, 0);
      drop_type(rept);
      goto out;
    }

    /*------------------------------------------------------------------*
     * Build the unwrapper.  It produces something of type rept		*
     * regardless of the case, since it produces () when rept is ().	*
     * Build expressions unwrap(x) and unwrap(y).			*
     *------------------------------------------------------------------*/

    prim      = (cuc->special) ? PRIM_DUNWRAP : PRIM_QUNWRAP;
    unwrapper = make_special_prim_p(prim,NULL,n,0,"unwrap",line);
    SET_TYPE(unwrapper->ty, function_t(NULL, rept));
    getp1     = apply_expr(unwrapper, param1, line);
    getp2     = apply_expr(unwrapper, param2, line);

    /*-----------------------------------------------------------------*
     * Build the case, whose form depends on whether rept = () or not. *
     *-----------------------------------------------------------------*/

    if(is_hermit_type(rept)) {
      SET_EXPR(cases,
	try_expr(getp1,
	         apply_expr(getp2, true_expr, line),
	         cases, TRY_F, line));
    }
    else {
      x = id_expr(HIDE_CHAR_STR "a", line);
      y = id_expr(HIDE_CHAR_STR "b", line);

      bump_type(this_eq_type = function_t(pair_t(rept,rept), boolean_type));
      SET_TYPE(this_eq_type, copy_type1(this_eq_type, &ty_b, 0));
      this_eq_expr = typed_id_expr(std_id[EQ_SYM], this_eq_type,
				   line);
      drop_type(this_eq_type);
      if(this_eq_expr == NULL) {
	warn0(CANNOT_DEFINE_EQUAL_ERR, 0);
	goto out;
      }
      testeq = apply_expr(this_eq_expr,
			  new_expr2(PAIR_E, x, y, line),
			  line);
      SET_EXPR(cases,
	  try_expr(new_expr2(LET_E, x, getp1, line),
	           try_expr(new_expr2(LET_E, y, getp2, line),
			    testeq,
			    false_expr, TRY_F, line),
		   cases, TRY_F, line));
    }
    drop_type(rept);
  }

  /*------------------------------------------------------------*
   * Now build the definition and issue it as a deferred	*
   * definition.						*
   *------------------------------------------------------------*/

  eq_id            = new_expr1t(IDENTIFIER_E, NULL, eq_type, line);
  eq_id->STR       = std_id[EQ_SYM];
  eq_id->PRIMITIVE = PRIM_QEQ;
  fun = new_expr2(FUNCTION_E, new_expr2(PAIR_E, pv1, pv2, line), cases, line);
  bump_expr(def = new_expr2(LET_E, eq_id, fun, line));
  mode = simple_mode(DEFAULT_MODE);  /* ref cnt is 1. */
  defer_issue_dcl_p(def, LET_E, mode);
  drop_mode(mode);

  /*-------------------------------------------------------------------*
   * Possibly generalize ==.  This is necessary when defining equality *
   * for a family F(C) where C includes types that are not in EQ.      *
   *-------------------------------------------------------------------*/

  pad_sym_def(EQ_SYM, t);

out:
  drop_expr(def);
  drop_expr(cases);
  drop_expr(false_expr);
  drop_expr(true_expr);
  drop_type(eq_type);
  drop_expr(pv1);
  drop_expr(pv2);
 out1:
  drop_list(l);
  drop_type(t);
  scan_and_clear_hash2(&ty_b, drop_hash_type);
}


/************************************************************************
 *			DECLARE_CONSTRUCTOR_FOR_HERMIT			*
 ************************************************************************
 * This function is a tool for declare_constructor_p.  See it for	*
 * details of the setting and parameters.				*
 *									*
 * A species or exception declaration is being given.			*
 * Declare the constructor constant constr_name for a case where	*
 * the domain is ().  This is what happens, for example,   		*
 * at an enumerated type, or at an exception declaration with    	*
 * no information contained in the exception.				*
 *									*
 * mode		is the mode of the declaration.				*
 *		It is a safe pointer: it will not be kept		*
 *		longer than the lifetime of this function call.		*
 ************************************************************************/

typedef Boolean (*ISSUE_DCL_FUN_TYPE)(EXPR *e, int kind, MODE_TYPE *mode);

PRIVATE void
declare_constructor_for_hermit(char *constr_name, TYPE *codomain, 
			       Boolean trap, int line, int prim, 
			       int arg, MODE_TYPE *mode,
			       Boolean build_declaration,
			       ISSUE_DCL_FUN_TYPE issue_dcl_fun)
{
  TYPE *codomain_cpy;
  EXPR *this_id_expr;

  /*---------------------------------------------------------------*
   * Copy the codomain, to insulate other dcls from the effects of *
   * issuing this one. 						   *
   *---------------------------------------------------------------*/

  bump_type(codomain_cpy = copy_type(codomain, 0));

  /*-----------------------------------------*
   * Build the constructor id being defined. *
   *-----------------------------------------*/

  bump_expr(this_id_expr = 
	    new_expr1t(GLOBAL_ID_E, NULL_E, codomain_cpy, line));
  this_id_expr->STR   = constr_name;
  this_id_expr->bound = trap;

  /*------------------------------------------------------*
   * Install the primitive info for the id being defined. *
   * An exception must be made when the primitive is      *
   * PRIM_EXC_WRAP and we are not building the definition,*
   * since then we have no global label in arg.           *
   *------------------------------------------------------*/

  {register int primitive, argument;
   switch(prim) {
     case PRIM_CAST:
       primitive = PRIM_CONST;
       argument  = HERMIT_I;
       break;

     case PRIM_QWRAP:
       primitive = STD_CONST;
       argument  = arg;
       break;

     case PRIM_WRAP: 
       primitive = 0;
       argument = 0;
       break;

     case PRIM_EXC_WRAP:
       if(build_declaration) {
         primitive = EXCEPTION_CONST;
         argument  = arg;
       }
       else {
         primitive = argument = 0;
       }
       break;

     default:
       primitive = argument = 0;
   }
   this_id_expr->PRIMITIVE = primitive;
   this_id_expr->SCOPE     = argument;
  }

  /*-----------------------------------------------------*
   * Build the declaration, and issue it, if called for. *
   *-----------------------------------------------------*/

  if(build_declaration) {
    EXPR* def;
    EXPR* body = new_expr1t(SPECIAL_E, NULL_E, codomain_cpy, line);

    body->STR       = constr_name;
    body->PRIMITIVE = this_id_expr->PRIMITIVE;
    body->SCOPE     = this_id_expr->SCOPE;

    if(prim == PRIM_WRAP) {
      SET_TYPE(body->ty, function_t(hermit_type, codomain_cpy));
      body->PRIMITIVE = PRIM_WRAP;
      body = apply_expr(body, same_e(hermit_expr, line), line);
    }

    bump_expr(def = new_expr2(LET_E, this_id_expr, body, line));
    issue_dcl_fun(def, LET_E, mode);
    drop_expr(def);
  }

  /*----------------------------------------------------------------*
   * If building the declaration is not called for, just put the id *
   * into the table.						    *
   *----------------------------------------------------------------*/

  else {
    quick_issue_define_p(this_id_expr, constr_name, mode, line);
  }

  drop_type(codomain_cpy);
  drop_expr(this_id_expr);
}


/************************************************************************
 *			DECLARE_CONSTRUCTOR_FUN				*
 ************************************************************************
 * This is a tool for declare_constructor_p.  See that function for     *
 * details on parameters.						*
 *									*
 * A species or exception declaration is being given.  Declare 		*
 * constructor constr_name in the case where the domain is not ().	*
 *									*
 * mode		is the mode of the declaration.				*
 *		It is a safe pointer: it will not be kept		*
 *		longer than the lifetime of this function call.		*
 ************************************************************************/

PRIVATE void 
declare_constructor_fun(char *constr_name, ROLE *constr_role,
			TYPE *domain, TYPE *codomain, 
			TYPE *constr_domain, TYPE *constr_codomain,
			Boolean withs, Boolean trap, int prim, int arg, 
			MODE_TYPE *mode, MODE_TYPE *special_mode,
			int line, Boolean build_declaration,
			ISSUE_DCL_FUN_TYPE issue_dcl_fun)
{
  TYPE *domain_cpy, *codomain_cpy, *constr_type, *constr_domain_cpy,
       *constr_codomain_cpy;
  EXPR *this_id_expr;

  /*----------------------------------------------------------*
   * Copy types to insulate other declarations from this one. *
   *----------------------------------------------------------*/

  {HASH2_TABLE* ty_b = NULL;
   bump_type(domain_cpy = copy_type1(domain, &ty_b, 0));
   bump_type(codomain_cpy = copy_type1(codomain, &ty_b, 0));  
   if(constr_domain == domain && constr_codomain == codomain) {
     bump_type(constr_domain_cpy = domain_cpy);
     bump_type(constr_codomain_cpy = codomain_cpy);
   }
   else {
     bump_type(constr_domain_cpy = copy_type1(constr_domain, &ty_b, 0));
     bump_type(constr_codomain_cpy = copy_type1(constr_codomain, &ty_b, 0));
   }
   bump_type(constr_type = function_t(domain_cpy, codomain_cpy));
   scan_and_clear_hash2(&ty_b, drop_hash_type);
  }
  
  /*------------------------------------------------------------*
   * Get the identifier being defined, and install primitive	*
   * info.  Do not install primitive info in the case where	*
   * we are not building the definition, and the primitive is 	*
   * PRIM_EXC_WRAP, because then arg is not known.		*
   *------------------------------------------------------------*/

  bump_expr(this_id_expr = new_expr1t(GLOBAL_ID_E, NULL_E, constr_type, line));
  this_id_expr->STR = constr_name;
  this_id_expr->bound = trap;
  bump_role(this_id_expr->role = constr_role);
  if(prim != PRIM_EXC_WRAP || build_declaration) {
    this_id_expr->PRIMITIVE = prim;
    this_id_expr->SCOPE     = arg;
  }

  /*------------------------------------------------------------*
   * If asked to build the declaration, build it and issue the  *
   * declaration.						*
   *------------------------------------------------------------*/

  if(build_declaration) {
    MODE_TYPE *constr_mode;
    EXPR* def;
    EXPR* body 	    	    = new_expr1(SPECIAL_E, NULL, line);

    body->PRIMITIVE         = prim;
    body->STR               = constr_name;
    body->SCOPE             = arg;
    bump_type(body->ty      = function_t(constr_domain_cpy, 
					 constr_codomain_cpy));

    /*---------------------------------------------------------------*
     * If there is a with-phrase, we need an application of 'forget' *
     * to the body.						      *
     *---------------------------------------------------------------*/

    constr_mode = mode;
    if(withs) {
      EXPR* pv        = new_pat_var(HIDE_CHAR_STR "x", line);
      EXPR* forgetfun = typed_global_sym_expr(FORGET_ID,
					      copy_type(forget_type,0),line);
      body = new_expr2(FUNCTION_E, pv, 
		       apply_expr(forgetfun, 
				  apply_expr(body, pv->E1, line), 
				  line),
		       line);
      constr_mode = special_mode;
    }
    bump_expr(def = new_expr2(LET_E, this_id_expr, body, line));

    /*--------------------------*
     * Declare the constructor. *
     *--------------------------*/

    issue_dcl_fun(def, LET_E, constr_mode);

    drop_expr(def);
  }

  /*--------------------------------------------------------------------*
   * If we are not asked to build the declaration, then just define	*
   * the id. 								*
   *--------------------------------------------------------------------*/

  else {
    quick_issue_define_p(this_id_expr, constr_name, mode, line);
  }

  drop_type(domain_cpy);
  drop_type(codomain_cpy);
  drop_type(constr_domain_cpy);
  drop_type(constr_codomain_cpy);
  drop_type(constr_type);
  drop_expr(this_id_expr);
}


/************************************************************************
 *			DECLARE_DESTRUCTOR_FUN				*
 ************************************************************************
 * This is a tool for declare_constructor_p.  See that function for     *
 * details of parameters.						*
 *									*
 * A species or family is being defined.				*
 * Declare the destructor that is the inverse of constructor 		*
 * constr_name. 							*
 *									*
 * codomain and domain are the codomain and domain types of the   	*
 * constructor, so they are the domain and codomain types, resp.  	*
 * of the destructor.						  	*
 ************************************************************************/

PRIVATE void
declare_destructor_fun(char *destr_name, 
		       TYPE *domain, ROLE *domainRole,
		       TYPE *codomain, ROLE *codomainRole,
		       int line, int prim, int arg,
		       MODE_TYPE *mode, Boolean irregular, Boolean defer,
		       Boolean build_declaration,
		       ISSUE_DCL_FUN_TYPE issue_dcl_fun)
{
  TYPE *destr_type;
  EXPR *this_id_expr, *destructor, *destr_fun, *def;

  /*--------------------------------------------------------*
   * Check that the primitive can be inverted. If it can't, *
   * we have big trouble.  The primitive should always be   *
   * invertible.					    *
   *--------------------------------------------------------*/

  if(invert_prim_val(prim) == 0) {
    die(155);
  }

  /*---------------------------------------------------------------*
   * Copy the types, to insulate other declarations from this one. *
   *---------------------------------------------------------------*/

  bump_type(destr_type = function_t(codomain, domain));
  SET_TYPE(destr_type, copy_type(destr_type, 0));

  /*--------------------------------------------------------*
   * Build the identifier to be defined and the destructor. *
   *--------------------------------------------------------*/

  bump_expr(this_id_expr = new_expr1t(GLOBAL_ID_E, NULL_E, destr_type, line));
  bump_role(this_id_expr->role = fun_role(codomainRole, domainRole));
  bump_expr(destructor = new_expr1t(SPECIAL_E, NULL_E, destr_type, line));
  this_id_expr->STR       =
    destructor->STR       = destr_name;
  if(build_declaration || prim != PRIM_EXC_WRAP) {
    this_id_expr->PRIMITIVE =
      destructor->PRIMITIVE = invert_prim_val(prim);
    this_id_expr->SCOPE     = 
      destructor->SCOPE     = arg;
  }
  else {
    this_id_expr->PRIMITIVE = 0;
    this_id_expr->SCOPE     = 0;
  }

  /*-------------------------------------------------*
   * If called for, build and issue the declaration. *
   *-------------------------------------------------*/

  if(build_declaration) {
    if(prim == PRIM_CAST && !irregular) destr_fun = destructor;
    else {
      EXPR* pv  = new_pat_var(HIDE_CHAR_STR "x", line);
      destr_fun = new_expr2(FUNCTION_E, pv, 
			    apply_expr(destructor, pv->E1, line), 
			    line);
    }
    bump_expr(def = new_expr2(LET_E, this_id_expr, destr_fun, line));

    issue_dcl_fun(def, LET_E, mode);
    drop_expr(def);
  }

  /*------------------------------------------------------------*
   * If we do not need to build the declaration, the just issue *
   * a definition.						*
   *------------------------------------------------------------*/

  else {
    quick_issue_define_p(this_id_expr, destr_name, mode, line);
  }

  /*------------------------------------------------------------*
   * If the destructor unwraps, then it might fail. Attach a 	*
   * fail role to it. 						*
   *------------------------------------------------------------*/

  if(prim == PRIM_QWRAP || prim == PRIM_DWRAP) {
    (defer ? defer_attach_property : attach_property)
      (pot_fail, destructor->STR, destr_type);
  }

  drop_expr(this_id_expr);
  drop_expr(destructor);
  drop_type(destr_type);
}


/************************************************************************
 *			DECLARE_CONSTRUCTOR_PATFUN			*
 ************************************************************************
 * This is a tool for declare_constructor_p.  See that function for     *
 * details of parameters.						*
 *									*
 * Declare constr_name as a pattern function.				*
 *									*
 * If use_prims is false and the primitive is PRIM_EXC_UNWRAP then do   *
 * not use the primitive information.  Instead, use the destructor      *
 * function by name.							*
 ************************************************************************/

PRIVATE void
declare_constructor_patfun(char *constr_name, char *destr_name, 
			   TYPE *domain, TYPE *codomain, ROLE *constr_role,
			   int destr_prim, int destr_arg, int line, 
			   Boolean use_prims, Boolean defer)
{
  EXPR *destructor;
  TYPE *domain_cpy, *codomain_cpy, *constr_type, *destr_type;

  Boolean old_local_error_occurred = local_error_occurred;

  /*----------------------------------------------------------------*
   * We would like to do this even if there is an error, to prevent *
   * lots more errors later.					    *
   *----------------------------------------------------------------*/

  local_error_occurred = FALSE;

  /*-----------------*
   * Copy the types. *
   *-----------------*/

  {HASH2_TABLE* ty_b = NULL;
   bump_type(domain_cpy = copy_type1(domain, &ty_b, 0));
   bump_type(codomain_cpy = copy_type1(codomain, &ty_b, 0));
   bump_type(destr_type = function_t(codomain_cpy, domain_cpy));
   bump_type(constr_type = function_t(domain_cpy, codomain_cpy));
   scan_and_clear_hash2(&ty_b, drop_hash_type);
  }

  /*----------------------*
   * Make the destructor. *
   *----------------------*/

  if(use_prims || destr_prim != PRIM_EXC_UNWRAP) {
    bump_expr(destructor = new_expr1t(SPECIAL_E, NULL_E, destr_type, line));
    destructor->STR       = destr_name;
    destructor->PRIMITIVE = destr_prim;
    destructor->SCOPE     = destr_arg;
  }
  else {
    bump_expr(destructor = typed_global_id_expr(destr_name, destr_type, line));
  }

 /*-------------------------------*
  * Declare the pattern function. *
  *-------------------------------*/

  auto_pat_fun_p(constr_name, constr_type, constr_role, 
		 destructor, defer);

  if(!local_error_occurred) {
    local_error_occurred = old_local_error_occurred;
  }

  drop_expr(destructor);
  drop_type(domain_cpy);
  drop_type(codomain_cpy);
  drop_type(constr_type);
  drop_type(destr_type);
}


/************************************************************************
 *			DECLARE_TESTER_FUN				*
 ************************************************************************
 * This is a tool for declare_constructor_p.  See that function for     *
 * details of parameters.						*
 *									*
 * Declare the tester constr_name?		 			*
 * Also declare constr_name as a pattern constant if domain_type is (). *
 * These are done together because the pattern constant is defined in   *
 * terms of the tester.				 			*
 *									*
 * mode		is the mode of the declaration.				*
 *		It is a safe pointer: it will not be kept		*
 *		longer than the lifetime of this function call.		*
 ************************************************************************/

PRIVATE void
declare_tester_fun(char *constr_name, TYPE *domain, TYPE *codomain, 
		   int prim, int arg, MODE_TYPE *mode, int line, 
		   Boolean build_declaration, Boolean defer,
		   ISSUE_DCL_FUN_TYPE issue_dcl_fun)
{
  TYPE *codomain_cpy;
  bump_type(codomain_cpy = copy_type(codomain, 0));

  /*---------------------------------------------*
   * Only declare the tester for a wrapped type. *
   *---------------------------------------------*/

  if(prim == PRIM_QWRAP || prim == PRIM_DWRAP || prim == PRIM_EXC_WRAP) {
    TYPE *tester_type;
    EXPR *destructor, *this_id_expr, *def;

    char* testr_name  = tester_name(constr_name);

    /*--------------------------------------------------*
     * Get the identifier being defined.   Mark it as 	*
     * primitive if appropriate.  If the primitive is   *
     * PRIM_EXC_WRAP and we are not building the        *
     * definition, then do not install the prim info,   *
     * since arg is not a valid label.			*
     *--------------------------------------------------*/

    bump_type(tester_type = function_t(codomain_cpy, boolean_type));
    this_id_expr      = new_expr1t(GLOBAL_ID_E, NULL_E, tester_type, line);
    this_id_expr->STR = testr_name;

    {register int primitive, argument;
     if(prim == PRIM_EXC_WRAP) {
       if(build_declaration) {
	 primitive = PRIM_EXC_TEST;
	 argument  = arg;
       }
       else {
	 primitive = argument = 0;
       }
     }
     else {
       primitive = PRIM_QTEST;
       argument  = arg;
     }
     this_id_expr->PRIMITIVE = primitive;
     this_id_expr->SCOPE     = argument;
    }
  
    /*----------------------------------------------------------*
     * If asked to build this declaration, then build it and    *
     * issue it.						*
     *----------------------------------------------------------*/

    if(build_declaration) {
      destructor            = new_expr1t(SPECIAL_E, NULL, tester_type, line);
      destructor->STR       = testr_name;
      destructor->PRIMITIVE = this_id_expr->PRIMITIVE;
      destructor->SCOPE     = arg;
      bump_expr(def = new_expr2(LET_E, this_id_expr, destructor, line));

      issue_dcl_fun(def, LET_E, mode);

      drop_expr(def);
    }

    /*-----------------------------------------------------------------*
     * If not asked to build the declaration, then just define the id. *
     *-----------------------------------------------------------------*/

    else {
      quick_issue_define_p(this_id_expr, testr_name, mode, line);
    }

    /*-------------------------------*
     * Declare the pattern constant. *
     *-------------------------------*/

    if(is_hermit_type(domain)) {
      auto_pat_const_p(constr_name, codomain_cpy, testr_name, 
		       tester_type, defer);
    }

    drop_type(tester_type);
  }

  /*----------------------------------------------------------------*
   * For a cast from the hermit, just declare the pattern constant. *
   * This only occurs when we have				    *
   *								    *
   *    Species S = name.					    *
   *----------------------------------------------------------------*/

  else if(prim == PRIM_CAST && is_hermit_type(domain)) {
    auto_pat_const_p(constr_name, codomain_cpy, NULL, NULL, defer);
  }

  drop_type(codomain_cpy);
}


/************************************************************************
 *			DECLARE_CONSTRUCTOR_HELP_P			*
 ************************************************************************
 * This is a tool for declare_constructor_p.  See that function for     *
 * a description.  This function does the work of declare_constructor_p.*
 *									*
 * When withs is true, constr_domain and constr_codomain		*
 * are set to the restricted domain and codomain.  Otherwise,		*
 * they are the same as domain and codomain.				*
 *									*
 * mode		is the mode of the declaration.				*
 *		It is a safe pointer: it will not be kept		*
 *		longer than the lifetime of this function call.		*
 ************************************************************************/

PRIVATE void 
declare_constructor_help_p(char *constr_name, char *true_constr_name,
			   TYPE *domain, ROLE *domainRole,
			   TYPE *codomain, ROLE *codomainRole,
			   int prim, int arg,
			   MODE_TYPE *mode, int irregular,
			   TYPE *constr_domain, TYPE *constr_codomain,
			   Boolean withs, Boolean trap, Boolean defer,
			   Boolean build_declaration) 

{
  MODE_TYPE *special_mode;
  int   line                 = current_line_number;
  char* new_constr_name      = new_name(constr_name, TRUE);
  char* new_true_constr_name = new_name(true_constr_name, TRUE);
  char* destr_name           = destructor_name(true_constr_name);

  ISSUE_DCL_FUN_TYPE issue_dcl_fun = 
             (defer ? defer_issue_dcl_p : issue_dcl_p);

  /*-------------------------------------------------------------*
   * If there was a previous error in this declaration, don't do *
   * any more.							 *
   *-------------------------------------------------------------*/

  if(local_error_occurred) return;

  bump_type(domain);
  bump_type(codomain);
  bump_type(constr_domain);
  bump_type(constr_codomain);

# ifdef DEBUG
    if(trace_defs) {
      trace_t(37, constr_name, prim, arg);
      print_two_types(domain, codomain);
    }
# endif

  /*------------------------------------------------------------*
   * When withs is false, ignore the input constr_domain and 	*
   * constr_codomain, and just use domain and codomain.		*
   *------------------------------------------------------------*/

  if(!withs) {
    SET_TYPE(constr_domain, domain);
    SET_TYPE(constr_codomain, codomain);
  }

  /*----------------------------------------------------------------------*
   * When the domain is the hermit, declare the constructor as a constant *
   * of the codomain type, and skip declaring the destructor and the	  *
   * pattern function.							  *
   *----------------------------------------------------------------------*/

  if(is_hermit_type(domain)) {
    declare_constructor_for_hermit(new_constr_name, codomain, trap, line,
				   prim, arg, mode, build_declaration,
				   issue_dcl_fun);
  }

  /*--------------------------------------------------------------------*
   * When the domain is not the hermit, declare the constructor as a	*
   * function, and declare it as a pattern function, and declare	*
   * the destructor function. Bring casts from idf. 			*
   *--------------------------------------------------------------------*/

  else {
    ROLE *constr_role;

    if(domainRole != NULL || codomainRole != NULL) {
      bump_role(constr_role = fun_role(domainRole, codomainRole));
    }
    else constr_role = NULL;

    if(irregular) {
      special_mode = copy_mode(mode);   /* ref cnt is 1. */
      add_mode(special_mode, IRREGULAR_MODE);
    }
    else bump_mode(special_mode = mode);
    declare_constructor_fun(new_constr_name, constr_role, domain, codomain,
			    constr_domain, constr_codomain, withs, 
			    trap, prim, arg, mode, special_mode,
			    line, build_declaration,
			    issue_dcl_fun);

    declare_destructor_fun(destr_name, domain, domainRole, codomain,
			   codomainRole, line, prim, arg,
			   special_mode, irregular, defer,
			   build_declaration, issue_dcl_fun);

    declare_constructor_patfun(new_constr_name, destr_name, domain, codomain, 
			       constr_role, invert_prim_val(prim), arg, 
			       line, build_declaration, defer);    

    drop_mode(special_mode);
    drop_role(constr_role);
  }

  /*--------------------------------------------------------------------*
   * For a coproduct type or family, declare the tester.  Do not 	*
   * declare the tester for the forgetful functions that end on ??.  	*
   * If the domain type is (), also declare the constructor as a 	*
   * pattern constant, using the tester. 				*
   *--------------------------------------------------------------------*/

  if(!withs) {
    declare_tester_fun(new_true_constr_name, domain, codomain, prim, arg, 
		       mode, line, build_declaration, defer, 
		       issue_dcl_fun);
  }

  drop_type(domain);
  drop_type(codomain);
  drop_type(constr_domain);
  drop_type(constr_codomain);

# ifdef DEBUG
    if(trace_defs) trace_t(38, constr_name);
# endif

}  


/****************************************************************
 * 			UNIFY_WITHS				*
 ****************************************************************
 * Lists withs is a list of the form				*
 *								*
 *    [a1,a2,b1,b2,...]						*
 *								*
 * Unify a1 with a2, b1 with b2, etc.  Print an error message	*
 * if any of the unifications fail.				*
 *								*
 * Set *binding_list_mark to the position in the unification	*
 * binding list just before all bindings done by unify_withs,	*
 * so that those bindings can be undone later.			*
 *								*
 * This function is called in by declare_constructor_p to force *
 * withs to be done when declaring constructors.  For example,  *
 * if a part of a species declaration is			*
 *								*
 *    Species S(`a) = 						*
 *        | constr(`b,`c) with `a = (`b,`c)			*
 *    %Species							*
 *								*
 * then list withs will contain pair (`a, (`b,`c)) in 		*
 * consecutive locations, and unify_withs will unify them.	*
 * This will cause the type of constr to be (`b,`c) -> S(`b,`c).*
 * Without unify_withs, the type of constr would be		*
 * (`b,`c) -> S(`a).						*
 ****************************************************************/

PRIVATE void unify_withs(LIST *withs, LIST **binding_list_mark)
{
  LIST *q;

  if(withs != NIL) {
    bump_list(*binding_list_mark = finger_new_binding_list());
    for(q = withs; q != NIL; q = q->tail->tail) {
      if(!unify_u(&(q->head.type), &(q->tail->head.type), TRUE)) {
	semantic_error(CANNOT_UNIFY_ERR, 0);
	break;
      }
    }
  }
}     


/************************************************************************
 *			DECLARE_CONSTRUCTOR_P				*
 ************************************************************************
 * Declare function constr_name of type domain -> codomain (and role 	*
 * domain_role -> codomain_role) as a function implemented by 		*
 * primitive prim, with argument arg.  					*
 *									*
 * If domain is (), then declare the constructor			*
 * to have type codomain instead of () -> codomain.			*
 *									*
 * Also declare:							* 
 *									*
 *   A destructor, which is the inverse of the destructor.  The name of	*
 *   the destructor is "un" followed by true_constr_name.		*
 *									*
 *   A pattern function called constr_name, if domain != (), and a 	*
 *   pattern constant called const_name if domain = ().  The pattern	*
 *   function inverts the constructor.					*
 *									*
 *   A tester called true_constr_name followed by "?". in the case	*
 *   where prim = PRIM_QWRAP or prim = PRIM_EXC_WRAP.			*
 *									*
 * Additional parameters are as follows.				*
 * ----									*
 * mode is the mode for the definitions. It is a safe pointer: it does  *
 * not live longer than this function call. 				*
 * ----									*
 * irregular is true if the destructor is irregular. 			*
 * ----									*
 * trap is true if the constructor is a trapped exception or exception  *
 * constructor.								*
 * ----									*
 * defer is true if declarations should be put in the defer_dcls chain, *
 * to be done when an extend-declaration is finished.  If defer is	*
 * false, declarations are done immediately.				*
 * ----									*
 * build_declaration is true if the full declaration should be built.   *
 * If build_declaration is false, then the identifiers should be defined*
 * for import, but there is no need to build the whole thing.           *
 * ----									*
 * withs is a list of unifications for a definition with a 'with' 	*
 * clause.  If this list is non-null, then there are two different      *
 * kinds of definitions to be made.  The main ones are restricted by 	*
 * the unifications called for in the withs list.  An additional 	*
 * destructor and pattern function are defined that can be used with	*
 * types that do not respect those unifications.  (They fail if the 	*
 * type is bad.)  For example, if there is a part			*
 *	  								*
 *    Species F(`a) =							*
 *      ...								*
 *      con(`b,`c)  with `a = (`b,`c)					*
 *      ...								*
 *									*
 * then the main constructor con has type (`b,`c) -> F(`b,`c), but the  *
 * second ("forgetful") construct con?? has type (`b,`c) -> F(`a).	*
 * ----									*
 * XREF: 								*
 *   Called by dclclass.c to declare constructors, destructors at	*
 *   species declarations.						*
 *									*
 *   Called by somedcls.c to declare an exception constructor and	*
 *   destructor								*
 ************************************************************************/

void declare_constructor_p(char *constr_name, char *true_constr_name,
			   TYPE *domain,   ROLE *domainRole,
			   TYPE *codomain, ROLE *codomainRole,
			   int prim, int arg,
			   MODE_TYPE *mode, int irregular,
			   LIST *withs, Boolean trap, Boolean defer,
			   Boolean build_declaration)
{
  TYPE *constr_codomain, *constr_domain;
  LIST *binding_list_mark;
  HASH2_TABLE* ty_b = NULL;

  bump_type(domain);
  bump_type(codomain);

  /*------------------------------------------------------------*
   * Use different copies for the constructor if withs != NIL,  *
   * since the constructor has extra bindings in its type then. *
   *------------------------------------------------------------*/

  if(withs != NIL) {
    unify_withs(withs, &binding_list_mark);
    constr_codomain = copy_type1(codomain, &ty_b, 0);
    constr_domain   = copy_type1(domain, &ty_b, 0);
    undo_bindings_u(binding_list_mark);
  }
  else {
    constr_codomain = codomain;
    constr_domain   = domain;
  }
  bump_type(constr_codomain);
  bump_type(constr_domain);
  scan_and_clear_hash2(&ty_b, drop_hash_type);

  /*--------------------------------*
   * Basic constructor, destructor. *
   *--------------------------------*/

  declare_constructor_help_p(constr_name, true_constr_name,
			     constr_domain, domainRole, 
			     constr_codomain, codomainRole, prim, arg, 
			     mode, irregular, constr_domain,
			     constr_codomain, FALSE, trap, defer,
			     build_declaration);

  /*-------------------------------------------------------*
   * Forgetful constructor, destructor, when withs != NIL. *
   *-------------------------------------------------------*/

  if(withs != NIL) {
    declare_constructor_help_p(forgetful_tester_name(constr_name), 
			       forgetful_tester_name(true_constr_name),
			       domain, domainRole,
			       codomain, codomainRole, prim, arg,
			       mode, irregular, constr_domain,
			       constr_codomain, TRUE, 0, defer,
			       build_declaration);
  }

  drop_type(constr_domain);
  drop_type(constr_codomain);
  drop_type(domain);
  drop_type(codomain);
}


/************************************************************************
 *			GET_TYPED_VARS					*
 ************************************************************************
 * Return a list of pattern variables, one for each type in list	*
 * types.  The variables are on line 'line'.				*
 *									*
 * List roles is parallel to types, and gives the roles to attach	*
 * to the types.							*
 ************************************************************************/

PRIVATE EXPR_LIST*
get_typed_pat_vars(TYPE_LIST *types, ROLE_LIST *roles, int line)
{
  if(types == NIL) return NIL;
  else {
    EXPR_LIST* rest = get_typed_pat_vars(types->tail, roles->tail, line);
    EXPR* id = fresh_pat_var(HIDE_CHAR_STR "x", line);
    bump_role(id->role = roles->head.role);
    return expr_cons(id, rest);
  }
}


/************************************************************************
 *			GET_NONPAT_VARS					*
 ************************************************************************
 * If pvs is list [?v1,?v2,?v3], then return list [v1,v2,v3].		*
 ************************************************************************/

PRIVATE EXPR_LIST*
get_nonpat_vars(EXPR_LIST *pvs, int line)
{
  if(pvs == NIL) return NIL;
  else {
    return expr_cons(pvs->head.expr->E1,
		     get_nonpat_vars(pvs->tail, line));
  }
}


/************************************************************************
 *			GET_CURRIED_CONSTR_FUN				*
 ************************************************************************
 * If pvs is [?v1,?v2,?v2], return expression				*
 * (?v1 => (?v2 => (?v3 => e))).					*
 ************************************************************************/

PRIVATE EXPR* 
get_curried_constr_fun(EXPR_LIST *pvs, EXPR *e, int line)
{
  if(pvs == NIL) return e;
  else {
    return new_expr2(FUNCTION_E, pvs->head.expr,
		     get_curried_constr_fun(pvs->tail, e, line),
		     line);
  }
}


/************************************************************************
 *		      MAKE_TRUE_CONSTR_CALL				*
 ************************************************************************
 * If pvs is [?v1,?v2,?v3], return expression				*
 * f ?v1 ?v2 ?v3, where f is a pattern function.  Also set *patfun to   *
 * pattern function f.  *patfun is not reference counted.		*
 ************************************************************************/

PRIVATE EXPR* 
make_true_constr_call(char *f, EXPR_LIST *pvs, int line, EXPR **patfun)
{
  EXPR *result;
  EXPR_LIST *p;

  *patfun = result = new_expr1(PAT_FUN_E, NULL_E, line);
  result->STR = f;
  for(p = pvs; p != NIL; p = p->tail) {
    result = apply_expr(result, p->head.expr, line);
  }
  return result;
}


/************************************************************************
 *			DECLARE_CURRIED_CONSTRUCTOR_HELP_P		*
 ************************************************************************
 * This is a tool for declare_curried_constructor_p. Declare		*
 * true_constr_name to be a curried constructor defined from uncurried	*
 * constructor constr_name: rep_type -> target_type.			*
 ************************************************************************/

PRIVATE void 
declare_curried_constructor_help_p
   (char *constr_name, TYPE *rep_type, TYPE *target_type,
    char *true_constr_name,
    TYPE_LIST *true_constr_types, ROLE_LIST *true_constr_roles,
    MODE_TYPE *mode, Boolean defer)
{

  /*-------------------------------------------------------------*
   * If there was a previous error in this declaration, don't do *
   * any more.							 *
   *-------------------------------------------------------------*/

  if(local_error_occurred) return;

  else {
    EXPR_LIST *vars, *pvs;

    int   line                 = current_line_number;
    char* new_constr_name      = new_name(constr_name, TRUE);
    char* new_true_constr_name = new_name(true_constr_name, TRUE);
    TYPE* constr_type          = function_t(rep_type, target_type);

    ISSUE_DCL_FUN_TYPE issue_dcl_fun = 
             (defer ? defer_issue_dcl_p : issue_dcl_p);

    bump_type(constr_type);

#   ifdef DEBUG
      if(trace_defs) {
        trace_t(578, nonnull(new_true_constr_name), nonnull(new_constr_name));
        trace_ty(constr_type);
	tracenl();
      }
#   endif

    /*---------------------------------------------------------*
     * Get a variable for each type in list true_constr_types. *
     * We call it [v1,...,vn] below - it is in variable vars.  *
     * Also get a parallel list pvs of pattern variables       *
     * [?v1,...,?vn].					       *
     *---------------------------------------------------------*/

    bump_list(pvs = get_typed_pat_vars(true_constr_types, 
				       true_constr_roles, line));
    bump_list(vars = get_nonpat_vars(pvs, line));

    /*----------------------------------------------------------*
     * Build expression fun =					*
     *								*
     *  (v1 => (... (vn => new_constr_name(v1,...,vn)))). 	*
     *								*
     * and definition						*
     *								*
     *   Let new_true_constr_name = fun.			*
     *								*
     * Then issue the declaration.				*
     *----------------------------------------------------------*/

    {EXPR* constr_def;
     EXPR* true_constr_id = id_expr(new_true_constr_name, line);
     EXPR* tuple          = list_to_expr(vars, 0);
     EXPR* constr_id      = typed_id_expr(new_constr_name, constr_type, line);
     EXPR* call_constr    = apply_expr(constr_id, tuple, line);
     EXPR* fun = get_curried_constr_fun(pvs, call_constr, line);
     bump_expr(constr_def = new_expr2(LET_E, true_constr_id, fun, line));
     issue_dcl_fun(constr_def, LET_E, mode);
     drop_expr(constr_def);
    }

    /*----------------------------------------------------------*
     * Build definition						*
     *								*
     *  Pattern new_true_constr_name ?v1 ?v2 ?v2 =>		*
     *      Match new_constr_name(?x1,?x2,?x3) = target.	*
     *  %Pattern						*
     *								*
     * and issue it.						*
     *----------------------------------------------------------*/

    {EXPR* patfun, *pat_dcl;
     EXPR* true_constr_call = 
       make_true_constr_call(new_true_constr_name, pvs, line, &patfun);
     EXPR* tuple       = list_to_expr(pvs, 0);
     EXPR* constr_id   = typed_id_expr(new_constr_name, constr_type, line);
     EXPR* constr_call = apply_expr(constr_id, tuple, line);
     EXPR* target_expr = typed_target(NULL_T, patfun, line);
     EXPR* match       = new_expr2(MATCH_E, constr_call, target_expr, line);
     EXPR* rule        = new_expr1(PAT_RULE_E, NULL_E, line);

     bump_expr(rule->E2  = true_constr_call);
     bump_expr(rule->E3  = match);
     rule->PAT_RULE_MODE = 0;
     rule->ETAGNAME      = true_constr_name;
     bump_expr(pat_dcl   = new_expr2(PAT_DCL_E, patfun, rule, line));
     issue_dcl_fun(pat_dcl, PAT_DCL_E, mode);
     drop_expr(pat_dcl);
    }

    drop_list(vars);
    drop_list(pvs);
    drop_type(constr_type);

#   ifdef DEBUG
      if(trace_defs) trace_t(38, constr_name);
#   endif

  }
}  


/************************************************************************
 *			GET_CURRY_TYPES					*
 ************************************************************************
 * If domain_types is [r,s,t] then set *constr_type to			*
 * r -> s -> t -> codomain_type.  Do a similar thing for		*
 * constr_role.								*
 *									*
 * Both constr_type and constr_role are reference counted.		*
 ************************************************************************/

PRIVATE void  
get_curry_types(TYPE_LIST *domain_types, ROLE_LIST *domain_roles, 
                TYPE *codomain_type, ROLE *codomain_role,
		TYPE **constr_type,  ROLE **constr_role)
{
  if(domain_types == NIL) {
    bump_type(*constr_type = codomain_type);
    bump_role(*constr_role = codomain_role);
  }
  else {
    TYPE *new_codomain_type;
    ROLE *new_codomain_role;
    get_curry_types(domain_types->tail, domain_roles->tail,
		    codomain_type, codomain_role,
		    &new_codomain_type, &new_codomain_role);
    bump_type(*constr_type = function_t(domain_types->head.type, 
				        new_codomain_type));
    bump_role(*constr_role = fun_role(domain_roles->head.role, 
					   new_codomain_role));
    drop_type(new_codomain_type);
    drop_role(new_codomain_role);
  }
}


/************************************************************************
 *			DECLARE_CURRIED_CONSTRUCTOR_P			*
 ************************************************************************
 * Declare function true_constr_name.  Its type is given by list 	*
 * domain_types and type codomain_type.  If domain_types is list 	*
 * [r,s,t], then function true_constr_name has type 			*
 * r -> s -> t -> codomain.						*
 *									*
 * The constructor is defined to be the same as running constr_name,    *
 * except that constr_name takes a single parameter that is a tuple.    *
 * If domain_types is [r,s,t], then the definition of true_constr_name 	*
 * is									*
 *									*
 *   true_constr_name = (x1 => (x2 => (x3 => constr_name(x1,x2,x3))))	*
 *									*
 * where constr_name has type rep_type -> codomain_type.		*
 *									*
 * List domain_roles is a list of roles that is parallel to list	*
 * domain_types, giving the role information for the types.  Parameter 	*
 * codomain_role is the role of type codomain.				*
 *									*
 * Also declare a pattern function called true_constr_name, of the same *
 * type as the constructor.  It is defined as follows when domain_types *
 * is [r,s,t].								*
 *									*
 *  Pattern true_constr_name ?x1 ?x2 ?x2 =>				*
 *      Match constr_name(x1,x2,x3) = target.				*
 *  %Pattern								*
 *									*
 * Additional parameters are as follows.				*
 * ----									*
 * mode is the mode for the definitions.  				*
 * ----									*
 * trap is true if the constructor is a trapped exception or exception  *
 * constructor.								*
 * ----									*
 * defer is true if declarations should be put in the defer_dcls chain, *
 * to be done when an extend-declaration is finished.  If defer is	*
 * false, declarations are done immediately.				*
 * ----									*
 * withs is a list of unifications for a definition with a 'with' 	*
 * clause.  								*
 * ----									*
 * XREF: 								*
 *   Called by dclclass.c to declare constructors, destructors at	*
 *   species declarations.						*
 *									*
 *   Called by somedcls.c to declare an exception constructor and	*
 *   destructor								*
 ************************************************************************/

void declare_curried_constructor_p
      (char *constr_name, char *true_constr_name,
       TYPE_LIST *domain_types,   
       ROLE_LIST *domain_roles,
       TYPE *rep_type, ROLE *rep_role,
       TYPE *codomain_type, ROLE *codomain_role,
       MODE_TYPE *mode, int is_irregular, LIST *withs, 
       Boolean defer)
{
  TYPE *true_constr_type;
  ROLE *true_constr_role;
  LIST *binding_list_mark;
  MODE_TYPE *newmode;

  /*---------------------------------*
   * Build the types from the lists. *
   *---------------------------------*/

  get_curry_types(domain_types, domain_roles, codomain_type, codomain_role,
		  &true_constr_type, &true_constr_role);
  
  /*--------------------------------------*
   * Basic constructor, pattern function. *
   *--------------------------------------*/

  if(is_irregular) {
    newmode = copy_mode(mode);         /* ref cnt is 1. */
    add_mode(newmode, IRREGULAR_MODE);
  }
  else bump_mode(newmode = mode);

  declare_curried_constructor_help_p(constr_name, rep_type,
				     codomain_type, true_constr_name,
				     domain_types, domain_roles,
				     newmode, defer);

  /*-------------------------------------------------------------*
   * Forgetful constructor, pattern function, when withs != NIL. *
   *-------------------------------------------------------------*/

  if(withs != NIL) {
    unify_withs(withs, &binding_list_mark);
    declare_curried_constructor_help_p(forgetful_tester_name(constr_name), 
				       rep_type, codomain_type,
				       forgetful_tester_name(true_constr_name),
				       domain_types, domain_roles,
				       newmode, defer);
    undo_bindings_u(binding_list_mark);
  }

  drop_mode(newmode);
  drop_type(true_constr_type);
  drop_role(true_constr_role);
}


/****************************************************************
 *			DECLARE_DOLLAR_P		        *
 ****************************************************************
 * Declare function $ for type t -> String.  L is the CUC-list 	*
 * that describes the rhs of the declaration of t.  It must be  *
 * nonempty. If type t is declared as				*
 *								*
 *  Species t = cons.						*
 *								*
 * then the declaration is					*
 *								*
 *  Let $(?x) = "cons".						*
 *								*
 * If type t is declared by					*
 *								*
 *  Species t = cons1 | cons2(R) | ....				*
 *								*
 * then the declaration of $ has the form			*
 *								*
 *  Let{default} $(?x) = 					*
 *    Try							*
 *      Let result = lazyDollar(x).				*
 *    then							*
 *      result							*
 *    else							*
 *      Choose first matching x					*
 *        case cons1 => "cons1"					*
 *        case cons2(?y) => showApply("cons2", y). 		*
 *        ...							*
 *        else => fail(noCaseX)					*
 *      %Choose							*
 *  %Let							*
 *								*
 * If type t is declared by					*
 *								*
 *  Species t = cons(R).					*
 *								*
 * then the choose expr will just have one case.  The 		*
 * declaration is always deferred, since it is only done in an 	*
 * extension.							*
 *								*
 * In the case of a curried constructor, such as		*
 *								*
 *  Species t = ... | cons A B C | ...				*
 *								*
 * the case is							*
 *								*
 *     case ;;cons(?y) => showCurriedApply 2 ("cons", y)	*
 *								*
 * where ;;cons is the constructor name cons with two copies of *
 * HIDE_CHAR in front of it, and the first parameter to		*
 * showCurriedApply is one less than the number of types that   *
 * follow the constructor.					*
 *								*
 * XREF: Called by dclclass.c to declare $ at a species 	*
 * declaration.							*
 ****************************************************************/

void declare_dollar_p(LIST *L, TYPE *t)
{
  Boolean singleton;
  LIST *p;
  EXPR *target_pat, *target_expr, *try_exp;
  EXPR *fun, *l_show, *show_apply, *show_apply_arg;
  char *this_cnstr, *this_real_cnstr;
  CLASS_UNION_CELL *cuc;

  int   line        = current_line_number;
  EXPR* choose_expr = NULL;
  EXPR* patfun      = NULL;
  EXPR* pat         = NULL;

  bump_list(L);
  bump_type(t);

  /*-------------------------------------------------------*
   * Singleton is true if the definition of t has the form *
   *   Species t = cons.				   *
   *-------------------------------------------------------*/

  singleton = (L->tail == NIL) && 
              L->head.cuc->tok == TYPE_ID_TOK && 
              is_hermit_type(L->head.cuc->CUC_TYPE);

  /*------------------------------------------------------------*
   * Build and issue the definition of $.  Start with the else	*
   * expression at the end of the choose expr if not singleton, *
   * and build backwards from there. 				*
   *------------------------------------------------------------*/

  if(!singleton) choose_expr = make_else_expr_p(FIRST_ATT, NO_CASE_EX, line);

  /*--------------------------------------*
   * target_pat is ?x.  target_expr is x. *
   *--------------------------------------*/

  target_pat  = new_pat_var(HIDE_CHAR_STR "x", line);
  target_expr = target_pat->E1;

  /*------------------------------------------------------------*
   * Build the body of the choose expression, or the		*
   * else part of the try if there is only one part. 		*
   *------------------------------------------------------------*/

  for(p = L; p != NULL; p = p->tail) {
    Boolean constant_case, is_curried;
    cuc = p->head.cuc;

    /*------------------------------------------------------------------*
     * constant_case is true if this case is a constant (such as cons1	*
     *               in the example above).				*
     *									*
     * is_curried    is true if this is a curried constructor case.	*
     *									*
     * this_cnstr    is the name of the constructor for this case.	*
     *------------------------------------------------------------------*/

    constant_case = cuc->tok == TYPE_ID_TOK && is_hermit_type(cuc->CUC_TYPE);
    is_curried    = cuc->tok == TYPE_LIST_TOK;
    this_cnstr = this_real_cnstr = cuc->name;
    if(is_curried) {
      this_cnstr = attach_prefix_to_id(HIDE_CHAR_STR HIDE_CHAR_STR, 
				       this_cnstr, 0);
    }

    /*------------------------------------------------------------------*
     * fun is the name of the constructor, as a string constant.	*
     *									*
     * pat_fun is the pattern function that selects this case. 		*
     *									*
     * If cuc->withs != NIL, need to use the forgetful pattern		*
     * function, ending on ??. 						*
     *------------------------------------------------------------------*/

    fun = const_expr(display_name(this_real_cnstr), STRING_CONST, 
		     string_type, line);
    if(!singleton) {
      char* patfun_name = (cuc->withs == NIL) 
	                    ? this_cnstr 
			    : forgetful_tester_name(this_cnstr);
      patfun = id_expr(patfun_name, line);
    }

    /*------------------------------------------------------------------*
     * Get the pattern and value to use in the next case to be added	*
     * to the choose expression.  If singleton is true, then just	*
     * define the value l_show.						*
     *------------------------------------------------------------------*/

    if(constant_case) {
      if(!singleton) {
	pat = patfun;
	bump_type(patfun->ty = t);
      }
      l_show = fun;
    }
    else {
      TYPE *rep_type;
      EXPR *pvar;

      /*--------------------------------------------------------*
       * show_apply is the function that shows this constructor	*
       * applied to its argument(s).  It is showApply for a 	*
       * non-curried constructor, and is showCurriedApply(k-1) 	*
       * for a curried constructor, where k is the number of 	*
       * parameters.						*
       *							*
       * rep_type is the representation type.			*
       *--------------------------------------------------------*/

      rep_type = cuc_rep_type(cuc);
      if(is_curried) {
        EXPR *num, *showcurried;
        TYPE *showcurried_type;
        LONG k;
        char buf[25];

        showcurried_type = function_t(natural_type, 
				      copy_type(show_apply_type, 0));
        showcurried = typed_global_sym_expr(SHOWCURRIEDAPPLY_ID, 
					    showcurried_type, line);
        k = list_length(cuc->CUC_TYPES);
        sprintf(buf, "%ld", k-1);
        num = const_expr(string_const_tb0(buf), NAT_CONST, natural_type, line);
        show_apply = apply_expr(showcurried, num, line);
      }
      else {
        show_apply = typed_global_sym_expr(SHOWAPPLY_ID,
					   copy_type(show_apply_type,0), 
					   line);
      }
      bump_type(patfun->ty = function_t(rep_type,t));

      bump_expr(pvar = new_pat_var(HIDE_CHAR_STR "y", line));
      show_apply_arg = new_expr2(PAIR_E, fun, pvar->E1, line);
      l_show         = apply_expr(show_apply, show_apply_arg, line);
      if(!singleton) {
	pat = apply_expr(patfun, pvar, line);
      }
      drop_expr(pvar);
    }

    /*--------------------------------------------------*
     * Add a case to the front of the choose expr body.	*
     *--------------------------------------------------*/

    if(!singleton) {
      EXPR* match_expr  = new_expr2(MATCH_E, pat, target_expr, line);
      choose_expr = try_expr(match_expr, l_show, choose_expr, 
			     TRY_F, line);
    }
    else {
      choose_expr = l_show;
    }
  }

  /*---------------------------*
   * Build the try expression. *
   *---------------------------*/

  {EXPR* result_id = id_expr("result", line);
   EXPR* lazyDollar_id = global_sym_expr(LAZYDOLLAR_ID, line);
   EXPR* let = new_expr2(LET_E, result_id, 
			 apply_expr(lazyDollar_id, target_expr, line),
			 line);
   try_exp = try_expr(let, result_id, choose_expr, TRY_F, line);
  }

  /*----------------------------------*
   * Build and issue the declaration. *
   *----------------------------------*/

  {EXPR *heading, *body, *def;
   MODE_TYPE *mode;

   heading = id_expr(std_id[DOLLAR_SYM], line);
   bump_type(heading->ty = function_t(copy_type(t,1), string_type));
   body = new_expr2(FUNCTION_E, target_pat, try_exp, line);
   bump_expr(def = new_expr2(LET_E, heading, body, line));
   mode = simple_mode(DEFAULT_MODE);
   defer_issue_dcl_p(def, LET_E, mode);
   drop_mode(mode);
   drop_expr(def);
  }

  /*---------------------------------------------------------------*
   * Possibly pad the definition. This is needed for a restrictive *
   * family definition.						   *
   *---------------------------------------------------------------*/

  pad_sym_def(DOLLAR_SYM, t);

  /*-----------------------*
   * Drop refs and return. *
   *-----------------------*/

  drop_list(L);
  drop_type(t);
}


/************************************************************************
 *			DECLARE_PULL_P			        	*
 ************************************************************************
 * Declare function pull for type String -> (t,String).  		*
 *									*
 * Also add this pull function to pullWrappedBox.			*
 *									*
 * L is the CUC-list that describes the rhs of the declaration  	*
 * of t.  It must be nonempty.						*
 *									*
 * If type t is declared by						*
 *									*
 *  Species t = cons1 | cons2(R) | ....					*
 *									*
 * then the declaration of pull has the form				*
 *									*
 *  Let{default} pull(?s) = 						*
 *    Let ss = skipWS(s).						*
 *    Choose first							*
 *									*
 *      case nil? ss => fail listExhaustedX				*
 *									*
 *      case do Let rest = ss _lminus_ "cons1". 			*
 *                => (cons1, rest) 					*
 *									*
 *      case do Match (?y, ?rest) = pullHelp(ss, "cons2", pull).	*
 *                => (cons2(y), rest)					*
 *		   							*
 *          (where pull has type String -> (R, String))			*
 *									*
 *        ...								*
 *      else => fail conversionX					*
 *    %Choose								*
 *  %Let								*
 *									*
 * In the case of a curried constructor					*
 *									*
 *   Species S = ... | cons A B C | ...					*
 *									*
 * the case is								*
 *									*
 *        case do Match (?y, ?rest) = pullCurriedHelp 2 (ss, "cons2", pull).*
 *                => (;;cons(y), rest)					*
 *									*
 *          (where pull has type String -> ((A,B,C), String) and 2 is   *
 *           one less than the number of types after the constructor.)	*
 *									*
 * XREF: Called by dclclass.c to declare pull at a species 		*
 * declaration.								*
 ************************************************************************/

void declare_pull_p(LIST *L, TYPE *t)
{
  LIST *p;
  EXPR *choose_expr, *ss, *fun_body, *target_pat, *target_expr;

  int line = current_line_number;

  bump_list(L);
  bump_type(t);

  /*------------------------------------------------------------*
   * Build the definition of pull.  Start with the else	        *
   * expression at the end of the choose expr			*
   * and build backwards from there. 				*
   *------------------------------------------------------------*/

  choose_expr = make_else_expr_p(FIRST_ATT, CONVERSION_EX, line);

  /*------------------------------------*
   * target_pat is ?s.  		*
   * target_expr is identifier s. 	*
   * ss is identifier ss		*
   *------------------------------------*/

  target_pat  = new_pat_var(HIDE_CHAR_STR "s", line);
  target_expr = target_pat->E1;
  ss          = id_expr(HIDE_CHAR_STR "ss", line);

  /*--------------------------------------------*
   * Build the body of the choose expression,	*
   * excluding the case that tests nil? ss.	*
   *--------------------------------------------*/

  for(p = L; p != NULL; p = p->tail) {
    Boolean constant_case, is_curried;
    char *this_cnstr, *this_real_cnstr;
    EXPR *case_test, *case_val, *cnstr_as_str, *this_cnstr_id;

    CLASS_UNION_CELL* cuc = p->head.cuc;

    /*------------------------------------------------------------------*
     * constant_case is true if this case is a constant (such as cons1	*
     *               in the example above).				*
     * is_curried    is true if this is a curried constructor case.	*
     * this_cnstr    is the name of the constructor for this case.	*
     * this_cnstr_id is this_cnstr, as an expression.			*
     * cnstr_as_str  is the name of the constructor, as a string	*
     *		     constant.						*
     *------------------------------------------------------------------*/

    constant_case = cuc->tok == TYPE_ID_TOK && is_hermit_type(cuc->CUC_TYPE);
    is_curried    = cuc->tok == TYPE_LIST_TOK;
    this_cnstr = this_real_cnstr = cuc->name;
    if(is_curried) {
      this_cnstr = attach_prefix_to_id(HIDE_CHAR_STR HIDE_CHAR_STR, 
				       this_cnstr, 0);
    }
    this_cnstr_id = id_expr(this_cnstr, line);
    cnstr_as_str  = const_expr(display_name(this_real_cnstr), STRING_CONST, 
			       string_type, line);

    /*--------------------------------------------------*
     * Build a case and add it				*
     * to the front of the choose expr body.		*
     *--------------------------------------------------*/

    if(constant_case) {
      EXPR *lminus, *pr, *rhs, *rest;
      bump_type(this_cnstr_id->ty = t);
      lminus  = global_sym_expr(LMINUS_ID, line);
      pr        = new_expr2(PAIR_E, ss, cnstr_as_str, line);
      rhs       = apply_expr(lminus, pr, line);
      rest      = id_expr(HIDE_CHAR_STR "rest", line);
      case_test = new_expr2(LET_E, rest, rhs, line);
      case_val  = new_expr2(PAIR_E, this_cnstr_id, rest, line);
    }
    else {
      EXPR *y_pat, *y_id, *rest_pat, *rest_id;
      EXPR *pull, *pat, *pullHelp, *pullHelpArg;
      TYPE *rep_type, *pull_type;

      rep_type = cuc_rep_type(p->head.cuc);
      if(is_curried) {
        char buf[25];
        LONG k;
        EXPR *num;

        k = list_length(p->head.cuc->CUC_TYPES);
        sprintf(buf, "%ld", k-1);
        num = const_expr(string_const_tb0(buf), NAT_CONST, natural_type, line);
        pullHelp = apply_expr(global_sym_expr(PULLCURRIEDHELP_ID, line),
			      num, line);
      }
      else {
        pullHelp = global_sym_expr(PULLHELP_ID, line);
      }
      bump_type(this_cnstr_id->ty = function_t(rep_type, t));

      y_pat    = new_pat_var(HIDE_CHAR_STR "y", line);
      y_id     = y_pat->E1;
      rest_pat = new_pat_var(HIDE_CHAR_STR "rest", line);
      rest_id  = rest_pat->E1;
      pat      = new_expr2(PAIR_E, y_pat, rest_pat, line);
      pull_type = function_t(string_type, pair_t(rep_type, string_type));
      pull     = typed_global_sym_expr(PULL_ID, pull_type, line);
      pullHelpArg = new_expr2(PAIR_E, ss, 
			      new_expr2(PAIR_E, cnstr_as_str, pull, line),
			      line);
      case_test = new_expr2(MATCH_E, pat,
			    apply_expr(pullHelp, pullHelpArg, line),
			    line);

      case_val = new_expr2(PAIR_E, 
			   apply_expr(this_cnstr_id, y_id, line), 
			   rest_id, line);
      
    }

    choose_expr = try_expr(case_test, case_val, choose_expr, TRY_F, line);
  }

  /*----------------------------------------------------*
   * Add the case					*
   *							*
   *  case nil? ss => fail listExhaustedX		*
   *							*
   * to the front of the body of the choose expression.	*
   *----------------------------------------------------*/

  {EXPR *failer, *test, *nilq;

   failer = new_expr1(CONST_E, NULL_E, line);
   failer->SCOPE = FAIL_CONST;
   failer->OFFSET = LISTEXHAUSTED_EX;
   nilq = global_sym_expr(NILQ_ID, line);
   test = apply_expr(nilq, ss, line);
   choose_expr = new_expr3(IF_E, test, failer, choose_expr, line);
  }

  /*----------------------------------------------------*
   * Build the body of the function, consisting of	*
   *							*
   *   Let ss = skipWS(s).				*
   *							*
   * followed by the choose expression.			*
   *----------------------------------------------------*/

  {EXPR* skipws = global_sym_expr(SKIPWS_ID, line);
   EXPR* skip_s = apply_expr(skipws, target_expr, line);
   EXPR* let    = new_expr2(LET_E, ss, skip_s, line);
   fun_body     = apply_expr(let, choose_expr, line);
  }

  /*------------------------------------------*
   * Build and issue the declaration of pull. *
   *------------------------------------------*/

  {EXPR *heading, *body, *def;
   TYPE *pull_type;
   MODE_TYPE *mode;

   pull_type = function_t(string_type, pair_t(copy_type(t,1), string_type));
   heading   = id_expr(std_id[PULL_ID], line);
   bump_type(heading->ty = pull_type);
   body      = new_expr2(FUNCTION_E, target_pat, fun_body, line);
   bump_expr(def = new_expr2(LET_E, heading, body, line));
   mode = allocate_mode();  /* ref cnt is 1. */
   add_mode(mode, DEFAULT_MODE);
   add_mode(mode, OVERRIDES_MODE);
   defer_issue_dcl_p(def, LET_E, mode);
   drop_mode(mode);
   drop_expr(def);
  }

  /*---------------------------------------------------------------*
   * Possibly pad the definition. This is needed for a restrictive *
   * family definition.						   *
   *---------------------------------------------------------------*/

  pad_sym_def(PULL_ID, t);

  /*------------------------------------------------------------*
   * Create an execute declaration that adds this pull function	*
   * to pullWrappedBox, by calling function InstallWrapPuller.	*
   * The execute has the form					*
   *								*
   *   Execute							*
   *     InstallPull (pull:main_pull_type).			*
   *   %Execute							*
   *------------------------------------------------------------*/

  {EXPR *installer, *pull_id, *execute;
   TYPE *pull_type, *rep_type, *ft;
   MODE_TYPE *mode;

   ft = find_u(t);
   rep_type = (TKIND(ft) == FAM_MEM_T) 
                ? fam_mem_t(ft->TY2, WrappedANY_type)
	        : ft;
   pull_type = function_t(string_type, pair_t(rep_type, string_type));
   installer = global_sym_expr(INSTALLWRAPPULLER_ID, line);
   pull_id   = typed_global_sym_expr(PULL_ID, pull_type, line);
   execute   = apply_expr(installer, pull_id, line);
   mode      = simple_mode(HIDE_MODE);  /* ref cnt is 1. */
   defer_issue_dcl_p(execute, EXECUTE_E, mode);
   drop_mode(mode);
  }

  /*-----------------------*
   * Drop refs and return. *
   *-----------------------*/

  drop_list(L);
  drop_type(t);
}


/****************************************************************
 *			DECLARE_COPY_P				*
 ****************************************************************
 * Declare function copy for type CopyFlavor -> t -> t.		*
 * L is the CUC-list that describes the rhs of the declaration  *
 * of t.  It must be nonempty. 					*
 *								*
 *--------------------------------------------------------------*
 *								*
 * If type t is uniform (having only one constructor) then the	*
 * definition is						*
 *								*
 *  Let{default} copy = cast copy.				*
 *								*
 * where the copy on the rhs of this declaration is for the	*
 * representation species.					*
 *								*
 * If type t is declared by					*
 *								*
 *  Species t = cons1 | cons2(R) | ....				*
 *								*
 * then the declaration of copy has the form			*
 *								*
 *  Let copy ?flav ?x = 					*
 *    Choose first matching x					*
 *      case cons1 => cons1					*
 *      case cons2(?y) => cons2(copy flav y).			*
 *      ...							*
 *      else => fail(noCaseX)					*
 *    %Choose							*
 *  %Let							*
 *								*
 * In the case of a curried constructor, such as		*
 *								*
 *  Species t = ... | cons A B C | ...				*
 *								*
 * the case is							*
 *								*
 *     case ;;cons(?y) => ;;cons(copy y)			*
 *								*
 * where ;;cons is the constructor name cons with two copies of *
 * HIDE_CHAR in front of it.					*
 *								*
 * XREF: Called by dclclass.c to declare copy at a species 	*
 * declaration.							*
 ****************************************************************/

void declare_copy_p(LIST *L, TYPE *t)
{
  TYPE *rep_type, *new_copy_type, *old_copy_type;
  MODE_TYPE *mode;
  int   line = current_line_number;

  bump_list(L);
  bump_type(t);
  bump_type(new_copy_type = function_t(copyflavor_type, function_t(t, t)));
  mode = simple_mode(DEFAULT_MODE); /* ref cnt is 1. */
  add_mode(mode, OVERRIDES_MODE);

  /*--------------------------------------------------------*
   * If there is only one constructor, then just do a cast. *
   *--------------------------------------------------------*/

  if(L->tail == NIL) {
    rep_type = cuc_rep_type(L->head.cuc);
    bump_type(old_copy_type = function_t(copyflavor_type, 
					 function_t(rep_type, rep_type)));
    define_by_cast_from_id(std_id[COPY_ID], new_copy_type, 
			   std_id[COPY_ID], old_copy_type,
			   mode, TRUE, line);
    drop_type(old_copy_type);
  }

  /*--------------------------------------------------*
   * For multiple constructors, build the definition. *
   *--------------------------------------------------*/

  else {
    EXPR *target_pat, *target_expr, *match_expr, *case_result;
    EXPR *patfun, *cnstr, *choose_expr, *pat, *flav_pat, *flav_id;
    char *this_cnstr, *this_real_cnstr, *patfun_name;
    Boolean constant_case, is_curried;
    LIST *p;
    CLASS_UNION_CELL *cuc;

    /*----------------------------------------------------------*
     * Start with the else expression at the end of the choose 	*
     * expr and build backwards from there. 			*
     *----------------------------------------------------------*/

    choose_expr = make_else_expr_p(FIRST_ATT, NO_CASE_EX, line);

    /*--------------------------------------*
     * target_pat is ?x.  target_expr is x. *
     * flav_pat is ?flav.  flav_id is flav. *
     *--------------------------------------*/

    target_pat  = new_pat_var(HIDE_CHAR_STR "x", line);
    target_expr = target_pat->E1;
    flav_pat    = new_pat_var(HIDE_CHAR_STR "flav", line);
    flav_id     = flav_pat->E1;

    /*------------------------------------------*
     * Build the body of the choose expression.	*
     *------------------------------------------*/

    for(p = L; p != NULL; p = p->tail) {
      cuc = p->head.cuc;

      /*----------------------------------------------------------------*
       * constant_case is true if this case is a constant (such as	*
       *               cons1 in the example above).			*
       *								*
       * is_curried    is true if this is a curried constructor case.	*
       *								*
       * this_cnstr    is the name of the constructor for this case.	*
       *----------------------------------------------------------------*/

      constant_case = cuc->tok == TYPE_ID_TOK && is_hermit_type(cuc->CUC_TYPE);
      is_curried    = cuc->tok == TYPE_LIST_TOK;
      this_cnstr = this_real_cnstr = cuc->name;
      if(is_curried) {
        this_cnstr = attach_prefix_to_id(HIDE_CHAR_STR HIDE_CHAR_STR, 
				         this_cnstr, 0);
      }

      /*----------------------------------------------------------------*
       * pat_fun is the pattern function that selects this case. 	*
       *								*
       * cnstr is the constructor (same name as pat_fun).		*
       *								*
       * If cuc->withs != NIL, need to use the forgetful pattern	*
       * function, ending on ??. 					*
       *----------------------------------------------------------------*/

      patfun_name = (cuc->withs == NIL) 
	                    ? this_cnstr 
			    : forgetful_tester_name(this_cnstr);
      patfun = id_expr(patfun_name, line);
      cnstr  = id_expr(patfun_name, line);

      /*------------------------------------------------*
       * Get the pattern and the case value. 		*
       *------------------------------------------------*/

      if(constant_case) {
        pat = patfun;
        bump_type(pat->ty = t);
        case_result = cnstr;
      }
      else {
        EXPR *pvar, *copier;

        rep_type = cuc_rep_type(p->head.cuc);
        bump_type(patfun->ty = function_t(rep_type, t));
        bump_type(cnstr->ty  = patfun->ty);
        bump_expr(pvar = new_pat_var(HIDE_CHAR_STR "y", line));
        pat = apply_expr(patfun, pvar, line);
        copier = apply_expr(id_expr(std_id[COPY_ID], line), flav_id, line);
        case_result = apply_expr(cnstr, apply_expr(copier, pvar->E1, line), 
				 line);
        drop_expr(pvar);
      }

      /*--------------------------------------------------------*
       * Add a case to the front of the choose expr body.	*
       *--------------------------------------------------------*/

      match_expr  = new_expr2(MATCH_E, pat, target_expr, line);
      choose_expr = try_expr(match_expr, case_result, choose_expr, 
			     TRY_F, line);
    }

    /*----------------------------------*
     * Build and issue the declaration. *
     *----------------------------------*/

    {EXPR *heading, *body, *def;

     heading = id_expr(std_id[COPY_ID], line);
     bump_type(heading->ty = new_copy_type);
     body = new_expr2(FUNCTION_E, flav_pat,
		      new_expr2(FUNCTION_E, target_pat, 
			        choose_expr, line),
		      line);
     bump_expr(def = new_expr2(LET_E, heading, body, line));
     defer_issue_dcl_p(def, LET_E, mode);
     drop_expr(def);
    }
  }

  /*---------------------------------------------------------------*
   * Possibly pad the definition. This is needed for a restrictive *
   * family definition.						   *
   *---------------------------------------------------------------*/

  pad_sym_def(COPY_ID, t);

  /*-----------------------*
   * Drop refs and return. *
   *-----------------------*/

  drop_list(L);
  drop_type(t);
  drop_type(new_copy_type);
  drop_mode(mode);
}


/****************************************************************
 *			DECLARE_MOVE_COPY_P			*
 ****************************************************************
 * Declare functions Move, Copy and new for species t.  t must  *
 * be a uniform species, and L is its cuc-list.  (So L must	*
 * have just one member.)					*
 * 								*
 * The declaration is by a cast from Move and Copy for the	*
 * representation type.						*
 ****************************************************************/

void declare_move_copy_p(LIST *L, TYPE *t)
{
  TYPE *new_move_type, *old_move_type, *rep_type;
  TYPE *new_newfun_type, *old_newfun_type;
  MODE_TYPE *mode;

  rep_type = cuc_rep_type(L->head.cuc);
  bmp_type(new_move_type = function_t(pair_t(t,t), hermit_type));
  bmp_type(old_move_type = function_t(pair_t(rep_type, rep_type), 
				      hermit_type));
  bmp_type(new_newfun_type = function_t(boxflavor_type, t));
  bmp_type(old_newfun_type = function_t(boxflavor_type, rep_type));
  mode = simple_mode(DEFAULT_MODE); /* ref cnt is 1. */

  if(is_visible_typed_global_tm(std_id[MOVE_ID], current_package_name, 
				old_move_type, FALSE)) {
    define_by_cast_from_id(std_id[MOVE_ID], new_move_type, 
			   std_id[MOVE_ID], old_move_type,
			   mode, TRUE, current_line_number);
  }

  if(is_visible_typed_global_tm(std_id[CCOPY_ID], current_package_name, 
				old_move_type, FALSE)) {
    define_by_cast_from_id(std_id[CCOPY_ID], new_move_type, 
			   std_id[CCOPY_ID], old_move_type,
			   mode, TRUE, current_line_number);
  }

  if(is_visible_typed_global_tm(std_id[NEW_ID], current_package_name, 
				old_newfun_type, FALSE)) {
    define_by_cast_from_id(std_id[NEW_ID], new_newfun_type, 
			   std_id[NEW_ID], old_newfun_type,
			   mode, TRUE, current_line_number);
  }
  drop_mode(mode);
  drop_type(new_move_type);
  drop_type(old_move_type);
  drop_type(new_newfun_type);
  drop_type(old_newfun_type);
}


/************************************************************************
 *			DECLARE_UPCAST_P		        	*
 *			DECLARE_DOWNCAST_P		        	*
 *			DECLARE_UPDOWNCAST_P		        	*
 ************************************************************************
 * Function declare_upcast_p defines ^^ for type			*
 * F(`a) -> F(`b) constraint (`b >= `a).				*
 *									*
 * Function declare_downcast_p defines downcast for type		*
 * F(`a) -> F(`b) constraint (`a >= `b).				*
 *									*
 * Function declare_updowncast_p is shared code for both.  Parameter	*
 * up is true for an upcast and false for a downcast.			*
 *									*
 * These functions should only be used when F is a transparent		*
 * family.  								*
 *									*
 * L is the CUC-list that describes the rhs of the declaration  	*
 * of F.  It must be nonempty.						*
 *									*
 * ARG is the argument type of F in the species definition.		*
 *									*
 * The return value is TRUE on success and FALSE on failure.		*
 *----------------------------------------------------------------------*
 * 									*
 * If family F is uniform (having only one constructor) then the 	*
 * definition of ^^ is							*
 *									*
 *   Let{underride} ^^ = cast(^^).					*
 *									*
 * where ^^ on the rhs is for the representation type.			*
 *									*
 * If family F is declared by						*
 *									*
 *  Species F(`a) = cons1 | cons2(R) | ....				*
 *									*
 * then the declaration of ^^ has the form				*
 *									*
 *  Let{underride} ^^(?x) = 						*
 *    Choose first							*
 *      case do Match cons1 = x.     => cons1	 			*
 *      case do Match cons2(?y) = x. => (cons2(^^(y)))			*
 *      ...								*
 *    %Choose								*
 *  %Let								*
 *									*
 * In the case of a curried constructor, such as			*
 *									*
 *  Species F(`a) = ... | cons A B C | ...				*
 *									*
 * the case is								*
 *									*
 *     case ;;cons(?y) => ;;cons(^^ y)					*
 *									*
 * where ;;cons is the constructor name cons with two copies of 	*
 * HIDE_CHAR in front of it.						*
 *									*
 * The definition of downcast is similar.  				*
 ************************************************************************/

/*---------------------------------------------------------------*
 * build_udcast_choose builds the choose expression that is part *
 * of the definition of downcast or ^^ in the case of a family	 *
 * definition with more than one constructor.			 *
 *---------------------------------------------------------------*/

PRIVATE EXPR*
build_udcast_choose(LIST *L, TYPE *domain, TYPE *codomain,
		    EXPR **targ_pat, HASH2_TABLE **ty_b, 
		    char *udcast_name, int line)
{
  EXPR *target_pat, *target_expr, *match_expr, *case_result;
  EXPR *patfun, *cnstr, *choose_expr, *pat;
  char *this_cnstr, *this_real_cnstr, *patfun_name;
  LIST *p;
  CLASS_UNION_CELL *cuc;
  Boolean constant_case, is_curried;

  /*------------------------------------------------------------*
   * Start with the else expression at the end of the choose	*
   * expr and build backwards from there. 			*
   *------------------------------------------------------------*/

  choose_expr = make_else_expr_p(FIRST_ATT, NO_CASE_EX, line);

  /*--------------------------------------*
   * target_pat is ?x.  target_expr is x. *
   *--------------------------------------*/

  target_pat  = *targ_pat = new_pat_var(HIDE_CHAR_STR "x", line);
  target_expr = target_pat->E1;

  /*------------------------------------------*
   * Build the body of the choose expression. *
   *------------------------------------------*/

  for(p = L; p != NULL; p = p->tail) {
    cuc = p->head.cuc;

    /*------------------------------------------------------------------*
     * constant_case is true if this case is a constant (such as	*
     *               cons1 in the example above).			*
     *									*
     * is_curried    is true if this is a curried constructor case.	*
     *									*
     * this_cnstr    is the name of the constructor for this case.	*
     *------------------------------------------------------------------*/

    constant_case = cuc->tok == TYPE_ID_TOK && is_hermit_type(cuc->CUC_TYPE);
    is_curried    = cuc->tok == TYPE_LIST_TOK;
    this_cnstr = this_real_cnstr = cuc->name;
    if(is_curried) {
      this_cnstr = attach_prefix_to_id(HIDE_CHAR_STR HIDE_CHAR_STR, 
				       this_cnstr, 0);
    }

    /*----------------------------------------------------------*
     * pat_fun is the pattern function that selects this case. 	*
     *								*
     * cnstr is the constructor (same name as pat_fun).		*
     *								*
     * If cuc->withs != NIL, need to use the forgetful pattern	*
     * function, ending on ??. 					*
     *----------------------------------------------------------*/

    patfun_name = (cuc->withs == NIL) 
	                    ? this_cnstr 
			    : forgetful_tester_name(this_cnstr);
    patfun = id_expr(patfun_name, line);
    cnstr  = id_expr(patfun_name, line);

    /*------------------------------------------------*
     * Get the pattern and the case value. 	      *
     *------------------------------------------------*/

    if(constant_case) {
      pat = patfun;
      bump_type(pat->ty = domain);
      case_result = cnstr;
    }
    else {
      EXPR *pvar, *udcaster;
      TYPE *rep_type, *rep_domain, *rep_codomain, *old_udcast_type;

      bump_type(rep_type = copy_type1(cuc_rep_type(p->head.cuc), ty_b, 0));
      rep_domain   = var_t(NULL);
      rep_codomain = var_t(NULL);
      bump_type(old_udcast_type = function_t(rep_domain, rep_codomain));

      bump_type(patfun->ty = function_t(rep_type, domain));
      bump_type(cnstr->ty  = function_t(rep_codomain, codomain));
      bump_expr(pvar = new_pat_var(HIDE_CHAR_STR "y", line));
      pat = apply_expr(patfun, pvar, line);
      udcaster = typed_id_expr(udcast_name, old_udcast_type, line);
      case_result = apply_expr(cnstr, 
			       apply_expr(udcaster, pvar->E1, line), 
			       line);
      drop_expr(pvar);
      drop_type(old_udcast_type);
    }

    /*--------------------------------------------------*
     * Add a case to the front of the choose expr body.	*
     *--------------------------------------------------*/

    match_expr  = new_expr2(MATCH_E, pat, target_expr, line);
    choose_expr = try_expr(match_expr, case_result, choose_expr, 
			   TRY_F, line);
  }
  return choose_expr;
}

/*-----------------------------------------------------------*/

PRIVATE Boolean
declare_updowncast_p(LIST *L, TYPE *F, TYPE *arg, Boolean up)
{
  TYPE *rep_type, *old_udcast_type, *new_udcast_type;
  MODE_TYPE *mode;
  char *udcast_name;
  int udcast_name_sym;
  Boolean result;
  int line = current_line_number;

  result = TRUE;  /* default */
  bump_list(L);
  bump_type(F);
  mode = simple_mode(UNDERRIDES_MODE); /* ref cnt is 1. */

  udcast_name_sym = up ? DBL_HAT_SYM : DOWNCAST_ID;
  udcast_name     = std_id[udcast_name_sym];

  /*------------------------------------------------------------*
   * If there is only one constructor, then just do a cast. 	*
   * The definition is equivalent to				*
   *								*
   *   Let ^^(con(x)) = con(^^(x))				*
   *								*
   * where con is the constructor and the types are as		*
   * follows.							*
   *   lhs:							*
   *     ^^ : F(arg_type1) -> F(arg_type2)			*
   *     con: rep_type1 -> F(arg_type1)   (A copy of the type	*
   *					   of the constructor)	*
   *     x  : rep_type1						*
   *   rhs:							*
   *     ^^ : rep_type1 -> rep_type2				*
   *     con: rep_type2 -> F(arg_type2)  (Another copy of the   *
   *					  constructor type.)	*
   *------------------------------------------------------------*/

  if(L->tail == NIL) {
    TYPE *constr_type, *constr_type1, *constr_type2;
    TYPE *rep_type1, *rep_type2, *constr_codom1, *constr_codom2;
    Boolean ok1, ok2;

    rep_type = cuc_rep_type(L->head.cuc);
    bump_type(constr_type = function_t(rep_type, fam_mem_t(F, arg)));
    bump_type(constr_type1 = copy_type(constr_type, 0));
    bump_type(constr_type2 = copy_type(constr_type, 0));
    rep_type1     = constr_type1->TY1;
    constr_codom1 = constr_type1->TY2;
    rep_type2     = constr_type2->TY1;
    constr_codom2 = constr_type2->TY2;
    bump_type(old_udcast_type = function_t(rep_type1, rep_type2));
    bump_type(new_udcast_type = function_t(constr_codom1, constr_codom2));
    if(up) {
      ok1 = CONSTRAIN(rep_type2, rep_type1, 0, 0);
      ok2 = CONSTRAIN(constr_codom2, constr_codom1, 0, 0);
    }
    else {
      ok1 = CONSTRAIN(rep_type1, rep_type2, 0, 0);
      ok2 = CONSTRAIN(constr_codom1, constr_codom2, 0, 0);
    }
    if(!ok1 || !ok2) result = FALSE;
    else {
      define_by_cast_from_id(udcast_name, new_udcast_type, 
			     udcast_name, old_udcast_type,
			     mode, TRUE, line);
    }
    drop_type(old_udcast_type);
    drop_type(new_udcast_type);
    drop_type(constr_type1);
    drop_type(constr_type2);
  }

  /*--------------------------------------------------*
   * For multiple constructors, build the definition. *
   *--------------------------------------------------*/

  else {
    EXPR *choose_expr, *target_pat;
    TYPE *new_udcast_domain, *new_udcast_codomain, *arg_cpy;
    HASH2_TABLE *ty_b;
    Boolean ok;

    /*----------------------------------------------------------*
     * Build the type of the up or down caster.  We copy the	*
     * types, so need to keep track of a table (ty_b) to	*
     * control the copying.					*
     *----------------------------------------------------------*/

    ty_b = NULL;
    bump_type(arg_cpy = copy_type1(arg, &ty_b, 0));
    bump_type(new_udcast_codomain = fam_mem_t(F,var_t(NULL)));
    bump_type(new_udcast_domain   = fam_mem_t(F, arg_cpy));
    bump_type(new_udcast_type     = function_t(new_udcast_domain, 
					       new_udcast_codomain));
    if(up) {
      ok = CONSTRAIN(new_udcast_codomain, new_udcast_domain, 0, 0);
    }
    else {
      ok = CONSTRAIN(new_udcast_domain, new_udcast_codomain, 0, 0);
    }
    if(!ok) result = FALSE;
    else {
      choose_expr = build_udcast_choose(L, new_udcast_domain, 
                                        new_udcast_codomain, &target_pat,
					&ty_b, udcast_name, line);    

      /*----------------------------------*
       * Build and issue the declaration. *
       *----------------------------------*/

      {EXPR *heading, *body, *def;

       heading = id_expr(udcast_name, line);
       bump_type(heading->ty = new_udcast_type);
       body = new_expr2(FUNCTION_E, target_pat, choose_expr, line);
       bump_expr(def = new_expr2(LET_E, heading, body, line));
       defer_issue_dcl_p(def, LET_E, mode);
       drop_expr(def);
      }
    }

    drop_type(arg_cpy);
    drop_type(new_udcast_domain);
    drop_type(new_udcast_codomain);
    drop_type(new_udcast_type);
    scan_and_clear_hash2(&ty_b, drop_hash_type);
  }

  /*---------------------------------------------------------------*
   * Possibly pad the definition. This is needed for a restrictive *
   * family definition.						   *
   *---------------------------------------------------------------*/

  {TYPE* t = fam_mem_t(F, arg);
   bump_type(t);
   pad_sym_def(udcast_name_sym, t);
  }

  /*-----------------------*
   * Drop refs and return. *
   *-----------------------*/

  drop_list(L);
  drop_type(F);
  drop_mode(mode);
  return result;
}

/*-------------------------------------------------------------*/

void declare_upcast_p(LIST *L, TYPE *F, TYPE *arg)
{
  declare_updowncast_p(L, F, arg, TRUE);
}

/*-------------------------------------------------------------*/

void declare_downcast_p(LIST *L, TYPE *F, TYPE *arg)
{
  declare_updowncast_p(L, F, arg, FALSE);
}


/****************************************************************
 *			DECLARE_ROLE_SELECTOR			*
 ****************************************************************
 * Declare role_name as a selector.				*
 * If this selector is not irregular, then the definition	*
 * has the form							*
 *								*
 *       Let role_name = cast fun %Let				*
 *								*
 * where 							*
 *       role_name: main_type     -> reptype.type		*
 *       fun      : main_rep_type -> reptype.type		*
 *	 cast     : type(fun)     -> type(role_name)		*
 *								*
 * and fun is a PRIM_SELECT primitive.  If this is an irregular *
 * destructor, then the definition has the form			*
 *								*
 *       Let role_name = (?x => fun(unwrap(x)))			*
 *								*
 * where							*
 *       role_name: main_type     -> reptype.type		*
 *       fun      : main_rep_type -> reptype.type		*
 *       unwrap   : main_type     -> main_rep_type		*
 *								*
 * where fun is a PRIM_SELECT primitive and unwrap is a 	*
 * PRIM_UNWRAP or PRIM_DUNWRAP primitive that does all of the 	*
 * unwrapping.							*
 ****************************************************************/

PRIVATE void declare_role_selector
  (char *role_name, TYPE *main_type, TYPE *main_rep_type, 
   RTYPE reptype, ROLE *destr_output_role, 
   LIST *posnlist, int discr, Boolean irregular,
   MODE_TYPE *mode, int line)
{
  TYPE *destr_type, *cpy_destr_type, *cpy_main_rep_type;
  EXPR *destr_id, *fun, *def, *cfun;

  HASH2_TABLE* ty_b       = NULL;
  int          extra_mode = -1;

  /*------------------------------------------------------------*
   * We need to copy the types, since they will be bound during	*
   * type inference. 						*
   *------------------------------------------------------------*/

  bump_type(destr_type = function_t(main_type, reptype.type));
  bump_type(cpy_destr_type = copy_type1(destr_type, &ty_b, 0));
  bump_type(cpy_main_rep_type = copy_type1(main_rep_type, &ty_b, 0));
  scan_and_clear_hash2(&ty_b, drop_hash_type);

  /*---------------------------------------------*
   * Get the destructor that does the selection. *
   *---------------------------------------------*/

  bump_expr(destr_id = 
	    new_expr1t(IDENTIFIER_E, NULL_E, cpy_destr_type, line));
  destr_id->STR = role_name;
  bump_role(destr_id->role = fun_role(NULL, destr_output_role));

  /*---------------------------------------------------------------*
   * If this selector is not irregular, then select based strictly *
   * on integer tag and structure.				   *
   *---------------------------------------------------------------*/

  if(!irregular) {
    TYPE *caster_domain_type;
    EXPR *caster;

    bump_type(caster_domain_type = 
	      function_t(cpy_main_rep_type, cpy_destr_type->TY2));
    fun    = make_special_prim_p(PRIM_SELECT, posnlist, discr, 0, 
				 HIDE_CHAR_STR "selector", 
				 current_line_number);
    caster = make_cast(caster_domain_type, cpy_destr_type, line);
    cfun   = apply_expr(caster, fun, line);
    drop_type(caster_domain_type);
  }

  /*-------------------------------------------------------*
   * If the selector is irregular, then we need to unwrap. *
   *-------------------------------------------------------*/

  else {
    EXPR *arg, *funbody, *unwrap;
    TYPE *unwrap_ty;
    int prim;

    fun               = make_special_prim_p(PRIM_SELECT, posnlist, -1, 0, 
					    HIDE_CHAR_STR "selector", line);
    unwrap_ty         = function_t(cpy_destr_type->TY1, cpy_main_rep_type);
    prim              = (discr >= 0) ? PRIM_DUNWRAP : PRIM_UNWRAP;
    unwrap            = make_special_prim_p(prim, NULL, discr, 0, role_name, 
					  line);
    bump_type(unwrap->ty = unwrap_ty);
    arg               = new_pat_var(HIDE_CHAR_STR "x", line);
    funbody         = apply_expr(fun, apply_expr(unwrap, arg->E1, line), line);
    cfun            = new_expr2(FUNCTION_E, arg, funbody, line);
    extra_mode      = IRREGULAR_MODE;
  }

  bump_expr(def = new_expr2(LET_E, destr_id, cfun, line));
  {MODE_TYPE *amode;
   if(extra_mode < 0) amode = mode;
   else {
     amode = copy_mode(mode);
     add_mode(amode, extra_mode);
   }
   bump_mode(amode);
   defer_issue_dcl_p(def, LET_E, amode);
   drop_mode(amode);
  }

  /*----------------------------------------------------------*
   * Attach the possiblity of failing when an unwrap is done. *
   *----------------------------------------------------------*/

  if(discr >= 0) {
    defer_attach_property(pot_fail, role_name, destr_type);
  }

  drop_expr(def);
  drop_type(cpy_main_rep_type);
  drop_type(cpy_destr_type);
  drop_type(destr_type);
  drop_expr(destr_id);
}


/****************************************************************
 *			DECLARE_ROLE_PATFUN			*
 ****************************************************************
 * Declare ;role_name'??' as a pattern function.  The		*
 * definition is						*
 *								*
 *   Pattern ;role_name'??'(p) => match p = role_name(target)..	*
 *								*
 * Also declare ;role_name'??' as missing.			*
 *								*
 * Suppress doing this if suppress_role_patfun_modifier is true.*
 ****************************************************************/

PRIVATE void declare_role_patfun
  (char *role_name, TYPE *main_type, RTYPE reptype, 
   ROLE *destr_output_role, LIST *posnlist, 
   int discr, Boolean irregular, MODE_TYPE *mode, int line)
{
  TYPE *patfun_type, *destr_type;
  ROLE *patfun_role;
  EXPR *destr_id;

  char* patfun_name = make_role_sel_id(role_name);
  HASH2_TABLE* ty_b = NULL;

  /*---------------------------*
   * Build and copy the types. *
   *---------------------------*/

  bump_type(patfun_type = function_t(reptype.type, main_type));
  bump_type(destr_type = function_t(main_type, reptype.type));
  SET_TYPE(patfun_type, copy_type1(patfun_type, &ty_b, 0));
  SET_TYPE(destr_type, copy_type1(destr_type, &ty_b, 0));
  scan_and_clear_hash2(&ty_b, drop_hash_type);

  /*------------------------------------------------------------*
   * Build the identifier being defined, and the destructor in	*
   * terms of which it is defined. 				*
   *------------------------------------------------------------*/

  bump_role(patfun_role = fun_role(destr_output_role, NULL));
  bump_expr(destr_id = 
	    new_expr1t(IDENTIFIER_E, NULL_E, destr_type, line));
  destr_id->STR = role_name;

  /*----------------------------------*
   * Build and issue the declaration. *
   *----------------------------------*/
  
  long_auto_pat_fun_p(patfun_name, patfun_type, patfun_role, 
		      destr_id, TRUE, TRUE, posnlist, discr, irregular);

  /*--------------------------------------------------------------*
   * The pattern function is globally assumed a pattern function. *
   *--------------------------------------------------------------*/

  patfun_assume_tm(patfun_name, 1);

  /*--------------------------------------------------------*
   * Issue a missing declaration for this pattern function. *
   *--------------------------------------------------------*/

  issue_missing_tm(patfun_name, patfun_type, 0);

  drop_type(patfun_type);
  drop_type(destr_type);
  drop_role(patfun_role);
  drop_expr(destr_id);
}


/************************************************************************
 *			DECLARE_ROLE_MODIFIER				*
 ************************************************************************
 * Build and declare the modifier ;role_name'!!'.  			*
 * If not irregular, the definition has the form			*
 *									*
 *       Let ;role_name'!!' = cast fun %Let				*
 *									*
 * where								*
 *       ;role_name'!!': (main_type, reptype.type)     -> main_type	*
 *	 fun           : (main_rep_type, reptype.type) -> main_type 	*
 *	 cast          : type(fun) -> type(;role_name'!!')		*
 *									*
 * and fun is a PRIM_MODIFY primitive.  If irregular, then the		*
 * definition has the form						*
 *									*
 *       Let{irregular} ;role_name'!!'(?x,?v) =				*
 *          wrap(modify(unwrap(x),v))					*
 *       %Let								*
 *									*
 * where 								*
 *      ;role_name'!!': (main_type, rt') -> main_type			*
 *      unwrap        : main_type -> main_rep_type			*
 *      modify        : (main_rep_type, rt') -> mrt'			*
 *      wrap          : mrt' -> mt'					*
 *									*
 * where rt', mrt' and mt' are obtained as follows.			*
 *									*
 *     Make copies mt', mrt', rt' of main_type, main_rep_type and 	*
 *     reptype.type, respectively, using common variables.		*
 *									*
 *     Build mrt'' by substituting rt' for reptype.type in 		*
 *     main_rep_type. 							*
 *									*
 *     Unify mrt'' with mrt'.						*
 ************************************************************************/

PRIVATE void declare_role_modifier
  (char *role_name, TYPE *main_type, TYPE *main_rep_type, 
   RTYPE reptype, ROLE *destr_output_role,
   LIST *posnlist, int discr, Boolean irregular,
   MODE_TYPE *mode, int line)
{
  TYPE *modifier_type, *cpy_main_rep_type, *cpy_main_type, *cpy_rep_type;
  char *modifier_name;
  EXPR *modifier_id, *def, *cfun;
  ROLE *modifier_role;
  MODE_TYPE *newmode;

  HASH2_TABLE* ty_b = NULL;

  bump_mode(newmode = mode);

  /*--------------------------*
   * Build and copy the type. *
   *--------------------------*/

  bump_type(modifier_type = 
	    function_t(pair_t(main_type, reptype.type), main_type));
  SET_TYPE(modifier_type, copy_type1(modifier_type, &ty_b, 0));
  bump_type(cpy_main_rep_type = copy_type1(main_rep_type, &ty_b, 0));
  cpy_main_type = modifier_type->TY2;
  cpy_rep_type  = modifier_type->TY1->TY2;
  scan_and_clear_hash2(&ty_b, drop_hash_type);
  
  /*-----------------------------------*
   * Build the modifier id expression. *
   *-----------------------------------*/

  modifier_name = make_role_mod_id(role_name);
  modifier_role = fun_role(pair_role(NULL, destr_output_role), NULL);
  bump_expr(modifier_id = 
	    new_expr1t(IDENTIFIER_E, NULL_E, modifier_type, line));
  modifier_id->STR = modifier_name;
  bump_role(modifier_id->role = modifier_role);

  /*-------------------------------------------*
   * Case of a modifier that is not irregular. *
   *-------------------------------------------*/

  if(!irregular) {
    TYPE *caster_domain_type;
    EXPR *fun, *caster;

    fun =  make_special_prim_p(PRIM_MODIFY, posnlist, discr, 0, 
			       HIDE_CHAR_STR "modifier", current_line_number);
    bump_type(caster_domain_type = 
	      function_t(pair_t(cpy_main_rep_type, cpy_rep_type), 
			 cpy_main_type));
    caster = make_cast(caster_domain_type, modifier_type, line);
    cfun = apply_expr(caster, fun, line);
    drop_type(caster_domain_type);
  }

  /*---------------------------------------*
   * Case of a modifier that is irregular. *
   *---------------------------------------*/

  else {
    TYPE *mrt1, *mt1, *rt1, *mrt2, *unwrap_type, *wrap_type, 
         *inner_modifier_type;
    EXPR *unwrap, *wrap, *inner_modifier, *x_pv, *v_pv, *formal,
         *unwrapped_x, *modified_x, *result;

    /*----------------*
     * Get the types. *
     *----------------*/

    ty_b = NULL;
    bump_type(mrt1 = copy_type1(main_rep_type, &ty_b, 0));
    bump_type(mt1  = copy_type1(main_type, &ty_b, 0));
    bump_type(rt1  = copy_type1(reptype.type, &ty_b, 0));
    unwrap_type = function_t(cpy_main_type, cpy_main_rep_type);
    wrap_type   = function_t(mrt1, mt1);
    inner_modifier_type = function_t(pair_t(cpy_main_rep_type, rt1), mrt1);

    /*-------------------------------------*
     * Perform the extra type unification. *
     *-------------------------------------*/

    bump_type(mrt2 = substitute_at_posn_t(cpy_main_rep_type, rt1, posnlist));
    unify_u(&mrt1, &mrt2, 0);

    /*------------------------------------------*
     * The type of modifier_id should be 	*
     * (cpy_main_type, rt1) -> cpy_main_type. 	*
     *------------------------------------------*/

    SET_TYPE(modifier_id->ty,
	     function_t(pair_t(cpy_main_type, rt1), cpy_main_type));

    /*----------------------------------*
     * Build the wrapper and unwrapper. *
     *----------------------------------*/

    unwrap      = new_expr1t(SPECIAL_E, NULL, unwrap_type, line);
    unwrap->STR = modifier_name;
    wrap        = new_expr1t(SPECIAL_E, NULL, wrap_type, line);
    wrap->STR   = HIDE_CHAR_STR "wrap";
    if(discr >= 0) {
      unwrap->PRIMITIVE = PRIM_DUNWRAP;
      wrap->PRIMITIVE   = PRIM_DWRAP;
      unwrap->SCOPE     = wrap->SCOPE = discr;
    }
    else {
      unwrap->PRIMITIVE = PRIM_UNWRAP;
      wrap->PRIMITIVE   = PRIM_WRAP;
    }

    /*---------------------------*
     * Build the inner modifier. *
     *---------------------------*/

    inner_modifier = new_expr1t(SPECIAL_E, NULL, inner_modifier_type, line);
    inner_modifier->PRIMITIVE = PRIM_MODIFY;
    inner_modifier->SCOPE     = -1;
    inner_modifier->STR       = HIDE_CHAR_STR "modify";
    bump_list(inner_modifier->EL3 = posnlist);

    /*----------------------------------*
     * Build the value of role_name'!!' *
     *----------------------------------*/

    x_pv   = new_pat_var(HIDE_CHAR_STR "x", line);
    v_pv   = new_pat_var(HIDE_CHAR_STR "v", line);
    formal = new_expr2(PAIR_E, x_pv, v_pv, line);
    unwrapped_x = apply_expr(unwrap, x_pv->E1, line);
    modified_x  = apply_expr(inner_modifier,
			     new_expr2(PAIR_E, unwrapped_x, v_pv->E1, line),
			     line);
    result = apply_expr(wrap, modified_x, line);
    cfun   = new_expr2(FUNCTION_E, formal, result, line);

    /*---------------------*
     * Add irregular mode. *
     *---------------------*/

    drop_mode(newmode);
    newmode = simple_mode(IRREGULAR_MODE);         /* ref cnt is 1. */

    drop_type(mrt1);
    drop_type(mt1);
    drop_type(rt1);
    drop_type(mrt2);
  }

  bump_expr(def = new_expr2(LET_E, modifier_id, cfun, line));
  defer_issue_dcl_p(def, LET_E, newmode);
        
  drop_mode(newmode);
  drop_type(modifier_type);
  drop_type(cpy_main_rep_type);
  drop_expr(modifier_id);
  drop_expr(def);
}


/****************************************************************
 *			HANDLE_ROLE				*
 ****************************************************************
 * Declare role_name as a selector, and declare the related	*
 * function ;role_name'??' and ;role_name'!!'.			*
 *								*
 * If suppress_role_extras is true, then only declare the	*
 * selector, skipping ;role_name'??' and 'role_name'!!'.	*
 ****************************************************************/

PRIVATE void handle_role(char *role_name, TYPE *main_type, TYPE *main_rep_type,
			RTYPE reptype, LIST *posnlist, int discr,
			Boolean irregular, MODE_TYPE *mode)
{
  int line = current_line_number;
  ROLE *destr_output_role;

  role_name = new_name(role_name, TRUE);
  bump_role(destr_output_role = 
	    pair_role(reptype.role->role1, reptype.role->role2));

  /*---------------------------------*
   * Declare the selector role_name. *
   *---------------------------------*/

  declare_role_selector(role_name, main_type, main_rep_type,
			reptype, destr_output_role,
			posnlist, discr, irregular, mode, line);

  if(!suppress_role_extras) {

    /*----------------------------------------------*
     * Declare the pattern function ;role_name'??'. *
     *----------------------------------------------*/

    declare_role_patfun(role_name, main_type, reptype, destr_output_role,
			posnlist, discr, irregular, mode, line);
 
    /*--------------------------------------*
     * Declare the modifier ;role_name'!!'. *
     *--------------------------------------*/

    declare_role_modifier(role_name, main_type, main_rep_type, reptype,
			  destr_output_role, posnlist, discr, irregular, 
			  mode, line);
  }

  drop_role(destr_output_role);
}


/****************************************************************
 *			DECLARE_FIELDS_P			*
 ****************************************************************
 * A declaration of the form					*
 *								*
 *   Species{mode} main_type = reptype.				*
 *								*
 * or								*
 *								*
 *   Species{mode} main_type = ... | reptype | ...              *
 *								*
 * is being issued, where main_type is either a type id or a	*
 * family member.						*
 *								*
 * Declare the field accessors from roles.  Parameters are	*
 * as follows.							*
 *								*
 * Posnlist 	tells the position of this part in the whole 	*
 *		type; it should be NIL when declare_fields_p is *
 *		called from outside.				*
 *								*
 * Discr 	is the discriminator number for this part, 	*
 * 		when the declaration is a nonuniform declaration*
 *		(form 2 above, with more than one thing on the  *
 *		right-hand side), and is -1 for a uniform	*
 *		declaration (form 1 above).			*
 *								*
 * irregular	is 1 if this part is irregular, and 0 if not	*
 *		irregular.					*
 *								*
 * main_rep_type should be reptype.type when called from 	*
 *		 outside.  It is the rep_type.type of the 	*
 *		 main call.					*
 *								*
 * mode		is the mode of the declaration.			*
 *		It is a safe pointer: it will not be kept	*
 *		longer than the lifetime of this function call.	*
 *								*
 * XREF: Called by dclclass.c to declare $ at a species 	*
 * declaration.							*
 ****************************************************************/

void declare_fields_p(TYPE *main_type, TYPE *main_rep_type,
		      RTYPE reptype, INT_LIST *posnlist, int discr,
		      Boolean irregular, MODE_TYPE *mode)

{
  char *role_name;

# ifdef DEBUG
    if(trace_implicit_defs) {
      trace_t(39);
      trace_ty(main_type);
      trace_t(483);
      trace_ty(main_rep_type);
      trace_t(484);
      trace_ty(reptype.type);
      trace_t(485);
      fprint_role(TRACE_FILE, reptype.role);
      trace_t(486);
      print_str_list_nl(posnlist);
      trace_t(40, discr, irregular);
    }
# endif

  if(reptype.role == NULL) return;

  bump_type(main_type);
  bump_rtype(reptype);
  bump_list(posnlist);
  role_name = get_singleton(reptype.role);

  /*-----------------------------*
   * Handle the top role name r. *
   *-----------------------------*/

  if(role_name != NULL) {
    MODE_TYPE *newmode;
    newmode = copy_mode(mode);  /* ref cnt is 1. */
    modify_mode(newmode, reptype.role->mode, FALSE);
    handle_role(role_name, main_type, main_rep_type, reptype, posnlist,
		discr, irregular, newmode);
    drop_mode(newmode);
  }

  /*---------------------------*
   * Recursion to lower roles. *
   *---------------------------*/

  {int kind;
   TYPE *t;
   INT_LIST *left_list, *right_list;
   RTYPE r;

   kind = RKIND(reptype.role);
   t = find_u(reptype.type);
   if(kind == PAIR_ROLE_KIND && TKIND(t) == PAIR_T) {
     bump_list(left_list = int_cons(LEFT_SIDE, posnlist));
     bump_list(right_list = int_cons(RIGHT_SIDE, posnlist));
     r.type = t->TY1;
     r.role = reptype.role->role1;
     declare_fields_p(main_type, main_rep_type, r, left_list, discr,
		      irregular, mode);
     r.type = t->TY2;
     r.role = reptype.role->role2;
     declare_fields_p(main_type, main_rep_type, r, right_list,discr,
		      irregular, mode);
     drop_list(left_list);
     drop_list(right_list);
   }
  }

  drop_type(main_type);
  drop_rtype(reptype);
  drop_list(posnlist);

}


/******************************************************************
 *			MAKE_SPECIAL_PRIM_P			  *
 ******************************************************************
 * Return a special primitive with primitive type prim, and extra *
 * information given by discr, posnlist and irregular.		  *
 ******************************************************************/

PRIVATE EXPR* 
make_special_prim_p(int prim, INT_LIST *posnlist, int discr, 
		     Boolean irregular, char *name, int line)
{
  EXPR* result          = new_expr1(SPECIAL_E, NULL, line);
  result->STR           = name;
  result->PRIMITIVE     = prim;
  result->SCOPE         = discr;
  result->irregular	= irregular;
  bump_list(result->EL3 = posnlist);
  return result;
}


