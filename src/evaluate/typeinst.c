/**********************************************************************
 * File:    evaluate/typeinst.c
 * Purpose: Evaluate type instructions
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

/************************************************************************
 * This file is responsible for evaluating instructions that manipulate *
 * the type stack and create types for the interpreter.			*
 ************************************************************************/

#include <memory.h>
#ifdef USE_MALLOC_H
#  include <malloc.h>
#else
#  include <stdlib.h>
#endif
#include "../misc/misc.h"
#include "../utils/lists.h"
#include "../utils/rdwrt.h"
#include "../alloc/allocate.h"
#include "../classes/classes.h"
#include "../standard/stdtypes.h"
#include "../machdata/entity.h"
#include "../machdata/except.h"
#include "../machstrc/machstrc.h"
#include "../intrprtr/intrprtr.h"
#include "../evaluate/evaluate.h"
#include "../evaluate/instruc.h"
#include "../tables/tables.h"
#include "../clstbl/classtbl.h"
#include "../clstbl/typehash.h"
#include "../parser/tokens.h"
#include "../rts/rts.h"
#include "../unify/unify.h"
#include "../unify/constraint.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/****************************************************************
 *			PUBLIC VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			type_storage				*
 *			type_open				*
 ****************************************************************
 * type_storage is an array where temporary types are stored.   *
 * See instructions STORE_T_I and GET_T_I.			*
 *								*
 * type_open is the next available spot in type_storage.	*
 *								*
 * type_storage is reallocated if more space is needed. 	*
 * type_storage_size is the current physical size of array      *
 * type_storage.						*
 ****************************************************************/

PRIVATE TYPE* FARR    type_storage_space[TYPE_STORE_SIZE]; 
PRIVATE TYPE* HUGEPTR type_storage      = type_storage_space;
PRIVATE int           type_storage_size = TYPE_STORE_SIZE;
TYPE* HUGEPTR        type_open         = type_storage_space;

/****************************************************************
 *			type_stack				*
 *			type_stack_top				*
 ****************************************************************
 * type_stack is the temporary stack for evaluation of type     *
 * expressions by the interpreter.  See, for example,		*
 * STAND_T_I, PAIR_T_I, HEAD_T_I, SWAP_T_I.  Type results are   *
 * typically left on the top of this stack.			*
 *								*
 * type_stack_top is the top occupied cell in type_stack.  It   *
 * is one less than type_stack if the stack is empty.		*
 *								*
 * type_stack is reallocated if more space is needed. 		*
 * type_stack_size is the current physical size of array        *
 * type_stack.						 	*
 ****************************************************************/

PRIVATE TYPE* FARR type_stack_space[TYPE_STACK_SIZE];  
TYPE* HUGEPTR     type_stack      = type_stack_space;
PRIVATE int        type_stack_size = TYPE_STACK_SIZE;
TYPE* HUGEPTR	  type_stack_top  = type_stack_space - 1;


/*********************************************************
 *			CLEAR_TYPE_STORAGE		 *
 *********************************************************
 * Empty the type storage, dropping reference counts of  *
 * all current entries.					 *
 *********************************************************/

PRIVATE void clear_type_storage(void)
{
  while(type_open > type_storage) {
    drop_type(*(--type_open));
  }
}


/****************************************************************
 *			REMEMBER_TYPE_STORAGE			*
 ****************************************************************
 * Record the information in the type storage and the type      *
 * stack into structure info, and clear the type stack and      *
 * storage.							*
 ****************************************************************/

void remember_type_storage(TYPE_HOLD *info)
{
  int k;
  info->type_stack_occ   =  type_stack_top - type_stack + 1;
  info->type_storage_occ = type_open - type_storage;
  info->type_stack = 
	  (TYPE **) BAREMALLOC(info->type_stack_occ * sizeof(TYPE *));
  info->type_storage = 
	  (TYPE **) BAREMALLOC(info->type_storage_occ * sizeof(TYPE *));

  for(k = 0; k < info->type_stack_occ; k++) {
    bump_type(info->type_stack[k] = type_stack[k]);
  }
  for(k = 0; k < info->type_storage_occ; k++) {
    bump_type(info->type_storage[k] = type_storage[k]);
  }
  clear_type_stk();
}
  

/****************************************************************
 *			RESTORE_TYPE_STORAGE			*
 ****************************************************************
 * Restore the type storage to what is described in info.  This *
 * must undo the action of remember_type_storage.		*
 ****************************************************************/

void restore_type_storage(TYPE_HOLD *info)
{
  clear_type_stk();
  memcpy(type_stack, info->type_stack, info->type_stack_occ*sizeof(TYPE *));
  memcpy(type_storage, info->type_storage, 
	 info->type_storage_occ*sizeof(TYPE *));
  type_stack_top = type_stack + info->type_stack_occ - 1;
  type_open      = type_storage + info->type_storage_occ;
  FREE(info->type_stack);
  FREE(info->type_storage);
  info->type_stack = info->type_storage = NULL;
}
  

/********************************************************
 *		REALLOCATE_TYPE_ARRAY			*
 ********************************************************
 * Reallocate the type_storage or type_stack array,	*
 * making it bigger.   The parameters are		*
 *							*
 * array_begin  : the array itself (type_storage or	*
 *	      	  type_stack).				*
 * array_current: the current pointer (type_open or	*
 * 		  type_stack_top).			*
 * array_size   : the current size.			*
 * init_size    : the initial array size.		*
 ********************************************************/

PRIVATE void reallocate_type_array(TYPE* HUGEPTR * array_begin, 
				  TYPE* HUGEPTR * array_current,
				  int* array_size,
				  int init_size)
{
  int    oldsize          = *array_size;
  int    oldsize_in_bytes = oldsize * sizeof(TYPE*);
  int    newsize          = oldsize + oldsize;
  TYPE* HUGEPTR new_array = alloc(newsize * sizeof(TYPE*));
  longmemcpy(new_array, *array_begin, oldsize_in_bytes);
  if(oldsize > init_size) {
    give_to_small((charptr)(*array_begin), oldsize_in_bytes);
  }
  *array_current = new_array + (*array_current - *array_begin);
  *array_begin   = new_array;
  *array_size    = newsize;
}


/********************************************************
 *		REALLOCATE_TYPE_STORAGE			*
 ********************************************************
 * Reallocate the type_storage array, making it bigger. *
 ********************************************************/

PRIVATE void reallocate_type_storage(void)
{
  reallocate_type_array(&type_storage, &type_open, &type_storage_size,
			TYPE_STORE_SIZE);
}


/********************************************************
 *			STORE_A_TYPE			*
 ********************************************************
 * Store the type that is on the top of the type stack  *
 * in the type storage at location type_open, 		*
 * and increment type_open.  The type stack is not 	*
 * popped.						*
 *							*
 * The reference count of *type_open is not incremented *
 * -- that must be done by the caller if necessary.  	*
 ********************************************************/

PRIVATE void store_a_type(void)
{
  if(type_open >= type_storage + type_storage_size - 1) {
    reallocate_type_storage();
  }
  *(type_open++) = *type_stack_top;  /* Ref handled by caller */
}


/***************************************************************
 *                 PRINT_TYPE_STORE                            *
 ***************************************************************
 * Print the contents of the type store, for debugging.        *
 ***************************************************************/

#ifdef DEBUG
void print_type_store(void)
{
  TYPE * HUGEPTR tt;

  if(type_open > type_storage) {
    trace_i(337);
    set_in_binding_list();
    for(tt = type_storage; tt < type_open; tt++) {
      trace_ty(*tt);
      tracenl();
    }
    get_out_binding_list();
  }
}
#endif

/***************************************************************
 *                 PRINT_TYPE_STK                              *
 ***************************************************************
 * Print the contents of the type stack, for debugging.        *
 ***************************************************************/

#ifdef DEBUG
void print_type_stk(void)
{
  TYPE * HUGEPTR tt;

  if(!type_stk_is_empty()) {
    trace_i(80);
    set_in_binding_list();
    for(tt = type_stack_top; tt >= type_stack; tt--) {
      trace_ty(*tt);
      tracenl();
    }
    get_out_binding_list();
  }
}
#endif


/***************************************************************
 *                 CLEAR_TYPE_STK                              *
 ***************************************************************
 * Empty out the type stack and the type storage,              *
 * dropping the reference count of each type that they         *
 * currently contain.                                          *
 ***************************************************************/

void clear_type_stk(void)
{
  while(type_stack_top >= type_stack) {
    drop_type(*(type_stack_top--));
  }

  clear_type_storage();
  type_stack_top = type_stack - 1;
  type_open      = type_storage;
}


/********************************************************
 *		REALLOCATE_TYPE_STACK			*
 ********************************************************
 * Reallocate the type_stack array, making it bigger.   *
 ********************************************************/

PRIVATE void reallocate_type_stack(void)
{
  reallocate_type_array(&type_stack, &type_stack_top, &type_stack_size,
			TYPE_STACK_SIZE);
}


/********************************************************
 *			PUSH_A_TYPE			*
 ********************************************************
 * Push type t onto the type stack.  This function	*
 * bumps the reference count of t.  			*
 ********************************************************/

void push_a_type(TYPE *t)
{
  if(type_stack_top >= type_stack + type_stack_size - 1) {
    reallocate_type_stack();
  }
  bump_type(*(++type_stack_top) = t);
}


/********************************************************
 *			FREEZE_TYPE_STK_TOP		*
 ********************************************************
 * Replace the top of the type stack by the result of   *
 * freezing that type.					*
 ********************************************************/

void freeze_type_stk_top(void)
{
  SET_TYPE(*type_stack_top, freeze_type(*type_stack_top));
}


/********************************************************
 *			GET_CTCNUM			*
 ********************************************************
 * Returns the index in the class table of a type, 	*
 * family, community, etc.  				*
 *							*
 * If f is not NULL, then a global label is read from 	*
 * file f, and the index of the thing defined at that 	*
 * global label is returned, by looking it up in the 	*
 * run-time code array.  This form is used when reading	*
 * in a package.					*
 *							*
 * If f is NULL, then the index is found at address cc	*
 * instead, and cc is bumped up to point just after the	*
 * index.  This form is used during evaluation to get	*
 * an index from the code array.  			*
 *							*
 * If f is NULL, then flag use_labels can be used to 	*
 * force an extra level of indirection.  When use_labels*
 * is true, the index at address cc is interpreted as	*
 * a global label, and the index stored at that global	*
 * label is returned.					*
 ********************************************************/

PRIVATE int get_ctcnum(FILE *f, CODE_PTR *cc, Boolean use_labels)
{
  register int n;

  if(f != NULL) {n = toint(index_at_label(toint(get_int_m(f))));}
  else {
    n = toint(next_int_m(cc));
    if(use_labels) n = toint(index_at_label(n));
  }
  return n;
}


/****************************************************************
 *			TYPE_INSTRUCTION_I			*
 ****************************************************************
 * Execute type instruction i.  If f != NULL, then the next	*
 * bytes, if needed, are obtained from file f, and the offset is*
 * relative to *cc.  Otherwise, the next bytes are obtained 	*
 * from the code at location *cc, and *cc is incremented.	*
 * Bindings are from environment env.  If use_labels is true or *
 * f != NULL, global label arguments to instructions are true 	*
 * global labels, not offsets.					*
 ****************************************************************/

void type_instruction_i(int i, CODE_PTR *cc, FILE *f, 
			ENVIRONMENT *env, Boolean use_labels)
{
  TYPE* (*make_var_fun)(CLASS_TABLE_CELL *);

# ifdef DEBUG
    if(trace_types) {
      trace_i(115);
      if(type_stack_top >= type_stack) {
	print_type_stk();
      }
      else trace_i(118);
      if(f == NULL) {
        fprintf(TRACE_FILE,"++");
        print_instruction(TRACE_FILE, *cc, i);
      }
    }
# endif

  switch(i) {
      case STAND_T_I:

	/*------------------------------------------------------*
	 * This is followed by a byte that holds the index of a	*
	 * standard type or family.  Push that type or family   *
         * onto the type stack. 				*
	 *------------------------------------------------------*/

	{register int n = (f != NULL) ? fgetuc(f) : *((*cc)++);
         push_a_type(ctcs[n + (FIRST_STANDARD_TYPE - 1)]->ty);
	 break;
	}

      case STAND_WRAP_T_I:

	/*------------------------------------------------------*
	 * This is followed by a byte that holds 0 or the index *
	 * of a standard genus or community X. 			*
         * Push X as a secondary species onto the type stack. 	*
	 *------------------------------------------------------*/

	{register int n = (f != NULL) ? fgetuc(f) : *((*cc)++);
	 register CLASS_TABLE_CELL* ctc = 
	     (n == 0) ? NULL : ctcs[n + (FIRST_STANDARD_GENUS - 1)];
	 register TYPE* t;
	 bmp_type(t = wrap_tf(ctc));
         push_a_type(type_tb(t));
	 drop_type(t);
	 break;
	}

      case STAND_VAR_T_I:

	/*-------------------------------------------------------*
	 * This instruction is followed by a byte that holds the *
	 * index of a standard genus or community.  Push a 	 *
	 * new variable that ranges over that genus or community *
	 * onto the type stack. 				 *
	 *-------------------------------------------------------*/

         make_var_fun = var_t;
	 goto do_standard_var;  /* below, under STAND_WRAP_VAR_T_I */

      case STAND_PRIMARY_VAR_T_I:

	/*-------------------------------------------------------*
	 * This instruction is followed by a byte that holds the *
	 * index of a standard genus or community.  Push a 	 *
	 * new primary variable that ranges over that genus or   *
	 * community onto the type stack. 			 *
	 *-------------------------------------------------------*/

	make_var_fun = primary_var_t;
	goto do_standard_var;  /* below, under STAND_WRAP_VAR_T_I */

      case STAND_WRAP_VAR_T_I:

	/*-------------------------------------------------------*
	 * This instruction is followed by a byte that holds the *
	 * index of a standard genus or community.  Push a 	 *
	 * new wrap variable that ranges over that genus or	 *
	 * community onto the type stack. 			 *
	 *-------------------------------------------------------*/

	make_var_fun = wrap_var_t;
      
      do_standard_var:

	{register int n = (f != NULL) ? fgetuc(f) : *((*cc)++);
	 register CLASS_TABLE_CELL* ctc = 
	   (n == 0) ? NULL : ctcs[n + (FIRST_STANDARD_GENUS - 1)];
	 push_a_type(make_var_fun(ctc));
	 break;
	}

      case TYPE_ID_T_I:
      case TYPE_VAR_T_I:

	/*------------------------------------------------------*
	 * This instruction is followed by a global label or 	*
	 * index of a species, family, genus or community.  	*
	 * a type or variable.  These are similar to the 	*
	 * preceding two instructions, but are for nonstandard	*
	 * types, etc.						*
	 *------------------------------------------------------*/

	{register int n = get_ctcnum(f, cc, use_labels);
	 register TYPE* t = ctcs[n]->ty;
	 if(i == TYPE_VAR_T_I) t = copy_type_node(t);
	 push_a_type(t);
	 break;
	}

      case PRIMARY_TYPE_VAR_T_I:
	{register int n = get_ctcnum(f, cc, use_labels);
	 register TYPE* t = ctcs[n]->ty;
	 t = copy_type_node(t);
	 t->kind = (TKIND(t) == TYPE_VAR_T) 
	              ? PRIMARY_TYPE_VAR_T
		      : PRIMARY_FAM_VAR_T;
	 push_a_type(t);
	 break;
	}

      case WRAP_TYPE_ID_T_I:
      case WRAP_TYPE_VAR_T_I:

	/*------------------------------------------------------*
	 * This instruction is followed by a global label or 	*
	 * index of a genus or community X.  			*
	 *							*
         * Push X as a secondary species for a WRAP_TYPE_ID_T   *
	 * instruction, and X``a for a WRAP_TYPE_VAR_T_I	*
	 * instruction.						*
	 *------------------------------------------------------*/

	{register int n = get_ctcnum(f, cc, use_labels);
	 register CLASS_TABLE_CELL* ctc = ctcs[n];
	 register TYPE* t;
	 if(i == WRAP_TYPE_ID_T_I) {
	   register TYPE* tt;
	   bmp_type(tt = wrap_tf(ctc));
	   t = type_tb(tt);
	   drop_type(tt);
	 }
         else {
	   t = wrap_var_t(ctc);
	 }
	 push_a_type(t);
	 break;
	}

      case CONSTRAIN_T_I:
	{TYPE* t   = pop_type_stk();   /* Ref from type_stack */
         TYPE* tt  = pop_type_stk();   /* Ref from type_stack */
	 if(!CONSTRAIN(tt, t, 0, 0)) failure = SPECIES_EX;
	 drop_type(t);
	 drop_type(tt);
	 break;
	}

      case FUNCTION_T_I:
      case PAIR_T_I:

	/*--------------------------------------------------------------*
	 * Pop the two types on top of the type stack, and build	*
	 * either a function type or a pair type from them.		*
	 *--------------------------------------------------------------*/

	{register TYPE* x;
	 register TYPE* t = pop_type_stk();   /* Ref from type_stack */
	 bump_type(x = (i == FUNCTION_T_I) ? function_t(*type_stack_top, t)
				           : pair_t(*type_stack_top, t));
	 set_type(type_stack_top, type_tb(x));
	 drop_type(t);
	 drop_type(x);
	 break;
	}

      case LIST_T_I:

	/*---------------------*
	 * Pop t and push [t]. *
	 *---------------------*/

	{register TYPE *t;
	 bump_type(t = list_t(*type_stack_top));
	 set_type(type_stack_top, type_tb(t));
	 drop_type(t);
	 break;
	}

      case BOX_T_I:

	/*-------------------------*
	 * Pop t and push Box(t).  *
	 *-------------------------*/

	{register TYPE *t;
	 bump_type(t = box_t(*type_stack_top));
	 set_type(type_stack_top, type_tb(t));
	 drop_type(t);
	 break;
	}

      case FAM_MEM_T_I:

	/*----------------------------*
	 * Pop t and f and push f(t). *
	 *----------------------------*/

	{register TYPE* x;
	 register TYPE* t = pop_type_stk();  /* ref from type stack */
	 bump_type(x = fam_mem_t(t, *type_stack_top));
	 set_type(type_stack_top, type_tb(x));
	 drop_type(t);
	 drop_type(x);
	 break;
        }

      case UNPAIR_T_I:

	/*--------------------------------------------------------*
	 * Pop (a,b) or (a -> b) or b(a) (b a family) and push a  *
	 * and b, in that order, leaving b on the top of the	  *
	 * stack. 						  *
	 *--------------------------------------------------------*/

	{TYPE* t = *type_stack_top;      /* ref from type stack */
	 register TYPE_TAG_TYPE t_kind = TKIND(t);
	 if(t_kind >= FIRST_BOUND_T) {
	   SET_TYPE(t, find_u(t));
	   t_kind = TKIND(t);
	 }
	 if(!IS_STRUCTURED_T(t_kind)) die(177);
	 bump_type(*type_stack_top = t->TY1);
         push_a_type(t->TY2);
	 drop_type(t);
	 break;
	}

      case HEAD_T_I:
      case TAIL_T_I:

	/*------------------------------------*
	 * HEAD_T_I pops t and pushes t->TY1. *
	 * TAIL_T_I pops t and pushes t->TY2. *
	 *------------------------------------*/

	{register TYPE*         t      = *type_stack_top;
	 register TYPE_TAG_TYPE t_kind = TKIND(t);
	 if(t_kind >= FIRST_BOUND_T) {
	   t      = find_u(t);
	   t_kind = TKIND(t);
	 }
	 if(!IS_STRUCTURED_T(t_kind)) die(177);
	 set_type(type_stack_top, (i == HEAD_T_I) ? t->TY1 : t->TY2);
	 break;
        }

      case POP_T_I:

	/*--------------------------*
	 * Pop t and throw it away. *
	 *--------------------------*/

	drop_type(pop_type_stk());
	break;

      case SWAP_T_I:

	/*----------------------------------------------*
	 * Swap the two types on top of the type stack. *
	 *----------------------------------------------*/

	{register TYPE* temp = type_stack_top[0];
	 type_stack_top[0] = type_stack_top[-1];
         type_stack_top[-1] = temp;
	 break;
        }

      case CLEAR_STORAGE_T_I:

	/*-----------------------------*
	 * Empty out the type storage. *
	 *-----------------------------*/

	clear_type_storage();
	break;

      case STORE_T_I:

	/*-----------------------------------------------------------------*
	 * Pop t and store it at the current location in the type storage, *
	 * and bump the current open location to the next spot.		   *
	 *-----------------------------------------------------------------*/

	store_a_type();
	type_stack_top--;  /* Storage inherits ref, so no drop. */
	break;

      case STORE_AND_LEAVE_T_I:

	/*------------------------------------------*
	 * Like STORE_T_I, but don't pop the stack. *
	 *------------------------------------------*/

	store_a_type();
	bump_type(type_open[-1]);   /* Ref for stored value */
	break;

      case GET_T_I:

	/*------------------------------------------------------*
	 * This instruction is followed by a byte that holds an *
	 * index in the type storage.  Push the contents of the	*
	 * type storage at that index.				*
	 *------------------------------------------------------*/

	{register int n = (f != NULL) ? fgetuc(f) : *((*cc)++);
	 push_a_type(type_storage[n]);

#	 ifdef DEBUG
	   if(trace_types) {
	     trace_i(117, n);
	     trace_ty(*type_stack_top);
	     tracenl();
	   }
#	 endif
	 break;
	}

      case SECONDARY_DEFAULT_T_I:
	(*type_stack_top)->PREFER_SECONDARY = 1;
	break;

      case GLOBAL_FETCH_T_I:
	
	/*------------------------------------------------------*
	 * This instruction is followed by a byte that holds an	*
	 * offset in the current global environment.  Push the	*
	 * type stored at that offset in the global environment.*
	 *------------------------------------------------------*/

	{register int n = (f != NULL) ? fgetuc(f) : *((*cc)++);
	 push_a_type(type_env(env, n));
	 break;
	}

      default:

	/*-----------------------------------------------------------*
	 * If the instruction is anything else, we have big trouble. *
	 *-----------------------------------------------------------*/

	die(73, (char *) (LONG) i);

  } /* end switch */

# ifdef DEBUG
    if(trace_types) {
      if(type_stack_top >= type_stack) {
        trace_ty(*type_stack_top);
	tracenl();
      }
      else trace_i(118);
      tracenl();
    }
# endif
}


/********************************************************
 *			EVAL_TYPE_INSTRS		*
 ********************************************************
 * Evaluate type instructions in array p, to END_LET_I,	*
 * in environment env.  Leave the result on the top of	*
 * the type stack.					*
 ********************************************************/

void eval_type_instrs(CODE_PTR p, ENVIRONMENT *env)
{
  int c;

# ifdef DEBUG
   if(trace_puts) {
     CODE_PTR q = p;
     trace_i(119);
     while(*q != END_LET_I) {
       fprintf(TRACE_FILE,"   %d\n", toint(*q)); q++;
     }
     fprintf(TRACE_FILE,"   %d\n", END_LET_I);
     trace_i(120, type_stack_top, type_open);
   }
# endif

  while((c = *p) != END_LET_I) {
    p++;
    type_instruction_i(c, &p, NULL, env, 0);
  }
}


/********************************************************
 *	   EVAL_TYPE_INSTRS_WITH_BINDING_LIST		*
 ********************************************************
 * Evaluate type instructions in array p, to END_LET_I,	*
 * in environment env, using binding list bl for	*
 * variable bindings.  Return the result type.		*
 *							*
 * This function clears the type stack and type storage.*
 ********************************************************/

TYPE*
eval_type_instrs_with_binding_list(CODE_PTR p, ENVIRONMENT *env, LIST *bl)
{
  TYPE *result;
  clear_type_stk();
  set_in_binding_list_to(bl);
  eval_type_instrs(p, env);
  bump_type(result = freeze_type(top_type_stk()));
  get_out_binding_list_to(&bl);
  clear_type_stk();
  if(result != NULL) result->ref_cnt--;
  return result;
}
