/******************************************************************
 * File:    generate/genglob.c
 * Purpose: Generate code to fetch global ids.
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

/****************************************************************
 * This file manages the global environment of a declaration.   *
 * The global environment is used to store things that are      *
 * computed once for a given object -- mainly the values of     *
 * globals to which the object refers.  For example, if the	*
 * definition of function f refers to function g, then each     *
 * object f will have, in its global environment, a value of g  *
 * of an appropriate type.  If the definition of f is 		*
 * polymorphic, there can be many different objects f around.   *
 * Each will have its own global environment, holding 		*
 * appropriately typed globals for it.				*
 *								*
 * Globals that need to be fetched through the			*
 * dispatcher because their types involve run-time bound	*
 * variables are not placed in the global environment.		*
 ****************************************************************/

#include "../misc/misc.h"
#include "../utils/hash.h"
#include "../utils/lists.h"
#include "../utils/strops.h"
#include "../alloc/allocate.h"
#include "../classes/classes.h"
#include "../standard/stdtypes.h"
#include "../exprs/expr.h"
#include "../evaluate/instruc.h"
#include "../tables/tables.h"
#include "../generate/generate.h"
#include "../generate/prim.h"
#include "../error/error.h"
#include "../lexer/modes.h"
#include "../unify/unify.h"
#include "../infer/infer.h"  /* glob_bound_vars */
#ifdef DEBUG
# include "../debug/debug.h"
#endif


/****************************************************************
 *			PUBLIC VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			num_globals				*
 ****************************************************************
 * num_globals is the number of cells that are needed in the 	*
 * global environment for the current declaration.	     	*
 ****************************************************************/

int num_globals;


/****************************************************************
 *			PRIVATE VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			global_list				*
 ****************************************************************
 * global_list is a list of the things that need to be placed 	*
 * in the global environment.  Each has its offset within it. 	*
 * The offsets are in decreasing order.				*
 ****************************************************************/

PRIVATE EXPR_LIST* global_list = NIL;


/****************************************************************
 *			INIT_GEN_GLOBALS			*
 ****************************************************************/

void init_gen_globals(void)
{
  SET_LIST(global_list, NIL);
}


/****************************************************************
 *			GEN_GLOBAL_ID_G				*
 ****************************************************************
 * Generate instructions to push global id e onto the stack.	*
 *								*
 * irregular_ok is true if e can be an irregular id, false	*
 * if not.					   		*
 ****************************************************************/

#ifdef DEBUG
PRIVATE void trace_gen_global_id(EXPR *e)
{
  PRINT_TYPE_CONTROL ctl;

  begin_printing_types(TRACE_FILE, &ctl);
  trace_t(146, e->STR);
  print_ty1_with_constraints(e->ty, &ctl); 
  tracenl();
  print_glob_bound_vars(&ctl);
  end_printing_types(&ctl);
}
#endif

/*------------------------------------------------------------*/

void gen_global_id_g(EXPR *e, Boolean irregular_ok)
{
  int n, prim, instr;
  TYPE *t, *e_ty;
  PART *part;
  INT_LIST *sel_list;
  Boolean irregular_prim, is_irregular;

# ifdef DEBUG
    if(trace_gen > 1) {
      trace_gen_global_id(e);
    }
# endif

  /*---------------------------------------------------------------*
   * An irregular id should not occur in a context where it is not *
   * being applied 						   *
   *---------------------------------------------------------------*/

  e = skip_sames(e);
  is_irregular = e->irregular;
  if(!irregular_ok && is_irregular) {
    semantic_error1(IRREGULAR_CONTEXT_ERR, display_name(e->STR), e->LINE_NUM);
  }

  /*--------------------------------------------------------------------*
   * Get the associated primitive, if any.  (prim will be 0 if this is	*
   * not a primitive.)  If this is a simple, entity primitive, just	*
   * generate it here.  						*
   *--------------------------------------------------------------------*/

  prim = get_prim_g(e, &instr, &part, &irregular_prim, &sel_list);
  if(prim != 0 && gen_ent_prim_g(prim,instr)) return;

  /*--------------------------------------------------*
   * Generate a fetch of the global.  If this is      *
   * an irregular function, force its codomain to (). *
   *--------------------------------------------------*/

  bump_type(e_ty = e->ty);
  bump_type(t = find_u(e_ty));
  if(is_irregular) {
    if(TKIND(t) != FUNCTION_T) die(176);
    SET_TYPE(t, function_t(t->TY1, hermit_type));
    SET_TYPE(e->ty, t);
  }
  reset_frees(t);
  if(t->free == 0) {
    n = global_offset_g(e);
    gen_g(G_FETCH_I); gen_g(n);
  }
  else {
    genP_g(DYN_FETCH_GLOBAL_I, id_loc(new_name(e->STR,TRUE), ID_DCL_I));
    gen_g(type_offset_g(t));
  }
  if(is_irregular) {
    SET_TYPE(e->ty, e_ty);
  }
  drop_type(t);
  drop_type(e_ty);
}


/****************************************************************
 *			GMEM_G					*
 ****************************************************************
 * Returns NULL if global identifier or SPECIAL_E node e is not *
 * a member of list l, and returns the corresponding member of 	*
 * l otherwise. 						*
 *								*
 * The membership test for global ids compares names and types.	*
 * If both are equal, then the identifiers or special nodes     *
 * are presumed equal.						*
 *								*
 * The membership test for SPECIAL_E nodes only compares types. *
 * SPECIAL_E nodes are used for entering a type into the 	*
 * global environment.						*
 ****************************************************************/

PRIVATE EXPR* gmem_g(EXPR *e, EXPR_LIST *l)
{
  EXPR_LIST *p;
  EXPR *a;

  char* name = e->STR;
  TYPE* type = e->ty;
  EXPR_TAG_TYPE kind = EKIND(e);

  for(p = l; p != NIL; p = p->tail) {
    a = p->head.expr;
    if(EKIND(a) == kind && full_type_equal(type, a->ty)) {
      if(kind == GLOBAL_ID_E) {
	if(name == a->STR) return a;
      }
      else return a;
    }
  }
  return NULL;
}


/****************************************************************
 *			GLOBAL_OFFSET_G				*
 ****************************************************************
 * Return the offset of e in the global environment, 		*
 * simultaneously placing e in the global list for generation 	*
 * if necessary. e must be a SPECIAL_E or GLOBAL_ID_E node.	*
 *								*
 * If e is a SPECIAL_E node, then only put the type of e in	*
 * the global environment.					*
 *								*
 * XREF: 							*
 *   Called in genexec.c to generate a global id fetch.		*
 *   								*
 *   Called in genvar.c to generate cells that hold types.	*
 *								*
 *   Called in prim.c to generate cells that hold types, for	*
 *   TY_PRIM functions.						*
 ****************************************************************/

int global_offset_g(EXPR *e)
{
  EXPR *a, *newe;

# ifdef DEBUG
    if(trace_gen > 1) {
      trace_t(217, nonnull(e->STR), expr_kind_name[EKIND(e)]);
      trace_ty(e->ty);
      tracenl();
    }
# endif

  /*-----------------------------------------------------------------*
   * Look in the current global list.  If found, then get the offset *
   * from the global list entry.  In the case where this is a 	     *
   * SPECIAL_E node holding just a variable, check to see whether    *
   * the variable already has an offset.			     *
   *-----------------------------------------------------------------*/

  if(EKIND(e) == SPECIAL_E) {
    register TYPE* e_ty = find_u(e->ty);
    if(IS_VAR_T(TKIND(e_ty)) && e_ty->TOFFSET != 0) {
#     ifdef DEBUG
        if(trace_gen > 1) trace_t(231, toint(e_ty->TOFFSET) - 1);
#     endif
      return toint(e_ty->TOFFSET) - 1;
    }
  }

  a = gmem_g(e, global_list);
  if(a != NULL) {
#   ifdef DEBUG
      if(trace_gen > 1) trace_t(231, a->OFFSET);
#   endif
    return a->OFFSET;
  }

  /*------------------------------------------------------------------*
   * If not found in the global list, then make an entry in that list *
   * for e.  We need to copy e in case e is changed later.  	      *
   *------------------------------------------------------------------*/

  newe = copy_global_id_expr(e);
  SET_LIST(global_list, expr_cons(newe, global_list));
  return e->OFFSET = newe->OFFSET = num_globals++;
}


/****************************************************************
 *			TYPE_OFFSET_G				*
 ****************************************************************
 * Call for storing type t in the global environment, and 	*
 * return the offset where it will be found.			*
 ****************************************************************/

int type_offset_g(TYPE *t)
{
  EXPR* ex;
  int result;

  bump_expr(ex = new_expr1t(SPECIAL_E, NULL, t, 0));
  result = global_offset_g(ex);
  drop_expr(ex);
  return result;
}


/****************************************************************
 *			TAKE_APART_G				*
 ****************************************************************
 * This function should only be used while generating the	*
 * global environment.  It binds the main bound variables	*
 * to their actual values for a given copy of a polymorphic	*
 * definition, placing the results in the global environment.	*
 *								*
 * The type stack contains a type s.  This function generates 	*
 * code to unify s and t, performing appropriate variable	*
 * bindings.  s must be a species -- it has no variables.  	*
 * For variables in t, each variable's TOFFSET field is set to	*
 * one greater than the offset in the global environment where	*
 * that variable's binding can be found.  			*
 *								*
 * Function take_apart_help_g does the main work.  It returns	*
 * true if any useful work was done.				*
 * When true is returned, s is popped from the type stack.  	*
 * When false is returned, s is left on the type stack.		*
 *								*
 * MODE is the mode of the declaration that we are generating   *
 * globals for.  It is a safe pointer: it does not live longer  *
 * than this function call.					*
 *								*
 * XREF:							*
 *   Called in gendcl.c to create heading of define dcl.	*
 *								*
 *   Called in genstd.c to create heading of standard define	*
 *   dcl.							*
 ****************************************************************/

PRIVATE Boolean take_apart_help_g(TYPE *t)
{
  Boolean ok1, ok2;

# ifdef DEBUG
    if(trace_gen) {
      trace_t(218);
      trace_ty(t); tracenl();
    }
# endif

  t = find_u(t);

  /*----------------------------------------------------------*
   * If t has no variables, then there are no bindings to do. *
   *----------------------------------------------------------*/

  if(t->copy == 0) return FALSE;

  switch(TKIND(t)) {
    case FUNCTION_T:
    case PAIR_T:
    case FAM_MEM_T:
      gen_g(UNPAIR_T_I);
      ok1 = take_apart_help_g(t->TY2);
      if(!ok1) gen_g(POP_T_I);

      ok2 = take_apart_help_g(t->ty1);
      if(!ok2) gen_g(POP_T_I);

      /*--------------------------------------------------------------*
       * If no useful work was done in either branch, then remove the *
       * UNPAIR_T_I and two POP_T_I instructions that were generated. *
       *--------------------------------------------------------------*/

      if(!ok1 && !ok2) {
	*genloc -= 3;
	return FALSE;
      }
      else return TRUE;

    case MARK_T:
    case TYPE_VAR_T:
    case FAM_VAR_T:
    case PRIMARY_TYPE_VAR_T:
    case PRIMARY_FAM_VAR_T:
    case WRAP_TYPE_VAR_T:
    case WRAP_FAM_VAR_T:

       /*----------------------------------------------------------*
        * If this variable has no entry in the global environment, *
	* create one.  But if this variable already has an entry,  *
	* then there is nothing to do.				   *
        *----------------------------------------------------------*/

       if(t->TOFFSET == 0) {
	 t->TOFFSET = ++num_globals;
         gen_g(TYPE_ONLY_I);

#        ifdef DEBUG
	   if(trace_gen > 1) {
	     trace_t(219, t, toint(t->TOFFSET) - 1);
	   }
#        endif

	 return TRUE;
       }
       else return FALSE;

    default: return FALSE;
  }
}    

/*-----------------------------------------------------------*/

void take_apart_g(TYPE *t, MODE_TYPE *mode)
{
  /*------------------------------------------------------------*
   * For an irregular function, throw away the codomain and	*
   * only take apart the domain.				*
   *------------------------------------------------------------*/

  if(has_mode(mode, IRREGULAR_MODE)) {
    t = find_u(t);
    if(TKIND(t) == FUNCTION_T) {
      gen_g(HEAD_T_I);
      take_apart_help_g(find_u(t)->TY1);
      return;
    }
  }

  take_apart_help_g(t);
}


/****************************************************************
 *			DO_GLOBALS_G				*
 ****************************************************************
 * Generate the global identifiers in list l, in reverse order, *
 * in a form suitable for the preamble to a define or execute   *
 * declaration of the machine.					*
 *								*
 * We want to use a single collection of stored types for all   *
 * of the types, allowing us to share work from one type to the *
 * next.  For that reason, we generate types directly via	*
 * bare_generate_type_g.  Initialization must be done		*
 * for bare_generate_type_g before calling do_globals.		*
 *								*
 * This function always returns TRUE.  If it becomes possible   *
 * for it to fail, it should return FALSE on failure.		*
 ****************************************************************/

PRIVATE Boolean do_globals_g(EXPR_LIST *l, HASH2_TABLE **ty_b, int *st_loc)
{
  EXPR *e;
  int ok;
  char *name;

  /*---------------------------------------*
   * Recursion for entire list, backwards. *
   *---------------------------------------*/

  if(l == NIL) return TRUE;
  ok = do_globals_g(l->tail, ty_b, st_loc);

  /*--------------------------------------------------------------*
   * Generate this global.  If e is a global id, then we need     *
   * to generate its type and the global label of an 		  *
   * ID_DCL_I instruction for this identifier.			  *
   *								  *
   * If e is a SPECIAL_E node then only get the type of e.        *
   *--------------------------------------------------------------*/

  e = l->head.expr;
  reset_frees(e->ty);
  bare_generate_type_g(e->ty, ty_b, st_loc, 1);
  if(EKIND(e) == GLOBAL_ID_E) {
    name =  new_name(e->STR, TRUE);
    genP_g(GET_GLOBAL_I, id_loc(name, ID_DCL_I));
  }
  else gen_g(TYPE_ONLY_I);
  return ok;
}


/******************************************************************
 *			GENERATE_GLOBALS_G			  *
 ******************************************************************
 * Generate the preamble to a define, let or execute declaration. *
 * The preamble fills the global environment.			  *
 *								  *
 * Returns TRUE on success, false on failure.  (Currently, it     *
 * cannot fail.)						  *
 *								  *
 * XREF:							  *
 *   Called in gendcl.c to create heading of define dcl.	  *
 *							 	  *
 *   Called in genstd.c to create heading of standard define 	  *
 *   dcl.							  *
 ******************************************************************/

Boolean generate_globals_g(void)
{
  int ok, st_loc;
  HASH2_TABLE *ty_b;

# ifdef DEBUG
    if(trace_gen) {
      trace_t(221);
      print_list(global_list, 1);
    }
# endif

  /*------------------------------------------------------------*
   * Generate the type of each id in the list of globals, 	*
   * followed by an instruction to get the id. The list needs 	*
   * to be reversed, since the ids were pushed at the front as  *
   * they were encountered. 					*
   *								*
   * Begin by setting up for bare_generate_type_g, which        *
   * is called by do_globals_g to generate the types.		*
   *------------------------------------------------------------*/

  ty_b = NULL;
  st_loc = 0;
  count_occurrences_2(global_list);
  ok = do_globals_g(global_list, &ty_b, &st_loc);
  clear_seenTimes_2(global_list);
  free_hash2(ty_b);

  SET_LIST(global_list, NIL);
  return ok;
}
