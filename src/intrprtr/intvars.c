/**********************************************************************
 * File:    intrprtr/intvars.c
 * Purpose: General globals for interpreter
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

#include "../misc/misc.h"

/********************* INTERPETER GLOBALS *****************************/


/****************************************************************
 *			os_environment				*
 ****************************************************************
 * os_environment is an array of strings of the form		*
 *								*
 *   NAME=VALUE							*
 *								*
 * where NAME is an environment variable of the operating 	*
 * system, and VALUE is its value.				*
 ****************************************************************/

char** os_environment = NULL;

/****************************************************************
 *			main_package_name			*
 ****************************************************************
 * main_package_name is the name of the package that is		*
 * called on the astr line.					*
 ****************************************************************/

char* main_package_name = NULL;

/****************************************************************
 *			main_program_name			*
 ****************************************************************
 * main_program_name is the name of the program that is		*
 * called on the astr line.  (This is the file name, as written *
 * on the command line.)					*
 ****************************************************************/

char* main_program_name = NULL;

/****************************************************************
 *			main_program_file_name			*
 ****************************************************************
 * main_program_file_name is the full name (absolute from the	*
 * root of the file system) of the file that was specified on	*
 * the astr command line.					*
 ****************************************************************/

char* main_program_file_name;

/****************************************************************
 *			ast_rts_msg				*
 ****************************************************************
 * ast_rts_msg is the message that is printed as an uncaught or *
 * trapped exception.						*
 ****************************************************************/

char FARR ast_rts_msg[] = 
  "See file ast.rts for the configuration at failure.\n";

/****************************************************************
 *			max_st_depth				*
 ****************************************************************
 * max_st_depth is the number of stack frames at which there is *
 * a warning to the user that the stack has become deep.  If    *
 * the user says that is ok, then max_st_depth is doubled, and  *
 * computation continues.					*
 ****************************************************************/

LONG max_st_depth = MAX_ST_DEPTH;	   

/****************************************************************
 *			max_heap_bytes				*
 ****************************************************************
 * max_heap_bytes is the number of bytes that must be allocated *
 * in the heap before the user is warned that heap usage is     *
 * getting large.  If the user says ok, then max_heap_bytes is  *
 * doubled, and computation continues.				*
 ****************************************************************/

LONG max_heap_bytes = MAX_HEAP_BYTES;

/****************************************************************
 *			the_act					*
 ****************************************************************
 * the_act is the activation record for the currently running   *
 * thread.							*
 ****************************************************************/

ACTIVATION the_act;

/****************************************************************
 *			fail_act				*
 *			fail_instr				*
 *			fail_ex					*
 ****************************************************************
 * fail_act holds what was in the_act when a failure 		*
 * occurred.  It is used to report where the program was	*
 * at an uncaught failure.					*
 *								*
 * fail_instr is set to the instruction that caused the		*
 * failure that occurred at fail_act.  fail_ex is the exception *
 * that occurred.						*
 ****************************************************************/

ACTIVATION fail_act;
int        fail_instr;
ENTITY	   fail_ex;

/****************************************************************
 *			timed_out_kind				*
 *			timed_out_comp				*
 ****************************************************************
 * When a thread is cut off because it has run too long, data   *
 * sufficient to restart it is stored in timed_out_comp.  If    *
 * there are no concurrent threads, kept in a control, then     *
 * the value of timed_out_comp is an ACTIVATION*.  If there is  *
 * control information related to other threads, then 		*
 * timed_out_comp is a CONTROL*.  timed_out_kind tells what     *
 * kind of thing timed_out_comp is - 0 for a CONTROL, 1 for     *
 * an ACTIVATION.						*
 ****************************************************************/

int              timed_out_kind;
union ctl_or_act timed_out_comp;

/****************************************************************
 *			execute_state				*
 ****************************************************************
 * execute_state is the state in which execute-declarations     *
 * run.  It gives bindings of nonshared boxes.  Changes to this *
 * state by one execute-declaration are seen by the next one.   *
 ****************************************************************/

STATE* execute_state;

/****************************************************************
 *			global_trap_vec				*
 ****************************************************************
 * global_trap_vec tells which exceptions are to be trapped	*
 * when starting an execute-declaration or when evaluating a    *
 * symbol in the outer environment.				*
 ****************************************************************/

TRAP_VEC* global_trap_vec;

/****************************************************************
 *			runtime_shadow_st			*
 ****************************************************************
 * When a lazy evaluation starts, the current thread is 	*
 * suspended.  We need to keep a record of it, so that		*
 * suspended threads can be found.  runtime_shadow_st is a 	*
 * stack of suspended activations.  Each member points to a	*
 * thread ACTIVATION.  Each time a lazy evaluation starts, a    *
 * thread is pushed on this stack.				*
 ****************************************************************/

LIST* runtime_shadow_st;


/****************************************************************
 *			precision				*
 *			default_precision			*
 *			default_digits_precision		*
 ****************************************************************
 * precision is the current precision for real number		*
 * operations, in digits to base INT_BASE.  It is -1 if the	*
 * current precision is not available from this variable, but	*
 * must instead be found by getting the content of nonshared	*
 * box precision!.  This variable is used to speed up precision *
 * lookups.							*
 *								*
 * default_precision is the initial value of precision!.  It    *
 * is the minimum number of decimal digits of precision.  	*
 *								*
 * default_digits_precision is the same, but it is the initial	*
 * precision in (internal, INT_BASE) digits rather than in	*
 * decimal digits.						*
 ****************************************************************/

LONG	precision;
int	default_precision    = DBL_DIG;
int	default_digits_prec  = DECIMAL_TO_DIGIT(DBL_DIG);

/****************************************************************
 *			stdin_redirect				*
 *			stdout_redirect				*
 *			stderr_redirect				*
 ****************************************************************
 * stdin_redirect is the name of the file to which stdin should *
 * be redirected, or is NULL if there is no redirection.	*
 *								*
 * stdout_redirect and stderr_redirect are similar redirections *
 * of stdout and stderr.					*
 *								*
 * These file names can be in either internal or external	*
 * format.  That is, they can uses either INTERNAL_DIR_SEP_CHAR	*
 * or EXTERNAL_DIR_SEP_CHAR to separate directories.		*
 ****************************************************************/

char* stdin_redirect   = NULL;
char* stdout_redirect  = NULL;
char* stderr_redirect  = NULL;


/****************************************************************
 *			program_is_running			*
 ****************************************************************
 * program_is_running is nonzero if the program is currently    *
 * running, and is 0 if nothing is running.			*
 ****************************************************************/

int program_is_running = 0;

/****************************************************************
 *			in_visualizer				*
 *			start_visualizer_at_trap		*
 ****************************************************************
 * in_visualizer	is true if the interpreter has started  *
 *			the debugger.				*
 *								*
 * start_visualizer_at_trap is true if the interpreter should	*
 * 			    start the debugger when a failure	*
 *			    is trapped.				*
 ****************************************************************/

Boolean in_visualizer            = 0;
Boolean start_visualizer_at_trap = 0;

/****************************************************************
 *			query_if_out_of_date			*
 ****************************************************************
 * If query_if_out_of_date is true, then the package reader     *
 * should ask the user whether to continue when it encounters   *
 * a .aso file that is out of date with respect to its source	*
 * (.ast or .asi) file(s).					*
 ****************************************************************/

Boolean query_if_out_of_date = 1;

/****************************************************************
 *			force_stdout_redirect			*
 *			force_stderr_redirect			*
 ****************************************************************
 * If force_stdout_redirect is false and the standard output    *
 * is redirected (by command line option -o) to a file that	*
 * exists, then ask the user whether to overwrite the file.     *
 * If force_stdout_redirect is true, then don't ask, just       *
 * overwrite.							*
 *								*
 * force_stderr_redirect is similar, but for redirecting stderr.*
 ****************************************************************/

Boolean force_stdout_redirect = 0;
Boolean force_stderr_redirect = 0;

/****************************************************************
 *			verbose_mode				*
 ****************************************************************
 * When verbose_mode is true, the interpreter gives extra       *
 * information about loading packages.				*
 ****************************************************************/

Boolean verbose_mode = 0;

/****************************************************************
 *			force_make				*
 ****************************************************************
 * When force_make is true, recompile files that are out of 	*
 * date without asking for permission.				*
 ****************************************************************/

Boolean force_make = 0;

/****************************************************************
 *			do_tro					*
 ****************************************************************
 * do_tro is true if the interpreter should perform		*
 * improvements, including deleting frames in tail recursive    *
 * calls and dropping environment references that will not be   *
 * used.							*
 ****************************************************************/

Boolean do_tro = 1;

/****************************************************************
 *			store_fail_act				*
 ****************************************************************
 * store_fail_act is true if the interpreter should store	*
 * fail_act at a failure.  If store_fail_act is false, then     *
 * the interpreter will not do that store.			*
 ****************************************************************/

Boolean store_fail_act;


/****************************************************************
 *			import_err				*
 ****************************************************************
 * import_err is set TRUE when there is a missing imported	*
 * package.							*
 ****************************************************************/

Boolean import_err;

/****************************************************************
 *			num_thread_switches			*
 *			num_repauses				*
 *			input_was_blocked			*
 ****************************************************************
 * num_thread_switches is the number of times a thread switch	*
 * has been done since last start in do_executes.		*
 *								*
 * num_repauses is the number of repauses that have been done	*
 * since last start in do_executes. 				*
 *								*
 * input_was_blocked is set true when a thread is blocked 	*
 * because it is waiting for input.				*
 ****************************************************************/

Boolean input_was_blocked;
LONG num_thread_switches;  
LONG num_repauses;

/****************************************************************
 *			num_threads				*
 ****************************************************************
 * num_threads is the number of threads that are currently	*
 * active.							*
 ****************************************************************/

LONG num_threads;
