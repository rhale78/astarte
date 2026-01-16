/*****************************************************************
 * File:    show/printrts.c
 * Purpose: Print run time stack after trap, and provide a simple
 *          debugger.
 * Author:  Karl Abrahamson
 *****************************************************************/

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


#include <signal.h>
#include <string.h>
#ifdef USE_MALLOC_H
#  include <malloc.h>
#else
#  include <stdlib.h>
#endif
#include <stdlib.h>
#include "../misc/misc.h"
#ifdef UNIX
# include <sys/types.h>
# include <sys/time.h>
  time_t time(time_t *tloc);
#endif
#ifdef MSWIN
# include <time.h>
# include <windows.h>
  void out_str(char *);
  extern HWND MainWindowHandle;
#endif
#include "../utils/lists.h"
#include "../utils/hash.h"
#include "../utils/filename.h"
#include "../utils/strops.h"
#include "../alloc/allocate.h"
#include "../classes/classes.h"
#include "../evaluate/evaluate.h"
#include "../evaluate/instruc.h"
#include "../intrprtr/intrprtr.h"
#include "../rts/rts.h"
#include "../machdata/entity.h"
#include "../machdata/except.h"
#include "../machstrc/machstrc.h"
#include "../show/printrts.h"
#include "../show/getinfo.h"
#include "../show/prtent.h"
#include "../show/gprint.h"
#include "../gc/gc.h"
#include "../standard/stdtypes.h"
#include "../tables/tables.h"
#include "../unify/unify.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif


PRIVATE void print_rts_activation(ACTIVATION *a);
PRIVATE void print_node(FILE *f, char *name, CODE_PTR pc, 
			CODE_PTR result_type_instrs, ENVIRONMENT *env, 
			int num_entries, STATE *st, LIST *bl, int n);
PRIVATE void print_rts_down_control(DOWN_CONTROL *c);
PRIVATE void print_rts_up_control(UP_CONTROL *c);
PRIVATE void print_rts_state     (STATE *s);
PRIVATE void print_rts_env       (FILE *f, ENVIRONMENT *env, int num_entries, 
				  LONG pc_offset, STATE *st, LIST *bl);
PRIVATE void print_rts_globals   (void);
PRIVATE void remember_rts_info	 (RTS_INFO *info);
PRIVATE void restore_rts_info	 (RTS_INFO *info);


/****************************************************************
 *			PUBLIC VARIABLES			*
 ****************************************************************/

/********************************************************
 *			trap_msg			*
 *			trap_msg_len			*
 ********************************************************
 * File messages/trapmsg.txt contains some error	*
 * messages that the interpreter might need to print,	*
 * for example when an exception is trapped.  That file *
 * is only read when a message must be shown.  When it  *
 * is read, trap_msg is set to point to a newly		*
 * allocated array of strings, where trap_msg[i] is the *
 * i-th line of trapmsg.txt.				*
 *							*
 * At the same time, trap_msg_len is set to an array so *
 * that trap_msg_len[i] = strlen(trap_msg[i]).		*
 ********************************************************/

char** trap_msg     = NULL;
int*   trap_msg_len = NULL;

/************************************************************************
 *			break_info					*
 ************************************************************************
 * Structure break_info contains information that is used by the	*
 * debugger to decide when to break computation, and information	*
 * sent to the debugger when a break occurs.  The fields		*
 * are as follows.							*
 *									*
 * ------------ Fields influencing when breaks happen -----------------	*
 *									*
 * break_execute_packages						*
 *			break_execute_packages is a list of the 	*
 *			names of the packages whose executes		*
 *			should be broken, provided 			*
 *			break_executes is true.				*
 *			Only packages in this list are subject		*
 *			to breaks at executes.  If this list is NULL,	*
 *			then all packages are subject to breaks		*
 *			at executes when break_executes is true.	*
 *									*
 * breakable_funs 	breakable_funs is a list of the names of 	*
 * 			functions where debugging should be active, 	*
 * 			when in debug mode.				*
 * 									*
 * breakable_fun_packages						*
 *			breakable_fun_packages is a list of the 	*
 *			names of packages where debugging 		*
 *			should be active, when in debug mode.  		*
 *			All of the functions in these packages 		*
 *			are implicitly in list breakable_funs.		*
 *									*
 * breakable_packages	breakable_packages is a list of then		*
 *			names of packages where debugging can		*
 *			be active.  Any function that is not in		*
 *			a package in this list is not being debugged.	*
 *			(If breakable_packages is NULL, then all	*
 *			packages are presumed breakable.)		*
 *									*
 * break_all_funs       is true if all functions should be		*
 * 			treated as members of breakable_funs.		*
 * 									*
 * (So a function f is breakable if it occurs in a package in list	*
 *  breakable_packages or breakable_packages is NULL, and		*
 *  either f is in breakable_funs or the package of f is in		*
 *  breakable_fun_packages or break_all_funs is true.)			*
 *									*
 * suppress_breaks	suppress_breaks is nonzero to suppress 		*
 *			all breaking.  It should be incremented to 	*
 *			suppress breaks, and then decremented when 	*
 *			done, to prevent interfering with another	*
 *			place that needs to suppress breaks.		*
 *									*
 * break_lets		is true if the interpreter should		*
 * 			break to the debugger at a LET_I or		*
 *			similar instruction.				*
 *									*
 * show_val		is true if the interpreter should show the	*
 *			value that is being given a name at a let, or	*
 *			what is being returned from a function or	*
 *			passed to a function.				*
 * 									*
 * break_applies	is true if the interpreter should		*
 * 			break to the debugger at APPLY_I and		*
 * 			other apply instructions.			*
 * 									*
 * break_lazy		is true if the interpreter should 		*
 * 			break to the debugger at the start		*
 * 			and end of lazy evaluation.			*
 * 									*
 * break_executes	is true if the interpreter should		*
 *  			break to the debugger at the start		*
 * 			and end of an execute.				*
 * 									*
 * break_names		is true if the interpreter should		*
 * 			break to the debugger at NAME_I and		*
 * 			SNAME_I instructions.				*
 * 									*
 * break_failures	is true if the interpreter should		*
 * 			break to the debugger at failure.		*
 * 									*
 * break_assigns	is true if the interpreter should		*
 * 			break to the debugger at ASSIGN_I		*
 * 			and similar instructions.			*
 * 									*
 * break_prints		is true if the interpreter should		*
 * 			break to the debugger at PRINT_I		*
 * 			and related instructions.			*
 *									*
 * ------------ The following communicate information about ----------	*
 * ------------ a break to the debugger.		    ----------	*
 *									*
 * break_apply_name	break_apply_name is the name of the 		*
 *			function about to be applied, when 		*
 *			breaking at an APPLY_I or similar instruction.	*
 *									*
 * break_mode		break_mode is the mode that the current 	*
 *			break was started in.				*
 *									*
 * vis_let_offset	vis_let_offset is the offset of the identifier	*
 *			that is about to be defined at a LET_I		*
 *			or similar instruction.				*
 *									*
 * pc			pc is the program counter where the instruction *
 *			that caused the break occurs.			*
 *									*
 * result		the result for lazy computations.		*
 *									*
 * result_type_instrs   the address of instructions that compute the    *
 *                      type of result, for lazy computatiohs.		*
 ************************************************************************/

BREAK_INFO break_info;

/****************************************************************
 *			rts_file				*
 ****************************************************************
 * rts_file is the file on which we are showing the run-time    *
 * stack.  It is only used here and in intrprtr.c, where is     *
 * is closed on an error exit.					*
 ****************************************************************/

FILE* rts_file = NULL;

/****************************************************************
 *			printing_rts				*
 ****************************************************************
 * printing_rts is TRUE when inside a print_rts call.  Its 	*
 * purposes is:							*
 *								*
 *   to cause garbage collection in evaluate not to relocate;	*
 *   to cause boxpl_to_str_stdf to record the content of a box  *
 *   or place in the box and place tables.			*
 ****************************************************************/

Boolean printing_rts = FALSE; 

/****************************************************************
 *		        suppress_print_rts_in_abort		*
 ****************************************************************
 * When this is true, possibly_abort_dialog will not do a 	*
 * print_rts when aborting.  This should be true initially to	*
 * prevent the package reader from trying to do print_rts 	*
 * before the_act is set up.					*
 *								*
 * This is set FALSE in intrprtr.c after setting up the_act.	*
 ****************************************************************/

Boolean suppress_print_rts_in_abort = TRUE;

/****************************************************************
 *			PRIVATE VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			program_is_done				*
 ****************************************************************
 * program_is_done is set true when the program is finished.	*
 ****************************************************************/

PRIVATE Boolean program_is_done;

/****************************************************************
 *			rts_file_chars_left			*
 *			rts_file_max_chars			*
 *			rts_file_max_entity_chars		*
 ****************************************************************
 * rts_file_max_chars is the maximum number of characters that  *
 * should be written into ast.rts.  It is a soft limit; more    *
 * than this number of characters might be written, but the     *
 * number written should typically not exceed this by much.     *
 *								*
 * rts_file_chars_left is used to limit the number of characters*
 * that are printed to ast.rts.  It is the number of characters *
 * that can be printed before stopping the dump.		*
 *								*
 * rts_file_max_entity_chars is the maximum number of characters*
 * to use in the value of any given entity during the dump to   *
 * ast.rts.							*
 ****************************************************************/

LONG rts_file_max_chars        = INIT_RTS_FILE_MAX_CHARS;
LONG rts_file_max_entity_chars = INIT_RTS_FILE_MAX_ENTITY_CHARS;
PRIVATE LONG rts_file_chars_left;

/****************************************************************
 *			rts_box_table				*
 *			rts_place_table				*
 ****************************************************************
 * rts_box_table associates a type with each of some nonshared  *
 * box numbers.  						*
 *								*
 * rts_place_table associates a type with each of some shared   *
 * box addresses.						*
 *								*
 * In both cases, the type is the content type of the box.	*
 *								*
 * These tables are used so that print_rts_state can know	*
 * which boxes should have their contents printed, and what 	*
 * the content type is.						*
 ****************************************************************/

PRIVATE HASH2_TABLE* rts_box_table   = NULL;
PRIVATE HASH2_TABLE* rts_place_table = NULL;


/*********************************************************************
 *			READ_TRAP_MSGS				     *
 *********************************************************************
 * Read file messages/trapmsg.txt into trapmsg, allocating the 	     *
 * necessary space.						     *
 * 								     *
 * NOTE: NUM_TRAP_MSGS must be the number of lines in file	     *
 * trapmsg.txt.							     *
 *********************************************************************/

void read_trap_msgs(void)
{
  if(trap_msg == NULL) {
    int k;
    char* s = (char*) BAREMALLOC(strlen(MESSAGE_DIR) + 20);
    sprintf(s, "%s%s", MESSAGE_DIR, TRAP_MSG_FILE);
    init_err_msgs(&trap_msg, s, NUM_TRAP_MSGS);
    trap_msg_len = (int*) alloc(NUM_TRAP_MSGS * sizeof(int*));
    for(k = 0; k < NUM_TRAP_MSGS; k++) {
      trap_msg_len[k] = strlen(trap_msg[k]);
    }
    FREE(s);
  }
}


/****************************************************************
 *			INIT_BREAK_INFO				*
 ****************************************************************
 * Set up break_info for initializing the interpreter.		*
 ****************************************************************/

void init_break_info(void)
{
  memset(&break_info, 0, sizeof(BREAK_INFO));
}


/************************************************************************
 *			BREAKABLE					*
 ************************************************************************
 * Return true if the function described by the_act is breakable.	*
 ************************************************************************/

Boolean breakable(void)
{
  if(break_info.break_all_funs) return TRUE;

  if(the_act.pack_name == NULL) {
    char *file_name;
    int line;
    get_line_info(the_act.program_ctr, &the_act.pack_name, &file_name, &line);
  }

  if(break_info.breakable_packages != NULL &&
     !str_member(the_act.pack_name, break_info.breakable_packages)) {
    return FALSE;
  }

  if(str_member(quick_display_name(the_act.name), break_info.breakable_funs) ||
     str_member(the_act.pack_name, break_info.breakable_fun_packages)) {
    return TRUE;
  }

  return FALSE;
}


/****************************************************************
 *			DUMP_DIE				*
 ****************************************************************
 * Dump the run-time stack then die.  The exception reported    *
 * is LIMIT_EX.							*
 ****************************************************************/

void dump_die(int n)
{
  print_rts(&the_act, ENTU(LIMIT_EX), 0, the_act.program_ctr);
  die(n);
}


/****************************************************************
 *			PRINT_EXCEPTION_DESCRIPTION		*
 ****************************************************************
 * Print the description of exception ex on file f, if there	*
 * is a description.  Follow the description by a newline.	*
 *								*
 * The return value is the number of characters printed.	*
 ****************************************************************/

int print_exception_description(FILE *f, ENTITY ex)
{
  int n;
  char *descr;

  n = qwrap_tag(ex);
  descr = exception_data[n].descr;
  if(*descr != 0) {
    gprintf(f, "\n--");
    gprint_str(f, descr, -1);
    gprintf(f, "\n");
    return strlen(descr) + 4;
  }
  else return 0;
}


/****************************************************************
 *			PRINT_DUMP_HEADING			*
 ****************************************************************
 * Print a heading for a dump on file rts_file.  The heading	*
 * shows the exception (ex) and the instruction that caused it.	*
 ****************************************************************/

PRIVATE void
print_dump_heading(int instr, ENTITY ex, union ctl_or_act coa, 
		   int coa_kind, CODE_PTR pc)
{
  char *name, *cpy_main_file_name;
  time_t tloc;

  /*---------------------------------------------*
   * Show the first heading line, with the date. *
   *---------------------------------------------*/

  cpy_main_file_name = strdup(main_program_file_name);
  force_external(cpy_main_file_name);
  time(&tloc);
  fprintf(rts_file, "%s", ctime(&tloc));
  fprintf(rts_file, "File %s\n\n", cpy_main_file_name);
  FREE(cpy_main_file_name);

  /*------------------------------*
   * Now the rest of the heading. *
   *------------------------------*/

  rts_file_chars_left = rts_file_max_chars;
  read_trap_msgs();
  if(coa_kind != CTL_F && ENT_NE(ex, NOTHING)) {

    /*-----------------------------------*
     * Show the exception that occurred. *
     *-----------------------------------*/

    fprintf(rts_file, trap_msg[0]);
    rts_file_chars_left -= trap_msg_len[0];
    failure = -1;
    rts_file_chars_left -= 
      print_entity_with_state(ex, exception_type, rts_file, the_act.state_a, 
			      no_trap_tv, 0, rts_file_max_entity_chars);
    fnl(rts_file);
    rts_file_chars_left -= 
      print_exception_description(rts_file, ex);
    fnl(rts_file);

    /*----------------------------------------------------*
     * Show the instruction that caused the trap, and its *
     * arguments.					  *
     *----------------------------------------------------*/

    if(instr != 0) {
      ENTITY s;
      int    num_args = instinfo[instr].args;
      STACK* st       = coa.act->stack;
      REG_TYPE mark2  = reg1(&s);

      fprintf(rts_file, trap_msg[1]);
      read_instr_names();
      name = (instr > LIST2_PREF_I) ? instr_name[instr]
	                            : prefix_instr_name[instr][pc[1]];
      fprintf(rts_file, "%s\n", nonnull(name));

      if(num_args > 0 && not_empty_stack(st)) {
	fprintf(rts_file, trap_msg[2]);
	s = pop_this_stack(&st);
	if(num_args > 1 && not_empty_stack(st)) {
          rts_file_chars_left -= 
	    print_entity(rts_file, pop_this_stack(&st), 
			 rts_file_max_entity_chars);
	  fnl(rts_file);
	}
	rts_file_chars_left -= 
          print_entity(rts_file, s, rts_file_max_entity_chars);
	fprintf(rts_file, "\n\n");
      }
      else fnl(rts_file);
      unreg(mark2);

    } /* end if(instr != 0) */

  } /* end if(coa_kind != CTL_F ...) */
}


/****************************************************************
 *			PRINT_RTS1				*
 ****************************************************************
 * print_rts1 is a helper for print_rts_on_file, and is a       *
 * little more general. It is also used by deadlock_complain,	*
 * below.   							*
 *								*
 * coa is an activation or down-control to print. coa_kind is 0 *
 * if coa is a control, 1 if coa is an activation.  In the case *
 * where coa is a control, ex, instr and pc are ignored.	*
 * pc_decr is also only used when coa is an activation.  It is  *
 * subtracted from the program counter of the activation before *
 * the activation is printed.  This makes it possible to back   *
 * up to the address of the instruction that caused a trap.     *
 ****************************************************************/

PRIVATE Boolean print_rts1(union ctl_or_act coa, int coa_kind, int pc_decr,
			   ENTITY ex, int instr,  CODE_PTR pc)
{
  LIST *p;
  REG_TYPE mark = reg1_param(&ex);

  /*-------------------------------------------------*
   * Turn off tracing, unless trace_print_rts is on. *
   *-------------------------------------------------*/

# ifdef DEBUG
    UBYTE old_trace         = trace;
    UBYTE old_trace_control = trace_control;
    trace = trace_print_rts;
    trace_control = 0;
# endif

  failure = -1;     /* Need so that ast_num_to_str does not get confused. */

  /*------------------------------------------------------------*
   * Garbage collect, to reduce file sizes.  Otherwise, end up  *
   * trying to print whole files.  				*
   *								*
   * After the initial garbage collection we need to suppress	*
   * compactification throughout print_rts, since 		*
   * compactification moves boxes, and makes the output 	*
   * confusing.							*
   *------------------------------------------------------------*/

  force_file_cut = TRUE;
  gc(1);
  force_file_cut = FALSE;
  suppress_compactify++;

  /*-------------------------------*
   * Print a heading for the dump. *
   *-------------------------------*/

  print_dump_heading(instr, ex, coa, coa_kind, pc);

  /*------------------------------------*
   * Possibly warn about optimizations. *
   *------------------------------------*/

  if(do_tro) {
    fprintf(rts_file, trap_msg[3]);
    rts_file_chars_left -= trap_msg_len[3];
  }

  /*-----------------------------------------------------------*
   * Print the configuration.  When coa is an activation,      *
   * decrement its program_ctr first, to put it back into the  *
   * instruction that caused the trap. But be sure to restore. *
   *-----------------------------------------------------------*/

  if(coa_kind == CTL_F) {
    fprintf(rts_file, trap_msg[5]);
    fprintf(rts_file, trap_msg[21]);
    fprintf(rts_file, trap_msg[24]);
    fprintf(rts_file, trap_msg[5]);
    rts_file_chars_left -= 2*trap_msg_len[5] + trap_msg_len[21] 
			   + trap_msg_len[24];
    print_rts_down_control(coa.ctl);
  }
  else {
    fprintf(rts_file, trap_msg[5]);
    fprintf(rts_file, trap_msg[4]);
    fprintf(rts_file, trap_msg[24]);
    fprintf(rts_file, trap_msg[5]);
    rts_file_chars_left -= 2*trap_msg_len[5] + trap_msg_len[4] + 
			   trap_msg_len[24];
    coa.act->program_ctr -= pc_decr;
    print_rts_activation(coa.act);
    if(rts_file == stderr) {
      if(more_wait()) {
	coa.act->program_ctr += pc_decr;
        goto out;
      }
    }
    coa.act->program_ctr += pc_decr;
    for(p = runtime_shadow_st; p != NIL; p = p->tail) {
      if(rts_file_chars_left <= 0) break;
      fnl(rts_file);
      fprintf(rts_file, trap_msg[5]);
      fprintf(rts_file, trap_msg[6]);
      fprintf(rts_file, trap_msg[5]);
      fnl(rts_file);
      rts_file_chars_left -= 2*trap_msg_len[5] + trap_msg_len[6];
      if(p->head.act != NULL) {
	print_rts_activation(p->head.act);
        if(rts_file == stderr) {
          if(more_wait()) goto out;
        }
      }
    }
  }
  print_rts_globals();

  if(rts_file_chars_left <= 0) {
    fprintf(rts_file, trap_msg[25]);
  }

out:
  if(suppress_compactify > 0) suppress_compactify--;

# ifdef DEBUG
    trace = old_trace;
    trace_control = old_trace_control;
# endif

  unreg(mark);
  return TRUE;
}

/****************************************************************
 *			PRINT_RTS				*
 ****************************************************************
 * print_rts prints the configuration from activation a		*
 * onto file AST_RTS.						*
 *								*
 * It includes activations deferred due to lazy evaluation.	*
 * ex is the exception, instr the instruction and pc the	*
 * instruction location where a trap occurred.  If there is	*
 * no trap, then ex should be NOTHING.  In that case, instr	*
 * and pc are ignored.  If instr = 0, then instr and pc are	*
 * ignored regardless of the value of ex.			*
 * Return true on success.					*
 ****************************************************************/

Boolean print_rts(ACTIVATION *a, ENTITY ex, int instr, CODE_PTR pc)
{
  FILE *f;

# ifdef DEBUG
    if(trace) trace_i(316);
# endif

  if(printing_rts) return FALSE;

  /*---------------------------------*
   * Open the file to hold the dump. *
   *---------------------------------*/

  f = fopen(AST_RTS, TEXT_WRITE_OPEN);
  if(f == NULL) {
#   ifdef UNIX
      flush_stdout();
      fprintf(stderr, trap_msg[7]);
#   endif
    return FALSE;
  }

  /*---------------*
   * Do the print. *
   *---------------*/

  return print_rts_on_file(f, a, ex, instr, pc);

  /*-----------------*
   * Close the file. *
   *-----------------*/

  fclose(f);
  rts_file = NULL;
}


/****************************************************************
 *			PRINT_RTS_ON_FILE			*
 ****************************************************************
 * print_rts_on_file is similar to print_rts, but it prints     *
 * on file f.							*
 ****************************************************************/

Boolean print_rts_on_file(FILE *f, ACTIVATION *a, ENTITY ex, 
			  int instr, CODE_PTR pc)
{
  union ctl_or_act coa;
  Boolean result;
  RTS_INFO info;
  remember_rts_info(&info);

  rts_file          = f;
  special_condition = interrupt_occurred = do_profile = FALSE;
  coa.act           = a;
  printing_rts      = TRUE;
  rts_box_table     = NULL;
  rts_place_table   = NULL;

# ifdef UNIX
    signal(SIGSEGV, SIG_DFL);
# endif

  result  = print_rts1(coa, 1, 1, ex, instr, pc);

# ifdef UNIX
    signal(SIGSEGV, segviol);
# endif

  restore_rts_info(&info);
  return result;
}


/****************************************************************
 *			PRINT_RTS_ON_FILE_NAME			*
 ****************************************************************
 * print_rts_on_file_name is similar to print_rts, but it 	*
 * prints on file filename.  It assumes that there is no 	*
 * exception or instruction.					*
 ****************************************************************/

PRIVATE void print_rts_on_file_name(char *file_name, ACTIVATION *a)
{
  FILE *f;
  int c;
  struct stat stat_buf;

  if(stat(file_name, &stat_buf) == 0) {
    fprintf(stderr, "File %s exists.\n Overwrite? ", file_name);
    c = getchar();
    if(c != 'y') return;
  }
  f = fopen(file_name, "w");
  if(f == NULL) {
    fprintf(stderr, "Cannot open %s\n", file_name);
  }
  else {
    print_rts_on_file(f, a, NOTHING, 0, 0);
  }
  fclose(f);
}


/****************************************************************
 *			REMEMBER_RTS_INFO			*
 ****************************************************************
 * Record information for a recursive call to print_rts, so     *
 * that it can be restored after the recursive call.		*
 ****************************************************************/

PRIVATE void remember_rts_info(RTS_INFO *info)
{
  info->rts_file            = rts_file;  
  info->rts_file_chars_left = rts_file_chars_left;
  info->printing_rts        = printing_rts;
  info->rts_box_table       = rts_box_table;
  info->rts_place_table     = rts_place_table;
  info->special_condition   = special_condition;
  info->interrupt_occurred  = interrupt_occurred;
  info->do_profile          = do_profile;
  remember_type_storage(&(info->type_info));
}


/****************************************************************
 *		       RESTORE_RTS_INFO				*
 ****************************************************************
 * Restore the information recorded in info by 			*
 * remember_rts_info.						*
 ****************************************************************/

PRIVATE void restore_rts_info(RTS_INFO *info)
{
  printing_rts        = info->printing_rts;
  rts_file            = info->rts_file;  
  rts_file_chars_left = info->rts_file_chars_left;
  rts_box_table       = info->rts_box_table;
  rts_place_table     = info->rts_place_table;
  special_condition   = info->special_condition;
  interrupt_occurred  = info->interrupt_occurred;
  do_profile          = info->do_profile;
  restore_type_storage(&(info->type_info));
}


/****************************************************************
 *			DEADLOCK_COMPLAIN			*
 ****************************************************************
 * Tell the user that the program has deadlocked, and show the  *
 * active threads.						*
 ****************************************************************/

void deadlock_complain(void)
{
  signal(SIGSEGV, SIG_DFL);
  special_condition = interrupt_occurred = do_profile = FALSE;

  fflush(stdout);
  rts_file = fopen(AST_RTS, TEXT_WRITE_OPEN);
  if(rts_file == NULL) {
#   ifdef UNIX
      flush_stdout();
      fprintf(stderr, trap_msg[7]);
#   endif
  }
  else {
    read_trap_msgs();
    ASTR_ERR_MSG(trap_msg[22]);
    failure = -1;
    if(!printing_rts) {

      /*----------------------------------------------------------------*
       * Show the configuration.  Be careful, because timed_out_comp	*
       * and timed_out_kind can be changed by print_rts1.		*
       *----------------------------------------------------------------*/

      CONTROL_REG cmark;
      ACTIVATION_REG amark;
      int hold_timed_out_kind = timed_out_kind;

      ASTR_ERR_MSG(trap_msg[18]);
      printing_rts = TRUE;
      if(hold_timed_out_kind == CTL_F) {
        cmark = reg_control(timed_out_comp.ctl);
      }
      else amark = reg_activation(timed_out_comp.act);

      print_rts1(timed_out_comp, timed_out_kind, 0, NOTHING, 0, NULL);

      if(hold_timed_out_kind == CTL_F) unreg_control(cmark);
      else unreg_activation(amark);
      
      printing_rts = FALSE;
    }
  }
}


/****************************************************************
 *			PRINT_NODE_SUMMARY			*
 ****************************************************************
 * Show the n-th frame from the stack as having function name   *
 * name, and line number according to program counter pc.	*
 * Print on file f.						*
 ****************************************************************/

PRIVATE void print_node_summary(FILE *f, int n, char *name, CODE_PTR pc)
{
  char *pack_name, *dname, *file_name;
  int line;

  dname = quick_display_name(name);
  get_line_info(pc, &pack_name, &file_name, &line);
  fprintf(f, "%3d. %s (line %d package %s)\n", n, dname, line, pack_name);
}


/****************************************************************
 *			SHOW_WAITING				*
 ****************************************************************
 * Show the activations that are waiting for deferred		*
 * evaluations to complete, but in short form.			*
 *								*
 * This is only done if a is &the_act, since otherwise it       *
 * is not meaningful.						*
 ****************************************************************/

PRIVATE void show_waiting(ACTIVATION *a)
{
  if(a != &the_act || runtime_shadow_st == NIL) {
    fprintf(stderr, trap_msg[45]);
  }
  else {
    LIST *p;
    int n = 1;
    for(p = runtime_shadow_st; p != NIL; p = p->tail) {
      if(p->head.act != NULL) {
	ACTIVATION* aa = p->head.act;
	print_node_summary(stderr, 0, aa->name, aa->program_ctr);
        if(n % SUMMARY_LINES_PER_PAGE == 0) {
          if(more_wait()) return;
        }
	n++;
      }
    }
  }
}


/****************************************************************
 *			SHOW_STACK_FRAME			*
 ****************************************************************
 * Show the frame, including environment, n down from the top   *
 * of the stack starting at activation a.			*
 ****************************************************************/

PRIVATE void show_stack_frame(FILE *f, ACTIVATION *a, int n)
{
  CONTINUATION *p;
  STATE *st;
  LIST *bl;
  int k;

  st = a->state_a;
  rts_file_chars_left = rts_file_max_chars;
  if(n == 0) {
    bl = get_act_binding_list(a);
    fprintf(stderr, trap_msg[11]);
    fprintf(stderr, trap_msg[8]);
    print_node(f, a->name, a->program_ctr, a->result_type_instrs,
	       a->env, a->num_entries, st, bl, n);
  }
  else {
    for(k = 1, p = a->continuation; 
        k < n && p != NULL; 
        k++, p = p->continuation){}
    if(p != NULL) {
      bl = get_cont_binding_list(p);
      fprintf(stderr, trap_msg[11]);
      fprintf(stderr, trap_msg[8]);
      print_node(f, p->name, p->program_ctr, p->result_type_instrs,
		 p->env, p->num_entries, st, bl, n);
    }
  }
}


/****************************************************************
 *			PRINT_NODE				*
 ****************************************************************
 * Print a frame of the run-time stack with given name, program *
 * counter, environment	and number of environment entries, on	*
 * file f. 							*
 *								*
 * result_type_instrs is a pointer to a sequence of instructions*
 * that build the result type of the node.			*
 *								*
 * List bl is the binding_list.					*
 *								*
 * framenum is the frame number.				*
 ****************************************************************/

PRIVATE void 
print_node(FILE *f, char *name, CODE_PTR pc, CODE_PTR result_type_instrs,
	   ENVIRONMENT *env, int num_entries, STATE *st, LIST *bl, 
	   int framenum)
{
  struct package_descr *pd;
  char *pack_name, *file_name, *dname;
  int line, n;
  LONG offset;

  if(rts_file_chars_left <= 0) return;

  /*----------------------------------------------------*
   * Print the frame number and function/frame name.	*
   *----------------------------------------------------*/

  dname = quick_display_name(name);
  fprintf(f, "%3d.%-20s ", framenum, dname);
  n = strlen(dname);
  rts_file_chars_left -= max(20,n);
  
  /*--------------------------------------------*
   * Print the package name and line number.	*
   *--------------------------------------------*/

  get_line_info(pc, &pack_name, &file_name, &line);
  fprintf(f, "%-20s %5d\n\n", pack_name, line);
  n = strlen(pack_name);
  rts_file_chars_left -= max(20, n) + 6;

  /*--------------------------------------------*
   * Print the result type of this frame.	*
   *--------------------------------------------*/

  {TYPE *t;
   fprintf(f, trap_msg[51]);
   bump_type(t = 
	     eval_type_instrs_with_binding_list(result_type_instrs, env, bl));
   fprint_ty(f, t);
   fnl(f);
   drop_type(t);
  }

  /*-------------------------*
   * Print the environment.  *
   *-------------------------*/

  pd = get_pd_entry(pc);
  offset = (pd != NULL) ? pc - pd->begin_addr : -1;
  print_rts_env(f, env, num_entries, offset, st, bl);

  /*----------------------------------------------------*
   * Print a line separating this frame from the next   *
   * one.						*
   *----------------------------------------------------*/

  if(rts_file_chars_left > 0) {
    fprintf(f, trap_msg[8]);
    rts_file_chars_left -= trap_msg_len[8];
  }
}


/****************************************************************
 *			MORE_WAIT				*
 ****************************************************************
 * Wait until a q or newline is typed.				*
 *								*
 * Return true if a q was typed, false otherwise.		*
 ****************************************************************/

Boolean more_wait(void)
{
  int c;

  fprintf(stderr, "---- more (newline for more, q to quit)");
  c = getchar();
  if(c == 'q') return TRUE;
  while(c != '\n') c = getchar();
  return FALSE;
}


/****************************************************************
 *			SHOW_STACK_BRIEFLY			*
 ****************************************************************
 * Print the run-time stack of activation a, one line per frame.*
 * Only show the function and line number.			*
 ****************************************************************/

PRIVATE void show_stack_briefly(ACTIVATION *a)
{
  int n;
  CONTINUATION *p;

  if(a == NULL) return;
  
  rts_file_chars_left = rts_file_max_chars;
  print_node_summary(stderr, 0, a->name, a->program_ctr);
  n = 1;
  for(p = a->continuation; p != NULL; p = p->continuation) {
    print_node_summary(stderr, n, p->name, p->program_ctr);
    n++;
    if(n % SUMMARY_LINES_PER_PAGE == 0) {
      if(more_wait()) return;
    }
  }
}


/****************************************************************
 *			PRINT_RTS_ACTIVATION			*
 ****************************************************************
 * Print the run time stack from activation a.  		*
 *								*
 * If full is true, then this includes the frames of all 	*
 * continuations in the stack.					*
 *								*
 * If full is false, then only what is in this activation (the	*
 * top stack frame) is shown.					*
 ****************************************************************/

PRIVATE void print_rts_activation(ACTIVATION *a)
{
  CONTINUATION *p;
  STATE *st;
  LIST *cr, *bl;
  int n;

  if(rts_file_chars_left <= 0) return;

  if(a == NULL) {
    fprintf(rts_file, trap_msg[9]);
    rts_file_chars_left -= trap_msg_len[9];
    return;
  }
  
  cr = a->coroutines;
  st = a->state_a;
  bl = get_act_binding_list(a);

  /*-----------------------*
   * Check for coroutines. *
   *-----------------------*/

  if(cr != NIL && (cr->head.list != NIL || cr->tail != NIL)) {
    fprintf(rts_file, trap_msg[10]);
    rts_file_chars_left -= trap_msg_len[10];
  }
  fprintf(rts_file, trap_msg[11]);
  rts_file_chars_left -= trap_msg_len[11];

  /*-----------------------------------*
   * Print the root of the activation. *
   *-----------------------------------*/

  print_node(rts_file, a->name, a->program_ctr, a->result_type_instrs,
	     a->env, a->num_entries, st, bl, 0);

  /*------------------------------------------------------------------*
   * Print the run-time stack by scanning the chain of continuations. *
   *------------------------------------------------------------------*/

  for(n = 1, p = a->continuation; 
      p != NULL && rts_file_chars_left > 0; 
      p = p->continuation, n++) {
    bl = get_cont_binding_list(p);
    print_node(rts_file, p->name, p->program_ctr, p->result_type_instrs,
	       p->env, p->num_entries, st, bl, n);
  }
  if(rts_file_chars_left <= 0) return;
  
  /*-------------------------------------*
   * Show the state for this activation. *
   *-------------------------------------*/

  print_rts_state(st);
  free_hash2(rts_place_table);
  free_hash2(rts_box_table);
  rts_place_table = rts_box_table = NULL;
  if(rts_file_chars_left <= 0) return;

  /*-------------------------------------------*
   * Show the other threads, if there are any. *
   *-------------------------------------------*/

  if(there_are_other_threads(a->control)) {
    fprintf(rts_file, trap_msg[5]);
    fprintf(rts_file, trap_msg[23]);
    rts_file_chars_left -= trap_msg_len[5] + trap_msg_len[23];
    print_rts_up_control(a->control);
  }
}


/****************************************************************
 *			PRINT_RTS_THREAD			*
 ****************************************************************
 * Print activation a, with a preamble indicating that we are   *
 * introducing a new thread.					*
 ****************************************************************/

PRIVATE void print_rts_thread(ACTIVATION *a)
{
  if(rts_file_chars_left <= 0) return;

  fnl(rts_file);
  fprintf(rts_file, trap_msg[5]);
  fprintf(rts_file, trap_msg[20]);
  rts_file_chars_left -= trap_msg_len[5] + trap_msg_len[20];
  print_rts_activation(a);
}


/********************************************************
 *			PRINT_RTS_DOWN_CONTROL		*
 ********************************************************
 * Show DOWN_CONTROL c on rts_file.			*
 ********************************************************/

PRIVATE void print_rts_down_control(DOWN_CONTROL *c)
{

tail_recur:
  if(rts_file_chars_left <= 0) return;
  if(c == NULL) return;

  if(CTLKIND(c) > MIX_F) {

    /*-----------------------------------------------------*
     * No need to show a mark, reset, try or branch node.  *
     * Just print the (active) child.			   *
     *-----------------------------------------------------*/

    if(LCHILD_IS_ACT(c)) {
      print_rts_thread(c->left.act);
    }
    else {
      c = c->left.ctl;
      goto tail_recur;
    }
  }
  else {

    /*--------------------------------------------*
     * At a mix or choose-mix, show both threads. *
     *--------------------------------------------*/

    if(LCHILD_IS_ACT(c)) print_rts_thread(c->left.act);
    else print_rts_down_control(c->left.ctl);

    if(RCHILD_IS_ACT(c)) print_rts_thread(c->right.act);
    else {
      c = c->right.ctl;
      goto tail_recur;
    }
  }
}


/********************************************************
 *			PRINT_RTS_UP_CONTROL		*
 ********************************************************
 * Show UP_CONTROL c on rts_file.			*
 ********************************************************/

PRIVATE void print_rts_up_control(UP_CONTROL *c)
{

  while(c != NULL) {

    /*--------------------------------------------------------*
     * At a mix or choose-mix, show the child thread(s), and  *
     * then continue with the parent.  At other kinds of      *
     * nodes, just continue with the parent.		      *
     *--------------------------------------------------------*/

    if(rts_file_chars_left <= 0) return;
    if(CTLKIND(c) <= MIX_F) {
    
      if(RCHILD_IS_ACT(c)) print_rts_thread(c->right.act);
      else print_rts_down_control(c->right.ctl);
    }
    c = c->PARENT_CONTROL;
  }
}
 

/****************************************************************
 *		      RECORD_BXPL_FOR_PRINT_RTS			*
 ****************************************************************
 * Put box or place bxpl and its type into the appropriate 	*
 * hash table so that its type will be known when 		*
 * printing the configuration. 					*
 *								*
 * ty is the type of a function that converts a box or place    *
 * to a string.  So the content type of the box is 		*
 * ty->ty1->ty1.						*
 *								*
 * Recursively put the boxes and places referenced by		*
 * the content of this box or place into the tables.		*
 ****************************************************************/

void record_bxpl_for_print_rts(ENTITY bxpl, TYPE *ty)
{
  TYPE *t;
  HASH2_CELLPTR h;
  HASH_KEY u;
  HASH2_TABLE **tbl;
  STATE *st;
  LONG val;

  t = ty->TY1->TY1;
  if(TAG(bxpl) == BOX_TAG) {
    tbl = &rts_box_table;
    val = VAL(bxpl);
  }
  else {
    tbl = &rts_place_table;
    val = (LONG) ENTVAL(bxpl);
  }
  u.num = val;
  h     = insert_loc_hash2(tbl, u, inthash(val), eq);
  if(h->key.num == 0) {
    h->key.num  = val;
    h->val.type = t;
    st          = the_act.state_a;

    /*-----------------------------------------------------*
     * Recursive call on content.  Nothing will be printed *
     * here, because the suppress parameter of 		   *
     * print_entity_with_state is TRUE.  Boxes will just   *
     * be put into the tables, by boxpl_to_str_stdf.	   *
     *-----------------------------------------------------*/

    print_entity_with_state(prcontent_stdf(bxpl), t, NULL, 
			    st, no_trap_tv, TRUE, LONG_MAX);
  }
}


/********************************************************
 *			INSTALL_GLOBAL_BOXES		*
 ********************************************************
 * Install the global boxes into rts_box_table and      *
 * rts_place_table.					*
 ********************************************************/

PRIVATE void install_global_boxes_help(HASH2_CELLPTR h)
{
  TYPE* t = find_u(h->key.type);

  if(t != NULL && TKIND(t) == FAM_MEM_T) {
    LONG val;
    HASH2_TABLE** tbl;
    HASH2_CELLPTR new_cell;
    HASH_KEY u;
    ENTITY bx;
    int tag;

    /*-------------------------------------------------------*
     * Get the box. If the value has not been evaluated,     *
     * then ignore it.					     *
     *-------------------------------------------------------*/

    bx = h->val.entity;
    tag = TAG(bx);
    while(tag == INDIRECT_TAG || tag == GLOBAL_INDIRECT_TAG) {
      bx = *ENTVAL(bx);
      tag = TAG(bx);
    }
    if(tag == BOX_TAG) {
      val = VAL(bx);
      tbl = &rts_box_table;
    }
    else if(tag == PLACE_TAG) {
      val = (LONG) ENTVAL(bx);
      tbl = &rts_place_table;
    }
    else return;

    /*-------------------*
     * Install this box. *
     *-------------------*/

    u.num    = val;
    new_cell = insert_loc_hash2(tbl, u, inthash(val), eq);
    if(new_cell->key.num == 0) {
      new_cell->key.num = val;
      new_cell->val.type = t->TY1;
    }
  }
}

/*---------------------------------------------------------------*/

PRIVATE void install_global_boxes(void)
{
  int i;
  HASH_KEY u;
  HASH2_CELLPTR new_cell;

  /*-------------------------*
   * Get the standard boxes. *
   *-------------------------*/

  for(i = 1; i < FIRST_FREE_BOX_NUM; i++) {
    u.num    = i;
    new_cell = insert_loc_hash2(&rts_box_table, u, inthash(i), eq);
    if(new_cell->key.num == 0) {
      new_cell->key.num  = i;
      new_cell->val.type = std_box_content_type[i];
    }
  }

  /*-------------------*
   * Scan the globals. *
   *-------------------*/

  for(i = 0; i < next_ent_num; i++) {
    HASH2_TABLE* tbl = outer_bindings[i].mono_table;
    scan_hash2(tbl, install_global_boxes_help);
  }
}


/********************************************************
 *			PRINT_RTS_STATE			*
 ********************************************************
 * print_rts_state shows the bindings of state s.	*
 *							*
 * It is required that rts_box_table and		*
 * rts_place_table have already been set up, at least   *
 * as far as boxes in environments and data structures  *
 * go.  print_rts_state will add boxes from the global  *
 * environment.						*
 ********************************************************/

PRIVATE STATE *the_rts_state ;

PRIVATE void print_place_content(HASH2_CELLPTR h)
{
  ENTITY place   = ENTP(PLACE_TAG, h->key.entp);
  ENTITY content = *ENTVAL(place);
  if(rts_file_chars_left <= 0) return;

  if(ENT_NE(content, NOTHING)) {
    fprintf(rts_file, "%8ld := ", VAL(place));
    rts_file_chars_left -= 
      print_entity_with_state(content, h->val.type, 
			      rts_file, the_rts_state, 
			      no_trap_tv, FALSE, 
			      rts_file_max_entity_chars) + 9;
    fprintf(rts_file, ": ");
    rts_file_chars_left -= fprint_ty(rts_file, h->val.type);
    fnl(rts_file);
  }
}

/*-----------------------------------------------------------------*/

PRIVATE Boolean not_started_free_boxes;

PRIVATE void print_box_content(HASH2_CELLPTR h)
{
  LONG box;
  ENTITY content, *contentp;

  if(rts_file_chars_left <= 0) return;

  /*---------------------------------*
   * Get the box from the hash cell. *
   *---------------------------------*/

  box = h->key.num;
  if(box == 0) return;

  /*-------------------------------------------------------*
   * Get the content of box. If there is none, do nothing. *
   *-------------------------------------------------------*/

  contentp = ast_content_s(box, the_rts_state, 0);
  if(contentp == NULL) {
    contentp = ast_content_s(box, initial_state, 0);
  }
  if(contentp == NULL) return;
  content = *contentp;
  if(ENT_EQ(content, NOTHING)) {failure = -1; return;}

  /*------------------------------------------*
   * If this is a standard box, add its name. *
   *------------------------------------------*/

  if(box < FIRST_FREE_BOX_NUM) {
    fprintf(rts_file, "  (%20s) ", std_box_name[toint(box)]);
    rts_file_chars_left -= 25;
  }
  else if(not_started_free_boxes) {
    fnl(rts_file);
    not_started_free_boxes = FALSE;
  }
  fprintf(rts_file, "%8ld := ", box);
  rts_file_chars_left -= 9;
  rts_file_chars_left -= 
    print_entity_with_state(content, h->val.type, rts_file, the_rts_state,
			    no_trap_tv, FALSE, rts_file_max_entity_chars);
  fprintf(rts_file, ": ");
  rts_file_chars_left -= fprint_ty(rts_file, h->val.type);
  fnl(rts_file);
}

/*-----------------------------------------------------------------*/

PRIVATE void print_rts_state(STATE *s)
{
  if(rts_file_chars_left <= 0) return;

  install_global_boxes();
  if(rts_box_table == NULL && rts_place_table == NULL) return;
  fnl(rts_file);
  fprintf(rts_file, trap_msg[5]);
  fprintf(rts_file, trap_msg[12]);
  fprintf(rts_file, trap_msg[5]);
  rts_file_chars_left -= 2*trap_msg_len[5] + trap_msg_len[12];
  the_rts_state = s;

  /*-------------------------------------*
   * Print the table of nonshared boxes. *
   *-------------------------------------*/

  if(rts_box_table != NULL) {
    fprintf(rts_file, trap_msg[13]);
    rts_file_chars_left -= trap_msg_len[13];
    sort_int_hash2(rts_box_table);
    not_started_free_boxes = TRUE;
    scan_and_clear_hash2(&rts_box_table, print_box_content);
  }

  /*-------------------------------------*
   * Print the table of shared boxes.    *
   *-------------------------------------*/

  if(rts_place_table != NULL) {
    fprintf(rts_file, trap_msg[14]);
    rts_file_chars_left -= trap_msg_len[14];
    sort_int_hash2(rts_place_table);
    scan_and_clear_hash2(&rts_place_table, print_place_content);
  }
  fprintf(rts_file, trap_msg[5]);
  rts_file_chars_left -= trap_msg_len[5];
  the_rts_state = NULL;
}


/****************************************************************
 *			PRINT_RTS_ENV				*
 ****************************************************************
 * Print the first num_entries fields of environment env for a  *
 * runtime stack print after a fail.  Print in state st.	*
 * Use binding list bl for evaluating types.			*
 ****************************************************************/

PRIVATE void print_rts_env(FILE *f, ENVIRONMENT *env, int num_entries,
			   LONG pc_offset, STATE *st, LIST *bl)
{
  NAME_TYPE info[256];
  struct package_descr *pd;
  CODE_PTR pc;
  char *id;
  int i;
  ENTITY e;
  REG_TYPE mark = reg1(&e);

  /*----------------------------------*
   * Scan through environment scopes. *
   *----------------------------------*/

  while (env != NULL && env->kind == LOCAL_ENV) {
    if(rts_file_chars_left <= 0) return;

    if(num_entries > 0 && pc_offset >= 0) {
      get_env_info(info, env, num_entries, pc_offset, bl);

      /*-------------------------------------------------*
       * Show each entry in scope env.  But skip	 *
       * entries whose name begins with HIDE_CHAR, since *
       * they are compiler-generated names.		 *
       *-------------------------------------------------*/

      for(i = 0; i < num_entries; i++) {
        if(rts_file_chars_left <= 0) break;
        id = info[i].name;
	if(id != NULL && id[0] != HIDE_CHAR) {
	  e = local_binding_env(env, i);
	  if(ENT_NE(e, NOTHING)) {
	    TYPE *tt;
            fprintf(f, "    %s = ", id);
	    rts_file_chars_left -= strlen(id);
	    failure = -1;
	    bump_type(tt = copy_type(info[i].type, 0));
	    rts_file_chars_left -= 
              print_entity_with_state(e, tt, f, st, no_trap_tv, 
				      FALSE, rts_file_max_entity_chars);
            fprintf(f, ": ");
            rts_file_chars_left -= fprint_ty(f, tt);
	    drop_type(tt);
	    rts_file_chars_left -= 5;
	    fnl(f);
          }
	}
      }

      for(i = 0; i < num_entries; i++) {
	drop_type(info[i].type);
      }
    } /* end if(num_entries > 0 ...) */

    num_entries = env->num_link_entries;
    env = env->link;
    if(env == NULL || env->kind != LOCAL_ENV) goto out;
    pc = env->pc;
    pd = get_pd_entry(pc);
    pc_offset = (pd != NULL) ? pc - get_pd_entry(pc)->begin_addr : -1;

  } /* end while(env != NULL ...) */

 out:
  unreg(mark);
}


/**********************************************************
 *			PRINT_RTS_GLOBALS		  *
 **********************************************************
 * Show the values of all globals that are not functions. *
 **********************************************************/

PRIVATE char* print_rts_globals_name;   /* Holds name of global being    *
				        * looked at.			*/

PRIVATE void print_rts_globals_help(HASH2_CELLPTR h)
{
  TYPE* t = find_u(h->key.type);

  if(rts_file_chars_left <= 0) return;

  if(t != NULL && TKIND(t) != FUNCTION_T) {
    fprintf(rts_file, "%s\n  : ", print_rts_globals_name);
    rts_file_chars_left -= fprint_ty(rts_file, t) + 
                           strlen(print_rts_globals_name) + 10;
    fprintf(rts_file, "\n  = ");
    rts_file_chars_left -=
      print_entity_with_state(h->val.entity, t, rts_file, execute_state, 
			      no_trap_tv, 0, rts_file_max_entity_chars);
    fnl(rts_file);
  }
}

/*-----------------------------------------------------------------*/

PRIVATE int cmp_global_header_names(const int* a, const int* b)
{
  return strcmp(outer_bindings[*a].name, outer_bindings[*b].name);
}

typedef int (*QSORT_CMP_TYPE)(const void*, const void*);

/*-----------------------------------------------------------------*/

PRIVATE void print_rts_globals(void)
{
  int i, copy_outer_bindings_size;
  int* copy_outer_bindings;

  if(rts_file_chars_left <= 0) return;

  fnl(rts_file);
  fprintf(rts_file, trap_msg[5]);
  fprintf(rts_file, trap_msg[19]);
  fprintf(rts_file, trap_msg[5]);
  fnl(rts_file);
  rts_file_chars_left -= 2*trap_msg_len[5] + trap_msg_len[19];

  /*------------------------------------------------------------*
   * We are going to sort a copy of outer_bindings by name, so  *
   * that the output is in alphabetical order.  But we must be	*
   * careful, because outer_bindings can be reallocated.  So the*
   * copy holds indices in outer_bindings, which remain valid	*
   * even after outer_bindings is reallocated.  Also,		*
   * next_ent_num can change as new entries are made.  We	*
   * freeze it, only showing the entries that were present when	*
   * print_rts_globals started (plus possibly a few more).	*
   *------------------------------------------------------------*/

  copy_outer_bindings_size = (int) next_ent_num;
  copy_outer_bindings = (int*) 
		   BAREMALLOC(copy_outer_bindings_size * sizeof(int));
  for(i = 0; i < copy_outer_bindings_size; i++) {
    copy_outer_bindings[i] = i;
  }
  qsort((char *) copy_outer_bindings, 
	copy_outer_bindings_size, 
	sizeof(int),
	(QSORT_CMP_TYPE)cmp_global_header_names);

  /*------------------------------------------------------------*
   * Now we would like to print the contents of 		*
   * outer_bindings[cpy_outer_bindings[i]].mono_table, for each	*
   * i. A difficulty is that the act of printing it can cause	*
   * new entries to be made, which modifies the table.  The	*
   * modifications can cause the table to be reallocated, so we *
   * cannot presume that it will stay in one place.  The	*
   * solution is to copy the hash table, and scan the copy.	*
   *------------------------------------------------------------*/

  for(i = 0; i < copy_outer_bindings_size; i++) {

    /*----------------------*
     * Copy the mono table. *
     *----------------------*/

    HASH2_TABLE* orig_mono_table = 
      outer_bindings[copy_outer_bindings[i]].mono_table;
    if(orig_mono_table != NULL) {
      int size_index = orig_mono_table->size;
      HASH2_TABLE* copy_mono_table = allocate_hash2(size_index);
      LONG byte_size = sizeof(HASH2_TABLE) 
		       + (hash_size[size_index] - 1) * sizeof(HASH2_CELL);
      longmemcpy(copy_mono_table, orig_mono_table, byte_size);

      /*--------------------------------*
       * Print the mono table contents. *
       *--------------------------------*/

      print_rts_globals_name = outer_bindings[copy_outer_bindings[i]].name;
      scan_hash2(copy_mono_table, print_rts_globals_help);
      free_hash2(copy_mono_table);
    }
  } /* end for(i = ...) */

  /*-------------------------------*
   * Free the outer_bindings copy. *
   *-------------------------------*/

  FREE(copy_outer_bindings);
}


/****************************************************************
 *			SHOW_ENV_STDF				*
 ****************************************************************
 * Show the_act.env on file f.					*
 ****************************************************************/

ENTITY show_env_stdf(ENTITY f)
{
  FILE *ff;
  struct package_descr *pd;
  LONG offset;
  STATE *st;
  LIST *bl;

  ff = file_from_entity(f);
  if(ff == NULL) return hermit;
  pd = get_pd_entry(the_act.program_ctr);
  offset = (pd != NULL) ? the_act.program_ctr - pd->begin_addr : -1;
  st = the_act.state_a;
  bl = get_act_binding_list(&the_act);
  print_rts_env(ff, the_act.env, the_act.num_entries, offset, st, bl);
  return hermit;
}


/****************************************************************
 *			SHOW_CONFIG_STDF			*
 ****************************************************************
 * Show the current configuration on file f.			*
 ****************************************************************/

ENTITY show_config_stdf(ENTITY f)
{
  FILE *ff;

  ff = file_from_entity(f);
  if(ff == NULL) return hermit;
  print_rts_on_file(ff, &the_act, NOTHING, 0, NULL);
  return hermit;
}


/****************************************************************
 *			CLEAR_REST_OF_LINE			*
 ****************************************************************
 * If c is a newline character, do nothing.  Otherwise, 	*
 * Consume characters from stdin until a newline is consumed.   *
 ****************************************************************/

PRIVATE void clear_rest_of_line(int c)
{
  while(c != '\n') {
    c = getchar();
  }
}


/****************************************************************
 *			GET_DEBUG_CMD				*
 ****************************************************************
 * Read a string up to white space, skipping over any leading   *
 * white space.  Store the string into buffer buf, which has    *
 * buf_size bytes.  Stop at and end-of-line.			*
 *								*
 * Return the last character read.				*
 ****************************************************************/

PRIVATE int get_debug_cmd(char *buf, int buf_size)
{
  int c, k;
  k = 0;
  do {
    c = getchar();
  } while(c == ' ' || c == '\t');
  while(!is_white(c) && k < buf_size - 1) {
    buf[k++] = c;
    c = getchar();
  } 
  buf[k] = 0;
  return c;
}


/****************************************************************
 *			GET_NAME_REST_OF_LINE			*
 ****************************************************************
 * If prev is not a newline:					*
 * Read the rest of the current line from stdin into buffer     *
 * name, up to a maximum of n characters.  Then set name to     *
 * the name that occurs on that line up to a white space        *
 * character.							* 
 *								*
 * If prev is a newline, set name to an empty string.		*
 ****************************************************************/

void get_name_rest_of_line(char *name, int n, int prev)
{
  int i;
  char c;

  if(prev == '\n') {
    *name = 0;
  }
  else {
    fgets(name, n, stdin);
    for(i = 0; c = name[i], c != 0 && !is_white(c); i++) {}
    name[i] = 0;
  }
}


/****************************************************************
 *			INTERRUPT_VIS				*
 ****************************************************************
 * interrupt_vis handles interrupts in the visualizer by just   *
 * printing "interrupt".					*
 ****************************************************************/

PRIVATE void interrupt_vis(int signo_unused)
{
  signal(SIGINT, SIG_IGN);
  fprintf(stderr, "\ninterrupt\n");
  signal(SIGINT, interrupt_vis);
}


/****************************************************************
 *			DEBUG_INTERACT				*
 ****************************************************************
 * debug_interact() interacts with the user, allowing the user  *
 * to perform some simple debug actions.			*
 ****************************************************************/

#ifdef UNIX
void debug_interact(void)
{
  char cmd[20];
  int c;
  int context_offset = 0;
  ACTIVATION* current_act = &the_act;

  signal(SIGINT, interrupt_vis);
  for(;;) {
    fprintf(stderr, "> ");
    fflush(stderr);
    c = get_debug_cmd(cmd, 20);
    
    if(cmd[0] == 0) {
      clear_rest_of_line(c);
    }

    else if(prefix("break", cmd)) {

      /*-----------------------------------*
       * breakApplies: break at applies.   *
       *-----------------------------------*/

      if(strcmp(cmd+5, "Applies") == 0) {
	clear_rest_of_line(c);
	break_info.break_applies = TRUE;
      }

      /*-------------------------------------------*
       * breakFail: break at failure.	 	   *
       *-------------------------------------------*/

      else if(strcmp(cmd+5, "Fail") == 0) {
	clear_rest_of_line(c);
	break_info.break_failures = TRUE;
      }

      /*-------------------------------------------*
       * breakLazy: break at lazy evaluation.	   *
       *-------------------------------------------*/
      
      else if(strcmp(cmd+5, "Lazy") == 0) {
	clear_rest_of_line(c);
	break_info.break_lazy = TRUE;
      }

      /*-----------------------------------*
       * breakLets: break at lets.	   *
       *-----------------------------------*/

      else if(strcmp(cmd+5, "Lets") == 0) {
	clear_rest_of_line(c);
	break_info.break_lets = TRUE;
      }

      /*-----------------------------------*
       * breakFun: break a function.	   *
       *-----------------------------------*/

      else if(strcmp(cmd+5, "Fun") == 0) {
	char funname[MAX_NAME_LENGTH + 1];

	get_name_rest_of_line(funname, MAX_NAME_LENGTH, c);
	SET_LIST(break_info.breakable_funs, 
		 str_cons(stat_str_tb(display_name(funname)), 
			  break_info.breakable_funs));
	break_info.break_applies = TRUE;
      }

      /*-----------------------------------*
       * breakPackage: break all functions *
       *               in a given package. *
       *-----------------------------------*/

      else if(strcmp(cmd+5, "Package") == 0) {
	char packname[MAX_NAME_LENGTH + 1];
	
	get_name_rest_of_line(packname, MAX_NAME_LENGTH, c);
	SET_LIST(break_info.breakable_fun_packages, 
		 str_cons(stat_str_tb(packname), 
			  break_info.breakable_fun_packages));
	break_info.break_applies = TRUE;
      }
    }

    else if(prefix("nobreak", cmd)) {

      /*-------------------------------------------*
       * nobreakApplies: unbreak at applies.	   *
       *-------------------------------------------*/
      
      if(strcmp(cmd+7, "Applies") == 0) {
	clear_rest_of_line(c);
	break_info.break_applies = FALSE;
      }

      /*----------------------------------*
       * nobreakFail: unbreak at failure. *
       *----------------------------------*/

      else if(strcmp(cmd+7, "Fail") == 0) {
	clear_rest_of_line(c);
	break_info.break_failures = FALSE;
      }

      /*-------------------------------------------*
       * nobreakLazy: unbreak at lazy evaluations. *
       *-------------------------------------------*/

      else if(strcmp(cmd+7, "Lazy") == 0) {
	clear_rest_of_line(c);
	break_info.break_lazy = FALSE;
      }

      /*-----------------------------------*
       * nobreakLets: unbreak at lets.	   *
       *-----------------------------------*/
      
      else if(strcmp(cmd+7, "Lets") == 0) {
	clear_rest_of_line(c);
	break_info.break_lets = FALSE;
      }

      /*-----------------------------------*
       * nobreakFun: unbreak a function.   *
       *-----------------------------------*/

      else if(strcmp(cmd+7, "Fun") == 0) {
	char funname[MAX_NAME_LENGTH + 1];

	get_name_rest_of_line(funname, MAX_NAME_LENGTH, c);
	SET_LIST(break_info.breakable_funs, 
		 delete_string(funname, break_info.breakable_funs));
      }

      /*------------------------------------------------*
       * nobreakPackage: Remove a given package from 	*
       *                 breakable_fun_packages 	*
       *------------------------------------------------*/

      else if(strcmp(cmd+7, "Package") == 0) {
	char packname[MAX_NAME_LENGTH + 1];
	
	get_name_rest_of_line(packname, MAX_NAME_LENGTH, c);
	SET_LIST(break_info.breakable_fun_packages, 
		 delete_string(packname, break_info.breakable_fun_packages));
      }

    }

    /*-----------------------------------*
     * noShowVal: do not show value      *
     *            at a let.		 *
     *-----------------------------------*/
      
    else if(strcmp(cmd, "noShowVal") == 0) {
      clear_rest_of_line(c);
      break_info.show_val = FALSE;
    }

    /*-------------------------------------------------------*
     * restrictTo: Add a package to list breakable_packages. *
     *-------------------------------------------------------*/

    else if(strcmp(cmd, "restrictTo") == 0) {
      char packname[MAX_NAME_LENGTH + 1];

      get_name_rest_of_line(packname, MAX_NAME_LENGTH, c);
      SET_LIST(break_info.breakable_packages, 
	       str_cons(stat_str_tb(packname), break_info.breakable_packages));
    }

    /*---------------------------------------*
     * config: show configuration on stderr. *
     *---------------------------------------*/

    else if(strcmp(cmd, "config") == 0) {
      clear_rest_of_line(c);
      if(program_is_running) {
        print_rts_on_file(stderr, current_act, NOTHING, 0, 0);
      }
      else fprintf(stderr, trap_msg[27]);
    }

    /*-------------------------------------------------*
     * configOn name: show configuration on file name. *
     *-------------------------------------------------*/

    else if(strcmp(cmd, "configOn") == 0) {
      char filename[MAX_FILE_NAME_LENGTH + 1];

      get_name_rest_of_line(filename, MAX_FILE_NAME_LENGTH, c);
      if(program_is_running) {
        print_rts_on_file_name(filename, current_act);
      }
      else fprintf(stderr, trap_msg[27]);
    }

    /*----------------------------------------*
     * c, cont: return, continuing execution. *
     *----------------------------------------*/

    else if(strcmp(cmd, "c") == 0 || strcmp(cmd, "cont") == 0) {
      clear_rest_of_line(c);
      return;
    }

    /*----------------------------------------------------------*
     * frame n: Show the n-th stack frame from the top, where	*
     *          the top frame is frame 0.			*
     *----------------------------------------------------------*/

    else if(strcmp(cmd, "frame") == 0) {
      int n,k;
      if(c != '\n') k = scanf("%d", &n);
      else k = 0;
      clear_rest_of_line(c);
      if(k > 0) {
	if(program_is_running) {
          RTS_INFO info;
	  remember_rts_info(&info);
	  show_stack_frame(stderr, current_act, n);
	  restore_rts_info(&info);
	}
	else fprintf(stderr, trap_msg[27]);
      }
      else fprintf(stderr, "usage: frame n (where n is an integer)\n");
    }

    /*------------------------*
     * ?, help: Provide help. *
     *------------------------*/

    else if(strcmp(cmd, "?") == 0 || strcmp(cmd, "help") == 0) {
      clear_rest_of_line(c);
      fprintf(stderr, "%s", trap_msg[17]);
    }

    /*---------------------------------------*
     * helpBreak: Provide help about breaks. *
     *---------------------------------------*/

    else if(strcmp(cmd, "helpBreak") == 0) {
      clear_rest_of_line(c);
      fprintf(stderr, "%s", trap_msg[50]);
    }

    /*--------------------------------------------------*
     * waiting: Show the things that are waiting for	*
     *          deferred evaluations to finish.		*
     *--------------------------------------------------*/

    else if(strcmp(cmd, "waiting") == 0) {
      clear_rest_of_line(c);
      show_waiting(current_act);
    }

    /*----------------------------------------------------*
     * backTo: Move context to the activation n back on   *
     *         the run-time shadow stack.		  *
     *----------------------------------------------------*/

    else if(strcmp(cmd, "backTo") == 0) {
      int n,k;
      LIST *p;

      if(c != '\n') k = scanf("%d", &n);
      else k = 0;
      clear_rest_of_line(c);

      for(n = 0, p = runtime_shadow_st;
	  n < k && p != NIL;
	  n++, p = p->tail) {}

      if(p == NIL) {
	fprintf(stderr, trap_msg[48]);
      }
      else {
        current_act = p->head.act;
	print_node_summary(stderr, k, current_act->name, 
			   current_act->program_ctr);
      }
    }

    /*--------------------------*
     * quit: abort the program. *
     *--------------------------*/

    else if(strcmp(cmd, "quit") == 0) {
      clear_rest_of_line(c);
      if(!program_is_done) {
        char ch;
        fprintf(stderr, trap_msg[32]);
        ch = getchar();
        if(ch == 'y') clean_up_and_exit(0);
        else clear_rest_of_line(ch);
      }
      else clean_up_and_exit(0);
    }

    /*----------------------------*
     * stack: Show stack briefly. *
     *----------------------------*/

    else if(strcmp(cmd, "stack") == 0) {
      clear_rest_of_line(c);
      if(program_is_running) {
	show_stack_briefly(current_act);
      }
      else fprintf(stderr, trap_msg[27]);
    }

    /*-----------------------------------*
     * term: Force failure and continue. *
     *-----------------------------------*/

    else if(strcmp(cmd, "term") == 0) {
      clear_rest_of_line(c);
      if(program_is_running) {
	failure = TERMINATE_EX;
	return;
      }
      else fprintf(stderr, trap_msg[27]);
    }

    /*----------------------------*
     * top: Show top stack frame. *
     *----------------------------*/

    else if(strcmp(cmd, "top") == 0) {
      clear_rest_of_line(c);
      if(program_is_running) {
        RTS_INFO info;
	remember_rts_info(&info);
	show_stack_frame(stderr, current_act, 0);
	restore_rts_info(&info);
      }
      else fprintf(stderr, trap_msg[27]);
    }

    /*-------------------------------*
     * Here, the command is unknown. *
     *-------------------------------*/

    else {
      fprintf(stderr, trap_msg[28]);
    }
  }

  signal(SIGINT, intrupt);
}
#endif


/****************************************************************
 *			SHOW_CURRENT_LINE			*
 ****************************************************************
 * Show line n of file filename. 				*
 *								*
 * aso_filename is the .aso file name.  It is used to check	*
 * whether the source file has been modified.			*
 ****************************************************************/

PRIVATE void show_current_line(char *filename, char *aso_filename, int n)
{
  int k, c;
  FILE *f;
  struct stat aso_stat, src_stat;

  f = fopen(filename, "r");
  if(f != NULL) {

    /*--------------------------------*
     * Skip over the first n-1 lines. *
     *--------------------------------*/

    c = 0;
    for(k = 1; k < n && c >= 0; k++) {
      do {
        c = getc(f);
      } while(c >= 0 && c != '\n');
    }

    if(c >= 0) {

      /*--------------------------------------------------*
       * Check whether the source file has been modified. *
       *--------------------------------------------------*/

      if(stat(aso_filename, &aso_stat) == 0 &&
         stat(filename, &src_stat) == 0 &&
         aso_stat.st_mtime < src_stat.st_mtime) {
	fprintf(stderr, trap_msg[46]);
      }

      /*-----------------*
       * Print line n.   *
       *-----------------*/

      fprintf(stderr, "%3d ", n);
      do {
        c = getc(f);
        if(c < 0) {putc('\n', stderr); break;}
        putc(c, stderr);
      } while(c != '\n');
    }
    fclose(f);
  }
}


/****************************************************************
 *			PRINT_BREAK_HEAD			*
 ****************************************************************
 * Print information about where a break of kind kind happened. *
 * See vis_break for the meanings of the kinds.			*
 ****************************************************************/

PRIVATE void print_break_head(int kind)
{
  struct package_descr *pd;
  struct line_rec *lr;
  char *file_name, *pack_name;
  int line;

  /*------------------------------------------*
   * Get the package and file names and line. *
   *------------------------------------------*/

  pd = get_pd_entry(break_info.pc);
  if(pd == NULL) {
    pack_name = NULL;
    file_name = NULL;
  }
  else {
    if(pd->imp_name != NULL) {
      LONG offset = break_info.pc - pd->begin_addr;
      if(offset >= pd->imp_offset) {
	pack_name = pd->imp_name;
        file_name = ast_name(pd->file_name, 0);
      }
      else {
	pack_name = pd->name;
	file_name = ast_name(pd->file_name, 1);
      }
    }
    else {
      pack_name = pd->name;
      file_name = ast_name(pd->file_name, 1);
    }
    lr   = get_linerec(pd, break_info.pc);
    line = lr->line;
  }

  /*---------------------------------------------------*
   * Show the common heading: package and line number. *
   *---------------------------------------------------*/

  program_is_done = FALSE;
  if(kind == NONE_VIS) {}
  else if(kind == DONE_VIS) {
    fprintf(stderr, trap_msg[30]);
    program_is_done = TRUE;
  }
  else fprintf(stderr, trap_msg[31], nonnull(pack_name), 
	       quick_display_name(the_act.name), line);

  /*--------------------------------*
   * Show the specific information. *
   *--------------------------------*/

  fprintf(stderr, "==>");
  switch(kind) {
    case NONE_VIS:
    case EXEC_END_VIS:
    case NAME_VIS:
    case TRAP_VIS:
    case DONE_VIS:
      break;

    case LET_VIS:
      {NAME_TYPE info;
       LIST *bl;
       LONG offset;

       bl = get_act_binding_list(&the_act);
       offset = (pd != NULL) ? the_act.program_ctr - pd->begin_addr : -1;
       get_one_env_info(&info, the_act.env, offset, bl, 
			break_info.vis_let_offset);
       fprintf(stderr, trap_msg[33], nonnull(info.name));
       fprint_ty(stderr, info.type);
       if(break_info.show_val) {
         fprintf(stderr, trap_msg[34]);
         print_entity_with_state(*top_stack(), info.type, stderr, 
			         the_act.state_a, no_trap_tv, FALSE, 
			         rts_file_max_entity_chars);
       }
       fnl(stderr);
       break;
      }

    case RETURN_VIS:
      fprintf(stderr, trap_msg[35], quick_display_name(the_act.name));
      if(break_info.show_val && the_act.result_type_instrs != NULL) {
	 TYPE* return_type;
	 eval_type_instrs(the_act.result_type_instrs, the_act.env);
	 return_type = pop_type_stk();
         print_entity_with_state(*top_stack(), return_type, stderr, 
			         the_act.state_a, no_trap_tv, FALSE, 
			         rts_file_max_entity_chars);
	 fprintf(stderr, ": ");
	 fprint_ty(stderr, return_type);
	 fnl(stderr);
	 drop_type(return_type);
       }
       fnl(stderr);
       break;

    case FAIL_VIS:
      fprintf(stderr, trap_msg[36]);
      if(break_info.show_val) {
        fprintf(stderr, trap_msg[37]);
        print_entity_with_state(last_exception, exception_type, stderr, 
			        the_act.state_a, no_trap_tv, FALSE, 
			        rts_file_max_entity_chars);
      }
      fnl(stderr);
      break;

    case RESUME_VIS:
      fprintf(stderr, trap_msg[38]);
      break;

    case LAZY_VIS:
      fprintf(stderr, trap_msg[39]);
      break;

    case END_LAZY_VIS:
      fprintf(stderr, trap_msg[40]);
      if(break_info.show_val && the_act.result_type_instrs != NULL) {
	 TYPE* result_type;
	 fprintf(stderr, trap_msg[49]);
	 eval_type_instrs(break_info.result_type_instrs, the_act.env);
	 result_type = pop_type_stk();
         print_entity_with_state(break_info.result, result_type, stderr, 
			         the_act.state_a, no_trap_tv, FALSE, 
			         rts_file_max_entity_chars);
	 fprintf(stderr, ": ");
	 fprint_ty(stderr, result_type);
	 fnl(stderr);
	 drop_type(result_type);
       }
      break;

    case APPLY_VIS:
      fprintf(stderr, trap_msg[41], quick_display_name(the_act.name));
      break;

    case EXEC_BEGIN_VIS:
      fprintf(stderr, trap_msg[44]);
      break;

    case ASSIGN_VIS:
      fprintf(stderr, trap_msg[42]);
      break;

    case PRINT_VIS:
      fprintf(stderr, trap_msg[43]);
      break;
  }

  /*-----------------------*
   * Show the source line. *
   *-----------------------*/

  if(file_name != NULL) {
    fnl(stderr);
    show_current_line(file_name, pd->file_name, line);
  }

}


/****************************************************************
 *			VIS_BREAK				*
 ****************************************************************
 * Break the interpreter and start debugger interaction.        *
 * Report the location of the break.				*
 *								*
 * kind tells what kind of instruction caused the break, and    *
 * has the following values.					*
 *								*
 * NONE_VIS		Nothing is currently running 		*
 *								*
 * LET_VIS		About to do a let 			*
 *								*
 * RETURN_VIS		About to return from a call 		*
 *								*
 * NAME_VIS		About to name a function 		*
 *								*
 * TRAP_VIS		Have executed a trap 			*
 *								*
 * FAIL_VIS		Have failed 				*
 *								*
 * RESUME_VIS		About to resume a coroutine 		*
 *								*
 * LAZY_VIS		About to do a lazy evaluation 		*
 *								*
 * END_LAZY_VIS		About to end a lazy evaluation 		*
 *								*
 * APPLY_VIS		About to apply a function 		*
 *								*
 * EXEC_BEGIN_VIS  	About to begin an execute 		*
 *								*
 * EXEC_END_VIS    	About to end an execute 		*
 *								*
 * ASSIGN_VIS		About to do assign 			*
 *								*
 * PRINT_VIS		About to do print	 		*
 *								*
 * DONE_VIS		Computation is finished.		*
 ****************************************************************/

void vis_break(int kind)
{
  print_break_head(kind);
  debug_interact();
}


/****************************************************************
 *			POSSIBLY_ABORT_DIALOG			*
 ****************************************************************
 * Ask the user whether to abort to continue.  If the user says *
 * no, then exit.  If the user says yes, then return 0 for y,   *
 * 1 for Y.							*
 ****************************************************************/

#ifdef UNIX
int possibly_abort_dialog(char *message)
{
  int c;

  flush_stdout();
  fprintf(stderr, message);
  read_trap_msgs();
  for(;;) {
    fprintf(stderr, trap_msg[15]);
    do {
      c = getchar();
    } while (c == ' ' || c == '\n');

    if(c == 'y') return 0;

    else if(c == 'Y') return 1;

    else if(c == 'N') clean_up_and_exit(1);

    else if(c == 'd') {
      debug_interact();
      return 0;
    }

    else if(c == '?') {
      fprintf(stderr, trap_msg[16]);
    }

    else if(c == 'w' || c == 'n' || c == EOF) {
      TYPE_HOLD info;
      info.type_stack = NULL;
      if(!suppress_print_rts_in_abort) {
        fprintf(stderr, trap_msg[18]);
	print_rts(&the_act, ENTU(INTERRUPT_EX), 0, the_act.program_ctr);
      }
      if(c != 'w') clean_up_and_exit(1);
    } 
  }
}
#endif

/*-----------------------------------------------------------------*/

#ifdef MSWIN
int possibly_abort_dialog(char *message)
{
  int c = MessageBox(MainWindowHandle, message, "Astr Message", MB_YESNO);
  if(c != IDYES) failure = INTERRUPT_EX;
  else if(!suppress_print_rts_in_abort) {
    int d = MessageBox(MainWindowHandle, "Dump ast.rts?", 
		       "Astr Message", MB_YESNO);
    if(d == IDYES) {
      print_rts(&the_act, ENTU(INTERRUPT_EX), 0, the_act.program_ctr);
    }
  }
  return 0;
}
#endif


