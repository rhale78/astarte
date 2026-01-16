/**********************************************************************
 * File:    evaluate/apply.c
 * Purpose: Handle function application and return for expression evaluator
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
 * This file provides support for function application in the 		*
 * evaluator.  It handles ordinary function application and tail	*
 * application.								*
 *									*
 * This file also handles pausing of threads.				*
 *									*
 * This file also handle processing of demons attached to boxes.	*
 ************************************************************************/

#include "../misc/misc.h"
#include "../utils/lists.h"
#include "../utils/rdwrt.h"
#include "../alloc/allocate.h"
#include "../classes/classes.h"
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
#include "../clstbl/typehash.h"
#include "../show/printrts.h"
#include "../show/profile.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/********************************************************
 *			START_FUN			*
 ********************************************************
 * Execute initial instructions at address start, and	*
 * return the address following initial instructions.	*
 *							*
 * There are k+1 arguments on the stack, which are 	*
 * supposed to be made into a product.	Any split	*
 * instructions can be done by just failing to do pairs.*
 * For example, if k = 3 and there is a single SPLIT_I  *
 * at the start of the function, then skip over the	*
 * SPLIT_I instruction and only do one PAIR_I operation.*
 *							*
 * A NAME_I or SNAME_I instruction is also handled 	*
 * here, since SPLIT_I instructions will be behind it.  *
 ********************************************************/

PRIVATE CODE_PTR start_fun(CODE_PTR start, int k)
{
  LONG n;
  CODE_PTR q = start;

  /*-------------------------------------------------------------*
   * Do the NAME_I or SNAME_I instruction, in order to reach the *
   * subsequent SPLIT_I instructions, if any. 			 *
   *-------------------------------------------------------------*/

  if(*q == NAME_I) {
    q++;
    n = next_int_m(&q);
    the_act.name = outer_bindings[n].name;
  }
  else if(*q == SNAME_I) {
    q++;
    n = next_int_m(&q);
    the_act.name = CSTRVAL(constants[n]);
  }

  /*--------------------------------------------------------------*
   * We are requested to simulate k PAIR_I instructions.  Do them *
   * now, possibly by skipping over SPLIT_I and NONNIL_TEST_I     *
   * instructions. 						  *
   *--------------------------------------------------------------*/

  while(k > 0 && (*q == SPLIT_I || *q == NONNIL_TEST_I)) {
    k--;
    q++;
  }
  if(k > 0) multi_pair_pr(k);

  return q;
}


/********************************************************
 *			REV_APPLY_I			*
 ********************************************************
 * Function rev_apply_i performs REV_APPLY_I and 	*
 * REV_PAIR_APPLY_I instructions.       		*
 * The function to be applied by the instruction has    *
 * k+1 arguments.					*
 *							*
 * NOTE: rev_apply_i is inlined in evaluate.c.  Any	*
 * changes made here should be reflected there.		*
 ********************************************************/

void rev_apply_i(int k, LONG *time_bound)
{
  /*------------------------------------------------------------*
   * Get the function and evaluate it. Leave it on the stack.	*
   *------------------------------------------------------------*/

  ENTITY* fp = top_stack();
  IN_PLACE_EVAL_FAILTO(*fp, time_bound, out);

  /*------------------------------------------------------------*
   * If evaluation succeeded, pop f and apply it.  Note that f  *
   * must be a function.					*
   *------------------------------------------------------------*/

  {ENTITY f = POP_STACK();
   if(TAG(f) != FUNCTION_TAG) dump_die(78);
   do_apply(CONTVAL(f), k);
  }

 out: 
  return;
}


/****************************************************************
 *			DO_APPLY				*
 ****************************************************************
 * do_apply_i(fa, k,) applies the function described		*
 * by continuation fa to the result of applying k PAIR_I 	*
 * instructions to the stack.  The result of this function      *
 * application is left on the stack.				*
 *								*
 * Applying this function involves updating the_act.		*
 * The new activation gets its parts from the following sources.*
 *								*
 *	program_ctr	From the function			*
 *	env		From the function			*
 *      num_entries     From the function			*
 *      type_binding_lists From the function.			*
 *	continuation	Set to the calling activation		*
 *	state_a		From the current activation.		*
 *      state_hold      From the current activation.		*
 *      trap_vec_a      From the current activation.		*
 *      trap_vec_hold   From the current activation.		*
 *      exception_list  NIL					*
 *      control		From the current activation.		*
 *      embedded_controsl From the current activation.		*
 *	stack		From the current activation.		*
 *      coroutines      From the current activation.		*
 ****************************************************************/

void do_apply(CONTINUATION *fa, int k)
{
  register CONTINUATION *cont;
  CODE_PTR start;
  int n, nl;
  int descr_num;

  /*--------------------------------------------------------------*
   * Set up a continuation, telling where we were before applying *
   * this function. cont inherits the_act's references so the     *
   * reference counts are not dropped below. 			  *
   *--------------------------------------------------------------*/
  
  cont                 = allocate_continuation();
  cont->continuation   = the_act.continuation; /* Inherits ref from the_act */
  cont->program_ctr    = the_act.program_ctr;
  cont->env            = the_act.env;          /* Inherits ref from the_act */
  cont->num_entries    = the_act.num_entries;
  cont->type_binding_lists = the_act.type_binding_lists; /* Inherits ref from 
							  the_act*/
  cont->exception_list = the_act.exception_list; /* Inherits ref from 
						    the_act */
  cont->name           = the_act.name;
  cont->result_type_instrs = the_act.result_type_instrs;

  /*---------------------------------------------------------------*
   * Set up the environment. Do not drop ref count on current env. *
   *---------------------------------------------------------------*/

  n = *(start = fa->program_ctr);     	     /* local env byte */
  start++;
  descr_num = toint(next_int_m(&start));     /* local env descriptor number */
  if(n == 0) {

    /*-----------------------*
     * No locals are needed. *
     *-----------------------*/

    nl                   = fa->num_entries;
    bump_env(the_act.env = fa->env, nl);
    the_act.num_entries  = nl;
  }
  else {

    /*-----------------------*
     * Locals are needed.    *
     *-----------------------*/

    the_act.env                   = allocate_local_env(n);   /* Ref cnt = 1 */
    nl                            = fa->num_entries;
    bump_env(the_act.env->link    = fa->env, nl);
    the_act.env->num_link_entries = nl;
    the_act.env->descr_num        = descr_num;
    the_act.num_entries           = 0;
  }

  /*----------------------------------------------------------------------*
   * The function starts execution just after the environment size index. *
   * byte and descriptor number. Start the function.  If k = 0, then      *
   * we can let the function start itself.  But if we are profiling,      *
   * then it is necessary to start the function, in order to get          *
   * the_act.name installed.						  *
   *----------------------------------------------------------------------*/

  the_act.continuation       = cont;	/* Ref from allocate_continuation. */
  the_act.exception_list     = NIL;     /* Old ref inherited above. */
  the_act.pack_name          = NULL;
  bump_list(the_act.type_binding_lists = fa->type_binding_lists); 
  					/* Old ref inherited above. */
  the_act.result_type_instrs = fa->result_type_instrs;
  if(do_profile) {
    char* caller = the_act.name;
    the_act.program_ctr = start_fun(start, k);
    install_profile_call(caller, the_act.name);
  }
  else {
    the_act.program_ctr = start_fun(start, k);
  }

  /*-------------------------------*
   * Possibly break for debugging. *
   *-------------------------------*/

  if(break_info.break_applies && !break_info.suppress_breaks && breakable()) {
    break_info.break_apply_name = fa->name;
    break_info.pc = the_act.program_ctr;
    vis_break(APPLY_VIS);
  }

  /*-------------------------------------------------------------------*
   * Possibly abort if there are too many stack frames.  Ask the user. *
   *-------------------------------------------------------------------*/

  {register LONG depth = the_act.st_depth;
   the_act.st_depth = depth + 1;
   if(depth >= max_st_depth) {
     char message[100];
#    ifdef MSWIN
       sprintf(message,
	       "Runtime stack has reached %ld frames\n"
	       "Continue?",
	       depth);
#    else
       sprintf(message,
	       "\nRuntime stack has reached %ld frames\n"
	       "Use -s<n> option to astr to set limit to <n>\n",
	       depth);
#    endif
     max_st_depth += max_st_depth;
     if(possibly_abort_dialog(message)) {
       max_st_depth = LONG_MAX;
     }
   }
  }
}


/***********************************************************
 *		REV_TAIL_APPLY_I	   		   *
 ***********************************************************
 * Function rev_tail_apply_i performs instructions 	   *
 * REV_TAIL_APPLY_I and REV_PAIR_TAIL_APPLY_I.		   *
 *							   *
 * k is the number of PAIR_I operations to simulate at the *
 * start of the function.				   *
 *							   *
 * NOTE: rev_tail_apply_i is inlined in evaluate.c.  Any   *
 * changes here might need to be reflected there as well.  *
 ***********************************************************/

void rev_tail_apply_i(int k, LONG *time_bound)
{
  /*-----------------------------------*
   * Get the function and evaluate it. *
   *-----------------------------------*/

  ENTITY* fp = top_stack();
  IN_PLACE_EVAL_FAILTO(*fp, time_bound, out);

  /*------------------------------------------------------------*
   * If evaluation succeeded, pop f and apply it.  Note that f  *
   * must be a function.					*
   *------------------------------------------------------------*/
  
  {ENTITY f = POP_STACK();
   if(TAG(f) != FUNCTION_TAG) die(78);
   do_tail_apply(CONTVAL(f), k);
  }

 out: 
  return;
}


/****************************************************************
 *			DO_TAIL_APPLY				*
 ****************************************************************
 * do_tail_apply(fa, k) is the same as do_apply(fa,k), but for  *
 * a tail recursive application.  The continuation is,          *
 * therefore, inherited from the current activation.  		*
 * The environment is reused if possible.  			*
 ****************************************************************/

void do_tail_apply(CONTINUATION *fa, int k)
{
  CODE_PTR start;
  int n, nl;
  int descr_num;

  /*---------------------------------------------------------------------*
   * Set up the environment. If the current environment is large enough, *
   * just reuse it.  If it is too small, then get a new environment.     *
   *---------------------------------------------------------------------*/

  n = *(start = fa->program_ctr);          /* local env byte */
  start++;
  descr_num = toint(next_int_m(&start));   /* local env descriptor number */

  /*------------------------------------------------------------------*
   * If n == 0, then no locals are needed.  Keep the old environment. *
   *------------------------------------------------------------------*/

  if(n == 0) {
    drop_env(the_act.env, the_act.num_entries);
    nl                   = fa->num_entries;
    bump_env(the_act.env = fa->env, nl);
    the_act.num_entries  = nl;
  }

  /*--------------------------------------------------------------------*
   * If n > 0, then a new environment node is needed.  Attach it to the *
   * front of the old environment chain. 				*
   *--------------------------------------------------------------------*/

  else { /* n != 0 */

    /*-------------------------------------------------------------------*
     * This is a tail call, so we might be able to use the old 		 *
     * local environment node.  If the front node in the environment 	 *
     * chain is not a local node, or if it is too small, or if it has    *
     * multiple references, then we are not free to use it, so get a new *
     * node.  Drop reference to the old node, since it is history now.   *
     *-------------------------------------------------------------------*/

    if(the_act.env->kind != LOCAL_ENV || n > the_act.env->sz 
       || the_act.env->ref_cnt > 1) {
      drop_env(the_act.env, the_act.num_entries);
      the_act.env = allocate_local_env(n);
    }

    /*------------------------------------------------------------------*
     * Reuse the node when suitable. So drop the reference to the node  *
     * beyond the first node in the environment chain, since it will    *
     * be replaced.							*
     *------------------------------------------------------------------*/

    else {
      drop_env(the_act.env->link, the_act.env->num_link_entries);
    }

    /*------------------------------------------------------*
     * Install the information about this environment node. *
     *------------------------------------------------------*/

    nl                            = fa->num_entries;
    bump_env(the_act.env->link    = fa->env, nl);
    the_act.env->num_link_entries = nl;
    the_act.env->descr_num        = descr_num;
    the_act.num_entries           = 0;
    drop_env_ref(the_act.env, the_act.env->most_entries);
    the_act.env->most_entries     = 0;
  } /* end else(n != 0) */

  /*-----------------------------------------------------------------------*
   * The function starts execution just after the environment size index.  *
   * byte and descriptor number.  start_fun finds that location.  We       *
   * start with an empty exception list, since we are not inside a try.    *
   *									   *
   * If we are profiling, we must start the function now to get		   *
   * the_act.name.							   *
   *-----------------------------------------------------------------------*/

  if(do_profile) {
    char* caller = the_act.name;
    the_act.program_ctr = start_fun(start, k);
    install_profile_call(caller, the_act.name);
  }
  else {
    the_act.program_ctr = start_fun(start, k);
  }
  SET_LIST(the_act.exception_list, NIL);
  SET_LIST(the_act.type_binding_lists, fa->type_binding_lists);
  the_act.result_type_instrs = fa->result_type_instrs;
  the_act.pack_name          = NULL;

  /*-------------------------------*
   * Possibly break for debugging. *
   *-------------------------------*/

  if(break_info.break_applies && !break_info.suppress_breaks && breakable()) {
    break_info.break_apply_name = fa->name;
    break_info.pc = the_act.program_ctr;
    vis_break(APPLY_VIS);
  }
}


/********************************************************
 *			QEQ_APPLY_I			*
 ********************************************************
 * Do a QEQ_APPLY_I instruction.  (The stack contains, 	*
 * from top down, function ==, argument x and 		*
 * argument y.  If either of x and y has a NOREF_TAG    *
 * tag, then x and y are equal just when they are 	*
 * identical in form.  Otherwise, handle by calling ==. *
 *							*
 * (== itself would get the right result, but it is     *
 * more efficient to do the test here for NOREF_TAG	*
 * entities.)			 			*
 *							*
 * XREF: Called in evaluate.c to handle QEQ_APPLY_I	*
 * instruction.						*
 ********************************************************/

void qeq_apply_i(LONG *time_bound)
{
  ENTITY f, arg1, arg2, *a;
  REG_TYPE mark = reg3(&f, &arg1, &arg2);

  /*--------------------------------------------------------------------*
   * Get the function and the arguments from the stack.  Evaluate the   *
   * arguments.  Wait to evaluate f until it is needed.			*
   *--------------------------------------------------------------------*/

  f    = pop_stack();               /* The == function to apply */
  arg1 = pop_stack();
  a    = top_stack();
  SET_EVAL_FAILTO(arg2, *a, time_bound, fail);  
  IN_PLACE_EVAL_FAILTO(arg1, time_bound, fail);  
  goto got_args;  /* below, around fail code */

  /*--------------------------------------------------*
   * A failure might be a time-out.  Put things back. *
   *--------------------------------------------------*/

 fail:
    *a = arg2;
    *push_stack() = arg1;
    *push_stack() = f;
    unreg(mark);
    return;

 got_args:

  /*--------------------------------------------------------*
   * If arg1 and arg2 are transparently equal, return true. *
   *--------------------------------------------------------*/

  if(ENT_EQ(arg1, arg2)) {
    *a = true_ent; 
    unreg(mark); 
    return;
  }

  /*--------------------------------------------------------------*
   * If arg1 and arg2 both have tag NOREF_TAG, then they must not *
   * be equal, since if they were the preceding case would have   *
   * returned true. 						  *
   *--------------------------------------------------------------*/

  if(TAG(arg1) == NOREF_TAG || TAG(arg2) == NOREF_TAG) {
    *a = false_ent; 
    unreg(mark); 
    return;
  }

  /*---------------------------------------------------------------*
   * If we get here, we must apply function f. Evaluate it first.  *
   * A failure in evaluating f might be a timeout, so push f back. *
   *---------------------------------------------------------------*/

  *a            = arg2;
  *push_stack() = arg1;
  IN_PLACE_EVAL_FAILTO(f, time_bound, fail2);  /* fail2 is below. */
  if(TAG(f) != FUNCTION_TAG) die(78);
  do_apply(CONTVAL(f), 1);
  goto out;

 fail2: *push_stack() = f;

 out:
  unreg(mark);
}

	  
/****************************************************************
 *			SHORT_APPLY				*
 ****************************************************************
 * Compute f(a), but with time cut short if the computation     *
 * takes a LONG time.  If the computation is cut short, a lazy  *
 * entity is returned.  time_bound is the time limit variable.	*
 *								*
 * XREF: Called in evaluate.c to handle SHORT_APPLY_I.		*
 ****************************************************************/

ENTITY short_apply(ENTITY f, ENTITY a, LONG *time_bound)
{
  register ENTITY result;
  LONG init_time, time;

  time = *time_bound;
  if(time > SHORT_APPLY_TIME) time = SHORT_APPLY_TIME;
  init_time = time;
  result = run_fun(f, a, the_act.state_a, the_act.trap_vec_a, &time);
  if(failure == TIME_OUT_EX) failure = -1;
  *time_bound -= init_time - time;
  return result;
}


/********************************************************
 *			PROCESS_DEMON			*
 ********************************************************
 * Function handleDemons is written in Astarte in 	*
 * standard.asi.  This function calls			*
 * handleDemons(oldcontent,newcontent,demon).		*
 * However, it converts oldcontent and newcontent to 	*
 * type Optional(T) first, for an appropriate type T.   *
 * If either is NOTHING, it is converted to noValue.	*
 *							*
 * XREF: Called in evaluate.c to handle MAKE_EMPTY_I	*
 * and ASSIGN_I.					*
 ********************************************************/

void process_demon(ENTITY demon, ENTITY oldcontent, ENTITY newcontent)
{
  TYPE* Any_type       = wrap_tf(NULL);
  TYPE* Optional_fam   = fam_id_t("Optional");
  TYPE* demon_type     = list_t(Any_type);
  TYPE* l_content_type = fam_mem_t(Optional_fam, hermit_type);
  TYPE* handle_arg     = pair_t(l_content_type, 
			        pair_t(l_content_type, demon_type));
  TYPE* t              = function_t(handle_arg, hermit_type);
  LONG  time           = LONG_MAX;
  ENTITY fun;
  REG_TYPE mark = reg1(&fun);

  /*----------------------------------------------------*
   * Get the argument to handleDemons.  The argument is *
   * (oldcontent, newcontent, demon).  Push it onto the *
   * stack.						*
   *----------------------------------------------------*/

  ENTITY optional_newcontent = ENT_EQ(newcontent, NOTHING) 
    				? noValue 
				: make_optional(newcontent);
  ENTITY optional_oldcontent = ENT_EQ(oldcontent, NOTHING) 
    				? noValue 
				: make_optional(oldcontent);

  *(push_stack()) = ast_triple(optional_oldcontent, 
			       optional_newcontent, 
			       demon);

  /*--------------------------*
   * Get handleDemons itself. *
   *--------------------------*/

  {ENTITY gp[2];
   gp[0] = ENTG(ent_str_tb(std_id[HANDLEDEMONS_ID]));
   bump_type(t);
   gp[1] = ENTP(TYPE_TAG, type_tb(t));
   drop_type(t);
   fun   = global_eval(gp, &time);
  }

  if(failure >= 0 || TAG(fun) != FUNCTION_TAG) die(78);

  /*-------------------*
   * Run handleDemons. *
   *-------------------*/

  do_apply(CONTVAL(fun), 0);
  unreg(mark);
}


/**********************************************************
 *			RETURN_I			  *
 **********************************************************
 * Return from a function application or lazy evaluation. *
 * This involves moving the_act.continuation back into    *
 * the_act.  inst is the instruction (RETURN_I or         *
 * INVISIBLE_RETURN_I).  Don't break for debugging at     *
 * an INVISIBLE_RETURN_I, but do break at a RETURN_I.	  *
 *							  *
 * XREF: Called in evaluate.c to handle RETURN_I and	  *
 * INVISIBLE_RETURN_I.					  *
 **********************************************************/

void return_i(int inst)
{
  CONTINUATION* cont = the_act.continuation;

  /*-------------------------------*
   * Possibly break for debugging. *
   *-------------------------------*/

  if(break_info.break_applies && !break_info.suppress_breaks 
     && inst == RETURN_I && breakable()) {
    break_info.pc = the_act.program_ctr - 1;
    vis_break(RETURN_VIS);
  }

  /*----------------------------------------------------------------*
   * If this is the top level activation, notify evaluate to return *
   * by setting the_act.program_ctr to NULL.			    *
   *----------------------------------------------------------------*/

  if(cont == NULL) {
    the_act.program_ctr = NULL;
    return;
  }

  /*----------------------------------------------------------------*
   * Install the continuation into the current activation. the_act  *
   * inherits references from cont. 				    *
   *----------------------------------------------------------------*/

# ifdef GCTEST
    check_cont_rc(cont);
# endif

  drop_env(the_act.env, the_act.num_entries);
  drop_list(the_act.exception_list);
  drop_list(the_act.type_binding_lists);
  the_act.exception_list      = cont->exception_list;
  the_act.type_binding_lists   = cont->type_binding_lists;
  the_act.continuation        = cont->continuation;
  the_act.program_ctr         = cont->program_ctr;
  the_act.name                = cont->name;
  the_act.pack_name	      = NULL;
  the_act.result_type_instrs  = cont->result_type_instrs;
  the_act.env                 = cont->env;
  the_act.num_entries         = cont->num_entries;
  the_act.st_depth--;

  /*----------------------------------------------------------------*
   * Typically, cont will only have one reference.  If so, free it. *
   * If not, bump its parts, so that they are still referenced.     *
   *----------------------------------------------------------------*/

  if(cont->ref_cnt == 1) free_continuation(cont);
  else {

    /*----------------------------------------------------------------*
     * Need to do explicit bumps, since can't inherit refs from cont. *
     *----------------------------------------------------------------*/

    bump_env(the_act.env, the_act.num_entries);
    bump_continuation(the_act.continuation);
    bump_list(the_act.exception_list);
    bump_list(the_act.type_binding_lists);
    cont->ref_cnt--;
  }

}


/********************************************************
 *		DO_FUNCTION_RETURN_APPLY		*
 ********************************************************
 * The_act is about to execute a FUNCTION_I instruction *
 * that is followed by a RETURN_I.  The instruction	*
 * that will be returned to is a REV_APPLY_I or related *
 * instruction.  Process all of those instructions.	*
 *							*
 * cont_pc is the address of the program_ctr field in   *
 * the continuation, where the REV_APPLY_I or related   *
 * instruction is pointed to.  type_instrs_index is the *
 * index of the type_instrs code for the function       *
 * that is being created and applied.			*
 ********************************************************/

void do_function_return_apply(CODE_PTR *cont_pc, int type_instrs_index)
{

  int nn, descr_num;
  Boolean use_tail_apply = FALSE;
  int pair_apply_k = 0;

  /*-------------------------------------------------*
   * Skip over the apply instruction in the          *
   * continuation, and get information about how     *
   * to do the apply.				     *
   *-------------------------------------------------*/

  switch(**cont_pc) {
    case REV_TAIL_APPLY_I:
      if(do_tro) use_tail_apply = TRUE;
      /* No break - continue with next case. */
    case REV_APPLY_I:
      (*cont_pc)++;
      break;

    case REV_PAIR_TAIL_APPLY_I:
      if(do_tro) use_tail_apply = TRUE;
      /* No break - fall through to next case. */
    case REV_PAIR_APPLY_I:
      pair_apply_k = (*cont_pc)[1];
      (*cont_pc) += 2;
      break;
  }

  the_act.result_type_instrs  = lazy_type_instrs[type_instrs_index];
  SET_LIST(the_act.exception_list, NIL);

  /*---------------------------------------------------*
   * Get the environment size byte and the environment *
   * descriptor number for the new function.	       *
   *---------------------------------------------------*/

  nn        = *(the_act.program_ctr++);  /* env size byte */
  descr_num = toint(next_int_m(&the_act.program_ctr));
  if(nn != 0) {
    ENVIRONMENT* env = allocate_local_env(nn); /* Ref cnt = 1 */
    bump_env(env->link = the_act.env, the_act.num_entries);
    env->num_link_entries = the_act.num_entries;
    env->descr_num = descr_num;
    the_act.num_entries = 0;
    the_act.env = env;
  }

  /*-------------------------------------------------------*
   * If the apply instruction is a tail-application, and   *
   * tail-apply is not suppressed, then bypass the 	   *
   * continuation.					   *
   *						     	   *
   * If the apply instruction is a tail application, 	   *
   * then the effect of the RETURN_I/apply 	     	   *
   * combination will be to decrease the stack 	     	   *
   * depth by one.					   *
   *-------------------------------------------------------*/

  if(use_tail_apply) {
    register CONTINUATION* contin = the_act.continuation;
    the_act.continuation = contin->continuation;  /* Ref from 
						     contin->continuation */
    the_act.st_depth--;
    contin->continuation = NULL;
    drop_continuation(contin);
  }

  /*-------------------------*
   * Start the new function. *
   *-------------------------*/

  if(pair_apply_k > 0) {
    the_act.program_ctr = start_fun(the_act.program_ctr, pair_apply_k);  
  }
}


/********************************************************
 *			PAUSE_I				*
 ********************************************************
 * Do a pause instruction.  Return TRUE if timed out,   *
 * and FALSE if switched to another coroutine.		*
 *							*
 * XREF: Called in evaluate.c to handle PAUSE_I and	*
 * REPAUSE_I.						*
 ********************************************************/

Boolean pause_i(LONG *time)
{
  /*-------------------------------------*
   * Try to switch to another coroutine. *
   *-------------------------------------*/

  if(coroutine_pause()) return FALSE;

  /*--------------------------------------------*
   * If no coroutines to resume, just time out. *
   *--------------------------------------------*/

# ifdef DEBUG
    if(trace) trace_i(77);
# endif

  *time   = 0;
  failure = TIME_OUT_EX;
  return TRUE;
}


/****************************************************************
 *			PAUSE_THIS_THREAD			*
 ****************************************************************
 * Pause the current thread, due to some kind of block.		*
 * time_bound is the time counter, which is set to 0.		*
 ****************************************************************/

void pause_this_thread(LONG *time_bound)
{
  *time_bound = 0;
  failure = TIME_OUT_EX;
  if(the_act.progress) the_act.progress = 0;
  else num_repauses++;
}
