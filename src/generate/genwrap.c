/******************************************************************
 * File:    generate/genwrap.c
 * Purpose: generate code to handle wrapping and unwrapping of
 *          values with types.
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

#include <string.h>
#include "../misc/misc.h"
#include "../alloc/allocate.h"
#include "../utils/lists.h"
#include "../utils/strops.h"
#include "../utils/hash.h"
#include "../evaluate/instruc.h"
#include "../error/error.h"
#include "../classes/classes.h"
#include "../standard/stdtypes.h"
#include "../standard/stdids.h"
#include "../exprs/expr.h"
#include "../tables/tables.h"
#include "../generate/generate.h"
#include "../generate/prim.h"
#include "../patmatch/patmatch.h"
#include "../unify/unify.h"
#include "../machdata/except.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/****************************************************************
 *			POSSIBLY_UNWRAP_WARN			*
 ****************************************************************
 * We are unifying t and A dynamically, and function fun is the *
 * irregular function that is performing this unification.	*
 *								*
 * If t and A cannot possibly be the same, always issue a 	*
 * warning stating that.  List boundvars gives dynamic variable *
 * bindings that are needed to check whether t and A might be	*
 * the same.							*
 ****************************************************************/

PRIVATE void 
possibly_unwrap_warn(EXPR *fun, TYPE *t, TYPE *A)
{
# ifdef DEBUG
    if(trace_gen > 1) {
      trace_t(548);
      print_two_types(A, t);
    }
# endif

  if(disjoint(A, t)) {
    sure_type_mismatch_warn(fun, t, A);
  }
}


/****************************************************************
 *			GEN_WRAP_G				*
 ****************************************************************
 * Generate instructions to wrap the top of the stack with	*
 * species t.							*
 ****************************************************************/

void gen_wrap_g(TYPE *t)
{
  generate_type_g(t, 0);
  gen_g(WRAP_I);
}


/****************************************************************
 *			GEN_UNWRAP_TEST_G			*
 ****************************************************************
 * Type A is on the type stack (of the interpreter, at run time.*
 *.That is, the type stack holds a type (without variables)	*
 * that is a possible value of A, which is a type expression	*
 * possibly containing variables.				*
 *								*
 * Generate instructions to unify A with t.  The		*	
 * instructions should fail with exception speciesX if the 	*
 * unification is impossible, and should dynamically bind any   *
 * variables that get bound during the unification.		*
 *								*
 * If A is NULL, then no information is known about what is on  *
 * the type stack.						*
 *								*
 * Expression the_fun is the irregular function that is		*
 * responsible for these bindings.				*
 *								*
 * Sometimes, nothing needs to be done.  If any instructions	*
 * were generated, then *did_something is set true.  If nothing *
 * was generated, then *did_something is set false.		*
 *								*
 * If type t contains a variable with both a >>= constraint	*
 * and a >= constraint, then give a warning.			*
 *								*
 * IMPORTANT NOTE						*
 *								*
 * When generating_standard_funs is true, no unwrap test code   *
 * is generated.  This is because the standard irregular 	*
 * functions do not want to do a test, just to produce their	*
 * answers.							*
 ****************************************************************/

void gen_unwrap_test_g(TYPE *A, TYPE *t, EXPR *the_fun, 
		       Boolean *did_something)
{
  if(generating_standard_funs) {
    *did_something = FALSE;
    return;
  }

  if(A != NULL) replace_null_vars(&A);

  bump_type(A = find_u(A));
  t = find_u(t);

# ifdef DEBUG
    if(trace_gen > 1) {
      trace_t(156);
      trace_ty(A); tracenl();
      trace_t(542);
      trace_ty(t); tracenl();
    }
# endif

  /*----------------------------------------------------*
   * If A and t are identical, then there is no need	*
   * to do anything.  No variables are bound.		*
   *----------------------------------------------------*/

  if(A != NULL && full_type_equal(A,t)) {
#   ifdef DEBUG
      if(trace_gen > 1) trace_t(14);
#   endif
    drop_type(A);
    *did_something = FALSE;
  }

  /*--------------------------------------------------------*
   * If A and t are not identical, generate code to unify.  *
   *--------------------------------------------------------*/

  else {
    generate_type_g(t, 0);
    gen_g(UNIFY_T_I);
    *did_something = TRUE;

    /*----------------------------------------*
     * Warn if there is a sure type mismatch. *
     *----------------------------------------*/    

    possibly_unwrap_warn(the_fun, t, A);
    drop_type(A);

    /*--------------------------------------------*
     * Warn if t contains a variable with both a  *
     * >>= constraint and a >= constraint.	  *
     *--------------------------------------------*/

    check_for_constrained_dynamic_var(t, the_fun);
  }
}


/****************************************************************
 *			UNWRAP_RESTRICTION			*
 ****************************************************************
 * Return a type that describes the most general type that      *
 * unwrapper fun with might return as its result type.		*
 ****************************************************************/

PRIVATE TYPE* unwrap_restriction(EXPR *fun)
{
  TYPE* fun_ty = find_u(fun->ty);
  TYPE* domain = find_u(fun_ty->TY1);
  TYPE_TAG_TYPE domain_kind = TKIND(domain);

  /*------------------------------------------------------------*
   * If the domain of the unwrapper is a wrapped species or  	*
   * family member, then compute the result type as follows. 	*
   *								*
   *   domain		result type				*
   *   G``x		  G`y		G a genus		*
   *   G		  G`y		G a genus		*
   *   C``x(A)	  	  C`y(`a)	C a community		*
   *   C(A)		  C`y(`a)	C a community		*
   *------------------------------------------------------------*/

  if(domain_kind == WRAP_TYPE_T) {
    return tf_or_var_t(domain->ctc);
  }
  else if(domain_kind == FAM_MEM_T) {
    TYPE* domain_fam = find_u(domain->TY2);
    if(TKIND(domain_fam) == WRAP_FAM_T) {
      return fam_mem_t(tf_or_var_t(domain_fam->ctc), type_var_t(NULL));
    }
  }
 
  /*--------------------------------------------------------------*
   * If this is a special unwrapper with a name, then get the     *
   * expectation for that name, and get the restriction type for  *
   * that id.							  *
   *--------------------------------------------------------------*/

  if(EKIND(fun) == SPECIAL_E && fun->STR != NULL) {
    EXPR* id;
    GLOBAL_ID_CELL* gic = get_gic_tm(fun->STR, TRUE);

    if(gic != NULL) {
      TYPE* result;
      bump_expr(id = id_expr(fun->STR, fun->LINE_NUM));
      id->kind = GLOBAL_ID_E;
      id->GIC  = gic;
      bump_type(id->ty = fun->ty);
      bump_type(result = get_restriction_type_tm(id));
      drop_expr(id);
      if(result != NULL) result->ref_cnt--;
      return result;
    }
  }

  if(EKIND(fun) == GLOBAL_ID_E) return get_restriction_type_tm(fun);
  else return var_t(NULL);
}


/****************************************************************
 *			GEN_UNWRAP_G				*
 ****************************************************************
 * Generate an UNWRAP_I instruction to unwrap the top of the	*
 * argument stack, followed by code to unify its type (which    *
 * must be some type in the set of types described by t) with t *
 * itself.  If this will lead to a speciesX exception, then     *
 * report function fun, line line  as the function that does    *
 * the unwrap.			                                *
 ****************************************************************/

void gen_unwrap_g(TYPE *t, EXPR *fun, int line_unused)
{
  package_index finger;
  Boolean did_something;
  TYPE* stack_type;
  int prim;

  EXPR* ss_fun = skip_sames(fun);
  TYPE* tt     = find_u(t);

  {int arg; 
   ENTPART *part;
   Boolean irreg;
   INT_LIST *sel_list;
   prim = get_prim_g(fun, &arg, &part, &irreg, &sel_list);
  }

  bump_type(stack_type = (prim == PRIM_UNWRAP) 
	                    ? unwrap_restriction(ss_fun)
			    : (EKIND(ss_fun) == GLOBAL_ID_E) 
                                ? get_restriction_type_tm(ss_fun)
		                : var_t(NULL));
  gen_g(UNWRAP_I);
  reset_frees(tt);
       
  /*----------------------------------------------------*
   * The UNWRAP_I instruction places some type in 	*
   * tt onto the type stack.  Generate code to unify	*
   * our tt with the type  that is on the type		*
   * stack.  If no useful code got generated, remove    *
   * any code that got generated.			*
   *----------------------------------------------------*/

  finger = finger_code_array();
  gen_unwrap_test_g(stack_type, tt, fun, &did_something);
  if(!did_something) {
    retract_code_to(finger);
    gen_g(POP_T_I);
  }
  drop_type(stack_type);
}

