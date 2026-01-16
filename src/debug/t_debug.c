/****************************************************************
 * File:    debug/t_debug.c
 * Purpose: Functions for debugging translator
 * Author:  Karl Abrahamson
 ****************************************************************/

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
 * compiler, and only when debugging is turned on.			*
 ************************************************************************/

#include <string.h>
#include <stdarg.h>
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
#include "../utils/strops.h"
#include "../classes/classes.h"
#include "../exprs/expr.h"
#include "../tables/tables.h"
#include "../debug/debug.h"
#include "../parser/parser.h"
#include "../generate/generate.h"
#include "../infer/infer.h"

extern char yytext[];
extern int yydebug;

/****************************************************************
 *			VARIABLES				*
 ****************************************************************/

/****************************************************************
 * The following are related to similarly named variables in    *
 * debug.c that are used for shared code.  Each indicates that  *
 * when tracing is turned on, the corresponding trace variable  *
 * should be set.  For example, if do_trace_frees is nonzero,   *
 * then when tracing is turned on, trace_frees will be set to	*
 * 1.								*
 ****************************************************************/

PRIVATE UBYTE do_trace_frees = 0;
PRIVATE UBYTE do_trace_infer = 0;
PRIVATE UBYTE do_trace_unify = 0;
PRIVATE UBYTE do_trace_gen = 0;
PRIVATE UBYTE do_trace_classtbl = 0;
PRIVATE UBYTE do_trace_overlap = 0;
PRIVATE UBYTE do_trace_missing = 0;
PRIVATE UBYTE do_trace_complete = 0;
PRIVATE UBYTE do_trace_pat_complete = 0;

/****************************************************************
 *			trace_arg				*
 *			trace_standard				*
 *			trace_imports				*
 *			trace_preamble				*
 ****************************************************************
 * trace_arg is 1 to enable tracing while reading the main 	*
 *	     file that is being compiled.			*
 *								*
 * trace_standard is 1 to enable tracing while reading 		*
 *		  standard.ast					*
 *								*
 * trace_imports is 1 to enable tracing while reading imported  *
 *		 packages					*
 *								*
 * trace_preamble is 1 to enable tracing while doing definitions*
 * 		  that are done before beginning standard.ast.	*
 ****************************************************************/

UBYTE trace_arg = 0, trace_imports = 0;
UBYTE trace_standard = 0, trace_preamble = 0;


/****************************************************************
 *			trace variables				*
 ****************************************************************
 * The following are in pairs.  The variable that does not	*
 * begin with do_ is used to indicate that tracing is being	*
 * done now.  The variable that begins with do_ indicates	*
 * that tracing of the given kind should be turned on when	*
 * tracing is turned on next (as controled by trace_imports, 	*
 * etc.								*
 *								*
 * Both the trace_ variables and the do_trace variables have	*
 * integer values indicating the trace level.  A higher		*
 * value usually indicates a more detailed trace.		*
 *								*
 * trace_locals is nonzero when actions of the local identifier	*
 *	 	table are being traced.				*
 * 								*
 * trace_defs 	is nonzero when actions of the global identifier*
 * 		table are being traced.				*
 *								*
 * trace_lexical is nonzero to trace action of the lexer.	*
 *								*
 * trace_importation is nonzero to trace pushing and popping	*
 *		     of files.					*
 *								*
 * trace_exprs 	is nonzero to cause expressions to be printed as*
 *		parts of traces.  (This can result in long	*
 *		traces.)					*
 *								*
 * trace_ids	is nonzero to trace actions of the id handler	*
 *		in ids.c					*
 *								*
 * trace_implicit_defs is nonzero to trace definitions of	*
 * 		automatic symbols, such as constructors		*
 *		for species.					*
 *								*
 * trace_pm	is nonzero to trace action of the pattern match	*
 *		/ expand translator.				*
 *								*
 * trace_role	is nonzero to trace role and property checking	*
 *								*
 * trace_open_expr is nonzero to trace handling of open ifs, etc.*
 *								*
 * trace_context_expr is nonzero to trace handling of context	*
 *		expressions.					*
 *								*
 * yydebug 	is defined in parser.c.  It is nonzero to	*
 *		trace the action of the parser.			*
 ****************************************************************/

UBYTE trace_locals = 0, 	do_trace_locals = 0;
UBYTE trace_defs = 0, 		do_trace_defs = 0;
UBYTE trace_lexical = 0, 	do_trace_lexical = 0;
UBYTE trace_importation = 0,    do_trace_importation = 0;
UBYTE trace_exprs = 0, 		do_trace_exprs = 0;
UBYTE trace_ids = 0, 		do_trace_ids = 0;
UBYTE trace_implicit_defs = 0, 	do_trace_implicit_defs = 0;
UBYTE trace_pm = 0, 		do_trace_pm = 0;
UBYTE trace_role = 0, 		do_trace_role = 0;
UBYTE trace_open_expr = 0, 	do_trace_open_expr = 0;
UBYTE trace_context_expr = 0, 	do_trace_context_expr = 0;
UBYTE 				do_yydebug = 0;

/****************************************************************
 *			debug_msg_t				*
 ****************************************************************
 * When a debug message needs to be printed (by trace_t), we    *
 * need to read the messages from file dmsgt.txt.  debug_msg_t	*
 * is set to point to an array of strings, namely the lines of	*
 * dmsgt.txt.  Until the first trace_t is done, however, 	*
 * debug_msg_t is left at NULL.					*
 ****************************************************************/

PRIVATE char** debug_msg_t = NULL;

/********************************************************
 *			INLINE_DEBUG_TBL 	      	*
 ********************************************************
 * The following table indicates what should be set     *
 * when a debug is advised.  It should hold triples     *
 * (s,v1,v2) where s is the string that names the debug *
 * parameter, v1 is the variable that is set when       *
 * tracing is actually being done, and v2 is the        *
 * variable that is set when tracing of a particular    *
 * option has been requested.  Note that tracing might  *
 * have been requested, but might not be in force, if   *
 * tracing is only done in a particular part of 	*
 * compilation.						*
 *							*
 * The last entry should contain all 0's.		*
 ********************************************************/

struct debug_tbl_struct FARR inline_debug_tbl[] =
{
  {"EB",	&trace, 		&trace},
  {"LEX",	&trace_lexical,		&do_trace_lexical},
  {"IMP",	&trace_importation,	&do_trace_importation},
  {"DEF",	&trace_defs,		&do_trace_defs},
  {"IMP",	&trace_implicit_defs,	&do_trace_implicit_defs},
  {"LOC",	&trace_locals,		&do_trace_locals},
  {"PMCOMP",	&trace_pat_complete,	&do_trace_pat_complete},
  {"COMP",	&trace_complete,	&do_trace_complete},
  {"PM",	&trace_pm,		&do_trace_pm},
  {"GEN",	&trace_gen,		&do_trace_gen},
  {"UNI",	&trace_unify,		&do_trace_unify},
  {"INF",	&trace_infer,		&do_trace_infer},
  {"ROL",	&trace_role,		&do_trace_role},
  {"EXP",	&trace_exprs,		&do_trace_exprs},
  {"FRE",	&trace_frees,		&do_trace_frees},
  {"CLT",	&trace_classtbl,	&do_trace_classtbl},
  {"OVL",	&trace_overlap,		&do_trace_overlap},
  {"MIS",	&trace_missing,		&do_trace_missing},
  {"IDS",	&trace_ids,		&do_trace_ids},
  {"OPEN",	&trace_open_expr,	&do_trace_open_expr},
  {"CONTEXT",	&trace_context_expr,	&do_trace_context_expr},
  {"A",		&trace_arg,		&trace_arg},
  {"I",		&trace_imports,		&trace_imports},
  {"S",		&trace_standard,	&trace_standard},
  {"P",		&trace_preamble,	&trace_preamble},
  {NULL,	NULL,			0}};


/********************************************************
 *			READ_DEBUG_MESSAGES	      	*
 ********************************************************
 * Read the lines of file dmsgt.txt into debug_msg_t,	*
 * and of dmsgs.txt into debug_mst_s.			*
 ********************************************************/

PRIVATE Boolean have_read_debug_messages = FALSE;

void read_debug_messages(void)
{
  if(!have_read_debug_messages) {
    char* s = 
      (char*) BAREMALLOC(strlen(MESSAGE_DIR) + strlen(DEBUG_MSG_S_FILE) + 1);
    sprintf(s, "%s%s", MESSAGE_DIR, DEBUG_MSG_S_FILE);
    init_err_msgs(&debug_msg_s, s, NUM_DEBUG_MSG_S);
    sprintf(s, "%s%s", MESSAGE_DIR, DEBUG_MSG_T_FILE);
    init_err_msgs(&debug_msg_t, s, NUM_DEBUG_MSG_T);
    have_read_debug_messages = TRUE;
    FREE(s);
  }
}


/****************************************************************
 *			BASIC PRINT FUNCTIONS			*
 ****************************************************************
 * trace_t(f,...) prints line f of file dmsgt.txt, as a format, *
 * with given optional parameters.				*
 ****************************************************************/

void trace_rol(ROLE *r) {fprint_role(TRACE_FILE, r);}

void trace_t(int f, ...)
{
  va_list args;

  va_start(args,f);
  read_debug_messages();
  vfprintf(TRACE_FILE, debug_msg_t[f], args);
  fflush(TRACE_FILE);
  va_end(args);
}


/********************************************************
 *			SET_INLINE_DEBUG	      	*
 ********************************************************
 * Set a debug option found at the front of opt, 	*
 * provided pref is a prefix of opt.  The actual option *
 * occurs after pref in opt.  For example, if pref is   *
 * "-D", then option "-DEB" sets debugging.  Return 	*
 * the  number of characters in the option.		*
 * 							*
 * Set variable number varnum (1 or 2) in 		*
 * inline_debug_tbl.					*
 *							*
 * Note: the option can be followed by a decimal digit. *
 * If it is, then that digit is the value of the	*
 * variable.						*
 ********************************************************/

int set_inline_debug(char *opt, char *pref, int varnum)
{
  int i;
  UBYTE val, len, preflen;
  char dig;

  if(prefix(pref, opt)) {
    read_debug_messages();
    preflen = strlen(pref);
    for(i = 0; inline_debug_tbl[i].str != NULL; i++) {
      if(prefix(inline_debug_tbl[i].str, opt+preflen)) {
	len = strlen(inline_debug_tbl[i].str) + preflen;
	val = 1;
	dig = opt[len];
	if(dig >= '0' && dig <= '9') {
	  val = dig - '0';
	  len++;
	}
	if(varnum == 1) {
	  *(inline_debug_tbl[i].var1) = val;
	}
	else {
	  *(inline_debug_tbl[i].var2) = val;
	}
	return len;
      }
    }

#   ifdef YYDEBUG
      if(prefix("YY", opt+preflen)) {
	yydebug = 1;
	return preflen + 2;
      }
#   endif

  }
  else if(prefix("xDEB", opt)) {
    set_traces(0);
    return 4;
  }
  return 0;
}


/********************************************************
 *			SET_TRACES			*
 ********************************************************
 * If should_trace is 0, then clear all traces.  If 	*
 * should_trace is 1, then set all traces that have     *
 * been turned on (by setting the do_... variables.     *
 ********************************************************/

void set_traces(Boolean should_trace)
{
  struct debug_tbl_struct *p;

  if(should_trace) {
    read_debug_messages();
    fprintf(TRACE_FILE,"Starting trace\n");
    for(p = inline_debug_tbl; p->str != NULL; p++) {
      *(p->var1) = *(p->var2);
    }
    trace = 1;
#   ifdef YYDEBUG
      yydebug = do_yydebug;
#  endif
  }
  else {
    if(trace) fprintf(TRACE_FILE,"Ending trace\n");
    for(p = inline_debug_tbl; strcmp(p->str, "A") != 0; p++) {
      *(p->var1) = 0;
    }

#   ifdef YYDEBUG
      yydebug = 0;
#   endif
  }
}


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
  begin_printing_types(TRACE_FILE, &ctl);
  print_ty1_with_constraints(t1, &ctl);
  tracenl();
  print_ty1_with_constraints(t2, &ctl);
  tracenl();
  end_printing_types(&ctl);
}


/****************************************************************
 *			   PRINT_EXPR_LIST			*
 ****************************************************************
 * Print the names of the identifiers that occur in list l.	*
 ****************************************************************/

void print_expr_list(EXPR_LIST *l)
{
  EXPR_LIST *p;
  EXPR *e;
  EXPR_TAG_TYPE kind;

  fprintf(TRACE_FILE,"(");
  for(p = l; p != NIL; p = p->tail) {
    e    = p->head.expr;
    kind = EKIND(e);
    if(kind == LOCAL_ID_E || kind == IDENTIFIER_E) {
      fprintf(TRACE_FILE, "%s(%p), ", e->STR, e);
    }
    else if(kind == PAT_VAR_E) {
      fprintf(TRACE_FILE, "%s(%p/%p), ", e->STR, e, e->E1);
    }
    else fprintf(TRACE_FILE, "?, ");
  }
  fprintf(TRACE_FILE,")\n");
}


/****************************************************************
 *			   PRINT_LIST				*
 ****************************************************************
 * Print the strings, integers, expressions and types in 	*
 * list l.							*
 ****************************************************************/

void print_list(LIST *l, int n)
{
  LIST *p;

  if(l == NULL) {
    indent(n);
    fprintf(TRACE_FILE,"NIL\n");
  }

  for(p = l; p != NULL; p = p->tail) {
    switch(LKIND(p)) {

      case STR_L:
	indent(n);
	fprintf(TRACE_FILE,"%s;;\n", p->head.str);
	break;

      case EXPR_L:
        print_expr(p->head.expr, n);
	indent(n); fprintf(TRACE_FILE,";;\n");
	break;

      case INT_L:
	indent(n);
	fprintf(TRACE_FILE,"%ld;;\n", top_int(p));
	break;

      case TYPE_L:
      case TYPE1_L:
	indent(n); 
	trace_ty(p->head.type); 
	if(LKIND(p) == TYPE1_L) fprintf(TRACE_FILE, "*");
	tracenl();
	indent(n); fprintf(TRACE_FILE,";;\n");
	break;

      case FILE_L:
	indent(n); fprintf(TRACE_FILE,"A file;;\n");
	break;

      default: 
	indent(n); fprintf(TRACE_FILE,"??;;\n");
    }
  }
}


/****************************************************************
 *			   PRINT_GLOB_BOUND_VARS		*
 ****************************************************************
 * Print the variables that are currently in glob_bound_vars	*
 * and other_vars.						*
 ****************************************************************/

void print_glob_bound_vars(PRINT_TYPE_CONTROL *ctl1)
{
  LIST *p;
  PRINT_TYPE_CONTROL *ctl, c;

  ctl = ctl1;
  if(ctl == NULL) {
    begin_printing_types(TRACE_FILE, &c);
    ctl = &c;
  }
  trace_t(73);
  for(p = glob_bound_vars; p != NIL; p = p->tail) {
    print_ty1_with_constraints(p->head.type, ctl); 
    fprintf(TRACE_FILE," at %p,  ", p->head.type);
  }
  tracenl();

  trace_t(572);
  for(p = other_vars; p != NIL; p = p->tail) {
    print_ty1_with_constraints(p->head.type, ctl); 
    fprintf(TRACE_FILE," at %p,  ", p->head.type);
  }
  tracenl();
  if(ctl1 == NULL) end_printing_types(&c);
}


#endif

