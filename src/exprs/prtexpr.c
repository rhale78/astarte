/**************************************************************
 * File:    exprs/prtexpr.c
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

/************************************************************************
 * This file is concerned with printing an expression in reasonable	*
 * (programmer readable) form.  The current functions print abbreviated *
 * versions, suitable for putting in error messages.  An ellipsis is 	*
 * used to keep the text short for large expressions.			*
 *									*
 * If you want to print an expression for debugging, and want to see 	*
 * all the gory detail, use function print_expr from debug/dprtexpr.c.	*
 ************************************************************************/

#include <ctype.h>
#include "../misc/misc.h"
#include "../lexer/modes.h"
#include "../exprs/expr.h"
#include "../exprs/prtexpr.h"
#include "../utils/hash.h"
#include "../utils/strops.h"
#include "../alloc/allocate.h"
#include "../parser/parser.h"
#include "../classes/classes.h"
#include "../generate/prim.h"
#include "../machstrc/machstrc.h"
#include "../machdata/except.h"

/****************************************************************
 *			PRIVATE_VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			spe_buffer				*
 *			spe_lcount				*
 *			spe_rcount				*
 *			spe_pos					*
 *			spe_elipsis				*
 *			need_sep				*
 ****************************************************************
 * When an expression is printed in short form, a prefix of it  *
 * is copied into spe_prefix_buffer.  The remaining characters  *
 * are stored in spe_suffix_buffer in a circular manner.  	*
 *								*
 * spe_prefix_pos is the position where the next character 	*
 *                should be placed in spe_prefix_buffer.	*
 *								*
 * spe_suffix_pos is the position where the next character	*
 *                should be placed in spe_suffix_buffer.        *
 *								*
 * spe_lcount is the number of characters remaining to be	*
 * 	      put into spe_prefix_buffer before switching to	*
 *	      spe_suffix_buffer.				*
 *								*
 * spe_rcount is the number of characters that are available    *
 *            in spe_suffix_buffer.  Once it reaches 		*
 *	      SPE_SUFFIX_BUFFER_SIZE, it will not grow larger.	*
 *								*
 * spe_elipsis is true if any characters have been overwritten  *
 * 	       in spe_suffix_buffer, due to the circular nature *
 *	       of the buffer.					*
 *								*
 * spe_prefix_full is true if spe_prefix_buffer became full, 	*
 *		   and the entire expression did not fit into   *
 *		   that buffer.					*
 *								*
 * need_sep    is true if the most recently printed string	*
 *	       needs to be followed by a separator, if it	*
 *	       is followed by a string that needs a separator   *
 *	       on its left.					*
 *								*
 * stop_at_prefix_end is true if printing should be aborted 	*
 *		      when a the prefix buffer is overflowed.	*
 ****************************************************************/

PRIVATE char	spe_prefix_buffer[SPE_PREFIX_BUFFER_SIZE];
PRIVATE char	spe_suffix_buffer[SPE_SUFFIX_BUFFER_SIZE];
PRIVATE int	spe_prefix_pos, spe_suffix_pos, spe_lcount, spe_rcount;
PRIVATE Boolean spe_elipsis, spe_prefix_full, need_sep, stop_at_prefix_end;

PRIVATE void short_print_expr1(EXPR *e);
PRIVATE void short_print_exprs1(EXPR *e);
PRIVATE void print_e(char *s);
PRIVATE void print_char_e(char c);
PRIVATE void lprint_e(char *s);
PRIVATE void rprint_e(char *s);
PRIVATE void lrprint_e(char *s);
PRIVATE void print_string_e(char *s);
PRIVATE void print_suffix_buffer(FILE *f);
PRIVATE void print_prefix_buffer(FILE *f);

/****************************************************************
 *			SHORT_PRINT_EXPR			*
 ****************************************************************
 * Print e in abbreviated form on file f, with its type.	*
 * The abbreviated form looks like prefix...suffix, where	*
 * prefix has length at most SHORT_PRINT_PREFIX_LENGTH_DEFAULT, *
 * and suffix has length at most SPE_SUFFIX_BUFFER_SIZE.  If    *
 * nothing is left out, no elipsis is printed.			*
 ****************************************************************/

void short_print_expr(FILE *f, EXPR *e)
{
  PRINT_TYPE_CONTROL ctl;

  shrt_pr_expr(f, e);
  fprintf(f, ": ");
  begin_printing_types(f, &ctl);
  print_ty1_with_constraints_indented(e->ty, &ctl, 8);
  end_printing_types(&ctl);
  fnl(f);
}


/****************************************************************
 *			SHORT_PRINT_TWO_EXPRS			*
 ****************************************************************
 * Print expressions e1 and e2 on file f.  The form is		*
 *								*
 *   s1 e1:t1							*
 *   s2 e2:t2							*
 *								*
 * where t1 is the type of e1 and t2 is the type of t2.	Each	*
 * expression e1 and e2 is printed in abbreviated form as in	*
 * short_print_expr.						*
 ****************************************************************/

void short_print_two_exprs(FILE *f, char *s1, EXPR *e1, char *s2, EXPR *e2)
{
  PRINT_TYPE_CONTROL ctl;

  begin_printing_types(f, &ctl);
  fprintf(f, "%s", s1);
  shrt_pr_expr(f, e1);
  fprintf(f, ": ");
  print_ty1_with_constraints_indented(e1->ty, &ctl, 8);
  fnl(f);
  fprintf(f, "%s", s2);
  shrt_pr_expr(f, e2);
  fprintf(f, ": ");
  print_ty1_with_constraints_indented(e2->ty, &ctl, 8);
  fnl(f);
  end_printing_types(&ctl);
}


/****************************************************************
 *			TRY_SHORT_PRINT_EXPR			*
 ****************************************************************
 * Print e in abbreviated form on file f, provided that the	*
 * number of characters printed is no more than max_chars.	*
 *								*
 * If the expression is printed, then return TRUE.		*
 *								*
 * If the expression is too long, then nothing is printed, and  *
 * FALSE is returned.						*
 ****************************************************************/

Boolean try_short_print_expr(FILE *f, EXPR *e, int max_chars)
{
  stop_at_prefix_end = TRUE;
  need_sep   = spe_elipsis = spe_prefix_full = FALSE;
  spe_rcount = spe_prefix_pos = spe_suffix_pos = 0;
  spe_lcount = min(SPE_PREFIX_BUFFER_SIZE, max_chars);
  
  short_print_expr1(e);
  if(spe_prefix_full) return FALSE;
  else {
    print_prefix_buffer(f);
    return TRUE;
  }
}


/****************************************************************
 *			SHRT_PR_EXPR				*
 ****************************************************************
 * Print expression e on file f in abbreviated form.  Do not	*
 * add its type.						*
 *								*
 * The abbreviated form is as described for short_print_expr.	*
 ****************************************************************/

void shrt_pr_expr(FILE *f, EXPR *e)
{
  stop_at_prefix_end = FALSE;
  need_sep   = spe_elipsis = spe_prefix_full = FALSE;
  spe_rcount = spe_prefix_pos = spe_suffix_pos = 0;
  spe_lcount = SHORT_PRINT_EXPR_PREFIX_LENGTH_DEFAULT;

  short_print_expr1(e);
  print_prefix_buffer(f);
  if(spe_elipsis) fprintf(f, "...");
  print_suffix_buffer(f);
}


/****************************************************************
 *			PRINT_PREFIX_BUFFER			*
 ****************************************************************
 * Print the contents of spe_prefix_buffer, as it should occur  *
 * as a prefix of an expression string, on file f.		*
 ****************************************************************/

PRIVATE void
print_prefix_buffer(FILE *f)
{
  int i;

  for(i = 0; i < spe_prefix_pos; i++) {
    putc(spe_prefix_buffer[i], f);
  }
}


/****************************************************************
 *			PRINT_SUFFIX_BUFFER			*
 ****************************************************************
 * Print the contents of spe_suffix_buffer, as it should occur  *
 * as a suffix of an expression string, on file f.		*
 ****************************************************************/

PRIVATE void
print_suffix_buffer(FILE *f)
{
  int i,j;

  j = (spe_rcount < SPE_SUFFIX_BUFFER_SIZE) ? 0 : spe_suffix_pos;
  for(i = 0; i < spe_rcount; i++) {
    putc(spe_suffix_buffer[j], f);
    j++;
    if(j >= SPE_SUFFIX_BUFFER_SIZE) j = 0;
  }
}


/****************************************************************
 *			SHORT_PRINT_EXPRS1			*
 ****************************************************************
 * Expression e is (e1,e2,...,en,()).  Print expressions e1,...,*
 * e2, separated by commas, in the same way as 			*
 * short_print_expr1 would (in spe_prefix_buffer and		*
 * spe_suffix_buffer.)						*
 ****************************************************************/

PRIVATE void
short_print_exprs1(EXPR *e)
{
  EXPR* a = skip_sames(e);
  while(EKIND(a) == PAIR_E) {
    short_print_expr1(a->E1);
    a = a->E2;
    if(EKIND(a) == PAIR_E) print_e(",");
  }
}


/****************************************************************
 *			SHORT_PRINT_EXPR1			*
 ****************************************************************
 * Print expression e into spe_prefix_buffer and		*
 * spe_suffix_buffer.						*
 *								*
 * If stop_at_prefix_end is true, then stop prematurely if	*
 * spe_prefix_buffer cannot hold the entire expression.  In	*
 * that case, spe_prefix_full will be set true.			*
 ****************************************************************/

PRIVATE void short_print_expr1(EXPR *e)
{
  EXPR_TAG_TYPE kind;
  char *name;
  EXPR *e1, *e2, *e3;
  
 tail_recur:
  if(e == NULL_E) {
    lrprint_e("??(NULL)");
    return;
  }
  e1 = e->E1;
  e2 = e->E2;
  e3 = e->E3;
  kind = EKIND(e);
  switch(kind) {

    case OPEN_E:
        lrprint_e("open");
        if(!stop_at_prefix_end || !spe_prefix_full) {
	   short_print_expr1(e1);
        }
	break;

    case IF_E:
        name = "If";
	lrprint_e(name);
	goto iftry;  /* Just below, under TRY_E */

    case TRY_E:
	name = "Try";
	lrprint_e(name);
	{int t_kind = e->TRY_KIND;
	 if(t_kind == TRYEACH_F || t_kind == TRYEACHTERM_F) {
	   lrprint_e("catchingEachThread");
	 }
	 if(t_kind == TRYTERM_F || t_kind == TRYEACHTERM_F) {
	   lrprint_e("catchingallExceptions");
	 }
        }

      iftry:
	if(stop_at_prefix_end && spe_prefix_full) break;
	short_print_expr1(e1);
	if(!is_hermit_expr(e2)) {
	  lrprint_e("then");
	  if(stop_at_prefix_end && spe_prefix_full) break;
	  short_print_expr1(e2);
	}
	if(!is_hermit_expr(e3)) {
	  lrprint_e("else");
	  if(stop_at_prefix_end && spe_prefix_full) break;
	  short_print_expr1(e3);
	}
	lprint_e("%");
        rprint_e(name);
	break;

    case TRAP_E:
	if(e->SCOPE != 0) lrprint_e("open");
	name = (e->TRAP_FORM == TRAP_ATT) ? "Trap" : "Untrap";
	lrprint_e(name);
	if(stop_at_prefix_end && spe_prefix_full) break;
	if(e1 == NULL_E) lrprint_e("all");
	else short_print_expr1(e1);
	lprint_e("=>");
	if(stop_at_prefix_end && spe_prefix_full) break;
	short_print_expr1(e2);
	lprint_e("%");
	rprint_e(name);
	break;

    case FOR_E:
        lrprint_e("For");
	if(e->STR4 != NULL) {
	  lrprint_e("from");
	  short_print_expr1(e2);
	  lrprint_e("selection:");
	  lrprint_e(quick_display_name(e->STR4));
	  lrprint_e("match:");
	  short_print_expr1(e1);
	}
	else {
	  if(stop_at_prefix_end && spe_prefix_full) break;
	  short_print_expr1(e1);
	  lrprint_e("from");
	  if(stop_at_prefix_end && spe_prefix_full) break;
	  short_print_expr1(e2);
	}
	if(e->FOR_FORM != ORDERED_ATT) lrprint_e("mixed");
        lrprint_e("do");
	if(stop_at_prefix_end && spe_prefix_full) break;
	short_print_expr1(e3);
	lrprint_e("%For");
	break;

    case LOOP_E:
	if(e->OPEN_LOOP != 0) lrprint_e("open");
        lrprint_e("Loop");
	if(stop_at_prefix_end && spe_prefix_full) break;
        if(e1 != NULL) {
	  short_print_expr1(e1->E1);
	  lrprint_e("=");
	  short_print_expr1(e1->E2);
	}
	if(e->STR4 != NULL) {
	  lprint_e("target:");
	  lrprint_e(quick_display_name(e->STR4));
	}
        lrprint_e("body:");
	if(stop_at_prefix_end && spe_prefix_full) break;
	short_print_expr1(e2);
	lrprint_e("%Loop");
	break;

    case AWAIT_E:
	if(e1 == NULL) {
	  print_e("(:");
	  if(stop_at_prefix_end && spe_prefix_full) break;
	  short_print_expr1(e2);
	  print_e(":)");
	}
	else {
	  lrprint_e("Await");
	  if(stop_at_prefix_end && spe_prefix_full) break;
	  short_print_exprs1(e1);
	  lrprint_e("then");
	  if(stop_at_prefix_end && spe_prefix_full) break;
	  short_print_expr1(e2);
	  lrprint_e("%Await");
	}
	break;

    case LAZY_LIST_E:
	print_e("[:");
	if(stop_at_prefix_end && spe_prefix_full) break;
	short_print_expr1(e1);
	print_e(":]");
	break;

    case PAIR_E:
	if(e->SCOPE) lrprint_e("open");
	print_e("(");
	if(stop_at_prefix_end && spe_prefix_full) break;
	short_print_expr1(e1);
	print_e(",");
	if(stop_at_prefix_end && spe_prefix_full) break;
	short_print_expr1(e2);
	print_e(")");
	break;

    case FUNCTION_E:
	print_e("(");
	if(is_stack_expr(e1)) {
	  print_e("?");
	  rprint_e(quick_display_name(skip_sames(e1)->STR));
	}
	else {
	  if(stop_at_prefix_end && spe_prefix_full) break;
	  short_print_expr1(e1);
	}
	lrprint_e("=>");
	if(stop_at_prefix_end && spe_prefix_full) break;
	short_print_expr1(e2);
	print_e(")");
	break;

    case LET_E:
	name = (e->LET_FORM == 0) ? "Let" : "Relet";
	goto letetc;  /* Just below, under MATCH_E */

    case DEFINE_E:
	name = "Define";
	goto letetc;  /* Just below, under MATCH_E */

    case MATCH_E:
	name = "Match";
      letetc:
	if(kind != DEFINE_E && e->SCOPE != 0) lrprint_e("open");
	lrprint_e(name);
	if(stop_at_prefix_end && spe_prefix_full) break;
	short_print_expr1(e1);
	lrprint_e("=");
	if(stop_at_prefix_end && spe_prefix_full) break;
	short_print_expr1(e2);
	lprint_e("%");
	rprint_e(name);
	break;

    case EXECUTE_E:
    case SINGLE_E:
	{int attr, closed_attr;
	 attr   = e->SINGLE_MODE;
	 closed_attr = e->SCOPE;
         if(!closed_attr) lrprint_e("open");
	 name = (kind == EXECUTE_E)      ? "Execute"    :
		(attr == FIRST_ATT)      ? "Unique"     :
	        (attr == ATOMIC_ATT)     ? "Atomic"  : 
		(attr == CUTHERE_ATT)    ? "CutHere" 
		  			 : NULL;
	 if(name == NULL) {
	   e = e1;
	   goto tail_recur;
	 }
         lrprint_e(name);
 	 if(stop_at_prefix_end && spe_prefix_full) break;
	 short_print_expr1(e1);
	 lprint_e("%");
	 rprint_e(name);
	 break;
        }

    case STREAM_E:
	{char *separator;
	 if(e->PRIMITIVE == 0) {
	   name      = "Stream";
	   separator = "then";
	 }
	 else {
	   name      = "Mix";
	   separator = "with";
	 }
	 lrprint_e(name);
	 if(stop_at_prefix_end && spe_prefix_full) break;
	 while(kind == STREAM_E) {
	   short_print_expr1(e->E1);
	   lrprint_e(separator);
	   kind = EKIND(e->E2);
	   if(kind == STREAM_E) e = e->E2;
	 }
	 if(stop_at_prefix_end && spe_prefix_full) break;
	 short_print_expr1(e->E2);
	 lprint_e("%");
	 rprint_e(name);
	 break;
        }

    case TEST_E:
	print_e("{");
	if(stop_at_prefix_end && spe_prefix_full) break;
        short_print_expr1(e1);
	if(e->E2 != NULL) {
	  lrprint_e("else");
	  if(stop_at_prefix_end && spe_prefix_full) break;
	  short_print_expr1(e2);
	}
	print_e("}");
	break;

    case LAZY_BOOL_E:
	name = (e->PRIMITIVE == AND_BOOL) ? "_and_" :
	       (e->PRIMITIVE == OR_BOOL)  ?  "_or_" 
	                                  : "_implies_";
	goto do_op;

    case WHERE_E:
	name = "_where_";
    do_op:
	print_e("(");
	if(stop_at_prefix_end && spe_prefix_full) break;
	short_print_expr1(e1);
	lrprint_e(name);
	if(stop_at_prefix_end && spe_prefix_full) break;
	short_print_expr1(e2);
	print_e(")");
	break;

    case CONST_E:
	{int e_kind = e->SCOPE;
	 switch(e_kind) {
	   case STRING_CONST:
	     lprint_e("\"");
	     print_string_e(e->STR);
	     rprint_e("\"");
	     break;

	   case CHAR_CONST:
	      lprint_e("'");
              print_e(e->STR);
	      break;

	   case HERMIT_CONST:
	     print_e("()");
	     break;

	   case BOOLEAN_CONST:
	     lrprint_e((e->STR == NULL) ? "false" : "true");
	     break;

	   case FAIL_CONST:
	     print_e("(fail ");
             print_e(exception_data[e->OFFSET].name);
             print_e(")");
	     break;

	   default:
	     lrprint_e(e->STR);
	 }
         break;
        }

    case IDENTIFIER_E:
    case LOCAL_ID_E:
    case GLOBAL_ID_E:
    case OVERLOAD_E:
    case UNKNOWN_ID_E:
    case PAT_FUN_E:
	lrprint_e(quick_display_name(e->STR));
	break;

    case PAT_VAR_E:
	lprint_e("?");
	if(e->STR != NULL && e->STR[0] != '#') {
	  lrprint_e(quick_display_name(e->STR));
	}
	break;

    case SPECIAL_E:
	kind = e->PRIMITIVE;
	if(kind == PRIM_TARGET) lrprint_e("target");
	else if(kind == PRIM_EXCEPTION) lrprint_e("exception");
        else if(kind == PRIM_SPECIES) print_e("(a species)");
        else if(e->STR != NULL) {
          lrprint_e(quick_display_name(e->STR));
	}
	else lrprint_e("??");
        break;

    case SAME_E:
	e = e1;
	goto tail_recur;

    case APPLY_E:
	print_e("(");
	if(stop_at_prefix_end && spe_prefix_full) break;
	short_print_expr1(e1);
	if(stop_at_prefix_end && spe_prefix_full) break;
	short_print_expr1(e2);
	print_e(")");
	break;

    case SEQUENCE_E:
	lprint_e("Match(?");
	print_e(quick_display_name(e->STR3));
        print_e(",?");
        print_e(quick_display_name(e->STR4));
        print_e(")=");
	if(stop_at_prefix_end && spe_prefix_full) break;
        short_print_expr1(e1);
	lrprint_e("%Match");
	print_e("(");
	if(stop_at_prefix_end && spe_prefix_full) break;
        short_print_expr1(e2);
        print_e(")");
	break;

    case RECUR_E:
	lrprint_e("continue");
	if(stop_at_prefix_end && spe_prefix_full) break;
	if(e1 != NULL) short_print_expr1(e1);
	break;

    case MANUAL_E:
	if(e->PRIMITIVE <= 1) {
	  lrprint_e("Description...%Description");
	}
	else {
	  lrprint_e("Advisory...%Advisory");
	}
	break;	

    case EXPAND_E:
        lrprint_e("Expand");
	short_print_expr1(e1);
	lrprint_e(e->EXPAND_FORM == 0 ? "=>" : "=");
	short_print_expr1(e2);
	lrprint_e("%Expand");
	break;

    case PAT_DCL_E:
      lrprint_e("Pattern");
      short_print_expr1(e2);
      lrprint_e("%Pattern");
      break;

    case PAT_RULE_E:
      short_print_expr1(e2);
      lrprint_e("=>");
      short_print_expr1(e3);
      break;

    default: 
	{char kindch[3];
	 kindch[0] = kind + 'A';
	 kindch[1] = ')';
	 kindch[2] = '\0';
	 lrprint_e("??(");
	 print_e(kindch);
	 break;
	}
  }
}


/****************************************************************
 *			PRINT_CHAR_E				*
 ****************************************************************
 * Print character c as part of an expression into 		*
 * spe_prefix_buffer or spe_suffix_buffer.			*
 ****************************************************************/

PRIVATE void 
print_char_e(char c)
{
  if(spe_lcount > 0) {
    spe_prefix_buffer[spe_prefix_pos++] = c;
    spe_lcount--;
  }
  else {
    spe_prefix_full = TRUE;
    spe_suffix_buffer[spe_suffix_pos++] = c;
    if(spe_rcount < SPE_SUFFIX_BUFFER_SIZE) spe_rcount++;
    if(spe_suffix_pos == SPE_SUFFIX_BUFFER_SIZE) {
      spe_suffix_pos = 0;
      spe_elipsis = TRUE;
    }
  }
}


/****************************************************************
 *			PRINT_E					*
 ****************************************************************
 * Print string s as part of an expression. String s does not   *
 * need a separator on either the left or right side.		*
 ****************************************************************/

PRIVATE void 
print_e(char *s)
{
  char *p;

  if(s == NULL) s = "(null)";
  for(p = s; *p != '\0'; p++) {
    if(spe_lcount > 0) {
      spe_prefix_buffer[spe_prefix_pos++] = *p;
      spe_lcount--;
    }
    else {
      spe_prefix_full = TRUE;
      spe_suffix_buffer[spe_suffix_pos++] = *p;
      if(spe_rcount < SPE_SUFFIX_BUFFER_SIZE) spe_rcount++;
      if(spe_suffix_pos == SPE_SUFFIX_BUFFER_SIZE) {
	spe_suffix_pos = 0;
	spe_elipsis = TRUE;
      }
    }
  }
  need_sep = FALSE;
}


/****************************************************************
 *			LPRINT_E				*
 ****************************************************************
 * Print s with a separator at its left, if necessary.		*
 ****************************************************************/

PRIVATE void 
lprint_e(char *s)
{
  if(need_sep) print_e(" ");
  print_e(s);
}


/****************************************************************
 *			RPRINT_E				*
 ****************************************************************
 * Print s with a separator at its right, if necessary.		*
 ****************************************************************/

PRIVATE void 
rprint_e(char *s)
{
  print_e(s);
  need_sep = TRUE;
}


/****************************************************************
 *			LRPRINT_E				*
 ****************************************************************
 * Print s with a separator at each end, if necessary. 		*
 ****************************************************************/

PRIVATE void 
lrprint_e(char *s)
{
  if(need_sep) print_e(" ");
  print_e(s);
  need_sep = TRUE;
}


/****************************************************************
 *			PRINT_STRING_E				*
 ****************************************************************
 * Print string constant s.  The quotes have been printed.	*
 * Just print the what goes between the quotes, escaping things *
 * that need to be escaped.					*
 ****************************************************************/

PRIVATE void 
print_string_e(char *s)
{
  char *p, c;
  for(p = s; *p != 0; p++) {
    c = *p;
    if(c == '"') print_e("\\\"");
    else if(c == '\\') print_e("\\\\");
    else if(c == '\n') print_e("\\n");
    else if(!isprint(c)) {
      char num[50];
      print_e("\\{");
      sprintf(num, "%d", c);
      print_e(num);
      print_char_e('}');
    }
    else print_char_e(c);
  }
}
