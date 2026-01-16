/****************************************************************
 * File:    patmatch/mode.c
 * Purpose: Handle modes of pattern functions
 * Author:  Karl Abrahamson
 ****************************************************************/

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
 * Part of determining whether a give pattern rule can be used in a	*
 * given context is checking that the modes are compatible.  This file	*
 * performs those checks.						*
 ************************************************************************/

#include <string.h>
#include "../misc/misc.h"
#include "../utils/lists.h"
#include "../utils/strops.h"
#include "../exprs/expr.h"
#include "../patmatch/patmatch.h"
#include "../error/error.h"
#include "../exprs/prtexpr.h"
#include "../standard/stdids.h"
#include "../unify/unify.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif


/****************************************************************
 *		        CONSTANTS_MATCH_PM			*
 ****************************************************************
 * Return true just when constant exprs c1 and c2 are the 	*
 * same constant.						*
 ****************************************************************/

PRIVATE Boolean constants_match_pm(EXPR *c1, EXPR *c2)
{
  if(c1->SCOPE == REAL_CONST || c2->SCOPE == REAL_CONST) {
    return real_consts_equal(c1->STR, c2->STR);
  }
  if(c1->SCOPE != c2->SCOPE) return FALSE;
  return strcmp(c1->STR, c2->STR) == 0;
}


/****************************************************************
 *			IS_CONST_CASTER				*
 ****************************************************************
 * is_const_caster(f) is true just when f is ratconst or 	*
 * natconst.							*
 ****************************************************************/

PRIVATE Boolean is_const_caster(EXPR *f)
{
  return is_id_p(f) && 
	 (f->STR == std_id[NATCONST_ID] || f->STR == std_id[RATCONST_ID]);
    
}


/****************************************************************
 *			FUNS_MATCH				*
 ****************************************************************
 * Return true if function names name1 and name2 match.         *
 * If they are equal, they match.  Also, we allow ratconst      *
 * to match natconst, so that constants of different kinds can  *
 * match one another.  (The constant 0 is replaced by		*
 * natconst(0), and the constant 0.0 is replaced by		*
 * ratconst(0.0).)						*
 ****************************************************************/

PRIVATE Boolean funs_match(char *name1, char *name2)
{
  if(name1 == name2) return TRUE;
  {register char* natconst = std_id[NATCONST_ID];
   register char* ratconst = std_id[RATCONST_ID];
   if(name1 == natconst && name2 == ratconst) return TRUE;
   if(name1 == ratconst && name2 == natconst) return TRUE;
   return FALSE;
  }
}


/****************************************************************
 *			MODE_MATCH_PM				*
 ****************************************************************
 * Return true if a pattern rule with mode rule_mode is  	*
 * consistent with an argument of mode arg_mode.  		*
 *								*
 * If check_fun is true, make sure the function being applied	*
 * at an application is the same in rule_mode and arg_mode, and *
 * that a constant identifier rule_mode is the same as		*
 * arg_mode.  If check_fun is false, suppress those tests.	*
 *								*
 * If using rule_mode would force a restrictive type binding    *
 * on arg_mode, then return TRUE, but set is_restrictive to 	*
 * TRUE.  If there are no restrictions, set is_restrictive to	*
 * FALSE.  The value of is_restrictive is undefined when FALSE  *
 * is returned.							*
 ****************************************************************/

Boolean mode_match_pm(EXPR *rule_mode, EXPR *arg_mode, Boolean check_fun,
		      Boolean *is_restrictive)
{
  EXPR_TAG_TYPE rule_kind, arg_kind;
  Boolean result;

  /*------------------------------------------------------------*
   * The default for is_restrictive is FALSE.  Note that	*
   * it is only necessary to check types at function		*
   * applications, since all other parts of patterns		*
   * will be tested there.  (The top level is an application	*
   * of the main pattern function.				*
   *------------------------------------------------------------*/

  *is_restrictive = FALSE;

  rule_mode = skip_sames(rule_mode);
  rule_kind = EKIND(rule_mode);
  arg_mode  = skip_sames(arg_mode);
  arg_kind  = EKIND(arg_mode);

# ifdef DEBUG
    if(trace_pm) {
      trace_t(411, check_fun, arg_mode, rule_kind);
      short_print_expr(TRACE_FILE,rule_mode);
      trace_t(412, arg_kind);
      short_print_expr(TRACE_FILE,arg_mode);
    }
# endif

  /*----------------------------------------------------------*
   * Case where rule_mode is ?x. Then rule_mode matches any   *
   * arg_mode.						      *
   *----------------------------------------------------------*/

  if(is_ordinary_pat_var(rule_mode)) {
    result = TRUE;
    goto out;
  }

  /*------------------------------------------------*
   **** Cases where rule_kind and arg_kind match ****
   *------------------------------------------------*/

  if(rule_kind == arg_kind) {

    /*-------------------------------------------------------------*
     * Case where rule_mode is a CONSTANT.  Use constants_match_pm *
     * to compare. 						   *
     *-------------------------------------------------------------*/

    if(rule_kind == CONST_E) {
      result = constants_match_pm(rule_mode, arg_mode);
      goto out;
    }

    /*---------------------------------------------------------------*
     * Case where rule_mode is a GLOBAL ID or a PATTERN CONSTANT.    *
     * Then rule_mode only matches itself, if we are testing for     *
     * matches, and matches anything if we are not testing.	     *
     *---------------------------------------------------------------*/

    if(rule_kind == GLOBAL_ID_E || rule_kind == PAT_FUN_E) {
      result = !check_fun || rule_mode->STR == arg_mode->STR;
      goto out;
    }

    /*----------------------------------------------------------*
     * Case where rule_mode is ?.  This rule_mode only matches  *
     * an actual mode that is also an anonymous pattern		*
     * variable.						*
     *----------------------------------------------------------*/

    if(rule_kind == PAT_VAR_E) {
      result = !is_ordinary_pat_var(arg_mode);
      goto out;
    }

    /*--------------------------------------------------*
     * Case where rule_mode is a PAIR.  Recursively	*
     * check the parts. 				*
     *--------------------------------------------------*/

    if(rule_kind == PAIR_E) {
      Boolean restr1, restr2;
      result = mode_match_pm(rule_mode->E1, arg_mode->E1, TRUE, &restr1) &&
               mode_match_pm(rule_mode->E2, arg_mode->E2, TRUE, &restr2);
      *is_restrictive = restr1 | restr2;
      goto out;
    }

    /*------------------------------------------------------------------*
     * Case where rule_mode is an APPLICATION.  Check the function	*
     * and the argument. 						*
     *------------------------------------------------------------------*/

    if(rule_kind == APPLY_E) {
      EXPR* rpf      = NULL;
      EXPR* apf      = NULL;
      EXPR* rule_fun = rule_mode->E1;
      EXPR* arg_fun  = arg_mode->E1;

      /*----------------------------------------------------------------*
       * Check the functions at the bottom of an application chain to   *
       * see if they match.						*
       *----------------------------------------------------------------*/

      if(check_fun) {
        rpf = get_applied_fun(rule_mode, TRUE);
        apf = get_applied_fun(arg_mode, TRUE);
        if(rpf == NULL || apf == NULL || !funs_match(rpf->STR, apf->STR)) {
	  result = FALSE;
	  goto out;
	}
      }

      /*----------------------*
       * Check the argument.  *
       *----------------------*/

      {Boolean restr1;
       result = mode_match_pm(rule_mode->E2, arg_mode->E2, TRUE, &restr1);
       if(restr1) *is_restrictive = TRUE;
      }

      /*-------------------------------------------------------------*
       * Check the function being applied (if the function is itself *
       * an application). 					     *
       *-------------------------------------------------------------*/ 

      if(result && EKIND(skip_sames(rule_fun)) == APPLY_E) {
	Boolean restr1;
        result = mode_match_pm(rule_fun, arg_fun, check_fun,&restr1);
        if(restr1) *is_restrictive = TRUE;
      }

      /*------------------------------------------------------------*
       * We must unify types of the functions.  		    *
       * If they cannot unify, then the modes do not match.  If     *
       * the unification results in a restrictive binding, then	    *
       * indicate so by setting *is_restrictive to true.	    *
       *							    *
       * Special case: If the functions are both natconst or	    *
       * ratconst, then allow the domain types to mismatch, and     *
       * only check the codomains.				    *
       *------------------------------------------------------------*/

      if(result) {
	int ov;
	if(is_const_caster(rpf) && is_const_caster(apf)) {
	  ov = half_overlap_u(arg_fun->ty->TY2, rule_fun->ty->TY2);
	}
        else ov = half_overlap_u(arg_fun->ty, rule_fun->ty);

        if(ov == DISJOINT_OV) result = FALSE;
        else if(ov == CONTAINS_OR_BAD_OV) {
 	  *is_restrictive = TRUE;
        }
      }

      goto out;
    }
  }

  /*-------------------------------------------------------------*
   **** Cases where rule_kind and arg_kind might be different ****
   *-------------------------------------------------------------*/

  /*--------------------------------------------------------------------*
   * A global id in rule_mode vs a pattern function in arg_mode, or 	*
   * the other way around.  Match if the names are the same, or if	*
   * we are not checking. Don't match others.				*
   *--------------------------------------------------------------------*/

  if(rule_kind == GLOBAL_ID_E || rule_kind == PAT_FUN_E) {
    if(arg_kind == GLOBAL_ID_E || arg_kind == PAT_FUN_E) {
      result = !check_fun || rule_mode->STR == arg_mode->STR;
    }
    else result = FALSE;
    goto out;
  }

  /*----------------------------------------------------------------*
   * A local id formal matches any actual that has no pattern vars. *
   *----------------------------------------------------------------*/

  if(rule_kind == LOCAL_ID_E) {
    result = not_pat(arg_mode);
    goto out;
  }

  /*------------------------*
   * Others -- don't match. *
   *------------------------*/

  result = FALSE;

 out:

# ifdef DEBUG
    if(trace_pm) trace_t(81, arg_mode, result, *is_restrictive);
# endif

  return result;
}

