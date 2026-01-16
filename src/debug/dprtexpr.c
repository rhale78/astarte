/**************************************************************
 * File:    debug/prtexpr.c
 * Purpose: Print expressions
 * Author:  Karl Abrahamson
 **************************************************************/

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
#include "../lexer/modes.h"
#include "../alloc/allocate.h"
#include "../exprs/expr.h"
#include "../classes/classes.h"
#include "../generate/prim.h"
#include "../debug/debug.h"

#ifdef DEBUG

/*****************************************************************
 *			VARIABLES				 *
 *****************************************************************/

char * FARR expr_kind_name[] =
{
  "BAD_E",
  "IDENTIFIER_E",
  "GLOBAL_ID_E",
  "LOCAL_ID_E",
  "UNKNOWN_ID_E",
  "OVERLOAD_E",
  "CONST_E",
  "SPECIAL_E",
  "DEFINE_E",
  "LET_E",
  "MATCH_E",
  "OPEN_E",
  "PAIR_E",
  "SAME_E",
  "IF_E",
  "TRY_E",
  "TEST_E",
  "APPLY_E",
  "STREAM_E",
  "FUNCTION_E",
  "SINGLE_E",
  "LAZY_LIST_E",
  "AWAIT_E",
  "LAZY_BOOL_E",
  "TRAP_E",
  "LOOP_E",
  "RECUR_E",
  "FOR_E",
  "PAT_FUN_E",
  "PAT_VAR_E",
  "PAT_RULE_E",
  "PAT_DCL_E",
  "EXPAND_E",
  "MANUAL_E",
  "WHERE_E",
  "EXECUTE_E",
  "SEQUENCE_E"
};


/****************************************************************
 *			 PRINT_EXPR1				*
 ****************************************************************
 * print_expr1(e,n,ctl) prints expression e in long (debugging) *
 * form on TRACE_FILE, indented n spaces, using type bindings	*
 * in ctl.							*
 ****************************************************************/

void print_expr1(EXPR *e, int n, PRINT_TYPE_CONTROL *ctl)
{
  EXPR_TAG_TYPE kind;
  EXPR *e1, *e2, *e3;

  if(e == NULL_E) {
    indent(n);
    fprintf(TRACE_FILE, "NULL_E\n");
  }
  else{
    indent(n); trace_t(50, e);
    indent(n); trace_t(51, 
		       expr_kind_name[EKIND(e)],
		       EKIND(e),
		       tolong(e->ref_cnt),
		       toint(e->PRIMITIVE),
		       toint(e->SCOPE),
		       toint(e->pat),
		       toint(e->done),
		       toint(e->extra),
		       toint(e->mark),
		       toint(e->LINE_NUM));
    e1 = e->E1;
    e2 = e->E2;
    e3 = e->E3;
    kind = EKIND(e);
    switch(kind){
      case TRY_E:
        indent(n); fprintf(TRACE_FILE, "kind = %d\n", toint(e->TRY_KIND));
	/* No break - fall through to next case */

      case IF_E:
      case FOR_E:
	indent(n); fprintf(TRACE_FILE, "e1 =\n"); print_expr1(e1,n+1,ctl);
	indent(n); fprintf(TRACE_FILE, "e2 =\n"); print_expr1(e2,n+1,ctl);
	indent(n); fprintf(TRACE_FILE, "e3 =\n"); print_expr1(e3,n+1,ctl);
	break;

      case OPEN_E:
	indent(n); fprintf(TRACE_FILE, "e1 =\n"); print_expr1(e1,n+1,ctl);
	indent(n); fprintf(TRACE_FILE, "l1 = "); print_str_list_nl(e->EL1);
	indent(n); fprintf(TRACE_FILE, "l2 = "); print_str_list_nl(e->EL2);
	break;

      case AWAIT_E:
	indent(n); fprintf(TRACE_FILE, "extra = %d\n", toint(e->extra));
	goto rest_await;  /* Just below, under LOOP_E */

      case LOOP_E:
	indent(n); fprintf(TRACE_FILE, "EL2 = %p\n", e->EL2);
      rest_await:
      case DEFINE_E:
      case FUNCTION_E:
	indent(n); fprintf(TRACE_FILE, "OFFSET = %d\n", toint(e->OFFSET));
        /* No break - fall through to next case */

      case APPLY_E:
      case SEQUENCE_E:
      case STREAM_E:
      case LET_E:
      case MATCH_E:
      case LAZY_BOOL_E:
      case PAT_DCL_E:
      case EXPAND_E:
      case TRAP_E:
      case WHERE_E:
	indent(n); fprintf(TRACE_FILE, "SCOPE = %d\n", toint(e->SCOPE));
	indent(n); fprintf(TRACE_FILE, "e1 =\n"); print_expr1(e1,n+1,ctl);
	indent(n); fprintf(TRACE_FILE, "e2 =\n"); print_expr1(e2,n+1,ctl);
	break;

      case LAZY_LIST_E:
      case SINGLE_E:
	indent(n); 
	trace_t(52, toint(e->SCOPE), toint(e->OFFSET));
	if(e->STR != NULL) {
	  indent(n);
	  fprintf(TRACE_FILE, "STR = %s\n", nonnull(e->STR));
	}
        /* No break - fall through to next case */

      case RECUR_E:
	indent(n); fprintf(TRACE_FILE, "e1 =\n"); print_expr1(e1, n+1, ctl);
	indent(n); fprintf(TRACE_FILE, "e2 = %p\n", e2);
	break;

      case MANUAL_E:
	indent(n); fprintf(TRACE_FILE, "e1 =\n"); print_expr1(e1, n+1, ctl);
	indent(n); fprintf(TRACE_FILE, "STR = %s\n", nonnull(e->STR));
	indent(n); fprintf(TRACE_FILE, "EL2 = ");
	print_str_list_nl(e->EL2);
	break;

      case TEST_E:
	indent(n); fprintf(TRACE_FILE, "e1 =\n"); print_expr1(e1, n+1, ctl);
	indent(n); fprintf(TRACE_FILE, "e2 =\n"); print_expr1(e2, n+1, ctl);
	break;

      case CONST_E:
        indent(n); 
	trace_t(53, nonnull(e->STR), toint(e->SCOPE), toint(e->OFFSET));
	break;

      case SPECIAL_E:
        {int prim = e->PRIMITIVE;
         indent(n); 
	 trace_t(54,  toint(prim), toint(e->SCOPE), toint(e->extra),
		nonnull(e->STR));
         if(prim == PRIM_TARGET) {
	   indent(n); fprintf(TRACE_FILE, "e1 = %p\n", e1);
	 }
	 else if(prim == PRIM_SPECIES) {
	   indent(n); fprintf(TRACE_FILE, "type-val = ");
	   if(e->E1 == NULL) fprintf(TRACE_FILE, "NONE");
	   else trace_ty(e->E1->ty);
	   fnl(TRACE_FILE);
	 }
	 else if(prim == PRIM_SELECT) {
	   indent(n); fprintf(TRACE_FILE, "sel_list = ");
	   print_str_list_nl(e->EL3);
	 }
	 break;
	}

      case GLOBAL_ID_E:
	indent(n); 
	trace_t(55,  nonnull(e->STR), e->STR, toint(e->SCOPE),
		toint(e->OFFSET), toint(e->PRIMITIVE), toint(e->irregular));
	indent(n); fprintf(TRACE_FILE, "gic = (%p)\n", e->GIC); 
#	ifdef NEVER
	  print_gic(e->GIC, n+1);
#       endif
	break;

      case SAME_E:
	indent(n); 
	fprintf(TRACE_FILE, "final = %d, team = %d, mode = %d, close = %d\n", 
		toint(e->extra), toint(e->TEAM_MODE), toint(e->SAME_MODE),
		toint(e->SAME_CLOSE));
	if(e->SAME_MODE == 5) {
          indent(n);
	  fprintf(TRACE_FILE, "DCL_MODE\n");
	  print_mode(e->SAME_E_DCL_MODE, n+1);
	  indent(n);
	  fprintf(TRACE_FILE, "Versions = ");
	  print_type_list(e->EL0);
	  tracenl();
	} 
	indent(n); 
	fprintf(TRACE_FILE, "e1 = \n"); print_expr1(e1, n+1,ctl);
        break;

      case EXECUTE_E:
      case PAT_VAR_E:
	indent(n); 
	trace_t(56, nonnull(e->STR), e->STR, toint(e->SCOPE), 
		toint(e->OFFSET));
	indent(n); 
	fprintf(TRACE_FILE, "e1 = \n"); print_expr1(e1, n+1,ctl);
	break;

      case IDENTIFIER_E:
        indent(n); 
	trace_t(57, nonnull(e->STR), e->STR, toint(e->PRIMITIVE),
		toint(e->SCOPE));
        break;

      case LOCAL_ID_E:
	indent(n); 
	trace_t(58, nonnull(e->STR), e->STR, toint(e->SCOPE),
	        toint(e->OFFSET), 
	        toint(e->bound), toint(e->mark), toint(e->ETEMP));
	indent(n); fprintf(TRACE_FILE, "EL0 = "); print_str_list_nl(e->EL0);
	break;

      case UNKNOWN_ID_E:
      case OVERLOAD_E:
	indent(n); 
	trace_t(59, nonnull(e->STR), e->STR, e->GIC, toint(e->PRIMITIVE));
	break;

      case PAIR_E:
	indent(n); fprintf(TRACE_FILE, "e1 =\n"); print_expr1(e1, n+1,ctl);
	indent(n); fprintf(TRACE_FILE, "e2 =\n"); print_expr1(e2, n+1,ctl);
	break;

      case PAT_RULE_E:
	indent(n); fprintf(TRACE_FILE, "mode = %d, tag = %s\n", 
			   toint(e->PAT_RULE_MODE), nonnull(e->ETAGNAME));
	indent(n); fprintf(TRACE_FILE, "formals(E2) = \n"); 
	print_expr1(e2, n+1, ctl);
	indent(n); 
	trace_t(60); 
	print_expr1(e3, n+1, ctl);
	indent(n); 
	trace_t(61); 
	print_expr1(e1, n+1, ctl);
	break;

      case PAT_FUN_E:
	indent(n); trace_t(62, nonnull(e->STR), e->GIC);
	indent(n); fprintf(TRACE_FILE, "E3 = %p\n", e->E3);
	indent(n); fprintf(TRACE_FILE, "EL3 = ");
	print_str_list_nl(e->EL3);
#       ifdef NEVER
	  indent(n); 
	  fprintf(TRACE_FILE, "rules =\n"); print_expr1(e3, n+1, ctl);
#       endif
	break;

      default: 
	indent(n); fprintf(TRACE_FILE, "???");
    }

    if(kind != PAT_RULE_E && kind != PAT_DCL_E && kind != EXPAND_E) {
      indent(n); fprintf(TRACE_FILE, "type = "); 
      print_ty1_with_constraints(e->ty, ctl);
      tracenl();
      indent(n); fprintf(TRACE_FILE, "role = "); 
      fprint_role(TRACE_FILE, e->role); 
      tracenl();
    }
  }
}


/****************************************************************
 *			 PRINT_EXPR				*
 ****************************************************************
 * print_expr(e,n) prints expression e, indented n spaces,	*
 * in long form on the trace file.				*
 ****************************************************************/

void print_expr(EXPR *e, int n)
{
  PRINT_TYPE_CONTROL ctl;
  begin_printing_types(TRACE_FILE, &ctl);
  print_expr1(e, n, &ctl);
  end_printing_types(&ctl);
}

#endif


