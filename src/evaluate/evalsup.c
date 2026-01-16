/**********************************************************************
 * File:    evaluate/evalsup.c
 * Purpose: Support for expression evaluator
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
 * This file provides miscellaneous support for the evaluator. 		*
 * Included are								*
 *									*
 *  An identity function on entities					*
 *									*
 *  Support for starting up activations					*
 *									*
 *  Testing for lazy entities						*
 *									*
 *  A function to get a hidden box.					*
 ************************************************************************/

#include <string.h>
#include "../misc/misc.h"
#include "../utils/lists.h"
#include "../utils/rdwrt.h"
#include "../utils/miscutil.h"
#include "../alloc/allocate.h"
#include "../classes/classes.h"
#include "../unify/unify.h"
#include "../machdata/entity.h"
#include "../machdata/except.h"
#include "../machstrc/machstrc.h"
#include "../intrprtr/intrprtr.h"
#include "../evaluate/evaluate.h"
#include "../evaluate/instruc.h"
#include "../rts/rts.h"
#include "../gc/gc.h"
#include "../standard/stdtypes.h"
#include "../standard/stdids.h"
#include "../tables/tables.h"
#include "../show/getinfo.h"
#include "../show/prtent.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif


/****************************************************************
 *			AST_IDF					*
 ****************************************************************
 * This is the identity function.                               *
 ****************************************************************/

ENTITY ast_idf(ENTITY x) {return x;}


/********************************************************
 *			START_NEW_ACTIVATION		*
 ********************************************************
 * Get a private stack for the_act, and set precision   *
 * = -1, to indicate that we are possibly entering a    *
 * thread, and don't know the value of precision in     *
 * this thread.                                         *
 *							*
 * If the_act has kind 2, then set failure = 		*
 * TERMINATE_EX, since this activation has been 	*
 * terminated by another thread.			*
 *							*
 * Perform any leading NAME_I or SNAME_I instruction.	*
 ********************************************************/

void start_new_activation(void)
{
  if(the_act.program_ctr == NULL) return;

  if(the_act.stack == NULL) the_act.stack = new_stack();
  else if(the_act.stack->ref_cnt > 1) {
    the_act.stack->ref_cnt--;
    the_act.stack = get_stack();  /* Ref from get_stack */
  }
  precision = -1;

  if(the_act.kind == 2) {
    failure = TERMINATE_EX;
    last_exception = failure_as_entity = ENTU(TERMINATE_EX);
  }
  else {

    /*-------------------------------------*
     * Perform initial NAME_I or SNAME_I   *
     * instruction. 			   *
     *-------------------------------------*/

    LONG nn;
    if(*the_act.program_ctr == NAME_I) {
      the_act.program_ctr++;
      nn = next_int_m(&the_act.program_ctr);
      the_act.name = outer_bindings[nn].name;
    }
    if(*the_act.program_ctr == SNAME_I) {
      the_act.program_ctr++;
      nn = next_int_m(&the_act.program_ctr);
      the_act.name = CSTRVAL(constants[nn]);
    }
  }
}


/********************************************************
 *			TEST_LAZY			*
 ********************************************************
 * Return 0 if x is lazy, and 1 if x is not lazy.  	*
 * Exception: when k != 0, sometimes return 0 even when *
 * x is not lazy. The determiniation of when to return  *
 * 0 when x is not lazy is done by testing and          *
 * modifying qlazy_count.                               * 
 ********************************************************/

PRIVATE int qlazy_count = QLAZY_COUNT_INIT;

Boolean test_lazy(ENTITY x, int k)
{
  /*---------------------------------------------*
   * If the count tells us to pretend that x is  *
   * lazy, then be lazy, and reset the count to  *
   * a "randomly" chosen number.		 *
   *---------------------------------------------*/

  if(k != 0) {
    if(sortof_random_bit()) qlazy_count--;
    if(qlazy_count <= 0) {
      qlazy_count = QLAZY_COUNT_INIT;
      return 0;
    }
  }

  /*---------------------------------------------*
   * Otherwise, look at x.			 *
   *---------------------------------------------*/
  
  if(is_lazy(x)) return 0;
  return 1;
}


/********************************************************
 *			GET_STACK_DEPTH			*
 ********************************************************
 * Return the current stack depth.			*
 ********************************************************/

ENTITY get_stack_depth(ENTITY herm_unused)
{
  return ast_make_int(the_act.st_depth);
}


/********************************************************
 *			CONTINUATION_NAME		*
 ********************************************************
 * Return the name of the activation that the_act will	*
 * continue with on return.				*
 ********************************************************/

ENTITY continuation_name(ENTITY herm_unused)
{
  return (the_act.continuation == NULL) 
		? make_cstr("nobody")
	        : make_cstr(the_act.continuation->name);
}


/****************************************************************
 *			ACQUIRE_BOX_STDF			*
 ****************************************************************
 * If e is a box, return a pair (e,k), where k is true for a	*
 * nonshared box and false for a shared box.  Fail with		*
 * exception TEST_EX if e is not a box.				*
 *								*
 * This is used to implement the acquireBox function, used	*
 * be standard.asi.  See its use there.				*
 ****************************************************************/

ENTITY acquire_box_stdf(ENTITY e)
{
  int tag = TAG(e);
  if(tag == BOX_TAG) return ast_pair(e, true_ent);

  else if(tag == PLACE_TAG) return ast_pair(e, false_ent);

  else {
    failure = TEST_EX;
    return nil;
  }
}


/****************************************************************
 *			RESTORE_STATE				*
 ****************************************************************
 * Restore the state of the_act to the state that was pushed	*
 * into the_act.state_hold.  Since we don't know the state,	*
 * clear precision, which caches the value of box precision!.	*
 ****************************************************************/

void restore_state(void)
{
  LIST* hold_states = the_act.state_hold;

  SET_STATE(the_act.state_a, hold_states->head.state);
  SET_LIST(the_act.state_hold, hold_states->tail);
  precision = -1;
}

