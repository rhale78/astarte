/**************************************************************
 * File:    classes/m_tyutil.c
 * Purpose: General utilities for types, used only by the
 *          interpreter.
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

#include "../misc/misc.h"
#include "../alloc/allocate.h"
#include "../classes/classes.h"
#include "../standard/stdtypes.h"
#include "../utils/lists.h"
#include "../unify/unify.h"
#include "../intrprtr/intrprtr.h"
#include "../evaluate/evaluate.h"
#include "../machdata/except.h"
#include "../machdata/entity.h"
#include "../clstbl/typehash.h"
#include "../clstbl/m_dflt.h"
#ifdef DEBUG
#  include "../debug/debug.h"
#endif


/************************************************************************
 *			SET_IN_BINDING_LIST				*
 ************************************************************************
 * Set unification to use the binding list from the_act.		*
 *									*
 * Indicator use_binding_list is incremented.				*
 * If a binding list is already in effect, then just increment the	*
 * indicator, without installing a new binding list.			*
 *									*
 * To remove the binding list, use get_out_binding_list.		*
 ************************************************************************/

void set_in_binding_list(void)
{
  if(use_binding_list++ == 0) {
    LIST_LIST* bll = the_act.type_binding_lists;
    LIST*      bl  = (bll == NIL) ? NIL : bll->head.list;
#   ifdef DEBUG
      if(trace_types) {
	trace_i(338);
	print_type_list_separate_lines_without_constraints(bl, " ");
      }
#   endif
    SET_LIST(binding_list, bl);
  }
}


/************************************************************************
 *			SET_IN_BINDING_LIST_TO				*
 ************************************************************************
 * Set unification to use binding list bl.  This is similar		*
 * to set_in_binding_list, the only difference being in where the	*
 * binding list is obtained.  						*
 *									*
 * Note that, if a binding list is already in use, list bl is		*
 * not installed as the unification binding list.  The old one is left	*
 * in place.								*
 *									*
 * To remove the binding list, use get_out_binding_list_to.		*
 ************************************************************************/

void set_in_binding_list_to(TYPE_LIST *bl)
{
  if(use_binding_list++ == 0) {
#   ifdef DEBUG
      if(trace_types) {
	trace_i(339);
	print_type_list_separate_lines_without_constraints(bl, " ");
      }
#   endif
    SET_LIST(binding_list, bl);
  }
}


/************************************************************************
 *			GET_OUT_BINDING_LIST				*
 ************************************************************************
 * This function is used to exit a binding list scope that was entered  *
 * using set_in_binding_list.						*
 *									*
 * If this is the ground-level exit, then get the binding list from	*
 * unification, and put it into the head of the_act.type_binding_lists.	*
 *									*
 * In any event, decrement the binding list indicator, to exit this	*
 * binding list scope.							*
 ************************************************************************/

void get_out_binding_list(void)
{
  if(--use_binding_list == 0) {
#   ifdef DEBUG
      if(trace_types) {
	trace_i(340);
	print_type_list_separate_lines_without_constraints(binding_list, " ");
      }
#   endif

    if(the_act.type_binding_lists == NIL) {
      if(binding_list != NIL) {
	bump_list(the_act.type_binding_lists = list_cons(binding_list, NIL));
      }
    }
    else {
      SET_LIST(the_act.type_binding_lists->head.list, binding_list);
    }
    drop_list(binding_list);
    binding_list = NIL;
  }
}


/************************************************************************
 *			GET_OUT_BINDING_LIST_TO				*
 ************************************************************************
 * This function is used to exit a binding list scope that was entered  *
 * using set_in_binding_list_to.					*
 *									*
 * If this is the ground-level exit, then get the binding list from	*
 * unification, and put it into *bl.  It is assumed that *bl has a	*
 * meaningful value already, and reference counts are maintained.	*
 *									*
 * In any event, decrement the binding list indicator, to exit this	*
 * binding list scope.							*
 ************************************************************************/

void get_out_binding_list_to(TYPE_LIST **bl)
{
  if(--use_binding_list == 0) {
#   ifdef DEBUG
      if(trace_types) {
	trace_i(341);
	print_type_list_separate_lines_without_constraints(binding_list, " ");
      }
#   endif
    SET_LIST(*bl, binding_list);
    drop_list(binding_list);
    binding_list = NIL;
  }
}


/************************************************************************
 *			DO_DYNAMIC_DEFAULTS				*
 ************************************************************************
 * Bind each unbound variable in t, using unification, to a type or 	*
 * family, and replace each bound variable by what it is bound to, and  *
 * return the resulting type.						*
 *									*
 * t must not have any null types.					*
 *									*
 * The result type is in the type table.				*
 ************************************************************************/

PRIVATE TYPE* do_dynamic_defaults(TYPE *t)
{
  TYPE_TAG_TYPE t_kind;

  IN_PLACE_FIND_U_NONNULL(t);
  if(t->THASH > 0) return t;

  t_kind = TKIND(t);
  if(IS_STRUCTURED_T(t_kind)) {
    TYPE* t1 = do_dynamic_defaults(t->TY1);
    TYPE* t2 = do_dynamic_defaults(t->TY2);
    if(t1 == t->TY1 && t2 == t->TY2) return type_tb(t);
    else {
      TYPE *tt, *result;
      bump_type(tt = new_type2(t_kind, t1, t2));
      result = type_tb(tt);
      drop_type(tt);
      return result;
    }
  }

  else if(IS_VAR_T(t_kind)) {
    TYPE *tt, *result;
    bump_type(tt = dynamic_default(t));
    result = type_tb(tt);

#   ifdef DEBUG
      if(trace_types) {
	trace_i(342);
	trace_ty(result);
	tracenl();
      }
#   endif

    if(!bind_u(t, result, FALSE, NULL)) die(179);
    drop_type(tt);
    return result;
  }

  else return type_tb(t);
}


/************************************************************************
 *			FORCE_GROUND					*
 ************************************************************************
 * Force type t to be ground by freezing and then defaulting all	*
 * variables.								*
 ************************************************************************/

TYPE* force_ground(TYPE *t)
{
  TYPE *result;

  /*------------------------------------------------------------*
   * If t->THASH is positive, then this type is already ground. *
   *------------------------------------------------------------*/

  if(t->THASH > 0) return t;

  set_in_binding_list();
  result = do_dynamic_defaults(t);

# ifdef DEBUG
    if(trace_types) {
      trace_i(116);
      trace_ty(t); tracenl();
      trace_i(332);
      trace_ty(result); tracenl();
    }
# endif

  get_out_binding_list();
  return result;
}


/****************************************************************
 *			DYNAMIC_DEFAULT_ENT			*
 ****************************************************************
 * Entity tt is a type (tag TYPE_TAG).  Return an entity that   *
 * is the same type, but with defaults done.			*
 ****************************************************************/

ENTITY dynamic_default_ent(ENTITY tt)
{
  return ENTP(TYPE_TAG, force_ground(TYPEVAL(tt)));
}


/************************************************************************
 *			FREEZE_TYPE					*
 ************************************************************************
 * Returns the type obtained by replacing each bound variable in t by   *
 * the type to which it is bound.  t must not have any null types.	*
 *									*
 * Bindings are taken from the_act's binding list.			*
 ************************************************************************/

PRIVATE TYPE* freeze_type_help(TYPE *t)
{
  TYPE_TAG_TYPE t_kind;

  IN_PLACE_FIND_U_NONNULL(t);
  if(t->THASH > 0) return t;

  t_kind = TKIND(t);
  if(IS_STRUCTURED_T(t_kind)) {
    TYPE *t1, *t2;
    t1 = freeze_type_help(t->TY1);
    t2 = freeze_type_help(t->TY2);
    if(t1 == t->TY1 && t2 == t->TY2) return type_tb(t);
    else {
      TYPE *result, *tt;
      bump_type(tt = new_type2(t_kind, t1, t2));
      result = type_tb(tt);
      drop_type(tt);
      return result;
    }
  }
  else return type_tb(t);
}

/*-----------------------------------------------------*/

TYPE* freeze_type(TYPE *t)
{
  TYPE *result;

  if(t->THASH > 0) return t;

  set_in_binding_list();
  result = freeze_type_help(t);
  get_out_binding_list();
  return result;
}

