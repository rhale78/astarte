/******************************************************************
 * File:    generate/gentype.c
 * Purpose: Generate code for a type expression.
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
 * This file contains functions that are concerned with generating	*
 * byte-code that, when run, produces a type.				*
 ************************************************************************/

#include "../misc/misc.h"
#include "../utils/hash.h"
#include "../utils/lists.h"
#include "../alloc/allocate.h"
#include "../classes/classes.h"
#include "../error/error.h"
#include "../evaluate/instruc.h"
#include "../tables/tables.h"
#include "../clstbl/classtbl.h"
#include "../generate/generate.h"
#include "../unify/unify.h"
#include "../standard/stdtypes.h"
#include "../exprs/expr.h"
#include "../infer/infer.h"
#include "../parser/tokens.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

PRIVATE void gen_raw_variable(TYPE *t);

/************************************************************************
 *			GEN_FREE_VARS					*
 ************************************************************************
 * For each variable that occurs in t, and that is not in 		*
 * glob_bound_vars or other_vars, do the following.			*
 *   a. add it to list other_vars;					*
 *   b. set aside space in the global environment for it.		*
 *   c. set its TOFFSET field.						*
 *   d. generate it as a raw variable					*
 * This is for use in the preamble of a declaration, for creating       *
 * first copies of variables.						*
 ************************************************************************/

PRIVATE void install_var(TYPE *V)
{
  SET_LIST(other_vars, type_cons(V, other_vars));
  V->TOFFSET = ++num_globals;

# ifdef DEBUG
    if(trace_gen > 1) {
      trace_t(269, toint(V->TOFFSET) - 1);
      trace_ty_without_constraints(V);
      tracenl();
    }
# endif

  gen_raw_variable(V);
  gen_g(TYPE_ONLY_I);
}

/*----------------------------------------------------*/

void gen_free_vars(TYPE *t)
{
  TYPE_TAG_TYPE t_kind;

 tail_recur:
  if(t == NULL) return;
  t = find_u(t);
  t_kind = TKIND(t);

  if(IS_STRUCTURED_T(t_kind)) {
    gen_free_vars(t->TY1);
    t = t->TY2;
    goto tail_recur;
  }

  else if(IS_VAR_T(t_kind)) {
    if(t->TOFFSET == 0) install_var(t);
    if(t->LOWER_BOUNDS != NIL) {
      TYPE_LIST *p;
      for(p = t->LOWER_BOUNDS; p != NIL; p = p->tail) {
        if(LKIND(p) == TYPE_L) {
	  gen_free_vars(p->head.type);
        }
      }
    }
  }
}


/************************************************************************
 *			GENERATE_CONSTRAINT				*
 ************************************************************************
 * Generate constraint ub >= lb.  The other parameters are as for	*
 * bare_generate_type_g.						*
 ************************************************************************/

PRIVATE void
generate_constraint(TYPE *ub, TYPE *lb, HASH2_TABLE **ty_b,
		    int *st_loc, int mode)
{
  bare_generate_type_g(ub, ty_b, st_loc, mode);
  bare_generate_type_g(lb, ty_b, st_loc, mode);
  gen_g(CONSTRAIN_T_I);
}


/************************************************************************
 *			GENERATE_CONSTRAINTS_G				*
 ************************************************************************
 * Generate all constraints on variables in each type in list L.	*
 * This is for use in the preamble of a declaration, for creating       *
 * first copies of variables.						*
 ************************************************************************/

void generate_constraints_g(TYPE_LIST *L)
{
  TYPE_LIST *p, *q;

  for(p = L; p != NIL; p = p->tail) {
    TYPE* t = p->head.type;
    for(q = t->LOWER_BOUNDS; q != NIL; q = q->tail) {
      if(LKIND(q) == TYPE_L) {
	HASH2_TABLE* ty_b = NULL;
	int st_loc = 0;
	TYPE *reducet;
	bump_type(reducet = reduce_type(t));
	if(count_occurrences_1(reducet)) {
	  gen_g(CLEAR_STORAGE_T_I);
        }
        generate_constraint(t, q->head.type, &ty_b, &st_loc, 1);
	clear_seenTimes_1(reducet);
	free_hash2(ty_b);
	drop_type(reducet);
      }
    }
  }
}


/****************************************************************
 *			GEN_STD_VAR_G				*
 ****************************************************************
 * t is a variable G`x or G`*x or G``x, where G is a		*
 * standard genus or commuunity with standard number num.       *
 * Generate an instruction that puts t on the type stack.	*
 ****************************************************************/

PRIVATE void gen_std_var_g(TYPE_TAG_TYPE kind, int num)
{
  gen_g(IS_WRAP_VAR_T(kind)         ? STAND_WRAP_VAR_T_I :
	kind >= FIRST_PRIMARY_VAR_T ? STAND_PRIMARY_VAR_T_I
				    : STAND_VAR_T_I);
  gen_g(num);
}


/****************************************************************
 *			GEN_VAR_G				*
 ****************************************************************
 * t is a variable G`x or G`*x or G``x.  Generate an 		*
 * instruction  that puts t on the type stack.			*
 ****************************************************************/

PRIVATE struct gen_var_info {
   UBYTE instr, dcl_i;
} gen_var_info[] =
{
  /* TYPE_VAR_T          */       {TYPE_VAR_T_I,         GENUS_DCL_I},
  /* FAM_VAR_T           */       {TYPE_VAR_T_I,         COMMUNITY_DCL_I},
  /* PRIMARY_TYPE_VAR_T  */       {PRIMARY_TYPE_VAR_T_I, GENUS_DCL_I},
  /* PRIMARY_FAM_VAR_T   */       {PRIMARY_TYPE_VAR_T_I, COMMUNITY_DCL_I},
  /* WRAP_TYPE_VAR_T     */       {WRAP_TYPE_VAR_T_I,    GENUS_DCL_I},
  /* WRAP_FAM_VAR_T      */       {WRAP_TYPE_VAR_T_I,    COMMUNITY_DCL_I}
};

/*--------------------------------------------------------------------*/

PRIVATE void gen_var_g(TYPE *t)
{
  struct gen_var_info info = gen_var_info[TKIND(t) - TYPE_VAR_T];

  genP_g(info.instr, id_loc(t->ctc->name, info.dcl_i));
}


/************************************************************************
 *			GEN_RAW_VARIABLE				*
 ************************************************************************
 * Generate instructions to push a variable like t onto the type stack. *
 * Do not generate any constraints.					*
 ************************************************************************/

PRIVATE void gen_raw_variable(TYPE *t)
{
  TYPE_TAG_TYPE t_kind = TKIND(t);
  CLASS_TABLE_CELL* t_ctc = t->ctc;

  if     (t_ctc == NULL)       gen_std_var_g(t_kind, 0);
  else if(t_ctc->std_num != 0) gen_std_var_g(t_kind, t_ctc->std_num);
  else                         gen_var_g(t);
  if(t->PREFER_SECONDARY) gen_g(SECONDARY_DEFAULT_T_I);
}


/************************************************************************
 *			GENERATE_MODE1_VARIABLE				*
 ************************************************************************
 * Same as generate_variable, below, but for the case where 		*
 * mode = 1.								*
 ************************************************************************/

PRIVATE void
generate_mode1_variable(TYPE *t)
{
  /*----------------------------------------------------*
   * This variable must already stored in the global    *
   * environment.  If not, complain bitterly.		*
   *----------------------------------------------------*/

  if(t->TOFFSET == 0) {
    die(26);
  }

  gen_g(GLOBAL_FETCH_T_I);
  gen_g(toint(t->TOFFSET) - 1);
}


/************************************************************************
 *			GENERATE_MODE2_VARIABLE				*
 ************************************************************************
 * Same as generate_variable, below, but for the case where 		*
 * mode = 2. Do not use the global environment to store the variable.	*
 *									*
 * The return value is TRUE if this is the first time this variable is  *
 * generated, and FALSE otherwise.					*
 ************************************************************************/

PRIVATE Boolean
generate_mode2_variable(TYPE *t, HASH2_TABLE **ty_b, int *st_loc)
{
  HASH_KEY u;
  HASH2_CELLPTR h = NULL;

  /*------------------------------------------------------------*
   * There might be several occurrences, so we will store it    *
   * for later fecthes.  For now, find out where this variable  *
   * goes in the table. 					*
   *------------------------------------------------------------*/

  if(ty_b != NULL) {
    u.num = tolong(t);
    h     = insert_loc_hash2(ty_b, u, inthash(u.num), eq);
  }

  /*------------------------------------------------------------*
   * If this variable was not previously generated and stored,  *
   * then generate it now and store it. 			*
   *------------------------------------------------------------*/

  if(ty_b == NULL || h->key.num == 0) {
    gen_raw_variable(t);
    if(ty_b != NULL) {
      gen_g(STORE_AND_LEAVE_T_I);
      h->key.num = u.num;
      h->val.num = (*st_loc)++;

#     ifdef DEBUG
	if(trace_gen > 1) {
	  trace_t(449, 2);
	  trace_t(237, t, h->val.num);
	}
#     endif

    }
    return TRUE;
  }

  /*-------------------------------------------------*
   * Otherwise this variable has already been bound. *
   *-------------------------------------------------*/

  else {
    gen_g(GET_T_I);
    gen_g(toint(h->val.num));

#   ifdef DEBUG
      if(trace_gen > 1) {
	trace_t(449, 2);
	trace_t(238, t, h->val.num);
      }
#   endif

    return FALSE;
  }
}


/************************************************************************
 *			GENERATE_VARIABLE				*
 ************************************************************************
 * Generate t, which must be a variable.				*
 *									*
 * Similar to bare_generate_type_g (below), but for a variable. mode	*
 * must be 1 or 2.							*
 *									*
 * Return TRUE if this is the first time this variable has been 	*
 * encountered in the current type, and FALSE if it has been seen	*
 * before.  Also return FALSE if this variable is not generated at all, *
 * but a canonical member is generated instead.				*
 ************************************************************************/

PRIVATE void
generate_variable(TYPE *t, HASH2_TABLE **ty_b, int *st_loc, int mode)
{
# ifdef DEBUG
    if(trace_gen > 1) {
      trace_t(549, toint(t->TOFFSET) - 1);
    }
# endif

  if(mode == 1) {
    generate_mode1_variable(t);
  }
  else {
    if(generate_mode2_variable(t, ty_b, st_loc)) {
      TYPE_LIST *p;
      for(p = t->LOWER_BOUNDS; p != NIL; p = p->tail) {
	if(LKIND(p) == TYPE_L) {
	  generate_constraint(t, p->head.type, ty_b, st_loc, 2);
	}
      }
    }
  }
}


/************************************************************************
 *			GENERATE_STRUCTURED_TYPE_G			*
 ************************************************************************
 * Generate type t.							*
 *									*
 * Similar to bare_generate_type_g (below), but for a		        *
 * FAM_MEM_T, PAIR_T  or FUNCTION_T type.				*
 *									*
 * kind tells what kind (FAM_MEM_T, PAIR_T, FUNCTION_T) type t is.	*
 ************************************************************************/

PRIVATE void
generate_structured_type_g(int kind, TYPE *t, HASH2_TABLE **ty_b,
			   int *st_loc, int mode)
{
  HASH_KEY u;
  HASH2_CELLPTR h;

  /*------------------------------------------------------------*
   * If this type occurs more than once in the expression, then *
   * it should be saved in the type storage the first time it   *
   * is built, and fetched at subsequent encounters.  ty_b      *
   * points to a hash table where locations in the type storage *
   * are put.  Look up what is there, if appropriate.	        *
   *------------------------------------------------------------*/

  h = NULL;
  if(ty_b != NULL && t->seenTimes > 1) {
    u.num = tolong(t);
    h     = insert_loc_hash2(ty_b, u, inthash(u.num), eq);
  }

  /*------------------------------------------------------------*
   * If there was already an entry in ty_b for this type, then  *
   * just generate a fetch from the type storage.  It is 	*
   * possible, due to constraints, to find that a dummy entry	*
   * has been created.  Do not use such an entry.	 	*
   *------------------------------------------------------------*/

  if(h != NULL && h->key.num != 0 && h->val.num >= 0) {

#   ifdef DEBUG
      if(trace_gen > 1) {
	trace_t(449, mode);
	trace_t(225);
	trace_ty(t);
	trace_t(226, t, h->val.num);
      }
#   endif

    gen_g(GET_T_I); gen_g(toint(h->val.num));
  }

  /*-------------------------------------------------------*
   * If this type is not already in the type storage, then *
   * build it now.   First, reserve space in the hash	   *
   * table if necessary.				   *
   *-------------------------------------------------------*/

  else {
    if(h != NULL) {
      h->key.num = u.num;
      h->val.num = -1;
    }

#   ifdef DEBUG
      if(trace_gen > 1) {
	trace_t(227);
	trace_ty(t);
	trace_t(228);
      }
#   endif

    bare_generate_type_g(t->TY1, ty_b, st_loc, mode);
    switch(kind) {
      case PAIR_T:
	bare_generate_type_g(t->TY2, ty_b, st_loc, mode);
	gen_g(PAIR_T_I);
	break;

      case FUNCTION_T:
	bare_generate_type_g(t->TY2, ty_b, st_loc, mode);
	gen_g(FUNCTION_T_I);
	break;

      case FAM_MEM_T:
	{TYPE* fam = find_u(t->TY2);
	 if(TKIND(fam) == FAM_T) {
	   if(fam->ctc == List_ctc) {gen_g(LIST_T_I); break;}
	   if(fam->ctc == Box_ctc)  {gen_g(BOX_T_I);  break;}
	 }
	 bare_generate_type_g(fam, ty_b, st_loc, mode);
	 gen_g(FAM_MEM_T_I);
	 break;
	}

      default: {}
    }

    /*-------------------------------------------------------*
     * Now store this type, if we did a lookup for it above. *
     * It is possible for this same type already to have     *
     * stored during the recursive calls.  Do not store it   *
     * again.						     *
     *-------------------------------------------------------*/

    if(h != NULL) {
      /*------------------------------------------------------*
       * Need to look up again -- the hash table might have   *
       * moved do to insertions done by recursive     	      *
       * calls above.					      *
       *------------------------------------------------------*/

      h = locate_hash2(*ty_b, u, inthash(u.num), eq);
      if(h->val.num < 0) {
	gen_g(STORE_AND_LEAVE_T_I);
	h->val.num = (*st_loc)++;

#       ifdef DEBUG
	  if(trace_gen > 1) {
	    trace_t(449, mode);
	    trace_t(229);
	    trace_ty(t);
	    trace_t(230, t, h->val.num);
	  }
#       endif
      }
    } /* end if(h != NULL) */
  } /* end else (h == NULL || h->key.num == 0) */
}


/************************************************************************
 *			BARE_GENERATE_TYPE_G				*
 ************************************************************************
 * Generate instructions that will push type t onto the type stack.	*
 *									*
 * To avoid generating common subtypes more than once, subtypes are     *
 * put in the type storage. Some types might already be in the type     *
 * storage due to actions of this function.  ty_b associates with       *
 * types their offset in the type storage.                              *
 *                                                                      *
 * *st_loc contains the next offset in the type storage that a 		*
 * STORE_AND_LEAVE_T_I instruction will use to store a type.		*
 * It is updated as types are stored.					*
 *									*
 * If ty_b and st_loc are null, then no common subtype                  *
 * improvement is done.                                                 *
 *									*
 * The generated code is controlled by mode.                            *
 *                                                                      *
 *   mode       meaning                                                 *
 *   ----       --------                                                *
 *     0        t contains no variables.  Just generate it.             *
 *                                                                      *
 *     1        Generate all variables by putting them in the   	*
 *              global environment and fetching them.  In this  	*
 *              mode, the type is generated and stored if it    	*
 *		has not already been generated.  Variables are		*
 *		put in the global environment when they are		*
 *		first encountered.    					*
 *                                                              	*
 *     2        Generate all variables as new variables, and do         *
 *              not store them in the global environment.  Code         *
 *              to create this type is generated here.                  *
 *									*
 * NOTE: If this function is to perform common subtype improvement,     *
 * then the type must be processed with reduce_type, and occurrences    *
 * of subtypes must be counted, as is done in generate_type_g below.    *
 *                                                                      *
 * XREF:								*
 *   Called below, and by genglob.c, which wants careful control	*
 *   over how types are generated.					*
 ************************************************************************/

void bare_generate_type_g(TYPE *t, HASH2_TABLE **ty_b, int *st_loc, int mode)
{
  TYPE_TAG_TYPE t_kind;

  /*--------------------------------*
   * We can't generate a null type. *
   *--------------------------------*/

  if(t == NULL_T) {
#   ifdef DEBUG
      if(trace) {
	trace_t(449, mode); 
	trace_t(223);
      }
#   endif
    semantic_error(TYPE_ERR, 0);
    return;
  }

  t      = find_u(t);
  t_kind = TKIND(t);

# ifdef DEBUG
    if(trace_gen) {
      trace_t(449, mode);
      trace_t(224);
      trace_ty(t);
      tracenl();
    }
# endif

  switch(t_kind) {
    case BAD_T:
        return;

    case FUNCTION_T:
    case PAIR_T:
    case FAM_MEM_T:
      generate_structured_type_g(t_kind, t, ty_b, st_loc, mode);
      return;

    case TYPE_ID_T:
    case FAM_T:
	if(t->ctc->std_num != 0) {
	  gen_g(STAND_T_I);
	  gen_g(t->ctc->std_num);
	}
	else {
	  int dcl_i = (t_kind != FAM_T)  ? SPECIES_DCL_I : FAMILY_DCL_I;
	  genP_g(TYPE_ID_T_I, id_loc(t->ctc->name, dcl_i));
	}
	return;

    case FICTITIOUS_TYPE_T:
    case FICTITIOUS_FAM_T:
    case FICTITIOUS_WRAP_TYPE_T:
    case FICTITIOUS_WRAP_FAM_T:

      /*-------------------------------------------------*
       * We should never be generating fictitious types. *
       *-------------------------------------------------*/

      die(171);

    case WRAP_TYPE_T:
    case WRAP_FAM_T:

	{CLASS_TABLE_CELL* t_ctc = t->ctc;
	 if(t_ctc == NULL) {
	   gen_g(STAND_WRAP_T_I);
	   gen_g(0);
	 }
	 else if(t_ctc->std_num != 0 &&
		 (t_ctc->code == GENUS_ID_CODE
		  || t_ctc->code == COMM_ID_CODE)) {
	   gen_g(STAND_WRAP_T_I);
	   gen_g(t_ctc->std_num);
	 }
	else {
          int dcl_i = (t_kind != WRAP_FAM_T) ? GENUS_DCL_I : COMMUNITY_DCL_I;
	  genP_g(WRAP_TYPE_ID_T_I, id_loc(t_ctc->name, dcl_i));
	}
	return;
       }

    case FAM_VAR_T:
    case TYPE_VAR_T:
    case PRIMARY_TYPE_VAR_T:
    case PRIMARY_FAM_VAR_T:
    case WRAP_FAM_VAR_T:
    case WRAP_TYPE_VAR_T:
      generate_variable(t, ty_b, st_loc, mode);
      return;

    default:
	die(32, (char *) tolong(t_kind));
  }
}


/****************************************************************
 *			GENERATE_TYPE_G		        	*
 ****************************************************************
 * generate_type_g generates code that pushes type t on the     *
 * type stack.                                                  *
 *                                                              *
 * The generated code is controlled by mode.                    *
 *                                                              *
 *   mode       meaning                                         *
 *   ----       --------                                        *
 *     0        Generate this type by putting it in the global  *
 *              environment, and fetching it.  This is the      *
 *              usual mode.  The type is put in the global      *
 *              environment just by asking global_offset_g to   *
 *              record it for future generation.  No code for   *
 *              the type is actually generated.                 *
 *                                                              *
 *     1        Generate all variables by putting them in the   *
 *              global environment and fetching them.  In this  *
 *              mode, the type is generated and stored if it    *
 *		has not already been generated.  Variables are	*
 *		put in the global environment when they are	*
 *		first encountered.    				*
 *                                                              *
 *     2        Generate all variables as new variables, and do *
 *              not store them in the global environment.  Code *
 *              to create this type is generated here.          *
 ****************************************************************/

void generate_type_g(TYPE *t, int mode)
{
  /*------------------------------------------------------------*
   * If we are going to generate this type now, then replace    *
   * common subtypes by identical nodes.  This allows           *
   * us to generate common subtypes only once.                  *
   *                                                            *
   * Then count occurrences of types in the new type, so that   *
   * only subtypes that occur more than once will be stored to  *
   * be fetched again.  Prepare to generate, by clearing the    *
   * type storage, and generate the type.                       *
   *------------------------------------------------------------*/

  if(mode > 0) {
    HASH2_TABLE* ty_b = NULL;
    int st_loc = 0;
    TYPE *reducet;
    bump_type(reducet = reduce_type(t));
    if(count_occurrences_1(reducet)  || mode == 2) {
      gen_g(CLEAR_STORAGE_T_I);
    }
    bare_generate_type_g(reducet, &ty_b, &st_loc, mode);
    clear_seenTimes_1(reducet);
    free_hash2(ty_b);
    drop_type(reducet);
  }

  /*----------------------------------------------------*
   * For mode 0, all variables in the type must 	*
   * already be in the global environment.  Install	*
   * any that need installing.  Then store this		*
   * type in the environment and fetch it, provided	*
   * it is not just a simple type or family.  If 	*
   * it is just a type or family, generate it		*
   * directly now.  					*
   *----------------------------------------------------*/

  else {
    TYPE_TAG_TYPE t_kind = TKIND(t);
    if(IS_TYPE_T(t_kind)) {
      bare_generate_type_g(t, NULL, NULL, 0);
    }
    else {
      int index;
      index = type_offset_g(t);
      gen_g(GLOBAL_FETCH_T_I);
      gen_g(index);
    }
  }
}


