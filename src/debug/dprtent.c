/********************************************************************
 * File:    debug/dprtent.c
 * Purpose: Functions to print entities
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


#include "../misc/misc.h"

#ifdef DEBUG
#include "../alloc/allocate.h"
#include "../utils/hash.h"
#include "../utils/strops.h"
#include "../classes/classes.h"
#include "../tables/tables.h"
#include "../intrprtr/intrprtr.h"
#include "../evaluate/evaluate.h"
#include "../machdata/entity.h"
#include "../machdata/except.h"
#include "../machstrc/machstrc.h"
#include "../gc/gc.h"
#include "../rts/rts.h"
#include "../show/prtent.h"
#include "../debug/debug.h"


/********************************************************
 *			LONG_PRINT_ENTITY		*
 ********************************************************
 * Print entity e, indented n spaces, on the trace	*
 * file in long form.  If full is true, 		*
 * print activations for lazy entities and functions.	*
 ********************************************************/

void long_print_entity(ENTITY e, int n, Boolean full)
{
  ENTITY *a;

  indent(n); trace_i(22, GCTAG(e));

  if(MEMBER(GCTAG(e), entp_tags)) {
    trace_i(275, ENTVAL(e));
  }

  switch(GCTAG(e)) {
    case NOREF_TAG:
    case BOX_TAG:
      fprintf(TRACE_FILE, "val = %ld\n", NRVAL(e));
      return;

    case GLOBAL_TAG:
      fprintf(TRACE_FILE, "global-name = %s\n", outer_bindings[VAL(e)].name);
      return;

    case GLOBAL_INDIRECT_TAG:
       a = ENTVAL(e);
       indent(n);
       trace_i(23);
       long_print_entity(a[0], n+1, full);
       indent(n); trace_ty(TYPEVAL(a[1]));
       tracenl();
       return;

    case INDIRECT_TAG:
      indent(n);
      if(ENT_EQ(e, NOTHING)) fprintf(TRACE_FILE, "NOTHING\n");
      else {
	trace_i(24, ENTVAL(e));
	long_print_entity(*ENTVAL(e), n+1, full);
      }
      return;

    case LAZY_LIST_TAG:
      tracenl();
      if(full) print_up_control(TRACE_FILE,CTLVAL(e), 1);
      return;

    case LAZY_TAG:
      tracenl();
      if(full) print_down_control(TRACE_FILE,CTLVAL(e), "", 1);
      return;

    case LAZY_PRIM_TAG:
      a = ENTVAL(e);
      indent(n);
      trace_i(29, NRVAL(a[0]));
      long_print_entity(a[1], n+1, full);
      long_print_entity(a[2], n+1, full);
      return;

    case FUNCTION_TAG:
      tracenl();
      if(full) print_continuation(CONTVAL(e));
      else tracenl();
      return;

    case FAIL_TAG:
    case QWRAP0_TAG:
    case QWRAP1_TAG:
    case QWRAP2_TAG:
    case QWRAP3_TAG:
      a = ENTVAL(e);
      long_print_entity(*a, n+1, full);
      return;

    case WRAP_TAG:

      /*--------------------------------------------*
       * The value is a pointer to a pair (tag,val) *
       *--------------------------------------------*/

      {ENTITY* w = ENTVAL(e);
       indent(n);
       fprintf(TRACE_FILE, "(wrap) ");
       if(GCTAG(w[0]) == NOREF_TAG) {
	 fprintf(TRACE_FILE, "%ld\n", NRVAL(w[0]));
       }
       else {
	 trace_ty(TYPEVAL(w[0]));
	 tracenl();
       }
       long_print_entity(w[1], n+1, full);
       return;
      }

    case BIGPOSINT_TAG: 
    case BIGNEGINT_TAG:
      {CHUNKPTR chunk = BIVAL(e);
       intcellptr buff = BIGINT_BUFF(chunk);
       LONG    size    = BIGINT_SIZE(chunk);
       LONG i;
       fprintf(TRACE_FILE, "val = {[%p:%ld] ", chunk, size);
       for(i = 0; i < size; i++) {
	 fprintf(TRACE_FILE, "%04x ", buff[i]);
       }
#      ifdef NEVER
         fprintf(TRACE_FILE, " | %s}\n", ast_num_to_str(e,zero));
#      endif
       fprintf(TRACE_FILE, "}\n");
       return;
      }

    case SMALL_REAL_TAG:
      {SMALL_REAL* sr = SRVAL(e);
       trace_i(25, sr->val);
       return;
      }

    case LARGE_REAL_TAG:
      {LARGE_REAL* lr = LRVAL(e);
       indent(n);
       fprintf(TRACE_FILE, "(lg real)\n");
       indent(n);
       fprintf(TRACE_FILE, "man =    ");
#      ifdef NEVER
         trace_print_entity(lr->man);
         tracenl();
#      endif
       long_print_entity(lr->man, 0, 0);
       indent(n);
       fprintf(TRACE_FILE, " ex =     ");
#      ifdef NEVER
         trace_print_entity(lr->ex);
         tracenl();
#      endif
       long_print_entity(lr->ex, 0, 0);
       return;
     }

    case APPEND_TAG:
    case PAIR_TAG:
    case DEMON_TAG:
      a = ENTVAL(e);
      indent(n);
      fprintf(TRACE_FILE, "first =\n");
      long_print_entity(a[0], n+1, full);
      indent(n);
      fprintf(TRACE_FILE, "second =\n");
      long_print_entity(a[1], n+1, full);
      return;

    case TREE_TAG:
      a = ENTVAL(e);
      indent(n);
      fprintf(TRACE_FILE, "key = \n");
      long_print_entity(a[0], n+1, full);
      indent(n);
      fprintf(TRACE_FILE, "front = \n");
      long_print_entity(a[1], n+1, full);
      indent(n);
      fprintf(TRACE_FILE, "front len = ");
      trace_print_entity(a[2]);
      tracenl();
      indent(n);
      fprintf(TRACE_FILE, "rear = \n");
      long_print_entity(a[3], n+1, full);
      indent(n);
      fprintf(TRACE_FILE, "rear len = ");
      trace_print_entity(a[4]);
      tracenl();
      return;

    case CSTR_TAG:
      fprintf(TRACE_FILE, "addr = %p, val = \"%s\"\n", CSTRVAL(e), CSTRVAL(e));
      return;

    case STRING_TAG:
      {CHUNKPTR      chunk = BIVAL(e);
       UBYTE HUGEPTR buff  = (UBYTE HUGEPTR) STRING_BUFF(chunk);
       LONG          size  = STRING_SIZE(chunk);
       LONG i;
       fprintf(TRACE_FILE, "val = {[%p:%ld] ", chunk, size);
       for(i = 0; i < size; i++) {
	 fprintf(TRACE_FILE, "%02x ", buff[i]);
       }
       if(only_printable(buff, size)) {
         fprintf(TRACE_FILE, " | \"");
	 for(i = 0; i < size; i++) {
           fprintf(TRACE_FILE, "%c", buff[i]);
         }
	 fprintf(TRACE_FILE, "\"");
       }
       fprintf(TRACE_FILE, "}\n");
       return;
      }      

    case TRIPLE_TAG:
    case QUAD_TAG:
    /*case QUINT_TAG:*/
      {int i, t;
       a = ENTVAL(e);
       t = GCTAG(e);
       for(i = PAIR_TAG; i < t + 2; i++) {
	 long_print_entity(a[i - PAIR_TAG], n+1, full);
       }
       return;
      }

    case ARRAY_TAG:
      {LONG len;
       int tag;

       a   = ENTVAL(e);
       len = IVAL(a[0]);
       tag = GCTAG(a[1]);
       indent(n);
       trace_i(42, tag);
       trace_i(26, len);
       if(len > 100) len = 100;  /* In case of trouble */
       if(tag == BOX_TAG) {
         indent(n);
	 trace_i(27, VAL(a[1]));
	 return;
       }
       else if(tag == STRING_TAG) {
	 char* format;
	 CHUNKPTR chunk  = BIVAL(a[1]);
	 LONG     offset = IVAL(a[3]);
	 charptr  buff   = STRING_BUFF(chunk) + offset;
	 LONG i;

         indent(n);
	 format = only_printable(buff, len) ? "%x:%c " : "%x ";
	 for(i = 0; i < len; i++) {
	   register int c = (UBYTE) (buff[i]);
	   fprintf(TRACE_FILE, format, c, c);
	 }
	 tracenl();
       }
       else if(tag != PLACE_TAG) {
	 int k;
	 ENTITY* ax = ENTVAL(a[1]);

         indent(n);
	 trace_i(28);
	 for(k = 0; k < len; k++) {
           long_print_entity(ax[k],n+1,full);
         }
       }

       /*--------------------------------------------------------*
	* Print the follower.  BOX_TAG, the only kind without a	 *
 	* follower, was cut off above.				 *
	*--------------------------------------------------------*/

       indent(n);
       fprintf(TRACE_FILE, "Follower:\n");
       long_print_entity(a[2], n+1, full);

       /*-------------------------------------------------------*
        * Print the amount to mark beyond the end of the array, *
        * for an INDIRECT_TAG array.				*
        *-------------------------------------------------------*/

       if(tag == INDIRECT_TAG) {
         indent(n);
         fprintf(TRACE_FILE, "Mark after: ");
	 long_print_entity(a[3], n+1, full);
       }
       return;
      }

    case TYPE_TAG:
      {TYPE* t = TYPEVAL(e);
       fprintf(TRACE_FILE, "type: ");
       trace_ty(t);
       tracenl();
       return;
      }

    case FILE_TAG:
      {struct file_entity* fent = FILEENT_VAL(e);
       int kind = fent->kind;
       trace_i(288, kind, fent->mode);
       if(kind == INFILE_FK) {
	 indent(n);
	 trace_i(289, nonnull(fent->u.file_data.name), fent->u.file_data.pos);
	 long_print_entity(fent->u.file_data.val, n+1, full);
       }
       return;
      }

    default:
      tracenl();
      return;
  }
}

#endif


