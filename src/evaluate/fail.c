/**********************************************************************
 * File:    evaluate/fail.c
 * Purpose: Handle failure and time-out for expression evaluator
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
 * This file handles failure and time-out conditions, by causing the	*
 * interpreter to search for an exception handler or by switching to	*
 * another thread.							*
 ************************************************************************/

#include "../misc/misc.h"
#include "../utils/lists.h"
#include "../alloc/allocate.h"
#include "../classes/classes.h"
#include "../unify/unify.h"
#include "../machdata/entity.h"
#include "../machdata/except.h"
#include "../machstrc/machstrc.h"
#include "../intrprtr/intrprtr.h"
#include "../evaluate/evaluate.h"
#include "../evaluate/instruc.h"
#include "../show/printrts.h"
#include "../show/gprint.h"
#include "../rts/rts.h"
#include "../gc/gc.h"
#include "../standard/stdtypes.h"
#include "../tables/tables.h"
#include "../show/getinfo.h"
#include "../show/prtent.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

PRIVATE Boolean 
do_general_timeout(UP_CONTROL *ctl, int to_kind, 
		   union ctl_or_act to_coa, LONG *time);


/****************************************************************
 *			PRIVATE VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			in_trap_ex				*
 ****************************************************************
 * in_trap_ex is true while executing trap_ex, which executes   *
 * a trap of an exception.  It is used to suppress traps while  *
 * processing a trap.						*
 ****************************************************************/

PRIVATE Boolean in_trap_ex = FALSE;


/****************************************************************
 *			SHOULD_TRAP				*
 ****************************************************************
 * Return TRUE if should trap exception ex.  Examines the trap  *
 * vector of the_act, the currently executing activation.	*
 ****************************************************************/

Boolean should_trap(int ex)
{
  if(ex == TIME_OUT_EX || in_trap_ex) return FALSE;
  return (the_act.trap_vec_a->component[ex >> LOG_LONG_BITS] &
	  (1L << (ex & LOG_LONG_BITS_MASK))) != 0;
}


/****************************************************************
 *			TRAP_EX					*
 ****************************************************************
 * Trap exception ex.  That is, don't just indicate that ex 	*
 * should be trapped if it occurs, but actually cause the trap  *
 * to happen now.						*
 ****************************************************************/

void trap_ex(ENTITY ex, int instr, CODE_PTR pc)
{
  /*-------------------------------------------------------------------*
   * Hold onto ex, since it might be changed during later computation. *
   *-------------------------------------------------------------------*/

  ENTITY   hold_ex = ex;
  REG_TYPE mark    = reg1_param(&hold_ex);

  /*-------------------------------------------------*
   * Turn off tracing, unless trace_print_rts is on. *
   *-------------------------------------------------*/

# ifdef DEBUG
    UBYTE old_trace = trace;
    trace = trace_print_rts;
# endif

  flush_stdout();

  /*---------------------------------------------------------------*
   * Note that we are doing trap_ex.  This will suppress recursive *
   * entries into trap_ex.  See should_trap.  			   *
   *---------------------------------------------------------------*/

  in_trap_ex = TRUE;

  /*----------------------------*
   * Inform stderr of the trap. *
   *----------------------------*/

  {char *name;
   read_trap_msgs();
   fflush(stdout);
   ASTR_ERR_MSG(trap_msg[0]);
   failure = -1;
   print_entity_with_state(hold_ex, exception_type, STDERR, the_act.state_a,
			   no_trap_tv, 0, 1000);
   ASTR_ERR_MSG("\n");
   print_exception_description(STDERR, hold_ex);
   ASTR_ERR_MSG(trap_msg[1]);
   read_instr_names();
   name = (instr > LIST2_PREF_I) ? instr_name[instr]
				 : prefix_instr_name[instr][pc[1]];
   gprintf(STDERR,"%s\n", name);
  }

  /*--------------------------------------*
   * Produce ast.rts or run the debugger. *
   *--------------------------------------*/

  if(start_visualizer_at_trap) {
    unreg(mark);
    break_info.pc = pc;
    vis_break(TRAP_VIS);
  }
  else if(print_rts(&the_act, hold_ex, instr, pc)) {
    ASTR_ERR_MSG(ast_rts_msg);
    unreg(mark);

#   ifdef DEBUG
      pause_for_debug();
#   endif

#   ifdef UNIX
      clean_up_and_exit(1);
#   endif

#   ifdef MSWIN
      failure = TRAPPED_EX;
      setConsoleTerminated();
#   endif
  }

  in_trap_ex = FALSE;

# ifdef DEBUG
    trace = old_trace;
# endif

}


/****************************************************************
 *			DO_FAILURE				*
 ****************************************************************
 * The_act has failed with control c.  Set the_act for resuming *
 * after the failure. 						*
 *								*
 * If there is some place to fail to in control c, then return	*
 * FALSE.  							*
 *								*
 * If there is no place to fail to in control c, return TRUE.	*
 * In this case, if inst is not 0, and store_fail_act is true,	*
 * then record the activation that caused the failure into	*
 * fail_act, where inst is the instruction that caused failure.	*
 * Also push the exception of the failure onto the_act.stack.	*
 *								*
 * Set *time if necessary.  (If a new thread is started, the    *
 * time will need to be set to SLICE_TIME for it.)		*
 *								*
 * XREF: 							*
 *   evaluate.c calls do_failure to handle a failure.		*
 *								*
 *   lazy.c calls do_failure to simulate a failure when		*
 *   starting to evaluate a lazy list.				*
 ****************************************************************/

Boolean do_failure(UP_CONTROL *c, int inst, LONG *time)
{
  register UP_CONTROL* cf     = c;
  register int         kind   = 0;
  LONG                 try_id = first_try_id(the_act.embedding_tries);

  /*--------------------------------------------------------------------*
   * Scan along the control.  We are looking for the NULL at the        *
   * end, or a node that catches the failure.				*
   *--------------------------------------------------------------------*/

  for(;;) {

#   ifdef DEBUG
      if(trace) {
	trace_i(94, failure, cf);
        if(cf == NULL) tracenl();
	else trace_i(6, CTLKIND(cf), cf->identity, try_id);
      }
#   endif

    /*----------------------------------------------------------------*
     * Exit the loop on a null control. There is no place to fail to. *
     *----------------------------------------------------------------*/

    if(cf == NULL) break;

    kind = CTLKIND(cf);
    switch(kind) {

      case MIX_F:

        /*---------------------------------------------------------*
	 * If this mix node has lost its child, then skip over it. *
	 *---------------------------------------------------------*/

	if(cf->right.act == NULL) break;

        /*--------------------------------------------------------------*
         * If we come up to a mix node from the left, then resume with  *
         * the control on the right, deferring the parent.  This is	*
 	 * handled at the end of the loop.		  		*
         *--------------------------------------------------------------*/
      
        if(!UPLINK_IS_FROM_RIGHT(cf)) goto loopend;

	/*----------------------------------------------------------------*
	 * If we come up to a mix node from the right, then  	      	  *
	 * this entire mix has completed a round.  Treat like a 	  *
	 * time-out.  But drop the control that has just failed. 	  *
	 * Note that cf->left.ctl is the parent control, and cf->right    *
	 * is the control down to the left, cf is an UP_CONTROL.  	  *
	 * So pretend that cf->right has just timed-out. 		  *
	 *----------------------------------------------------------------*/

	else {
	  int k   = TRUE_RCHILD_KIND(cf);
	  failure = TIME_OUT_EX;
	  *time   = 0;
	  return do_general_timeout(cf->PARENT_CONTROL, k, cf->right, time);
	}

      case BRANCH_F:

	/*------------------------------------------------------*
         * A BRANCH_F node with a nonnull child catches the	*
	 * failure.						*
	 *------------------------------------------------------*/

        if(cf->right.act != NULL) goto loopend;
	break;

      case TRY_F:
      case TRYTERM_F:

	/*--------------------------------------------------------------*
	 * If this try is the one that we are looking for,		*
	 * and it is able to catch this exception, then exit the 	*
         * loop -- this is our node.  TRY_F nodes cannot   		*
	 * catch terminateX.						*
         *								*
         * If this try node has been cancelled, then some other thread  *
         * has exited this try successfully.  All other threads that	*
	 * fail within the try should be killed without the failure	*
	 * being caught.  So scan upwards across TRYEACH_F,		*
	 * TRYEACHTERM_F nodes and MARK_F nodes until a node is		*
	 * found that is not one of those.				*
 	 *								*
	 * If this is the try that we are looking for (the innermost	*
	 * try in which this thread is running) but it cannot catch	*
	 * the exception, then set try_id to -1 to indicate that	*
	 * testing of try_id should be suspended; we are surely in all	*
	 * tries above this.						*
	 *--------------------------------------------------------------*/

        if(try_id < 0 || cf->identity == try_id) {

          /*------------------------------------------------------------*
           * If this node is a cancelled try, then skip over TRYEACH_F, *
	   * TRYEACHTERM_F and MARK_F nodes above it, and continue	*
	   * the loop with the next control.  Do not just break the	*
	   * switch, since cf is updated at the end of the switch.	*
           *------------------------------------------------------------*/

          if(cf->right.act == NULL) {
	    try_id = -1;
            cf = cf->PARENT_CONTROL;
	    while(cf != NULL && CTLKIND(cf) >= TRYEACH_F) {
	      cf = cf->PARENT_CONTROL;
	    }
            continue;
          }

	  /*------------------------------------------------------------*
	   * If this node can catch this exception, then exit the	*
	   * loop.  Otherwise, just skip over this node.		*
	   *------------------------------------------------------------*/

	  if(kind == TRYTERM_F ||
	     (failure != TERMINATE_EX && failure != ENDTHREAD_EX)) {
	    goto loopend;
	  }
	  else try_id = -1;
	}
        break;

      /*--------------------------------------------------------------*
       * TRYEACH_F and TRYEACHTERM_F nodes cannot be cancelled.  So   *
       * handling of them is simpler.				      *
       *--------------------------------------------------------------*/

      case TRYEACH_F:
	if((try_id < 0 || cf->identity == try_id)
	    && failure != TERMINATE_EX && failure != ENDTHREAD_EX) {
	  goto loopend;
	}
        else break;

      case TRYEACHTERM_F:
	if(try_id < 0 || cf->identity == try_id) goto loopend;
        else break;

      case MARK_F:

	/*----------------------------*
	 * Never stop at a mark node. *
	 *----------------------------*/

	break;
    }
    cf = cf->PARENT_CONTROL;
  }

 loopend:

  /*--------------------------------------------------------------------*
   * If there is no control to continue with, copy the_act into		*
   * fail_act in case we return to do_executes. 			*
   *									*
   * Return TRUE to cause this evaluation to return. 			*
   *--------------------------------------------------------------------*/

  if(cf == NULL || cf->right.act == NULL) {
    if(inst != 0 && store_fail_act) {
      fail_act = the_act;
      bump_activation_parts_except_stack(&fail_act);
      fail_act.stack = get_stack(); /* Ref from get_stack */
      fail_ex        = failure_as_entity;
      fail_instr     = inst;
      *push_stack()  = make_fail_ent();
    }
    return TRUE;
  }

  /*--------------------------------------------------------------------*
   * Otherwise, start up the activation that is held at the control 	*
   * node.  If this is a thread switch, note it. If the thread switch	*
   * is for a mix, put the time back to SLICE_TIME.			*
   *									*
   * If the activation that is being started has been marked for	*
   * termination, then start_ctl_or_act will end up with failure =	*
   * TERMINATE_EX.  In that case, just fail again.			*
   *--------------------------------------------------------------------*/

  else {
    register UP_CONTROL *parent;
    bump_control(parent = cf->PARENT_CONTROL);
    if(kind <= BRANCH_F) {
      num_thread_switches++;
      if(kind == MIX_F) *time = SLICE_TIME;
    }

    failure           = -1;
    failure_as_entity = NOTHING;
    start_ctl_or_act(RCHILD_IS_ACT(cf), cf->right, parent, time);
    drop_control(parent);

    if(failure < 0) return FALSE;
    else return do_failure(the_act.control, 0, time);
  }
}


/****************************************************************
 *			GET_TIMEOUT_PARTS			*
 ****************************************************************
 * to_coa, of kind to_kind, has timed out, and c is its parent  *
 * control.  Return the control or activation that represents 	*
 * the currently timed-out computation and set 			*
 *								*
 *    result_kind = the kind of result (0 for control, 		*
 *		    1 for activation) 				*
 *								*
 *    cont        = the control whose right child should be 	*
 *		    continued with.  If cont is NULL, then	*
 *		    there is nothing to continue with.  	*
 *								*
 * If to_coa is NULL, then new nodes in the result are		*
 * simplified to take into account that to_coa does 		*
 * not do anything.						*
 *								*
 * XREF:							*
 *   Called below to find out what to do at a time-out.		*
 *								*
 *   Called in machstrc/control.c to shift a control to the	*
 *   next available thread.					*
 ****************************************************************/

union ctl_or_act get_timeout_parts(UP_CONTROL *c, int to_kind, 
				   union ctl_or_act to_coa, int *result_kind, 
				   UP_CONTROL **cont)
{
  int p_info;

  register UP_CONTROL* p        = c;
  union ctl_or_act     result   = to_coa;
  int                  res_kind = to_kind;

  /*--------------------------------------------------------------------*
   * Look upwards in the parent control for a node that can catch	*
   * a time-out.  Such a node must be a mix node whose child comes	*
   * up from the left.  It will resume with its right child.		*
   *--------------------------------------------------------------------*/

  for(;;) {

    /*----------------------------------------------------------*
     * Break at the end of the chain or at a mix node whose	*
     * child points up from the left.				*
     *----------------------------------------------------------*/

    if(p == NULL) break;

    p_info = p->info;
    if((p_info & (KIND_CTLMASK | UPLINK_CTLMASK)) == MIX_F) {
      break;
    }

    /*----------------------------------------------------------*
     * As we move up through the parent structure, we convert	*
     * the up-control to a down-control.			*
     *								*
     * Build a copy of p as the new result node.  (The copy is 	*
     * a downward pointing control node).  If result is null, 	*
     * then we don't need to build a copy. 			*
     *----------------------------------------------------------*/

    if(result.ctl == NULL) {
      if(CTLKIND(p) < MARK_F) {
        result   = p->right;
	res_kind = TRUE_RCHILD_KIND(p);
      }
    }

    else { /* result.ctl != NULL */
      register DOWN_CONTROL* q = allocate_control();

      q->identity = p->identity;
      if(!UPLINK_IS_FROM_RIGHT(p)) {
        q->info  = (p_info & (KIND_CTLMASK | RCHILD_CTLMASK))
		   | (res_kind << LCHILD_CTLSHIFT);
	q->left  = result;          /* bumped below */
	q->right = p->right;        /* bumped below */
      }
      else {
	q->info  = (p_info & (KIND_CTLMASK)) |
	            ((p_info & RCHILD_CTLMASK) 
		       >> (RCHILD_CTLSHIFT - LCHILD_CTLSHIFT)) |
		    (res_kind << RCHILD_CTLSHIFT);
        q->left  = p->right;   /* bumped below */
        q->right = result;     /* bumped below */
      }
      bmp_ctl_or_act(result, res_kind);		         /* as promised. */
      bmp_ctl_or_act(p->right, p_info & RCHILD_CTLMASK); /* as promised. */

      result.ctl = q;
      res_kind = CTL_F;
    }
    p = p->PARENT_CONTROL;
  }

  /*--------------------------------------------------------------------*
   * Upon exit from the loop, p is pointing to the desired parent	*
   * of the new down-control to enter.  Set *cont to that parent, 	*
   * and return the down-control that was built.			*
   *--------------------------------------------------------------------*/

  *cont = p;
  *result_kind = res_kind;
  return result;
}
  

/*********************************************************
 *			DO_TIMEOUT			 *
 *********************************************************
 * the_act has timed out.  Set things  up for continuing *
 * another activation, or returning from evaluate.	 *
 * Return true if should return from evaluate.		 *
 * Set time if necessary.				 *
 *							 *
 * XREF:						 *
 *   Called in evaluate.c to handle a time out.		 *
 *********************************************************/

Boolean do_timeout(LONG *time)
{
  union ctl_or_act a;
  UP_CONTROL *c;
  Boolean result;

  c = the_act.control;     /* Inherits ref from a.act, below. */
  a.act = get_the_act();   /* Ref from get_the_act. */
  a.act->control = NULL;   /* Here is where we give ref to c. a.act has a *
			    * copy of the_act.control, but we lose that   *
			    * reference here.				  */
  result = do_general_timeout(c, 1, a, time);
  drop_activation(a.act);
  drop_control(c);
  return result;
}


/****************************************************************
 *			DO_GENERAL_TIMEOUT			*
 ****************************************************************
 * Computation to_coa, of kind to_kind (CTL_F = CONTROL, 	*
 * ACT_F = ACTIVATION) has timed out, and its parent control	*
 * is ctl.							*
 *								*
 * Set things up for continuing another activation, or 		*
 * returning from evaluate. Return TRUE if should return from 	*
 * evaluate. Set time if necessary.				*
 ****************************************************************/

PRIVATE Boolean do_general_timeout(UP_CONTROL *ctl, int to_kind, 
			   union ctl_or_act to_coa, LONG *time)
{
  union ctl_or_act oldcont;
  UP_CONTROL *newcont, *c;
  int kind;

  /*----------------------------------*
   * Indicate one more thread switch. *
   *----------------------------------*/

  num_thread_switches++;

  /*--------------------------------------------------------------*
   * Get the old continuation (to defer) and the new continuation *
   * (to continue with).					  *
   *--------------------------------------------------------------*/

  oldcont = get_timeout_parts(ctl, to_kind, to_coa, &kind, &newcont);
  bump_ctl_or_act(oldcont, kind);

# ifdef DEBUG
    if(trace) {
      trace_i(95);
      if(trace_control) {
        print_up_control(TRACE_FILE, ctl, 1);
      }
      if(kind == CTL_F) {
	if(trace_control) {
          trace_i(96);
	  print_down_control(TRACE_FILE, oldcont.ctl, "", 1);
	}
      }
      else {
        trace_i(96);
	fprintf(TRACE_FILE,"<%s>\n", nonnull(oldcont.act->name));
      }
      if(trace_control) {
        trace_i(97);
        print_up_control(TRACE_FILE, newcont, 1);
      }
      tracenl();
    }
# endif

  /*----------------------------------------------------*
   * Should return from evaluate if no place to resume. *
   *----------------------------------------------------*/

  if(newcont == NULL) {
    drop_ctl_or_act(timed_out_comp, timed_out_kind);
    timed_out_kind = kind;
    timed_out_comp = oldcont;  /* Ref from oldcont. */
    return TRUE;
  }

  /*------------------------------------------------------*
   * Otherwise, set up new control and resume activation. *
   * If the activation being resumed has been marked for  *
   * termination, then force a failure now.		  *
   *------------------------------------------------------*/

  else {
    bump_control(newcont);
    bmp_control(c = allocate_control());
    bump_control(c->PARENT_CONTROL = newcont->PARENT_CONTROL);
    c->right = oldcont;    			    /* Ref from oldcont. */
    c->info = CTLKIND(newcont) | UPLINK_CTLMASK | (kind << RCHILD_CTLSHIFT);
    c->identity = newcont->identity;
    *time = SLICE_TIME;
    failure           = -1;
    failure_as_entity = NOTHING;
    start_ctl_or_act(RCHILD_IS_ACT(newcont), newcont->right, c, time);
    drop_control(newcont);
    drop_control(c);

    if(failure < 0) return FALSE;
    else return do_failure(the_act.control, 0, time);
  }
}
