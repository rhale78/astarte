/****************************************************************
 * File:    debug/debug.c
 * Purpose: General functions for debugging the compiler and interprter
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
 * This file contains functions and data that are shared by the 	*
 * compiler and interpreter, and that are only used in debug mode.	*
 ************************************************************************/

#include <stdarg.h>
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
#include "../unify/unify.h"
#include "../debug/debug.h"

int qwrap_tag(ENTITY);


/********************************************************
 *			VARIABLES			*
 ********************************************************/

/*--------------------------------------------------------*
 * The following switches control tracing in shared code. *
 *--------------------------------------------------------*/

/********************************************************
 *			trace				*
 ********************************************************
 * trace is nonzero when any kind of tracing has been	*
 * turned on.						*
 ********************************************************/

UBYTE trace = 0;

/********************************************************
 *			trace_frees			*
 ********************************************************
 * trace_frees is nonzero to cause storage freeing      *
 * functions to report their actions.  This can lead	*
 * to a lot of output.					*
 *							*
 * See alloc/(*.c).					*
 ********************************************************/

UBYTE trace_frees = 0;

/********************************************************
 *			trace_infer			*
 ********************************************************
 * trace_infer is nonzero to trace the action of type   *
 * inference.  Higher values generally give more 	*
 * detailed traces.					*
 *							*
 * See infer/(*.c).					*
 ********************************************************/

UBYTE trace_infer = 0;

/********************************************************
 *			trace_unify			*
 ********************************************************
 * trace_unify is nonzero to trace unifications.  A 	*
 * larger value gets a more detailed trace.		*
 *							*
 * See unify/unify.c.					*
 ********************************************************/

UBYTE trace_unify = 0;

/********************************************************
 *			trace_gen			*
 ********************************************************
 * trace_gen is set nonzero to trace code generation.   *
 *							*
 * See generate/(*.c).					*
 ********************************************************/

UBYTE trace_gen = 0;

/********************************************************
 *			trace_classtbl			*
 ********************************************************
 * trace_classtbl is set nonzero to trace changes to    *
 * the table that stores species, family, genus and	*
 * community definitions.				*
 *							*
 * See clstbl/classtbl.c.				*
 ********************************************************/

UBYTE trace_classtbl = 0;

/********************************************************
 *			trace_overlap			*
 ********************************************************
 * Set trace_overlap nonzero to trace test for how	*
 * polymorphic species overlap.				*
 *							*
 * See unify/overlap.c.					*
 ********************************************************/

UBYTE trace_overlap = 0;

/********************************************************
 *			trace_missing			*
 ********************************************************
 * Set trace_missing nonzero to trace action of missing *
 * definition testing.					*
 *							*
 * See tables/missing.c, classes/t_tyutil.c.		*
 ********************************************************/

UBYTE trace_missing = 0;

/********************************************************
 *			trace_missing_type		*
 ********************************************************
 * Set trace_missing_type nonzero to trace action of    *
 * function missing_type.				*
 *							*
 * See unify/complete.c.				*
 ********************************************************/

UBYTE trace_missing_type = 0;

/********************************************************
 *			trace_complete			*
 ********************************************************
 * Set trace_complete nonzero to trace tests of		*
 * completeness at the end of the compilation,		*
 *							*
 * See unify/complete.c, clstbl/t_compl.c. 		*
 ********************************************************/

UBYTE trace_complete = 0;

/********************************************************
 *			trace_pat_complete		*
 ********************************************************
 * Set trace_pat_complete to trace testing whether	*
 * patterns are exhaustive in choose-matching and	*
 * definition by cases.					*
 *							*
 * See unify/complete.c, ids/ids.c, patmatch/patutils.c,*
 * patmatch/pmcompl.c, clstbl/classtbl.c.		*
 ********************************************************/

UBYTE trace_pat_complete = 0;

/********************************************************
 *			force_ok_kind			*
 ********************************************************
 * Set nonzero to suppress tests of reference counts	*
 * when in GCTEST mode.					*
 ********************************************************/

UBYTE force_ok_kind = 0;

/********************************************************
 *			TRACE_FILE			*
 ********************************************************
 * This is the file where debug output is sent.		*
 ********************************************************/

FILE* TRACE_FILE = NULL;

/********************************************************
 *			debug_msg_s			*
 ********************************************************
 * When trace_s is called to print a shared debug	*
 * message, it allocates debug_msg_s and fills it with	*
 * messages to print from file messages/dmsgs.txt.	*
 ********************************************************/

char** debug_msg_s = NULL;      


/***************************************************************
 * The following counters count how many of given things have  *
 * been allocated.  They can be used to look for memory leaks. *
 ***************************************************************/

LONG allocated_lists = 0;
LONG allocated_types = 0;
LONG allocated_exprs = 0;
LONG allocated_hash1s = 0;
LONG allocated_hash2s = 0;
LONG allocated_cucs = 0;
LONG allocated_report_records = 0;
LONG allocated_roles = 0;
LONG allocated_descr_chains = 0;
LONG allocated_stacks = 0;
LONG allocated_states = 0;
LONG allocated_controls = 0;
LONG allocated_continuations = 0;
LONG allocated_activations = 0;
LONG allocated_trap_vecs = 0;
LONG allocated_environments = 0;
LONG allocated_global_envs = 0;


/****************************************************************
 *			TRACENL					*
 ****************************************************************
 * Print a newline on TRACE_FILE.				*
 ****************************************************************/

void tracenl(void) 
{
  fprintf(TRACE_FILE, "\n");
}


/****************************************************************
 *			TRACE_TY				*
 *			TRACE_TY_WITHOUT_CONSTRAINTS		*
 ****************************************************************
 * Print type t on TRACE_FILE.					*
 ****************************************************************/

void trace_ty(TYPE *t) 
{
  fprint_ty(TRACE_FILE, t);
}

/*-------------------------------------------------------------*/

void trace_ty_without_constraints(TYPE *t)
{
  fprint_ty_without_constraints(TRACE_FILE, t);
}


/****************************************************************
 *		   PRINT_TYPE_LIST				*
 *		   PRINT_TYPE_LIST_SEPARATE_LINES		*
 *		   PRINT_TYPE_LIST_WITHOUT_CONSTRAINTS		*
 ****************************************************************
 * Print the types in list L on the trace file.	Treat each      *
 * EXPECT_TABLE and NAME_TYPE entry by showing type and		*
 * package.  Treat each LIST_L entry as a pair (V,L) where V	*
 * is a variable and L is a list of lower bounds for it.	*
 *								*
 * print_type_list_without_constraints omits constraints.	*
 *								*
 * print_type_list_separate_lines prints the types one per line.*
 * Each is preceded by pref.					*
 ****************************************************************/

PRIVATE void 
print_type_list_help(TYPE_LIST *L, void (*prt_ty)(TYPE*), char *pref)
{
  TYPE_LIST *p;

  if(L == NIL) {
    if(pref != NULL) fprintf(TRACE_FILE, "%sNIL\n", pref);
    else fprintf(TRACE_FILE, "NIL");
    return;
  }

  if(pref != NULL) fprintf(TRACE_FILE, "%s", pref);
  else fprintf(TRACE_FILE, "[");

  for(p = L; p != NIL; p = p->tail) {
    int tag = LKIND(p);
    switch(tag) {
      case TYPE_L:
        prt_ty(p->head.type);
        break;
    
      case TYPE1_L:
        fprintf(TRACE_FILE, "1:");
        prt_ty(p->head.type);
	break;
    
      case TYPE2_L:
        fprintf(TRACE_FILE, "2:");
        prt_ty(p->head.type);
	break;
    
      case EXPECT_TABLE_L:
        prt_ty(p->head.expect_table->type);
        fprintf(TRACE_FILE, "(package %s)", 
	        nonnull(p->head.expect_table->package_name));
        break;

      case NAME_TYPE_L:
        prt_ty(p->head.name_type->type);
        fprintf(TRACE_FILE, "(package %s)", 
	        nonnull(p->head.name_type->name));
	break;
    
      case LIST_L:
	prt_ty(p->head.list->head.type);
	fprintf(TRACE_FILE, " >= ");
	print_type_list_without_constraints(p->head.list->tail);
	break;

      default: {}
    }


    if(pref != NULL) tracenl();
    else if(p->tail != NIL) fprintf(TRACE_FILE, ", ");
  }
  if(pref == NULL) fprintf(TRACE_FILE, "]");
}

/*--------------------------------------------------------------*/

void print_type_list(TYPE_LIST *L)
{
  print_type_list_help(L, trace_ty, NULL);
}

/*--------------------------------------------------------------*/

void print_type_list_without_constraints(TYPE_LIST *L)
{
  print_type_list_help(L, trace_ty_without_constraints, NULL);
}

/*--------------------------------------------------------------*/

void print_type_list_separate_lines(TYPE_LIST *L, char *pref)
{
  print_type_list_help(L, trace_ty, pref);
}

/*--------------------------------------------------------------*/

void 
print_type_list_separate_lines_without_constraints(TYPE_LIST *L, char *pref)
{
  print_type_list_help(L, trace_ty_without_constraints, pref);
}


/****************************************************************
 *			TRACE_S					*
 ****************************************************************
 * Print line f of file messages/dmsgs.txt on TRACE_FILE.  That *
 * line is a format.  Additional parameters of trace_s are      *
 * passed to fprintf with the selected format.			*
 ****************************************************************/

void trace_s(int f, ...)
{
  va_list args;

  va_start(args,f);
  read_debug_messages();
  vfprintf(TRACE_FILE, debug_msg_s[f], args);
  va_end(args);
}


/****************************************************************
 *			PAUSE_FOR_DEBUG				*
 ****************************************************************
 * Do nothing.  Provides stop place for debugging.		*
 ****************************************************************/

void pause_for_debug(void) {}


/****************************************************************
 *			INDENT					*
 ****************************************************************
 * indent(n) prints n spaces on TRACE_FILE.			*
 ****************************************************************/

void indent(int n)
{
  int i;
  for(i=1; i<=n; i++) putc(' ', TRACE_FILE);
}


/*****************************************************************
 *			   PRINT_STR_LIST			 *
 *			   PRINT_STR_LIST_NL			 *
 *****************************************************************
 * Print a list of strings or integers or exprs or entitiess.  	 *
 * Print_str_list_nl prints a newline at the end of the list,    *
 * but print_str_list does not.  				 *
 *								 *
 * If l is (LIST *) 1, then print_str_list prints *1.		 *
 *								 *
 * print_str_l is a helper, containing the meat of the function. *
 *****************************************************************/

#ifndef VAL
# define VAL(e) e
#endif

PRIVATE void print_str_l(STR_LIST *l)
{
  STR_LIST *p;

  for(p = l; p != NIL; p = p->tail) {
    switch(LKIND(p)) {
      case STR_L:
        fprintf(TRACE_FILE, "%s(%p)", p->head.str, p->head.str);
	break;

      case STR1_L:
      case STR2_L:
      case STR3_L:
	fprintf(TRACE_FILE, "%s:%d", p->head.str, LKIND(p) - STR_L);
	break;

      case INT_L:
	fprintf(TRACE_FILE, "%ld", p->head.i);
	break;

      case SHORTS_L:
	fprintf(TRACE_FILE, "(%d,%d)", toint(p->head.two_shorts.line), 
	       toint(p->head.two_shorts.col));
	break;

      case EXPR_L: 
        {EXPR_TAG_TYPE kind;
	 EXPR* e = p->head.expr;
         kind = EKIND(e);
         if(kind == LOCAL_ID_E || kind == IDENTIFIER_E || kind == PAT_VAR_E) 
	   fprintf(TRACE_FILE, "(%s@%p@%p)", e->STR, e->STR, e);
         else fprintf(TRACE_FILE, "%p", e);
	 break;
       }

      case ENTS_L:
	fprintf(TRACE_FILE, "%d", qwrap_tag(*(p->head.ents)));

      case TYPE_L:
      case TYPE1_L:

	/*--------------------------------------------------------------*
         * print_str_list is used for list of type/family variables	*
         * or pairs (V,A), where V is the variable and A is what	*
	 * it is bound to.  In the former case, just print the pointers *
         * to the variables.  In the later case, print (V,A) as p = A,  *
	 * where p is the address of V, and A is the short form of A. 	*
         *--------------------------------------------------------------*/

	{TYPE *t, *v;
	 TYPE_TAG_TYPE t_kind, v_kind;

	 t      =  p->head.type;
	 t_kind = TKIND(t);
	 if(t_kind == PAIR_T) {
	   v      = t->TY1;
	   v_kind = TKIND(v);
	 }
	 else {
	   v      = t;
	   v_kind = t_kind;
	 }

	 if(IS_VAR_T(v_kind) && v->TY1 != NULL) {
	   fprintf(TRACE_FILE, "%p(->%p)", v, find_u(v));
	 }
	 else {
	   fprintf(TRACE_FILE, "%p", v);
	 }
	 if(LKIND(p) == TYPE1_L) fprintf(TRACE_FILE, "*");

	 if(t_kind == PAIR_T) {
	   fprintf(TRACE_FILE, " = ");
	   trace_ty(t->TY2);
	 }

	 break;
        }

      default:
	fprintf(TRACE_FILE, "?");
    }
    if(p->tail != NIL) fprintf(TRACE_FILE, ", ");
  }
}

/*-------------------------------------------------------------------*/
  

void print_str_list(STR_LIST *l)
{
  if(l == (LIST *) 1) fprintf(TRACE_FILE, "*1");
  else {
    fprintf(TRACE_FILE, "<");
    print_str_l(l);
    fprintf(TRACE_FILE, ">");
  }
}


/*-------------------------------------------------------------------*/

void print_str_list_nl(STR_LIST *l)
{
  print_str_list(l);
  tracenl();
}


#endif /* defined DEBUG */
