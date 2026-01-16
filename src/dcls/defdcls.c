/*****************************************************************
 * File:    dcls/defdcls.c
 * Purpose: Support for define declarations
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
 * This file handles let/define, team and execute declarations.  The	*
 * functions here are called at the end of type inference to enter the	*
 * declarations into the global id table and generate code for them.	*
 ************************************************************************/

#include <string.h>
#include "../misc/misc.h"
#include "../utils/lists.h"
#include "../utils/strops.h"
#include "../alloc/allocate.h"
#include "../error/error.h"
#include "../tables/tables.h"
#include "../parser/parser.h"
#include "../lexer/modes.h"
#include "../unify/unify.h"
#include "../alloc/allocate.h"
#include "../classes/classes.h"
#include "../generate/generate.h"
#include "../generate/prim.h"
#include "../exprs/expr.h"
#include "../dcls/dcls.h"
#include "../infer/infer.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/****************************************************************
 *			QUICK_ISSUE_DEFINE_P			*
 ****************************************************************
 * Issue a definition of identifier id, whose name is name.     *
 * MODE is the mode, and line is the line of this		*
 * declaration.							*
 *								*
 * Unlike issue_define_p, this does not do a full definition.	*
 * It does not generate any code, does not do role checking or  *
 * checking for special conditions such as irregular		*
 * identifiers.  It only makes an entry in the global id table. *
 *								*
 * This function should be used with caution.			*
 *								*
 * MODE is a safe pointer.  It only lives as long as this	*
 * function call.						*
 *								*
 * XREF:							*
 *   Called in dclcnstr.c to declare automatic constructors,	*
 *   etc., when the definition does not need to be generated.	*
 *								*
 *   Called in primtbl.c to declare identifiers that are not	*
 *   standard, but are primitives that are created when		*
 *   certain packages are imported.				* 
 ****************************************************************/

void quick_issue_define_p(EXPR *id, char *name, MODE_TYPE *mode, int line)
{
  LIST *who_sees;
  bump_list(who_sees = get_visible_in(mode, name));
  drop_list(define_global_id_tm(id, NULL, 0, mode, 0, LET_E, line, 
				who_sees, NULL));
  drop_list(who_sees);
}


/****************************************************************
 *			ISSUE_DEFINE_P				*
 ****************************************************************
 * Issue a define or let declaration def, with mode MODE. 	*
 * Return the number of solutions.  				*
 *								*
 * If check_if_should_take is true, then check that		*
 * should_take(e) true.  If not, suppress the definition.	*
 * If check_if_should_take is false, always do the definition.	*
 *								*
 * MODE is a safe pointer.  It only lives as long as this	*
 * function call.						*
 *								*
 * XREF: 							*
 *   Called in dcl.c to issue a definition at the end of type	*
 *   inference.							*
 *								*
 *   Called in dclcnstr.c to define automatic constructors,	*
 *   etc.							*
 *								*
 *   Called in dclutil.c to issue automatic definitions.	*
 *								*
 *   Called in somedcls.c to handle teams			*
 *								*
 ****************************************************************/

int issue_define_p(EXPR *def, MODE_TYPE *mode, Boolean check_if_should_take)
{
  int result, arg, prim;
  TYPE_LIST *types;
  LIST *who_sees;
  EXPR *id, *ssdef, *rhs;
  char *name, *spec_name;
  PART *part;
  INT_LIST *selection_list;
  Boolean lhs_irregular, rhs_irregular, rhs_is_id;

# ifdef DEBUG
    if(trace_exprs) {
      trace_t(43);
      print_mode(mode, 1);
      print_expr(def, 0);
    }
# endif

  bump_expr(def);
  ssdef = skip_sames(def);

  /*--------------------------------------------------------------------*
   * Check if should take this definition.  Might want to reject it	*
   * because it was found in the first pass of type inference		*
   * not to be compatible with expectations. 				*
   *--------------------------------------------------------------------*/

  if(check_if_should_take && !should_take(ssdef)) {
    drop_expr(def); 
    return 0;
  }

  /*----------------------------------------------------*
   * Get the id being defined.  Set its primitive info  *
   * to the primitive info of the r.h.s.		*
   *----------------------------------------------------*/

  rhs           = skip_sames(ssdef->E2);
  id            = skip_sames(ssdef->E1);
  name          = id->STR;
  spec_name     = display_name(name);
  id->kind      = GLOBAL_ID_E;
  id->PRIMITIVE =
    prim        = get_prim_g(rhs, &arg, &part, &rhs_irregular, 
			     &selection_list);
  id->SCOPE     = arg;  /* Argument to the primitive */
  id->GIC       = NULL;
  bump_list(id->EL3 = selection_list);
  SET_ROLE(id->role, remove_knc(id->role, 1));
  SET_ROLE(ssdef->E1->role, remove_knc(ssdef->E1->role, 1));

  /*------------------------------------------------------*
   * Set irregular status.  If the right-hand side is an  *
   * identifier, then its irregular status must be the	  *
   * same as that given in the mode.			  *
   *------------------------------------------------------*/

  rhs_is_id = is_id_p(rhs);
  if(!rhs_irregular && rhs_is_id && rhs->irregular) {
    rhs_irregular = TRUE;
  }
  lhs_irregular = tobool(has_mode(mode, IRREGULAR_MODE) != 0);
  if(lhs_irregular != rhs_irregular && rhs_is_id) {
    semantic_error1(IRREG_EQUAL_REG_ERR, spec_name, def->LINE_NUM);
    drop_expr(def);
    return 0;
  }
  id->irregular = lhs_irregular;

  /*------------------------------------------------*
   * If we have Let{irregular} f = g, where f and g *
   * are both irregular function names, then this   *
   * is ok.  However, generate_exec_g will complain *
   * upon generating g, since an irregular function *
   * can only be applied.  So we fix the problem by *
   * marking g as not being irregular.		    *
   *------------------------------------------------*/

  if(rhs_irregular && rhs_is_id) {
    rhs->irregular = 0;
  }

  /*------------------*
   * Declare this id. *
   *------------------*/

  bump_list(who_sees = get_visible_in(mode, id->STR));
  bump_list(types = define_global_id_tm(ssdef->E1, ssdef, 0, mode,
					FALSE, 0, def->LINE_NUM,
					who_sees, NULL));
  drop_list(who_sees);

  /*------------------------------------------------------------*
   * For irregular primitives, need to put restriction type.	*
   *------------------------------------------------------------*/

  if(has_mode(mode, IRREGULAR_MODE) &&
     (prim == PRIM_CAST || prim == PRIM_DWRAP)) {
    add_restriction_tm(name, ssdef->E2->ty);
  }

  /*------------------*
   * Check the roles. *
   *------------------*/

  do_def_dcl_role_check(ssdef, id, spec_name, types, def->LINE_NUM);

  /*--------------------------------*
   * Generate code, if appropriate. *
   *--------------------------------*/

  if(types != NIL) {
    if(gen_code) {
      id->kind = GLOBAL_ID_E;
      id->GIC  = NULL;
      generate_define_dcl_g(def, types, mode);
    }
    clear_offsets(def);
  }

  drop_expr(def);
  result = list_length(types);
  drop_list(types);
  return result;
}


/*==============================================================*
 *			TEAM DECLARATIONS			*
 *==============================================================*/

/****************************************************************
 *			ISSUE_TEAM_DEFS_P	       		*
 ****************************************************************
 * e is a team, consisting of APPLY_E nodes linking LET_E and   *
 * DEFINE_E nodes.  Issue each of the LET_E and DEFINE_E	*
 * definitions in e.						*
 ****************************************************************/

PRIVATE void issue_team_defs_p(EXPR *e)
{
  EXPR *a;
  EXPR_TAG_TYPE k;
  MODE_TYPE *mode;

  bump_expr(e = skip_sames_to_dcl_mode(e));
  a = e;
  k = EKIND(a);

  /*-------------------------------------*
   * Handle teams with multiple entries. *
   *-------------------------------------*/

  while(k == APPLY_E) {
    issue_team_defs_p(a->E2);
    a = skip_sames_to_dcl_mode(a->E1);
    k = EKIND(a);
  }

  /*------------------------------------------------------------*
   * Here, we are looking at an individual entry.  It might be 	*
   * headed by a declaration mode node.  Process that node.	*
   *------------------------------------------------------------*/

  if(k == SAME_E) {
     mode = a->SAME_E_DCL_MODE;
     a = skip_sames(a);
     k = EKIND(a);
  }
  else mode = NULL;

  /*-----------------------------*
   * Now process the definition. *
   *-----------------------------*/

  if(k == DEFINE_E || k == LET_E) {
    skip_sames(a->E1)->PRIMITIVE = 0;
    issue_define_p(a, mode, FALSE);
  }
  drop_expr(e);
}


/****************************************************************
 *			ISSUE_TEAM_ADVISORIES_P	       		*
 ****************************************************************
 * e is a team, consisting of APPLY_E nodes linking LET_E and   *
 * DEFINE_E nodes.  Issue each of the MANUAL_E declarations 	*
 * in e that encodes an advisory.				*
 ****************************************************************/

PRIVATE void issue_team_advisories_p(EXPR *e)
{
  EXPR *a;
  EXPR_TAG_TYPE k;

  bump_expr(e = skip_sames(e));
  a = e;
  k = EKIND(a);

  /*-------------------------------------*
   * Handle teams with multiple entries. *
   *-------------------------------------*/

  while(k == APPLY_E) {
    issue_team_advisories_p(skip_sames(a->E2));
    a = skip_sames(a->E1);
    k = EKIND(a);
  }

  /*------------------------------------------------------------*
   * Here, we are looking at an individual entry. 		*
   * Process it.						*
   *------------------------------------------------------------*/

  if(k == MANUAL_E && a->MAN_FORM > 1) {
    LIST *ids;
    EXPR* this_id = skip_sames(a->E1);

    bump_list(ids = str_cons(this_id->STR, NIL));
    do_for_advisory(a->EL2, ids, this_id->ty);
    drop_list(ids);
  }
  drop_expr(e);
}


/****************************************************************
 *			ISSUE_TEAM_DESCRS_P	       		*
 ****************************************************************
 * e is a team, consisting of APPLY_E nodes linking LET_E and   *
 * DEFINE_E nodes.  Issue each of the MANUAL_E declarations 	*
 * in e that encodes a description.				*
 ****************************************************************/

PRIVATE void issue_team_descrs_p(EXPR *e)
{
  EXPR *a;
  EXPR_TAG_TYPE k;
  MODE_TYPE *mode;

  bump_expr(e = skip_sames_to_dcl_mode(e));
  a = e;
  k = EKIND(a);

  /*-------------------------------------*
   * Handle teams with multiple entries. *
   *-------------------------------------*/

  while(k == APPLY_E) {
    issue_team_descrs_p(a->E2);
    a = skip_sames_to_dcl_mode(a->E1);
    k = EKIND(a);
  }

  /*------------------------------------------------------------*
   * Here, we are looking at an individual entry.  It mibht be 	*
   * headed by a declaration mode node.  Process that node.	*
   *------------------------------------------------------------*/

  if(k == SAME_E) {
     mode = a->SAME_E_DCL_MODE;
     a = skip_sames(a);
     k = EKIND(a);
  }
  else mode = NULL;

  /*------------------------------------------------------------*
   * Here, we are looking at an individual entry. 		*
   * Process it.						*
   *------------------------------------------------------------*/

  if(k == MANUAL_E && a->MAN_FORM <= 1) {
    issue_description_p(a, mode);
  }
  drop_expr(e);
}


/****************************************************************
 *			ISSUE_TEAM_P		       		*
 ****************************************************************
 * e is a team declaration, consisting of APPLY_E nodes linking *
 * LET_E, DEFINE_E and MANUAL_E nodes.  Issue each of the 	*
 * declarations in e.						*
 *								*
 * If e should not be done because it has a rejected type, then *
 * do not do the definitions.					*
 *								*
 * XREF: Called by dcl.c:issue_dcl_p to issue a team declaration*
 * after type inference.					*
 ****************************************************************/

int issue_team_p(EXPR *e)
{
  int n;

  bump_expr(e);

  /*---------------------------------------------*
   * Check whether e should be processed at all. *
   *---------------------------------------------*/

  if(!should_take(e)) {
    drop_expr(e); 
    return 0;
  }

  /*-------------------------------------------------------------*
   * First issue the advisories. They might affect processing of *
   * the definitions.						 *
   *-------------------------------------------------------------*/

  issue_team_advisories_p(e);

  /*----------------------------*
   * Now issue the definitions. *
   *----------------------------*/

  issue_team_defs_p(e);
  n = !local_error_occurred;

  /*---------------------------------------------------------------*
   * Now issue the descriptions.  This must come last, since the   *
   * descriptions refer to things that are defined in the previous *
   * step.							   *
   *---------------------------------------------------------------*/

  issue_team_descrs_p(e);

  drop_expr(e);
  return n;
}


/*==============================================================*
 *			EXECUTE DECLARATIONS			*
 *==============================================================*/

/****************************************************************
 *			CHECK_EXECUTE_BODY	       		*
 ****************************************************************
 * Check that e does not produce a stream, and cannot fail.  If *
 * it produces a stream or fails, give a warning suitable for	*
 * an execute declaration.					*
 ****************************************************************/

PRIVATE void check_execute_body(EXPR *e)
{
  ROLE *this_dcl_role = e->role;
  STR_LIST* this_namelist =
    (this_dcl_role == NULL) ? NULL : this_dcl_role->namelist;

  if(this_namelist != NULL &&
     !should_suppress_warning(err_flags.suppress_property_warnings)) {

    if(!is_suppressed_property(act_produce_stream)
       && rolelist_member(act_produce_stream, this_namelist, 0)) {
      warn1(DCL_PRODUCE_STREAM_ERR, "execute", e->LINE_NUM);
    }

    if(!is_suppressed_property(act_fail)
       && rolelist_member(act_fail, this_namelist, 0)) {
      warn1(DCL_FAIL_ERR, "execute", e->LINE_NUM);
    }
  }
}


/****************************************************************
 *			DO_EXECUTE_P		       		*
 ****************************************************************
 * Expression e is the body of an execute declaration.  Issue   *
 * it as an execute declaration.				*
 *								*
 * XREF: Called by dcl.c:issue_dcl_p to issue an execute	*
 * declaration after type inference.				*
 ****************************************************************/

Boolean do_execute_p(EXPR *e, MODE_TYPE *mode)
{
  if(!local_error_occurred) {

    bump_expr(e);

#   ifdef DEBUG
      if(trace_exprs) {
	trace_t(46);
	print_expr(e,1);
      }
#   endif

    /*------------------------------------------------------*
     * Check for a stream or failing body.  The body of an  *
     * if expression should not produce a stream or fail.   *
     *------------------------------------------------------*/

    check_execute_body(e);

    /*--------------------*
     * Generate the code. *
     *--------------------*/

    if(gen_code) {
      generate_execute_dcl_g(e, mode);
    }

    /*---------------------------------------------*
     * Check for uses of missing definitions in e. *
     *---------------------------------------------*/

    check_missing_tm(e);

    drop_expr(e);
    return TRUE;
  }
  return FALSE;
}


/*==============================================================*
 *			DEFINITIONS AT ANTICIPATIONS		*
 *==============================================================*/

/****************************************************************
 *			GET_WRAP_UNWRAP_PAIR			*
 ****************************************************************
 * Set *pat and *val to related expressions, where *pat is a	*
 * pattern and *val is a corresponding value.  If kind = 1,	*
 * then wraps are in the pattern.  If kind = 2, then wraps	*
 * are in the value.						*
 *								*
 * Here are some examples of what is returned when kind = 1.	*
 * Note that V is a variable that is a parameter.		*
 *								*
 *   ty			*pat		*val			*
 *  -----		----		----			*
 *  Natural		?x		x			*
 *								*
 *  V			<<?x>>		x			*
 *								*
 *  (V,Natural)		(<<?x>>,?y)	(x,y)			*
 *								*
 * Here are some examples of what is returned when kind = 2.	*
 *								*
 *   ty			*pat		*val			*
 *  -----		----		----			*
 *  Natural		?x		x			*
 *								*
 *  V			?x		<<x>>			*
 *								*
 *  (V,Natural)		(?x,?y)		(<<x>>,y)		*
 *								*
 * Variables *pat and *val are not reference counted.		*
 ****************************************************************/

PRIVATE void 
get_wrap_unwrap_pair(TYPE *ty, TYPE* V, EXPR **pat, EXPR **val, 
		     int kind, int line)
{
  ty = find_u(ty);

  /*--------------------------------------------*
   * When V does not occur in ty,we want (?x,x) *
   * for (*pat, *val).				*
   *						*
   * This case is also the default when nothing	*
   * else applies.  At the end of this function	*
   * is a goto back to here.			*
   *--------------------------------------------*/

  if(!occurs_in(V,ty)) {
default_value:
    *pat = new_pat_var(new_temp_var_name(), line);
    *val = (*pat)->E1;
    return;
  }
 
  /*------------------------------------*
   * When ty is V, we want (*pat,*val)	*
   * to be				*
   *					*
   *     (<<?x>>,x)   if kind == 1,	*
   *     (?x, <<x>>)  if kind == 2.	*
   *------------------------------------*/

  else if(V == ty || TKIND(ty) == FAM_MEM_T && find_u(ty->TY2) == V) {
    EXPR* patt = new_pat_var(new_temp_var_name(), line);
    EXPR* vall = patt->E1;
    if(kind == 1) {
      *pat = make_wrap(patt, line);
      *val = vall;
    }
    else {
      *pat = patt;
      *val = make_wrap(vall, line);
    }
  }

  else if(TKIND(ty) == PAIR_T) {
    EXPR *pat1, *pat2, *val1, *val2;
    get_wrap_unwrap_pair(ty->TY1, V, &pat1, &val1, kind, line);
    get_wrap_unwrap_pair(ty->TY2, V, &pat2, &val2, kind, line);
    *pat = new_expr2(PAIR_E, pat1, pat2, line);
    *val = new_expr2(PAIR_E, val1, val2, line);
  }

  else goto default_value;
}


/****************************************************************
 *			DISPATCHER_VAL				*
 ****************************************************************
 * Return an expression for issue_dispatch_defn.  		*
 * The expression is the value that id:ty will have, where	*
 * id is being dispatched on variable V.			*
 *								*
 * Here are some examples of what is returned.			*
 *								*
 *    ty			result				*
 * --------			----------			*
 * V -> Boolean			(<<?x>> => id(x))		*
 *								*
 * V -> V			(<<?x>> => <<id(x)>>)		*
 *								*
 * (V,Natural) -> Boolean	((<<?x>>,?y) => id(x,y))	*
 *								*
 * V -> (V,V)			(<<?x>> =>			*
 *				    Match (?a,?b) = id(x).	*
 *				    (<<a>>, <<b>>))		*
 *								*
 * V -> Natural -> Natural	(<<?x>> => id(x))		*
 *								*
 * Natural -> V -> Natural	(?x => (<<?y>> => (id x y)))	*
 ****************************************************************/

PRIVATE EXPR* 
dispatcher_val(EXPR *id, TYPE *ty, TYPE *V, int line)
{
  ty = find_u(ty);

  /*-----------------------------------------------------------------*
   * Note: in the comments below, function dispatcher_val is called  *
   * E, and parameters V and line are suppressed.		     *
   *-----------------------------------------------------------------*/

  /*--------------------------------------------*
   * When V does not occur in ty, E(ty,x) = x.	*
   *--------------------------------------------*/

  if(!occurs_in(V,ty)) return id;

  /*--------------------*
   * E(V,x) = <<x>>.	*
   *--------------------*/

  if(V == ty) return make_wrap(id, line);

  switch(TKIND(ty)) {
    default: return id;

    case FAM_MEM_T:

      /*------------------------------------------------*
       * E(V(A), x) = <<x>> 				*
       * If V occurs in the argument A, then it is	*
       * not relevant. So E(-,x) = x.			*
       *------------------------------------------------*/

      if(find_u(ty->TY2) == V) return make_wrap(id, line);
      else return id;

    case PAIR_T:

      /*------------------------------------------------*
       * E((A,B), x) = Match pat = x. (val)		*
       *						*
       * where (pat,val) = get_wrap_unwrap_pair(ty, 2)	*
       *------------------------------------------------*/

      {EXPR *pat, *val, *match;
       get_wrap_unwrap_pair(ty, V, &pat, &val, 2, line);
       match = new_expr2(MATCH_E, pat, id, line);
       return apply_expr(match, val, line);
      }

    case FUNCTION_T:

      /*------------------------------------------------*
       * E(A -> B, f) = (pat => E(B, f(val)))		*
       *						*
       * where (pat, val) = get_wrap_unwrap_pair(A, 1) 	*
       *------------------------------------------------*/

      {EXPR *pat, *val;
       get_wrap_unwrap_pair(ty->TY1, V, &pat, &val, 1, line);
       return new_expr2(FUNCTION_E, 
		        pat, 
		        dispatcher_val(apply_expr(id, val, line), 
				       ty->TY2, 
				       V, line), 
		        line);
      }
  }
}


/****************************************************************
 *			ISSUE_DISPATCH_DEFN			*
 ****************************************************************
 * Issue a definition of id:ty as a dispatcher, where V is	*
 * the variable that indicates what is being dispatched.  See	*
 * function dispatcher_val for examples of what is issued.	*
 * The definition is an underriding definition.			*
 ****************************************************************/

PRIVATE void 
issue_dispatch_defn(char* id, TYPE *ty, TYPE *V)
{
  MODE_TYPE *mode = simple_mode(UNDERRIDES_MODE);  /* ref cnt is 1. */

  /*-----------------------------------------------------------*
   * If we are not doing an import, then build the definition. *
   *-----------------------------------------------------------*/
  
  if(main_context != IMPORT_CX  && main_context != INIT_CX) {
    EXPR *def_id, *id_as_expr, *id_val, *defn;

    def_id = typed_id_expr(id, ty, current_line_number);
    bump_expr(id_as_expr = id_expr(id, current_line_number));
    id_val = dispatcher_val(id_as_expr, ty, V, current_line_number);
    bump_expr(defn   = new_expr2(LET_E, def_id, id_val, current_line_number));

#   ifdef DEBUG
      if(trace_defs) {
	trace_t(554);
	if(trace_exprs) print_expr(defn, 0);
      }
#   endif

    suppress_using_local_expectations = TRUE;
    issue_dcl_p(defn, LET_E, mode);
    suppress_using_local_expectations = FALSE;
    drop_expr(defn);
    drop_expr(id_as_expr);
  }

  /*-----------------------------------------------------------*
   * If we are doing an import, then just do an expectation.   *
   *-----------------------------------------------------------*/
  
  else {
    expect_ent_id_p(id, ty, NULL, EXPECT_ATT, mode, 
		    current_line_number, FALSE, NULL);
  }

  drop_mode(mode);
}


/****************************************************************
 *			ISSUE_DISPATCH_DEFINITIONS		*
 ****************************************************************
 * An anticipation id:ty is being issued.  Issue underriding	*
 * unwrap definitions for id:ty.				*
 *								*
 * This is only done when id is a function.  The function that	*
 * is defined unwraps it argument(s), and dispatches id based	*
 * on the types wrapped with the arguments.			*
 *								*  
 * id is defined of a type that is a modification of ty.  The	*
 * modified ty is obtained by replacing each marked variable in *
 * ty by a wrap variable.					*
 ****************************************************************/

void issue_dispatch_definitions(char *id, TYPE *ty)
{
  TYPE_LIST *Vs, *marked;

  bump_type(ty = copy_type(ty, 4));
  bump_list(marked = marked_vars_t(ty, NIL));

# ifdef DEBUG
    if(trace_defs) {
      trace_t(556, id);
      trace_ty(ty);
      tracenl();
    }
# endif

  for(Vs = marked; Vs != NIL; Vs = Vs->tail) {

    /*----------------------------------------------------------*
     * Get the variable V to be replaced. 			*
     *								*
     * If V is a primary variable, we cannot bind it to a wrap	*
     * type, so skip it.					*
     *								*
     * If V does not occur in a dispatchable context exactly 	*
     * once in ty, then we cannot create the definition.	*
     *----------------------------------------------------------*/

    TYPE*         V      = Vs->head.type;
    TYPE_TAG_TYPE V_kind = TKIND(V);

#   ifdef DEBUG
      if(trace_defs) {
        trace_t(555);
	trace_ty(V);
	tracenl();
      }
#   endif

    if(!IS_PRIMARY_VAR_T(V_kind) && dispatchable_count_t(V, ty, 0) == 1) {

      /*------------------------------------------------*
       * Get the variable newV to replace V with. 	*
       * newV is has the same domain as V, but is 	*
       * a wrap variable.				*
       *------------------------------------------------*/

      TYPE* newV = copy_type(V, 0);
      newV->kind = IS_TYPE_VAR_T(V_kind) ? WRAP_TYPE_VAR_T : WRAP_FAM_VAR_T;

      /*------------------------------------*
       * Replace V with newV by binding it. *
       *------------------------------------*/

      bump_type(V->TY1 = newV);

      /*-----------------------*
       * Issue the definition. *
       *-----------------------*/

      issue_dispatch_defn(id, ty, newV);

      /*-------------------------------------------*
       * Unbind V and continue with next variable. *
       *-------------------------------------------*/

      SET_TYPE(V->TY1, NULL);

    } /* end if */
  } /* end for(Vs = ...) */

  drop_list(marked);  
  drop_type(ty);
}


