/**********************************************************************
 * File:    intrprtr/intrprtr.c
 * Purpose: Implement the interpreter.  This is the top level file.
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
 * This file contains the main function for the interpreter, as well    *
 * as support for reading command line options and setting options	*
 * during execution.  Top level support for reading in the package	*
 * and those that it imports, and for processing the execute 		*
 * declarations in those packages is found here.			*
 ************************************************************************/

#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#ifdef USE_MALLOC_H
#  include <malloc.h>
#endif
#include "../misc/misc.h"
#ifndef S_ISDIR
# define	S_ISDIR(m)	(((m)&S_IFMT) == S_IFDIR)
#endif
#ifdef UNIX
# include <unistd.h>
#endif
#ifdef MSWIN
# include <io.h>
#endif
#include "../alloc/allocate.h"
#include "../utils/lists.h"
#include "../utils/strops.h"
#include "../utils/filename.h"
#include "../utils/rdwrt.h"
#include "../tables/tables.h"
#include "../clstbl/classtbl.h"
#include "../clstbl/typehash.h"
#include "../classes/classes.h"
#include "../machdata/entity.h"
#include "../machdata/except.h"
#include "../machstrc/machstrc.h"
#include "../evaluate/instruc.h"
#include "../evaluate/evaluate.h"
#include "../intrprtr/intrprtr.h"
#include "../show/profile.h"
#include "../show/prtent.h"
#include "../show/printrts.h"
#include "../show/getinfo.h"
#include "../standard/stdtypes.h"
#include "../standard/stdids.h"
#include "../rts/rts.h"
#include "../gc/gc.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif
#ifdef MSWIN
  extern char* console_cutfile_name;
  extern void setConsoleTitle(char *str);
  extern void flush_console_buff(void);
  extern void closeConsoleFont(void);
  extern void out_str(char *s);
  extern void free_profile_strings(void);
  extern void free_keyboard_buffer(void);
#endif


/****************************************************************
 *			PRIVATE VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			interrupt_level				*
 ****************************************************************
 * interrupt_level tells how many successive interrupts have	*
 * occurred.  The first INTERRUPT_COUNT interrupts are handled  *
 * by noting them and letting evaluate catch them.  After that, *
 * it is assumed that things are really bad, and the interpreter*
 * is killed.							*
 ****************************************************************/
 
PRIVATE volatile int interrupt_level = 0;

/****************************************************************
 *			hermit_build				*
 ****************************************************************
 * Array hermit_build contains instructions whose execution	*
 * puts type () on the type stack.  These instructions must     *
 * end on an END_LET_I instruction. 				*
 *								*
 * This array is initialized in init_interpreter.		*
 ****************************************************************/

PRIVATE UBYTE hermit_build[3];


/****************************************************************
 *			USAGE_ERR				*
 ****************************************************************
 * Complain about command line usage, and die.			*
 ****************************************************************/

PRIVATE void usage_err(void)
{
  die(150);
}


/****************************************************************
 *			GET_OPTION_NAME				*
 ****************************************************************
 * If opt is not an empty string then return string opt.	*
 *								*
 * If opt is empty, then return argv[*i+1] and increment *i.    *
 *								*
 * Abort on usage_err if opt is empty and *i >= argc - 1.	*
 ****************************************************************/

PRIVATE char* get_option_name(char *opt, int argc, char **argv, int *i)
{
  if(*opt != 0) {
    return opt;
  }
  else if(*i < argc -1) {
    (*i)++;
    return argv[*i];
  }
  else {
    usage_err();
    return NULL;  /* not reached */
  }
}


/****************************************************************
 *			SET_STDIN				*
 ****************************************************************
 * Set stdin_redirect to the file whose name is given by opt,   *
 * or by the next option if opt is an empty string.  When 	*
 * getting the next option, use argc, argv and i to find the	*
 * option, and increment i, as in get_option_name.		*
 ****************************************************************/

PRIVATE void set_stdin(char *opt, int argc, char **argv, int *i)
{
  struct stat stat_buf;
  stdin_redirect = get_option_name(opt, argc, argv, i);
  if(stat_file(stdin_redirect,&stat_buf) != 0) {
    die(136, stdin_redirect);
  }
  if(S_ISDIR(stat_buf.st_mode)) {
    die(154, stdin_redirect);
  }
}


/****************************************************************
 *			SET_PRECISION				*
 ****************************************************************
 * Set default_precision to the integer given by opt,           *
 * or by the next option if opt is an empty string.  When 	*
 * getting the next option, use argc, argv and i to find the	*
 * option, and increment i, as in get_option_name.		*
 ****************************************************************/

PRIVATE void set_precision(char *opt, int argc, char **argv, int *i)
{
  LONG prec;
  char* theopt = get_option_name(opt, argc, argv, i);
  if(sscanf(theopt, "%ld", &prec) > 0) {
    default_precision   = toint(prec);
    default_digits_prec = prec_digits(default_precision); 
  }
  else usage_err();
}


/****************************************************************
 *			SET_LIMIT				*
 ****************************************************************
 * Set variable *v (which will be either max_st_depth or	*
 * max_heap_bytes) to factor times the value given starting at  *
 * opt, unless that is an empty string.  When opt is empty, use *
 * argv[i+1] and increment i, provided i+1 < argc.		*
 *								*
 * If opt begins with a '+', then set *v to LONG_MAX,		*
 * indicating that the limit is suppressed.			*
 ****************************************************************/

PRIVATE void 
set_limit(LONG *v, char *opt, int argc, char **argv, int* i, int factor)
{
  LONG n;
  char* theopt = get_option_name(opt, argc, argv, i);

  if(theopt[0] == '+') *v = LONG_MAX;
  else {
    if(sscanf(theopt, "%ld", &n) != 1) usage_err();
    else *v = factor*n;
  }
}


/*********************************************************************
 *			DYN_SET_LIMIT				     *
 *********************************************************************
 * set variable v to the value indicated by e.  Similar to	     *
 * set_limit, but the limit is given by an entity rather than by a   *
 * string.							     *
 *********************************************************************/

PRIVATE ENTITY dyn_set_limit(LONG *v, ENTITY e)
{
  *v = (TAG(e) == INT_TAG) ? IVAL(e) : LONG_MAX;
  return hermit;
} 


/*********************************************************************
 *			DYN_GET_LIMIT				     *
 *********************************************************************
 * Return the value of limit varible v, as an entity.  If it is      *
 * "infinite", return a vary large integer.			     *
 *********************************************************************/

PRIVATE ENTITY dyn_get_limit(LONG v)
{
  return (v == LONG_MAX) ? a_large_int : ENTI(v);
}


/*********************************************************************
 *			SET_STACK_LIMIT				     *
 *********************************************************************
 * Set the stack limit to e.  					     *
 *********************************************************************/

ENTITY set_stack_limit(ENTITY e) 
{
  return dyn_set_limit(&max_st_depth, e);
}


/*********************************************************************
 *			GET_STACK_LIMIT				     *
 *********************************************************************
 * Return the current stack limit.				     *
 *********************************************************************/

ENTITY get_stack_limit(ENTITY herm_unused) 
{
  return dyn_get_limit(max_st_depth);
}


/*********************************************************************
 *			SET_HEAP_LIMIT				     *
 *********************************************************************
 * Set the heap limit to e.					     *
 *********************************************************************/

ENTITY set_heap_limit(ENTITY e) 
{
  return dyn_set_limit(&max_heap_bytes, e);
}


/*********************************************************************
 *			GET_HEAP_LIMIT				     *
 *********************************************************************
 * Return the current heap limit.				     *
 *********************************************************************/

ENTITY get_heap_limit(ENTITY herm_unused) 
{
  return dyn_get_limit(max_heap_bytes);
}


/****************************************************************
 *			CLEAN_UP_AND_EXIT			*
 ****************************************************************
 * Exit the interpreter, returning val to the operating system. *
 * Clean up first -- close open files, etc.			*
 ****************************************************************/

void clean_up_and_exit(int val)
{
  /*-----------------------------------------*
   * Print profile information if necessary. *
   *-----------------------------------------*/

  print_profile();

  /*--------------------------------------------------------*
   * Flush the console output for MSWIN and delete the temp *
   * file for console buffer overflow.			     *
   *--------------------------------------------------------*/

# ifdef MSWIN
    flush_console_buff();
    unlink_file(console_cutfile_name);
# endif

  /*-------------------------------------*
   * Close all open files in file_table. *
   *-------------------------------------*/

  close_all_open_files();

  /*----------------------------------------------------*
   * Close all open files in current_pack_params chain. *
   *----------------------------------------------------*/

  {PACK_PARAMS *p;
   for(p = current_pack_params; p != NULL; p = p->parent) {
     if(p->packfile != NULL) fclose(p->packfile);
   }
  }

  /*--------------------------------*
   * Close TRACE_FILE and rts_file. *
   *--------------------------------*/

  if(rts_file != NULL) fclose(rts_file);
# ifdef DEBUG
    if(TRACE_FILE != NULL) fclose(TRACE_FILE);
# endif

  /*-----------------------*
   * Close font for MSWIN. *
   *-----------------------*/

# ifdef MSWIN
    closeConsoleFont();
    close_all_open_fonts();
# endif

  /*---------------------------------*
   * Free allocated space for MSWIN. *
   *---------------------------------*/

# ifdef MSWIN
    free_all_blocks();
    free_profile_strings();
    free_keyboard_buffer();
# endif

  /*------------------------------------------------------------*
   * When running this program in instrumented mode, write	*
   * the instrument data now.					*
   *------------------------------------------------------------*/

# ifdef INSTRUMENT
    zzwrite();
# endif

# ifdef MSWIN
    exit(0);
# else
    exit(val);
# endif
}


/****************************************************************
 *			INTERRUPT HANDLING			*
 ****************************************************************
 * Function intrupt fields an interrupt.  Normally, that	*
 * involves setting interrupt_occurred.  The evaluator should	*
 * see this, and take action.  But if more than 		*
 * INTERRUPT_COUNT interrupts are done in a row without an	*
 * interrupt being handled, then the interpreter is killed	*
 * instead.							*
 *								*
 * handle_interrupt is the interrupt handler that is called by  *
 * the evaluator when it sees the interrupt.			*
 * 								*
 * Interrupts are handled this way so that the evaluator has	*
 * a chance to bring things to a stable state before the	*
 * interrupt is processed.					*
 *								*
 * segviol handles a memory protection fault.			*
 ****************************************************************/

void intrupt(int signo_unused)
{
  signal(SIGINT, SIG_IGN);
  if(!interrupt_occurred) {
    interrupt_occurred = TRUE;
    special_condition++;
  }
  interrupt_level++;
  if(interrupt_level <= INTERRUPT_COUNT) signal(SIGINT, intrupt);
  else signal(SIGINT, SIG_DFL);
}

/*---------------------------------------------------------*/

void handle_interrupt(void)
{
  if(interrupt_level <= INTERRUPT_COUNT) {
    char *message;
#   ifdef MSWIN
      message = "Interrupt\nContinue?";
#   else
      message = "Interrupt\n";
#   endif
    possibly_abort_dialog(message);
    interrupt_level    = 0;
    interrupt_occurred = FALSE;
    special_condition--;
    signal(SIGINT, intrupt);  
  }

  else {
    clean_up_and_exit(1);
  }
}

/*---------------------------------------------------------*/

#ifdef MSWIN
  void indicate_pause(void)
  {
    if(!should_pause) {
      special_condition++;
      should_pause = TRUE;
    }
  }

  void indicate_resume(void)
  {
    if(failure == PAUSE_EX) failure = -1;
  }
#endif

/*---------------------------------------------------------*/

#ifdef UNIX
PRIVATE void basicsegviol(int signo_unused)
{
  die(152);
}

/*---------------------------------------------------------*/

PRIVATE int segviolflag = 0;

void segviol(int signo_unused)
{
  if(segviolflag == 0) {
    segviolflag = 1;
    signal(SIGSEGV, segviol);
#   ifdef UNIX
      fprintf(stderr, "Segmentation violation\n");
      fflush(stdout);
      fflush(stderr);
#   endif

#   ifdef DEBUG
      if(TRACE_FILE != NULL) fflush(TRACE_FILE);
#   endif

    print_rts(&the_act, ENTU(INTERRUPT_EX), 0, 0);
    clean_up_and_exit(1);
  }
  else {
    if(segviolflag == 2) {
      signal(SIGSEGV, SIG_DFL);
      clean_up_and_exit(1);
    }
    segviolflag++;
    signal(SIGSEGV, segviol);
    die(153);
  }
}
#endif


/****************************************************************
 *			INIT_INTERPETER				*
 ****************************************************************
 * Do the main initialization for the interpreter.		*
 ****************************************************************/

PRIVATE void init_interpreter(void)
{
  init_io();
  runtime_shadow_st = NIL;
# ifndef SMALL_ENTITIES
    init_basic_entities();
# endif
  init_temp_strs();
  init_temp_str_utils();
  init_str_tb();
  init_std_ids();
  init_globals();
  init_class_tbl_tm();
  init_types();
  init_type_tb();
  init_exception_names();
  init_real();
  init_entity();
  init_state();
  init_stack();
  init_evaluate();
  init_package_reader();
  init_mono_globals();
  hermit_build[0] = STAND_T_I;
  hermit_build[1] = Hermit_std_num;
  hermit_build[2] = END_LET_I;
  NULL_CTL_OR_ACT.ctl = NULL;
}

/****************************************************************
 *			COMMAND LINE OPTIONS			*
 ****************************************************************
 * The following are used to hold command line options.		*
 ****************************************************************/

PRIVATE Boolean 
  stop_with_visualizer = 0,   /* At end, start the visualizer */

  start_visualizer = 0,       /* Start visualizer immediately */

  main_do_overlaps = 1,	      /* Do overlap tests while reading packages */

  do_execs = 1;		      /* Do execute declarations */

#ifdef DEBUG
  Boolean 
    trace_read = 0, 
    main_trace = 0, 
    show_package = 0, 
    show_standard = 0,
    main_trace_puts = 0, 
    main_trace_env_descr = 0;
#endif


/****************************************************************
 *		   SHOW_ASTR_OPTIONS				*
 ****************************************************************
 * Show the options to astr.					*
 ****************************************************************/

PRIVATE void show_astr_options(void)
{
  char filename[MAX_FILE_NAME_LENGTH];
  strcpy(filename, MESSAGE_DIR);
  strcat(filename, ASTR_OPTION_FILE);
  print_file(filename);
}


/****************************************************************
 *			HANDLE_OPTIONS				*
 ****************************************************************
 * Handle command line options to astr.  Return the name of the *
 * program to be interpreted.  Set theCommandLine to the	*
 * command  line for the interpreted program.   Don't return at *
 * all if there is an error on the command line -- just		*
 * complain and exit.						*
 ****************************************************************/

PRIVATE Boolean do_help    = FALSE;
PRIVATE Boolean do_version = FALSE;

PRIVATE char* handle_options(int argc, char **argv)
{
  int i;
  char *opt, *program_name;
  Boolean have_read_program_name = FALSE;
  STR_LIST* cmdline = NIL;

  program_name = NULL;
  for(i = 1; i < argc; i++) {
    opt = argv[i];

    /*-----------------------------------------------------------*
     * If the program name hasn't been read yet, then this is an *
     * astr option or the program name.				 *
     *-----------------------------------------------------------*/

    if(!have_read_program_name) {

      /*------------------------------------*
       ***** Check for the program name *****
       *------------------------------------*/

      if(opt[0] != '-') {
	program_name = opt; 
	have_read_program_name = TRUE;
        bump_list(cmdline = str_cons(opt, NIL));
      }

      /*-----------------------*
       ***** Basic options *****
       *-----------------------*/

      /*------------------------------------------------------*
       * -d, --debug-at-trap : Start the visualizer at a trap *
       *------------------------------------------------------*/

      else if(strcmp(opt, "-d") == 0 || strcmp(opt, "--debug-at-trap") == 0) {
	start_visualizer_at_trap = TRUE;
      }

      /*-------------------------------------------*
       * -e file,				   *
       * -e! file,				   *
       * --stderr file : Redirect standard error.  *
       * --stderr! file : Redirect standard error. *
       *-------------------------------------------*/

      else if(prefix("-e", opt)) {
	if(opt[2] == '!') {
	  stderr_redirect = get_option_name(opt+3, argc, argv, &i);
	  force_stderr_redirect = TRUE;
	}
	else {
	  stderr_redirect = get_option_name(opt+2, argc, argv, &i);
	}
      }
      else if(prefix("--stderr", opt)) {
	if(opt[8] == '!') {
	  stderr_redirect = get_option_name(opt+9, argc, argv, &i);
	  force_stderr_redirect = TRUE;
	}
	else {
	  stderr_redirect = get_option_name(opt+8, argc, argv, &i);
	}
      }

      /*------------------------------------------------------*
       * -f, --no-check : Don't query about out-of-date files. *
       *------------------------------------------------------*/

      else if(strcmp(opt, "-f") == 0 || strcmp(opt, "--no-check") == 0) {
	query_if_out_of_date = 0;
      }

      /*--------------------------------------------*
       * -hN, --heap-limitN : Set heap limit to N.  *
       * -h+, --heap-limitN : Disable heap limit.   *
       *--------------------------------------------*/

      else if(prefix("-h", opt)) {
	set_limit(&max_heap_bytes, opt+2, argc, argv, &i, 1000);
      }
      else if(prefix("--heap-limit", opt)) {
	set_limit(&max_heap_bytes, opt+12, argc, argv, &i, 1000);
      }

      /*--------------------------------------------------------*
       * --help : 						*
       *    Ask for usage information.			 	*
       *--------------------------------------------------------*/

      else if(strcmp(opt, "--help") == 0) {
        do_help = TRUE;
      }

      /*------------------------------------------*
       * -i file,				  *
       * --stdin file : Redirect standard input.  *
       *------------------------------------------*/

      else if(prefix("-i", opt)) {
	set_stdin(opt+2, argc, argv, &i);
      }
      else if(prefix("--stdin", opt)) {
	set_stdin(opt+7, argc, argv, &i);
      }

      /*--------------------------------------------------*
       * -m, --make : Compile other files without asking. *
       *--------------------------------------------------*/

      else if(strcmp(opt, "-m") == 0 || strcmp(opt, "--make") == 0) {
	force_make = TRUE;
      }

      /*----------------------------------------*
       * -n, --no-run : Suppress doing executes. *
       *----------------------------------------*/

      else if(strcmp(opt, "-n") == 0 || strcmp(opt, "--no-run") == 0) {
	do_execs = FALSE;
      }

      /*--------------------------------------------*
       * -o file,				    *
       * -o! file,				    *
       * --stdout file : Redirect standard output.  *
       * --stdout! file : Redirect standard output. *
       *--------------------------------------------*/

      else if(prefix("-o", opt)) {
	if(opt[2] == '!') {
	  stdout_redirect = get_option_name(opt+3, argc, argv, &i);
	  force_stdout_redirect = TRUE;
	}
	else {
	  stdout_redirect = get_option_name(opt+2, argc, argv, &i);
	}
      }
      else if(prefix("--stdout", opt)) {
	if(opt[8] == '!') {
	  stdout_redirect = get_option_name(opt+9, argc, argv, &i);
	  force_stdout_redirect = TRUE;
	}
	else {
	  stdout_redirect = get_option_name(opt+8, argc, argv, &i);
	}
      }

      /*-------------------------------------------------*
       * -pN, --precisionN : Set default precision to N. *
       *-------------------------------------------------*/

      else if(prefix("-p", opt)) {
	set_precision(opt+2, argc, argv, &i);
      }
      else if(prefix("--precision", opt)) {
	set_precision(opt+11, argc, argv, &i);
      }

      /*--------------------------------*
       * --profile : Turn on profiling. *
       *--------------------------------*/

      else if(strcmp(opt, "--profile") == 0) {
	do_profile = special_condition = TRUE;
      }

      /*---------------------------------------------------------*
       * -q, --quick-load : Quick load.  Don't do overlap tests. *
       *---------------------------------------------------------*/

      else if(strcmp(opt, "-q") == 0 || strcmp(opt, "--quick-load") == 0) {
	main_do_overlaps = FALSE;
      }

      /*----------------------------------------------------------*
       * -rtN, --dump-sizeN: Set ast.rts total size limit to N.	  *
       *----------------------------------------------------------*/

      else if(prefix("-rt", opt)) {
	LONG kk;
        char* theopt = get_option_name(opt+3, argc, argv, &i);
	if(sscanf(theopt, "%ld", &kk) <= 0) usage_err();
        else rts_file_max_chars = 1000*kk;
      }
      else if(prefix("--dump-size", opt)) {
	LONG kk;
        char* theopt = get_option_name(opt+11, argc, argv, &i);
	if(sscanf(theopt, "%ld", &kk) <= 0) usage_err();
        else rts_file_max_chars = 1000*kk;
      }

      /*----------------------------------------------------------*
       * -riN, --dump-value-size : Set ast.rts individual size    *
       * limit to N.		  				  *
       *----------------------------------------------------------*/

      else if(prefix("-ri", opt)) {
	char* theopt = get_option_name(opt+3, argc, argv, &i);
	if(sscanf(theopt, "%ld", &rts_file_max_entity_chars) <= 0) {
	  usage_err();
	}
      }
      else if(prefix("--dump-value-size", opt)) {
	char* theopt = get_option_name(opt+17, argc, argv, &i);
	if(sscanf(theopt, "%ld", &rts_file_max_entity_chars) <= 0) {
	  usage_err();
	}
      }

      /*---------------------------------------------*
       * -sN, --stack-limitN : Set stack limit to N.  *
       * -s+, --stack-limit+ : Disable stack limit.   *
       *---------------------------------------------*/

      else if(prefix("-s", opt)) {
	set_limit(&max_st_depth, opt+2, argc, argv, &i, 1);
      }
      else if(prefix("--stack-limit", opt)) {
	set_limit(&max_st_depth, opt+12, argc, argv, &i, 1);
      }

      /*-----------------------------------------------------------------*
       * -t, --no-optimize: Don't do tail recursion improvement or         *
       * environment pruning. 						 *
       *-----------------------------------------------------------------*/

      else if(strcmp(opt, "-t") == 0 || strcmp(opt, "--no-optimize") == 0) {
	do_tro = FALSE;
      }

      /*-------------------------------------*
       * -v, --debug : Start the visualizer. *
       *-------------------------------------*/

      else if(strcmp(opt, "-v") == 0 || strcmp(opt, "--debug") == 0) {
	start_visualizer          = TRUE;
        stop_with_visualizer      = TRUE;
	break_info.break_executes = TRUE;
        break_info.show_val       = TRUE;
	do_tro                    = FALSE;
      }

      /*--------------------------------------------------------*
       * --version : 						*
       *    Ask for version information.			*
       *--------------------------------------------------------*/

      else if(strcmp(opt, "--version") == 0) {
        do_version = TRUE;
      }

      /*--------------------------------------------------------*
       * --verbose : 						*
       *    Ask for verbose output.				*
       *--------------------------------------------------------*/

      else if(strcmp(opt, "--verbose") == 0) {
	verbose_mode = TRUE;
      }

      /*-----------------------*
       ***** Debug options *****
       *-----------------------*/

#     ifdef DEBUG
        else if(prefix("-T", opt)) {
	  TRACE_FILE = fopen(opt+2, TEXT_WRITE_OPEN);
	  if(TRACE_FILE == NULL) die(19);
        }
        else if(prefix("-D", opt)) {
  	  read_debug_messages();
	  if(strcmp(opt, "-D") == 0) main_trace = 1;
	  else if(strcmp(opt, "-DR") == 0) trace_read = 1;
	  else if(strcmp(opt, "-Dshow") == 0) show_package = TRUE;
	  else if(strcmp(opt, "-DshowS") == 0) show_standard = TRUE;
	  else if(strcmp(opt, "-DTP") == 0) main_trace_puts = TRUE;
	  else if(strcmp(opt, "-DEN") == 0)  main_trace_env_descr = TRUE;
	  else if(prefix("-DF", opt) && strlen(opt) > 3) {
	    SET_LIST(trace_fun, str_cons(opt + 3, trace_fun));
	  }
	  else if(!set_interpreter_debug(opt+2)) {
	    usage_err();
	  }
        }
#     endif

      else usage_err();
    }

    /*-----------------------------------------------------*
     * If the program name has already been read, then the *
     * remaining options are for the program itself. Add   *
     * the program name as argument 0. 			   *
     *-----------------------------------------------------*/

    else SET_LIST(cmdline, str_cons(argv[i], cmdline));
  } /* end for(i = ...) */

  if(program_name == NULL) usage_err();

  /*------------------------------------------------------*
   * We built the comand line backwards.  Reverse it, and *
   * build theCommandLine from it.                        *
   *------------------------------------------------------*/

  SET_LIST(cmdline, reverse_list(cmdline));
  theCommandLine = list_to_entlist(cmdline);
  drop_list(cmdline);

  return program_name;
}


/********************************************************
 *		READ_STANDARD_PACKAGE			*
 ********************************************************
 * Read package standard.aso.  If it does not exist,    *
 * complain and exit -- don't return.			*
 ********************************************************/

PRIVATE void read_standard_package(void)
{
  char fname[MAX_FILE_NAME_LENGTH];
  char *ffname;
  strcpy(fname, STD_DIR);
  strcat(fname, INTERNAL_DIR_SEP_STR);
  strcat(fname, STANDARD_AST_NAME ".aso");
  force_internal(fname);

# ifdef DEBUG
    if(trace) trace_i(189);
# endif

  do_overlap_tests = FALSE;
  ffname = full_file_name(fname, 0);
  if(ffname == NULL) {
    die(136, fname);
  }
  else if(read_package(ffname,1,FALSE) != 1) {
    clean_up_and_exit(1);
  }
}


/********************************************************
 *		READ_ARGUMENT_PACKAGE			*
 ********************************************************
 * Read file this_program.  Abort if not possible.	*
 ********************************************************/

PRIVATE void read_argument_package(char *this_program)
{
  char *aso_path_var;
  STR_LIST *aso_path, *dirlist;
  char* this_program_cpy = BAREMALLOC(MAX_FILE_NAME_LENGTH + 1);
  strcpy(this_program_cpy, this_program);
  force_internal(this_program_cpy);

  /*---------------------------------------------*
   * Get the search path for finding .aso files. *
   *---------------------------------------------*/

  aso_path_var = getenv("ASOPATH");
  bump_list(aso_path =
    (aso_path_var == NULL) ? str_cons(".", NIL)
			   : file_colonsep_to_list(aso_path_var));

  /*--------------------------------------------------------*
   * If the program name is absolute, then don't modify it. *
   *--------------------------------------------------------*/

  if(is_absolute(this_program_cpy)) {
    int status;
    install_standard_dir(this_program_cpy, MAX_FILE_NAME_LENGTH);
#   ifdef UNIX
      install_home_dir(this_program_cpy, MAX_FILE_NAME_LENGTH);
#   endif
    main_program_file_name = full_file_name(this_program_cpy, 0);
    status = read_package(main_program_file_name, 1, TRUE);
    if(status == 1) goto out;
    if(status < 0) clean_up_and_exit(1);
  }

  /*-------------------------------------------------------*
   * If not absolute, then try each directory in aso_path. *
   *-------------------------------------------------------*/

  else {
    dirlist = aso_path;
    do {
      char* ffname = 
        concat_string(concat_string(dirlist->head.str, INTERNAL_DIR_SEP_STR),
		      this_program_cpy);
      main_program_file_name = get_full_file_name(ffname, 0);

#     ifdef DEBUG
	if(trace || trace_read || main_trace) {
	  trace_i(191, nonnull(main_program_file_name));
	}
#     endif

      if(main_program_file_name != NULL) {
	int status = read_package(main_program_file_name,-1,TRUE);
	if(status == 1) goto out;
	else if(status < 0) clean_up_and_exit(1);
      }
      dirlist = dirlist->tail;
    } while (dirlist != NIL);
  }

  /*-------------------------------------------*
   * Here, no suitable file was found.  Abort. *
   *-------------------------------------------*/

  if(!import_err) {
    die(136, aso_name(this_program));
  }
  FREE(this_program_cpy);
  clean_up_and_exit(1);

  /*------------------------------------*
   * Normal exit after successful read. *
   *------------------------------------*/

 out:
  drop_list(aso_path);
  FREE(this_program_cpy);
  return;
}

/********************************************************
 *		START_DEBUGGER_BEFORE_EXECUTES		*
 ********************************************************
 * Indicate that the debugger should be run before      *
 * any executes are done, and prepare the debugger.	*
 * This involves asking the user which package to debug.*
 ********************************************************/

#ifdef UNIX

PRIVATE void start_debugger_before_executes(void)
{
  char pack_name[MAX_NAME_LENGTH + 1];
  char yesno[5];

  read_trap_msgs();

 again:
  fprintf(stderr, trap_msg[26], nonnull(main_package_name));
  get_name_rest_of_line(pack_name, MAX_NAME_LENGTH, ' ');
  if(pack_name[0] == 0) strcpy(pack_name, main_package_name);
  else if(strcmp(pack_name, "none") != 0 &&
	  get_pd_entry_by_package_name(pack_name) == NULL) {
    fprintf(stderr, trap_msg[47], pack_name);
    goto again;
  }

  bump_list(break_info.break_execute_packages = 
	    str_cons(stat_str_tb(pack_name), NIL));
  fprintf(stderr, trap_msg[29], pack_name);
  get_name_rest_of_line(yesno, 5, ' ');
  if(yesno[0] == 'y' || yesno[0] == 0) {
    SET_LIST(break_info.breakable_fun_packages, 
	     str_cons(stat_str_tb(pack_name), 
		      break_info.breakable_fun_packages));
    break_info.break_applies = TRUE;
  }
}

#endif

/********************************************************
 *			MAIN				*
 ********************************************************/

#ifdef MSWIN
 int astrmain(int argc, char **argv, char **env)
#else
 int main(int argc, char **argv, char **env)
#endif
{

  /*---------------------------------------------------------------------*
   ***** If there are no arguments, then show the command line       *****
   ***** options.						     *****
   *---------------------------------------------------------------------*/

# ifdef UNIX
    if(argc == 1) {
      show_astr_options();
      return 0;
    }
# endif

  /*----------------------------------------------------*
   * If this is an instrumented run, then set up	*
   * the instrumentation.				*
   *----------------------------------------------------*/

# ifdef INSTRUMENT
    zzinit();
# endif

  /*---------------------*
   * Set up for tracing. *
   *---------------------*/

# ifdef DEBUG
#   ifdef UNIX
      TRACE_FILE = stdout;
#   else
      TRACE_FILE = NULL;
#   endif
    trace           = 0;
# endif

  os_environment = env;

  /*-------------------------------------------------------------*
   * We must initialize storage allocation immediately, since it *
   * is needed early.						 *
   *-------------------------------------------------------------*/

  init_alloc();
  gc_init_alloc();
  init_gc();

  /*----------------------------*
   * Read command line options. *
   *----------------------------*/

  if(argc < 2) {usage_err();}

  init_break_info();
  main_program_name = handle_options(argc, argv);

# ifdef UNIX
    if(do_help) {usage_err();}
    if(do_version) {
      printf("astr version %s\n", ASTC_VERSION);
      return 0;
    }
# endif

# ifdef MSWIN
    setConsoleTitle(main_program_name);
# endif

  /*-----------------------------*
   * Initialize the interpreter. *
   *-----------------------------*/

  init_interpreter();

  /*----------------------------*
   * Read the standard package. *
   *----------------------------*/

  read_standard_package();

# ifdef DEBUG
    if(show_standard) show();
# endif

  /*----------------------------*
   * Read the argument package. *
   *----------------------------*/

# ifdef DEBUG
    if(trace) trace_i(190);
    trace            = trace_read;
    trace_puts       = main_trace_puts;
    trace_env_descr  = main_trace_env_descr;
# endif

  do_overlap_tests = main_do_overlaps;
  import_err = FALSE;
  read_argument_package(main_program_name);
  end_read_package(TRUE);

# ifdef DEBUG
    if(trace) trace_i(192);
# endif

# ifdef DEBUG
    if(show_package) show();
# endif

  /*---------------------------------------------------------------*
   * Initialize the exceptions.  This was not done earlier because *
   * we needed the argument package info first.			   *
   *---------------------------------------------------------------*/

  init_exceptions();

  /*------------------------------------------------------------*
   * Start the debugger if requested.  Here, we only ask what	*
   * package to start in.  The debugger is actually started 	*
   * in do_executes, below.					*
   *------------------------------------------------------------*/

# ifdef UNIX
    if(start_visualizer) {
      start_debugger_before_executes();
    }
# endif

  /*------------------*
   * Do the executes. *
   *------------------*/

# ifdef DEBUG
    trace = main_trace;
# endif

# ifdef UNIX
    signal(SIGSEGV, basicsegviol);
# endif

  if(do_execs) {
    int doexec_result;
    doexec_result = do_executes();
    print_profile();
#   ifdef UNIX
      if(doexec_result) clean_up_and_exit(1);
#   endif
  }

  /*------------------------------------------------------*
   * Go to the visualizer if requested after all executes *
   * are finished.					  *
   *------------------------------------------------------*/

  if(stop_with_visualizer) vis_break(DONE_VIS);

# ifdef UNIX
    clean_up_and_exit(0);
# endif

# ifdef MSWIN
    flush_console_buff();
# endif

  return 0;
}


/*********************************************************************
 *		      SET_UP_THE_ACT_FOR_EXECUTE		     *
 *********************************************************************
 * Prepare the_act for performing an execute whose executable code   *
 * (after global stuff) begins at address c, with global environment *
 * genv.							     *
 *********************************************************************/

PRIVATE void set_up_the_act_for_execute(CODE_PTR c, ENVIRONMENT *genv)
{
  int size, descr_num;

  in_show               	= 0;
  size 		     		= *(c++);
  descr_num            		= toint(next_int_m(&c));
  the_act.name          	= "Execute";
  the_act.pack_name		= NULL;
  the_act.result_type_instrs 	= hermit_build;
  the_act.program_ctr   	= c;
  the_act.continuation  	= NULL;
  the_act.control       	= NULL;
  the_act.embedding_tries	= NULL;
  the_act.embedding_marks	= NULL;
  the_act.coroutines    	= NIL;
  the_act.in_atomic     	= 0;
  the_act.stack	     		= new_stack();
  the_act.type_binding_lists	= NIL;
  the_act.exception_list 	= NIL;
  the_act.st_depth 	  	= 0;
  the_act.kind          	= 0;
  the_act.state_hold    	= NIL;
  the_act.trap_vec_hold 	= NIL;

  bump_state(the_act.state_a = execute_state);
  bump_trap_vec(the_act.trap_vec_a = global_trap_vec);

  if(size > 0) {
    the_act.env                   = allocate_local_env(size);
    the_act.env->link             = genv;
    the_act.env->num_link_entries = (genv == NULL) ? 0 : genv->most_entries;
    the_act.env->descr_num        = descr_num;
    the_act.num_entries           = 0;
  }
  else {
    the_act.env         = genv;
    the_act.num_entries = (genv == NULL) ? 0 : genv->most_entries;
  }
}


/*********************************************************************
 *			DO_EXECUTES				     *
 *********************************************************************
 * Perform all of the execute declarations that are recorded in	     *
 * array executes.						     *
 *								     *
 * Return 0 if the execute succeeded, and 1 if it failed.	     *
 *********************************************************************/

int do_executes()
{
  int j;
  LONG t;
  CODE_PTR c;
  ENVIRONMENT *genv;

  signal(SIGINT, intrupt);
# ifdef UNIX
    signal(SIGSEGV, segviol);
# endif

  for(j = 0; j < num_executes; j++) {
    int deadlock_rounds = 0;
    int input_blocked_rounds = 0;
    Boolean there_was_a_success;

    /*---------------------------------------------------------*
     * Get the necessary globals for this execute declaration. *
     *---------------------------------------------------------*/

    clear_type_stk();
    c    = package_descr[executes[j].packnum].begin_addr + executes[j].offset;
    genv = setup_globals(&c, NULL); /* Ref from setup_globals */
    clear_type_stk();

    /*------------------------*
     * Set up the activation. *
     *------------------------*/

    set_up_the_act_for_execute(c, genv);

    /*------------------------------------------------------------------*
     * Clear suppress_print_rts_in_abort so that possibly_abort_dialog  *
     * will show the_act.						*
     *------------------------------------------------------------------*/

    suppress_print_rts_in_abort = FALSE;

    /*--------------------------------------------------------------*
     * Run this activation. We need to run it in a loop because the *
     * activation can time-out, and need to be continued from here. *
     *--------------------------------------------------------------*/

    num_threads = 1;
    t = LONG_MAX;
#   ifdef DEBUG
      if(time_out_often) t = 100;
#   endif
    fail_act.program_ctr = NULL;

    /*----------------------------------------------------------*
     * If requested, break to the debugger at the start of      *
     * the execute.  But don't do this for a hidden execute.	*
     *----------------------------------------------------------*/
 
    if(break_info.break_executes && !break_info.suppress_breaks &&
       !executes[j].hidden) {
      char *pack_name, *file_name;
      int line;

      get_line_info(the_act.program_ctr, &pack_name, &file_name, &line);
      if(str_member(pack_name, break_info.break_execute_packages)) {
	break_info.pc = the_act.program_ctr;
	vis_break(EXEC_BEGIN_VIS);
      }
    }

    /*------------------------------------------------------------------*
     * Start this execute, and keep continuing it until it finishes	*
     * without having been timed-out.					*
     *------------------------------------------------------------------*/

    there_was_a_success = FALSE;
    do {

      /*----------------------------------------------------------------*
       * The following are used to detect deadlock, or other cases 	*
       * where all threads are waiting.	num_thread_switches tells how   *
       * many threads switches were done, including the one that times  *
       * out the last thread and returns control here.  num_repauses 	*
       * tells how many of those thread switches were due to repauses	*
       * or blocked input, and input_was_blocked is true if any of 	*
       * those switches were from block input.				*
       *----------------------------------------------------------------*/

      num_thread_switches = num_repauses = 0;
      input_was_blocked = FALSE;

      /*------------------------------------------------------*
       * Clear fail_act, in case it has junk left over in it. *
       *------------------------------------------------------*/

      if(fail_act.program_ctr != NULL) {
	drop_activation_parts(&fail_act);
        fail_act.program_ctr = NULL;
      }

#     ifdef DEBUG
        if(trace) trace_i(193);
#     endif

      /*-------------------------*
       * Perform the evaluation. *
       *-------------------------*/

      program_is_running++;
      evaluate(&t);
      program_is_running--;

      t = LONG_MAX;
#     ifdef DEBUG
        if(time_out_often) t = 100;
#     endif

      /*----------------------------------------------------------------*
       * If one thread has succeeded and there are other threads	*
       * waiting, then run the other threads.				*
       *								*
       * If this is the first thread to reach the end of this execute,  *
       * then record its state for the next execute.			*
       *								*
       * do_failure can return FALSE either because there is nowhere to *
       * resume or because it has decided to time out.			*
       *----------------------------------------------------------------*/

      if(failure < 0) {
        if(!there_was_a_success) {
	  SET_STATE(execute_state, the_act.state_a);
	}
	there_was_a_success = TRUE;
	if(the_act.control != NULL) {
	  failure = ENDTHREAD_EX;
	  if(!do_failure(the_act.control, 0, &t)) {
	    failure = TIME_OUT_EX;
	    continue;
          }
  	  else if(failure != TIME_OUT_EX) failure = -1;
	}
      }

      /*----------------------------------------------------------------*
       * If all threads are blocked, sleep when at least one is waiting *
       * for input, and report deadlock when all did repauses.	  	*
       * Actually, wait for two consecutive rounds of this, since it    *
       * is possible for a delayed repause to free something for the    *
       * next round.							*
       *----------------------------------------------------------------*/

      if(failure == TIME_OUT_EX) {

#       ifdef DEBUG
          if(trace) {
	    trace_i(313, num_thread_switches, num_repauses, 
		    input_was_blocked, input_blocked_rounds, deadlock_rounds);
	  }
#       endif

	if(num_thread_switches == num_repauses) {
	  if(input_was_blocked) {
#           ifdef UNIX
	      if(input_blocked_rounds > 0) {
#               ifdef DEBUG
                  if(trace) trace_i(315);
#               endif
	        sleep(SLEEP_SECONDS);
	      }
	      input_blocked_rounds++;
#           endif
	  }
	  else {
	    if(deadlock_rounds > 0) {
	      deadlock_complain();
	      goto out;
	    }
	    deadlock_rounds++;
	  }
	}
	else deadlock_rounds = input_blocked_rounds = 0;

	count_threads(timed_out_kind, timed_out_comp);

#       ifdef DEBUG
          if(trace) trace_i(314, num_threads);
#       endif

	if(num_threads > 0) {
          start_ctl_or_act(timed_out_kind, timed_out_comp, NULL, &t);
        }
      } /* end if(failure == TIME_OUT_EX) */

      drop_ctl_or_act(timed_out_comp, timed_out_kind);
      timed_out_comp.ctl = NULL;
      timed_out_kind = CTL_F;
    } while(failure == TIME_OUT_EX);

#   ifdef NEVER
      if(break_info.break_executes) vis_break(EXEC_END_VIS);
#   endif

    /*--------------------------------------------*
     * Report failure and dump the configuration  *
     * if none of the threads succeeded.	  *
     *--------------------------------------------*/

    if(!there_was_a_success && failure >= 0) {
      ENTITY hold_fail_ex;
      ACTIVATION hold_fail_act;
      REG_TYPE fmark;

      if(failure == TRAPPED_EX) return 1;  /* Already reported. */

      fmark = reg1(&hold_fail_ex);
      hold_fail_ex = fail_ex;
      flush_stdout();
      failure = -1;   /* Clear so that print_entity_with_state will go */

      /*------------------------------------------------------------*
       * Print the exception that happened.  This calls evaluate,   *
       * which will destroy fail_act.  So hold onto fail_act first. *
       *------------------------------------------------------------*/

      if(fail_act.program_ctr != NULL) {
	hold_fail_act = fail_act;
	bump_activation_parts(&hold_fail_act);
      }
      else hold_fail_act.program_ctr = NULL;

      ASTR_ERR_MSG("Execution has failed with exception ");
      print_entity_with_state(failure_as_entity, exception_type, STDERR,
			      execute_state, no_trap_tv, 0, 1000);
      ASTR_ERR_MSG("\n");
      print_exception_description(STDERR, hold_fail_ex);
      ASTR_ERR_MSG("\n");

      /*----------------------------------------------------*
       * Dump the configuration where the failure occurred. *
       * But first put hold_fail_act back into fail_act.    *
       * This is necessary because print_rts does a garbage *
       * collection, and the garbage collector will mark    *
       * the things accessible to fail_act, but not those   *
       * accessible to hold_fail_act.			    *
       *----------------------------------------------------*/

      if(hold_fail_act.program_ctr != NULL) {
        if(fail_act.program_ctr != NULL) drop_activation_parts(&fail_act);
        fail_act = hold_fail_act;  /* inherits refs from hold_fail_act */
	if(print_rts(&fail_act, hold_fail_ex, fail_instr,
		     fail_act.program_ctr)) {
	  ASTR_ERR_MSG(ast_rts_msg);
	}
      }
      unreg(fmark);
      return 1;
    } /* end if(failure >= 0) */

    drop_activation_parts(&the_act);

  } /* end for(j = ...) */

 out:
  signal(SIGINT, SIG_DFL);
# ifdef UNIX
    signal(SIGSEGV, SIG_DFL);
# endif

  return 0;
}
