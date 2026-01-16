/********************************************************************
 * File:    show/prtent.c
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

#include <string.h>
#ifdef USE_MALLOC_H
#  include <malloc.h>
#else
#  include <stdlib.h>
#endif
#include "../misc/misc.h"
#include "../alloc/allocate.h"
#include "../utils/hash.h"
#include "../utils/lists.h"
#include "../utils/strops.h"
#include "../utils/rdwrt.h"
#include "../classes/classes.h"
#include "../tables/tables.h"
#include "../machdata/entity.h"
#include "../machdata/except.h"
#include "../machdata/gc.h"
#include "../machstrc/machstrc.h"
#include "../gc/gc.h"
#include "../evaluate/evaluate.h"
#include "../evaluate/instruc.h"
#include "../intrprtr/intrprtr.h"
#include "../standard/stdtypes.h"
#include "../standard/stdids.h"
#include "../rts/rts.h"
#include "../show/gprint.h"
#include "../show/prtent.h"
#include "../show/printrts.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/********************************************************
 *			PRINT_ENTITY_WITH_STATE		*
 ********************************************************
 * Print e, of type t, on file where.			*
 * If where = NULL, print in the config window.		*
 * Do not print a newline.				*
 *							*
 * st is the state and tv is the trap vector 		*
 * for evaluation of $.					*
 *							*
 * If suppress is true, simulate print_entity, but do	*
 * not print anything.  This is used to get boxes that  *
 * are encountered to be stored in the box tables,      *
 * for print_rts.					*
 *							*
 * max_chars is the maximum number of characters to     *
 * print.  If more than that many are requested, the    *
 * print is stopped and ... is shown.			*
 *							*
 * max_chars is a soft limit.  It might be exceeded.    *
 *							*
 * The return value is the number of characters printed.*
 *							*
 * Note: The time spent converting to a string is       *
 * limited by PRINT_ENT_TIME.  If the conversion takes  *
 * too long, then it will be cut off, and a partial     *
 * result will be shown.				*
 ********************************************************/

LONG print_entity_with_state(ENTITY e, TYPE *t, FILE *where, 
			     STATE *st, TRAP_VEC *tv, Boolean suppress,
			     LONG max_chars)
{
  ENTITY dollar;
  LONG time_bound, chars_printed = 0;
  char str[MAX_LAZY_NAME_LENGTH];
  TYPE *dollar_type1;
  REG_TYPE mark;

  if(failure >= 0) return 0;

  /*-------------------------------------------*
   ******* Check for special conditions. *******
   *-------------------------------------------*/

  if(MARKED(e)) {
    if(!suppress) {
      gprintf(where, "(MARKED)");
      return 8;
    }
    else return 0;
  }

  if(BASICTAG(e) == RELOCATE_TAG) {
    if(!suppress) {
      if(ENT_EQ(e, GARBAGE)) {
        gprintf(where, "(GARBAGE)");
        return 9;
      }
      else {
	gprintf(where, "(RELOCATED)");
        return 11;
      }
    }
    else return 0;
  }

  if(ENT_EQ(e, NOTHING)) {
    if(!suppress) {
      gprintf(where, "(NOTHING)");
      return 9;
    }
    else return 0;
  }

  /*---------------------------------*
   ******* Print a lazy value. *******
   *---------------------------------*/

  if(is_lazy(e)) {
    if(!suppress) {
      char* name = lazy_name(str, e);
      gprintf(where, "%s", name);
      return strlen(name);
    }
    else return 0;
  }

  /*------------------------------------*
   ******* Print a nonlazy value. *******
   *------------------------------------*/

  mark = reg1(&dollar);
  reg1_param(&e);
  in_show++;
  break_info.suppress_breaks++;

  /*-------------------------------------------------*
   * First get the $ function that will convert this *
   * entity to a string.			     *
   *-------------------------------------------------*/

  if(t != NULL) {
    LONG l_time = PRINT_ENT_TIME;
    bump_type(dollar_type1 = function_t(t, list_t(char_type)));
    dollar = get_and_eval_global(DOLLAR_SYM, dollar_type1, &l_time);
    drop_type(dollar_type1);
  }
  else {
    failure = GLOBAL_ID_EX;
    dollar  = zero;             /* Suppress uninitialized warning */
  }

  /*-------------------------------------------------------------------*
   * If $ is not defined for this type, then print the representation. *
   *-------------------------------------------------------------------*/

  if(failure >= 0) {
    failure = -1;
    failure_as_entity = NOTHING;
    if(!suppress) {
      chars_printed = print_entity(where, e, max_chars);
    }
  }

  /*------------------*
   * Otherwise run $. *
   *------------------*/

  else {
    ENTITY dollar_result;
    reg1(&dollar_result);

    time_bound     = PRINT_ENT_TIME;
    dollar_result  = run_fun(dollar, e, st, tv, &time_bound);

    /*-----------------------------------------------------------*
     * Now print the string, unless such printing is suppressed. *
     *-----------------------------------------------------------*/

    if(!suppress) {

      /*------------------------------------------------------------*
       * If $ succeeded, then print its result using ast_print_str. *
       *------------------------------------------------------------*/

      if(failure < 0) {
        ENTITY defer;
	time_bound = PRINT_ENT_TIME;
	defer = ast_print_str(where, dollar_result, &time_bound, 
			      max_chars, &chars_printed);
	if(failure == TIME_OUT_EX) {
	  gprintf(where, "(...$ timed out)");
	  chars_printed += 16;
	}
        else if(!IS_NIL(defer)) {
	  gprintf(where, "...");
	  chars_printed += 3;
	}
      }

      /*--------------------------------------------------------------*
       * If $ failed, then print the representation.  If $ timed-out, *
       * then print the representation of the string produced by      *
       * $.  That will prevent further evaluation.		      *
       *--------------------------------------------------------------*/

      else {
	if(failure == TIME_OUT_EX) {
	  chars_printed = print_entity(where, dollar_result, max_chars) + 16;
	  gprintf(where, "(...$ timed out)");
	}
	else {
	  chars_printed = print_entity(where, e, max_chars);
	}
      }
    }
  }
  failure           = -1;
  failure_as_entity = NOTHING;

  in_show--;
  break_info.suppress_breaks--;
  unreg(mark);
  return chars_printed;
}


/********************************************************
 *		   TRACE_PRINT_ENTITY			*
 ********************************************************
 * Print entity e on the trace file, in raw form.	*
 ********************************************************/

#ifdef DEBUG
void trace_print_entity(ENTITY e)
{
  print_entity(TRACE_FILE, e, LONG_MAX);
}
#endif


/********************************************************
 *			PRINT_ENTITY			*
 ********************************************************
 * Print the internal form of e on file where.		*
 *							*
 * max_chars is the maximum number of characters to 	*
 * print.  If the print is cut off, ... is printed at   *
 * the end.						*
 *							*
 * max_chars is a soft limit.  It might be exceeded.    *
 *							*
 * The return value is the number of characters printed.*
 ********************************************************/

LONG print_entity(FILE *where, ENTITY e, LONG max_chars)
{
  Boolean suppress = FALSE;
  LONG result;
  result = print_ent(e, where, 0, 0, max_chars, &suppress);
  if(suppress) {
    gprintf(where, "...");
    return result + 3;
  }
  else return result;
}


/********************************************************
 *			TRAVERSE_TREE			*
 ********************************************************
 * Show the preorder traversal of tree t on file where.	*
 * need_comma is true if a comma should be put in front *
 * of the tree description.				*
 *							*
 * In this and its helper functions, max_chars is the   *
 * maximum number of characters to print, and the	*
 * return value if the number of characters printed.	*
 * Parameter suppress is set true if some output was	*
 * suppressed.						*
 ********************************************************/

PRIVATE LONG 
traverse_tree(FILE *where, ENTITY t, Boolean need_comma, 
	      LONG max_chars, Boolean *suppress);

/*-----------------------------------------------------------*
 * print_rev prints list l in reverse order.  Each member of *
 * list l is a representation of a list, and the members of  *
 * each such list are printed in forward order.		     *
 *-----------------------------------------------------------*/

PRIVATE LONG 
print_rev(FILE *where, ENTITY l, Boolean need_comma,
	  LONG max_chars, Boolean *suppress)
{ ENTITY h, t;
  REG_TYPE mark;
  LONG chars_printed = 0;

  if(TAG(l) == NOREF_TAG) return 0;

  if(max_chars <= 0 || *suppress) {
    *suppress = TRUE;
    return 0;
  }

  mark = reg2(&h, &t);

  ast_split(l, &h, &t);
  chars_printed += print_rev(where, t, need_comma, max_chars, suppress);
  if(*suppress) goto out;

  while(TAG(h) == INDIRECT_TAG) h = *ENTVAL(h);

  if(TAG(h) == TREE_TAG) {
    chars_printed +=
      traverse_tree(where, h, TAG(t) != NOREF_TAG, 
		    max_chars - chars_printed, suppress);
  }
  else if(TAG(h) != NOREF_TAG) {
    gprintf(where, "]++");
    chars_printed += 6 + print_ent(h, where, 0, 0, 
				   max_chars - chars_printed, suppress);
    if(!(*suppress)) gprintf(where, "++[");
  }

 out:
  unreg(mark);
  return chars_printed;
}

/*------------------------------------------------------------*
 * print_forw prints list l in forward order.  Each member of *
 * list l is a representation of a list, and the members of   *
 * each such list are printed in forward order.		      *
 *------------------------------------------------------------*/

PRIVATE LONG 
print_forw(FILE *where, ENTITY l, Boolean need_comma,
	   LONG max_chars, Boolean *suppress)
{
  int tag;
  ENTITY s, h;
  REG_TYPE mark;
  LONG chars_printed = 0;

  if(max_chars <= 0 || *suppress) {
    *suppress = TRUE;
    return 0;
  }

  mark = reg2(&s, &h);
  s    = l;
  while((tag = TAG(s)) != NOREF_TAG) {

    if(*suppress) goto out;

    if(tag == APPEND_TAG) {
      ENTITY* p = ENTVAL(s);
      chars_printed += print_forw(where, p[0], need_comma,
				  max_chars - chars_printed, suppress);
      s = p[1];
      need_comma = TRUE;
      continue;
    }

    while(tag == INDIRECT_TAG) {
      s   = *ENTVAL(s);
      tag = TAG(s);
    }

    if(tag == NOREF_TAG) break;

    if(tag == LAZY_PRIM_TAG) {
      ENTITY* q = ENTVAL(s);
      if(NRVAL(q[0]) == REVERSE_TMO) {
	chars_printed += print_rev(where, q[1], need_comma,
				   max_chars - chars_printed, suppress);
	s = q[2];
	need_comma = TAG(q[1]) != NOREF_TAG;
	continue;
      }
      goto out;
    }

    else {
      h = ast_head(s);
      while(TAG(h) == INDIRECT_TAG) h = *ENTVAL(h);
      if(TAG(h) == TREE_TAG) {
        chars_printed += traverse_tree(where, h, need_comma,
				       max_chars - chars_printed, suppress);
      }
      else if(TAG(h) != NOREF_TAG) {
	gprintf(where, "]++");
	chars_printed = print_ent(h, where, 0, 0, max_chars - chars_printed,
				  suppress);
	if(!(*suppress)) gprintf(where, "++[");
	need_comma = FALSE;
      }
      s = ast_tail(s);
    }
  }

 out:
  unreg(mark);
  return chars_printed;
}

/*---------------------------------------------------------------*/

PRIVATE LONG 
traverse_tree(FILE *where, ENTITY t, Boolean need_comma, 
	     LONG max_chars, Boolean *suppress)
{
  ENTITY* p = ENTVAL(t);
  LONG chars_printed = 0;

  if(max_chars <= 0 || *suppress) {
    *suppress = TRUE;
    return 0;
  }

  if(ENT_NE(p[0], NOTHING)) {
    if(need_comma) {
      gprintf(where, ",");
      chars_printed++;
    }
    chars_printed += print_ent(p[0], where, 0, 0, 
			       max_chars - chars_printed, suppress);
    need_comma = TRUE;
  }
  chars_printed += print_forw(where, p[1], need_comma,
			      max_chars - chars_printed, suppress);
  chars_printed += print_rev(where, p[3], TRUE, 
			     max_chars - chars_printed, suppress);
  return chars_printed;
}


/********************************************************
 *			PRINT_ENT			*
 ********************************************************
 * Print entity e on file where, in internal form.	*
 * If in_pair is 1, this is the second (or later) 	*
 * entry in a pair, so can skip parentheses if this	*
 * is a pair.  After printing e, print parens right	*
 * parentheses.  So if parens is 3, we print ")))" at   *
 * the end.						*
 *							*
 * max_chars is the maximum number of characters to 	*
 * print.  max_chars is a soft limit.  It might be	*
 * exceeded.    					*
 *							*
 * *suppress is set true if any output is suppressed    *
 * due to the max_chars limit.				*
 *							*
 * The return value is the number of characters printed.*
 ********************************************************/

LONG print_ent(ENTITY e, FILE *where, Boolean in_pair, int parens,
	       LONG max_chars, Boolean *suppress)
{
  int e_tag;
  LONG chars_printed = 0;

again:
  if(chars_printed >= max_chars || *suppress) {
    *suppress = TRUE;
    goto out;
  }

  e_tag = BASICTAG(e);
  switch(e_tag) {
    case NOREF_TAG:
      {char str[20];
       sprintf(str, "%ld", IVAL(e));
       gprintf(where, "%s", str);
       chars_printed += strlen(str);
       goto out;
      }

    case LAZY_TAG:
      {char str[MAX_LAZY_NAME_LENGTH];
#      ifdef DEBUG
#        ifdef NEVER
	 if(trace) {
	   gprintf(where,"(lazy ");
	   print_down_control(where, CTLVAL(e), "", 1);
	   gprintf(where,")");
	 }
	 else
#        endif
#      endif
       {gprintf(where, lazy_name(str, e));
	chars_printed += strlen(str);
       }
       goto out;
      }

    case LAZY_LIST_TAG:
      {char str[MAX_LAZY_NAME_LENGTH];
#      ifdef DEBUG
#        ifdef NEVER
	  if(trace) {
	   gprintf(where, "(lazy list ");
	   print_up_control(where, CTLVAL(e), 1);
	   gprintf(where, ")");
	 }
	 else
#        endif
#      endif
       {gprintf(where, lazy_name(str, e));
	chars_printed += strlen(str);
       }
	goto out;
      }

    case LAZY_PRIM_TAG:
#     ifdef DEBUG
	{ENTITY* p = ENTVAL(e);
	 int tag = toint(NRVAL(p[0]));
	 while(tag >= UNKNOWN_TMO) {
	   ENTITY binding = prcontent_stdf(p[1]);
	   if(ENT_EQ(binding, NOTHING)) break;
	   if(trace) gprintf(where, "(*%ld)", box_rank(p[1]));
	   if(TAG(binding) != LAZY_PRIM_TAG) {
	     e = binding;
	     break;
	   }
	   else {
	     p   = ENTVAL(e = binding);
	     tag = toint(NRVAL(p[0]));
	   }
	 }
	}
#     else
	{Boolean b;
	 e = scan_through_unknowns(e, &b);
	}
#     endif
      e_tag = BASICTAG(e);
      if(e_tag != LAZY_PRIM_TAG) goto again;
      /* No break -- fall through to next case. */

    case GLOBAL_TAG:
    case FAIL_TAG:
    case FILE_TAG:
      {
#      ifdef SMALL_STACK
         char* str = (char*) BAREMALLOC(MAX_FILE_NAME_LENGTH + 20);
#      else
         char str[MAX_FILE_NAME_LENGTH + 20];
#      endif

       gprintf(where, "%s", lazy_name(str, e));
       chars_printed += strlen(str);

#      ifdef SMALL_STACK
         FREE(str);
#      endif

#      ifdef DEBUG
         if(trace_extra && BASICTAG(e) == LAZY_PRIM_TAG) {
	   fprintf(TRACE_FILE, "Val:\n");
	   long_print_entity(e, 1, 0);
	 }
#      endif

       goto out;
      }

    case GLOBAL_INDIRECT_TAG:
      {ENTITY *ap;
       TYPE *t;

       ap = ENTVAL(e);
       if(TAG(ap[1]) != TYPE_TAG) die(126);

       t = TYPEVAL(ap[1]);
       bump_type(t = freeze_type(t));

#      ifdef DEBUG
	 if(trace) gprintf(where, "^");
#      endif

       if(TAG(ap[0]) == GLOBAL_TAG) {
         char* name = outer_bindings[VAL(ap[0])].name;
	 gprintf(where, "(global %s:", name);
         chars_printed += strlen(name) + 10 + fprint_ty(where, t);
	 gprintf(where, ")");
	 drop_type(t);
	 goto out;
       }
       else {
	 e       = ap[0];
	 in_pair = 0;
	 drop_type(t);
	 goto again;
       }
      }

    case INDIRECT_TAG:
      if(VAL(e) == 0) {
	gprintf(where, "NOTHING");
	chars_printed += 7;
      }
      else {

#	ifdef DEBUG
	  if(trace) gprintf(where, "*");
#	endif

	e = *ENTVAL(e);
	goto again;
      }
     goto out;

    case FUNCTION_TAG:
      {char* name = function_name(e);
       gprintf(where, "(function %s)", name);
       chars_printed += strlen(name) + 11;
       goto out;
      }

    case BIGPOSINT_TAG:
    case BIGNEGINT_TAG:
      {char* str = ast_num_to_str(e, zero);
       int n = gprint_str(where, str, max_chars - chars_printed);
       if(n < toint(strlen(str))) *suppress = TRUE;
       chars_printed += n;
       goto out;
      }

    case SMALL_REAL_TAG:
      {char str[30];
       sprintf(str, "%24.16e", SRVAL(e)->val);
       gprintf(where, str);
       chars_printed += strlen(str);
       goto out;
      }

    case LARGE_REAL_TAG:
      {if(TAG(LRVAL(e)->man) == RELOCATE_TAG) {
	 gprintf(where, "(GARBAGE)");
	 chars_printed += 9;
       }
       else {
         char* str = ast_num_to_str(e,ENTI(40));
	 gprint_str(where, str, -1);
	 chars_printed += strlen(str);
       }
       goto out;
      }

    case WRAP_TAG:
      {ENTITY* w     = ENTVAL(e);
       int    w0tag = BASICTAG(w[0]);
       if(w0tag == NOREF_TAG) {
	 gprintf(where, "{");
         chars_printed += 1;
       }
       else {
	 gprintf(where, "<<");
	 chars_printed += 2;
       }
       chars_printed += print_ent(w[1], where, 0, 0, 
				  max_chars - chars_printed, suppress);
       if(*suppress) goto out;
       gprintf(where, ":");
       chars_printed += 1;
       if(w0tag == NOREF_TAG) {
	 char str[20];
	 sprintf(str, "%ld", (LONG)NRVAL(w[0]));
	 gprintf(where, str);
	 chars_printed += strlen(str);
       }
       else {
	 chars_printed += fprint_ty(where, TYPEVAL(w[0]));
       }
       if(w0tag == NOREF_TAG) {
	 gprintf(where, "}");
         chars_printed += 1;
       }
       else {
	 gprintf(where, ">>");
	 chars_printed += 2;
       }
       goto out;
     }

    case QWRAP0_TAG:
    case QWRAP1_TAG:
    case QWRAP2_TAG:
    case QWRAP3_TAG:
      {char str[20];
       gprintf(where, "{");
       chars_printed += 1 + print_ent(*ENTVAL(e), where, 0, 0, 
				      max_chars - chars_printed, suppress);
       if(*suppress) goto out;
       sprintf(str, "%ld", (LONG)(e_tag - QWRAP0_TAG));
       gprintf(where, ":%s}", str);
       chars_printed += strlen(str) + 2;
       goto out;
      }

    case CSTR_TAG:
      {int n;
       gprintf(where, "\"");
       n = gprint_str(where, CSTRVAL(e), max_chars - chars_printed);
       chars_printed += n + 2;
       if(n < toint(strlen(CSTRVAL(e)))) {
	 *suppress = TRUE;
	 goto out;
       }
       gprintf(where, "\"");
       goto out;
      }

    case STRING_TAG:
      {CHUNKPTR chunk = BIVAL(e);
       LONG     size  = STRING_SIZE(chunk);
       LONG n;
       unsigned char HUGEPTR buff = (unsigned char HUGEPTR)STRING_BUFF(chunk);
       if(only_printable(buff, size)) {
	 n = max_chars - chars_printed;
	 if(n > size) n = size;
	 else *suppress = TRUE;
         gprintf(where, "\"");
         gprint_str(where, buff, n);
	 chars_printed -= n + 2;
	 if(*suppress) goto out;
         gprintf(where, "\"");
       } 
       else {
         LONG i;
	 gprintf(where, "(");
	 chars_printed++;
	 for(i = 0; i < size - 1; i++) {
	   if(chars_printed >= max_chars) {
	     *suppress = TRUE;
	     break;
	   }
	   gprintf(where, "%02x,", buff[i]);
	   chars_printed += 3;
         }
	 if(!(*suppress) && chars_printed < max_chars) {
	   gprintf(where, "%02x,0)", buff[i]);
	   chars_printed += 5;
	 }
	 else *suppress = TRUE;
       }
       goto out;
      }

    case ARRAY_TAG:
      {ENTITY HUGEPTR a;
       LONG len, a1tag;

       a     = ENTVAL(e);
       a1tag = BASICTAG(a[1]);
       len   = IVAL(a[0]);

       if(a1tag == BOX_TAG) {
	 char str[60];
	 LONG rank1 = VAL(a[1]);
	 sprintf(str, "(nonsharedBoxes %ld..%ld)", rank1, rank1 + len - 1);
	 gprintf(where, str);
	 chars_printed += strlen(str);
       }

       else if(a1tag == PLACE_TAG) {
	 char str[60];
	 LONG rank1 = box_rank(a[1]);
	 sprintf(str, "(sharedBoxes %ld..%ld)", rank1, rank1 + len - 1);
	 gprintf(where, str);
	 chars_printed += strlen(str);
	 if(!IS_NIL(a[2])) {
           gprintf(where, "++");
	   chars_printed += 2;
	   e       = a[2];
	   in_pair = 0;
	   goto again;
	 }
       }

       else if(a1tag == INDIRECT_TAG) {
	 int k;
         ENTITY *ap;
	 ap = ENTVAL(a[1]);
	 if(!in_pair) {
	   gprintf(where, "(");
	   chars_printed++;
	 }
	 for(k = 0; k < len-1; k++) {
	   chars_printed += print_ent(ap[k], where, 0, 0, 
				      max_chars - chars_printed, suppress);
	   if(*suppress) break;
	   gprintf(where, ",");
	 }
	 chars_printed += print_ent(ap[len-1], where, 1, 0,
				    max_chars - chars_printed, suppress);
	 if(*suppress) goto out;
	 if(!IS_NIL(a[2])) {
	   gprintf(where, ",");
	   chars_printed += 1 + print_ent(a[2], where, 1, 0,
					  max_chars - chars_printed, suppress);
	 }
	 if(*suppress) goto out;
	 if(!in_pair) {
	   gprintf(where, ",0)");
	   chars_printed += 3;
	 }
       }

       else if(a1tag == STRING_TAG) {
         LONG i;
	 int n;
         char str[20];
         CHUNKPTR chunk = BIVAL(a[1]);
         charptr  buff  = STRING_BUFF(chunk) + IVAL(a[3]);

	 if(!in_pair) {
	   gprintf(where, "(");
	   chars_printed++;
	 }
         gprintf(where, "{");
	 chars_printed++;
         for(i = 0; i < len-1; i++) {
	   if(*suppress) break;
	   sprintf(str, "%d,", buff[i]);
	   gprintf(where, str);
	   chars_printed += strlen(str);
         }
	 if(chars_printed < max_chars) {
	   sprintf(str, "%d", buff[i]);
	   gprintf(where, str);
	   chars_printed += strlen(str);
	 }
         else *suppress = TRUE;
         if(chars_printed < max_chars && only_printable(buff, len)) {
	   n = max_chars - chars_printed;
	   if(n > len) n = len;
           gprintf(where, "|\"");
	   n = gprint_str(where, buff, n);
           if(n < len) *suppress = TRUE;
	   gprintf(where, "\"");
	   chars_printed += n + 3;
         }
         if(*suppress) goto out;
         gprintf(where, "}");
	 chars_printed++;

         if(!IS_NIL(a[2])) {
           gprintf(where, ",");
	   chars_printed += print_ent(a[2], where, 1, 0, 
				      max_chars - chars_printed, suppress);
         }
         if(*suppress) goto out;
	 if(!in_pair) {
	   gprintf(where, ")");
	   chars_printed++;
	 }
       }

       else if(a1tag == ENTITY_TOP_TAG) {
	 int k;
	 ENTITY* ap = ENTVAL(a[1]);

	 for(k = 0; k < len; k++) {
	   chars_printed += print_ent(ap[k], where, 0, 0,
				      max_chars - chars_printed, suppress);
	   if(*suppress) break;
	   gprintf(where, "++");
	 }
	 e       = a[2];
         in_pair = 0;
         goto again;
       }

       else {
	 gprintf(where, "(An unknown kind of array)");
	 chars_printed += 26;
       }

       goto out;
     }

    case APPEND_TAG:
      {ENTITY* pair = ENTVAL(e);
       chars_printed += print_ent(pair[0], where, 0, 0, 
				  max_chars - chars_printed, suppress);
       if(*suppress) goto out;
       gprintf(where, "++");
       e       = pair[1];
       in_pair = 0;
       goto again;
      }

    case TREE_TAG:
#     ifdef DEBUG
	if(trace) gprintf(where, "tree");
#     endif

      gprintf(where, "[");
      chars_printed += 2 +
	traverse_tree(where, e, FALSE, max_chars - chars_printed, suppress);
      if(!(*suppress)) gprintf(where, "]");
      goto out;

    case DEMON_TAG:
      {ENTITY* a = ENTVAL(e);
       gprintf(where, "demon(");
       chars_printed += 6 + 
         print_ent(*a, where, 0, 0, max_chars - chars_printed, suppress);
       if(*suppress) goto out;
       gprintf(where, ",");
       chars_printed += 6 + 
	 print_ent(a[1], where, 0, 0, max_chars - chars_printed, suppress);
       if(*suppress) goto out;
       gprintf(where, ")");
       goto out;
     }

    case PAIR_TAG:  /* Also RATIONAL_TAG */
    case TRIPLE_TAG:
    case QUAD_TAG:
    /*case QUINT_TAG:*/
      {ENTITY *a, ee;
       int i, tag;
       ee = e;
       tag = BASICTAG(ee);
       if(!in_pair) {
	 gprintf(where, "(");
	 chars_printed++;
       }
       while(PAIR_TAG <= tag && tag <= QUAD_TAG) {
	 a = ENTVAL(ee);
	 for(i = PAIR_TAG; i <= tag; i++) {
	   chars_printed += 1 +
	     print_ent(a[i - PAIR_TAG], where, 0, 0, 
		       max_chars - chars_printed, suppress);
	   if(*suppress) goto out;
	   gprintf(where, ",");
	 }
	 ee = a[i - PAIR_TAG];
	 tag = BASICTAG(ee);
       }
       e = ee;
       if(!in_pair) parens++;
       in_pair = 1;
       goto again;
      }

    case BOX_TAG:
      {char str[20];
       sprintf(str, "%ld", VAL(e));
       gprintf(where, "(nonsharedBox %s)", str);
       chars_printed += strlen(str) + 16;
       goto out;
      }
	 
    case PLACE_TAG: 
      {char str[20];
       sprintf(str, "%ld", box_rank(e));
       gprintf(where, "(sharedBox %s)", str);
       chars_printed += strlen(str) + 12;
       goto out;
      }

    case TYPE_TAG:
      {TYPE *t;
       t = TYPEVAL(e);
       gprintf(where,"(type ");
       chars_printed += 7 + fprint_ty(where, t);
       gprintf(where, ")");
       goto out;
      }

    case RELOCATE_TAG:
      gprintf(where, "(GARBAGE)");
      chars_printed += 9;
      goto out;

    default:
      gprintf(where, "(???tag %d)", e_tag);
      goto out;
  }

  out:
    if(parens > 0) {
      if(parens > max_chars - chars_printed) {
	*suppress = TRUE;
      }
      else {
        int i;
	for(i = 0; i < parens; i++) {
	  gprintf(where, ")");
	}
	chars_printed += parens;
      }
    }

    return chars_printed;
}


/****************************************************************
 *                       LAZY_NAME                              *
 ****************************************************************
 * Copy a string describing lazy object e into array result. 	*
 *								*
 * If e is not a file, then array result should have at least	*
 * MAX_LAZY_NAME_LENGTH bytes.  				*
 *								*
 * If e is a file, then array result should have at least	*
 * MAX_FILE_NAME_LENGTH + 20 bytes.				*
 *								*
 * The return value is result.					*
 ****************************************************************/

PRIVATE SHORT FARR lazy_prim_name[] = 
{
/*			*/	0,
/* EQUAL_TMO 		*/ 	EQ_SYM,
/*  			*/	INTERNSTRING_ID,
/* LENGTH_TMO 		*/	LENGTH_ID,
/* SUBSCRIPT_TMO 	*/	SHARP_SYM,
/*  			*/	0,
/*  			*/	0,
/* MERGE_TMO 		*/	FAIRMERGE_ID,
/* SUBLIST_TMO 		*/	DBL_SHARP_SYM,
/* FULL_EVAL_TMO 	*/	FORCE_ID,
/*  			*/	0,
/* LAZY_HEAD_TMO 	*/	LAZYHEAD_ID,
/* LAZY_TAIL_TMO 	*/	LAZYTAIL_ID,
/* LAZY_LEFT_TMO 	*/	LAZYLEFT_ID,
/* LAZY_RIGHT_TMO 	*/	LAZYRIGHT_ID,
/* REVERSE_TMO 		*/	REVERSE_ID,
/* UPTO_TMO 		*/	UPTO_ID,
/* DOWNTO_TMO 		*/	DOWNTO_ID,
/* INFLOOP_TMO 		*/	EVALUATING_ID,
/* CMDMAN_TMO 		*/	CMDMAN_ID,
/* DOCMD_TMO 		*/	DOCMD_ID,
/* SCAN_FOR_TMO 	*/	SCANFORCHAR_ID,
/* PACK_TMO 		*/	PACK_ID
};

/*----------------------------------------------------------*/

char* lazy_name(char *result, ENTITY e)
{
  int tag;

  for(;;) {
    tag = BASICTAG(e);
    switch(tag) {
      case LAZY_TAG:
      case LAZY_LIST_TAG:

	{CONTROL *ctl;
	 ACTIVATION *a;
	 char *name;

	 /*-----------------------------------*
	  * Find the name of this lazy value. *
	  *-----------------------------------*/

	 ctl = CTLVAL(e);
	 if(tag == LAZY_LIST_TAG) {

	   /*-----------------------*
	    * ctl is an up-control. *
	    *-----------------------*/

	   while(ctl != NULL && (CTLKIND(ctl) >= MARK_F
		 || ctl->right.ctl == NULL)) {
	     ctl = ctl->PARENT_CONTROL;
	   }
	   if(ctl != NULL) {
	     if(!RCHILD_IS_ACT(ctl)) {
	       ctl = ctl->right.ctl;
	       while(!LCHILD_IS_ACT(ctl)) ctl = ctl->PARENT_CONTROL;
	       a = ctl->left.act;
	     }
	     else a = ctl->right.act;
	   }
	   else a = NULL;
	 }
	 else /* tag == LAZY_TAG */ {

	   /*------------------------*
	    * ctl is a down-control. *
	    *------------------------*/

	   while(!LCHILD_IS_ACT(ctl)) ctl = ctl->left.ctl;
	   a = ctl->left.act;
	 }
	 name = get_act_name(a,TRUE);

	 /*------------------*
	  * Print the value. *
	  *------------------*/

	 if(a == NULL) sprintf(result, "[]");
	 else if(tag == LAZY_TAG) {
	   sprintf(result, "(lazy %s)", name);
	 }
	 else {
	   sprintf(result, "(lazy list %s)", name);
	 }
	 return result;
       }

      case GLOBAL_INDIRECT_TAG:
	e = *ENTVAL(e);
	break;

      case GLOBAL_TAG:
	sprintf(result, "(global %s)", outer_bindings[VAL(e)].name);
	return result;

      case LAZY_PRIM_TAG:
	{ENTITY* a = ENTVAL(e);
	 int a_tag = toint(NRVAL(a[0]));

	 if(a_tag >= UNKNOWN_TMO) {
           int box_tag = TAG(a[1]);
	   char* format = (a_tag == UNKNOWN_TMO)
			   ? ((box_tag == BOX_TAG)
	                        ? "(UpNsUnknown %ld)"
			        : "(UpShUnknown %ld)")
			   : ((box_tag == BOX_TAG)
	                        ? "(PrNsUnknown %ld)"
			        : "(PrShUnknown %ld)");
	   sprintf(result, format, box_rank(a[1]));
	 }
	 else {
	   sprintf(result, "(lazy prim %s)", std_id[lazy_prim_name[a_tag]]);
	 }
	 return result;
	}

      case FAIL_TAG:
	{int exc_val = qwrap_tag(*ENTVAL(e));
	 char* exc_name = (exc_val < next_exception)
			     ? exception_data[exc_val].name : "?";
	 sprintf(result, "(fail %s)", exc_name);
	 return result;
	}

      case INDIRECT_TAG:
	if(VAL(e) == 0) {
	  /*---------*
	   * NOTHING *
	   *---------*/
	  strcpy(result, "NOTHING");
	  return result;
	}
	e = *ENTVAL(e);
	break;

      case FILE_TAG:
	{struct file_entity* fent = FILEENT_VAL(e);
	 int                 kind = fent->kind;
	 switch(kind) {
	   case STDIN_FK:
	    strcpy(result, "(stdin)");
	    return result;

	   case STDOUT_FK:
	     strcpy(result, "(stdout)");
	     return result;

	   case STDERR_FK:
	     strcpy(result, "(stderr)");
	     return result;

	   case INFILE_FK:
	     if(fent->u.file_data.name == NULL) {
	       strcpy(result, "(input file)");
	     }
	     else {
	       sprintf(result, "(infile %s+%ld)",
		       fent->u.file_data.name, fent->u.file_data.pos);
	     }
	     return result;

	   case OUTFILE_FK:
	     sprintf(result, "(outfile %s)", fent->u.file_data.name);
	     return result;

	   case NO_FILE_FK:
	     strcpy(result, "(a closed file)");
	     return result;

	   case FONT_FK:
	     sprintf(result, "(font %s)", fent->u.font_data.fontName);
	     return result;

	   default:
	     strcpy(result, "(an unknown kind of file)");
	     return result;
	 }
	}

      case PLACE_TAG:
	strcpy(result, "(a pipe)");
	return result;

      case APPEND_TAG:
	strcpy(result, "(lazy append)");
	return result;

      case TREE_TAG:
	strcpy(result, "(lazy tree)");
	return result;
	
      default:
	sprintf(result, "(unknown tag %d)", BASICTAG(e));
	return result;

    } /* end switch */

  } /* end for */
}


/****************************************************************
 *			LAZY_DOLLAR				*
 ****************************************************************
 * Return the lazy name of x, if x is lazy and in_show is true. *
 * Otherwise fail and return x.					*
 ****************************************************************/

ENTITY lazy_dollar(ENTITY x)
{
  if(in_show && is_lazy(x)) {
    char str[MAX_LAZY_NAME_LENGTH];
    lazy_name(str, x);
    return make_str(str);
  }

  failure = TEST_EX;
  return x;
}


/****************************************************************
 *			FUNCTION_NAME				*
 ****************************************************************
 * Return the name of function f.				*
 ****************************************************************/

char* function_name(ENTITY f)
{
  char *name;
  CODE_PTR pc;
  CONTINUATION *cont;
  int instr;
  LONG k;

  cont  = CONTVAL(f);

  /*-----------------------------------------------------------------------*
   * Skip over local env size byte and local environment descriptor index. *
   *-----------------------------------------------------------------------*/

  pc    = cont->program_ctr + (1 + CODED_INT_SIZE); 

  instr = *(pc++);
  k     = next_int_m(&pc);
  name  = (instr == NAME_I)  ? outer_bindings[k].name :
          (instr == SNAME_I) ? CSTRVAL(constants[k])
			     : cont->name;
  return name;
}


/****************************************************************
 *			FUN_TO_STR				*
 ****************************************************************
 * Return a string describing function f.  The string is just   *
 * a prefix of what is used in $ for functions.  See		*
 * standard.asi for the definition of $.			*
 ****************************************************************/

ENTITY fun_to_str(ENTITY f)
{
  char* fun_name     = function_name(f);
  int   fun_name_len = strlen(fun_name);
  char* str          = BAREMALLOC(fun_name_len + 12);
  ENTITY result;
  
  sprintf(str, "(function %s", fun_name);
  result = make_str(str);
  FREE(str);
  return result;
}


/****************************************************************
 *			BOXPL_TO_STR				*
 ****************************************************************
 * Return a string describing box or place x.			*
 ****************************************************************/

ENTITY boxpl_to_str(ENTITY x)
{
  char str[24];
  int tag;

  tag = TAG(x);
  if(tag == BOX_TAG) sprintf(str, "(nonsharedBox %ld)", VAL(x));
  else sprintf(str, "(sharedBox %ld)", VAL(x));
  return make_str(str);
}


/****************************************************************
 *			BOXPL_TO_STR_STDF			*
 ****************************************************************/

ENTITY boxpl_to_str_stdf(ENTITY e, TYPE *t)
{
  if(printing_rts) record_bxpl_for_print_rts(e,t);
  return boxpl_to_str(e);
}


/****************************************************************
 *			GET_ACT_NAME				*
 ****************************************************************
 * Return the name associated with activation a.  If moveout is	*
 * true, try to return the name of the outermost continuation.	*
 ****************************************************************/

char* get_act_name(ACTIVATION *a, Boolean moveout)
{
  char *name;
  CODE_PTR pc;
  LONG kk;
  CONTINUATION *c;

  name = NULL;
  if(a != NULL) {
    name = a->name;
    if(!moveout || a->continuation == NULL) {
      pc = a->program_ctr;
      if(pc != NULL) {
        if(a->kind) pc += 1 + CODED_INT_SIZE;
	if(*pc == NAME_I) {
	  pc++;
	  kk = next_int_m(&pc);
	  name = outer_bindings[kk].name;
	}
	else if(*pc == SNAME_I) {
	  pc++; 
	  kk = next_int_m(&pc);
	  name = CSTRVAL(constants[kk]);
	}
      }
    }
    else {
      c = a->continuation;
      while(c != NULL) {
	name = c->name;
	c = c->continuation;
      }
    }
    if(name == NULL) name = a->name;
  }
  if(name == NULL) name = "(null)";
  return name;
}
