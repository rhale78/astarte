/**********************************************************************
 * File:    machdata/except.c
 * Purpose: Functons for dealing with exceptions (as entities).
 *          These functions are not the exception handling ones.  They
 *          are found in machstrc/control.c and evaluate/fail.c.
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

#include <string.h>
#ifdef USE_MALLOC_H
#  include <malloc.h>
#else
#  include <stdlib.h>
#endif
#include "../misc/misc.h"
#include "../alloc/allocate.h"
#include "../utils/lists.h"
#include "../machdata/entity.h"
#include "../machdata/except.h"
#include "../intrprtr/intrprtr.h"
#include "../evaluate/evaluate.h"
#include "../evaluate/instruc.h"
#include "../tables/tables.h"
#include "../show/printrts.h"
#include "../show/prtent.h"
#include "../show/gprint.h"
#include "../standard/stdtypes.h"
#include "../standard/stdids.h"
#include "../gc/gc.h"
#include "../rts/rts.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif
#ifdef MSWIN
 extern void flush_console_buff(void);
 extern void out_str(char *s);
 extern void setConsoleTerminated(void);
#endif

extern char **prefix_instr_name[];

/************************************************************************
 *				PUBLIC VARIABLES			*
 ************************************************************************/

/********************************************************
 *			exception_data			*
 ********************************************************
 * exception_data[i] holds data about exception number  *
 * i, including the name and the type of data that is   *
 * stored with it.  The type is stored as a sequence of *
 * instructions that will build the type and put it on  *
 * the type stack when they are executed.		*
 ********************************************************/

struct exception_data_struct FARR exception_data[256];

#ifdef MACHINE
/********************************************************
 *			trap_vec_size			*
 ********************************************************
 * trap_vec_size is the number of longs used to hold a  *
 * trap vector.						*
 ********************************************************/

int trap_vec_size;

/********************************************************
 *			next_exception			*
 ********************************************************
 * next_exception is the number that will be assigned   *
 * to the next new exception.  It is used and updated   *
 * in package.c.					*
 ********************************************************/

int next_exception;

/********************************************************
 *			no_trap_tv			*
 ********************************************************
 * no_trap_tv is a trap vector that indicates that no   *
 * exceptions are to be trapped.			*
 ********************************************************/

TRAP_VEC* no_trap_tv;

/************************************************************************
 *				PRIVATE VARIABLES			*
 ************************************************************************/

/****************************************************************
 *			trap_list				*
 ****************************************************************
 * trap_list contains the numbers of all nonstandard exceptions *
 * that are trapped by default.					*
 ****************************************************************/

PRIVATE INT_LIST* trap_list = NULL;

#endif

/*********************************************************************
 *			INIT_EXCEPTION_NAMES			     *
 *********************************************************************
 * init_exception_names sets up the exception_data structure.	     *
 *								     *
 * string_instrs is a sequence of instructions that create species   *
 * [Char].  The missing byte is set to hold Char_std_num, the 	     *
 * standard number of species Char.  That is not known at compile    *
 * time.							     *
 *********************************************************************/

#ifdef MACHINE
UBYTE string_instrs[] = 
  {STAND_T_I, 0 /* replaced below */, LIST_T_I, END_LET_I};
#endif

/*---------------------------------------------------------------*/

#ifdef TRANSLATOR
# define make_perm_str(s) (s)
# define install_exc_info(num, id, c) install_tr_exc_info(num, id)

  PRIVATE void install_tr_exc_info(int exc_num, int exc_id)
  {
    exception_data[exc_num].name = std_id[exc_id];
  }
#else
  PRIVATE void install_exc_info(int exc_num, int exc_id, char *comment)  
  {
    exception_data[exc_num].name = make_perm_str(std_id[exc_id]);
    exception_data[exc_num].descr = comment;
  }
#endif

void init_exception_names(void)
{
  int i;
  char* s = make_perm_str("unknown exception");

  for(i = 0; i < 256; i++) {
    exception_data[i].name = s;
    exception_data[i].type_instrs = NULL;
    exception_data[i].descr = "";
  }

  install_exc_info(TEST_EX, TESTX_ID, 
    "A test failed");
  install_exc_info(MEM_EX,  MEMX_ID,  
    "Memory is exhausted");
  install_exc_info(ENDTHREAD_EX, ENDTHREADX_ID,
    "The thread terminated itself");
  install_exc_info(EMPTY_BOX_EX, EMPTYBOXX_ID,
     "Attempt to take the content of an empty box");
  install_exc_info(EMPTY_LIST_EX, EMPTYLISTX_ID,
     "Attempt to take the head or tail of an empty list");
  install_exc_info(DOMAIN_EX, DOMAINX_ID,
     "Attempt to evaluate a function with a parameter not in its domain");
  install_exc_info(GLOBAL_ID_EX, NODEFX_ID,
     "Attempt to get an undefined identifier from the outer scope");
  install_exc_info(CONVERSION_EX, CONVERSIONX_ID,
     "Impossible conversion");
  install_exc_info(SIZE_EX, SIZEX_ID,
     "Size mismatch");
  install_exc_info(NO_CASE_EX, NOCASEX_ID,
     "There was no successful case in a choose, loop or definition by cases.\n"
     "  Sometimes this occurs because the target of a match fails.");
  install_exc_info(NO_FILE_EX, NOFILEX_ID,
     "File does not exist, or has incorrect permissions");
  install_exc_info(SPECIES_EX, SPECIESX_ID,
     "Species mismatch at run time");
  install_exc_info(INF_LOOP_EX, INFINITELOOPX_ID,
     "Attempt to use a lazy value while evaluating that value");
  install_exc_info(INTERRUPT_EX, INTERRUPTX_ID,
     "An interrupt occurred");
  install_exc_info(CANNOT_SEEK_EX, CANNOTSEEKX_ID,
     "Attempt to do a seek on a nonseekable file");
  install_exc_info(DO_COMMAND_EX, SYSTEMX_ID,
     "Execution of a system command failed");
  install_exc_info(TOO_MANY_FILES_EX, TOOMANYFILESX_ID,
     "Too many open files");
  install_exc_info(INPUT_EX, BADINPUTX_ID,
     "Input has incorrect form");
  install_exc_info(LIMIT_EX, LIMITX_ID,
     "A compiled-in limit, such as a file name length limit, has been exceeded");
  install_exc_info(BIND_EX, BINDUNKNOWNX_ID,
     "Attempt to bind an unknown failed, possiby because the value was known");
  install_exc_info(NO_FONT_EX, NOFONTX_ID,
     "Font does not exist");
  install_exc_info(CLOSED_FILE_EX, CLOSEDFILEX_ID,
     "Attempt to write to a closed file");
  install_exc_info(ENSURE_EX, ENSUREX_ID,
     "A condition that should have been ensured has failed");
  install_exc_info(REQUIRE_EX, REQUIREX_ID, 
    "A requirement has failed");
  install_exc_info(SUBSCRIPT_EX, SUBSCRIPTX_ID,
     "Subscript out of bounds");
  install_exc_info(TERMINATE_EX, TERMINATEX_ID,
     "The thread was terminated by an outside agent");
  install_exc_info(LISTEXHAUSTED_EX, LISTEXHAUSTEDX_ID,
     "Attempt to pull a value from an empty list");

# ifdef MACHINE
  string_instrs[1] = Char_std_num;
  exception_data[DOMAIN_EX].type_instrs    = string_instrs;
  exception_data[NO_FILE_EX].type_instrs   = string_instrs;
  exception_data[GLOBAL_ID_EX].type_instrs = string_instrs;
  exception_data[NO_FONT_EX].type_instrs   = string_instrs;
  exception_data[ENSURE_EX].type_instrs    = string_instrs;
  exception_data[REQUIRE_EX].type_instrs   = string_instrs;

  next_exception = LAST_EX + 1;
# endif
}


#ifdef MACHINE

/*********************************************************************
 *			EXCEPTION_STRING_STDF			     *
 *********************************************************************
 * Return the description of exception ex, as an ENTITY.	     *
 *********************************************************************/

ENTITY exception_string_stdf(ENTITY ex)
{
  int   n     = qwrap_tag(ex);
  char* descr = exception_data[n].descr;
  return make_cstr(descr);
}


/*********************************************************************
 *			INIT_EXCEPTIONS				     *
 *********************************************************************
 * Set up trap information about exceptions.			     *
 *********************************************************************/

void init_exceptions()
{
  int i;
  INT_LIST *p;

  trap_vec_size = 1 + (next_exception - 1) / LONG_BITS;

  no_trap_tv      = allocate_trap_vec();
  global_trap_vec = allocate_trap_vec();
  no_trap_tv->ref_cnt = global_trap_vec->ref_cnt = 1;
  for(i = 0; i < trap_vec_size; i++) {
    no_trap_tv->component[i]      = 0;
    global_trap_vec->component[i] = 0;
  }

  /*------------------------------------*
   * Set traps for standard exceptions. *
   *------------------------------------*/

  set_trap(MEM_EX, 		global_trap_vec);
  set_trap(EMPTY_BOX_EX, 	global_trap_vec);
  set_trap(EMPTY_LIST_EX, 	global_trap_vec);
  set_trap(SUBSCRIPT_EX, 	global_trap_vec);
  set_trap(DOMAIN_EX, 		global_trap_vec);
  set_trap(GLOBAL_ID_EX, 	global_trap_vec);
  set_trap(SIZE_EX, 		global_trap_vec);
  set_trap(NO_CASE_EX, 		global_trap_vec);
  set_trap(INF_LOOP_EX, 	global_trap_vec);
  set_trap(INTERRUPT_EX, 	global_trap_vec);
  set_trap(CANNOT_SEEK_EX, 	global_trap_vec);
  set_trap(REQUIRE_EX, 		global_trap_vec);
  set_trap(ENSURE_EX, 		global_trap_vec);
  set_trap(LIMIT_EX, 		global_trap_vec);
  set_trap(TOO_MANY_FILES_EX, 	global_trap_vec);

  /*------------------------------------------------------*
   * Set traps for nonstandard exceptions from trap_list. *
   *------------------------------------------------------*/

  for(p = trap_list; p != NIL; p = p->tail) {
    set_trap(toint(p->head.i), global_trap_vec);
  }
}


/****************************************************************
 *			TRAP_BY_DEFAULT				*
 ****************************************************************
 * Record that exception ex is trapped by default.		*
 ****************************************************************/

void trap_by_default(int exc_num)
{
  SET_LIST(trap_list, int_cons(exc_num, trap_list));
}


/****************************************************************
 *			CLEAR_TRAP				*
 ****************************************************************
 * Clear trap for exception ex in trap vector tr.  This is what *
 * is done at an untrap expression.  If ex is ALL_TRAP, then    *
 * clear all traps in vector tr.				*
 ****************************************************************/

void clear_trap(int ex, TRAP_VEC *tr)
{
  int i;

  if(ex == ALL_TRAP) {
    for(i = 0; i < trap_vec_size; i++) {
      tr->component[i] = 0;
    }
  }
  else {
    tr->component[ex >> LOG_LONG_BITS] &= ~(1L << (ex & LOG_LONG_BITS_MASK));
  }
}  


/****************************************************************
 *			SET_TRAP				*
 ****************************************************************
 * Set a trap for exception ex in trap vector tr.  This is done *
 * at a trap expression.  If ex is ALL_TRAP, then set all traps *
 * in vector tr.						*
 ****************************************************************/

void set_trap(int ex, TRAP_VEC *tr)
{
  int i;

  if(ex == ALL_TRAP) {
    for(i = 0; i < trap_vec_size; i++) {
      tr->component[i] = ~0;
    }
  }
  else {
    tr->component[ex >> LOG_LONG_BITS] |= (1L << (ex & LOG_LONG_BITS_MASK));
  }
}


/****************************************************************
 *			COPY_TRAP_VEC				*
 ****************************************************************
 * Return a copy of tr.						*
 ****************************************************************/

TRAP_VEC* copy_trap_vec(TRAP_VEC *tr)
{
  int i;
  TRAP_VEC* result = allocate_trap_vec();

  for(i = 0; i < trap_vec_size; i++) result->component[i] = tr->component[i];
  return result;
}


/****************************************************************
 *			WILL_TRAP_STDF				*
 ****************************************************************
 * Return true if should trap exception ex.			*
 ****************************************************************/

ENTITY will_trap_stdf(ENTITY ex)
{
  return ENTU(should_trap(qwrap_tag(ex)));
}

#endif
