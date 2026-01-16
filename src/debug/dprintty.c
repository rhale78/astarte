/*****************************************************************
 * File:    debug/dprintty.c
 * Purpose: Print a type in long format, for debugging
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


#include "../misc/misc.h"

#ifdef DEBUG
#include "../classes/classes.h"
#include "../debug/debug.h"

/****************************************************************
 *			type_kind_name				*
 ****************************************************************
 * This array is used when printing types.  It tells the name   *
 * of each tag for TYPE.					*
 ****************************************************************/

char* type_kind_name[] =        /* Names of type tags. */
{
 "FAM_MEM_T",
 "FUNCTION_T",
 "PAIR_T",
 "BAD_T",
 "TYPE_ID_T",
 "FAM_T",
 "FICTITIOUS_TYPE_T",
 "FICTITIOUS_FAM_T",
 "WRAP_TYPE_T",
 "WRAP_FAM_T",
 "FICTITIOUS_WRAP_TYPE_T",
 "FICTITIOUS_WRAP_FAM_T",
 "MARK_T",
 "TYPE_VAR_T",
 "FAM_VAR_T",
 "PRIMARY_TYPE_VAR_T",
 "PRIMARY_FAM_VAR_T",
 "WRAP_TYPE_VAR_T",
 "WRAP_FAM_VAR_T"
};


/****************************************************************
 *			LONG_PRINT_TY				*
 ****************************************************************
 * long_print_ty(t,n) prints type t, indented n spaces, in long *
 * format.							*
 ****************************************************************/

void long_print_ty(TYPE *t, int n)
{
  if(t == NULL_T){
    indent(n); 
    fprintf(TRACE_FILE, "NULL_TYPE\n");
  }
  else {
    indent(n); 
    trace_s(12, t, t->THASH, toint(t->seenTimes));
    indent(n); 
    trace_s(13,
	    type_kind_name[toint(TKIND(t))], toint(TKIND(t)), 
	    toint(t->ref_cnt), toint(t->hermit_f), toint(t->seen), 
	    toint(t->standard), toint(t->special), toint(t->dfsmark));
    indent(n);
    trace_s(14, toint(t->free), toint(t->copy), toint(t->freeze));
    if(t->prmark != 0) {
      indent(n); 
      fprintf(TRACE_FILE, "(loop %p)\n", t);
    }
    else {
      t->prmark = 1;
      switch(TKIND(t)){
        case FAM_MEM_T:
        case PAIR_T:
        case FUNCTION_T:
	  indent(n); 
          fprintf(TRACE_FILE, "ty1 =\n");
	  long_print_ty(t->TY1, n+1);
	  indent(n);
	  fprintf(TRACE_FILE, "ty2 =\n");
	  long_print_ty(t->TY2, n+1);
#         ifdef NEVER
            print_two_types(t->TY1, t->TY2, n+1);
#         endif
	  break;

        case TYPE_ID_T:
        case FAM_T:
        case FICTITIOUS_TYPE_T:
        case FICTITIOUS_FAM_T:
	case WRAP_TYPE_T:
        case WRAP_FAM_T:
        case FICTITIOUS_WRAP_TYPE_T:
        case FICTITIOUS_WRAP_FAM_T:
	  indent(n); 
	  if(t->ctc == NULL) fprintf(TRACE_FILE, "name = (null)\n");
          else fprintf(TRACE_FILE, "name = %s\n", nonnull(t->ctc->name));
	  break;

        case MARK_T:
	  indent(n); 
	  fprintf(TRACE_FILE, "ty1 =\n"); 
	  long_print_ty(t->ty1, n+1);
	  break;
	  
        case TYPE_VAR_T:
        case FAM_VAR_T:
        case PRIMARY_TYPE_VAR_T:
        case PRIMARY_FAM_VAR_T:
        case WRAP_TYPE_VAR_T:
        case WRAP_FAM_VAR_T:
	  indent(n); 
	  if(t->ctc == NULL) fprintf(TRACE_FILE, "domain = (null)\n");
	  else fprintf(TRACE_FILE, "domain = %s\n", nonnull(t->ctc->name));

	  indent(n); 
	  fprintf(TRACE_FILE, "ty1 =\n"); 
	  long_print_ty(t->ty1, n+1);

	  indent(n);
	  fprintf(TRACE_FILE, "lower bounds = ");
	  print_type_list_without_constraints(t->LOWER_BOUNDS);
	  tracenl();
	  break;

        case BAD_T:
	  indent(n);
	  fprintf(TRACE_FILE, "STR2 = %s\n", nonnull(t->STR2));
	  break;

      }
      t->prmark = 0;
    }    
  }
}


#endif
