/*****************************************************************
 * File:    dcls/report.c
 * Purpose: Routines to report declarations in the listing.
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

/************************************************************************
 * When a declaration is processed, some information is often printed	*
 * in the listing after the declaration.  That information is held	*
 * until the declaration and printed when a line is finished.  This	*
 * file manages remembering and printing such information.		*
 ************************************************************************/

#include <string.h>
#ifdef USE_MALLOC_H
#  include <malloc.h>
#else
#  include <stdlib.h>
#endif
#include "../misc/misc.h"
#include "../utils/lists.h"
#include "../utils/strops.h"
#include "../utils/filename.h"
#include "../alloc/allocate.h"
#include "../error/error.h"
#include "../tables/tables.h"
#include "../parser/parser.h"
#include "../lexer/modes.h"
#include "../lexer/lexer.h"
#include "../unify/unify.h"
#include "../alloc/allocate.h"
#include "../classes/classes.h"
#include "../generate/generate.h"
#include "../generate/prim.h"
#include "../exprs/expr.h"
#include "../dcls/dcls.h"
#include "../infer/infer.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif


/****************************************************************
 *			PUBLIC VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			delay_reports				*
 ****************************************************************
 * Reports are delayed until delay_report is 0.			*
 ****************************************************************/

int delay_reports = 0;

/****************************************************************
 *			show_all_reports			*
 *			set_show_all_reports			*
 *			list_reports_without_listing		*
 ****************************************************************
 * If show_all_reports is true, then reports will be put into   *
 * the listing for all packages, including imported ones.  	*
 * When show_all_reports is false, reports are only done for	*
 * the current package.						*
 *								*
 * When option -a is selected, reports well be shown for all	*
 * packages except the standard one.  For that option, 		*
 * set_show_all_reports is made true, which causes 		*
 * show_all_reports to be made true after the standard package	*
 * has been handled.						*
 *								*
 * when list_reports_without_lising is true, reports will be    *
 * shown even when a listing is not being generated.		*
 ****************************************************************/

Boolean show_all_reports             = FALSE;
Boolean set_show_all_reports         = FALSE;
Boolean list_reports_without_listing = FALSE;

/****************************************************************
 *			reports					*
 *			import_reports				*
 ****************************************************************
 * reports is a chain of structures that give information about *
 * declarations that need to be reported in the listing.	*
 *								*
 * import_reports is a chain of names of imported packages that *
 * are to be reported in the listing.				*
 ****************************************************************/

REPORT_RECORD*	reports        = NULL;
REPORT_RECORD*	import_reports = NULL;


/****************************************************************
 *	      		REPORT_DCL_P 				*
 *			REPORT_DCL_AUX_P			*
 ****************************************************************
 * report_dcl_aux_p places an entry for reporting in the	*
 * program listing.  The parameters are put into the entry.     *
 * The meaning of the parameters depends on the kind.  See	*
 * print_reports_p, below, for the possibilities.		*
 *								*
 * Note: If no listing is being generated currently, then	*
 * no report entry is made by report_dcl_aux_p.			*
 * 								*
 * If name begins with HIDE_CHAR, then no report is made,	*
 * unless tracing is turned on.					*
 *								*
 * report_dcl_p is similar to report_dcl_aux_p, but assumes     *
 * that the aux, ctc and LP fields are NULL.			*
 *								*
 * mode		is the mode of the declaration.			*
 *		It is a safe pointer: it will not be kept	*
 *		longer than the lifetime of this function call.	*
 *								*
 * XREF: 							*
 *   Called in tables/classtbl.c to report creating or 		*
 *   extending a genus or community.				*
 *								*
 *   Called in tables/meettbl.c to report meets.		*
 *								*
 *   Called in dclclass.c to report creation of some automatic  *
 *   functions.							*
 *								*
 *   Called in lexer/lexsup.c to report an import.		*
 *								*
 *   Called in tables/globtbl.c to report an entity id 		*
 *   definition.						*
 ****************************************************************/

void report_dcl_aux_p(char *name, int kind, MODE_TYPE *mode, TYPE *ty, 
		      ROLE *role, char *aux, CLASS_TABLE_CELL *ctc, LPAIR lp)
{
  if(kind == IMPORT_E && gen_listing == 3) return;

# ifdef DEBUG
    if(name[0] == HIDE_CHAR && !trace) return;
# else
    if(name[0] == HIDE_CHAR) return;
# endif

  if(kind == IMPORT_E ||
     show_all_reports ||
     ((should_list() || list_reports_without_listing)
      && main_context != INIT_CX)
#    ifdef DEBUG
       || trace
#    endif
  ) {

    REPORT_RECORD* r = allocate_report_record();

    r->name = (kind == IMPORT_E) ? name : name_tail(name);
    bump_type(r->type = ty);
    bump_role(r->role = role);
    r->kind = kind;
    r->mode = copy_mode(mode);
    r->aux  = aux;
    r->ctc  = ctc;
    r->lp   = lp;
    if(kind != IMPORT_E) {
      r->next = reports;
      reports = r;
    }
    else {
      r->next        = import_reports;
      import_reports = r;
    }
  }
}

/*---------------------------------------------------------------*/

void report_dcl_p(char *name, int kind, MODE_TYPE *mode, TYPE *ty, ROLE *role)
{
  report_dcl_aux_p(name, kind, mode, ty, role, NULL, NULL, NULL_LP);
}


/****************************************************************
 * 			SORT_REPORTS_P				*
 ****************************************************************
 * sort_reports_p(r) sorts report record chain r into descending*
 * order first by the kind field and then by the name field.    *
 * The sort is stable.						*
 *								*
 * sort_reports_help(r,k,rest) sorts the first k members of 	*
 * chain r, and returns that sorted chain.  It also sets *rest  *
 * to point to the (k+1)st member of the chain, or to NULL if   *
 * there are only k members.					*
 * 								*
 * merge_reports(r,s) merges report chains r and s, which must  *
 * already be sorted.						*
 *								*
 * All of these routines are destructive: The achieve the	*
 * sorting by rearranging pointers rather than by copying.	*
 ****************************************************************/

/*------------------------------------------------------------------*
 * Merge sorted chains r and s into a single sorted chain, and	    *
 * return the resulting chain.  The merge is destructive.	    *
 *------------------------------------------------------------------*/

PRIVATE REPORT_RECORD*
merge_reports(REPORT_RECORD* r, REPORT_RECORD* s)
{
  if(r == NULL) return s;
  if(s == NULL) return r;
  {int   r_kind = r->kind;
   int   s_kind = s->kind;
   char* r_name = r->name;
   char* s_name = s->name;
   if(r_kind > s_kind ||
      (r_kind == s_kind &&
       (r_name == NULL || (s_name != NULL && strcmp(r_name, s_name) >= 0)))) {
     r->next = merge_reports(r->next, s);
     return r;
   }
   else {
     s->next = merge_reports(r, s->next);
     return s;
   }
  }
}

/*------------------------------------------------------------------*
 * Sort the first k cells in chain l_reports, and return a pointer  *
 * to the sorted chain.  Set *rest to the suffix of l_reports that  *
 * comes after the first k cells.  The sort is destructive.	    *
 *------------------------------------------------------------------*/

PRIVATE REPORT_RECORD* 
sort_reports_help(REPORT_RECORD* l_reports, int k, REPORT_RECORD** rest)
{
  if(k == 1) {
    *rest = l_reports->next;
    l_reports->next = NULL;
    return l_reports;
  }
  else {
    int half = k>>1;
    REPORT_RECORD *r1, *r2, *rest1;
    r1 = sort_reports_help(l_reports, half, &rest1);
    r2 = sort_reports_help(rest1, k-half, rest);
    return merge_reports(r1, r2);
  }
}

/*------------------------------------------------------------------*/

PRIVATE REPORT_RECORD* sort_reports_p(REPORT_RECORD* l_reports)
{
  if(l_reports == NULL) return NULL;

  /*------------------------------------------------------------*
   * Count the number of reports, then use sort_reports_help to *
   * sort.							*
   *------------------------------------------------------------*/

  {REPORT_RECORD* rest = l_reports;
   int k = 0;
   while(rest != NULL) {k++; rest = rest->next;}
   return sort_reports_help(l_reports, k, &rest);
  }
}


/****************************************************************
 *			REDUCE_REPORTS				*
 ****************************************************************
 * Remove expectations from report record chain rep that are    *
 * subsumed by other definitions or expectations in rep.	*
 *								*
 * This operation is destructive.  It changes the records in    *
 * chain rep, and frees the records that are deleted from the   *
 * chain.							*
 *								*
 * The return value is the reduced chain.			*
 *								*
 * Helper function reduce_reports_help sets the type field to   *
 * NULL in each EXPECT_E or ANTICIPATE_E node that is subsumed. *
 ****************************************************************/

PRIVATE void reduce_reports_help(REPORT_RECORD* rep)
{
  REPORT_RECORD *p, *q;
  for(p = rep; p != NULL; p = p->next) {
    int p_kind = p->kind;
    if(p_kind == EXPECT_E || p_kind == ANTICIPATE_E) {
      for(q = rep; q != NULL; q = q->next) {
        if(q != p) {
	  int q_kind = q->kind;
          if(q->name == p->name && 
             (q_kind == EXPECT_E || q_kind == LET_E || q_kind == DEFINE_E)) {
	    if(q->type != NULL && 
	       half_overlap_u(p->type, q->type) == EQUAL_OR_CONTAINED_IN_OV) {
	      SET_TYPE(p->type, NULL);
	    }
	  }
        }
      }
    }
  }
}

/*------------------------------------------------------------*/

PRIVATE REPORT_RECORD* reduce_reports(REPORT_RECORD* rep)
{
  REPORT_RECORD *result, **prev_ptr, *p;

  reduce_reports_help(rep);
  result   = rep;
  prev_ptr = &result;
  p = rep;
  while(p != NULL) {
    int p_kind = p->kind;
    if((p_kind == EXPECT_E || p_kind == ANTICIPATE_E) && p->type == NULL) {
      *prev_ptr = p->next;
      p->next = NULL;
      free_report_record(p);
    }
    else {
      prev_ptr = &(p->next);
    }
    p = *prev_ptr;
  }
  return result;
}


/****************************************************************
 *			PRINT_REPORTS_P				*
 ****************************************************************
 * Print the reports in report record 'reports'.  The work is   *
 * done in print_reports1_p.					*
 *								*
 * The possible forms of the report are as follows.  name, type *
 * etc are the fields of the REPORT_RECORD, as set by		*
 * report_dcl_aux_p.  ctc represents the thing for which ctc	*
 * is the class-table entry (ctc->name)				*
 *								*
 * kind			What is being reported			*
 *								*
 * TYPE_E		Species name				*
 *								*
 * FAM_E		Family name				*
 *								*
 * GENUS_E		Genus name				*
 *								*
 * COMM_E		Community name				*
 *								*
 * EXTEND_E		Relate ctc(lp) isKindOf name.		*
 *								*
 * MEET_E		Relate name & aux = ctc(lp)		*
 *								*
 * ANTICIPATE_E		Anticipate name: type/role		*
 *								*
 * EXPECT_E		Expect name: type/role			*
 *								*
 * PAT_FUN_E		Pattern name: type/role			*
 *								*
 * EXPAND_AUX_E		Additional information for a pattern 	*
 *			or expand declaration.  name is the	*
 *			name of the pattern function or		*
 *			expander and aux is the name of the	*
 *			variable whose type is to be reported.	*
 *								*
 * EXPAND_E		Expand name: type/role			*
 *								*
 * LET_E		Let name: type/role			*
 *								*
 * DEFINE_E		Define name: type/role			*
 *								*
 * BEHAVIOR_E		name: type/role, where role has been	*
 *			added to name:type.			*
 *								*
 * IMPORT_E		Import name.  (These are placed in	*
 *			import_reports rather than in reports)	*
 *								*
 * XREF: print_reports is called in lexer.lex at the end of	*
 * a line, and by compiler.c at a premature exit.		*
 ****************************************************************/

PRIVATE int report_mode_mask =
  (IRREGULAR_MODE_MASK | 
   DEFAULT_MODE_MASK   | 
   OVERRIDES_MODE_MASK | 
   UNDERRIDES_MODE_MASK);

/*------------------------------------------------------*/

PRIVATE char *report_kind_name[] =
{
 "Import ",
 "Species ",
 "Parameterized species ",
 "Abstraction ",  
 "Parameterized abstraction ",
 "",
 "Relate ",
 "Relate ",
 "Relate ",
 ""
};

/*------------------------------------------------------*/

PRIVATE void print_reports1_p(REPORT_RECORD *r)
{
  int n, kind;
  LONG mode;
  char *name;

  if(r == NULL) return;

  mode = get_define_mode(r->mode);
  kind = r->kind;
  name = r->name;
  print_reports1_p(r->next);

  /*---------------------------------------------------------------*
   * For kinds at or beyond IMPORT_E, we do the report in the form *
   *								   *
   *	-------> Import ...					   *
   *								   *
   * or a similar form suitable for the kind.			   *
   *---------------------------------------------------------------*/

  if(kind >= IMPORT_E && kind <= LAST_E) {

    if(kind == IMPORT_E) err_nl();

    if(kind != EXPAND_AUX_E) {
      err_print_str("------> ");
      err_print_str(report_kind_name[kind - IMPORT_E]);
    }

    if(kind == IMPORT_E) {
      char* ext_name = strdup(name);
      force_external(ext_name);
      err_print_str("%s", ext_name);
      FREE(ext_name);
    }
    else if(kind == EXPAND_AUX_E) {
      err_print_str("   %19s: ", display_name(r->aux));
      err_print_rt(r->type, r->role);
    }
    else if(kind != EXTEND_E) err_print_str("%s", name);

    if(kind == MEET_E) {
      err_print_str(" & %s = ", display_name(r->aux));
      err_print_labeled_type(r->ctc, r->lp);
    }
    else if(kind == EXTEND_E) {
      err_print_labeled_type(r->ctc, r->lp);
      err_print_str(" isKindOf %s", name);
    }
    err_nl();
    return;
  }

  /*------------------------------------------------------*
   * Other kinds of reports are reported, for example, as *
   *							  *
   *  Expct-> name: type				  *
   *							  *
   * except LET_E, DEFINE_E, etc., where the mode is	  *
   * shown, as for example				  *
   *							  *
   *  {d}----> name: type				  *
   *							  *
   * for default mode.					  *
   *------------------------------------------------------*/

  else if(kind == EXPECT_E)     err_print_str("Expct");
  else if(kind == ANTICIPATE_E) err_print_str("Ant--");
  else if(kind == PAT_FUN_E)    err_print_str("Pat--");
  else if(kind == EXPAND_E)     err_print_str("Expnd");
  else if(kind == BEHAVIOR_E)   err_print_str("Beh--");
  else if((mode & report_mode_mask) != 0) {
    n = 2;
    err_print_str("{");
    if(mode & IRREGULAR_MODE_MASK) {err_print_str("i"); n++;}
    if(mode & DEFAULT_MODE_MASK) {err_print_str("d"); n++;}
    if(mode & OVERRIDES_MODE_MASK) {err_print_str("o"); n++;}
    if(mode & UNDERRIDES_MODE_MASK) {err_print_str("u"); n++;}
    err_print_str("}");
    for(;n < 5; n++) err_print_str("-");
  }
  else err_print_str("-----");
  err_print_str("-> ");

  /*-------------------------------------*
   * Now print the name: type/role part. *
   *-------------------------------------*/

  if(kind == BEHAVIOR_E) {
    err_print_str("%s for ", r->aux);
  }
  err_print_str("%-15s: ", display_name(r->name));
  err_print_rt_with_constraints_indented(r->type, r->role, 26);
  err_nl();
}

/*------------------------------------------------------*/

void print_reports_p()
{
  Boolean doing_listing;
  if(delay_reports || (reports == NULL && import_reports == NULL)) return;

  doing_listing = should_list();
  if((doing_listing || list_reports_without_listing || show_all_reports)
#   ifdef DEBUG
      || trace
#   endif
  ) {

    if(doing_listing) err_nl(); 
    else err_nl_if_needed();

    print_reports1_p(sort_reports_p(reduce_reports(reports)));

    if(import_reports != NULL) {
      print_reports1_p(import_reports);
      free_report_record(import_reports);
      import_reports = NULL;
    }

    if(doing_listing) err_nl();
  }

  if(reports != NULL) {
    free_report_record(reports);
    reports = NULL;
  }
}
