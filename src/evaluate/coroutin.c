/**********************************************************************
 * File:    evaluate/coroutin.c
 * Purpose: Functions to handle coroutines.
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
 * This file provides support for coroutines.  				*
 *									*
 * Coroutines are currently supported but not documented.  They have	*
 * been inadequately tested, and must be developed further before	*
 * being advertised.							*
 ************************************************************************/

#include <string.h>
#include "../misc/misc.h"
#include "../utils/lists.h"
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

/*****************************************************************
 *		 MOVE_DONE_COROUTINES				 *
 *****************************************************************
 * Coroutines in activations are stored as a pair of lists,      *
 * consisting of a done list and a waiting list.  The coroutines *
 * in the done list have had a chance to go in the current round,*
 * while those that are in the waiting list are still waiting    *
 * their turn.  Move_done_coroutines is used to move done        *
 * coroutines back to the waiting list to begin another round.   *
 * It returns the list cell that should replace cr in the_act.   *
 *****************************************************************/

PRIVATE LIST *move_done_coroutines(LIST *cr)
{
  LIST *l, *p;

# ifdef DEBUG
    if(trace && cr->tail != NIL) {
      trace_i(74);
    }
# endif

  l = NIL;
  for(p = cr->tail; p != NIL; p = p->tail) {
    l = act_cons(p->head.act, l);
  }
  return list_cons(l, NIL);
}


/********************************************************
 *			RESUME_I			*
 ********************************************************
 * Resume a coroutine, if possible.  Pick one from the  *
 * waiting list if possible.				*
 *							*
 * XREF: Called in evaluate.c to handle RESUME_I.	*
 ********************************************************/

void resume_i(void)
{
  LIST *cr, *cr1;
  UP_CONTROL *ctlhold;
  ACTIVATION *new_act;
  STATE *state_a;

  cr = the_act.coroutines;
  if(cr == NIL) {
  no_coroutines:
    die(80);
  }

  /*-----------------------------------------------------------------*
   * If there are no waiting coroutines, move the done coroutines to *
   * the waiting list 						     *
   *-----------------------------------------------------------------*/

  cr1 = cr->head.list;
  if(cr1 == NIL) {
    SET_LIST(cr, move_done_coroutines(cr));
    the_act.coroutines = cr;
    cr1 = cr->head.list;
  }

  /*-------------------------------------------------------------*
   * If there is a waiting coroutine, then resume the first one. *
   *-------------------------------------------------------------*/

  if(cr1 != NIL) {
#   ifdef DEBUG
      if(trace) trace_i(75);
#   endif
    state_a     = the_act.state_a;      /* Ref from the_act */
    ctlhold     = the_act.control;      /* Ref from the_act */
    drop_activation_parts_except_ccs(&the_act);
    drop_list(the_act.state_hold);
    new_act            = cr1->head.act; 
    the_act            = *new_act;
    bump_activation_parts_except_ccs(&the_act);

    the_act.control = ctlhold;  	/* Ref from ctlhold */
    the_act.state_a = state_a;          /* Ref from state_a */
    bump_list(the_act.state_hold = new_act->state_hold);
    bump_list(the_act.coroutines = list_cons(cr1->tail, cr->tail));
    drop_list(cr);
    start_new_activation();
  }
  else goto no_coroutines;
}


/********************************************************
 *			COROUTINE_PAUSE			*
 ********************************************************
 * Execute a PAUSE_I or REPAUSE_I instruction by	*
 * switching to another coroutine, if possible.  	*
 * Return TRUE if switched to another coroutine, and	*
 * FALSE if did not.					*
 *							*
 * XREF: Called in pause_i, in apply.c.			*
 ********************************************************/

Boolean coroutine_pause(void)
{
  LIST *cr, *cr1;
  ACTIVATION *old_act, *new_act;

  /*-----------------------------------------------*
   * Check for a coroutine that should be resumed. *
   *-----------------------------------------------*/

  cr = the_act.coroutines;
  if(cr != NIL) {
    cr1 = cr->head.list;
    if(cr1 == NIL) {
      SET_LIST(cr, move_done_coroutines(cr));
      the_act.coroutines = cr;
      cr1 = cr->head.list;
    }

    /*---------------------------------------------*
     * If a sibling coroutine is found, resume it. *
     *---------------------------------------------*/

    if(cr1 != NIL) {

#     ifdef DEBUG
        if(trace) trace_i(76);
#     endif

      old_act = allocate_activation();
      *old_act = the_act;   /* Refs from the_act */
      old_act->ref_cnt = 1;
      new_act 		 = cr1->head.act;
      the_act 		 = *new_act;
      bump_activation_parts_except_ccs(&the_act);

      the_act.control    = old_act->control;    /* Ref from old_act */
      old_act->control 	 = NULL;

      bmp_list(the_act.coroutines =
	       list_cons(cr1->tail, act_cons(old_act, cr->tail)));       
      drop_list(cr);
      old_act->coroutines = NIL;

      bump_state(the_act.state_a = old_act->state_a);
      bump_list(the_act.state_hold = new_act->state_hold);

      old_act->ref_cnt--;      /* Bumped at allocate_act, act_cons */
      start_new_activation();
      return TRUE;
    }
  }

  return FALSE;
}


/*********************************************************************
 *			GET_COROUTINE_CONT			     *
 *********************************************************************
 * Return a continuation for a coroutine.  The result has ref_cnt 1. *
 *********************************************************************/

CONTINUATION *get_coroutine_cont(CODE_PTR retpc)
{
  CONTINUATION *cont;

  cont = allocate_continuation();  /* ref from allocate_continuation */
  bump_continuation(cont->continuation = the_act.continuation);
  cont->program_ctr = retpc;
  bump_env(cont->env = the_act.env, the_act.num_entries);
  cont->num_entries = the_act.num_entries;
  bump_list(cont->exception_list = the_act.exception_list);
  cont->name = the_act.name;
  cont->result_type_instrs = the_act.result_type_instrs;
  return cont;
}


/************************************************************************
 *			PUSH_COROUTINE					*
 ************************************************************************
 * Install a coroutine into the list of done coroutines.  The new	*
 * coroutine starts execution at pc, and returns to continuation cont.	*
 * The activation has only the tail of the held states in it.		*
 ************************************************************************/

void push_coroutine(CODE_PTR pc, CONTINUATION *cont) 
{
  ACTIVATION *a;

  a = allocate_activation();		 /* ref from allocate_activation */
  bump_continuation(a->continuation = cont);
  a->program_ctr 	  	= pc;
  bump_env(a->env 		= the_act.env, the_act.num_entries);
  a->num_entries		= the_act.num_entries;
  a->st_depth 		  	= the_act.st_depth + 1;
  bump_list(a->exception_list 	= the_act.exception_list);
  bump_trap_vec(a->trap_vec_a 	= the_act.trap_vec_a);
  bump_list(a->trap_vec_hold    = the_act.trap_vec_hold);
  bump_list(a->state_hold 	= the_act.state_hold);
  a->name                       = the_act.name;
  a->state_a			= NULL;
  a->control 			= NULL;
  a->stack 			= NULL;
  a->coroutines 		= NULL;
  a->kind 			= 0;
  if(the_act.coroutines == NULL) {
    bump_list(the_act.coroutines = list_cons(NIL, NIL));
  }
  SET_LIST(the_act.coroutines->tail, act_cons(a, the_act.coroutines->tail));
  drop_activation(a);  /* Bumped twice above, once at allocate_act, once
			  at act_cons. */
}


/****************************************************************
 *			PROCESS_COROUTINE_ARGS			*
 ****************************************************************
 * Process arguments to a COROUTINE_I instruction.  Return	*
 * the address of the next instruction.  pc is the address of	*
 * this COROUTINE_I instruction.  Set start to the start addr	*
 * of the coroutine.						*
 */

CODE_PTR process_coroutine_args(CODE_PTR pc, CODE_PTR *start)
{
  ENTITY *a, e1;
  int k;
  long jump;
  
  the_act.program_ctr = pc+1;
  jump = next_three_bytes;
  k = next_byte;
  *start = the_act.program_ctr;
  a = allocate_entity(1);
  *a = NOTHING;
  e1 = ENTP(INDIRECT_TAG, a);
  local_bind_env(k, e1, pc);
  return jump + pc;
}


