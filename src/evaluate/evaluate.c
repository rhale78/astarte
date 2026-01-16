/**********************************************************************
 * File:    evaluate/evaluate.c
 * Purpose: Expression evaluator
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
 * This file contains the expression evaluator, called evaluate.	*
 * This function is quite long, since it handles all instructions,	*
 * some of them by code that is written here for efficiency.		*
 ************************************************************************/

#include <string.h>
#include "../misc/misc.h"
#include "../utils/lists.h"
#include "../utils/hash.h"
#include "../utils/rdwrt.h"
#include "../utils/strops.h"
#include "../alloc/allocate.h"
#include "../classes/classes.h"
#include "../unify/unify.h"
#include "../unify/constraint.h"
#include "../machdata/entity.h"
#include "../machdata/except.h"
#include "../machstrc/machstrc.h"
#include "../intrprtr/intrprtr.h"
#include "../evaluate/evaluate.h"
#include "../evaluate/instruc.h"
#include "../rts/rts.h"
#include "../gc/gc.h"
#include "../standard/stdtypes.h"
#include "../tables/tables.h"
#include "../clstbl/classtbl.h"
#include "../clstbl/typehash.h"
#include "../show/getinfo.h"
#include "../show/prtent.h"
#include "../show/printrts.h"
#include "../show/profile.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif
#ifdef SMALL_STACK
#  ifdef USE_MALLOC_H
#    include <malloc.h>
#  else
#    include <stdlib.h>
#  endif
#endif
#ifdef MSWIN
  void handlePendingMessages(void);
  int handlePendingCount = 0;
  void flush_console_buff(void);
  void setConsoleTitle(char *);
#endif

#ifdef DEBUG
PRIVATE void trace_eval	(CODE_PTR qt_goto, int qt_stack_depth, 
			 LONG l_time, int inst);
#endif

/****************************************************************
 *			PUBLIC VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			special_condition			*
 ****************************************************************
 * When there is a special condition to check when the		*
 * evaluator gets to the top of its main loop, special_condition*
 * is set positive.  That way, we only need to test one variable,*
 * not several.  special_condition is volatile because it can   *
 * be set at an interrupt.					*
 ****************************************************************/

volatile int special_condition = 0; 

/****************************************************************
 *			should_pause				*
 ****************************************************************
 * should_pause is set TRUE if the user has requested that the  *
 * interpreter should pause.  It is volatile because it can	*
 * be set asynchronously.					*
 ****************************************************************/

volatile char should_pause = 0;

/****************************************************************
 *			perform_gc				*
 ****************************************************************
 * Set true when evaluate should perform a			*
 * garbage collection the next time it reaches the top of	*
 * its main loop. It is set by one of the allocators in		*
 * alloc/gcalloc.c 						*
 ****************************************************************/

Boolean perform_gc = 0;

/****************************************************************
 *			interrupt_occurred			*
 ****************************************************************
 * interrupt_occurred is set true when an interrupt occurs.  It *
 * tested by the evaluator, to break off evaluation in a	*
 * clean way.							*
 ****************************************************************/

volatile Boolean interrupt_occurred = 0;

/****************************************************************
 *			do_profile				*
 ****************************************************************
 * do_profile is set TRUE when profiling information should	*
 * be written.							*
 ****************************************************************/

Boolean do_profile = 0;

/****************************************************************
 *			failure					*
 *			failure_as_entity			*
 *			last_exception				*
 ****************************************************************
 * failure = -1 normally.  When computation fails,		*
 * failure is set to a nonnegative value that is		*
 * the tag of the exception. Failure is volatile		*
 * because it can be set by an interrupt.  			*
 *								*
 * failure_as_entity is the exception that occurred,		*
 * if the last exception was an exception that has		*
 * associated data.  For simple exceptions, failure_as_entity	*
 * is nil. 							*
 *								*
 * last_exception is the value of the last exception.		*
 * It is used to keep track of an exception after failure	*
 * and failure_as_entity have been reset. 			*
 ****************************************************************/

volatile int failure = -1;	

ENTITY failure_as_entity, last_exception;


/****************************************************************
 *			in_show					*
 ****************************************************************
 * in_show is set to a nonzero value when in			*
 * a call to view.  That way, lazyDollar 			*
 * knows whether to be lazy.  Each entry to			*
 * show increments in_show, and each exit from			*
 * show decrements in_show. 					*
 ****************************************************************/

int in_show = 0;

/****************************************************************
 *			PRIVATE VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			eval_temp_ent				*
 ****************************************************************
 * eval_temp_ent is used to store a tempory entity		*
 * for LOCK_TEMP_I and GET_TEMP_I instructions.			*
 ****************************************************************/

PRIVATE ENTITY eval_temp_ent;

/****************************************************************
 *			timeout_at_end_atomic			*
 ****************************************************************
 * timeout_at_end_atomic is true to indicate that a timeout	*
 * should occur as soon as the current thread is no longer	*
 * inside an atomic-construct. 					*
 ****************************************************************/

PRIVATE Boolean timeout_at_end_atomic = FALSE;


/************************************************************************
 *			PREFIX INSTRUCTION TABLES			*
 ************************************************************************
 * These tables associate functions with instructions that follow       *
 * UNARY_PREF_I, BINARY_PREF_I, LIST1_PREF, LIST2_PREF_I, TY_PREF_I     *
 * or UNK_PREF_I.							*
 ************************************************************************/

PRIVATE ENTITY 	(*unaries[N_UNARIES + 1])(ENTITY) =
{/* 0 				*/	NULL,
 /* 1 CONTENT_STDF 		*/	ast_content_stdf,
 /* 2 CONTENT_TEST_STDF 	*/      ast_content_test_stdf,
 /* 3 PRCONTENT_STDF 		*/	prcontent_stdf,
 /* 4 MAKE_ARRAY_STDF 		*/      ast_array,
 /* 5 MAKE_PARRAY_STDF 		*/      ast_place_array,
 /* 6 RANK_BOX_STDF 		*/      ast_rank_box,
 /* 7 FLAVOR_STDF		*/	ast_box_flavor,
 /* 8 BOXFLAVOR_TO_STRING_STDF  */	boxflavor_to_string,
 /* 9 COPYFLAVOR_TO_STRING_STDF  */	copyflavor_to_string,

 /* 10 NAT_TO_HEX_STDF		*/	ast_int_to_hex_string,
 /* 11 ZEROP_STDF 		*/	ast_zerop,
 /* 12 NEGATE_STDF 		*/      ast_negate,
 /* 13 SQRT_STDF 		*/ 	sqrt_real,
 /* 14 FLOOR_STDF 		*/ 	ast_floor,
 /* 15 CEILING_STDF 		*/ 	ast_ceiling,
 /* 16 NATURAL_STDF 		*/ 	ast_natural,
 /* 17 INTEGER_STDF		*/	ast_integer,
 /* 18 RATIONAL_STDF		*/ 	ast_rational,
 /* 19 REAL_STDF 		*/	ast_real,
 /* 20 ABS_STDF 		*/     	ast_abs,
 /* 21 RECIPROCAL_STDF 		*/ 	ast_reciprocal,
 /* 22 MAKE_RAT1_STDF 		*/	ast_make_rat1,
 /* 23 ODD_STDF 		*/ 	ast_odd,
 /* 24 SIGN_STDF 		*/ 	ast_sign_as_ent,
 /* 25 PRED_STDF		*/	pred_stdf,
 /* 26 EXP_STDF 		*/	exp_real,
 /* 27 LN_STDF 			*/	ln_real,
 /* 28 SIN_STDF 		*/	sin_real,
 /* 29 PULL_APART_REAL_STDF 	*/	pull_apart_real,
 /* 30 INVTAN_STDF 		*/	atan_real,
 /* 31 STRING_TO_REAL_STDF	*/	ast_string_to_real,
 /* 32 STRING_TO_NAT_STDF 	*/	ast_string_to_nat,
 /* 33 STRING_TO_INT_STDF 	*/	ast_string_to_int,
 /* 34 STRING_TO_RAT_STDF 	*/	ast_string_to_rat,
 /* 35 HEX_TO_NAT_STDF		*/	ast_hex_to_nat,

 /* 36 FCLOSE_STDF 		*/	ast_fclose,
 /* 37 CHDIR_STDF 		*/	chdir_stdf,
 /* 38 GETENV_STDF 		*/	getenv_stdf,
 /* 39 DATE_STDF 		*/	date_stdf,
 /* 40 SECONDS_STDF 		*/	seconds_stdf,
 /* 41 FSTAT_STDF 		*/	fstat_stdf,
 /* 42 RM_STDF 			*/	rm_stdf,
 /* 43 RMDIR_STDF 		*/	rmdir_stdf,
 /* 44 MKDIR_STDF 		*/	mkdir_stdf,
 /* 45 DIRLIST_STDF 		*/	dirlist_stdf,
 /* 46 GETCWD_STDF 		*/	getcwd_stdf,
 /* 47 FLUSH_FILE_STDF 		*/	flush_file_stdf,
 /* 48 OS_ENV_STDF		*/	os_env_stdf,

 /* 49 TO_STRING1_STDF 		*/	ast_num_to_strng,
 /* 50 BOOL_TO_STRING_STDF 	*/   	bool_to_string,
 /* 51				*/   	NULL,
 /* 52 HERMIT_TO_STRING_STDF 	*/     	hermit_to_string,
 /* 53 EXCEPTION_TO_STRING_STDF */  	exception_to_string,
 /* 54 COMPARISON_TO_STRING_STDF*/ 	comparison_to_string,
 /* 55 FILEMODE_TO_STRING_STDF	*/ 	fileMode_to_string,
 /* 56 OUTFILE_DOLLAR_STDF 	*/	outfile_dollar_stdf,
 /* 57				*/ 	NULL,
 /* 58 SPECIES_DOLLAR_STDF	*/	species_dollar_stdf,

 /* 59 PRIVATE_UNKNOWN_STDF 	*/	private_unknown_stdf,
 /* 60 PUBLIC_UNKNOWN_STDF 	*/   	public_unknown_stdf,
 /* 61 PROT_PRIV_UNKNOWN_STDF 	*/ 	protected_private_unknown_stdf,
 /* 62 PROT_PUB_UNKNOWN_STDF 	*/ 	protected_public_unknown_stdf,

 /* 63 BV_MASK_STDF 		*/ 	bv_mask,
 /* 64 BV_MIN_STDF 		*/      bv_min,
 /* 65 BV_MAX_STDF 		*/	bv_max,

 /* 66 WILLTRAP_STDF 		*/      will_trap_stdf,
 /* 67 FORCE_STDF 		*/	ast_idf,
 /* 68 PRIMTRACE_STDF 		*/	prim_trace,
 /* 69 CONT_NAME_STDF 		*/	continuation_name,
 /* 70 PROFILE_STDF 		*/	set_profile,
 /* 71 SET_STACK_LIMIT_STDF 	*/  	set_stack_limit,
 /* 72 GET_STACK_LIMIT_STDF 	*/  	get_stack_limit,
 /* 73 SET_HEAP_LIMIT_STDF 	*/   	set_heap_limit,
 /* 74 GET_HEAP_LIMIT_STDF 	*/   	get_heap_limit,
 /* 75 SHOW_ENV_STDF 		*/	show_env_stdf,
 /* 76 SHOW_CONFIG_STDF 	*/      show_config_stdf,
 /* 77 LOAD_PACKAGE_STDF 	*/	load_package_stdf,
 /* 78 EXCEPTION_STRING_STDF	*/	exception_string_stdf,
 /* 79 POSITION_STDF 		*/	position_stdf,
 /* 80 ACQUIRE_BOX_STDF 	*/	acquire_box_stdf,
 /* 81 SUPPRESS_COMPACTIFY_STDF */  	suppress_compactify_stdf,
 /* 82 GET_STACK_DEPTH_STDF	*/	get_stack_depth
};


PRIVATE ENTITY	(*binaries[N_BINARIES + 1])(ENTITY,ENTITY) =
{/* 0 				*/	NULL,
 /* 1 PLUS_STDF 		*/	ast_add,
 /* 2 MINUS_STDF 		*/	ast_subtract,
 /* 3 ABSDIFF_STDF 		*/	ast_absdiff,
 /* 4 TIMES_STDF 		*/	ast_mult,
 /* 5 DIVIDE_STDF 		*/	ast_divide,
 /* 6 MAKE_RAT2_STDF 		*/	ast_make_rat,
 /* 7 TO_STRING2_STDF 		*/	ast_num_to_string,
 /* 8 DIV_STDF 			*/	ast_div,
 /* 9 MOD_STDF 			*/	ast_mod,
 /* 10 GCD_STDF 		*/	ast_gcd,
 /* 11 POW_STDF 		*/	ast_power,
 /* 12 				*/	NULL,
 /* 13 UPTO_STDF 		*/	ast_upto,
 /* 14 DOWNTO_STDF 		*/	ast_downto,

 /* 15 COMPARE_STDF 		*/ 	ast_compare,
 /* 16 LT_STDF 			*/	ast_lt,
 /* 17 LE_STDF 			*/	ast_le,
 /* 18 GT_STDF 			*/	ast_gt,
 /* 19 GE_STDF 			*/	ast_ge,
 /* 20 NE_STDF			*/	ast_ne,

 /* 21 BV_SETBITS_STDF		*/	bv_set_bits,
 /* 22 BV_AND_STDF 		*/	bv_and,
 /* 23 BV_OR_STDF 		*/	bv_or,
 /* 24 BV_XOR_STDF 		*/	bv_xor,
 /* 25 BV_SHL_STDF 		*/	bv_shl,
 /* 26 BV_SHR_STDF 		*/	bv_shr,
 /* 27 BV_FIELD_STDF		*/	bv_field,

 /* 28 RENAME_STDF 		*/	rename_stdf,
 /* 29 INPUT_STDF 		*/	ast_input_stdf,
 /* 30 FOPEN_STDF 		*/	ast_fopen_stdf,

 /* 31 ONASSIGN_STDF 		*/	on_assign_stdf
};


PRIVATE ENTITY	(*listones[N_LISTONES + 1])(ENTITY) =
{/* 0 				*/	NULL,
 /* 1 LENGTH_STDF 		*/	ast_length_stdf,
 /* 2 REVERSE_STDF 		*/	reverse_stdf,
 /* 3 PACK_STDF 		*/	ast_pack_stdf,
 /* 4 LAZY_LEFT_STDF 		*/	lazy_left_stdf,
 /* 5 LAZY_RIGHT_STDF 		*/      lazy_right_stdf,
 /* 6 LAZY_HEAD_STDF 		*/      lazy_head_stdf,
 /* 7 LAZY_TAIL_STDF 		*/  	lazy_tail_stdf,
 /* 8 INTERN_STRING_STDF	*/	intern_string_stdf
};


PRIVATE ENTITY	(*listtwos[N_LISTTWOS + 1])(ENTITY, ENTITY) =
{/* 0 				*/	NULL,
 /* 1 APPEND_STDF 		*/	tree_append,
 /* 2 DOCMD_STDF 		*/	do_cmd_stdf,
 /* 3 SCAN_FOR_STDF 		*/	scan_for_stdf
};


PRIVATE ENTITY (*ty_stdfs[N_TY_STDFS + 1])(ENTITY, TYPE *) =
{/* 0 				*/	NULL,
 /* 1 BOX_TO_STR_STDF 		*/	boxpl_to_str_stdf,
 /* 4 WRAP_STDF			*/	domain_wrap,
 /* 5 WRAP_NUM_STDF		*/	wrap_number,
 /* 6 COMPAT_STDF		*/	compat_stdf
};


PRIVATE ENTITY (*unk_stdfs[N_UNK_STDFS + 1])(ENTITY) =
{/* 0 				*/      NULL,
 /* 1 UNKNOWNQ_STDF 		*/	unknownq_stdf,
 /* 2 PROTECTED_UNKNOWNQ_STDF	*/	protected_unknownq_stdf,
 /* 3 UNPROTECTED_UNKNOWNQ_STDF */ 	unprotected_unknownq_stdf,
 /* 4 SAME_UNKNOWN_STDF 	*/	same_unknown_stdf
};


PRIVATE void do_end_atomic(LONG *l_time, Boolean *no_backup);

/**********************************************************
 *			EVALUATE			  *
 **********************************************************
 * Function evaluate evaluates the expression described   *
 * by activation the_act.  Variable time_bound is used as *
 * a down-counter to determine when, if ever, to time-out.*
 **********************************************************/

void evaluate(LONG *time_bound)
{
  Boolean force_timeout = FALSE,    /* Used to indicate that a timeout should
				       occur, but could not be done because
				       something prevented it. */

          no_backup = FALSE;	   /* When most instructions time out, they
				      have their state restored, and are 
				      executed again when the timed-out
				      thread is resumed.  An instruction
				      sets no_backup to prevent that, saying
				      that it has finished in spite of the
				      timeout. */

  UBYTE inst;		          /* inst is the current instruction */

  CODE_PTR this_instr_pc;	  /* Where instruction inst is located. */

  CODE_PTR qt_goto = NULL;	  /* See next comment. */

  int qt_env_numentries = 0, 
      qt_stack_depth = 0;	  /* Quick tries are equivalent to ordinary
				     tries, but are only used when the
				     tested expression does no applications
				     or other complicated things.  To manage
				     quick tries, the evaluator must keep
				     track of how many values have been
				     pushed on the stack since the quick try
				     started, how many environment entries
				     there were when the quick try started,
				     and where to go at a failure, so that
				     failure can be executed. */
				     
  LONG l_time;			 /* A replacement for *time_bound, for a
				    little more efficiency. */

  ENTITY s, ss;	 	 	 /* Temporaries */

  REG_TYPE mark = reg2(&s, &ss);  /* Register the temporaries with the */
  				  /* garbage collector.                */

  /*---------------------------------------------------------*
   * Prepare the_act for evaluation, and initialize globals. *
   *---------------------------------------------------------*/

  failure           = -1;
  failure_as_entity = NOTHING;
  last_exception    = NOTHING;
  l_time            = *time_bound;
  start_new_activation();

  /*-----------------------------------------------------*
   * Indicate that the_act is to be stored (in fail_act) *
   * when there is a failure, so that it is possible to  *
   * see where the failure happened. 			 *
   *-----------------------------------------------------*/

  store_fail_act = TRUE;       

# ifdef DEBUG
    if(trace) {
      trace_i(69);
      trace_i(70, the_act.program_ctr, failure);
    }
# endif

  /*---------------------------------------------------------*
   * Function start_new_activation can set failure =	     *
   * TERMINATE_EX.  Check that now. 			     *
   *							     *
   * If we fail right away, then restart with what the 	     *
   * control says to do or exit, as the case may be.	     *
   *---------------------------------------------------------*/

  if(failure >= 0) {
    inst = 0;
    this_instr_pc = the_act.program_ctr; 
    goto fail;  /* bottom of main switch */
  }

  /*------------------------------------------------------------*
   * During evaluation, the_act.type_binding_lists holds	*
   * bindings of type variables.  Indicate that they should	*
   * be consulted when processing types.			*
   *------------------------------------------------------------*/

  use_the_act_bindings++;

  /*----------------------*
   * Main evaluation loop *
   *----------------------*/

  for(;;){

#   ifdef GCTEST
      if(the_act.stack == NULL || the_act.stack->ref_cnt != 1) {
	if(the_act.stack != NULL) {
	  trace_i(72, toint(the_act.stack->ref_cnt));
	}
	die(63);
      }
#   endif

    /*----------------------------------------------------------*
     * 			IMPORTANT NOTE				*
     *								*
     * Part of this initialization code is duplicated below	*
     * under TYPE_INSTRUCTIONS.  If a change is made here, it	*
     * might also need to be made there.			*
     *----------------------------------------------------------*/

    /*------------------------------------------*
     * Get the current instruction address, and *
     * increment the_act.program_ctr.		*
     *						*
     * If the_act.program_ctr == NULL, then it 	*
     * is time to exit. 			*
     *------------------------------------------*/

    this_instr_pc = the_act.program_ctr++;
    if(this_instr_pc == NULL) {
     out:

#     ifdef DEBUG
	if(trace) {
	  trace_i(71, failure);
	  trace_i(69);
	}
#     endif

      *time_bound = l_time;
      unreg(mark);
      the_act.program_ctr = NULL;
      use_the_act_bindings--;
      return;
    }

    /*-------------------------------------*
     * Get the instruction to be executed. *
     *-------------------------------------*/

    inst = *this_instr_pc;

#   ifdef DEBUG
      if(trace) trace_eval(qt_goto, qt_stack_depth, l_time, inst);
#   endif

 perform_instruction:

#   ifdef MSWIN
      /*------------------------------------------------------------*
       * Handle messages, such as attempt to drag down menus.  This *
       * also lets other processes have a chance to run.  Only	    *
       * do this every so often, to avoid really slowing down	    *
       * the interpreter.                                           *
       *------------------------------------------------------------*/

      if(handlePendingCount-- <= 0) {
	handlePendingMessages();
	handlePendingCount = HANDLE_PENDING_INTERVAL;
      }
#   endif

    /*------------------------------------------------------*
     * Check for a special condition, such as an interrupt. *
     *------------------------------------------------------*/

    if(special_condition) {

      /*----------------------*
       * Handle an interrupt. *
       *----------------------*/

      if(interrupt_occurred) {
	handle_interrupt();
	if(failure >= 0) goto fail;  /* end of main switch */
      }

      /*----------------------------------------------------*
       * Handle a pause request by blocking until unpaused. *
       *----------------------------------------------------*/

#     ifdef MSWIN
	if(should_pause) {
	  if(failure < 0) {
	    flush_console_buff();
	    failure = PAUSE_EX;
	    setConsoleTitle("Paused");
	    while(failure == PAUSE_EX) {
	      handlePendingMessages();
	      if(interrupt_occurred) {
		handle_interrupt();
		should_pause = 0;
		goto fail;
              }
	    }
	    should_pause = 0;
	    setConsoleTitle(main_program_name);
	    special_condition--;
	  }
	}
#     endif

      /*---------------------------------------*
       * Garbage collect, if it is called for. *
       *---------------------------------------*/

      if(perform_gc) {
	gc(printing_rts ? 0 : 1);
	perform_gc = 0;
	special_condition--;
      }

      /*-------------------------------------------------------------*
       * Install information in the execution profile if called for. *
       *-------------------------------------------------------------*/

      if(do_profile) install_profile_instruction();
    }

    /*------------------------------------------------------------------*
     * Note: in the descriptions of these instructions, s is used to	*
     * mean what is on top of the stack, and ss is what is immediately	*
     * under s.	By the local environment, we mean the ENVIRONMENT	*
     * structure pointed to directly by the_act.env.  By the global	*
     * environment, we mean the ENVIRONMENT structure at the end of the	*
     * chain pointed to by the_act.env.  By the outer environment, we	*
     * mean the environment described by array outer_bindings.  File	*
     * evaluate/typeinst.c defines a type stack and a type environment.	*
     * They are manipulated by some instructions.			*
     *									*
     * Many instructions have parameters.  Parameters can be one byte  	*
     * long or 3 bytes long.  						*
     *------------------------------------------------------------------*/

    switch(inst) {
      case UNARY_PREF_I:

	/*-------------------------------------------------------------*
	 * Execute unary instruction k, where k is the byte parameter. *
	 * The parameter to the instruction is s, fully evaluated.     *
	 * The parameter is popped, and the result of the instruction  *
	 * is pushed onto the stack.				       *
	 *-------------------------------------------------------------*/

	{register ENTITY* sp = top_stack();
	 register int k;

  	 IN_PLACE_FULL_EVAL_FAILTO(*sp, &l_time, fail_unary_pref_i); 
  	 if((k = next_byte) > N_UNARIES) die(71);
	 *sp = (unaries[k])(*sp);

       fail_unary_pref_i:
  	 break;
        }

      case BINARY_PREF_I:

	/*--------------------------------------------------------------*
	 * Execute binary instruction k, where k is the byte parameter. *
	 * The parameters to the instruction are ss and s, both fully   *
	 * evaluated.     						*
	 * The parameters are popped, and the result of the instruction *
	 * is pushed onto the stack.				        *
	 *--------------------------------------------------------------*/

	{register ENTITY* ssp;
	 register int k;

	 s   = POP_STACK();
  	 ssp = top_stack();
  	 IN_PLACE_FULL_EVAL_FAILTO(*ssp, &l_time, fail_binary_pref_i);
  	 IN_PLACE_FULL_EVAL_FAILTO(s, &l_time, fail_binary_pref_i2);
  	 k  = next_byte;
	 if(k > N_BINARIES) die(71);
	 *ssp = (binaries[k])(*ssp, s);
	 qt_stack_depth--;
	 break;

       fail_binary_pref_i2:
	 /*---------------------------------------------------*
	  * At a time-out while evaluating s, we alter the    *
	  * stack contents to prevent re-evaluation of *ssp.  *
	  *---------------------------------------------------*/

	 if(failure == TIME_OUT_EX && MEMBER(TAG(*ssp), full_eval_tags)) {
	   *ssp = make_lazy_prim(FULL_EVAL_TMO, *ssp, zero);
	 }

       fail_binary_pref_i:
	 *push_stack() = s;
	 break;
        }

      case LIST1_PREF_I:

	/*-------------------------------------------------------------*
	 * Execute list1 instruction k, where k is the byte parameter. *
	 * The parameter to the instruction is s, unevaluated.         *
	 * The parameter is popped, and the result of the instruction  *
	 * is pushed onto the stack.  The list1 instructions must      *
	 * return a viable (but lazy) result even when they timeout,   *
	 * so don't redo this instruction at a timeout.		       *
	 *-------------------------------------------------------------*/

	{register ENTITY* sp = top_stack();
	 register int k;

	 if((k = next_byte) > N_LISTONES) die(71);
	 *sp = (listones[k])(*sp);
         if(failure == TIME_OUT_EX) no_backup = TRUE;
	 break;
	}
	
      case LIST2_PREF_I:

	/*-------------------------------------------------------------*
	 * Execute list2 instruction k, where k is the byte parameter. *
	 * The parameters to the instruction are ss and s, both        *
	 * unevaluated.     					       *
	 * The parameters are popped, and the result of the instruction*
	 * is pushed onto the stack.  The list2 instructions must      *
	 * return a viable (but lazy) result even when they time out,  *
	 * so don't redo this instruction at a timeout.		       *
	 *-------------------------------------------------------------*/

	{register ENTITY* ssp;
	 register int k;

	 s   = POP_STACK();
	 ssp = top_stack();
	 if((k = next_byte) > N_LISTTWOS) die(71);
	 *ssp = (listtwos[k])(*ssp, s);
         if(failure >= 0) {
           if(failure == TIME_OUT_EX) no_backup = TRUE;
           else *push_stack() = s;
         }
	 else qt_stack_depth--;
	 break;
	}

      case TY_PREF_I:

	/*--------------------------------------------------------------*
	 * Execute ty-instruction k, where k is the byte parameter.  	*
	 * The second byte parameter is an offset in the global      	*
	 * environment of a type T.  The parameters to this instruction *
	 * are s and T.  s is popped, and the result of the instruction *
	 * is pushed onto the stack.					*
	 *--------------------------------------------------------------*/

	{register TYPE *t;
	 register ENTITY* sp = top_stack();
	 register int k;
	 IN_PLACE_EVAL_FAILTO(*sp, &l_time, fail_ty_pref_i);
	 if((k = next_byte) > N_TY_STDFS) die(71);
	 t = type_env(the_act.env, next_byte);
	 *sp = (ty_stdfs[k])(*sp, t);

       fail_ty_pref_i:
	 break;
	}

      case UNK_PREF_I:

	/*-----------------------------------------------------------*
	 * Execute unk-instruction k, where k is the byte parameter. *
	 * The parameter is s, evaluated up to any unknown.  Pop s,  *
	 * and push the result of the instruction.		     *
	 *							     *
	 * (Normally, encountering an unknown would cause the thread *
	 * to pause.  These instructions do not pause at an unknown.)*
	 *-----------------------------------------------------------*/

	{register ENTITY* sp = top_stack();
	 register int k;
	 IN_PLACE_EVAL_TO_ANY_UNKNOWN_FAILTO(*sp, &l_time, fail_unk_pref_i);
	 if((k = next_byte) > N_UNK_STDFS) die(71);
	 *sp = (unk_stdfs[k])(*sp);

       fail_unk_pref_i:
	 break;
	}

      case DUP_I:

	/*-----------------------------------*
	 * Push what is on top of the stack. *
	 *-----------------------------------*/

	{register ENTITY* sp = top_stack();
	 *(PUSH_STACK()) = *sp;
	 qt_stack_depth++;
	 goto next_instr_without_timeout;	/* Can't fail. 		*
						 * Don't need to count. */
	}

      case POP_I:

	/*----------------*
	 * Pop the stack. *
	 *----------------*/

	POP_STACK();
	qt_stack_depth--;
	goto next_instr_without_timeout;	/* Can't fail. 		*
						 * Don't need to count. */

      case SWAP_I:

	/*---------------------------------------*
	 * Swap the top two things on the stack. *
	 *---------------------------------------*/

	{register ENTITY* ssp;
	 s   = POP_STACK();
	 ssp = top_stack();
	 *(PUSH_STACK()) = *ssp;
	 *ssp = s;
	 goto next_instr_without_timeout;	/* Can't fail.          *
						 * Don't need to count. */
	}

      case LOCK_TEMP_I:

	/*---------------------------------------------------------*
	 * Enter atomic mode and pop the stack into eval_temp_ent. *
	 *---------------------------------------------------------*/

	the_act.in_atomic++;
	eval_temp_ent = POP_STACK();
	qt_stack_depth--;
	goto next_instr_without_timeout;	/* Can't fail.          *
						 * Don't need to count. */

      case GET_TEMP_I:

	/*---------------------------------------------------------------*
	 * Push eval_temp_ent back onto the stack, and exit atomic mode. *
	 *---------------------------------------------------------------*/

        *(PUSH_STACK()) = eval_temp_ent;
	qt_stack_depth++;
	do_end_atomic(&l_time, &no_backup);
	break;

      case EMPTY_BOX_I:

	/*-----------------------------------*
	 * Test whether s is an empty box.   *
	 * Push true if it is, false if not. *
	 *-----------------------------------*/

	{register ENTITY* sp = top_stack();
	 IN_PLACE_EVAL_FAILTO(*sp, &l_time, fail_empty_box_i);
	 *sp = ENTU(is_empty_bxpl(*sp));
	}

      fail_empty_box_i:
	break;

      case BOX_I:

	/*---------------------------------*
	 * Push a new empty nonshared box. *
	 *---------------------------------*/

	*PUSH_STACK() = ast_new_box();
	qt_stack_depth++;
	goto next_instr;  	/* Can't fail. */

      case PLACE_I:

	/*------------------------------*
	 * Push a new empty shared box. *
	 *------------------------------*/

	*PUSH_STACK() = ast_new_place();
	qt_stack_depth++;
	goto next_instr;  	/* Can't fail. */

      case ASSIGN_I:
      case ASSIGN_INIT_I:
      case ASSIGN_NODEMON_I:

	/*-----------------------------------------------------------*
	 * Assign ss := s.  ASSIGN_INIT_I does the assignment in the *
	 * initial state.  ASSIGN_NODEMON_I avoids demon processing. *
	 *-----------------------------------------------------------*/

	if(break_info.break_assigns && !break_info.suppress_breaks 
	   && breakable()) {
	  break_info.pc = this_instr_pc;
	  vis_break(ASSIGN_VIS);
	}

	s  = POP_STACK();
	ss = POP_STACK();
	IN_PLACE_EVAL_FAILTO(ss, &l_time, fail_assign_i);
	if(inst == ASSIGN_I || inst == ASSIGN_NODEMON_I) {
	  ENTITY demon, oldcontent;
	  assign_bxpl(ss, s, &demon, &oldcontent);
	  if(ASSIGN_I == inst && TAG(demon) != NOREF_TAG) {
	    process_demon(demon, oldcontent, s);
	  }
	}
	else assign_init_bxpl(ss, s);
	qt_stack_depth -= 2;
	break;

      fail_assign_i:
	/*-------------------------------*
	 * Restore for possible timeout. *
	 *-------------------------------*/

	*push_stack() = ss;
	*push_stack() = s;
        break;

      case MAKE_EMPTY_I:
      case MAKE_EMPTY_NODEMON_I:

        /*------------------------------------------------------*
         * Pop s, which must be a box, and make s empty. 	*
         * MAKE_EMPTY_NODEMON_I does not do any demon		*
	 * processing,  MAKE_EMPTY_I only does the demon at s	*
	 * if s was not already empty.				*
         *------------------------------------------------------*/

        {ENTITY demon, oldcontent;
         s = pop_stack();
	 IN_PLACE_EVAL_FAILTO(s, &l_time, fail_make_empty_nodemon_i);
	 if(!is_empty_bxpl(s)) {
	   make_bxpl_empty(s, &demon, &oldcontent);
	   if(inst == MAKE_EMPTY_I && TAG(demon) != NOREF_TAG) {
	     process_demon(demon, oldcontent, NOTHING);
	   }
	 }
	 qt_stack_depth--;
         break;

       fail_make_empty_nodemon_i:
         *push_stack() = s;
	 break;
	}

      case SMALL_INT_I:

	/*-----------------------------------------------*
	 * Push the integer given by the byte parameter. *
	 *-----------------------------------------------*/

	*PUSH_STACK() = ENTI(next_byte);
	qt_stack_depth++;
	goto next_instr;  	/* Can't fail. */

      case CONST_I:

	/*---------------------------------------------*
	 * Push constants[k], where k is the parameter *
	 *---------------------------------------------*/

	{LONG k = next_three_bytes;
	 *PUSH_STACK() = constants[k];
	 qt_stack_depth++;
	 goto next_instr;  	/* Can't fail. */
	}

      case ZERO_I:

	/*---------*
	 * Push 0. *
	 *---------*/

	*(PUSH_STACK()) = zero;
	qt_stack_depth++;
	goto next_instr;  	/* Can't fail. */

      case TRUE_I:

	/*------------*
	 * Push true. *
	 *------------*/

	*PUSH_STACK() = true_ent;
	qt_stack_depth++;
	goto next_instr;  	/* Can't fail. */

      case STD_BOX_I:

	/*------------------------------------------------*
	 * Push standard box k, where k is the parameter. *
	 *------------------------------------------------*/

	*PUSH_STACK() = ENTB(next_byte);
	qt_stack_depth++;
	goto next_instr;  	/* Can't fail. */

      case INT_DIVIDE_I:

	/*-------------------------------------*
	 * Pop s and ss, and push divide(ss,s) *
	 *-------------------------------------*/

	{ENTITY* ssp;
	 s   = POP_STACK();
	 ssp = top_stack();
	 IN_PLACE_EVAL_FAILTO(*ssp, &l_time, fail_int_divide_i); 
	 IN_PLACE_EVAL_FAILTO(s, &l_time, fail_int_divide_i);
	 if(ENT_EQ(s, zero)) {
	   failure = DOMAIN_EX;
	   failure_as_entity = divide_by_zero_ex;
	 }
	 else {
	   ast_divide_int(*ssp, s, &ss, &s);
	   *ssp = ast_pair(ss, s);
	 }
	 qt_stack_depth--;
         break;

       fail_int_divide_i:
	 *push_stack() = s;
	 break;
	}

      case AS_I:

	/*------------------------------------------------------------*
	 * Pop s and ss.  Push ss as s -- the result of forcing ss to *
	 * have the type of s.	The parameter (k) tells where to look *
	 * for the type of s. 					      *
	 *------------------------------------------------------------*/

	{register TYPE *t;
	 register int k;
	 register ENTITY* ssp;

	 s   = pop_stack();
	 ssp = top_stack();
	 IN_PLACE_EVAL_FAILTO(*ssp, &l_time, fail_as_i);
	 IN_PLACE_EVAL_FAILTO(s, &l_time, fail_as_i);
	 k  = next_byte;
	 t  = type_env(the_act.env, k)->TY2;
	 *ssp = ast_as(*ssp, s, t);
	 qt_stack_depth--;
         break;

       fail_as_i:
	 *push_stack() = s;
	 break;
	}

      case NOT_I:

	/*------------------------*
	 * Pop s and push not(s). *
	 *------------------------*/

	{register ENTITY* sp = top_stack();
	 IN_PLACE_EVAL_FAILTO(*sp, &l_time, fail_not_i); 
	 *sp = NOT(*sp);

       fail_not_i:
	 break;
	}

      case EQ_I:

	/*------------------------------------------------------------*
	 * Pop s and ss.  Push ss == s. This only works for certain   *
	 * built-in types, where it compares representations.  See    *
	 * ast_equal in rts/compare.c. Even when ast_equal times out, *
	 * it returns a result that is a lazy value whose evaluation  *
	 * yields the desired value, so don't redo this instruction   *
	 * at a timeout.					      *
	 *------------------------------------------------------------*/

	{register ENTITY* ssp;
	 s    = POP_STACK();
	 ssp  = top_stack();
	 *ssp = ast_equal(*ssp, s, &l_time);
	 if(failure == TIME_OUT_EX) no_backup = TRUE;
	 qt_stack_depth--;
	 break;
	}

      case ENUM_CHECK_I:
      case LONG_ENUM_CHECK_I:

	/*-------------------------------------------------------------------*
	 * Check that s is nonnegative and is less than the parameter.  If   *
	 * so, then do nothing, leaving the stack unchanged.  If not, then   *
	 * set failure = CONVERSION_EX.					     *
	 *								     *
	 * The parameter is a byte for ENUM_CHECK_I, and is a long parameter *
	 * for a LONG_ENUM_CHECK_I instruction.  			     *
	 *-------------------------------------------------------------------*/

	{register ENTITY* sp = top_stack();
	 register LONG k, jump;

	 IN_PLACE_EVAL_FAILTO(*sp, &l_time, fail_enum_check_i); 
	 k = get_ival(*sp, CONVERSION_EX);
	 jump = (inst == ENUM_CHECK_I) ? next_byte : next_three_bytes;
	 if(k >= jump || jump < 0) {
	   failure = CONVERSION_EX;
	 }

       fail_enum_check_i:
	 break;
	}

      case TEST_I:

	/*---------------------------------------------------------------*
	 * Pop s.  If s is true, then do nothing.  If s is false, then	 *
	 * fail with the simple exception given by the next byte.	 *
	 *---------------------------------------------------------------*/

	{register int k = next_byte;
	 s = POP_STACK();
	 IN_PLACE_EVAL_FAILTO(s, &l_time, fail_test_i);
	 qt_stack_depth--;
	 if(VAL(s) == 0) {
	   failure = k;
	 }
	 break;

       fail_test_i:
	 *PUSH_STACK() = s;
	 break;
	}

      case NILQ_I:

	/*----------------------------------------------------------*
	 * Pop s, and push true is s is nil, false if s is not nil. *
	 *----------------------------------------------------------*/

	{register ENTITY* sp = top_stack();
	 IN_PLACE_EVAL_FAILTO(*sp, &l_time, fail_nilq_i);
	 *sp = (ENT_EQ(*sp, nil)) ? true_ent : false_ent;

       fail_nilq_i:
 	 break;
	}

      case NIL_FORCE_I:

	/*-------------------------------------------------------------*
	 * Like NILQ_I, but if s is an unprotected unknown, then force *
	 * s to be nil, and push true.				       *
	 *-------------------------------------------------------------*/

	{register ENTITY* sp = top_stack();
	 IN_PLACE_EVAL_TO_UNPROT_UNKNOWN_FAILTO(*sp,&l_time,fail_nil_force_i);
	 if(is_unprot_unknown(*sp)) {
	   bind_unknown(*sp, nil, 0, &l_time);
	   if(failure < 0) *sp = true_ent;
	 }
	 else *sp = (ENT_EQ(*sp, nil)) ? true_ent : false_ent;

       fail_nil_force_i:
	 break;
	}

      case PAIR_I:

	/*--------------------------------*
	 * Pop s and ss, and push (ss,s). *
	 *--------------------------------*/

	{register ENTITY* ssp;
	 s    = POP_STACK();
	 ssp  = top_stack();
	 *ssp = ast_pair(*ssp, s);
	 qt_stack_depth--;
	 goto next_instr;  	/* Can't fail. */
	}

      case MULTIPAIR_I:

	/*--------------------------------------------------------*
	 * Perform k PAIR_I instructions in a row, where k is the *
	 * parameter.						  *
	 *--------------------------------------------------------*/

	{register int k = next_byte;
	 multi_pair_pr(k);
	 qt_stack_depth -= k;
	 goto next_instr;  	/* Can't fail. */
	}

      case HEAD_I:

	/*---------------------------------------------------------------*
	 * Pop s and push head(s).  We expand ast_head inline for speed. *
	 *---------------------------------------------------------------*/

	{register ENTITY* sp = top_stack();
	 ENTITY t;
	 IN_PLACE_EVAL_FAILTO(*sp, &l_time, fail_head_i);
	 ast_split1(*sp, sp, &t, 1);

       fail_head_i:
	 break;
        }

      case TAIL_I:

	/*---------------------------------------------------------------*
	 * Pop s and push tail(s).  We expand ast_tail inline for speed. *
	 *---------------------------------------------------------------*/

	{register ENTITY* sp = top_stack();
	 ENTITY h;
	 IN_PLACE_EVAL_FAILTO(*sp, &l_time, fail_tail_i);
         ast_split1(*sp, &h, sp, 2);

       fail_tail_i:
	 break;
        }

      case LEFT_I:
      case RIGHT_I:

	/*-------------------------------------------------------------------*
	 * These instructions are similar to HEAD_I and TAIL_I, respectively,*
	 * but if s is an unprotected unknown they force s to be a pair.     *
	 *-------------------------------------------------------------------*/

	{register ENTITY* sp = top_stack();
	 ENTITY x;

	 IN_PLACE_EVAL_TO_UNPROT_UNKNOWN_FAILTO(*sp, &l_time, fail_left_i);
	 s = *sp;
	 if(is_unprot_unknown(s)) s = split_unknown(s);
	 if(inst == LEFT_I) {
	   ast_split1(s, sp, &x, 1);
	 }
	 else {
	   ast_split1(s, &x, sp, 2);
	 }

       fail_left_i:
	 break;
	}

      case MULTI_RIGHT_I:

	/*------------------------------------------------------*
	 * Perform k RIGHT_I instructions in a row, where k is  *
	 * the parameter.					*
	 *------------------------------------------------------*/

	{register ENTITY* sp = top_stack();
	 register int k;

	 s = *sp;
	 k = next_byte;
	 while(k > 0 && failure < 0) {
	   IN_PLACE_EVAL_TO_UNPROT_UNKNOWN_FAILTO
		 (s,&l_time,fail_multi_right_i);
	   if(failure < 0) {
	     if(is_unprot_unknown(s)) s = split_unknown(s);
	     ast_split1(s, &ss, &s, 2);
	     k--;
	   }
	 }

	 if(failure < 0) *sp = s;

       fail_multi_right_i:
	 break;
	}

      case SPLIT_I:
      case NONNIL_FORCE_I:

	/*--------------------------------------------------------------*
	 * SPLIT_I pops s, which must be an ordered pair (a,b) or 	*
	 * nonempty list a::b. It then pushes a and b.			*
 	 *								*
         * NONNIL_FORCE_I is similar to NONNIL_TEST_I.  Both SPLIT_I	*
	 * and NONNIL_FORCE_I, however, will force s to be a pair if s  *
 	 * is an unprotected unknown. 					*
	 *								*
	 * If s is nil, then these instructions fail with exception	*
	 * TEST_EX.							*
	 *--------------------------------------------------------------*/

	{register ENTITY* sp = top_stack();
	 IN_PLACE_EVAL_TO_UNPROT_UNKNOWN_FAILTO(*sp, &l_time, fail_split_i);
	 s = *sp;
	 if(is_unprot_unknown(s)) s = split_unknown(s);
	 if(IS_NIL(s)) failure = TEST_EX;
	 else if(inst == SPLIT_I) {
	   ast_split(s, sp, &ss);
	   *PUSH_STACK() = ss;
	   qt_stack_depth++;
	 }

       fail_split_i:
	 break;
	}

      case NONNIL_TEST_I:

	/*--------------------------------------------------*
	 * Pop s.  Fail with exception TEST_EX if s is nil. *
	 * Otherwise, do nothing. 			    *
	 *--------------------------------------------------*/

	{register ENTITY* sp = top_stack();
	 IN_PLACE_EVAL_FAILTO(*sp, &l_time, fail_nonnil_test_i);
	 if(ENT_EQ(*sp, nil)) failure = TEST_EX;

       fail_nonnil_test_i:
	 break;
	}

      case SUBLIST_I:
      case SUBSCRIPT_I:

	/*---------------------------------------*
	 * Pop s and ss, and push ss#s or ss##s, *
	 * depending on the instruction 	 *
	 *---------------------------------------*/

	{register ENTITY* ssp;
	 s = POP_STACK();
	 IN_PLACE_FULL_EVAL_FAILTO(s, &l_time, fail_sublist_i);
	 ssp = top_stack();
	 *ssp = (inst == SUBSCRIPT_I) 
	             ? ast_subscript1(*ssp, s, &l_time)
	             : ast_sublist(*ssp, s);
	 no_backup = TRUE;
	 qt_stack_depth--;
         break;

       fail_sublist_i:
	 *push_stack() = s;
	 break;
	}

      case FUNCTION_I:

	/*--------------------------------------------------------------*
	 * Push a function onto the stack.  The function begins at the  *
	 * current address, beginning there with an environment size	*
	 * byte.  Resume this thread at address jump from the current	*
	 * instruction.         					*
	 *--------------------------------------------------------------*/

	{register CONTINUATION  *cont;
	 register CODE_PTR      next_pc;
	 register int		k;

	 next_pc  = this_instr_pc + next_three_bytes;
	 k        = toint(next_three_bytes);  /* lazy type instr index */

	/*--------------------------------------------------------------*
	 * If this FUNCTION_I instruction is followed by a RETURN_I,	*
	 * and the instruction returned to is a REV_APPLY_I 		*
	 * instruction, or a related instruction (REV_TAIL_APPLY_I, 	*
	 * etc.) then those instructions can all be done		*
	 * at once by altering the_act to enter this new		*
	 * function.							*
	 *--------------------------------------------------------------*/

	 cont = the_act.continuation;
	 if(cont != NULL && *next_pc == RETURN_I) {
	   register CODE_PTR*  cont_pc    = &(cont->program_ctr);
	   register UBYTE      cont_instr = **cont_pc;
	   if(cont_instr >= REV_APPLY_I && 
	      cont_instr <= REV_PAIR_TAIL_APPLY_I) {
	     do_function_return_apply(cont_pc, k);
	     goto next_instr;  	                  /* Can't fail. */
	   }
	 }

         /*-----------------------------------------------------*
	  * If this is not the above special case, then handle	*
	  * by building the function.				*
	  *-----------------------------------------------------*/

	 cont 		      = allocate_continuation(); /* Ref from alloc */
	 bump_env(cont->env   = the_act.env, the_act.num_entries);
         bump_list(cont->type_binding_lists = the_act.type_binding_lists);
	 cont->name           = the_act.name;
	 cont->num_entries    = the_act.num_entries;
	 cont->program_ctr    = the_act.program_ctr;
	 cont->exception_list = NIL;
	 cont->result_type_instrs = lazy_type_instrs[k];
	 cont->continuation   = fun_conts;
	 fun_conts            = cont;
	 *push_stack()        = ENTP(FUNCTION_TAG, cont);
	                           /* Inherits ref from cont */
	 the_act.program_ctr  = next_pc;
	 goto next_instr;  	/* Can't fail. */
        }

      case REV_APPLY_I:

	/*------------------------------------------------------*
	 * Apply s to ss, where s is a function. 		*
	 * The following is rev_apply_i, inlined.		*
	 *------------------------------------------------------*/

	{register ENTITY* sp = top_stack(); /* the function */
	 IN_PLACE_EVAL_FAILTO(*sp, &l_time, fail_rev_apply_i); 
	 s = POP_STACK();
	 if(TAG(s) != FUNCTION_TAG) dump_die(78);
	 do_apply(CONTVAL(s), 0);

       fail_rev_apply_i:
	 break;
	}

      case REV_TAIL_APPLY_I:

	/*------------------------------------------------------------*
	 * Apply s to ss, where s is a function.  This is a tail      *
	 * application, so we can reuse the current activations parts *
	 *------------------------------------------------------------*/
	 
	if(do_tro) {  

	  /*----------------------------------------*
	   * rev_tail_apply_i(0, &l_time), inlined. *
	   *----------------------------------------*/

          register ENTITY* sp = top_stack();
	  IN_PLACE_EVAL_FAILTO(*sp, &l_time, fail_rev_tail_apply_i);
	  s = POP_STACK();
	  if(TAG(s) != FUNCTION_TAG) die(78);
	  do_tail_apply(CONTVAL(s), 0);
        }

	else rev_apply_i(0, &l_time);

      fail_rev_tail_apply_i:
	break;

      case REV_PAIR_APPLY_I:

	/*-------------------------------------------------------------------*
	 * This is similar to REV_APPLY_I, but it does k PAIR_I instructions *
	 * before doing the REV_APPLY_I, where k is the byte parameter to    *
	 * this instruction.						     *
	 *-------------------------------------------------------------------*/

	rev_apply_i(next_byte, &l_time);
	break;

      case REV_PAIR_TAIL_APPLY_I:

	/*---------------------------------------------------*
	 * Similar to REV_PAIR_APPLY+_I, but tail recursive. *
	 *---------------------------------------------------*/

	if(do_tro) rev_tail_apply_i(next_byte, &l_time);
	else rev_apply_i(next_byte, &l_time);
	break;

      case QEQ_APPLY_I:

	/*---------------------------------------------------*
	 * See apply.c for the discussion under qeq_apply_i. *
	 *---------------------------------------------------*/

	qeq_apply_i(&l_time);
	break;

      case SHORT_APPLY_I:

	/*---------------------------------------*
	 * Apply s to ss, but with time limited. *
	 *---------------------------------------*/

	{register ENTITY* ssp;
	 s    = pop_stack();
	 ssp  = top_stack();
	 *ssp = short_apply(*ssp, s, &l_time);
	 break;
	}

      case RETURN_I:
      case INVISIBLE_RETURN_I:

	/*--------------------------------------------------------*
	 * Perform a function return. INVISIBLE_RETURN_I does not *
	 * cause a debug break. 				  *
	 *--------------------------------------------------------*/

	return_i(inst);
	break;

      case DYN_FETCH_GLOBAL_I:

	/*----------------------------------------------------------*
	 * Fetch the value of a global id.  The offset in	    *
         * outer_bindings is given by parameter kk, and the type    *
	 * is given by the global environment at offset nn.	    *
	 *----------------------------------------------------------*/

        {register LONG    kk = next_three_bytes;
	 register int     nn = next_byte;
	 register ENTITY* a  = allocate_entity(2);
	 a[1]    = dynamic_default_ent(global_binding_env(the_act.env, nn));
	 a[0]    = ENTG(kk);
	 *PUSH_STACK() = ENTP(GLOBAL_INDIRECT_TAG, a);
	 qt_stack_depth++;
	 goto next_instr;  	/* Can't fail. */
        }

      case G_FETCH_I:

	/*---------------------------------------------------------------*
	 * Fetch the entity at offset k in the global environment, where *
	 * k is the parameter.						 *
	 *---------------------------------------------------------------*/

	*PUSH_STACK() = global_binding_env(the_act.env, next_byte);
	qt_stack_depth++;
	goto next_instr;  	/* Can't fail. */

      case GLOBAL_ID_VAL_I:

	/*------------------------------------------------------------------*
	 * The parameter (k) is the offset in the global environment record *
	 * of type (String -> T).  Pop s, which must be a string, and push  *
	 * the value associated with s:T in the outer environment.          *
	 *								    *
	 * Type T must not contain any run-time bound variables.	    *
	 *								    *
	 * Only identifiers that are not local to the body of a package	    *
	 * can be fetched this way.					    *
	 *------------------------------------------------------------------*/

	{register TYPE *t;
	 char name[MAX_ID_SIZE+1];
	 ENTITY rest, *gid;
	 register ENTITY* sp;
	 register int k;

	 sp = top_stack();
	 IN_PLACE_FULL_EVAL_FAILTO(*sp, &l_time, fail_global_id_val_i); 
	 k = next_byte;
	 t = force_ground(find_u(type_env(the_act.env, k))->TY2);
	 copy_str(name, *sp, MAX_ID_SIZE, &rest);
	 if(!IS_NIL(rest) || is_package_local_name(name)) {
	   failure = GLOBAL_ID_EX;
	   failure_as_entity = qwrap(GLOBAL_ID_EX, *sp);
	 }
	 else {
	   LONG kk = ent_str_tb(name);
	   gid     = allocate_entity(2);
	   ss      = ENTP(GLOBAL_INDIRECT_TAG, gid);
	   gid[1]  = ENTP(TYPE_TAG, t);
	   gid[0]  = ENTG(kk);
	   SET_EVAL(*sp, ss, &l_time);
	 }

       fail_global_id_val_i:
	 break;
	}

      case FETCH_I:

	/*------------------------------------------------------*
	 * Fetch the environment entry at offset k, and push it *
	 * onto the stack.					*
	 *------------------------------------------------------*/

	{register int k = next_byte;
         *PUSH_STACK() = local_binding_env(the_act.env, k);
	 qt_stack_depth++;
	 goto next_instr;  	/* Can't fail. */
	}

      case FINAL_FETCH_I:

	/*------------------------------------------------------*
	 * Fetch the environment entry at offset k, and push it *
	 * onto the stack.  This environment cell will never be *
	 * fetched again, so set its content to NOTHING to free *
	 * the memory to which it refers, provided such		*
	 * behavior is not suppressed by do_tro being false.	*
	 *------------------------------------------------------*/

	{register int k = next_byte;
	 *PUSH_STACK() = local_binding_env(the_act.env, k);
	 if(do_tro) local_bind_env(k, NOTHING, this_instr_pc);
	 qt_stack_depth++;
	 goto next_instr;  	/* Can't fail. */
	}

      case LONG_FETCH_I:
      case FINAL_LONG_FETCH_I:

	/*----------------------------------------------------------*
	 * Fetch a value stored in a nonlocal environment.  k tells *
	 * how far from the current environment to look -- chain    *
	 * though k links.  The parameter after k is the offset     *
	 * within the environment.				    *
	 *----------------------------------------------------------*/

	{register int k = next_byte;
	 *PUSH_STACK() = gen_binding_env(the_act.env, next_byte, k);
	 qt_stack_depth++;
	 goto next_instr;  	/* Can't fail. */
        }

      case EXIT_SCOPE_I:

	/*--------------------------------------------------------------*
	 * Pop the local environment back until it has k bindings in 	*
	 * it, where k is the parameter. 				*
	 *--------------------------------------------------------------*/

	exit_scope_env(next_byte);
	goto next_instr;  	/* Can't fail. */

      case DEF_I:

	/*-----------------------------------------------------------*
	 * Do a define expression.  The expression that computes the*
	 * value to be put in the environment begins at the current  *
	 * address.  Build a lazy entity and store that in the 	     *
	 * environment.  jump tells where to continue after doing    *
 	 * define.						     *
         *							     *
         * Note: The lazy entity needs to refer to this same	     *
	 * environment, with itself in it.  So we need to put	     *
	 * something in the environment before building the lazy     *
	 * entity.						     *
	 *-----------------------------------------------------------*/

	{LONG jump     = next_three_bytes;
	 int k         = next_byte;
	 int nn        = toint(next_three_bytes);

	 local_bind_env(k, NOTHING, this_instr_pc);
	 local_bind_env(k, 
                        make_lazy(the_act.program_ctr, 1, NULL, 
				  lazy_type_instrs[nn], FALSE, k+1), 
			this_instr_pc);
	 the_act.program_ctr = this_instr_pc + jump;
	 goto next_instr;  	/* Can't fail. */
        }

      case TEAM_I:

	/*----------------------------------------------------------*
	 * Begin a team.  Perform the definitions in the team. This *
	 * is pretty ugly.					    *
	 *----------------------------------------------------------*/

	{int i, ndefs, nn, k;
	 LONG jump;
	 CODE_PTR ipc;

	 ndefs = next_byte;
         k     = next_byte;

         for(i = 0; i < ndefs; i++) {
	   if(*the_act.program_ctr != DEF_I) die(67);
	   ipc       = the_act.program_ctr++;
	   jump      = next_three_bytes;
	   k         = next_byte;
	   nn        = toint(next_three_bytes);
	   local_bind_env(k, NOTHING, this_instr_pc);
	   local_bind_env(k, 
			  make_lazy(the_act.program_ctr, 1, 
				    NULL, lazy_type_instrs[nn], FALSE, k+1), 
			  ipc);
	   the_act.program_ctr = ipc + jump;
         }
	 goto next_instr;  	/* Can't fail. */
        }   

      case LET_I:
      case DEAD_LET_I:

	/*-----------------------------------------------------------*
	 * Make an entry in the local environment.  The offset is k. *
	 * Pop s, and store it there.  A DEAD_LET_I is for a binding *
	 * that will never be fetched.  It must nonetheless be done  *
	 * so that the binding can be seen by the debugger,	     *
 	 * but only if tail-recursion improvement is turned off.     *
	 *-----------------------------------------------------------*/

	{register int k = next_byte;

	 if(break_info.break_lets && !break_info.suppress_breaks 
	    && breakable()) {
	   break_info.vis_let_offset = k;
	   break_info.pc = this_instr_pc;
	   vis_break(LET_VIS);
	 }

	 s = POP_STACK();
	 qt_stack_depth--;

	 /*-------------------------------------------------------*
	  * If this is a DEAD_LET_I and we are asked to optimize, *
	  * then there is no need to do anything.		 *
	  *-------------------------------------------------------*/
	 
	 if(inst == LET_I || !do_tro) {

	   /*----------------------------------------------------*
	    * Typically, we are asked to put the entry at the	*
	    * end of the environment.  That is very fast, so we	*
	    * do it as a special case.  (It would work to let	*
	    * local_bind_env do the job.)			*
	    *----------------------------------------------------*/

	   register ENVIRONMENT* env1 = the_act.env;
	   int most_ent = env1->most_entries;
	   if(k == most_ent) {
	     register struct envcell* cell = env1->cells + k;
	     drop_env_ref(env1, the_act.num_entries);
	     the_act.num_entries = env1->most_entries = k+1;
	     cell->refs = 1;
	     env1->pc   = this_instr_pc;
	     cell->val  = s;
	   }
	   else {
	     local_bind_env(k, s, this_instr_pc);
	   }
	 }
	 goto next_instr;  	/* Can't fail. */
        }

      case LET_AND_LEAVE_I:
      case FINAL_LET_AND_LEAVE_I:

	/*--------------------------------------------------------*
	 * Do a let, but don't pop s -- leave it on the stack. A  *
	 * FINAL_LET_AND_LEAVE_I indicates that this binding will *
	 * never be queried. 					  *
	 *--------------------------------------------------------*/

	{register int k = next_byte;

	 if(break_info.break_lets && !break_info.suppress_breaks 
	    && breakable()) {
	   break_info.vis_let_offset = k;
	   break_info.pc = this_instr_pc;
	   vis_break(LET_VIS);
	 }

	 if(inst == LET_AND_LEAVE_I || !do_tro) {
	   local_bind_env(k, *top_stack(), this_instr_pc);
	 }
	 goto next_instr;  	/* Can't fail. */
        }

      case RELET_I:
      case RELET_AND_LEAVE_I:
      case FINAL_RELET_AND_LEAVE_I:

        /*-----------------------------*
         * These should never be seen. *
         *-----------------------------*/

        goto bad_instruction;

      case GOTO_I:

	 /*----------------------------------------------------------------*
	  * Do an unconditional jump to the location whose offset from     *
	  * this instruction is given by the parameter of this instruction *
	  *----------------------------------------------------------------*/

	 {register LONG jump = next_three_bytes;
          the_act.program_ctr = this_instr_pc + jump;
	  goto next_instr;  	/* Can't fail. */
	 }

      case PAIR_GOTO_I:

	 /*-----------------------------------------------------------------*
	  * The parameters are jump and k.  Do k PAIR_I instructions, then  *
	  * jump to location this_instr_pc + jump.  This is done by looking *
	  * at the instructions at the new address.  If there are SPLIT_I   *
	  * instructions, then we can avoid doing PAIR_I SPLIT_I pairs, and *
	  * instead just decrease the number of PAIR_I instructions to do.  *
	  *-----------------------------------------------------------------*/

	 {register LONG jump = next_three_bytes;
	  register int  k    = next_byte;
	  this_instr_pc += jump;
	  while(k > 0 && *this_instr_pc == SPLIT_I) {
	    k--; 
	    this_instr_pc++;
	  }
	  if(k > 0) {
	    multi_pair_pr(k);
	    qt_stack_depth -= k;
	  }
	  the_act.program_ctr = this_instr_pc;
	  goto next_instr;  	/* Can't fail. */
	 }

      case GOTO_IF_FALSE_I:

	/*--------------------------------------------------------------*
	 * Pop s from the stack.  Jump to the address jump from the 	*
	 * current instruction if s is s is 0.				*
	 *--------------------------------------------------------------*/

	{register LONG jump;
	 s = POP_STACK();
	 IN_PLACE_EVAL_FAILTO(s, &l_time, fail_goto_if_false_i);
	 jump = next_three_bytes;
	 if(VAL(s) == 0) the_act.program_ctr = this_instr_pc + jump;
	 qt_stack_depth--;
         break;
        }

      fail_goto_if_false_i:
	*push_stack() = s;
	break;

      case GOTO_IF_NIL_I:

	/*------------------------------------------------------*
	 * Peek at s, but do not pop the stack.			*
	 * Jump to the address at offset jump from the present  *
	 * instruction if s is nil.  Otherwise, do nothing.	*
	 *------------------------------------------------------*/

	{register ENTITY* sp  = top_stack();
	 register LONG jump;

	 IN_PLACE_EVAL_FAILTO(*sp, &l_time, fail_goto_if_nil_i);
	 jump = next_three_bytes;
	 if(ENT_EQ(*sp, nil)) the_act.program_ctr = this_instr_pc + jump;

       fail_goto_if_nil_i:
	 break;
        }

      case AND_SKIP_I:
      case OR_SKIP_I:

	/*---------------------------------------------------------------*
	 * AND_SKIP_I: if s is false, leave s on the stack and go to the *
	 * address given by offset jump.  If s is true, then pop s and 	 *
	 * continue with the next instruction.				 *
         *								 *
	 * OR_SKIP_I: if s is true, leave s on the stack and go to the	 *
	 * address given by offset jump.  If s is false, then pop s and  *
	 * continue with the next instruction.				 *
	 *---------------------------------------------------------------*/

	{register ENTITY* sp = top_stack();
	 register LONG jump;

	 IN_PLACE_EVAL_FAILTO(*sp, &l_time, fail_and_skip_i); 
	 jump = next_three_bytes;
	 if(NRVAL(*sp) == inst - AND_SKIP_I) {
	   the_act.program_ctr = this_instr_pc + jump;
	 }
	 else {
	   POP_STACK();
	   qt_stack_depth--;
	 }
	 
       fail_and_skip_i:
	 break;
        }

      case LLABEL_I:
      case LONG_LLABEL_I:

        /*-----------------------------*
         * These should never be seen. *
         *-----------------------------*/

        goto bad_instruction;

      case TRY_I:

	/*--------------------------------------------------------------*
	 * Enter a try expression.  Parameter kind tells which  	*
	 * kind of try as follows.					*
	 *    								*
         *    TRY_F		try					*
	 *    TRYEACH_F		try catchingEachThread			*
	 *    TRYTERM_F		try catchingAllExceptions		*
	 *    TRYEACHTERM_F	try catchingEachThread 			*
	 *			    catchingAllExceptions		*
	 *    								*
	 * Parameter jump tells how far ahead to jump to reach the else *
	 * part.  							*
	 *--------------------------------------------------------------*/

	{register LONG jump = next_three_bytes;
	 register int try_kind  = next_byte;
	 push_try(push_try_c(this_instr_pc + jump, try_kind));
	 goto next_instr;  		/* Can't fail. */
        }

      case QUICK_TRY_I:

	/*------------------------------------------------------------*
	 * Start a quick try expression. This is similar in spirit to *
	 * starting a try, but avoids using the control, and is much  *
	 * faster.  We must be sure to restore the stack, environment *
         * and state (of nonshared boxes) at a failure.		      *
	 *							      *
	 * A quick-try is only used for TRY_F tries, not for other    *
	 * kinds.  So there is no try_kind parameter.		      *
	 *------------------------------------------------------------*/

	qt_goto           = next_three_bytes + this_instr_pc;
	qt_stack_depth    = 0;
	qt_env_numentries = the_act.num_entries;
	SET_LIST(the_act.state_hold, 
		 state_cons(the_act.state_a, the_act.state_hold));
	goto next_instr;  	/* Can't fail. */

      case THEN_I:

	/*----------------------------------------------------------------*
	 * The condition of a try-expression has just succeeded.  If this *
	 * is a general try (qt_goto == NULL) then pop the control back   *
	 * to the beginning of the try.  If this is a quick try, just     *
	 * set qt_goto = NULL to indicate that we are no longer in this   *
	 * try, and undo the pushing of the state that is done at a quick *
	 * try.								  *
	 *----------------------------------------------------------------*/

	if(qt_goto == NULL) {
	  CONTROL *c;
          register LONG try_id = first_try_id(the_act.embedding_tries);
	  bump_control(c = then_c(the_act.control, try_id));
	  pop_try();
	  SET_CONTROL(the_act.control, 
		      clean_up_control_c(c, &the_act));
	  drop_control(c);
	}
	else {
	  qt_goto = NULL;
	  SET_LIST(the_act.state_hold, the_act.state_hold->tail);
	}
	goto next_instr;  	/* Can't fail. */

      case PUSH_EXC_I:

	/*---------------------------------------------------------*
	 * This is executed at the start of an else-part of a try  *
	 * construct.  It pushes last_exception onto 	           *
	 * the_act.exception_list.				   *
	 *---------------------------------------------------------*/

	{register ENTITY* a = allocate_entity(1);
	 *a = last_exception;
	 SET_LIST(the_act.exception_list, 
		  ents_cons(a, the_act.exception_list));
	 goto next_instr;  	/* Can't fail. */
        }

      case POP_EXC_I:

	/*--------------------------------*
	 * Undo the action of PUSH_EXC_I. *
	 *--------------------------------*/

	if(the_act.exception_list == NIL) die(66);
	SET_LIST(the_act.exception_list, the_act.exception_list->tail);
	goto next_instr;  	/* Can't fail. */

      case EXCEPTION_I:

	/*----------------------------------------------------------*
	 * Push the current exception.  It is found at the front of *
	 * the_act.exception_list.				    *
	 *----------------------------------------------------------*/

	*PUSH_STACK() = (the_act.exception_list != NIL)
			  ? *(the_act.exception_list->head.ents) : zero;
	qt_stack_depth++;
	goto next_instr;  	/* Can't fail. */

      case STREAM_I:

	/*--------------------------------------------------------------*
	 * Begin a stream.  Jump is the offset to the instruction that  *
	 * begins the alternative thread.  			 	*
	 *--------------------------------------------------------------*/

	{register LONG jump = next_three_bytes;
	 branch_c(this_instr_pc + jump);
	 goto next_instr;  	/* Can't fail. */
        }

      case MIX_I:

	/*----------------------------------------------------------------*
	 * Begin a mix construct.  Jump is the offset to the code that    *
	 * executes the alternative thread.  These       		  *
	 * instructions generate an artificial time-out to let the        *
	 * other thread get itself established.  Don't backup into them  *
	 * because of the timeout.					  *
	 *----------------------------------------------------------------*/

	{register LONG jump = next_three_bytes;
	 mix_c(this_instr_pc + jump);
	 l_time = 0;
	 failure = TIME_OUT_EX;
	 num_threads++;
	 no_backup = TRUE;
	 break;
        }

      case FAIL_I:

	/*------------------------------------------------------*
	 * Fail with the exception that is on top of the stack. *
	 *------------------------------------------------------*/

	{register ENTITY* sp = top_stack();
	 IN_PLACE_EVAL_FAILTO(*sp, &l_time, fail_fail_i);
	 failure = qwrap_tag(*sp);
	 failure_as_entity = *sp;

       fail_fail_i:
         break;
        }

      case FAILC_I:

	/*----------------------------------------------------------*
	 * Fail with the simple exception that is given by the byte *
	 * parameter. 						    *
	 *----------------------------------------------------------*/

	failure = next_byte;
	break;

      case BEGIN_CUT_I:

	/*-------------------------------------------------------------------*
	 * Begin a CutHere construct.  This involves pushing a marker onto   *
	 * the control so that we know where to cut back to at an END_CUT_I, *
	 * and pushing the identity of that marker onto the stack of	     *
	 * embedding constructs.					     *
	 *-------------------------------------------------------------------*/

        push_mark(mark_c());
	goto next_instr;  	/* Can't fail. */

      case END_CUT_I:

	/*-------------------------------------------------------------*
	 * End a CutHere construct.  Pop the stack of embedding	       *
	 * constructs.  If the bottom node of the control is the       *
         * corresponding MARK node, delete that node.		       *
	 *-------------------------------------------------------------*/

        pop_mark();
	SET_CONTROL(the_act.control,
		    clean_up_control_c(the_act.control, &the_act));
	goto next_instr;  	/* Can't fail. */

      case CUT_I:

	/*-------------------------------------------------------------*
	 * Pop the stack (which should hold () on its top) and perform *
	 * a cut.  Cut back to the mark that was pushed at a   	       *
	 * BEGIN_CUT_I instruction.				       *
	 *-------------------------------------------------------------*/

	{UP_CONTROL* c;
	 POP_STACK();  /* remove () */
         bump_control(c = chop_c(the_act.control, 
			         first_mark_id(the_act.embedding_marks)));
	 SET_CONTROL(the_act.control, clean_up_control_c(c, &the_act));
         drop_control(c);
	 goto next_instr;  	/* Can't fail. */
        }

      case BEGIN_PRESERVE_I:

	/*--------------------------------------------------------------*
	 * Remember the current state in the_act.state_hold, pushing it *
	 * on the front of that list.  It will be restored an an        *
	 * END_PRESERVE_I.						*
	 *--------------------------------------------------------------*/

	SET_LIST(the_act.state_hold, 
		 state_cons(the_act.state_a, the_act.state_hold));
	goto next_instr;  	/* Can't fail. */

      case END_PRESERVE_I:

	/*--------------------------------------------*
	 * Restore the state from the_act.state_hold. *
	 *--------------------------------------------*/

	restore_state();
	break;

      case PUSH_TRAP_I:

	/*--------------------------------------------------------------*
	 * Push the current trap vector onto the_act.trap_vec_hold.     *
	 * This is done at the beginning of a trap or untrap construct  *
	 * so that the trap vector can be restored at the end of the    *
	 * construct.							*
	 *--------------------------------------------------------------*/

	SET_LIST(the_act.trap_vec_hold, 
		 trap_vec_cons(the_act.trap_vec_a,
			       the_act.trap_vec_hold));
	goto next_instr;  	/* Can't fail. */

      case POP_TRAP_I:

	/*-----------------------------------------------------*
	 * Restore the trap vector from the_act.trap_vec_hold. *
	 *-----------------------------------------------------*/

        SET_TRAP_VEC(the_act.trap_vec_a, the_act.trap_vec_hold->head.trap_vec);
	SET_LIST(the_act.trap_vec_hold, the_act.trap_vec_hold->tail);
	goto next_instr;  	/* Can't fail. */

      case TRAP_I:
      case UNTRAP_I:

	/*------------------------------------------------------------------*
	 * Modify the trap status of an exception.  s is the exception to   *
	 * trap or untrap.  If s is ALL_TRAP, trap or untrap all exceptions.*
	 *------------------------------------------------------------------*/

	s = POP_STACK();
        IN_PLACE_EVAL_FAILTO(s, &l_time, fail_trap_i);
	{TRAP_VEC *tr;
	 tr = the_act.trap_vec_a;
	 if(tr->ref_cnt > 1) {
	   tr = copy_trap_vec(tr);
	   SET_TRAP_VEC(the_act.trap_vec_a, tr);
	 }
	 if(inst == TRAP_I) set_trap(qwrap_tag(s), tr);
	 else clear_trap(qwrap_tag(s), tr);
	 qt_stack_depth--;
        }
	break;

      fail_trap_i:
	*push_stack() = s;
	break;

      case ALL_TRAP_I:

	/*----------------------------------------------------------------*
	 * Push ALL_TRAP onto the stack.  This is done immediately before *
	 * a TRAP_I or UNTRAP_I instruction.				  *
	 *----------------------------------------------------------------*/

	*PUSH_STACK() = ENTU(ALL_TRAP);
	qt_stack_depth++;
	goto next_instr_without_timeout;	/* Can't fail.          *
						 * Don't need to count. */

      case REPAUSE_I:

	/*-----------------------------------------*
	 * Do a pause, but increment num_repauses. *
	 *-----------------------------------------*/

	num_repauses++;
	/* No break -- continue with PAUSE_I. */

      case PAUSE_I:

	/*--------------------------------------------------------------*
	 * Pop the stack, which should have () on its top.		*
	 * Pause the current thread, giving control to another thread.	*
	 * Generally, don't back up, so that the pause is not done	*
	 * repeatedly.							*
	 *--------------------------------------------------------------*/

	pop_stack();                /* Argument () */
	no_backup = pause_i(&l_time);
	force_timeout = TRUE;
	break;

      case BEGIN_ATOMIC_I:

	/*-----------------------------------------------------------------*
	 * Start an atomic construct.  Atomic constructs can be nested, so *
	 * we must be sure to keep track of nesting depth in in_atomic.    *
	 *-----------------------------------------------------------------*/

	the_act.in_atomic++;
	goto next_instr;  	/* Can't fail. */

      case END_ATOMIC_I:

	/*--------------------------*
	 * End an atomic construct. *
	 *--------------------------*/

	do_end_atomic(&l_time, &no_backup);
	break;

      case BIND_UNKNOWN_I:

	/*---------------------------------------------------------*
	 * Pop s and ss.					   *
	 *							   *
	 * If the next byte is 0, then s should be an unprotected  *
	 * unknown.  Bind s to ss.  				   *
	 *							   *
	 * If the next byte is 1, then s is (u,k), where u is a    *
	 * protected unknown and k is a write key for it.  Bind u  *
	 * to ss using write key k.		   		   *
	 *---------------------------------------------------------*/

	s  = POP_STACK();
	ss = POP_STACK();
	bind_unknown(ss, s, next_byte, &l_time);
	if(failure < 0) qt_stack_depth -= 2;
	else {
	  *push_stack() = ss;
	  *push_stack() = s;
	}
	break;

      case TEST_LAZY1_I:

	/*------------------------------------------------------------*
	 * Pop s, and push false if s is lazy, true if s is explicit. *
	 * Every once in a while, push false even when s is explicit. *
	 *------------------------------------------------------------*/

	{register ENTITY* sp = top_stack();
	 *sp = ENTU(test_lazy(*sp, 1));
	 break;
        }

      case TEST_LAZY_I:

	/*------------------------------------------------------------*
	 * Pop s, and push false if s is lazy, true if s is explicit. *
	 *------------------------------------------------------------*/

        {register ENTITY* sp = top_stack();
	 *sp = ENTU(test_lazy(*sp, 0));
	 break;
        }

      case LAZY_I:
      case LAZY_RECOMPUTE_I:

	 /*--------------------------------------------------------*
	  * Push a lazy entity whose executable code starts at the *
	  * current address onto the stack.  After doing that,     *
	  * continue with the address given by offset jump.	   *
	  *							   *
	  * The jump is followed by the index in lazy_type_instrs  *
	  * of the type of this entity.				   *
	  *--------------------------------------------------------*/

	 {register LONG jump = next_three_bytes;
	  register int  k    = toint(next_three_bytes);
	  s = make_lazy(the_act.program_ctr, 1, NULL,
			lazy_type_instrs[k], inst == LAZY_RECOMPUTE_I, 0);
	  *PUSH_STACK() = s;
	  the_act.program_ctr = this_instr_pc + jump;
	  goto next_instr;  	/* Can't fail. */
	 }

      case LAZY_LIST_I:
      case LAZY_LIST_RECOMPUTE_I:

	 /*--------------------------------------------------------*
	  * Push the lazy list whose executable code begins at the *
	  * current address onto the stack.  After doing that,     *
	  * continue with address given by offset jump.		   *
	  *--------------------------------------------------------*/

	 {register LONG jump = next_three_bytes;
	  register int  k    = toint(next_three_bytes);
	  s = lazy_list_i(the_act.program_ctr, 1,
		      lazy_type_instrs[k], (inst == LAZY_LIST_RECOMPUTE_I));
	  *PUSH_STACK() = s;
	  the_act.program_ctr = this_instr_pc + jump;
	  goto next_instr;  	/* Can't fail. */
	 }

      case NAME_I:

	/*-----------------------------------------------------------*
	 * The parameter give the offset of the name associated with *
	 * this code, as an index into outer_bindings.		     *
	 *-----------------------------------------------------------*/

	{register LONG k = next_three_bytes;
	 the_act.name = outer_bindings[k].name;

	 if(break_info.break_names && !break_info.suppress_breaks 
	    && breakable()) {
	   break_info.pc = this_instr_pc;
	   vis_break(NAME_VIS);
	 }

	 goto next_instr;  	/* Can't fail. */
	}

      case SNAME_I:

	/*-----------------------------------------------------------*
	 * The parameter give the offset of the name associated with *
	 * this code, as an index into constants.  constants[k] must *
	 * be a CSTR_TAG string.				     *
	 *-----------------------------------------------------------*/

	{register LONG k = next_three_bytes;
	 the_act.name = CSTRVAL(constants[k]);
	 goto next_instr;  	/* Can't fail. */
	}

      case EXC_CONST_I:

	/*--------------------------------------------------------------*
	 * Push a simple exception whose tag is given by the parameter. *
	 *--------------------------------------------------------------*/

	*push_stack() = ENTCH(next_byte);
	qt_stack_depth++;
	goto next_instr;  	/* Can't fail. */

      case GET_TYPE_I:

	/*---------------------------------------------------------*
	 * Let k be the byte parameter.  Push the type in the type *
	 * environment at offset k onto the type stack.  Pop s and *
	 * push () in its place.			           *
	 *---------------------------------------------------------*/

	{register int k = next_byte;
	 push_a_type(type_env(the_act.env, k)->TY1);
	 *(top_stack()) = hermit;
	 goto next_instr;  	/* Can't fail. */
        }

      case WRAP_I:

	/*-----------------------------------------------------------*
	 * Pop s, and pop type T from the type stack.  Push <<s:T>>, *
	 * entity s wrapped with type T.			     *
	 *-----------------------------------------------------------*/

	{register ENTITY* sp = top_stack();
	 register TYPE* tt = pop_type_stk();  /* ref from type stack */
         *sp = wrap(*sp, tt);
	 drop_type(tt);
         goto next_instr;      /* Can't fail. */
        }

      case UNWRAP_I:

	/*--------------------------------------------------------------*
	 * Pop s, which must be of the form <<x:T>>, where T is a type. *
	 * Push x, and push T onto the type stack.                      *
	 *--------------------------------------------------------------*/

	{register ENTITY *w;
	 register ENTITY* sp = top_stack();
	 IN_PLACE_EVAL_FAILTO(*sp, &l_time, fail_unwrap_i); 
	 if(TAG(*sp) != WRAP_TAG) die(68);
	 w  = ENTVAL(*sp);
	 *sp = w[1];
	 ss = w[0];
	 if(TAG(ss) != TYPE_TAG) {
	   die(69, toint(TAG(ss)));
	 }
	 push_a_type(TYPEVAL(ss));

       fail_unwrap_i:
	 break;
	}

      case QWRAP_I:
      case EXC_WRAP_I:

	/*------------------------------------------------------------------*
	 * Pop s, and push {s:k}, where k is the byte parameter, and        *
	 * {s:k} is the result of tagging s with tag k.  These instructions *
	 * are identical here, but are handled differently in package.c,    *
	 * because the parameter of EXC_WRAP_I needs to be altered there.   *
	 *------------------------------------------------------------------*/

	{register ENTITY* sp = top_stack();
	 *sp = qwrap(next_byte, *sp);
	 goto next_instr;  	/* Can't fail. */
        }

      case QUNWRAP_I:
      case EXC_UNWRAP_I:
      case QTEST_I:
      case EXC_TEST_I:

	/*--------------------------------------------------------*
	 * Pop s, which must be of the form {x:i}, where i is an  *
	 * integer tag.  EXC_TEST_I and QTEST_I each get the byte *
	 * parameter k, and push true if i = k, false if i != k.  *
	 * EXC_UNWRAP_I and QUNWRAP_I each push x if i = k, and   *
	 * fail with exception TEST_EX if i != k.                 *
	 *--------------------------------------------------------*/

	{int discr;
	 register int k;
	 register ENTITY* sp = top_stack();
	 qunwrap_i(&discr, sp, &l_time);
	 k = next_byte;
	 if(failure < 0) {
	   if(discr != k) {
	     if(inst <= EXC_UNWRAP_I) {
	       failure = TEST_EX;
	       *sp = qwrap(discr, *sp);  /* restore *sp. */
	     }
	     else *sp = false_ent;
           }
	   else if(inst >= QTEST_I) *sp = true_ent;
         }
	 break;
        }

      case UNIFY_T_I:

	/*------------------------------------------------------*
	 * UNIFY the two types on the top of the type stack. 	*
	 * If unification succeeds, do nothing.  If not,    	*
	 * fail with exception SPECIES_EX.			*
	 *------------------------------------------------------*/

	{TYPE *t1, *t2;
	 t1 = pop_type_stk();   /* ref from type stack */
	 t2 = pop_type_stk();   /* ref from type stack */
	 set_in_binding_list();
	 if(!UNIFY(t1,t2,FALSE)) failure = SPECIES_EX;
	 get_out_binding_list();
	 drop_type(t1);
	 drop_type(t2);
	 break;
	}

      case PUSH_TYPE_BINDINGS_I:
	/*------------------------------------------------------*
	 * Push the top of the_act.type_binding_lists		*
	 * onto the_act.type_binding_lists.  If 		*
	 * the_act.type_binding_list is NIL, then push NIL.	*
	 * This is used to enter a new type binding scope.	*
	 *------------------------------------------------------*/

	if(the_act.type_binding_lists == NIL) {
	  push_list(the_act.type_binding_lists, NIL);
	}
	else {
         push_list(the_act.type_binding_lists, 
		   top_list(the_act.type_binding_lists));
	}
	break;

      case POP_TYPE_BINDINGS_I:
	/*------------------------------------------------------*
	 * Pop the_act.type_binding_lists.  This is used	*
	 * to exit a type binding scope without committing to   *
	 * the bindings.					*
	 *------------------------------------------------------*/

	pop(&(the_act.type_binding_lists));
	break;

      case COMMIT_TYPE_BINDINGS_I:
	/*------------------------------------------------------*
	 * Delete the member of the_act.type_binding_lists	*
	 * that is just below the top member.  This 		*
	 * commits to the changes made since the last 		*
	 * PUSH_TYPE_BINDINGS_I instruction and exits the	*
	 * type binding scope.					*
	 *------------------------------------------------------*/

	SET_LIST(the_act.type_binding_lists->tail, 
		 the_act.type_binding_lists->tail->tail);
	break;

      case COROUTINE_I:

	/*--------------------------------------------------------------*
	 * Start a coroutine.  This is awfully ugly.  See language.doc. *
	 *--------------------------------------------------------------*/

	{CODE_PTR start1, start2, pc1, pc2;
	 CONTINUATION *cont;
	 pc1 = process_coroutine_args(this_instr_pc, &start1);
	 pc2 = process_coroutine_args(pc1, &start2);
	 cont = get_coroutine_cont(pc2+1);  /* Ref from get_coroutine_cont */
	 push_coroutine(start1, cont);
	 push_coroutine(start2, cont);
	 the_act.program_ctr = pc2;
	 drop_continuation(cont);
	 break;
        }

      case RESUME_I:

	/*---------------------*
	 * Resume a coroutine. *
	 *---------------------*/

	resume_i();
	break;

      case CHECK_DONE_I:

	/*--------------------------------------------------*
	 * Part of coroutine processing.  See language.doc. *
	 *--------------------------------------------------*/

	{register int k = next_byte;
	 s = local_binding_env(the_act.env, k);
	 if(TAG(s) != INDIRECT_TAG) die(64);
	 if(!ENT_EQ(*ENTVAL(s), NOTHING)) {
	   LIST* cr = the_act.coroutines;
	   if(cr->head.list == NIL && cr->tail == NIL) {
	     SET_LIST(the_act.coroutines, NIL);
	   }
	   return_i(inst);
         }
	 break;
        }

      case STORE_INDIRECT_I:

	/*---------------------------------------------------------------*
	 * Store s into the location referenced indirectly by what is in *
	 * the environment at an offset given by the parameter to this   *
	 * instruction .						 *
	 *---------------------------------------------------------------*/

	{register ENTITY* a;
	 s = local_binding_env(the_act.env, next_byte);
	 if(TAG(s) != INDIRECT_TAG) die(65);
	 a = ENTVAL(s);
	 *a = pop_stack();
	 break;
        }

      case LAZY_DOLLAR_I:

	/*-------------------------------*
	 * Pop s, and push lazyDollar(s) *
	 *-------------------------------*/

	{register ENTITY* sp  = top_stack();
	 *sp = lazy_dollar(*sp);
	 break;
        }

      case BEGIN_VIEW_I:

	/*---------------------------*
	 * Begin a view computation. *
	 *---------------------------*/

        in_show++;
	goto next_instr;  	/* Can't fail. */

      case END_VIEW_I:

        /*-------------------------*
         * End a view computation. *
         *-------------------------*/

        in_show--;
	goto next_instr;  	/* Can't fail. */

      case RAW_SHOW_I:

	/*-------------------------------------------------------------*
	 * Do RawShow(ss,s). where ss is the file and s is the entity  *
	 * to show.						       *
	 *-------------------------------------------------------------*/

	if(break_info.break_prints && !break_info.suppress_breaks 
	   && breakable()) {
	  break_info.pc = this_instr_pc;
	  vis_break(PRINT_VIS);
	}
	
	{FILE *f;

         /*---------------------------------------------*
          * Get s and ss (the stack top and next down). *
          *---------------------------------------------*/

	 s  = pop_stack();   /* entity to show */
	 ss = pop_stack();  /* file to show on */
	 IN_PLACE_FULL_EVAL(ss, &l_time); 

	 /*---------------------------------------------*
	  * If ss is not a file, we cannot do the show. *
	  *---------------------------------------------*/

         if(failure < 0 && TAG(ss) != FILE_TAG) {
	   failure = NO_FILE_EX;
	   failure_as_entity = 
	     qwrap(NO_FILE_EX,
		   make_str("Cannot raw-show to an internal file"));
	 }

	 /*---------------------------------*
	  * If all looks good, do the show. *
	  *---------------------------------*/

	 if(failure < 0) {
	   f = file_from_entity(ss);
	   if(failure < 0) {
	     print_entity(f, s, LONG_MAX);
	     break;
	   }
         }

	 /*------------------------------------------*
	  * At a time-out, prepare to do this again. *
	  *------------------------------------------*/

	 if(failure == TIME_OUT_EX) {
	   *push_stack() = ss;
	   *push_stack() = s;
	 }
	 break;
        }

      case LINE_I:

        /*-----------------------------*
         * This should never be seen.  *
         *-----------------------------*/

        goto bad_instruction;

      case FPRINT_I:

	/*-----------------------------------------*
	 * Pop s and ss, and do FWrite ss,s. where *
	 * ss is an external file.	           *
	 *-----------------------------------------*/

	s  = pop_stack();
	ss = pop_stack(); 

	if(break_info.break_prints && !break_info.suppress_breaks 
	   && breakable()) {
	  break_info.pc = this_instr_pc;
	  vis_break(PRINT_VIS);
	}

	if(TAG(ss) != FILE_TAG) {
	  IN_PLACE_FULL_EVAL_FAILTO(ss, &l_time, print_putback);
	}

	s = ast_fprint(ss, s, &l_time);
	if(failure >= 0) goto print_putback;
	else qt_stack_depth -= 2;
	break;

      print_putback:
	*push_stack() = ss;
	*push_stack() = s;
	break;

      case TOPFORCE_I:

	/*-------------------------------------------------*
	 * Evaluate the top of the stack to its top level. *
	 *-------------------------------------------------*/

	{register ENTITY* sp = top_stack();
	 IN_PLACE_EVAL(*sp, &l_time);
	 break;
        }

      case SPECIES_AS_VAL_I:
 
        /*--------------------------------------------------------------*
	 * Pop the type stack and push it back onto the computation	*
	 * stack as an entity.						*
	 *--------------------------------------------------------------*/

	{TYPE* t = pop_type_stk();	/* Ref from type_stack */
	 *push_stack() = ENTP(TYPE_TAG, t);
	 break;
	}

      TYPE_INSTRUCTIONS

	/*----------------------------------------------------------------*
	 * Handle the type instructions.  See instruc.h for definition of *
	 * TYPE_INSTRUCTIONS.	A sequence of type instructions is 	  *
	 * handled without interruption, since it modifies the shared	  *
	 * type stack.  That also makes it unnecessary to copy the	  *
	 * binding list in and out repeatedly.				  *
	 *----------------------------------------------------------------*/

	set_in_binding_list();
	do {
	  type_instruction_i(inst, &the_act.program_ctr, NULL, the_act.env, 0);
	  this_instr_pc = the_act.program_ctr++;
	  inst = *this_instr_pc;
          if(do_profile) install_profile_instruction();
#         ifdef DEBUG
            if(trace) trace_eval(qt_goto, qt_stack_depth, l_time, inst);
#         endif
	} while(FIRST_TYPE_INSTRUCTION <= inst && 
		inst <= LAST_TYPE_INSTRUCTION);
	freeze_type_stk_top();
        get_out_binding_list();
	
        /*--------------------------------------------------------*
	 * Now we must avoid getting a new instruction, since one *
	 * has already been fetched.				  *
	 *--------------------------------------------------------*/

	goto perform_instruction;

      default:
      bad_instruction:

	/*--------------------------------------*
	 * On an unrecognized instruction, die. *
	 *--------------------------------------*/

	die(72, (char *)(LONG) inst);

    } /* end switch */


    /****************************************************************/
    /************************ Handle failure ************************/

  fail:
    if(failure >= 0) {
      Boolean should_break;

      should_break = break_info.break_failures && 
	             !break_info.suppress_breaks && 
		     breakable();

      /*-----------------------------------------------------------*
       * The MSWIN version needs to get all the way out at a trap. *
       *-----------------------------------------------------------*/

#     ifdef MSWIN
	should_break = FALSE;
	if(failure == TRAPPED_EX) {
          unreg(mark);
	  use_the_act_bindings--;
          return;
        }
#     endif

#     ifdef DEBUG
	if(trace) {
	  trace_i(78, failure, toint(inst));
	}
#     endif

      /*======================================================*
       ******** Drop references for garbage collection ********
       *======================================================*/

      s = ss = false_ent;

      /*======================================================*
       ****** Record the exception in last_exception.  	*******
       ****** Also set failure_as_entity for simple 	*******
       ****** exceptions, since it is not set otherwise.*******
       *======================================================*/

      if(ENT_EQ(failure_as_entity, NOTHING)) {
	last_exception = failure_as_entity = ENTU(failure);
      }
      else {
	last_exception = failure_as_entity;
      }


      /*=========================================================*
       *************** Possibly trap the exception ***************
       *=========================================================*/

      /*------------------------------------------------------------*
       * Note: Don't trap if currently printing the run-time stack. *
       * We don't want this trap to replace the old one.	    *
       *------------------------------------------------------------*/

      if(should_trap(failure)) {
	trap_ex(last_exception, inst, this_instr_pc);
        unreg(mark);
	use_the_act_bindings--;
	return;
      }


      /*===============================================================*
       ******** Check for suppressed time-out.  This is needed  ********
       ******** because quick-try failure is influenced by      ********
       ******** time-outs                                       ********
       *===============================================================*/

      if(failure == TIME_OUT_EX && 
	 (!type_stk_is_empty() || 
	   (!force_timeout && the_act.in_atomic != 0))) {
        goto time_out_lbl;  /* Below, after quick-try section */
      }


      /*================================================*
       ******** Check for quick try failure  ************
       *================================================*/

      if(qt_goto != NULL) {
	CODE_PTR save_pc;
	STACK *save_stack;
	ENVIRONMENT *save_env;
	int save_num_entries, k;
	STATE *save_state;
        LIST *save_state_hold;

	/*----------------------------------------------*
	 * A quick try never catches a TERMINATE_EX	*
	 * or ENDTHREAD_EX exception.  Just ignore them.*
	 *----------------------------------------------*/

	if(failure == TERMINATE_EX || failure == ENDTHREAD_EX) {
	  qt_goto = NULL;
	  goto time_out_lbl;  /* below */
	}

        /*------------------------------------*
         * Clear the type stack at a failure. *
         *------------------------------------*/

	clear_type_stk();

        /*----------------------------------------------*
         * At a time-out, or if tracing, must convert 	*
	 * the quick try to a normal try. That is done  *
	 * below.  We need to save the current values   *
	 * of the program counter, stack and environment*
	 * in order to do that, and install them below.	*
         *----------------------------------------------*/

	if(failure == TIME_OUT_EX || should_break) {
	  save_pc               = the_act.program_ctr;
	  save_stack            = get_stack();
	  save_num_entries      = the_act.num_entries;
	  bump_env(save_env     = the_act.env, save_num_entries);
	  bump_state(save_state = the_act.state_a);
          bump_list(save_state_hold = the_act.state_hold);
        }

	/*------------------------------------------------------------*
	 * Go to failure point.  Pop stack back to where it should be *
         * after failure, and exit scope if necessary. 	Restore the   *
	 * state.						      *
	 *------------------------------------------------------------*/

	the_act.program_ctr = qt_goto;
	qt_goto = NULL;
	for(k = qt_stack_depth; k > 0; k--) POP_STACK();
	if(the_act.num_entries > qt_env_numentries) {
	  exit_scope_env(qt_env_numentries);
	}
        restore_state();

	/*----------------------------------------------------*
	 * Push the control node at a time-out or if tracing. *
	 *----------------------------------------------------*/

	if(failure == TIME_OUT_EX || should_break) {

	  /*------------------------------------------------------------*
	   * the_act now has the "else" act in it.  Do a try for it, to *
	   * convert the quick try to an ordinary try.			*
	   *------------------------------------------------------------*/

	  push_try(try_c(the_act.program_ctr));

	  /*------------------------------------------------------------*
	   * Restore the_act to what it was, and do a time-out.  Note 	*
	   * that only the pc, stack and environment were changed.	*
	   *------------------------------------------------------------*/

	  the_act.program_ctr = save_pc;
	  drop_stack(the_act.stack);
	  the_act.stack       = save_stack;         /* ref from save_stack */
	  drop_env(the_act.env, the_act.num_entries);
	  the_act.num_entries = save_num_entries;
	  the_act.env         = save_env;           /* ref from save_env */
	  drop_state(the_act.state_a);
	  the_act.state_a     = save_state;	    /* ref from save_state */
	  drop_list(the_act.state_hold);
	  the_act.state_hold  = save_state_hold;    /* ref from 
						       save_state_hold */
	}

	/*--------------------------------------------------------------*
	 * If not a time-out and not tracing, we are ready to recover   *
	 * and get the next instruction.				*
	 *--------------------------------------------------------------*/

	else {
	  failure = -1;
	  failure_as_entity = NOTHING;
	  goto next_instr;
	}
      } /* end if(qt_goto != NULL) */


      /*===============================================*
       ************* Check for a time out **************
       *===============================================*/

   time_out_lbl:
      if(failure == TIME_OUT_EX) {
        failure_as_entity = NOTHING;

        /*-----------------------------------------------------*
         * Restore program counter to instruction that failed, *
	 * unless suppressed.				       *
         *-----------------------------------------------------*/

        if(!no_backup) the_act.program_ctr = this_instr_pc;

        /*------------------------------------------------------------*
         * Don't time-out if the time-out was due to a list primitive *
         * cutting its operation short without really running out of  *
         * time 						      *
         *------------------------------------------------------------*/

	if(l_time > 1 && !force_timeout) {
	  l_time--;
	  failure = -1;
	  goto next_instr_without_timeout;
	}

        /*----------------------------------------------------------------*
         * Don't time-out if inside an atomic construct, except at pause. *
         *----------------------------------------------------------------*/

        if(!force_timeout && the_act.in_atomic != 0) {
	  l_time = LONG_MAX;
	  timeout_at_end_atomic = TRUE;
	  failure = -1;
	  goto next_instr_without_timeout;
	}

        /*-----------------------------------------------------------*
         * Don't time-out when there is something on the type stack, *
	 * since it is global. 					     *
         *-----------------------------------------------------------*/

	if(!type_stk_is_empty()) {
	  l_time = 10;
	  failure = -1;
	  goto next_instr_without_timeout;
	}

	/*---------------------------------*
	 * Possibly break before time-out. *
	 *---------------------------------*/

	if(should_break) {
	  break_info.pc = the_act.program_ctr;
	  vis_break(FAIL_VIS);
	}

        /*-------------------------------------------*
         * Handle other processes, if there are any. *
         *-------------------------------------------*/

        force_timeout = timeout_at_end_atomic = FALSE;
	if(do_timeout(&l_time)) goto out;

        /*-------------------------------*
         * Possibly break after failing. *
         *-------------------------------*/

	if(should_break) {
	  break_info.pc = the_act.program_ctr;
	  vis_break(RESUME_VIS);
	}

	failure = -1;
        goto next_instr_without_progress;
      } /* end if(failure == TIME_OUT_EX) */


      /*========================================================*
       ******** Case where the failure is not a timeout. ********
       *========================================================*/

      else { /* failure is not a timeout */
	if(should_break) {
	  break_info.pc = this_instr_pc;
	  vis_break(FAIL_VIS);
	}

        /*--------------------------------*
         * Clear type stack at a failure. *
         *--------------------------------*/

	clear_type_stk();

        /*---------------------*
         * Process the failure *
         *---------------------*/

	if(do_failure(the_act.control, inst, &l_time)) goto out;

	if(should_break) {
	  break_info.pc = the_act.program_ctr;
	  vis_break(RESUME_VIS);
	}

	/*-------------------------------------------------*
	 * fail_act is not needed, so drop its references. *
	 *-------------------------------------------------*/

	store_fail_act = TRUE;
	if(fail_act.program_ctr != NULL) {
	  drop_activation_parts(&fail_act);
	  fail_act.program_ctr = NULL;
	  fail_ex = NOTHING;
	}

        /*---------------------------------------------------*
         * Do not decrement the time count.  We need to give *
         * the new position a chance to run, so that a	     *
         * PUSH_EXC_I instruction will get the exception     *
         * before a thread switch. 			     *
         *---------------------------------------------------*/

         goto next_instr_without_timeout;

      } /* end else (failure is not a timeout) */
    }  /* end if(failure >= 0) */


    /*********************************************************************/
    /******************** Goto next instruction **************************/

  next_instr: 

    /*---------------------------------------------*
     * Decrement the time and check for time out.  *
     *---------------------------------------------*/

    if(l_time-- <= 0 && type_stk_is_empty()) {
#     ifdef DEBUG
        if(trace) trace_i(318);
#     endif
      l_time    = 0;
      failure   = TIME_OUT_EX;
      no_backup = TRUE;
      goto fail;  /* Above.  This will cause time out. */
    }

  next_instr_without_timeout: 
    the_act.progress = 1;

  next_instr_without_progress:
    no_backup = FALSE;

  } /* end for(;;) */
}


#ifdef DEBUG

/****************************************************************
 *			PRINT_THE_COROUTINES			*
 ****************************************************************/

PRIVATE void print_the_coroutines(LIST *cr)
{
  LIST *p;
  ACTIVATION *a;

  for(p = cr; p != NIL; p = p->tail) {
#   ifdef GCTEST
      lkindf(p);
#   endif
    a = p->head.act;
    fprintf(TRACE_FILE,"%s@%p[%d] ", 
	    a->name, a->program_ctr, toint(a->ref_cnt));
  }
  tracenl();
}


/****************************************************************
 *			TRACE_TYPE_BINDING_LIST			*
 ****************************************************************
 * Print the current type binding list of the_act, if it is not *
 * null.							*
 ****************************************************************/

PRIVATE void trace_type_binding_list(void)
{
  if(the_act.type_binding_lists != NULL &&
     the_act.type_binding_lists->head.list != NULL) {
    trace_i(336);
    print_type_list_separate_lines_without_constraints(
	the_act.type_binding_lists->head.list, " ");
  }
}


/****************************************************************
 *			TRACE_EVAL				*
 ****************************************************************
 * Print trace information about an instruction.		*
 ****************************************************************/

PRIVATE void trace_eval(CODE_PTR qt_goto, int qt_stack_depth, 
		       LONG l_time, int inst)
{
  LIST *pp;
  int k, line;
  char *pack_name, *file_name;

  if(trace_fun == NIL || str_member(the_act.name, trace_fun)) {
    tracenl();
    get_line_info(the_act.program_ctr - 1, &pack_name, &file_name, &line);

    /*-------------------------------------*
     * Show whether we have made progress. *
     *-------------------------------------*/

    if(!the_act.progress) fprintf(TRACE_FILE, "%%");

    /*----------------*
     * Quick try info *
     *----------------*/

    if(qt_goto != NULL) fprintf(TRACE_FILE,"*/%d", qt_stack_depth);

    /*-------------*
     * Atomic info *
     *-------------*/

    if(the_act.in_atomic) fprintf(TRACE_FILE, "@");

    /*------*
     * Name *
     *------*/

    trace_i(79, nonnull(the_act.name), toint(the_act.st_depth), 
	    num_threads, line, nonnull(pack_name));

    /*------------------------------------*
     * Show the stack and the type stack. *
     *------------------------------------*/

#   ifdef NEVER
      long_print_stack(the_act.stack,0);
#   endif
    print_stack(the_act.stack);
    if(!type_stk_is_empty()) {
      trace_type_binding_list();
      print_type_stk();
      print_type_store();
    }

    /*-----------------------*
     * Show the environment. *
     *-----------------------*/

    if(trace_env) {
      print_env(the_act.env, the_act.num_entries); 
#     ifdef NEVER
        long_print_env(the_act.env, 1);
#     endif
    }

    /*-----------------*
     * Show the state. *
     *-----------------*/

    if(trace_state) {
      STATE* st = the_act.state_a;
      if(st == NULL) {
	fprintf(TRACE_FILE, "State = NULL\n");
      }
      else {
	trace_i(81, toint(st->ref_cnt));
	short_print_state(st);
	tracenl();
      }
      pp = the_act.state_hold;
      if(pp != NIL) {
	trace_i(82);
        for(; pp != NIL; pp = pp->tail) {
	  st = pp->head.state;
	  if(st != NULL) {
	    trace_i(83, toint(st->ref_cnt));
	    short_print_state(st);
#           ifdef NEVER
	      print_state(st, 1);
#           endif
	    tracenl();
	  }
	  else trace_i(84);
	}
      }
    }

    /*------------------------------------*
     * Show the control, if there is one. *
     *------------------------------------*/

    if(trace_control && the_act.control != NULL) {
      trace_i(85);
      print_up_control(TRACE_FILE, the_act.control, 1);
      tracenl();
    }

    /*----------------------*
     * Show the exceptions. *
     *----------------------*/

    if(the_act.exception_list != NULL) {
      trace_i(86);
      for(pp = the_act.exception_list; pp != NIL; pp = pp->tail) {
	trace_print_entity(*(pp->head.ents));
	fprintf(TRACE_FILE, ", ");
      }
      fprintf(TRACE_FILE, ">\n");
    }

    /*---------------------------------------------------*
     * Show the embedding controls, if tracing controls. *
     *---------------------------------------------------*/

    if(trace_control) {
      if(the_act.embedding_tries != NIL) {
        trace_i(319);
        print_str_list_nl(the_act.embedding_tries);
      }
      if(the_act.embedding_marks != NIL) {
        trace_i(321);
        print_str_list_nl(the_act.embedding_marks);
      }
    }

    /*-----------------------*
     * Show the trap vector. *
     *-----------------------*/

    if(trace_tv) {
      trace_i(87);
      if(the_act.trap_vec_a == NULL) fprintf(TRACE_FILE,"NULL");
      else {
	TRAP_VEC *tv;
	tv = the_act.trap_vec_a;
	for(k = 0; k < trap_vec_size; k++) {
	  fprintf(TRACE_FILE,"%lx ", tv->component[k]);
	}
	fprintf(TRACE_FILE,"|rc=%d", toint(tv->ref_cnt));
      }
      fprintf(TRACE_FILE,")\n");
    }

    /*-----------------------*
     * Extra error checking. *
     *-----------------------*/

    if(the_act.actmark) trace_i(88);
    if(the_act.kind != 0) {
      trace_i(89, toint(the_act.kind));
    }

    /*--------------------------*
     * Show the time, if small. *
     *--------------------------*/

    if(l_time <= 1000) trace_i(90, l_time);

    /*-------------------------------------------------------------------*
     * Show the runtime shadow stack, holding activations that are	 *
     * waiting for a lazy evaluation to complete. 			 *
     *-------------------------------------------------------------------*/

    if(runtime_shadow_st != NIL) {
      trace_i(91);
      for(pp = runtime_shadow_st; pp != NIL; pp = pp->tail) {
	fprintf(TRACE_FILE,"%p ", pp->head.act);
      }
      tracenl();
    }

    /*----------------------------------------*
     * Show the coroutines, if there are any. *
     *----------------------------------------*/

    if(the_act.coroutines != NULL) {
      trace_i(92);
#     ifdef GCTEST
        lkindf(the_act.coroutines);
#     endif
      print_the_coroutines(the_act.coroutines->head.list);
      trace_i(93);
      print_the_coroutines(the_act.coroutines->tail);
    }

#   ifdef NEVER
      if(gctrace) print_gcend_info();
#   endif

    /*----------------------------------------------------*
     * Show the instruction that is about to be executed. *
     *----------------------------------------------------*/

    fprintf(TRACE_FILE,"\n--");
    print_instruction(TRACE_FILE, the_act.program_ctr, inst);
    tracenl();

    fflush(TRACE_FILE);
  }
}
#endif


/********************************************************
 *			DO_END_ATOMIC			*
 ******************************************************** 
 * Process an END_ATOMIC_I instruction.  This involves  *
 * decrementing in_atomic, and checking whether there   *
 * was a suppressed timeout during the atomic unit.     *
 ********************************************************/

PRIVATE void do_end_atomic(LONG *l_time, Boolean *no_backup) 
{
  the_act.in_atomic--;
  if(timeout_at_end_atomic && the_act.in_atomic == 0) {
    *l_time = 0;
    failure = TIME_OUT_EX;
    *no_backup = TRUE;
    timeout_at_end_atomic = FALSE;
  }
}


/********************************************************
 *			SET_PROFILE			*
 ********************************************************
 * Turn on or off profiling.  sw is true to turn it on, *
 * and is false to turn it off.				*
 ********************************************************/

ENTITY set_profile(ENTITY sw)
{
  if(VAL(sw) != do_profile) {
    if(VAL(sw) != 0) {
      do_profile = TRUE;
      special_condition++;
    }
    else {
      do_profile = FALSE;
      special_condition--;
    }
  }
  return hermit;
}


/********************************************************
 *			PRIM_TRACE			*
 ********************************************************
 * Handle PrimitiveTrace.				*
 ********************************************************/

ENTITY prim_trace(ENTITY sw)
{
# ifdef DEBUG
    LONG k = VAL(sw);
    if(k != 0) read_debug_messages();
    trace             = tobool(k & 1);
    trace_env         = tobool((k & 2) >> 1);
    trace_state       = tobool((k & 4) >> 2);
    trace_extra       = tobool((k & 8) >> 3);
    trace_global_eval = tobool((k & 16) >> 4);
    trace_types       = tobool((k & 32) >> 5);
    trace_tv          = tobool((k & 64) >> 6);
# endif
  return hermit;
}


/********************************************************
 *			INIT_EVALUATE			*
 ********************************************************/

void init_evaluate()
{
  init_object_names();
}

