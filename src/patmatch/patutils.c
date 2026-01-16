/****************************************************************
 * File:    patmatch/patutils.c
 * Purpose: Utility functions for pattern matching.
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

#include "../misc/misc.h"
#include "../utils/lists.h"
#include "../utils/strops.h"
#include "../exprs/expr.h"
#include "../patmatch/patmatch.h"
#include "../error/error.h"
#include "../unify/unify.h"
#include "../dcls/dcls.h"
#include "../tables/tables.h"
#include "../clstbl/cnstrlst.h"
#include "../classes/classes.h"
#include "../generate/prim.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/****************************************************************
 *			IS_SIMPLE_PATTERN			*
 ****************************************************************
 * Return true if e is a pattern that cannot fail when matched.	*
 ****************************************************************/

Boolean is_simple_pattern(EXPR *e)
{
  EXPR *sse;
  EXPR_TAG_TYPE kind;

  for(;;) {
    sse = skip_sames(e);
    kind = EKIND(sse);
    if(kind == PAT_VAR_E) return TRUE;
    if(kind == PAIR_E) {
      if(!is_simple_pattern(sse->E1)) return FALSE;
      e = sse->E2;
    }
    else return FALSE;
  }
}


/****************************************************************
 *			IS_FIXED_PAT				*
 ****************************************************************
 * Return TRUE if expression e has no pattern variables,	*
 * pattern functions or pattern constants, and is not an 	*
 * ordered pair expression.  Such expressions are treated as	*
 * constants in pattern matching.				*
 ****************************************************************/

Boolean is_fixed_pat(EXPR *pattern)
{
  EXPR_TAG_TYPE kind;
  EXPR* sspattern = skip_sames(pattern);

  /*-------------------------------------------*
   * Just in case somebody calls us with NULL. *
   *-------------------------------------------*/

  if(sspattern == NULL) return TRUE;

  /*--------------------------------------------------------------*
   * The pat field tells whether there are any pattern variables. *
   * If the pat field holds 1, then return FALSE, because there   *
   * are pattern variables.  Also return FALSE on a PAIR_E or     *
   * PAT_FUN_E (which will actually be a pattern constant) nodes. *
   *--------------------------------------------------------------*/

  kind = EKIND(sspattern);
  if(sspattern->pat || kind == PAIR_E || kind == PAT_FUN_E) return FALSE;

  /*---------------------------------------------------------------*
   * An application is not a fixed pattern if it is an application *
   * of a pattern function or a special pattern.		   *
   *---------------------------------------------------------------*/

  if(kind == APPLY_E) {
    EXPR*         patfun      = get_applied_fun(sspattern, FALSE);
    EXPR_TAG_TYPE patfun_kind = EKIND(patfun);

    if(patfun_kind == PAT_FUN_E) return FALSE;
    
    if(patfun_kind == SPECIAL_E) {
      int prim = sspattern->PRIMITIVE;
      return invert_prim_val(prim) == 0;
    }
  }

  /*-------------------------------------------------------------*
   * If this pattern has not looked like something that contains *
   * pattern components, say that it has none.			 *
   *-------------------------------------------------------------*/

  return TRUE;
}


/****************************************************************
 *			NOT_PAT					*
 ****************************************************************
 * Return true if pattern has no pattern variables or pattern   *
 * functions in it.  This is similar to is_fixed_pat, but 	*
 * pattern constants are not considered to be pattern functions *
 * here, and PAIR_E nodes are not significant here.		*
 ****************************************************************/

Boolean not_pat(EXPR *pattern)
{
  EXPR_TAG_TYPE kind;
  EXPR* sspattern = skip_sames(pattern);

  /*-------------------------------------------*
   * Just in case somebody calls us with NULL. *
   *-------------------------------------------*/

  if(sspattern == NULL) return TRUE;

  /*--------------------------------------------------------------*
   * The pat field tells whether there are any pattern variables. *
   * If the pat field holds 1, then return FALSE, because there   *
   * are pattern variables.  					  *
   *--------------------------------------------------------------*/

  kind = EKIND(sspattern);
  if(sspattern->pat) return FALSE;

  /*---------------------------------------------------------------*
   * At an application, check whether the function being applied   *
   * is a pattern function.					   *
   *---------------------------------------------------------------*/

  if(kind == APPLY_E) {
    EXPR *        patfun      = get_applied_fun(sspattern, FALSE);
    EXPR_TAG_TYPE patfun_kind = EKIND(patfun);

    if(patfun_kind == PAT_FUN_E) return FALSE;
    
    if(patfun_kind == SPECIAL_E) {
      int prim = sspattern->PRIMITIVE;
      return invert_prim_val(prim) == 0;
    }
  }

  /*-------------------------------------------------------------*
   * If this pattern has not looked like something that contains *
   * pattern components, say that it has none.			 *
   *-------------------------------------------------------------*/

  return TRUE;
}


/******************************************************************
 *			PAT_FUN_TAG				  *
 ******************************************************************
 * Return the constructor number associated with pattern function *
 * pf.  							  *
 *								  *
 * pat_ty is the type of the pattern of which pf is the pattern	  *
 * function.							  *
 *								  *
 * This will only work after pattern match substitution has	  *
 * been done, since it looks in the E3 field of the pattern       *
 * function node, and that is set during pattern match 		  *
 * translation.        						  *
 *								  *
 * Return -1 if no tag can be found.				  *
 ******************************************************************/

int pat_fun_tag(EXPR *pf, TYPE *pat_type)
{
  char *constr_name;
  int result;

  pf = skip_sames(pf);

# ifdef DEBUG
    if(trace_pat_complete > 1) {
      trace_t(121);
      print_expr(pf, 0);
    }
# endif

  /*------------------------------------------------------*
   * We need a pattern function with a non-null E3 field. *
   *------------------------------------------------------*/

  if(EKIND(pf) != PAT_FUN_E || pf->E3 == NULL) {
#   ifdef DEBUG
      if(trace_pat_complete > 1) trace_t(579);
#   endif

    return -1;
  }

  /*--------------------------------------*
   * We need a non-null constructor name. *
   *--------------------------------------*/

  constr_name = pf->E3->ETAGNAME;
  if(constr_name == NULL) {
#   ifdef DEBUG
      if(trace_pat_complete > 1) trace_t(580);
#   endif

    return -1;
  }

  /*------------------------------------------------------------*
   * If pf is a function, then it constructs something of its   *
   * codomain type.  If pf is not a function, then it constructs*
   * something of type pf->ty.					*
   *------------------------------------------------------------*/

  result = tag_name_to_tag_num(pat_type, constr_name);

# ifdef DEBUG
    if(trace_pat_complete > 1) trace_t(581, result, constr_name);
# endif

  return result;
}


