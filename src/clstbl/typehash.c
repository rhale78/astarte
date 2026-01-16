/**********************************************************************
 * File:    clstbl/typehash.c
 * Purpose: hash table for types
 * Author:  Karl Abrahamson
 **********************************************************************/

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

/**********************************************************************
 * In the interpreter, types that have no variables are stored in a   *
 * hash table to avoid storing them more than once and to make	      *
 * comparing them for equality easy.  This file manages computing     *
 * hash values for types and keeping the table.			      *
 *								      *
 * Types that have variables are also handled by the hash function,   *
 * but they are not stored in the table.  			      *
 **********************************************************************/

#include <stdlib.h>
#include "../misc/misc.h"
#include "../utils/hash.h"
#include "../alloc/allocate.h"
#include "../classes/classes.h"
#include "../clstbl/typehash.h"
#include "../standard/stdtypes.h"
#include "../unify/unify.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif


/****************************************************************
 *			PUBLIC VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			use_the_act_bindings			*
 ****************************************************************
 * When use_the_act_bindings is nonzero, type_tb takes into 	*
 * account variable bindings in the_act.type_binding_lists.	*
 ****************************************************************/

int use_the_act_bindings = 0;

/****************************************************************
 *			PRIVATE VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			type_table				*
 ****************************************************************
 * Each species that is created by the interpreter is stored    *
 * in type_table.  There are two reasons for doing that.	*
 *								*
 *  1. The interpreter keeps only one copy of each species.	*
 *								*
 *  2. Species can be compared for equality by comparing	*
 *     pointers.						*
 *								*
 ****************************************************************/

PRIVATE HASH1_TABLE* type_table = NULL;


/****************************************************************
 *			EQUALTYPE				*
 ****************************************************************
 * equaltype tests that two type nodes have the  		*
 * same values in them.  It is used for equality tests for hash *
 * table searches.						*
 *								*
 * IMPORTANT: equal_type presumes that pointers in the nodes 	*
 * point to equal structures only if they are the same pointer. *
 * So the subtrees are assumed already to be in the type table. *
 * It also presumes that ther are no bound variables to skip.   *
 *								*
 * equal_type does not work correctly for fictional types.      *
 * Such types should not be encountered in the interpreter.     *
 ****************************************************************/

PRIVATE Boolean equaltype(HASH_KEY a, HASH_KEY b)
{
  register TYPE_TAG_TYPE a_kind = TKIND(a.type);

  if(IS_STRUCTURED_T(a_kind)) {
    return a_kind == TKIND(b.type) && 
           a.type->TY1 == b.type->TY1 && 
           a.type->TY2 == b.type->TY2;
  }
  else {
    return a_kind == TKIND(b.type) && 
	   a.type->ctc == b.type->ctc;
  }
}


/****************************************************************
 *			TYPE_TB					*
 ****************************************************************
 * Returns a pointer to a type that is structurally the 	*
 * same as t, but that is the official one of those that is     *
 * found in the type table.					*
 *								*
 * If t has no variables, then its THASH field will be nonzero  *
 * afterwards, and will hold the hash value of this type.  	*
 * If t->THASH is positive, then t is presumed to be		*
 * in the table already, and t is returned.			*
 *								*
 * If t has variables, its THASH field will be 0.  		*
 * Bindings in the_act's type binding list are used if		*
 * use_the_act_bindings is true.				*
 *								*
 ****************************************************************/

PRIVATE TYPE* type_tb_help(TYPE *t);

TYPE* type_tb(TYPE *t) {
  TYPE *result;
  if(use_the_act_bindings) set_in_binding_list();
  result = type_tb_help(t);
  if(use_the_act_bindings) get_out_binding_list();
  return result;
}

/*-----------------------------------------------------------*/

PRIVATE TYPE* type_tb_help(TYPE *t)
{
  TYPE_TAG_TYPE t_kind;
  LONG t_hash;

  if(t == NULL) return NULL;

  IN_PLACE_FIND_U_NONNULL(t);

  if(t->THASH > 0) return t;

# ifdef DEBUG
    if(trace_types) {
      trace_i(218);
      trace_ty(t); tracenl();
    }
# endif

  /*-------------------------------------------------------*
   * Look up the subtrees, and replace them by their table *
   * values.  This is necessary, because equaltype does	   *
   * not recursively descend trees, but presumes that      *
   * subtrees are equal just when they have the same 	   *
   * pointer.						   *
   *							   *
   * If a variable is encountered, there is no need to     *
   * try to put t into the table, since the table only     *
   * holds variable-free types.  So jump to quick_exit on  *
   * encoutering a variable.				   *
   *-------------------------------------------------------*/

  bump_type(t);
  t_kind = TKIND(t);
  if(IS_STRUCTURED_T(t_kind)) {  
    TYPE* ty1 = t->TY1;
    TYPE* ty2 = t->TY2;
    if(ty1->THASH == 0) {
      ty1 = type_tb_help(ty1);
      SET_TYPE(t->TY1, ty1);
    }
    if(ty2->THASH == 0) {
      ty2 = type_tb_help(ty2);
      SET_TYPE(t->TY2, ty2);
    }
    if(ty1->THASH == 0 || ty2->THASH == 0) goto quick_exit; /* bottom */
    t_hash = labs(combine3(t_kind, ty1->THASH, ty2->THASH) + 1);
  }

  else if(IS_VAR_T(t_kind)) goto quick_exit;  /* bottom */
  else if(IS_KNOWN_WRAP_TYPE_T(t_kind)){
    t_hash = (t->ctc == NULL) ? 0xffffff : tolong(t->ctc->num) + 0xffffff;
  }
  else {
    t_hash = tolong(t->ctc->num + 1);
  }

  /*-------------------*
   * Look up the root. *
   *-------------------*/

  {HASH1_CELLPTR c;
   HASH_KEY u;

   u.type = t;
   c      = insert_loc_hash1(&type_table, u, t_hash, equaltype);

   /*----------------------------------------------------------*
    * If this is already in the table, just get what is there. *
    *----------------------------------------------------------*/

   if(c->key.num != 0) {
     drop_type(t);
     return c->key.type;
   }

   /*--------------------------------------------*
    * If not already in the table, put it there. *
    *--------------------------------------------*/

   else {
     c->key.type = t;
     t->freeze   = 1;
     t->THASH    = t_hash;
     return t;
   }
  }

 quick_exit:
  if(t != NULL) t->ref_cnt--;
  return t;
}


/****************************************************************
 *			INIT_TYPE_TB				*
 ****************************************************************
 * Create the type table.  Force the standard types into the    *
 * type table.							*
 ****************************************************************/

void init_type_tb()
{
  type_table 	   = create_hash1(9);

  box_fam 	   = type_tb(box_fam);
  outfile_fam 	   = type_tb(outfile_fam);
  list_fam 	   = type_tb(list_fam);
  hermit_type      = type_tb(hermit_type);
  boolean_type 	   = type_tb(boolean_type);
  char_type 	   = type_tb(char_type);
  comparison_type  = type_tb(comparison_type);
  exception_type   = type_tb(exception_type);
  natural_type 	   = type_tb(natural_type);
  integer_type	   = type_tb(integer_type);
  rational_type    = type_tb(rational_type);
  real_type 	   = type_tb(real_type);
  unknownKey_type  = type_tb(unknownKey_type);
  fileMode_type    = type_tb(fileMode_type);
  outfile_type 	   = type_tb(outfile_type);
  string_type 	   = type_tb(string_type);

  WrappedANY_type  	 = type_tb(WrappedANY_type);
  WrappedEQ_type 	 = type_tb(WrappedEQ_type);
  WrappedORDER_type 	 = type_tb(WrappedORDER_type);
  WrappedRANKED_type 	 = type_tb(WrappedRANKED_type);
  WrappedENUMERATED_type = type_tb(WrappedENUMERATED_type);
  WrappedREAL_type 	 = type_tb(WrappedREAL_type);
  WrappedRATIONAL_type 	 = type_tb(WrappedRATIONAL_type);
  WrappedINTEGER_type 	 = type_tb(WrappedINTEGER_type);
  WrappedRRING_type 	 = type_tb(WrappedRRING_type);
  WrappedRFIELD_type 	 = type_tb(WrappedRFIELD_type);
  WrappedRATRING_type	 = type_tb(WrappedRATRING_type);
}



