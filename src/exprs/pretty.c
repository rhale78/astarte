/**************************************************************
 * File:    exprs/pretty.c
 * Purpose: Print expressions in pretty form
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

#include <ctype.h>
#ifdef USE_MALLOC_H
#  include <malloc.h>
#else
#  include <stdlib.h>
#endif
#include "../misc/misc.h"
#include "../lexer/modes.h"
#include "../parser/parser.h"
#include "../utils/strops.h"
#include "../utils/hash.h"
#include "../alloc/allocate.h"
#include "../exprs/expr.h"
#include "../exprs/prtexpr.h"
#include "../classes/classes.h"
#include "../generate/prim.h"
#include "../machstrc/machstrc.h"
#include "../machdata/except.h"

#ifdef DEBUG
# include "../debug/debug.h"
#endif

PRIVATE void 
pretty_print_expr1(FILE *f, EXPR *e, int n, PRINT_TYPE_CONTROL *ctl, 
		   int context);
PRIVATE void
pretty_print_types(EXPR *e, int n, PRINT_TYPE_CONTROL* ctl);
PRIVATE void
pretty_print_types2(EXPR *a, EXPR *b, int n, PRINT_TYPE_CONTROL* ctl);

/****************************************************************
 *			MAX_EXPR_PRINT_LINE_LENGTH		*
 ****************************************************************
 * MAX_EXPR_PRINT_LINE_LENGTH is the normal maximum number of	*
 * characters that pretty_print_expr puts on a line.  If the	*
 * indentation becomes severe, it will put more.		*
 *								*
 * MIN_EXPR_PRINT_LINE_LENGTH is the minimum number of 		*
 * characters that pretty_print_exprs considers available on	*
 * a line, after the indent.					*
 ****************************************************************/

#define MAX_EXPR_PRINT_LINE_LENGTH 78
#define MIN_EXPR_PRINT_LINE_LENGTH 20


/****************************************************************
 *			 PRETTY_PRINT_EXPR			*
 ****************************************************************
 * pretty_print_expr(f,e,n) prints expression e, indented n	*
 * spaces, in pretty form on the file f.			*
 *								*
 * context is as for pretty_print_expr1.  Normally, it is	*
 * 0 if expression e is in the pre-pattern-match-translation	*
 * form, and 1 for after-pattern-match-translation form.	*
 ****************************************************************/

void pretty_print_expr(FILE *f, EXPR *e, int n, int context)
{
  PRINT_TYPE_CONTROL ctl;

  begin_printing_types(f, &ctl);
  pretty_print_expr1(f, e, n, &ctl, context);
  pretty_print_types(e, n, &ctl);
  end_printing_types(&ctl);
}


/****************************************************************
 *			 PRETTY_PRINT_PAT_RULE			*
 ****************************************************************
 * print pattern rule formals => translation in pretty form on  *
 * file f, indented n spaces.					*
 ****************************************************************/

void pretty_print_pat_rule(FILE *f, EXPR *formals, EXPR *translation, 
		   	   int n, int context)
{
  PRINT_TYPE_CONTROL ctl;

  begin_printing_types(f, &ctl);
  pretty_print_expr1(f, formals, n, &ctl, context);
  fprintf(f, "      =>\n");
  pretty_print_expr1(f, translation, n, &ctl, context);
  pretty_print_types2(formals, translation, n, &ctl);
  end_printing_types(&ctl);
}


/****************************************************************
 *			 PRT_INDENT				*
 ****************************************************************
 * Print the line number of expression e followed by an indent  *
 * of n spaces on file f.					*
 ****************************************************************/

PRIVATE void 
prt_indent(FILE *f, int n, EXPR *e)
{
  if(e == NULL) fprintf(f, "-     ");
  else fprintf(f, "-%4d ", e->LINE_NUM);
  findent(f,n);
}


/****************************************************************
 *			 INIT_INDENT				*
 ****************************************************************
 * Print n spaces on file f, provided the 			*
 * SUPPRESS_INITIAL_INDENT bit of context is not set.		*
 * The line number of expression e is used as the line.		*
 ****************************************************************/

PRIVATE void 
init_indent(FILE *f, int n, int context, EXPR *e)
{
  if(!(context & SUPPRESS_INITIAL_INDENT)) prt_indent(f,n,e);
}


/****************************************************************
 *			 MAKE_REC_CONTEXT			*
 ****************************************************************
 * Return the context to use for recursive things inside an	*
 * expr that is being printed with context CONTEXT.		*
 ****************************************************************/

PRIVATE int 
make_rec_context(int context)
{
  return context & ~(SUPPRESS_INITIAL_INDENT | SUPPRESS_FINAL_NEWLINE);
} 


/****************************************************************
 *			 NL					*
 ****************************************************************
 * Print a newline on file f, provided the 			*
 * SUPPRESS_FINAL_NEWLINE bit of context is not set.		*
 ****************************************************************/

PRIVATE void 
nl(FILE *f, int context)
{
  if(!(context & SUPPRESS_FINAL_NEWLINE)) fnl(f);
}


/****************************************************************
 *			 PRETTY_PRINT_EXPRS1			*
 ****************************************************************
 * If e is (e1,e2,...,en,()), print on file f			*
 *								*
 *   e1,							*
 *   e2,							*
 *   ...							*
 *   en								*
 *								*
 * where each line is indented n spaces 			*
 *								*
 * Parameters ctl and context is as for pretty_print_expr1, 	*
 * below.							*
 ****************************************************************/

PRIVATE void 
pretty_print_exprs1(FILE *f, EXPR *e, int n, PRINT_TYPE_CONTROL *ctl, 
		    int context)
{
  EXPR* a = skip_sames(e);

  while(EKIND(a) == PAIR_E) {
    EXPR* next = skip_sames(a->E2);
    int rec_context = (EKIND(next) == PAIR_E) 
                     ? context & ~SUPPRESS_FINAL_NEWLINE
                     : context;
    pretty_print_expr1(f, e->E1, n, ctl, rec_context);
    context = context & ~SUPPRESS_INITIAL_INDENT;
    a = next;
    if(EKIND(a) == PAIR_E) fprintf(f, ",\n");
    else if(!(context & SUPPRESS_FINAL_NEWLINE)) fnl(f);
  }
}


/****************************************************************
 *			 PRETTY_PRINT_EXPR1_LINE		*
 ****************************************************************
 * Same as pretty_print_expr1 (below), but forces context to	*
 * include SUPPRESS_INITIAL_INDENT and SUPPRESS_FINAL_NEWLINE.	*
 ****************************************************************/

PRIVATE void 
pretty_print_expr1_line(FILE *f, EXPR *e, int n, PRINT_TYPE_CONTROL *ctl, 
		   	int context)
{
  pretty_print_expr1(f, e, n, ctl, 
		     context | SUPPRESS_INITIAL_INDENT 
			     | SUPPRESS_FINAL_NEWLINE);
}


/****************************************************************
 *			 pretty_print_id			*
 ****************************************************************
 * Print identifier id on file f, with no initial indent or	*
 * final newline.						*
 ****************************************************************/

PRIVATE void 
pretty_print_id(FILE *f, EXPR *id)
{
  fprintf(f, "%s", quick_display_name(id->STR));
}


/****************************************************************
 *			 PRETTY_PRINT_STRING_CONST		*
 ****************************************************************
 * Print string constant s on file f, surrounded by quotes.     *
 ****************************************************************/

PRIVATE void
pretty_print_string_const(FILE *f, char *s)
{
  char *p, c;
  putc('"',f);
  for(p = s; *p != 0; p++) {
    c = *p;
    if(c == '"') fprintf(f, "\\\"");
    else if(c == '\\') fprintf(f, "\\\\");
    else if(!isprint(c)) fprintf(f, "\\{%d}", c);
    else putc(c,f);
  }
  putc('"',f);
}


/****************************************************************
 *			 IS_MAJOR				*
 ****************************************************************
 * Return true if e is a major kind of expression, which should *
 * not be on the same line as another expression.		*
 ****************************************************************/

Boolean is_major_val[] =
{
  /* BAD_E 		*/	0,
  /* IDENTIFIER_E 	*/	0,
  /* GLOBAL_ID_E 	*/	0,
  /* LOCAL_ID_E 	*/	0,
  /* UNKNOWN_ID_E 	*/	0,
  /* OVERLOAD_E 	*/	0,
  /* CONST_E 		*/	0,
  /* SPECIAL_E 		*/	0,
  /* DEFINE_E 		*/	1,
  /* LET_E 		*/	1,
  /* MATCH_E 		*/	1,
  /* OPEN_E 		*/	1,
  /* PAIR_E 		*/	0,
  /* SAME_E 		*/	0,
  /* IF_E 		*/	1,
  /* TRY_E 		*/	1,
  /* TEST_E 		*/	1,
  /* APPLY_E 		*/	0,
  /* STREAM_E 		*/	1,
  /* FUNCTION_E 	*/	0,
  /* SINGLE_E 		*/	1,
  /* LAZY_LIST_E 	*/	0,
  /* AWAIT_E 		*/	0,
  /* LAZY_BOOL_E 	*/	0,
  /* TRAP_E 		*/	1,
  /* LOOP_E 		*/	1,
  /* RECUR_E 		*/	0,
  /* FOR_E 		*/	1,
  /* PAT_FUN_E 		*/	0,
  /* PAT_VAR_E 		*/	0,
  /* PAT_RULE_E 	*/	1,
  /* PAT_DCL_E 		*/	1,
  /* EXPAND_E 		*/	1,
  /* MANUAL_E 		*/	1,
  /* WHERE_E 		*/	0,
  /* EXECUTE_E 		*/	1,
  /* SEQUENCE_E		*/	1
};

PRIVATE Boolean 
is_major(EXPR *e)
{
  EXPR_TAG_TYPE kind;

  e = skip_sames(e);
  kind = EKIND(e);
  if(kind == SPECIAL_E && e->PRIMITIVE == PRIM_SPECIES) return TRUE;
  return is_major_val[kind];
}


/****************************************************************
 *			 HAS_MAJOR				*
 ****************************************************************
 * Return TRUE if expression e has a major expression in it, as *
 * defined by is_major.						*
 ****************************************************************/

PRIVATE Boolean
has_major(EXPR *e)
{
 tail_recur:
  if(e == NULL) return FALSE;

  switch(EKIND(e)) {
      default:
        return FALSE;

      case SPECIAL_E:
	if(e->PRIMITIVE == PRIM_SPECIES) return TRUE;
	else return FALSE;

      case OPEN_E:
      case SINGLE_E:
      case EXECUTE_E:
      case MANUAL_E:
      case LET_E:
      case DEFINE_E:
      case SEQUENCE_E:
      case TEST_E:
      case STREAM_E:
      case LOOP_E:
      case MATCH_E:
      case TRAP_E:
      case PAT_DCL_E:
      case EXPAND_E:
      case IF_E:
      case TRY_E:
      case FOR_E:
      case PAT_RULE_E:
	return TRUE;

      case RECUR_E:
      case SAME_E:
      case LAZY_LIST_E:
	e = e->E1;
	goto tail_recur;

      case APPLY_E:
      case AWAIT_E:
      case PAIR_E:
      case FUNCTION_E:
      case LAZY_BOOL_E:
      case WHERE_E:
	if(has_major(e->E1)) return TRUE;
	e = e->E2;
	goto tail_recur;
  }
}


/****************************************************************
 *			 SHOULD_USE_SINGLE_LINE			*
 ****************************************************************
 * Return true if the structure of e is suitable for printing   *
 * on a single line.						*
 ****************************************************************/

PRIVATE Boolean 
should_use_single_line(EXPR *e)
{
  EXPR_TAG_TYPE kind;
  if(!is_major(e) && has_major(e)) return FALSE;
  kind = EKIND(e);
  if((kind == FOR_E || kind == LOOP_E) && e->STR4 != NULL) return FALSE;
  if(kind == SPECIAL_E && e->PRIMITIVE == PRIM_SPECIES) return FALSE;
  return TRUE;
}


/****************************************************************
 *			 PRETTY_PRINT_EXPR1			*
 ****************************************************************
 * pretty_print_expr1(f,e,n,ctl,context) prints expression e in *
 * pretty form on file f, indented n spaces, using type 	*
 * bindings in ctl.						*
 *								*
 * Parameter context influences how the expression is printed.  *
 * The indications made by context are as follows.		*
 *								*
 * 1. Some aspects of how an expression is represented are	*
 *    changed at pattern match translation.			*
 *    Expression context&AFTER_PATTERN_MATCH is nonzero if	*
 *    expression e is in the form used after pattern matching,  *
 *    and is 0 for before pattern matching.			*
 *								*
 * 2. If context&SUPPRESS_INITIAL_INDENT is nonzero, then	*
 *    no indent (of n spaces) is done on the first line.  If it *
 *    is 0, then there is an indent on the first line.		*
 *								*
 * 3. If context&SUPPRESS_FINAL_NEWLINE is nonzero, then skip	*
 *    the newline at the end of the last line.  Otherwise,	*
 *    put a newline after the last line.			*
 ****************************************************************/

PRIVATE void 
pretty_print_expr1(FILE *f, EXPR *e, int n, PRINT_TYPE_CONTROL *ctl, 
		   int context)
{
  EXPR_TAG_TYPE kind;
  int rec_context;
  char *name;
  EXPR *e1, *e2, *e3;

 tail_recur:
  if(e == NULL_E) return;

  init_indent(f,n,context,e);

  /*------------------------------------------------------------*
   * If the entire thing fits on one line, put it on one line.  *
   * But only do this if it is reasonable.			*
   *------------------------------------------------------------*/

  kind = EKIND(e);
  {int line_len = MAX_EXPR_PRINT_LINE_LENGTH - n;
   if(line_len < MIN_EXPR_PRINT_LINE_LENGTH) {
     line_len = MIN_EXPR_PRINT_LINE_LENGTH;
   }
   if(should_use_single_line(e)) {
     if(try_short_print_expr(f, e, line_len)) {
       nl(f, context);
       return;
     }
   }
  }

  e1 = e->E1;
  e2 = e->E2;
  e3 = e->E3;
  rec_context = make_rec_context(context);
  switch(kind){

    case OPEN_E:
      context = context | SUPPRESS_INITIAL_INDENT;
      fprintf(f, "open ");
      e = e1;
      goto tail_recur;

    case IF_E:
    case TRY_E:
      {int try_kind = e->TRY_KIND;

       /*-----------------------*
        * Print the first line. *
        *-----------------------*/

       name = try_kind == TRY_E ? "Try" : "If";
       fprintf(f, "%s ", name);
       if(kind == TRY_E && try_kind != TRY_F) {
         if(try_kind == TRYEACH_F || try_kind == TRYEACHTERM_F) {
           fprintf(f, "catchingEachThread ");
         }
         if(try_kind == TRYTERM_F || try_kind == TRYEACHTERM_F) {
	   fprintf(f, "catchingTermination ");
         }
       }
       fnl(f);

       /*-----------------*
	* Print the parts *
        *-----------------*/

       pretty_print_expr1(f, e1, n+1, ctl, rec_context);
       if(e2 != NULL && !is_hermit_expr(e2)) {
         prt_indent(f,n,e2);
         fprintf(f, "then\n");
         pretty_print_expr1(f, e2, n+1, ctl, rec_context);
       }
       if(e2 != NULL && !is_hermit_expr(e3)) {
	 prt_indent(f,n,e3);
         fprintf(f, "else\n");
         pretty_print_expr1(f, e3, n+1, ctl, rec_context);
       }
       prt_indent(f,n,e);
       fprintf(f, "%%%s", name);
       break;
      }

    case TRAP_E:
      name = e->TRAP_FORM == TRAP_ATT ? "Trap" : "Untrap";
      if(e->SCOPE != 0) fprintf(f, "open");
      fprintf(f, "%s ", name);
      if(e1 == NULL) fprintf(f, "all ");
      else {
	fnl(f);
	pretty_print_expr1(f, e1, n+1, ctl, rec_context);
	prt_indent(f,n,e2);
      }
      fprintf(f, "=>\n");
      pretty_print_expr1(f, e2, n+1, ctl, rec_context);
      prt_indent(f,n,e);
      fprintf(f, "%%%s", name);
      break;

    case FOR_E:
      fprintf(f, "For\n");
      if(context & AFTER_PATTERN_MATCH || e->STR4 != NULL) {
	prt_indent(f,n,e2);
	fprintf(f,"from\n");
	pretty_print_expr1(f, e2, n+1, ctl, rec_context);
	prt_indent(f,n,e);
	if(e->STR4 != NULL) {
	  fprintf(f, "selection: %s\n", quick_display_name(e->STR4));
	}
	prt_indent(f,n,e1);
	fprintf(f, "match:\n");
	pretty_print_expr1(f, e1, n+1, ctl, rec_context);
      }
      else {
	pretty_print_expr1(f, e1, n+1, ctl, rec_context);
	prt_indent(f,n,e2);
	fprintf(f, "from\n");
	pretty_print_expr1(f, e2, n+1, ctl, rec_context);
      }
      prt_indent(f,n,e3);
      if(e->FOR_FORM != ORDERED_ATT) fprintf(f, "mixed ");
      fprintf(f, "do\n");
      pretty_print_expr1(f, e3, n+1, ctl, rec_context);
      prt_indent(f,n,e);
      fprintf(f, "%%For");
      break;

    case LOOP_E:
      if(e->OPEN_LOOP != 0) fprintf(f, "open ");
      fprintf(f, "Loop\n");
      if(e1 != NULL) {
	pretty_print_expr1(f, e1->E1, n+1, ctl, 
			   rec_context | SUPPRESS_FINAL_NEWLINE);
	fprintf(f, "=\n");
	pretty_print_expr1(f, e1->E2, n+1, ctl, rec_context);
      }
      if(e->STR4 != NULL) {
	prt_indent(f, n, e);
	fprintf(f, "target: %s\n", quick_display_name(e->STR4));
      }
      prt_indent(f,n,e2);
      fprintf(f, "body:\n");
      pretty_print_expr1(f, e2, n+1, ctl, rec_context);
      prt_indent(f,n,e);
      fprintf(f, "%%Loop");
      break;

    case AWAIT_E:
      if(e1 == NULL) {
	fprintf(f, "(:");
	pretty_print_expr1(f, e2, n+1, ctl, 
			   rec_context | SUPPRESS_INITIAL_INDENT);
	fprintf(f, ":)");
      }
      else {
	fprintf(f, "Await ");
	fnl(f);
	pretty_print_exprs1(f, e1, n+1, ctl, rec_context);
	prt_indent(f,n,e2);
	fprintf(f, "then\n");
	pretty_print_expr1(f, e2, n+1, ctl, rec_context);
	prt_indent(f, n, e);
	fprintf(f, "%%Await");
      }
      break;

    case LAZY_LIST_E:
      fprintf(f, "[:\n");
      pretty_print_expr1(f, e1, n+1, ctl, rec_context);
      prt_indent(f,n,e);
      fprintf(f, ":]");
      break;

    case PAIR_E:
      fprintf(f, "(");
      pretty_print_expr1_line(f, e1, n+1, ctl, rec_context);
      fprintf(f, ",\n");
      pretty_print_expr1(f, e2, n+1, ctl, 
			 rec_context | SUPPRESS_FINAL_NEWLINE);
      fprintf(f, ")");
      break;

    case FUNCTION_E:
      fprintf(f, "(");
      if(is_stack_expr(e1)) fprintf(f, "?");
      pretty_print_expr1_line(f, e1, n+1, ctl, rec_context);
      fprintf(f, " =>\n");
      pretty_print_expr1_line(f, e2, n+1, ctl, 
			      rec_context | SUPPRESS_FINAL_NEWLINE);
      fprintf(f, ")");
      break;

    case DEFINE_E:
    case LET_E:
    case MATCH_E:
      name = (kind == DEFINE_E) ? "Define" : 
	     (kind == MATCH_E)  ? "Match"  :
	     (e->LET_FORM == 0) ? "Let"
	                        : "Relet";
      fprintf(f, "%s ", name);
      pretty_print_expr1_line(f, e1, n+1, ctl, rec_context);
      fprintf(f, " =\n");
      pretty_print_expr1(f, e2, n+1, ctl, rec_context);
      prt_indent(f, n, e);
      fprintf(f, "%%%s", name);
      break;

    case EXECUTE_E:
    case SINGLE_E:
      {int s_kind = e->SINGLE_MODE;
      
       name = (kind == EXECUTE_E)     ? "Execute" :
	      (s_kind == FIRST_ATT)   ? "Unique"  :
	      (s_kind == CUTHERE_ATT) ? "CutHere" :
	      (s_kind == ATOMIC_ATT)  ? "Atomic"  
				      : NULL;
       if(name == NULL) {
	 e = e1;
	 context = context | SUPPRESS_INITIAL_INDENT;
	 goto tail_recur;
       }
       else {
	 fprintf(f, "%s\n", name);
	 pretty_print_expr1(f, e1, n+1, ctl, rec_context);
	 prt_indent(f,n,e);
	 fprintf(f, "%%%s", name);
       }
       break;
      }

    case STREAM_E:
      {char *separator;

       if(e->STREAM_MODE == STREAM_ATT) {
	 name      = "Stream";
	 separator = "then";
       }
       else {
	 name      = "Mix";
	 separator = "with";
       }
       fprintf(f, "%s\n", name);
       pretty_print_expr1(f, e1, n+1, ctl, rec_context);
       prt_indent(f,n,e2);
       fprintf(f, separator);
       pretty_print_expr1(f, e2, n+1, ctl, rec_context);
       prt_indent(f,n,e);
       fprintf(f, "%%%s", name);
       break;
      }

    case TEST_E:
      fprintf(f, "{");
      pretty_print_expr1_line(f, e1, n+1, ctl, 
			      rec_context | SUPPRESS_INITIAL_INDENT);
      if(e2 != NULL) {
	fnl(f);
	prt_indent(f,n,e2);
	fprintf(f, "else\n");
	pretty_print_expr1(f, e2, n+1, ctl, rec_context);
	prt_indent(f,n,e);
      }
      fprintf(f, "}");
      break;

    case WHERE_E:
    case LAZY_BOOL_E:
      {int b_kind = e->LAZY_BOOL_FORM;
       name = (kind == WHERE_E)    ? "_where_" :
	      (b_kind == AND_BOOL) ? "_and_"   :
	      (b_kind == OR_BOOL)  ? "_or_"    
         			   : "_implies_";
       fprintf(f, "(");
       pretty_print_expr1(f, e1, n+1, ctl, 
			  rec_context | SUPPRESS_INITIAL_INDENT);
       prt_indent(f,n,e);
       fprintf(f, name);
       fnl(f);
       pretty_print_expr1(f, e2, n+1, ctl, 
			  rec_context | SUPPRESS_FINAL_NEWLINE);
       fprintf(f, ")");
       break;
      }

    case CONST_E:
      name = e->STR;
      switch(e->SCOPE) {
	case CHAR_CONST:
	  fprintf(f, "'%s", name);
	  break;

	case STRING_CONST:
	  pretty_print_string_const(f, name);
	  break;

	case NAT_CONST:
        case REAL_CONST:
	  fprintf(f, "%s", name);
	  break;

	case HERMIT_CONST:
	  fprintf(f, "()");
	  break;

	case BOOLEAN_CONST:
	  fprintf(f, name == NULL ? "false" : "true");
	  break;

	case FAIL_CONST:
	  fprintf(f, "(fail %s)", exception_data[e->OFFSET].name);
      }
      break;

    case GLOBAL_ID_E:
    case IDENTIFIER_E:
    case UNKNOWN_ID_E:
    case OVERLOAD_E:
    case PAT_FUN_E:
      pretty_print_id(f,e);
      break;

    case PAT_VAR_E:
      fprintf(f, "?");
      if(e1 != NULL) pretty_print_id(f, e1);
      break;

    case SPECIAL_E:
      {int s_kind = e->PRIMITIVE;
       switch(s_kind) {
         case PRIM_SPECIES:
	   print_ty1_without_constraints(e->E1->ty, ctl);
	   goto special_end;   /* Below: end of case SPECIAL_E */
	 
         case PRIM_TARGET:
	   name = "target";
	   break;

	 case PRIM_EXCEPTION:
	   name = "exception";
	   break;

	 default:
	   name = quick_display_name(e->STR);
	   if(name == NULL) name = "??";
	   break;
      }
      fprintf(f, name);

     special_end:
      break;
    }

    case SAME_E:
      e = e1;
      context = context | SUPPRESS_INITIAL_INDENT;
      goto tail_recur;

    case APPLY_E:
      if(e1->ty != NULL && is_hermit_type(e1->ty)) {
        while(kind == APPLY_E && e1->ty != NULL && is_hermit_type(e1->ty)) {
          pretty_print_expr1(f, e1, n, ctl, 
			     rec_context | SUPPRESS_INITIAL_INDENT);
	  kind = EKIND(skip_sames(e2));
	  if(kind == APPLY_E) {
	    e = skip_sames(e2);
	    e1 = e->E1;
	    e2 = e->E2;
	  }
	  else e = e2;
	  init_indent(f,n,0,e);
        }
	context = context | SUPPRESS_INITIAL_INDENT;
	goto tail_recur;
      }
      else {
	fprintf(f, "(");
	pretty_print_expr1_line(f, e1, n+1, ctl, rec_context);
	fprintf(f, ")\n");
	prt_indent(f,n,e1);
	fprintf(f, "(");
	pretty_print_expr1_line(f, e2, n+1, ctl, rec_context);
	fprintf(f, ")");
      }
      break;

    case SEQUENCE_E:
      {EXPR *match, *pat, *id1, *id2;
       int line = e->LINE_NUM;
       id1 = id_expr(e->STR3, line);
       id2 = id_expr(e->STR4, line);
       pat = new_expr2(PAIR_E, id1, id2, line);
       bump_expr(match = new_expr2(MATCH_E, pat, e1, line));
       pretty_print_expr1(f, match, n, ctl, rec_context);
       drop_expr(match);

       prt_indent(f,n,match);
       fprintf(f, "(");
       pretty_print_expr1_line(f, match, n, ctl, rec_context);
       fprintf(f, ")");
       break;
      }

    case RECUR_E:
      fprintf(f, "continue\n");
      pretty_print_expr1(f, e1, n+1, ctl, rec_context);
      break;

    case MANUAL_E:
      if(e->PRIMITIVE <= 1) {
	fprintf(f, "Description...%%Description");
      }
      else {
	fprintf(f, "Advisory...%%Advisory");
      }
      break;	

    case EXPAND_E:
      fprintf(f, "Expand\n");
      pretty_print_expr1(f, e1, n+1, ctl, rec_context);
      prt_indent(f,n,e2);
      fprintf(f, e->EXPAND_FORM == 0 ? "=>\n" : "=\n");
      pretty_print_expr1(f, e2, n+1, ctl, rec_context);
      prt_indent(f,n,e);
      fprintf(f, "%%Expand");
      break;

    case PAT_DCL_E:
      fprintf(f, "Pattern\n");
      pretty_print_expr1(f, e2, n+1, ctl, rec_context);
      prt_indent(f,n,e);
      fprintf(f, "%%Pattern");
      break;

    case PAT_RULE_E:
      pretty_print_expr1(f, e2, n+1, ctl, rec_context);
      prt_indent(f, n, e3);
      fprintf(f, "=");
      pretty_print_expr1(f, e3, n+1, ctl, rec_context);
      break;

    default:
      break;
  }
  nl(f,context);
}


/****************************************************************
 *			 PRETTY_PRINT_TYPES			*
 ****************************************************************
 * Print the types of the identifiers in expression e, indented *
 * n spaces.							*
 *								*
 * Parameter ctl tells where and how to print the types.	*
 ****************************************************************/

/*--------------------------------------------------------------*
 * When the type of identifier x at line n is printed,		*
 * string nx is put into table pretty_print_types_tbl		*
 * so that the same thing can be skipped the next time		*
 * it is encountered.						*
 *--------------------------------------------------------------*/

PRIVATE int                 pretty_print_types_indent;
PRIVATE PRINT_TYPE_CONTROL* pretty_print_types_ctl;
PRIVATE HASH1_TABLE*        pretty_print_types_tbl;

/*------------------------------------------------------------*/

PRIVATE Boolean 
pretty_print_types_help(EXPR **e)
{
  if(is_id_or_patvar_p(*e) || is_target_expr(*e)) {
    HASH_KEY u;
    HASH1_CELLPTR h;
    EXPR* sse  = skip_sames(*e);
    char* name = is_target_expr(sse) ? "target" : sse->STR;
    int   size = strlen(name) + 6;

    u.str = (char*) BAREMALLOC(size);
    snprintf(u.str, size, "%d%s", (*e)->LINE_NUM, name);
    h = insert_loc_hash1(&pretty_print_types_tbl, u, strhash(u.str), equalstr);
    if(h->key.str == NULL) {
      FILE* f = pretty_print_types_ctl->f;
      h->key.str = u.str;
      fprintf(f, "-%4d ", (*e)->LINE_NUM);
      findent(f, pretty_print_types_indent);
      fprintf(f, "%s: ", quick_display_name(name));
      print_ty1_without_constraints(sse->ty, pretty_print_types_ctl);
      fnl(f);
    }
    else FREE(u.str);
    return 1;
  }
  else return 0;
}

/*--------------------------------------------------------------*/

PRIVATE void
pretty_print_types_clear(HASH1_CELLPTR h)
{
  FREE(h->key.str);
}


/*--------------------------------------------------------------*/

PRIVATE void
pretty_print_types(EXPR *e, int n, PRINT_TYPE_CONTROL* ctl)
{
  fprintf(ctl->f, "-----Types:\n");
  pretty_print_types_indent = n;
  pretty_print_types_ctl    = ctl;
  pretty_print_types_tbl    = NULL;
  scan_expr(&e, pretty_print_types_help, FALSE);
  scan_hash1(pretty_print_types_tbl, pretty_print_types_clear);
  free_hash1(pretty_print_types_tbl);
}


/****************************************************************
 *			 PRETTY_PRINT_TYPES2			*
 ****************************************************************
 * Same as pretty_print_types, but print types in both a and b. *
 ****************************************************************/

PRIVATE void
pretty_print_types2(EXPR *a, EXPR *b, int n, PRINT_TYPE_CONTROL* ctl)
{
  fprintf(ctl->f, "-----Types:\n");
  pretty_print_types_indent = n;
  pretty_print_types_ctl    = ctl;
  pretty_print_types_tbl    = NULL;
  scan_expr(&a, pretty_print_types_help, FALSE);
  scan_expr(&b, pretty_print_types_help, FALSE);
  scan_hash1(pretty_print_types_tbl, pretty_print_types_clear);
  free_hash1(pretty_print_types_tbl);
}
