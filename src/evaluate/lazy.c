/**********************************************************************
 * File:    evaluate/lazy.c
 * Purpose: This package contains functions that build and evaluate
 *          lazy entities.
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
 * This file contains functions that create and evaluate lazy entities, *
 * including those created by await constructs and those created by	*
 * [:...:] constructs.  Lazy primitives are handled in lazyprim.c	*
 * The top level lazy evaluator is in eval.c.				*
 ************************************************************************/

#include <string.h>
#include "../misc/misc.h"
#include "../utils/lists.h"
#include "../utils/hash.h"
#include "../utils/rdwrt.h"
#include "../alloc/allocate.h"
#include "../classes/classes.h"
#include "../unify/unify.h"
#include "../machdata/entity.h"
#include "../machdata/except.h"
#include "../machstrc/machstrc.h"
#include "../intrprtr/intrprtr.h"
#include "../evaluate/evaluate.h"
#include "../evaluate/instruc.h"
#include "../gc/gc.h"
#include "../tables/tables.h"
#include "../clstbl/typehash.h"
#include "../standard/stdids.h"
#include "../show/printrts.h"
#ifdef DEBUG
# include "../debug/debug.h"
# include "../show/prtent.h"
#endif
#ifdef SMALL_STACK
#  ifdef USE_MALLOC_H
#    include <malloc.h>
#  else
#    include <stdlib.h>
#  endif
#endif


/****************************************************************
 *			PUBLIC VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			should_not_recompute			*
 ****************************************************************
 * Set to a positive number when in a context where ALL lazy	*
 * results should be memoized, even those that ask not to be	*
 * memoized.  When such a context is entered, 			*
 * should_not_recompute is incremented.  When the context is	*
 * exited, should_not_recompute is decremented.  This is done   *
 * in evalsup.c, by full_eval.					*
 ****************************************************************/

int should_not_recompute = 0; 


/****************************************************************
 *			MAKE_LAZY				*
 ****************************************************************
 * Creates a lazy entity pointing to an activation for a lazy   *
 * evaluation.  The activation's parts are as follows.		*
 *								*
 *	type_binding_lists from current activation.		*
 *	state_a		from current activation.		*
 *      state_hold      NIL					*
 *      trap_vec_a      from current activation.		*
 *      trap_vec_hold   NIL					*
 *      exception_list  from current activation.		*
 *      control		NULL					*
 *      embedding_tries NULL					*
 *      embedding_marks NULL					*
 *	env		from the current activation.		*
 *	num_entries     max(the_act.num_entries, ne).		*
 *	program_ctr	from parameter pc.			*
 *	continuation	NULL					*
 *	stack		will be set when used.			*
 *	coroutines	NIL					*
 *      in_atomic       0					*
 *      st_depth        0					*
 *      recompute	same as recompute param.  (true indicates*
 *		        should not store result on evaluation.) *
 *	kind		from parameter kind.			*
 *	name		from parameter name.			*
 *	result_type_instrs from parameter type_instrs.		*
 ****************************************************************/

ENTITY make_lazy(CODE_PTR pc, int kind, char *name, 
		 CODE_PTR type_instrs, Boolean recompute, int ne) 
{
  register ACTIVATION *a;
  register DOWN_CONTROL *c;
  ENTITY *ee, e;

  /*------------------------------------------------------------*
   * Build the activation that represents the computation that 	*
   * will compute the value, when needed.			*
   *------------------------------------------------------------*/

  a 			  = allocate_activation();
  bump_list(a->type_binding_lists = the_act.type_binding_lists);
  bump_state(a->state_a   = the_act.state_a);
  bump_trap_vec(a->trap_vec_a = the_act.trap_vec_a);
  bump_list(a->exception_list = the_act.exception_list);
  if(ne < the_act.num_entries) ne = the_act.num_entries;
  a->num_entries          = ne;
  bump_env(a->env 	  = the_act.env, ne);
  a->program_ctr 	  = pc;
  a->state_hold           = NIL;
  a->trap_vec_hold        = NIL;
  a->control              = NULL;
  a->embedding_tries      = NULL;
  a->embedding_marks      = NULL;
  a->continuation	  = NULL;
  a->stack		  = NULL;
  a->coroutines		  = NIL;
  a->in_atomic            = 0;
  a->kind		  = kind;
  a->name                 = name;
  a->pack_name		  = NULL;
  a->st_depth             = 0;
  a->result_type_instrs   = type_instrs;

  /*------------------------------------------------------------------*
   * A lazy entity actually points to a control.  Build that control. *
   * Tell the garbage collector that c is a control that belongs to   *
   * a lazy entity (using note_control).			      *
   *------------------------------------------------------------------*/

  c			  = allocate_control();
  c->info		  = BRANCH_F | LCHILD_CTLMASK;
  c->identity             = 0;
  c->right.ctl		  = NULL;
  c->left.act		  = a;
  c->recompute		  = recompute;
  note_control(c);

  /*----------------------------------------------------------*
   * Build the lazy entity. Be sure to return an indirection. *
   *----------------------------------------------------------*/

  e			  = ENTP(LAZY_TAG, c);
  ee			  = allocate_entity(1);
  *ee                     = e;
  return ENTP(INDIRECT_TAG, ee);
}


/*************************************************************
 *			MAKE_TIMEOUT_LAZY      		     *
 *************************************************************
 * Return a lazy value that continues timed-out activation   *
 * the_act.  This is used for cases where a lazy computation *
 * has run out of time, and wants to return a new lazy value *
 * that picks up the computation where the current 	     *
 * computation left off.  Parameter tag is either LAZY_TAG   *
 * or LAZY_LIST_TAG, depending on whether a (:...:) lazy     *
 * computation or a [:...:] computation has timed out.	     *
 *							     *
 * Parameter recompute is the recompute flag of the lazy     *
 * value (1 = do not memoize result).			     *
 *							     *
 * This function uses information in variables timed_out_kind*
 * and timed_out_comp, from fail.c.			     *
 *************************************************************/

PRIVATE ENTITY make_timeout_lazy(int tag, Boolean recompute)
{
  ENTITY e, *ee;
  CONTROL *c;

  /*---------------------------------------------------------------*
   * If what has timed out is a simple activation, without control *
   * information, just build a control that refers to the timed-   *
   * out activation.						   *
   *---------------------------------------------------------------*/

  if(timed_out_kind != CTL_F) {
    c = allocate_control();
    c->identity = 0;
    if(tag != LAZY_LIST_TAG) {

      /*---------------------------------------------------------------*
       * Build a down-control for a LAZY_TAG entity. Indicate that the *
       * left child of the control is an activation by setting         *
       * LCHILD_CTLMASK bit.  See machstrc/control.c.		       *
       *---------------------------------------------------------------*/

      c->info            = BRANCH_F | LCHILD_CTLMASK;
      c->left.act        = timed_out_comp.act;   /* Ref from timed_out_comp. */
      c->right.ctl       = NULL;
    }
    else {

      /*---------------------------------------------------------------*
       * Build an up-control for a LAZY_LIST_TAG entity. Indicate that *
       * right child is an activation by setting RCHILD_CTLMASK bit.   *
       * See machstrc/control.doc.				       *
       *---------------------------------------------------------------*/

      c->info 		 = BRANCH_F | RCHILD_CTLMASK;
      c->PARENT_CONTROL	 = NULL;
      c->right.act	 = timed_out_comp.act; /* Ref from timed_out_comp. */
    }
  }

  /*--------------------------------------------------------------------*
   * If what has timed out has control information, then build the lazy *
   * entity so that resuming it will bring back the same control info.  *
   *--------------------------------------------------------------------*/

  else { /* timed_out_kind == CTL_F */
    if(tag != LAZY_LIST_TAG) {
      bump_control(c = copy_control(timed_out_comp.ctl, TRUE)); 
    }
    else {
      ACTIVATION *a;
      UP_CONTROL *newc;
      LONG time;	/* Value not needed. */

      bump_control(c = get_up_control_c(timed_out_comp.ctl, NULL, &a, &time));
      bump_activation(a);
      SET_CONTROL(a->control, NULL);
      newc                  = allocate_control();  
      newc->info            = BRANCH_F | RCHILD_CTLMASK;
      newc->identity        = 0;
      newc->PARENT_CONTROL  = c;         /* Parent (ref from c). */
      newc->right.act       = a;	 /* Ref from a. */
      bump_control(c        = newc);
    }
    drop_control(timed_out_comp.ctl);
  }

  /*----------------------------------------------*
   * Clear out timed_out_kind and timed_out_comp. *
   *----------------------------------------------*/

  timed_out_comp.ctl = NULL;
  timed_out_kind = CTL_F;

  /*-----------------------------------------------------------------*
   * Build the lazy result, and tell the garbage collector that c is *
   * owned by a lazy entity.					     *
   *-----------------------------------------------------------------*/

  c->recompute = recompute;
  e            = ENTP(tag, c);  /* Now lose interest in ref count on c */
  note_control(c);

# ifdef DEBUG
    if(trace) {
      trace_i(99, tag);
      if(tag != LAZY_LIST_TAG) print_down_control(TRACE_FILE, c, "", 1);
      else print_up_control(TRACE_FILE, c, 1);
      tracenl();
    }
# endif

  /*-----------------------------------------------------*
   * Build and return an indirection to the lazy entity. *
   *-----------------------------------------------------*/

  ee   = allocate_entity(1);
  *ee  = e;

  return ENTP(INDIRECT_TAG, ee);
}


/****************************************************************
 *			LAZY_LIST_I				*
 ****************************************************************
 * Make a lizy list entity for the list whose code starts at c.	*
 * Kind is 1 if c points to an environment size			*
 * byte, and is 0 otherwise.  This list should not be stored	*
 * on evaluation if recompute is true.  type_instrs points to   *
 * code whose execution will produce the type of the entity     *
 * that is being created.  The parts of the activation are as   *
 * follows.							*
 *								*
 *    program_ctr  	c					*
 *    stack        	A new stack				*
 *    continuation 	NULL					*
 *    control      	NULL					* 
 *    embedding_tries   NULL					*
 *    embedding_marks   NULL					*
 *    coroutines	NIL					*
 *    type_binding_lists from the_act				*
 *    state_a		From the_act				*
 *    state_hold        NIL					*
 *    trap_vec_a	From the_act				*
 *    trap_vec_hold     NIL					*
 *    exception_list	From the_act				*
 *    kind		0					*
 *    in_atomic         0					*
 *    st_depth          0					*
 *    name              From the_act				*
 *    result_type_instrs From parameter type_instrs;		*
 *								*
 * XREF: Called in evaluate.c to do LAZY_LIST_I.		*
 ****************************************************************/

ENTITY lazy_list_i(CODE_PTR c, int kind, CODE_PTR type_instrs,
		   Boolean recompute)
{
  register ACTIVATION *p;
  register UP_CONTROL *ctl;
  ENVIRONMENT *env;
  ENTITY *e, hold;
  int n, m, descr_num;

  /*-------------------------------------------------------*
   * Get the environment size index n and the index of the *
   * environment descriptor.  If there is no environment,  *
   * both are 0.					   *
   *-------------------------------------------------------*/

  if(kind == 0) n = descr_num = 0;
  else {
    n = *(c++);
    descr_num = toint(next_int_m(&c));
  }

  /*------------------------------------------*
   * Build the activation for this lazy list. *
   *------------------------------------------*/

  p 			= allocate_activation();   /* Ref from 
						      allocate_activation */
  p->program_ctr  	= c;
  p->stack        	= new_stack();
  p->continuation 	= NULL;
  p->control      	= NULL;
  p->embedding_tries    = NULL;
  p->embedding_marks    = NULL;
  p->coroutines		= NIL;
  p->state_hold         = NIL;
  p->trap_vec_hold      = NIL;
  p->in_atomic          = 0;
  p->kind		= 0;
  p->st_depth           = 0;
  p->name               = the_act.name;
  p->pack_name          = NULL;
  p->result_type_instrs = type_instrs;
  bump_list(p->type_binding_lists = the_act.type_binding_lists);
  bump_state(p->state_a = the_act.state_a);
  bump_trap_vec(p->trap_vec_a = the_act.trap_vec_a);
  bump_list(p->exception_list = the_act.exception_list);

  /*-------------------------------------------------------------------------*
   * Build an up-control ctl such that failing with control ctl starts up p. *
   *-------------------------------------------------------------------------*/

  ctl			= allocate_control();
  ctl->info		= BRANCH_F | RCHILD_CTLMASK;
  ctl->identity         = 0;
  ctl->right.act	= p;			/* Ref from p. */
  ctl->PARENT_CONTROL	= NULL;
  ctl->recompute	= recompute;

  /*---------------------------------------*
   * Hold onto ctl for garbage collection. *
   *---------------------------------------*/

  hold  = ENTP(LAZY_LIST_TAG, ctl);
  note_control(ctl);

  /*------------------------*
   * Build the environment. *
   *------------------------*/

  m = the_act.num_entries;
  if(n == 0) {
    bump_env(p->env  = the_act.env, m);
    p->num_entries   = m;
  }
  else {
    env                   = allocate_local_env(n);  /* Ref from alloc */
    bump_env(env->link    = the_act.env, m);
    env->num_link_entries = m;
    p->env                = env;
    env->descr_num        = descr_num;
    p->num_entries        = 0;
  }

  /*------------------------------------------------*
   * Return an indirection to the lazy list entity. *
   *------------------------------------------------*/

  e  = allocate_entity(1);
  *e = hold;

  return ENTP(INDIRECT_TAG, e);
}


/********************************************************
 *			GET_LAZY_RESULT			*
 ********************************************************
 * Return the result of lazy evaluation (from the_act). *
 * If the lazy computation has timed-out, this is just  *
 * another lazy entity.  If computation failed, then    *
 * the result is a failure-entity.  If computation was  *
 * successful, the result is on the stack.		*
 *							*
 * Parameter recompute is the recompute flag of the lazy*
 * value (1 = do not memoize result) for the case where *
 * the result is timed-out.				*
 ********************************************************/

PRIVATE ENTITY get_lazy_result(Boolean recompute)
{
  if(failure >= 0) {
    if(failure == TIME_OUT_EX) {
      return make_timeout_lazy(LAZY_TAG, recompute);
    }
    else {
      if(ENT_EQ(failure_as_entity, NOTHING)) {
        failure_as_entity = ENTU(failure);
      }
      return make_fail_ent();
    }
  }
  else {
    return *top_stack();
  }
}


/****************************************************************
 *			RESTORE_AFTER_LAZY			*
 ****************************************************************
 * Restores the_act to old_act.  Drops the parts of the_act	*
 * first.  Since the_act is being terminated, all of its 	*
 * child processes are also being terminated.  So		*
 * the count of the number of processes is updated.		*
 * Pops the runtime shadow stack, which had an activation	*
 * pushed onto it for the garbage collector by hold_current_act.*
 ****************************************************************/

PRIVATE void restore_after_lazy(ACTIVATION *old_act)
{
  /*------------------------------------------------------*
   * If the lazy evaluation made any progress, then the   *
   * activation that caused the lazy evaluation also made *
   * progress.  So hold onto the_act.progress here.       *
   *------------------------------------------------------*/

  int progress = the_act.progress;

  /*----------------------------------*
   * Bring old_act back into the_act. *
   *----------------------------------*/

  drop_activation_parts(&the_act);
  the_act = *old_act;

  /*-----------------------------------------------------------------*
   * Install progress, and prepare the_act for continued evaluation. *
   *-----------------------------------------------------------------*/

  if(progress) the_act.progress = 1;
  start_new_activation();

  /*-------------------------------*
   * Pop the runtime shadow stack. *
   *-------------------------------*/

  pop(&runtime_shadow_st);
}


/********************************************************
 *			HOLD_CURRENT_ACT		*
 ********************************************************
 * Copy the_act into old_act and push old_act onto the  *
 * runtime shadow stack, so that it can be found by	*
 * the garbage collector.  DOES NOT BUMP PARTS OF 	*
 * THE_ACT.  Prepare to switch threads by setting       *
 * precision = -1.					*
 ********************************************************/

PRIVATE void hold_current_act(ACTIVATION *old_act)
{
  *old_act           = the_act;
  precision          = -1;
  old_act->ref_cnt   = 1;
  push_act(runtime_shadow_st, old_act);
}


/********************************************************
 *			LAZY_EVAL			*
 ********************************************************
 * Return the result of evaluating lazy entity e.	*
 *							*
 * XREF: Called in evalsup.c to evaluate a lazy entity. *
 ********************************************************/

ENTITY lazy_eval(ENTITY *e, LONG *time_bound)
{
  ACTIVATION old_act;
  DOWN_CONTROL *c;
  ENVIRONMENT *env;
  int n, descr_num;
  Boolean recompute;
  ENTITY *il, e_orig, result, hold;
  REG_TYPE mark;
  REGPTR_TYPE ptrmark;

  Boolean  should_break       = FALSE;
  CODE_PTR result_type_instrs = NULL;

# ifdef DEBUG
    if(trace) trace_i(100);
# endif

  /*---------------------------------------------------------------*
   * Hold on to the current activation, and push it on the runtime *
   * shadow stack for the garbage collector.			   *
   *---------------------------------------------------------------*/

  hold_current_act(&old_act);
  bump_activation_parts(&old_act);

  /*-------------------------------------------------------------------*
   * Set up the new activation.  The new activation inherits in_atomic *
   * status from the activation that called it.			       *
   *-------------------------------------------------------------------*/

  c = CTLVAL(*e);
  recompute = c->recompute && should_not_recompute == 0;
  move_into_control_c(c, NULL, time_bound);
  the_act.in_atomic = old_act.in_atomic;

  /*----------------------------------------------------------*
   * Register the local variables with the garbage collector. *
   *----------------------------------------------------------*/

  mark    = reg3(&e_orig, &result, &hold);
  ptrmark = reg1_ptrparam(&e);
  reg1_ptr(&il);
  e_orig = *e;

  /*----------------------------------*
   * Get the environment for the_act. *
   *----------------------------------*/

  if(the_act.kind != 0) {
    n                     = *(the_act.program_ctr++);
    descr_num             = toint(next_int_m(&the_act.program_ctr));
    if(n != 0) {
      env                   = allocate_local_env(n); /* Ref from allocate*/
      env->link             = the_act.env;     /* Inherits the_act.env's ref */
      env->num_link_entries = the_act.num_entries;
      env->descr_num        = descr_num;
      the_act.env           = env;  /* Ref from env */
      the_act.num_entries   = 0;
    }
    the_act.kind = 0;
  }

  /*------------------------------------------------------------------------*
   * Set *e to an indirection to infloop_timeout, so that garbage collector *
   * will not try to go into e, and some infinite looping will be	    *
   * detected.  The computation of e should not refer to e.		    *
   *------------------------------------------------------------------------*/

  *(il = allocate_entity(1)) = infloop_timeout;
  *e = hold = ENTP(INDIRECT_TAG, il);

  /*------------------------------------------------------------*
   * Start up the_act and possibly break before the evaluation. *
   * (The start_new_activation call must precede the break to   *
   * set the name of the function.)				*
   *------------------------------------------------------------*/

  start_new_activation();
  should_break = break_info.break_lazy && !break_info.suppress_breaks 
                 && breakable();
  if(should_break) {
    break_info.pc = the_act.program_ctr;
    vis_break(LAZY_VIS);
  }
  result_type_instrs = the_act.result_type_instrs;

  /*-------------------------*
   * Perform the evaluation. *
   *-------------------------*/

  evaluate(time_bound);

  /*--------------------*
   * Record the result. *
   *--------------------*/

  result = get_lazy_result(recompute);
  if(recompute && !MEMBER(TAG(result), always_store_tags)) {
    *e = *il = e_orig;
  }
  else {
    *e = *il = result;
  }
  if(TAG(result) == FAIL_TAG) store_fail_act = FALSE;

# ifdef DEBUG
    if(trace) {
      trace_i(101, failure);
      trace_print_entity(result);
      tracenl();
    }
# endif

  /*---------------------------------------------------------------*
   * Restore the activation to the caller of this lazy evaluation. *
   *---------------------------------------------------------------*/

  restore_after_lazy(&old_act);

  /*--------------------------------------*
   * Possibly break after the evaluation. *
   *--------------------------------------*/

  if(should_break) {
    break_info.pc = the_act.program_ctr;
    break_info.result = result;
    break_info.result_type_instrs = result_type_instrs;
    vis_break(END_LAZY_VIS);
  }

  /*--------------------*
   * Return the result. *
   *--------------------*/

  unreg(mark);
  unregptr(ptrmark);
  return result;
}


/********************************************************
 *			LAZY_LIST_EVAL			*
 ********************************************************
 * Evaluate lazy list e, but don't go more than 	*
 * *time_bound steps.					*
 *							*
 * XREF: Called in evalsup.c to evaluate a lazy list.   *
 ********************************************************/

ENTITY lazy_list_eval(ENTITY *e, LONG *time_bound)
{
  ACTIVATION old_act;
  UP_CONTROL *c;
  ENTITY l, *r, hold, hold2, *il, e_orig;
  REG_TYPE mark;
  REGPTR_TYPE ptrmark;
  Boolean recompute, orig_recompute;
  Boolean should_break = FALSE;
  CODE_PTR result_type_instrs = NULL;

# ifdef DEBUG
    if(trace) trace_i(102);
# endif

  /*-------------------------------*
   * Register the local variables. *
   *-------------------------------*/

  mark = reg3(&l, &hold, &e_orig);
  reg1(&hold2);
  ptrmark = reg1_ptrparam(&e);
  reg1_ptr(&il);

  /*--------------------------------*
   * Hold on to current activation. *
   *--------------------------------*/

  hold_current_act(&old_act);
  bump_activation_parts(&old_act);

  /*--------------------------------------------------------------------*
   * Bring in lazy list activation.  Control CTLVAL(*e) tells where the *
   * previous execution of the lazy list stopped. Force a failure, which*
   * prepares for getting the next value in the stream.			*
   *									*
   * Note that do_failure might return TRUE with failure being a time-  *
   * out.  In that case, just rebuild the list and return it.		*
   *--------------------------------------------------------------------*/

  c              = CTLVAL(*e);
  orig_recompute = c->recompute;
  recompute      = orig_recompute && should_not_recompute == 0;
  if(recompute) e_orig = *e;
  if(do_failure(c, 0, time_bound)) {
    if(failure == TIME_OUT_EX) {
      l = make_timeout_lazy(LAZY_LIST_TAG, orig_recompute);
    }
    else {
      l = nil;
    }
    *e = l;
    restore_after_lazy(&old_act);   /* Refs of the_act parts from old_act. */
    goto out;
  }
  the_act.in_atomic = old_act.in_atomic;

  /*--------------------------------------------------------------*
   * Drop time to SLICE_TIME in case there are other threads in   *
   * the control.						  *
   *--------------------------------------------------------------*/

  if(*time_bound > SLICE_TIME) *time_bound = SLICE_TIME;

  /*------------------------------------------------------------------------*
   * Set *e to an indirection to infloop_timeout, so that garbage collector *
   * will not try to go into e, and some infinite looping will be	    *
   * detected.  The computation of e should not refer to e.		    *
   *------------------------------------------------------------------------*/
 
  *(il = allocate_entity(1)) = infloop_timeout;
  *e = hold2 = ENTP(INDIRECT_TAG, il);
  the_act.kind = 0;

  /*------------------------------------------------------------*
   * Start up the_act and possibly break before the evaluation. *
   * (The start_new_activation call must precede the break to   *
   * set the name of the function.)				*
   *------------------------------------------------------------*/

  start_new_activation();
  should_break = break_info.break_lazy && !break_info.suppress_breaks 
                 && breakable();
  if(should_break) {
    break_info.pc = the_act.program_ctr;
    vis_break(LAZY_VIS);
  }
  result_type_instrs = the_act.result_type_instrs;

  /*------------------------------------------------*
   * Evaluate, to get the next value in the stream. *
   *------------------------------------------------*/

  evaluate(time_bound);

  /*--------------------------------*
   * Check for failure or time-out. *
   *--------------------------------*/

  if(failure >= 0) {
    if(failure == TIME_OUT_EX) {
      l = make_timeout_lazy(LAZY_LIST_TAG, orig_recompute);
    }
    else {

      /*-----------------------------------------------------------------*
       * Failed, so there are no more values in the stream.  Return nil. *
       *-----------------------------------------------------------------*/

      l = nil;
      failure = -1;
      failure_as_entity = NOTHING;
    }
    *e = *il = l;

    restore_after_lazy(&old_act);
    goto out;
  }

  /*-------------------------------------------------------------*
   * Get the value returned by the computation, which is the 	 *
   * head of the rest of the lazy list.  We put it into l, since *
   * the garbage collector knows about l.  l is changed to the   *
   * result list below.						 *
   *-------------------------------------------------------------*/

  l = pop_stack();

  /*-------------------------------------------------------------------*
   * Get the activation for the tail of this list, and build the list. *
   *-------------------------------------------------------------------*/

  if(the_act.control == NULL) {
    r = allocate_entity(2);
    r[0] = l;
    r[1] = nil;
  }
  else {
    bump_control(c = maybe_shift_up_control_c(the_act.control));
    SET_CONTROL(c, copy_control(c, TRUE));
    c->recompute = orig_recompute;
    hold    = ENTP(LAZY_LIST_TAG, c);
    note_control(c);
    r       = allocate_entity(3);
    r[0]    = l;
    r[1]    = ENTP(INDIRECT_TAG, &(r[2]));
    r[2]    = hold;
  }
  l = ENTP(PAIR_TAG, r);

  /*---------------------------------------------------------------*
   * Restore the activation to the caller of this lazy evaluation. *
   *---------------------------------------------------------------*/

  restore_after_lazy(&old_act);

  /*-------------------*
   * Store the result. *
   *-------------------*/

  if(recompute) {*e = *il = e_orig;}
  else {*e = *il = l;}

 out:

  /*--------------------------------------*
   * Possibly break after the evaluation. *
   *--------------------------------------*/

  if(should_break) {
    break_info.pc = the_act.program_ctr;
    break_info.result = l;
    break_info.result_type_instrs = result_type_instrs;
    vis_break(END_LAZY_VIS);
  }

# ifdef DEBUG
    if(trace) {
      trace_i(103);
      trace_print_entity(l);
      tracenl();
    }
# endif

  unreg(mark);
  unregptr(ptrmark);
  return l;
}


/*******************************************************************
 *			RUN_FUN					   *
 *******************************************************************
 * Compute the value of fun(arg) in state st, with trap vector tv. *
 * (If tv = NULL, don't trap any exceptions.)  Time-out after *time*
 * steps, decreasing *time by the amount of time spent.		   *
 *******************************************************************/

PRIVATE UBYTE run_fun_code[] = {REV_APPLY_I, RETURN_I};

ENTITY run_fun(ENTITY fun, ENTITY arg, STATE *st, TRAP_VEC *tv, LONG *time)
{
  ACTIVATION old_act;
  ENTITY result;

  hold_current_act(&old_act);

  the_act.program_ctr 		= run_fun_code;
  the_act.st_depth    		= 0;
  the_act.num_entries 		= 0;
  the_act.kind			= 0;
  the_act.env         		= NULL;
  the_act.stack 		= new_stack();
  the_act.name 			= "run-fun";
  the_act.pack_name		= NULL;
  the_act.result_type_instrs 	= NULL;
  the_act.continuation      	= NULL;
  the_act.control           	= NULL;
  the_act.embedding_tries       = NULL;
  the_act.embedding_marks       = NULL;
  the_act.coroutines		= NIL;
  the_act.exception_list	= NIL;
  the_act.type_binding_lists    = NIL;
  the_act.state_hold		= NIL;
  the_act.trap_vec_hold		= NIL;
  bump_state(the_act.state_a    = st);
  if(tv == NULL) tv = no_trap_tv;
  bump_trap_vec(the_act.trap_vec_a = tv);

  *push_stack() = arg;
  *push_stack() = fun;
  evaluate(time);
  result = get_lazy_result(0);
  restore_after_lazy(&old_act);
  return result;
}


/****************************************************************
 *			GLOBAL_EVAL				*
 ****************************************************************
 * Return the value of global identifier gp[0], and put it into *
 * gp[0]. gp[1] holds the type of gp[0].  gp[0] will only	*
 * be evaluated here if its tag is GLOBAL_TAG.			*
 *								*
 * XREF: 							*
 *   Called in evalsup.c to evaluate an uncomputed global.	*
 *								*
 *   Called in apply.c to evaluate handleDemons.		*
 *								*
 *   Called in show/prtent.c to evaluate function $.		*
 ****************************************************************/

ENTITY global_eval(ENTITY *gp, LONG *time_bound)
{
  HASH2_TABLE **tabl;
  HASH2_CELLPTR h;
  HASH_KEY typekey;
  CODE_PTR addr;
  GLOBAL_TABLE_NODE *g;
  ENVIRONMENT *env;
  ACTIVATION old_act;
  LONG name_index;
  int n, descr_num, old_use_the_act_bindings;
  TYPE *this_id_type;
  ENTITY e;
  REG_TYPE mark;
  REGPTR_TYPE ptrmark;
  Boolean drop_suppress_breaks = FALSE;

  /*------------------------------------------------------------------*
   * If gp[0] is not a GLOBAL_TAG entity, it must have been evaluated *
   * already.  Just return it.					      *
   *------------------------------------------------------------------*/

  e = gp[0];
  if(TAG(e) != GLOBAL_TAG) return e;

  /*--------------------------------------*
   * gp[1] should hold the type of gp[0]. *
   * It must not have any variables in it.*
   * If it does, then default them now.   *
   *--------------------------------------*/

  if(TAG(gp[1]) != TYPE_TAG) die(74);

  old_use_the_act_bindings = use_the_act_bindings;
  use_the_act_bindings = 0;
  this_id_type = force_ground(TYPEVAL(gp[1]));
  name_index = VAL(e);

  mark    = reg1(&e);
  ptrmark = reg1_ptrparam(&gp);

# ifdef DEBUG
    if(trace || trace_global_eval) {
      trace_i(104, name_index, outer_bindings[name_index].name, 
	      this_id_type->THASH);
      trace_ty(this_id_type);
      tracenl();
    }
# endif

  /*---------------------------------------------*
   * Try to look up gp in the monomorphic table. *
   *---------------------------------------------*/

  if(mono_lookup(this_id_type, name_index, &e)) {
    gp[0] = e;
    unreg(mark);
    unregptr(ptrmark);
    use_the_act_bindings = old_use_the_act_bindings;
    return e;
  }

  /*------------------------------------------------*
   * gp is not in the monomorphic table, so try the *
   * polymorphic table. 			    *
   *------------------------------------------------*/

  g = poly_lookup(outer_bindings[name_index].poly_table, this_id_type);

  /*--------------------------------------------------------*
   * If gp[0] was not found in either table, then fail with *
   * exception GLOBAL_ID_EX, with the name and type of the  *
   * identifier as extra information.			    *
   *--------------------------------------------------------*/

  if(g == NULL) {
    char* this_name;
    int this_name_len;
#   ifndef SMALL_STACK
      char temp[MAX_ID_SIZE + MAX_TYPE_STR_LEN + 20];
#   else
      char* temp = (char *) BAREMALLOC(MAX_ID_SIZE + MAX_TYPE_STR_LEN + 20);
#   endif

#   ifdef DEBUG
      if(trace_global_eval) trace_i(105);
#   endif

    failure       = GLOBAL_ID_EX;
    this_name     = outer_bindings[name_index].name;
    this_name_len = strlen(this_name);
    sprintf(temp, "%s:", this_name);
    sprint_ty(temp + 1 + this_name_len, 
	      (MAX_TYPE_STR_LEN + 19) - this_name_len, this_id_type);
    failure_as_entity = qwrap(GLOBAL_ID_EX, make_str(temp));
    unreg(mark);
    unregptr(ptrmark);
#   ifdef SMALL_STACK
      FREE(temp);
#   endif
    use_the_act_bindings = old_use_the_act_bindings;
    return bad_ent;
  }

# ifdef DEBUG
    if(trace_global_eval) {
      trace_i(106);
      trace_ty(g->V);
      tracenl();
    }
# endif

  /*--------------------------------------------------------------*
   * If the declaration of this identifier is a copy declaration  *
   * (of the form let{copy} x = y.), then try poly table nodes in *
   * the other id's (y in the example) table. 			  *
   *--------------------------------------------------------------*/

  while(g->start != NULL) {
    GLOBAL_TABLE_NODE *gnew;
    gnew = poly_lookup(g->start, this_id_type);
    if(gnew == NULL) break;
    g = gnew;
  }

  /*----------------------------------------------------*
   * Get the address of the code that computes this id. *
   *----------------------------------------------------*/

  addr = package_descr[g->packnum].begin_addr + g->offset;

# ifdef DEBUG
    if(trace_global_eval) {
      trace_i(107, addr, g->packnum, g->offset);
    }
# endif

  /*----------------------------------------*
   * Set up the environment for evaluation. *
   *----------------------------------------*/

  clear_type_stk();
  push_a_type(this_id_type);
  env = setup_globals(&addr, NULL);
  clear_type_stk();

  /*------------------------------*
   * Remember the old activation. *
   *------------------------------*/

  hold_current_act(&old_act);

  /*---------------------------------------*
   * Set up the activation for evaluation. *
   *---------------------------------------*/

  n = *(addr++);                         /* Env size byte */
  descr_num = toint(next_int_m(&addr));  /* Env descriptor number */
  if(n == 0) {
    the_act.env = env;         /* Ref from env */
    if(env != NULL) the_act.num_entries = env->most_entries;
    else the_act.num_entries = 0;
  }
  else {
    the_act.env                   = allocate_local_env(n);
    the_act.env->link             = env;                   /* Ref from env */
    the_act.env->num_link_entries = (env == NULL) ? 0 : env->most_entries;
    the_act.env->descr_num        = descr_num;
    the_act.num_entries           = 0;
  }
  the_act.program_ctr 		= addr;
  the_act.type_binding_lists    = NIL;
  the_act.control 		= NULL;
  the_act.embedding_tries       = NULL;
  the_act.embedding_marks       = NULL;
  the_act.continuation 		= NULL;
  the_act.coroutines		= NIL;
  the_act.name                  = outer_bindings[name_index].name;
  the_act.pack_name		= NULL;
  the_act.st_depth              = 0;
  the_act.kind			= 0;
  the_act.result_type_instrs    = g->ty_instrs;
  /* the_act.in_atomic          = old_act.in_atomic;    (Already there) */
  the_act.state_a	        = NULL; /*( global_eval_state) */
  bump_trap_vec(the_act.trap_vec_a  = global_trap_vec);
  the_act.state_hold		= NIL;
  the_act.trap_vec_hold		= NIL;
  the_act.exception_list	= NIL;
  the_act.stack 		= new_stack(); /* Ref from new_stack */

  /*-------------------------------------*
   * Perform initial NAME_I instruction. *
   *-------------------------------------*/

  if(*the_act.program_ctr == NAME_I) {
    LONG nn;
    the_act.program_ctr++;
    nn = next_int_m(&the_act.program_ctr);
    the_act.name = outer_bindings[nn].name;
  }

  /*--------------------------------------------------*
   * Possibly break.  But do not break for functions. *
   * (That is, do not break if this global is just a  *
   * function, created by FUNCTION_I.)		      *
   *--------------------------------------------------*/

  if((break_info.break_lazy || break_info.break_applies) 
     && *(the_act.program_ctr) == FUNCTION_I) {
    CODE_PTR p = the_act.program_ctr + 1;
    LONG     offset = next_int_m(&p);
    if(the_act.program_ctr[offset] == RETURN_I) {
      drop_suppress_breaks = TRUE;
      break_info.suppress_breaks++;
    }
  }

  if(break_info.break_lazy && !break_info.suppress_breaks && breakable()) {
    break_info.pc = the_act.program_ctr;
    vis_break(LAZY_VIS);
  }

  /*--------------------*
   * Do the evaluation. *
   *--------------------*/

  evaluate(time_bound);

  if(drop_suppress_breaks) break_info.suppress_breaks--;

  /*---------------------*
   * Memoize the result. *
   *---------------------*/

  e = gp[0] = get_lazy_result(FALSE);

# ifdef DEBUG
    if(trace_global_eval) {
      trace_i(108, outer_bindings[name_index].name);
      trace_print_entity(e);
      tracenl();
    }
# endif

  /*--------------------------------------------------------------*
   * Put the result into the mono table, so that we won't need to *
   * evaluate it again.						  *
   *--------------------------------------------------------------*/

  typekey.type = this_id_type;
  tabl = &(outer_bindings[name_index].mono_table);
  h = insert_loc_hash2(tabl, typekey, typekey.type->THASH, eq);
  if(h->key.num == 0) {

#   ifdef DEBUG
      if(trace_global_eval) {
	trace_i(109, typekey.type->THASH, typekey.type, 
		outer_bindings[name_index].name);
	trace_ty(typekey.type);
	tracenl();
      }
#   endif

    h->key        = typekey;
    h->val.entity = e;
  }

  /*-------------------------*
   * Restore the activation. *
   *-------------------------*/

  restore_after_lazy(&old_act);

  /*--------------------*
   * Return the result. *
   *--------------------*/

  unreg(mark);
  unregptr(ptrmark);
  use_the_act_bindings = old_use_the_act_bindings;
  return e;
}



/****************************************************************
 *			GET_AND_EVAL_GLOBAL			*
 ****************************************************************
 * Get the value of std_id[sym]:t, and evaluate it.		*
 ****************************************************************/

ENTITY get_and_eval_global(int sym, TYPE *t, LONG* l_time)
{
  ENTITY gp[2];

  t     = force_ground(t);
  gp[0] = ENTG(ent_str_tb(std_id[sym]));
  gp[1] = ENTP(TYPE_TAG, t);
  return global_eval(gp, l_time);
}
