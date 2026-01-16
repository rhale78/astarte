/********************************************************************
 * File:    debug/m_debug.c
 * Purpose: Functions to help debugging machine
 * Author:  Karl Abrahamson
 ********************************************************************/

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
 * This file contains functions and data for use only by the		*
 * interpreter, and only when debugging is turned on.			*
 ************************************************************************/

#include <stdarg.h>
#include <string.h>
#ifdef USE_MALLOC_H
#  include <malloc.h>
#else
#  include <stdlib.h>
#endif
#include "../misc/misc.h"

#ifdef DEBUG

#include "../parser/tokens.h"
#include "../alloc/allocate.h"
#include "../utils/hash.h"
#include "../utils/lists.h"
#include "../utils/rdwrt.h"
#include "../classes/classes.h"
#include "../standard/stdtypes.h"
#include "../exprs/expr.h"
#include "../tables/tables.h"
#include "../clstbl/classtbl.h"
#include "../debug/debug.h"
#include "../show/prtent.h"
#include "../intrprtr/intrprtr.h"
#include "../evaluate/evaluate.h"
#include "../evaluate/instruc.h"
#include "../machdata/entity.h"
#include "../machdata/gc.h"
#include "../machdata/except.h"
#include "../machstrc/machstrc.h"
#include "../gc/gc.h"

extern char **instr_name, **un_instr_name, **bin_instr_name;
extern char **prefix_instr_name[];
extern char *list1_instr_name[];
extern char *list2_instr_name[];
extern char *ty_instr_name[];
extern char *unk_instr_name[];


/****************************************************************
 *			VARIABLES				*
 ****************************************************************/

/****************************************************************
 *			gctrace					*
 *			gcshow					*
 *			rctrace					*
 *			alloctrace				*
 ****************************************************************
 * gctrace	Set gctrace nonzero to trace garbage 		*
 *		collection.  See  gc/gc.c.			*
 *								*
 * gcshow	Set gcshow just to see when garbage collection	*
 *		is done, without seeing what happens.		*
 *								*
 * smallgctrace Set smallgctrace nonzero to do a minimal 	*
 *		trace of garbage collection.			*
 *								*
 * rctrace	Set rctrace nonzero to trace action of 		*
 *		reference count management for machine types	*
 *		See alloc/mrcalloc.c				*
 *								*
 * alloctrace	Set alloctrace to trace allocation of garbage-	*
 *		collectible storage.  See alloc/gcalloc.c	*
 *		and gc/gc.c.					*
 ****************************************************************/

UBYTE gctrace = 0, 
      gcshow = 0, 
      rctrace = 0,
      alloctrace = 0,
      smallgctrace = 0;

/****************************************************************
 *			trace_puts				*
 ****************************************************************
 * Set trace_puts nonzero to see what package reader puts into	*
 * internal packages.  See intrprtr/package.c,			*
 * evaluate/typeinst.c.						*
 ****************************************************************/

UBYTE trace_puts = 0;

/****************************************************************
 *			statetrace				*
 ****************************************************************
 * Set statetrace nonzero to trace functions that manipulate	*
 * states, which are used to store bindings of nonshared	*
 * boxes.  See machstrc/state.c.				*
 ****************************************************************/

UBYTE statetrace = 0;

/****************************************************************
 *			trace_env_descr				*
 ****************************************************************
 * Set trace_env_descr to trace the construction of environment	*
 * descriptors, which are used to determine the names 		*
 * associated with particular offsets in environments, for	*
 * dumping ast.rts, etc.  See intrprtr/package.c, 		*
 * tables/m_glob.c.						*
 ****************************************************************/

UBYTE trace_env_descr = 0;

/****************************************************************
 *			trace_print_rts				*
 ****************************************************************
 * Set trace_print_rts to cause action of print_rts to be 	*
 * traced.  See show/printrts.c.				*
 ****************************************************************/

UBYTE trace_print_rts = 0;

/****************************************************************
 *			time_out_often				*
 ****************************************************************
 * Set time_out_often to cause the interpreter to artifically	*
 * break off execution of threads fairly often.  Use this to	*
 * test thread switching.					*
 ****************************************************************/

UBYTE time_out_often = 0;

/****************************************************************
 *			trace_global_eval			*
 ****************************************************************
 * Set trace_global_eval nonzero to trace the evaluation of	*
 * global identifiers (those defined in the outer environment.)	*
 * See tables/m_glob.c and evaluate/lazy.c.			*
 ****************************************************************/

UBYTE trace_global_eval = 0;

/****************************************************************
 *			trace_env				*
 *			trace_state				*
 *			trace_tv				*
 *			trace_control				*
 ****************************************************************
 * Set these to control what is shown in an execution trace.	*
 * 								*
 * trace_env	Show the environment after each instruction	*
 *								*
 * trace_state	Show the state after each instruction.		*
 *								*
 * trace_tv	Show the trap vector after each instruction	*
 *								*
 * trace_control Show the control after each instruction.	*
 *								*
 * See evaluate/evaluate.c.					*
 ****************************************************************/

UBYTE trace_env = 0;
UBYTE trace_state = 0;
UBYTE trace_tv = 0;
UBYTE trace_control = 0;

/****************************************************************
 *			trace_types				*
 ****************************************************************
 * When trace_types is nonzero, execution of type building 	*
 * instructions is traced.  See evaluate/typeinst.c,		*
 * tables/typehash.c.						*
 ****************************************************************/

UBYTE trace_types = 0;

/****************************************************************
 *			trace_extra				*
 ****************************************************************
 * When trace_extra is nonzero, a few things are traced.	*
 *								*
 *  rts/io.c:  Handling of files.				*
 *								*
 *  rst/product.c: sublist computations and packing.		*
 ****************************************************************/

UBYTE trace_extra = 0;


/****************************************************************
 *			trace_mem				*
 ****************************************************************
 * When trace_mem is nonzero, status of reference-counted 	*
 * memory is traced.  See alloc/mrcalloc.c.			*
 ****************************************************************/

UBYTE trace_mem = 0;

/****************************************************************
 *			trace_docmd				*
 ****************************************************************
 * Trace action of system command execution.  See		*
 * rts/system.c.						*
 ****************************************************************/

UBYTE trace_docmd = 0;

/****************************************************************
 *			trace_fun				*
 ****************************************************************
 * trace_fun is a list of the names of functions whose		*
 * executions should be traced, when trace is nonzero. 		*
 * If trace_fun is nonnull,  then only these functions are	*
 * traced.  If trace_fun is null, then all functions are traced.*
 ****************************************************************/

LIST* trace_fun = NULL;

/****************************************************************
 *			debug_msg_i				*
 ****************************************************************
 * When a debug message needs to be printed (by trace_i), we    *
 * need to read the messages from file dmsgi.txt.  debug_msg_i	*
 * is set to point to an array of strings, namely the lines of	*
 * dmsgi.txt.  Until the first trace_i is done, however, 	*
 * debug_msg_i is left at NULL.					*
 ****************************************************************/

PRIVATE char** debug_msg_i = NULL;

/********************************************************
 *			READ_DEBUG_MESSAGES		*
 ********************************************************
 * Read debug_msg_i and debug_msg_s.			*
 ********************************************************/

PRIVATE Boolean have_read_debug_messages = FALSE;

void read_debug_messages(void)
{
  if(!have_read_debug_messages) {
    char* s = 
      (char*) BAREMALLOC(strlen(MESSAGE_DIR) + strlen(DEBUG_MSG_S_FILE) + 1);
    sprintf(s, "%s%s", MESSAGE_DIR, DEBUG_MSG_S_FILE);
    init_err_msgs(&debug_msg_s, s, NUM_DEBUG_MSG_S);
    sprintf(s, "%s%s", MESSAGE_DIR, DEBUG_MSG_I_FILE);
    init_err_msgs(&debug_msg_i, s, NUM_DEBUG_MSG_I);
    have_read_debug_messages = TRUE;
    FREE(s);
  }
}


/****************************************************************
 *			TRACE_I					*
 ****************************************************************
 * print debug_msg_i[f] as a format, with optional arguments	*
 * to fprintf, on the trace file.				*
 ****************************************************************/

void trace_i(int f, ...)
{
  va_list args;

  va_start(args,f);
  read_debug_messages();
  vfprintf(TRACE_FILE, debug_msg_i[f], args);
  fflush(TRACE_FILE);
  va_end(args);
}


/********************************************************
 *			INTERPRETER_DEBUG_TABLE		*
 ********************************************************
 * An entry of the form					*
 *							*
 *    {s,v,i}						*
 *							*
 * in this array indicates that option -Ds indicates    *
 * that variable v should be set to value i.		*
 *							*
 * The last entry must be {"", NULL, 0}.		*
 ********************************************************/

struct interpreter_debug {
  char *opt_name;
  UBYTE *varbl;
  UBYTE val;
} FARR interpreter_debug_table[] =
{
  {"I", 	&trace, 		1},
  {"STATE", 	&trace_state, 		1},
  {"CONTROL", 	&trace_control, 	1},
  {"CONTROL2", 	&trace_control, 	2},
  {"ENV", 	&trace_env, 		1},
  {"ENV2", 	&trace_env, 		2},
  {"TV", 	&trace_tv, 		1},
  {"TYPE", 	&trace_types, 		1},
  {"EXTRA", 	&trace_extra, 		1},
  {"GLOBAL", 	&trace_global_eval, 	1},
  {"RC", 	&rctrace, 		1},
  {"SGC", 	&smallgctrace, 		1},
  {"GC", 	&gctrace, 		1},
  {"GC2", 	&gctrace, 		2},
  {"GC0", 	&gcshow, 		1},
  {"MEM", 	&trace_mem, 		1},
  {"ST", 	&statetrace, 		TRUE},
  {"AL", 	&alloctrace, 		1},
  {"AL2", 	&alloctrace, 		2},
  {"TPS", 	&trace_puts, 		TRUE},
  {"ENS", 	&trace_env_descr, 	TRUE},
  {"RTS", 	&trace_print_rts, 	TRUE},
  {"CLT", 	&trace_classtbl, 	1},
  {"CLT3", 	&trace_classtbl, 	3},
  {"TO", 	&time_out_often, 	1},
  {"CMDMAN", 	&trace_docmd, 		1},
  {"UNI", 	&trace_unify, 		1},
  {"UNI2", 	&trace_unify, 		2},
  {"OV2", 	&trace_overlap, 	2},
  {"OV", 	&trace_overlap, 	1},
  {"", 		NULL, 			0}};


/********************************************************
 *			SET_INTERPRETER_DEBUG		*
 ********************************************************
 * Handle debug option, and return true on success.	*
 * Return false if the option is unknown.		*
 ********************************************************/

Boolean set_interpreter_debug(char *opt)
{
  struct interpreter_debug *p;

  for(p = interpreter_debug_table; p->varbl != NULL; p++) {
    if(strcmp(opt, p->opt_name) == 0) {
      *(p->varbl) = p->val;
      return TRUE;
    }
  }
  return FALSE;
}


/********************************************************
 *			CHECK_CONT_RC			*
 ********************************************************
 * Check that the ref count of c is not negative. 	*
 ********************************************************/

#ifdef GCTEST

void check_cont_rc(CONTINUATION *c)
{
  if(c->ref_cnt < 0) badrc("continuation", toint(c->ref_cnt), (char *) c);
}

#endif

/****************************************************************
 *			PRINT_TWO_TYPES				*
 ****************************************************************
 * print_two_types(t1,t2) prints type t, in short format, on 	*
 * separate lines, on the trace file.  There is a newline at 	*
 * the end of each of the two lines.				*
 ****************************************************************/

void print_two_types(TYPE *t1, TYPE *t2)
{
  PRINT_TYPE_CONTROL ctl;
  FILE_OR_STR ff;
  ff.offset = -1;
  ff.u.file = TRACE_FILE;
  begin_printing_types(&ff, &ctl);
  print_ty1_with_constraints(t1, &ctl);
  tracenl();
  print_ty1_with_constraints(t2, &ctl);
  tracenl();
  end_printing_types(&ctl);
}


/********************************************************
 *			PRINT_COROUTINES		*
 ********************************************************
 * Show the coroutines in list cl on the trace file.	*
 ********************************************************/

void print_coroutines(LIST *cl)
{
  LIST *pr;

  if(cl != NULL) {
    fprintf(TRACE_FILE, "coroutines: ");
    for(pr = cl->head.list; pr != NIL; pr = pr->tail) {
      fprintf(TRACE_FILE, "%s ", pr->head.act->name);
    }
    fprintf(TRACE_FILE, "| ");
    for(pr = cl->tail; pr != NIL; pr = pr->tail) {
      fprintf(TRACE_FILE, "%s ", pr->head.act->name);
    }
    tracenl();
  }
}



/*********************************************************
 *			PRINT_TRAPVEC			 *
 *********************************************************
 * Print trap vector tr in debug form on the trace file. *
 *********************************************************/

void print_trapvec(TRAP_VEC *tr)
{
  int i;

  fprintf(TRACE_FILE, "TRAP_VEC: (");
  for(i = 0; i < trap_vec_size; i++) {
    fprintf(TRACE_FILE, "%lx ", tr->component[i]);
  }
  fprintf(TRACE_FILE, ")\n");
}


/********************************************************
 *			PRINT_TRAPVECS			*
 ********************************************************
 * Print list of trap vectors l on the trace file	*
 * in debug form.					*
 ********************************************************/

void print_trapvecs(LIST *l)
{
  trace_i(32);
  if(l == NIL) printf("NIL\n");
  else {
    while(l != NIL) {
      print_trapvec(l->head.trap_vec);
      l = l->tail;
    }
  }
}


/********************************************************
 *			PRINT_INSTRUCTION		*
 ********************************************************
 * Print instruction c, with arguments at location	*
 * addr.  Return the number of bytes in this		*
 * instruction, including all arguments printed.	*
 ********************************************************/

int print_instruction(FILE* where, CODE_PTR addr, int c)
{
  int a, b, x, y;
  LONG l;
  CODE_PTR caddr;
  char *name;

  read_instr_names();
  caddr = addr -1;
  name = (c <= LAST_NORMAL_INSTRUCTION) ? instr_name[c] :
         (c == END_LET_I) 		? "END-LET" :
    	 (c == END_I)     		? "--END" :
	 (c == ENTER_I)   		? "--ENTER" 
					: "UNKNOWN";
  fprintf(where, "%p: %-15s", caddr, name);
  switch(instinfo[c].class) {
      case END_LET_INST:
      case NO_PARAM_INST:
      case NO_TYPE_INST:
        fnl(where);
        return 1;

      case PREF_INST:
        switch(c) {
          case UNARY_PREF_I:
	    a = *addr;
            if(a <= N_UNARIES) {
	      fprintf(where, " %s\n", un_instr_name[a]);
	    }
            else fprintf(where, " UNKNOWN %d\n", a);
	    return 2;
	
          case BINARY_PREF_I:
	    a = *addr;
            if(a <= N_BINARIES) {
	      fprintf(where, " %s\n", bin_instr_name[a]);
	    }
            else fprintf(where, " UNKNOWN %d\n", a);
	    return 2;

          case LIST1_PREF_I:
	    a = *addr;
	    if(a <= N_LISTONES) {
	      fprintf(where, " %s\n", list1_instr_name[a]);
	    }
	    else fprintf(where, " UNKNOWN %d\n", a);
	    return 2;

          case LIST2_PREF_I:
	    a = *addr;
	    if(a <= N_LISTTWOS) {
	      fprintf(where, " %s\n", list2_instr_name[a]);
	    }
	    else fprintf(where, " UNKNOWN %d\n", a);
	    return 2;

	  case UNK_PREF_I:
	    a = *addr;
	    if(a <= N_UNK_STDFS) {
	      fprintf(where, " %s\n", unk_instr_name[a]);
	    }
	    else fprintf(where, " UNKNOWN %d\n", a);
	    return 2;
	}

      case TY_PREF_INST:
	a = *addr;
        if(a <= N_TY_STDFS) fprintf(where, " %s", ty_instr_name[a]);
        else fprintf(where, " UNKNOWN %d", a);
        fprintf(where, " %d\n", addr[1]);
	return 3;
	
      case ENTER_INST:
	a = *addr;
	fprintf(where, " *%d\n", a);
	return 2;

      case LET_INST:
      case RELET_INST:
      case BYTE_PARAM_INST:
      case BYTE_TYPE_INST:
      case EXC_INST:
	a = *addr;
	fprintf(where, " %d\n", toint(a));
	return 2;

      case LONG_NUM_PARAM_INST:
	l = next_int_m(&addr);
	fprintf(where, " %ld\n", l);
	return toint(addr - caddr);

      case LLABEL_PARAM_INST:
	l = next_int_m(&addr);
	fprintf(where, " :%p\n", caddr + l);
	return toint(addr - caddr);

      case BYTE_LLABEL_INST:
	l = next_int_m(&addr);
	a = *(addr++);
	fprintf(where, " %d :%p\n", a, caddr + l);
	return toint(addr - caddr);

      case STOP_G_INST:
	a = *(addr++);                /* Env size index */
	b = toint(next_int_m(&addr)); /* Env descriptor index */
	trace_i(41, a, b);
	return toint(addr - caddr);

      case LLABEL_ENV_INST:
	l = next_int_m(&addr);	       /* Continue offset */
	b = toint(next_int_m(&addr));  /* Type code index */
	x = *(addr++);	 	       /* Env size index */
	y = toint(next_int_m(&addr));  /* Env descr index */
	fprintf(where, ":%p  type:%d  envsize:%d  descr:%d\n",
		caddr + l, b, x, y);
	return toint(addr - caddr);

#     ifdef NEVER
        /*---------------------*
         * Not currently used. *
         *---------------------*/
        case LLABEL_ENV_NUM_INST:
	  d = *(addr++);	          /* Parameter 		*/
	  a = next_int_m(&addr);	  /* Continue offset	*/
	  b = next_int_m(&addr);          /* Type code index	*/
	  x = *(addr++);	 	  /* Env size index	*/
	  y = next_int_m(&addr);	  /* Env descr index	*/
	  fprintf(where, "%d :%p  type:%d  envsize:%d  descr:%d\n",
	          d, caddr + a, b, x, y);
	  return addr - caddr;
#     endif

      case TWO_BYTE_PARAMS_INST:
      case TWO_BYTE_TYPE_INST:
	a = *addr;
	b = addr[1];
	fprintf(where, " %d %d\n", a, b);
	return 3;

      case BYTE_GLABEL_INST:
	a = toint(next_int_m(&addr));
        b = *(addr++);
	fprintf(where, " %d %d\n", a, b);
	return toint(addr - caddr);

      case GLABEL_PARAM_INST:
      case GLABEL_TYPE_INST:
	a = toint(next_int_m(&addr));
	fprintf(where, " %d\n", a);
	return toint(addr - caddr);

      case DEF_INST:
	l = next_int_m(&addr);          /* Continue offset */
	b = *addr++;		      	/* Binding offset  */
	x = toint(next_int_m(&addr));	/* Type code index */
	y = *addr++;			/* Env size byte   */
	c = toint(next_int_m(&addr));	/* Env descr index */
	fprintf(where, " %d  :%p  type:%d  envsize:%d  descr:%d\n",
	        b, caddr + l, x, y, c);
	return toint(addr - caddr);

      default:
	fprintf(where, " (%d)\n", c);
	return 1;

  } /* end switch */
}


/********************************************************
 *			SHOW				*
 ********************************************************
 * Show the status of tables.				*
 ********************************************************/

PRIVATE void print_ent_cell(HASH2_CELLPTR h)
{
  trace_i(45,
         toint(h->key.type->kind),
         (h->key.type->ctc->name),
         toint(TAG(h->val.entity)),
         toint(VAL(h->val.entity)));
}

/*--------------------------------------------------------*/

void show(void)
{
  fprintf(TRACE_FILE, "----SHOW----\n");

  /*--------------------------------------------------*
   ************* Show the entity name table ***********
   *--------------------------------------------------*/

  {int i;
   GLOBAL_TABLE_NODE *q;
   HASH2_TABLE *r;
   trace_i(46, toint(next_ent_num));
   for(i = 0; i < next_ent_num; i++) {
     fprintf(TRACE_FILE, " %s\n", outer_bindings[i].name);
     for(q = outer_bindings[i].poly_table; q != NULL; q = q->next) {
       trace_i(47, 
	      toint(q->mode), toint(q->V->kind), toint(q->packnum), 
	      toint(q->offset));
     }
     r = outer_bindings[i].mono_table;
     scan_hash2(r, print_ent_cell);
   }
 }

  /*-----------------------------------------------*
   *************** Show the type table *************
   *-----------------------------------------------*/

  {int i;
   trace_i(48, toint(next_class_num));
   for(i = 0; i < next_class_num; i++) {
     if(ctcs[i] == NULL) fprintf(TRACE_FILE, "NULL\n");
     else {
       fprintf(TRACE_FILE, " %s", ctcs[i]->name);
       if(ctcs[i]->ty == NULL) fprintf(TRACE_FILE, "(null type)\n");
       else fprintf(TRACE_FILE, "(%d)\n", toint(ctcs[i]->ty->kind));
     }
   }
  }

  /*---------------------------------------------------*
   ************** Show the constant tables *************
   *---------------------------------------------------*/

  {int i;
   trace_i(49, toint(next_const));
   for(i = 0; i < next_const; i++) {
     fprintf(TRACE_FILE, "%3d ", i);
     trace_print_entity(constants[i]);
     tracenl();
   }
  }

  /*---------------------------------------------------*
   ************** Show the package contents ************
   *---------------------------------------------------*/

  {int n, i;
   CODE_PTR package_start, package_end, p;

   for(n = 0; n < num_packages; n++) {
     trace_i(50, n);
     package_start = package_descr[n].begin_addr;
     package_end   = package_descr[n].end_addr;
     for(i = 0, p = package_start; p < package_end; i++,p++){
       trace_i(51, i, toint(*p));
       print_instruction(TRACE_FILE, p+1, toint(*p));
     }
   }
  }

  /*-----------------------------------*
   * Show the package descriptor list. *
   *-----------------------------------*/

  {int j;
   struct package_descr *p;
   struct line_rec *l;
   trace_i(52);
   p = package_descr + current_pack_params->num;
   trace_i(53, p->name, p->begin_addr, p->end_addr);
   for(j = 0; j < p->log_lines_size; j++) {
     l = p->lines + j;
     fprintf(TRACE_FILE, "  %d: line %d\n", toint(l->offset), toint(l->line));
   }
 }

  /*----------------------------------------*
   * Show the environment descriptor table. *
   *----------------------------------------*/

  {struct env_descr *ed;
   CODE_PTR p;
   int i;

   trace_i(54);
   for(i = 0; i < next_env_descr_num; i++) {
     trace_i(55, i);
     for(ed = env_descriptors[i]; ed != NULL; ed = ed->next) {
       trace_i(56, ed->name, toint(ed->env_offset), 
	      tolong(ed->pc_offset));
       p = ed->type_instrs;
       while(*p != END_LET_I) {
	 p += print_instruction(TRACE_FILE, p+1, *p);
       }
     }
   }
  }

# ifdef NEVER
  /************** Show the stack *******************/

  print_stack(the_act.stack);

  /************* Show the current environment *************/

  print_env(the_act.env, the_act.num_entries);

  /************* Show the current program counter ************/

  trace_i(57, the_act.program_ctr); 
# endif

  trace_i(58); 
}


/********************************************************
 *			PRINT_GCEND_INFO		*
 ********************************************************
 * Print information at the end of garbage collection	*
 * indicating the status of available memory.		*
 ********************************************************/

void print_gcend_info()
{
  trace_i(59, get_before_gc);

  /*-----------------------*
   * Data for small reals. *
   *-----------------------*/

  {SMALL_REAL *sr;
   SMALL_REAL_BLOCK *srb;
   LONG n;
   for(srb = small_real_blocks, n = 0; srb != NULL; srb = srb->next) n++;
   trace_i(60, n);
   for(sr = free_small_reals, n = 0; sr != NULL; sr = sr->next) n++;
   trace_i(61, n);
  }

  /*--------------------*
   * Data for entities. *
   *--------------------*/

  {ENT_BLOCK *eb;
   LONG n;

   for(eb = used_blocks, n = 0; eb != NULL; eb = eb->next) n++;
   trace_i(62, n);
   print_entity_wheres();
  }

  /*--------------------*
   * Data for binaries. *
   *--------------------*/

  {BINARY_BLOCK *bb;
   LONG n;

   for(bb = used_binary_blocks, n = 0; bb != NULL; bb = bb->next) n++;
   trace_i(64, n);
   print_binary_wheres();
  }

  /*-------------------*
   * Available blocks. *
   *-------------------*/

  {BINARY_BLOCK* bb;
   LONG n;
   for(bb = avail_blocks, n = 0; bb != NULL; bb = bb->next) n++;
   trace_i(21, n);
  }

  /*-------------------------*
   * Data for file entities. *
   *-------------------------*/

  {struct file_entity *fe;
   struct file_ent_block *feb;
   LONG n;

   for(feb = file_ent_blocks, n = 0; feb != NULL; feb = feb->next) n++;
   trace_i(67, n);
   for(fe = free_file_entities, n = 0; fe != NULL; fe = fe->u.next) n++;
   trace_i(68, n);
  }
}

#endif
