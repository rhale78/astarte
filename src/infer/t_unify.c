/**************************************************************
 * File:    infer/t_unify.c
 * Purpose: These functions support type inference by unifying
 *          the type of an expression with a given type.
 * Author:  Karl Abrahamson
 **************************************************************/

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
 * These functions unify the type of an expression with a particular	*
 * type.								*
 ************************************************************************/

#include "../misc/misc.h"
#include "../utils/lists.h"
#include "../utils/strops.h"
#include "../exprs/expr.h"
#include "../error/error.h"
#include "../classes/classes.h"
#include "../unify/unify.h"
#include "../tables/tables.h"
#include "../infer/infer.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

Boolean deferral_ok = TRUE;

/****************************************************************
 *			UNIFY_TYPE_TC				*
 ****************************************************************
 * See unify_type_g_tc.						*
 ****************************************************************/

Boolean unify_type_tc(EXPR *e, TYPE **t, TYPE *h, Boolean report)
{
  return unify_type_g_tc(e, t, h, report, NULL);
}


/****************************************************************
 *			UNIFY_TYPE_G_TC				*
 ****************************************************************
 * Unify the type of e with type *t. Hold onto h, via a		*
 * reference count, to avoid losing *t.  Report failure as an	*
 * error if report is true.					*
 *								*
 * Unifications involving overloaded ids are deferred, by	*
 * placing them in the deferral lists, provided global var	*
 * deferral_ok is true.  If deferral_ok is false, then an	*
 * error is reported when overloaded ids are encountered.	*
 *								*
 * Return true if succeeded, false if failed.  If report error,	*
 * report at err_ex if err_ex != NULL, and at e otherwise.	*
 ****************************************************************/

Boolean unify_type_g_tc(EXPR *e, TYPE **t, TYPE *h, Boolean report, 
			EXPR *err_ex)
{
  EXPR_TAG_TYPE e_kind;
  Boolean overloaded;
  EXPR* sse;

# ifdef DEBUG
    if(trace_infer) trace_t(305, e);
# endif

  if(e == NULL) return TRUE;
  sse    = skip_sames(e);
  e_kind = EKIND(sse);

  /*------------------------------------------*
   * e is overloaded if it is an OVERLOAD_E.  *
   *------------------------------------------*/

  overloaded = (e_kind == OVERLOAD_E);

  /*--------------------------*
   * Perform the unification. *
   *--------------------------*/

  if(!unify_u(&(e->ty), t, FALSE)) {

    /*-------------------------------------------------------------*
     * We get here if the unification failed.  Report an errror if *
     * report is true.  The error is a type_error.  It is reported *
     * for expression e normally, or for expression err_ex if	   *
     * err_exp is not NULL.  When reporting for err_ex, put the    *
     * type of e into err_ex before reporting.			   *
     *-------------------------------------------------------------*/

    if(report) {
      if(err_ex == NULL) type_error(e, *t);
      else {
	TYPE* tmpt = err_ex->ty;
	TYPE* holdt = *t;
	err_ex->ty = e->ty;
	type_error(err_ex, holdt);
        err_ex->ty = tmpt;
      }
    }

#   ifdef DEBUG
      if(trace_infer) trace_t(29, FALSE);
#   endif
    return FALSE;
  }

  /*-----------------------------------------------------*
   * We get here if the unification succeeded.           *
   * If not overloaded, then we are done.  Return true.  *
   *-----------------------------------------------------*/

  if(!overloaded) {
#   ifdef DEBUG
      if(trace_infer) trace_t(29, TRUE);
#   endif
    return TRUE;
  }

  /*-------------------------------------------------------------*
   * Defer overloads.  We will need to try each species from the *
   * global id table for them in handle_deferrals_tc.  		 *
   *-------------------------------------------------------------*/

# ifdef DEBUG
    if(trace_infer) {
      trace_t(309);
      print_expr(e, 1);
      fprintf(TRACE_FILE, " ");
      trace_ty(*t);
      fprintf(TRACE_FILE, " (%p)\n", t);
    }
# endif

  if(deferral_ok) {
    LIST_TAG_TYPE kind;
    HEAD_TYPE u;
    SET_LIST(unification_defer_exprs, expr_cons(e, unification_defer_exprs));
    if(*t == NULL_T) {
      u.stype = t;
      kind = STYPE_L;

      /*-----------------------------------------------------------*
       * We might need to hold onto a type here to avoid losing it *
       * later.	 Put h into unification_defer_holds.		   *
       *-----------------------------------------------------------*/

      if(h != NULL) push_type(unification_defer_holds, h);
    }
    else { /* *t != NULL */
      u.type = *t;
      kind = TYPE_L;
    }

    SET_LIST(unification_defer_types,
	     general_cons(u, unification_defer_types, kind));

#   ifdef DEBUG
      if(trace_infer) trace_t(29, TRUE);
#   endif

    return TRUE;
  }

  else { /* !deferral_ok */
    char* name = "non-id";
    if(e_kind == PAT_FUN_E || e_kind == GLOBAL_ID_E || e_kind == OVERLOAD_E) {
      name = sse->STR;
    }
    syntax_error1(LATE_DEFER_ERR, display_name(name), e->LINE_NUM);

#   ifdef DEBUG
      if(trace_infer) trace_t(29, FALSE);
#   endif

    return FALSE;
  }
}

