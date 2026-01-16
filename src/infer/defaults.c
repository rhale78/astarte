/******************************************************************
 * File:    infer/defaults.c
 * Purpose: Handle defaults of unbound variables.
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
 * This file contains functions for defaulting unbound type and family  *
 * variables at the end of type inference.  The difficult thing here    *
 * is deciding which variables need to be defaulted, and which can	* 
 * remain unbound.							*
 *									*
 * The default information is not stored here.  It is stored in 	*
 * clstbl/classtbl.c.							*
 ************************************************************************/

#include "../misc/misc.h"
#include "../utils/hash.h"
#include "../utils/lists.h"
#include "../utils/strops.h"
#include "../alloc/allocate.h"
#include "../classes/classes.h"
#include "../standard/stdtypes.h"
#include "../exprs/expr.h"
#include "../tables/tables.h"
#include "../clstbl/dflttbl.h"
#include "../error/error.h"
#include "../unify/unify.h"
#include "../unify/constraint.h"
#include "../infer/infer.h"
#include "../generate/generate.h"
#include "../generate/prim.h"
#include "../patmatch/patmatch.h"
#include "../dcls/dcls.h"
#include "../parser/tokens.h"
#include "../parser/parser.h"
#include "../lexer/modes.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

PRIVATE void build_upper_bound_table(EXPR *ex);

PRIVATE TYPE_LIST *
get_runtimes(TYPE *t, EXPR *e, LIST *boundvars);

PRIVATE Boolean 
do_defaults_tc(TYPE **tt, EXPR *e, TYPE_LIST *rt_bound_vars, int wrn,
	       char *defining);

PRIVATE TYPE_LIST *
do_the_defaults (EXPR *e, TYPE_LIST *boundvars, char *defining, 
		 Boolean report);

/****************************************************************
 *			PUBLIC VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			default_bindings			*
 ****************************************************************
 * List default_bindings stores expressions whose types were    *
 * subject to default bindings.					*
 ****************************************************************/

EXPR_LIST* default_bindings = NIL;

/****************************************************************
 *			PRIVATE VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			there_are_runtimes			*
 ****************************************************************
 * there_are_runtimes is true if the current declaration has    *
 * dynamically bound variables, and is false otherwise.		*
 ****************************************************************/

PRIVATE Boolean there_are_runtimes;


/****************************************************************
 *			upper_bound_table			*
 ****************************************************************
 * upper_bound_table associates with each variable a list of 	*
 * its upper bound variables.  Members of the value list with	*
 * tag TYPE_L are direct upper bounds, and those with tag	*
 * TYPE1_L are indirect upper bounds.  U is a direct upper 	*
 * bound of V if U >= V is a constraint, and is an indirect	*
 * upper bound if U >= T is a constraint where T is a type	*
 * that contains V.						*
 *								*
 * The keys in upper_bound_table are not ref-counted, but the	*
 * val fields (lists) are.					*
 ****************************************************************/

PRIVATE HASH2_TABLE* upper_bound_table;

/****************************************************************
 *			SELECT_DEFAULT_TC			*
 ****************************************************************
 * Return an appropriate default for variable V, or indicate	*
 * that V should not be defaulted.  The choices are as follows,	*
 * with the first applicable one being chosen.			*
 *								*
 * 1.  lb    if V is a variable with a single maximal >= lower  *
 *           bound lb.						*
 *								*
 * 2.  NULL  if V is a variable that should not be defaulted	*
 *	     at compile time.  V should not be defaulted if it  *
 *	     has an upper bound or has more than one maximal	*
 *	     lower bound.					*
 *								*
 * 3.  d     if V is an ordinary variable that does not prefer 	*
 *	     a secondary default, or a primary variable, with a *
 *           default d given.  					*
 *								*
 * 4.  G     if V is a secondary variable G``a or an ordinary   *
 *	     variable G`x.					*
 *								*
 * 5. NULL   if none of the above rules apply.			*
 *								*
 * Set *dangerous to TRUE if the selected default is dangerous, *
 * and to FALSE otherwise.					*
 *								*
 * Parameters this_exp, rt_bound_vars and wrn are as for	*
 * do_defaults_tc, below.  Parameter defining is an identifier  *
 * in whose definition this default occurs.			*
 *								*
 * Global upper_bound_table is used to find upper bounds	*
 * of variables.						*
 ****************************************************************/

PRIVATE TYPE* 
select_default_tc(TYPE *V, Boolean *dangerous, EXPR *this_exp, 
		  TYPE_LIST *rt_bound_vars, int wrn, char *defining)
{
  TYPE_TAG_TYPE     V_kind = TKIND(V);
  CLASS_TABLE_CELL* V_ctc  = V->ctc;

  Boolean is_primary = IS_PRIMARY_VAR_T(V_kind);
  Boolean is_wrapped = IS_WRAP_VAR_T(V_kind);

  *dangerous = FALSE;

# ifdef DEBUG
    if(trace_infer > 1) {
      trace_t(183);
      trace_ty(V);
      tracenl();
    }
# endif

  /*---------*
   * Case 1. *
   *---------*/

  if(lower_bound_list(V) != NIL) {
    TYPE_LIST* V_lb;
    remove_redundant_constraints(V, TRUE);
    bump_list(V_lb = maximal_lower_bounds(lower_bound_list(V)));
    if(V_lb != NIL && V_lb->tail == NIL) {
      TYPE* result;

#     ifdef DEBUG
        if(trace_infer > 1) trace_t(148);
#     endif

      bump_type(result = V_lb->head.type);
      drop_list(V_lb);
      if(!do_defaults_tc(&result, this_exp, rt_bound_vars, wrn, defining)) {
	drop_type(result);
	return NULL;
      }
      else {
        result->ref_cnt--;
	return result;
      }
    }
    else drop_list(V_lb);
  }

  /*---------*
   * Case 2. *
   *---------*/

  {HASH2_CELLPTR h;
   HASH_KEY u;
   u.type = V;
   h      = locate_hash2(upper_bound_table, u, inthash((LONG) u.type), eq);

   if(h->key.type != NULL) {
#     ifdef DEBUG
        if(trace_infer > 1) trace_t(149);
#     endif
     return NULL;
   }
  }

  /*---------*
   * Case 3. *
   *---------*/

  if(is_primary || (!is_wrapped && !V->PREFER_SECONDARY)) {
    TYPE* result = get_default_tm(V, V_ctc, dangerous);
    if(result != NULL_T) {
#     ifdef DEBUG
        if(trace_infer > 1) trace_t(152);
#     endif
      return result;
    }
  }
  *dangerous = FALSE;
  
  /*---------*
   * Case 4. *
   *---------*/

  if(!is_primary) {
#     ifdef DEBUG
        if(trace_infer > 1) trace_t(153);
#     endif
    return wrap_tf(V_ctc);
  }

  /*---------*
   * Case 5. *
   *---------*/

# ifdef DEBUG
    if(trace_infer > 1) trace_t(154);
# endif

  return NULL;
}


/****************************************************************
 *			DO_DEFAULTS_TC				*
 ****************************************************************
 * Bind type and family variables in *tt to their defaults	*
 * where appropriate, and report errors where no default is	*
 * available.  In the event of error, this_exp is used as the	*
 * expression whose type could not be bound.			*
 *								*
 * If a default binding is done, record this_exp in list 	*
 * default_bindings. 						*
 *								*
 * All default bindings are recorded in the unification binding *
 * list, so that they can be undone using undo_bindings_u.	*
 *								*
 * Return true on success, false on failure.			*
 *								*
 * Warnings are controlled by parameter wrn.			*
 *								*
 *   wrn	action						*
 *    0 	Never warn about doing defaults.		*
 *    1		Warn only about dangerous defaults.		*
 *    2		Warn about all defaults.			*
 ****************************************************************/

PRIVATE Boolean 
do_defaults_tc(TYPE **tt, EXPR *this_exp, TYPE_LIST *rt_bound_vars, 
	       int wrn, char *defining)
{
  TYPE_TAG_TYPE t_kind;
  TYPE* t = *tt;

# ifdef DEBUG
    if(trace_infer > 2) {
      trace_t(182);
      trace_ty(t);
      tracenl();
    }
# endif

  /*------------------------------------------------------*
   * Check for `? (null ptr).  Bind it to a new variable. *
   *------------------------------------------------------*/

  if(t == NULL_T) {
    bump_type(t = var_t(NULL));
    *tt = t;
  }
    
  /*----------------------*
   * Here, t is not null. *
   *----------------------*/

  IN_PLACE_FIND_U_NONNULL(t);
  t_kind = TKIND(t);
  if(IS_STRUCTURED_T(t_kind)) {
    return  do_defaults_tc(&(t->TY1), this_exp, rt_bound_vars, wrn, defining) 
         && do_defaults_tc(&(t->TY2), this_exp, rt_bound_vars, wrn, defining);
  }

  else if(IS_VAR_T(t_kind)) {
    TYPE *dfault;
    Boolean dangerous;

    /*------------------------------------------------------*
     * Check for a variable that does not need to be bound. *
     *------------------------------------------------------*/

    if((class_mem(t, rt_bound_vars)) != NULL ||
       (class_mem(t, glob_bound_vars)) != NULL) {
      return TRUE;
    }

    /*--------------------------------------------------*
     * Check for variable that can be bound to default. *
     *--------------------------------------------------*/

#   ifdef DEBUG
      if(trace_infer) {
	trace_t(184);
	trace_ty(t);	
	fprintf(TRACE_FILE, " at %p\n", t);
      }
#   endif

    dfault = select_default_tc(t, &dangerous, this_exp, 
			       rt_bound_vars, wrn, defining);

    /*----------------------------------------------------------*
     * If no default is available, then this variable must be	*
     * bound to a default at run time.  If this is a primary	*
     * variable without a given default, then that will be	*
     * impossible, so insist on a tag now.			*
     *----------------------------------------------------------*/

    if(dfault == NULL_T) {
      if(IS_PRIMARY_VAR_T(t_kind) && 
	 get_default_tm(t, t->ctc, &dangerous) == NULL) {
	unbound_tyvar_error(this_exp, t, defining);
	return FALSE;
      }
      else return TRUE;  /* Variable will be defaulted at run time. */
    }

    /*-------------------------------------------*
     * If a default is available, then bind now. *
     *-------------------------------------------*/

    else {
      if(!UNIFY(t, dfault, TRUE)) {
	unbound_tyvar_error(this_exp, t, defining);

# 	ifdef DEBUG
	  if(trace) {
	    trace_t(186);
	    print_expr(this_exp,1);
	  }
# 	endif

        return FALSE;
      }
      if(dangerous && wrn > 0) {
	dangerous_default_warn(this_exp, t, dfault, defining);
      }

      if(wrn == 2) warne(DEFAULT_ERR, defining, this_exp);

      if(!int_member((LONG) this_exp, default_bindings)) {
	SET_LIST(default_bindings, expr_cons(this_exp, default_bindings));
      }

#     ifdef DEBUG
	if(trace_infer) {
	  trace_t(185, t);
	  trace_ty(t); 
	  tracenl();
	}
#     endif

      return TRUE;
    }
  }

  else {
    return TRUE;
  }
}


/****************************************************************
 *			DO_ALL_DEFAULTS_TC			*
 ****************************************************************
 * Do all of the default bindings for declaration e.  Also 	*
 * detect and manage run-time bound variables in e.  		*
 *								*
 * MODE is the mode of the declaration.  It is needed to	*
 * know whether this is a definition of an irregular function,	*
 * since that affects default handling.	 It is a safe pointer:  *
 * it does not live longer than this function call.		*
 *								*
 * If REPORT is true, report dangerous defaults.		*
 * 								*
 * All default bindings are recorded in the unification binding *
 * list, so that they can be undone using undo_bindings_u.	*
 *								*
 * PRECONDITION: Variable glob_bound_vars must be set to a list *
 * of the globally bound variables before entering this		*
 * function.							*
 ****************************************************************/

PRIVATE void 
do_all_defaults_tc(EXPR *e, MODE_TYPE *mode, Boolean report)
{
  EXPR_TAG_TYPE e_kind;

  e      = skip_sames(e);
  e_kind = EKIND(e);

  /*---------------------------------*
   * Initialize for do_the_defaults. *
   *---------------------------------*/

  clear_offsets(e);
  there_are_runtimes = FALSE;

  /*------------------------------------------------------------*
   * Suppose this declaration has the form 			*
   *								*
   *   Let{irregular} f = g.					*
   *								*
   * If g is an identifier, then do not do any defaults. 	*
   *								*
   * If g is a function expression, then mark it as irregular.	*
   *------------------------------------------------------------*/

  if(has_mode(mode, IRREGULAR_MODE) &&
     (e_kind == LET_E || e_kind == DEFINE_E)) {
    EXPR* sse2 = skip_sames(e->E2);
    EXPR_TAG_TYPE sse2_kind = EKIND(sse2);
    if(sse2_kind == GLOBAL_ID_E || sse2_kind == OVERLOAD_E) {
      return;
    }
    if(sse2_kind == FUNCTION_E) {
      sse2->IRREGULAR_FUN = 1;
    }
  }

# ifdef DEBUG
    if(trace_infer) {
      if(trace_infer > 1 && trace_exprs) {
        trace_t(187); 
        print_expr(e, 0);
      }
      trace_t(188, toint(e_kind));
      if(trace_infer > 1) print_glob_bound_vars(NULL);
    }
# endif

  /*-----------------------*
   * Do the defaults of e. *
   *-----------------------*/

  {char* defining = 
          (e_kind == LET_E || e_kind == DEFINE_E) 
	    ? skip_sames(e->E1)->STR
	    : "(none)";
   drop_list(do_the_defaults(e, NIL, defining, report));
  }

# ifdef DEBUG
    if(trace_infer) trace_t(189);
    if(trace_infer > 1) {
      trace_t(190);
      print_expr(e, 0);
    }
# endif

}


/****************************************************************
 *			DO_THE_DEFAULTS				*
 ****************************************************************
 * do_the_defaults(e,boundvars,DEFINING) defaults type/family 	*
 * variable bindings in e, and returns a list of all 		*
 * type/family vars in e that are dynamically bound by e,	*
 * assuming that vars in boundvars are already			*
 * bound.  Vars in boundvars are not included in the list	*
 * returned by do_the_defaults.  				*
 *								*
 * DEFINING is an identifier that we are currently defining.    *
 * It is used in warnings.					*
 *								*
 * If REPORT is true, report dangerous defaults.		*
 ****************************************************************/

PRIVATE TYPE_LIST* 
do_the_defaults(EXPR *e, TYPE_LIST *boundvars, char *defining, Boolean report)
{
  EXPR_TAG_TYPE kind;
  TYPE_LIST *bv1 = NULL, *bv2, *result;
  EXPR *e1, *e2;

  for(;;) { /* For tail recursion */

    if(e == NULL) return NIL;

    kind   = EKIND(e);
    e1     = e->E1;   /* These are not used in all cases, but it is safe */
    e2     = e->E2;   /* to get them here.			         */
    result = NIL;

#   ifdef DEBUG
      if(trace_infer > 1) {
	trace_t(191, e, expr_kind_name[kind]);
	print_str_list_nl(boundvars);
	if(kind == FUNCTION_E && e->IRREGULAR_FUN) {
	  trace_t(260);
	}
      }
#   endif

    switch(kind) {
      case TRY_E:

        /*--------------------------------------------------------------*
         * Try e1 then e2 else e3 %Try. 				*
         *								*
	 * The variables that are dynamically bound in e1 are only	*
         * bound in e2, not in e3.  So process e3 using the original	*
	 * boundvars list.						*
	 *								*
	 * The variables that are dynamically bound by this expression	*
	 * are those that are bound by both e1/e2 and e3.		*
	 *--------------------------------------------------------------*/

        {TYPE_LIST *bv3, *bv1p, *bv12;
         bump_list(bv1    = do_the_defaults(e1, boundvars, defining, report));

	 bump_list(bv1p   = append(bv1, boundvars));
	 bump_list(bv2    = do_the_defaults(e2, bv1p, defining, report));
	 bump_list(bv3    = do_the_defaults(e->E3, boundvars, defining,
					    report));
	 bump_list(bv12   = append(bv1, bv2));
	 bump_list(result = intersect_type_lists(bv12, bv3));
	 drop_list(bv1);
	 drop_list(bv2);
 	 drop_list(bv3);
	 drop_list(bv1p);
	 drop_list(bv12);
	 goto out;
	}

      case IF_E:

        /*--------------------------------------------------------------*
         * If e1 then e2 else e3 %If. 					*
         *								*
	 * The variables that are dynamically bound in e1 are		*
         * bound in e2 and in e3.  					*
	 *								*
	 * The variables that are dynamically bound by this expression	*
	 * are those that are bound by e1 or by both e2 and e3.		*
	 *--------------------------------------------------------------*/

	{TYPE_LIST *bv3, *bv1p, *bv23;
	 bump_list(bv1    = do_the_defaults(e1, boundvars, defining, report));
	 bump_list(bv1p   = append(bv1, boundvars));
	 bump_list(bv2    = do_the_defaults(e2, bv1p, defining, report));
	 bump_list(bv3    = do_the_defaults(e->E3, bv1p, defining, report));
	 bump_list(bv23   = intersect_type_lists(bv2, bv3));
	 bump_list(result = append(bv1, bv23));
	 drop_list(bv1);
	 drop_list(bv2);
         drop_list(bv3);
	 drop_list(bv1p);
	 drop_list(bv23);
         goto out;
        }

      case SAME_E:
      case RECUR_E:
      case EXECUTE_E:
      case SINGLE_E:
      case OPEN_E:

	/*-------------------*
	 * Tail recur on e1. *
	 *-------------------*/

	e = e1;
	break;
	
      case LET_E:

	/*----------------------------------------------*
	 * Let e1 = e2 %Let.	 			*
         *			 			*
	 * Tail recur on e2.  But note that we are now  *
	 * defining e1.					*
	 *----------------------------------------------*/

	defining = skip_sames(e1)->STR;
	e = e2;
	break;

      case PAIR_E:

	/*--------------------------------------------------------------*
	 * Expression (e1,e2).  Do defaults in e1 and e2.  Bindings	*
	 * done in one are invisible to the other.			*
	 *								*
	 * Expression (e1,e2) does all bindings done by e1 or by e2.	*
	 *--------------------------------------------------------------*/

	bump_list(bv1    = do_the_defaults(e1, boundvars, defining, report));
	bump_list(bv2    = do_the_defaults(e2, boundvars, defining, report));
	bump_list(result = append(bv1, bv2));
	drop_list(bv1);
	drop_list(bv2);
	goto out;

      case SEQUENCE_E:
      case WHERE_E:
      case TRAP_E:

	/*------------------------------------------------------*
	 * SEQUENCE_E(e1,e2) calls for computing e1, splitting  *
	 * it, and then computing e2.  Do the defaults in e1, 	*
	 * then do those in e2 using any dynamic bindings done	*
	 * in e1.						*
	 *							*
	 * The dynamic bindings of SEQUENCE_E(e1,e2) are those 	*
	 * done either by e1 or by e2.				*
	 *							*
	 * Trap e1 then e2 %Trap and (e1 where e2) are handled	*
	 * similarly in terms of defaults.			*
	 *------------------------------------------------------*/

	{TYPE_LIST *bv1p;
	 bump_list(bv1    = do_the_defaults(e1, boundvars, defining, report));
	 bump_list(bv1p   = append(bv1, boundvars));
	 bump_list(bv2    = do_the_defaults(e2, bv1p, defining, report));
	 bump_list(result = append(bv1, bv2));
	 drop_list(bv1);
	 drop_list(bv2);
	 drop_list(bv1p);
	 goto out;
        }

      case TEST_E:
      case LAZY_BOOL_E:

	/*------------------------------------------------------*
	 * {e1 else e2} is handled by doing defaults in e1, then*
	 * doing defaults in e2 using dynamic bindings done in	*
	 * e1.  However, since e2 is only executed when failing,*
	 * the dynamic bindings of {e1 else e2} are those that	*
	 * are done by e1.					*
	 *							*
	 * e1 _or_ e2 and e1 _and_ e2 are handled similarly	*
	 * since we don't know that e2 will be executed at all.	*
	 *------------------------------------------------------*/

	{TYPE_LIST *bv1p;
	 bump_list(bv1  = do_the_defaults(e1, boundvars, defining, report));
	 bump_list(bv1p = append(bv1, boundvars));
	 bump_list(bv2  = do_the_defaults(e2, bv1p, defining, report));
	 result = bv1;     /* Inherits ref cnt. */
	 drop_list(bv1p);
	 drop_list(bv2);
	 goto out;
	}

      case DEFINE_E:

	/*------------------------------------------------------*
	 * Define x = e2 %Define is handled by doing defaults	*
	 * in e1, but in the environment of the define, which	*
	 * is different from its surrounding environment.	*
	 *							*
	 * No dynamic bindings are exported, since they are	*
	 * hidden inside the separate environment.		*
	 *------------------------------------------------------*/

	bump_list(bv1 = do_the_defaults(e2, boundvars, defining, report));
	result = NIL;
	drop_list(bv1);
	goto out;

      case LAZY_LIST_E:

	/*------------------------------------------------------*
	 * [:e1:] is handled by doing defaults in e1. But the	*
         * environment of [:e1:] is different from 		*
	 * its surrounding environment.				*
	 *							*
	 * No dynamic bindings are exported, since they are	*
	 * hidden inside the separate environment.		*
	 *------------------------------------------------------*/

	bump_list(bv1 = do_the_defaults(e1, boundvars, defining, report));
	result = NIL;
	drop_list(bv1);
	goto out;

      case AWAIT_E:

	/*------------------------------------------------------*
	 * Await e1 then e2 %Await is handled by doing defaults	*
	 * in e1, then those in e2.  In the case of e2, the	*
 	 * environment is changed.				*
	 *							*
 	 * Only the dynamic bindings done in a are exported, 	*
	 * since those done in b are hidden in the separate	*
	 * environment.						*
	 *------------------------------------------------------*/

	{TYPE_LIST *bv1p;
         bump_list(bv1  = do_the_defaults(e1, boundvars, defining, report));
	 bump_list(bv1p = append(bv1, boundvars));
	 bump_list(bv2  = do_the_defaults(e2, bv1p, defining, report));
	 result = bv1;
	 drop_list(bv2);
	 drop_list(bv1p);
	 goto out;
	}

      case STREAM_E:

	/*--------------------------------------------------------------*
	 * Stream e1 then e2 %Stream or					*
	 * Mix e1 with e2 %Mix						*
	 *								*
	 * Here, e1 and e2 are handled independently.  The exported	*
	 * bindings are those done by both e1 and e2, since a 		*
	 * given thread might have only one or the other.		*
         *--------------------------------------------------------------*/

        bump_list(bv1    = do_the_defaults(e1, boundvars, defining, report));
	bump_list(bv2    = do_the_defaults(e2, boundvars, defining, report));
	bump_list(result = intersect_type_lists(bv1, bv2));
	drop_list(bv1);
	drop_list(bv2);
	goto out;

      case MATCH_E:

	/*--------------------------------------------------------------*
	 * Match e1 = e2 %Match						*
	 *								*
	 * Match expressions have been eliminated by the time we do	*
	 * defaults.  The following code is left in, however.  It was	*
	 * in effect in an earlier version, when defaults were done	*
	 * earlier.							*
	 *								*
	 * We must compute the target e2 first, then do defaults in	*
	 * the pattern e1, with bindings done in the target in effect,	*
	 * since that is how computation goes.				*
         *--------------------------------------------------------------*/

	{LIST *bv1p;
	 bump_list(bv1    = do_the_defaults(e2, boundvars, defining, report));
	 bump_list(bv1p   = append(bv1, boundvars));
	 bump_list(bv2    = do_the_defaults(e1, bv1p, defining, report));
	 bump_list(result = append(bv1, bv2));
	 drop_list(bv1);
	 drop_list(bv1p);
	 drop_list(bv2);
	 goto out;
	}

      case FUNCTION_E:

	/*--------------------------------------------------------------*
	 * Expression (e1 => e2)					*
	 *								*
 	 * When doing defaults, we have already replaced the pattern	*
	 * of every function expression by a STACK expression, of	*
	 * kind SPECIAL_E, unless we are importing a definition, in	*
	 * which case such processing might have been suppressed.	*
	 *								*
	 * Do defaults in e1 (since it is sitting on the stack) and then*
	 * defaults in e2.  Note that e1 cannot perform any bindings, 	*
	 * since it is just a STACK expression.	 			*
	 *								*
 	 * If this function is irregular, then bindings done in a will	*
	 * become available when the result type of this function is	*
	 * obtained.  So export the bindings in e2 in that case.	*
	 *--------------------------------------------------------------*/

	{Boolean irregular;

	 if(EKIND(e1) != SPECIAL_E && main_context != IMPORT_CX) die(14);
	 bump_list(bv1 = do_the_defaults(e1, boundvars, defining, report));
	 bump_list(bv2 = do_the_defaults(e2, boundvars, defining, report));
	 irregular = e->IRREGULAR_FUN;
	 bump_list(result = (irregular) ? bv2 : NIL);
	 drop_list(bv1);
	 drop_list(bv2);
	 goto out;
	}

      case LOOP_E:

	/*--------------------------------------------------------------*
	 * Expression Loop p = a e2 %Loop where e2 consists		*
	 * of the cases in the loop, and e1 is expression		*
	 * Match p = a %Match, or is NULL if (p = a) is omitted.	*
	 *								*
	 * Do defaults on Match p = a %Match, then on e2.  Export 	*
	 * bindings that are done by either one.  			*
	 * (a is always computed.)					*
	 *--------------------------------------------------------------*/

	{TYPE_LIST *bv1p;

	 if(e1 != NULL) {
	   bump_list(bv1  = do_the_defaults(e1, boundvars, defining, report));
	   bump_list(bv1p = append(bv1, boundvars));
	 }
	 else {
	   bump_list(bv1p = boundvars);
	 }
	 bump_list(bv2    = do_the_defaults(e2, bv1p, defining, report));
	 bump_list(result = (e1 == NULL) ? bv2 : append(bv1, bv2));
	 drop_list(bv1p);
	 drop_list(bv2);
	 goto out;
	}

      case FOR_E:

	/*------------------------------------------------------*
	 * Expression For e1 from e2 do e3 %For			*
	 *							*
	 * The order of computation is e2, then e1, then e3.	*
	 * Do defaults in that order, exporting dynamic		*
	 * bindings from each to the next.			*
	 *------------------------------------------------------*/

	{TYPE_LIST *bv3, *bv2p, *bv12p, *bv12;

	 bump_list(bv2    = do_the_defaults(e2, boundvars, defining, report));
	 bump_list(bv2p   = append(bv2, boundvars));
	 bump_list(bv1    = do_the_defaults(e1, bv2p, defining, report));
	 bump_list(bv12p  = append(bv1, bv2p));
	 bump_list(bv3    = do_the_defaults(e->E3, bv12p, defining, report));
	 bump_list(bv12   = append(bv1, bv2));
	 bump_list(result = append(bv3, bv12));
	 drop_list(bv1);
	 drop_list(bv2);
	 drop_list(bv3);
	 drop_list(bv12);
	 drop_list(bv2p);
	 drop_list(bv12p);
	 goto out;
	}

      case APPLY_E:

	/*--------------------------------------------------------------*
	 * Expression (e1)(e2).						*
	 *								*
	 * There are several cases to deal with, depending on the	*
	 * nature of e1.						*
	 *--------------------------------------------------------------*/

	{EXPR *sse1;
	 TYPE_LIST *bv1p;
	 TYPE *sse1_ty;

	 sse1      = skip_sames(e1);
         sse1_ty   = find_u(sse1->ty);

	 /*-------------------------------------------------*
	  * Check for application of an irregular function. *
 	  * Defaults should not be done on its codomain,    *
	  * since unbound variables there will be bound	    *
	  * dynamically.				    *
	  *-------------------------------------------------*/

	 if(is_irregular_fun(sse1)) {

	   bump_list(bv2  = do_the_defaults(e2, boundvars, defining, report));
	   bump_list(bv1p = append(bv2, boundvars));

	   /*-----------------------------------------------*
	    * We need to get the variables that will be     *
	    * bound by this irregular function.		    *
	    * If we haven't already done this one, then do  *
	    * it now. 					    *
	    *-----------------------------------------------*/

	   if(sse1->done == 0) {
	     bump_list(bv1 = get_runtimes(sse1_ty->TY2, sse1, bv1p));

#            ifdef DEBUG
	       if(trace_infer) {
		 trace_t(261, sse1);
		 print_str_list_nl(bv1);
	       }
#            endif

	     sse1->done = 1;
	   }
	   else {
#            ifdef DEBUG
	       if(trace_infer > 1) {
		 trace_t(262, sse1->STR);
	       }
#            endif
	     bv1 = NIL;
	   }
	 }

         /*------------------------------------------------------*
          * A function will be generated after its argument.     *
	  * (We want to permit run-time type bindings in the	 *
	  * argument to be seen when fetching the function. 	 *
          *------------------------------------------------------*/

	 else if(!is_hermit_type(sse1->ty)) {
	   bump_list(bv2  = do_the_defaults(e2, boundvars, defining, report));
	   bump_list(bv1p = append(bv2, boundvars));
	   bump_list(bv1  = do_the_defaults(e1, bv1p, defining, report));
	 }

	 /*-------------------------------------*
	  * When e1 has type (), do e1 first.	*
	  *-------------------------------------*/

         else {
	   bump_list(bv1  = do_the_defaults(e1, boundvars, defining, report));
	   bump_list(bv1p = append(bv1, boundvars));
	   bump_list(bv2  = do_the_defaults(e2, bv1p, defining, report));
	 }

	 /*----------------------------------------*
	  * Shared code for all three cases above. *
	  *----------------------------------------*/

	 bump_list(result = append(bv1, bv2));
	 drop_list(bv1);
	 drop_list(bv2);
	 drop_list(bv1p);
	 goto out;
        }

      case EXPAND_E:
        bump_list(bv1 = do_the_defaults(e1, boundvars, defining, report));
	drop_list(bv1);
	bump_list(bv1 = do_the_defaults(e2, boundvars, defining, report));
	drop_list(bv1);
	return NIL;

      case PAT_DCL_E:
        {EXPR *sse2, *rule;

	 sse2 = skip_sames(e2);
	 if(EKIND(sse2) != PAT_RULE_E) {
	   bump_list(bv1 = do_the_defaults(e2, boundvars, defining, report));
	   drop_list(bv1);
	 }
	 else {
	   for(rule = sse2; rule != NULL; rule = skip_sames(rule->E1)) {
	     bump_list(bv1 = do_the_defaults(rule->E3, boundvars, defining,
					     report));
	     drop_list(bv1);
	   }
	 }
         return NIL;
        }

      case PAT_RULE_E: 

	/*------------------------------*
         * These should not occur here. *
	 *------------------------------*/
        die(15);

      case CONST_E:

	/*--------------------------------------------------------------*
	 * A fail constant can be irregular.  It can have any type	*
	 * at all, and so should not insist on binding its variables.	*
	 * Bindings will never really be exported, since the expression *
	 * will kill the thread that executes it, but we handle the	*
	 * dynamic bindings anyway just to keep everybody happy.	*
	 *--------------------------------------------------------------*/

	if(e->SCOPE == FAIL_CONST) {
	  bump_list(bv1 = get_runtimes(e->ty, e, boundvars));
	  bump_list(result = append(bv1, boundvars));
	  drop_list(bv1);
	}
	goto out;

      case SPECIAL_E:
	if(e->PRIMITIVE == PRIM_SPECIES) {
	  e = e1;
	  break;
	}
	else goto out;

      default: 
	goto out;
    } /* end switch(kind) */
  } /* end for */

 out:

# ifdef DEBUG
    if(trace_infer > 1) {
      trace_t(263, e, expr_kind_name[kind]);
      print_str_list_nl(result);
      trace_t(264);
      print_str_list_nl(boundvars);
    }
# endif

 /*------------------------------------------------------*
  * Now that we have done defaults on the subexpressions *
  * of e, do defaults on e itself.			 *
  *------------------------------------------------------*/

  {int wrn = 0;
   if(report) {
     if(kind == SPECIAL_E) wrn = 0;
     else if(is_irregular_fun(e)) wrn = 2;
     else wrn = 1;
   }

   bump_list(bv1 = append(result, boundvars));
   do_defaults_tc(&(e->ty), e, bv1, wrn, defining);
   drop_list(bv1);
  }

  /*------------------------------------------------------------*
   * When a global id is fetched, dynamic bindings will be	*
   * done if necessary, causing all of the variables in its	*
   * type to become bound. Any that have not already been 	*
   * defaulted should be recognized as bound now.  If e 	*
   * is irregular, then only variables in its domain become	*
   * bound.							*
   *------------------------------------------------------------*/

  if(kind == GLOBAL_ID_E) {
    TYPE_LIST *bvars;
    TYPE* ty = is_irregular_fun(e) ? e->ty->TY1 : e->ty;
    bump_list(bvars = append(boundvars, glob_bound_vars));
    SET_LIST(result, copy_vars_to_list_t(ty, result, bvars));
    drop_list(bvars);
    if(result != NIL) result->ref_cnt--;
  }

  /*------------------------------------------------------------*
   * A non-global id has its done bit marked if it binds any	*
   * variables dynamically.  This is used by genexec.c to	*
   * know ahead of time where the bindings will occur.		*
   *------------------------------------------------------------*/

  else if(result != NIL) {
    e->done = 1;
    result->ref_cnt--;
  }

  return result;
}


/********************************************************
 *			PROCESS_DEFAULTS_TC		*
 ********************************************************
 * This is the function to call to perform default	*
 * bindings.  It does all defaults.			*
 *							*
 * This function does a show if SHOWIT is true and a	*
 * show has been requested.				*
 *							*
 * Dangerous defaults are reported if REPORT is true.	*
 *							*
 * DEF is the declaration to be processed, and 		*
 * MODE is the mode of the declaration.  MODE is a safe *
 * pointer: it does not live longer than this function  *
 * call.						*
 *							*
 * Before calling this, glob_bound_vars must have	*
 * been set up.						*
 *							*
 * The return value is TRUE if default processing	*
 * is successful, and FALSE if not.			*
 ********************************************************/

void process_defaults_tc(EXPR *def, MODE_TYPE *mode, Boolean showit,
			 Boolean report)
{
  build_upper_bound_table(def);
  do_all_defaults_tc(def, mode, report);
  if(!local_error_occurred) {
    if(showit && (show_types || force_show)) do_show_types(def);
  }
  SET_LIST(default_bindings, NIL);
  scan_hash2(upper_bound_table, drop_hash_list);
}


/****************************************************************
 *			UPDATE_UPPER_BOUND_TABLE		*
 ****************************************************************
 * update_upper_bound_table adds all members of d_ubs to the	*
 * list of direct upper bounds for all variables in type t, and *
 * adds all members of i_ubs to the list of indirect upper 	*
 * bounds for all variables in t.  It also adds upper bounds	*
 * that are consequences of constraints that occur in t itself.	*
 ****************************************************************/

PRIVATE void 
update_upper_bound_table(TYPE *t, TYPE_LIST *d_ubs, TYPE_LIST *i_ubs)
{
  TYPE_TAG_TYPE t_kind;

  t      = find_u(t);
  if(t == NULL) return;
  t_kind = TKIND(t);

# ifdef DEBUG
    if(trace_infer > 2) {
      trace_t(235);
      trace_ty(t); tracenl();
      trace_t(236);
      print_type_list_without_constraints(d_ubs);
      tracenl();
      print_type_list_without_constraints(i_ubs);
      tracenl();
    }
# endif

  if(IS_STRUCTURED_T(t_kind)) {
    if(d_ubs == NIL) {
      update_upper_bound_table(t->TY1, NIL, i_ubs);
      update_upper_bound_table(t->TY2, NIL, i_ubs);
    }
    else {
      TYPE_LIST *new_iubs;
      bump_list(new_iubs = append_without_dups(d_ubs, i_ubs));
      update_upper_bound_table(t->TY1, NIL, new_iubs);
      update_upper_bound_table(t->TY2, NIL, new_iubs);
      drop_list(new_iubs);
    }
  }

  else if(IS_VAR_T(t_kind) && !t->seen) {
    TYPE_LIST *new_ubs, *t_lb, *p;
    HASH2_CELLPTR h;
    HASH_KEY u;

    mark_seen(t);

    /*-----------------------------------------------------------*
     * Add all members of d_ubs and i_ubs to the upper bounds of *
     * variable t. The list tag is TYPE_L for a direct upper     *
     * bound and TYPE1_L for an indirect upper bound.		 *
     *-----------------------------------------------------------*/

    if(d_ubs != NIL || i_ubs != NIL) {
      u.type = t;
      h      = insert_loc_hash2(&upper_bound_table, u, 
			        inthash((LONG) u.type), eq);
      if(h->key.type == NULL) {
        h->key.type = u.type;
        h->val.list = NIL;
      }
      bump_list(new_ubs = h->val.list);
      for(p = d_ubs; p != NIL; p = p->tail) {
        if(!type_mem(p->head.type, new_ubs, TYPE_L)) {
          SET_LIST(new_ubs, type_cons(p->head.type, new_ubs));
	}
      }
      for(p = i_ubs; p != NIL; p = p->tail) {
        if(!type_mem(p->head.type, new_ubs, 0)) {
          HEAD_TYPE hd;
	  hd.type = p->head.type;
          SET_LIST(new_ubs, general_cons(hd, new_ubs, TYPE1_L));
	}
      }

#     ifdef DEBUG
        if(trace_infer > 2) {
	  trace_t(239, t);
	  print_type_list_without_constraints(new_ubs);
	  tracenl();
	}
#     endif
      SET_LIST(h->val.list, new_ubs);
      drop_list(new_ubs);
    }

    /*----------------------------------------------------------*
     * Add t to d_ubs if it is not already there, and install	*
     * upper bounds on the lower bounds of t.			*
     *----------------------------------------------------------*/

    t_lb = lower_bound_list(t);
    if(t_lb != NIL) {
      bump_list(new_ubs = d_ubs);
      if(!type_mem(t, new_ubs, 0)) {
	SET_LIST(new_ubs, type_cons(t, new_ubs));
      }
      for(p = t_lb; p != NIL; p = p->tail) {
        if(LKIND(p) == TYPE_L) {
          update_upper_bound_table(p->head.type, new_ubs, i_ubs);
        }
      }
      drop_list(new_ubs);
    }
  }

# ifdef DEBUG
    if(trace_infer > 2) {
      trace_t(240);
      trace_ty(t); tracenl();
    }
# endif
}


/********************************************************
 *		BUILD_UPPER_BOUND_TABLE			*
 ********************************************************
 * Construct table upper_bound_table for all types      *
 * in expression ex.					*
 ********************************************************/

PRIVATE Boolean build_upper_bound_table_help(EXPR **ex)
{
  update_upper_bound_table((*ex)->ty, NIL, NIL);
  reset_seen();
  return 0;
}

/*---------------------------------------------------------*/

PRIVATE void build_upper_bound_table(EXPR *ex)
{
  upper_bound_table = NULL;
  scan_expr(&ex, build_upper_bound_table_help, TRUE);
}


/********************************************************
 *			GET_RUNTIMES			*
 ********************************************************
 * Return the unbound, non-global type/family variables *
 * in type t.  A variable is considered to be unbound	*
 * if it is not in glob_bound_vars, and is not in 	*
 * boundvars.						*
 ********************************************************/

PRIVATE TYPE_LIST*
get_runtimes(TYPE *t, EXPR *e, LIST *boundvars)
{
  LIST *result;
  TYPE_TAG_TYPE t_kind;

  t      = find_u(t);
  t_kind = TKIND(t);

# ifdef DEBUG
    if(trace_infer > 1) {
      trace_t(265);
      trace_ty(t); tracenl();
      trace_t(264);
      print_str_list_nl(boundvars);
      trace_t(126);
      print_str_list_nl(glob_bound_vars);
    }
# endif

  result = NIL;
  if(IS_STRUCTURED_T(t_kind)) {
    TYPE_LIST *bv1, *bv2, *bv1p;
    bump_list(bv1 = get_runtimes(t->TY2, e, boundvars));
    bump_list(bv1p = append(bv1, boundvars));
    bump_list(bv2 = get_runtimes(t->TY1, e, bv1p));
    bump_list(result = append(bv1, bv2));
    drop_list(bv1);
    drop_list(bv2);
    drop_list(bv1p);
  }

  else if(IS_VAR_T(t_kind)) {
    if(!class_mem(t, glob_bound_vars) && !class_mem(t, boundvars)) {

#     ifdef DEBUG
        if(trace_infer) {
	  trace_t(268);
	  trace_ty(t); tracenl();
        }
#     endif

      there_are_runtimes = TRUE;
      bump_list(result = type_cons(t, NIL));
    }
  }

# ifdef DEBUG
    if(trace_infer > 1) {
      trace_t(270);
      print_str_list_nl(result);
    }
# endif

  if(result != NIL) result->ref_cnt--;
  return result;
}

