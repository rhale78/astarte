/******************************************************************
 * File:    generate/improve.c
 * Purpose: Code improver
 * Author:  Karl Abrahamson
 ******************************************************************/

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
 * The interpreter is quite slow, but we can keep it from being even	*
 * slower by improving the generated code somewhat.  This file 		*
 * contains a very primitive code optimizer.				*
 ************************************************************************/

#include <string.h>
#include "../misc/misc.h"
#include "../parser/tokens.h"
#include "../utils/rdwrt.h"
#include "../utils/strops.h"
#include "../utils/lists.h"
#include "../exprs/expr.h"
#include "../unify/unify.h"
#include "../error/error.h"
#include "../evaluate/instruc.h"
#include "../tables/tables.h"
#include "../parser/parser.h"
#include "../standard/stdfuns.h"
#include "../generate/generate.h"
#include "../machstrc/machstrc.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif


PRIVATE void copy_int_g(CODE_PTR *p);
PRIVATE void copy_to_g(CODE_PTR *p, CODE_PTR e, int quit_char);
PRIVATE void improve_code_out_g(CODE_PTR code, CODE_PTR *endloc);
PRIVATE void skip_to_endlet(CODE_PTR *p, CODE_PTR e, Boolean do_write);
PRIVATE CODE_PTR check_for_quick_try_g(CODE_PTR q, CODE_PTR e);

/****************************************************************
 *			PUBLIC VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			suppress_code_improvement		*
 ****************************************************************
 * When suppress_code_improvement is TRUE, do not do any code	*
 * improvement.							*
 ****************************************************************/

Boolean suppress_code_improvement = FALSE;

/****************************************************************
 *			PRIVATE VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			last_line_number_generated		*
 ****************************************************************
 * Holds last line generated.  Used to avoid generating two	*
 * LINE_I instructions for the same line. 			*
 ****************************************************************/

PRIVATE int last_line_number_generated = -1;  

/****************************************************************
 *			WRITE_LINE_INSTR			*
 ****************************************************************
 * Generate a LINE_I(n) instruction, unless n is the same as	*
 * the line number in the most recent LINE_I instruction.	*
 ****************************************************************/

PRIVATE void write_line_instr(int n)
{
  if(last_line_number_generated != n) {
    write_g(LINE_I);
    write_int_m(n,genf);
    last_line_number_generated = n;
  }
}


/*****************************************************************
 *			CODE_OUT_G				 *
 *****************************************************************
 * Output the contents of the array code, up to but not including*
 * *endloc.  Some code improvement is done, if not suppressed.   *
 * Set *endloc to code * when finished.				 *
 *								 *
 * Note: If code improvement has been suppressed, then atomicity *
 * spreading (moving END_ATOMIC_I across TRY_I and THEN_I 	 *
 * instructions) is also suppressed.  Improvement should only	 *
 * be suppressed for debugging.					 *
 *								 *
 * XREF:							 *
 *   Called in gendcl.c and in genstd.c to write code of	 *
 *   declarations.						 *
 *****************************************************************/

void code_out_g(CODE_PTR code, CODE_PTR *endloc)
{

  if(suppress_code_improvement) {
    write_code_array(code, *endloc);
  }

  else improve_code_out_g(code, endloc);
}


/*****************************************************************
 *			MOVE_CODED_INT				 *
 *****************************************************************
 * Move a three-byte integer from src to dest.  It is 		 *
 * permissible for the result ints to occupy overlapping space,	 *
 * as long as src >= dest.					 *
 ****************************************************************/

PRIVATE void move_coded_int(CODE_PTR dest, CODE_PTR src)
{
  dest[0] = src[0];
  dest[1] = src[1];
  dest[2] = src[2];
}


/*****************************************************************
 *			MOVE_ATOMIC				 *
 *****************************************************************
 * Move the END_ATOMIC_I instruction at address p across THEN_I, *
 * TRY_I, LINE_I, POP_I and END_SCOPE_I instructions if		 *
 * necessary.  							 *
 *								 *
 * Return TRUE if the END_ATOMIC_I instruction was moved, and	 *
 * FALSE if it was not moved.					 *
 *								 *
 * e is the address just after the end of the code array.	 *
 *****************************************************************/

PRIVATE Boolean move_atomic(CODE_PTR p, CODE_PTR e)
{
  CODE_PTR q = p;
  while(q+1 < e) {
    UBYTE next_inst = q[1];
    if(next_inst == THEN_I || next_inst == POP_I) *(q++) = next_inst;

    else if(next_inst == TRY_I) {
      q[0] = TRY_I;
      q[1] = q[2];
      q[2] = q[3];
      q += 3;
    }

    else if(next_inst == LINE_I) {
      q[0] = LINE_I;
      move_coded_int(q+1, q+2);
      q += 1 + CODED_INT_SIZE;
    }

    else if(next_inst == EXIT_SCOPE_I) {
      q[0] = EXIT_SCOPE_I;
      q[1] = q[2];
      q += 2;
    }

    else if(next_inst == LONG_LLABEL_I && q[3] == TRY_I) {
      int i;
      for(i = 0; i < 5; i++) q[i] = q[i+1];
      q += 5;
    }

    else break;
  }
  
  *q = END_ATOMIC_I;
  return (q != p);
}


/*****************************************************************
 *			REPLACE_HERMIT_POP			 *
 *****************************************************************
 * Replace HERMIT_I POP_I by nothing, even when there are 	 *
 * certain intervening instructions.				 *
 *								 *
 * Return TRUE if HERMIT_I POP_I got replaced or moved, and	 *
 * FALSE if they did not.					 *
 *								 *
 * p is the address of HERMIT_I in the code array, and e is	 *
 * the address just after the end of the code array.		 *
 *****************************************************************/

PRIVATE Boolean replace_hermit_pop(CODE_PTR p, CODE_PTR e)
{
  UBYTE p1 = p[1];

  if(p+1 < e && p1 == POP_I) {
    p[0] = p[1] = NOOP_I;
    return TRUE;
  }

  else if(p1 == NAME_I && p+(CODED_INT_SIZE+2) < e) {
    p[0] = NAME_I;
    move_coded_int(p+1, p+2);
    p[1 + CODED_INT_SIZE] = HERMIT_I;
    return TRUE;
  }
    
  else if(p+2 < e
	  && (p1 == END_ATOMIC_I  || p1 == END_CUT_I || p1 == END_PRESERVE_I) 
	  && p[2] == POP_I) {
    p[2] = p1;
    p[0] = p[1] = NOOP_I;
    return TRUE;
  }

  return FALSE;
}


/*****************************************************************
 *			HANDLE_PAIR_INSTRUCTION			 *
 *****************************************************************
 * p points to code that begins with PAIR_I.  Write code for     *
 * this sequence of instructions, doing replacements as		 *
 * necessary.  The following replacements are done.		 *
 *								 *
 *  1. Many PAIR_I instructions in a row are replaced by a 	 *
 *  MULTIPAIR_I.						 *
 *								 *
 *  2. PAIR_I SPLIT_I is replaced by nothing.			 *
 *								 *
 *  3. several PAIR_I instructions followed by a GOTO_I are	 *
 *  replaced by a PAIR_GOTO_I instruction.			 *
 *								 *
 *  4. several PAIR_I instructions followed by an APPLY_I are	 *
 *  replaced by a PAIR_APPLY_I instruction.			 *
 *								 *
 * Certain intervening instructions, such as LINE_I instructions,*
 * are handled.							 *
 *								 *
 * Parameter p points to the byte that holds the PAIR_I 	 *
 * instruction.  e points to the address just after the end	 *
 * of the code array.						 *
 *								 *
 * The return value is a pointer to the first instruction	 *
 * that did not get generated.					 *
 *****************************************************************/

PRIVATE CODE_PTR handle_pair_instruction(CODE_PTR p, CODE_PTR e)
{
  int ln, ex, n, c;
  ln = ex = -1;
  c = 0;
  n = 1;
  p++;

  /*-------------------------------------------------------------*
   * Count the net number n of pairs or splits. Also accumulate  *
   * LINE_I and EXIT_SCOPE_I instructions. 			 *
   *-------------------------------------------------------------*/
  
  while(p < e && n > 0) {
    c = *p;
    if(c == PAIR_I) {n++; p++;}
    else if(c == SPLIT_I) {n--; p++;}
    else if(c == LINE_I) {p++; ln = toint(next_int_m(&p));}
    else if(c == EXIT_SCOPE_I) {p++; ex = *(p++);}
    else break;
  }

  /*----------------------------------------------------*
   * Generate the line and exit scope info accumulated. *
   *----------------------------------------------------*/

  if(ln >= 0) write_line_instr(ln);
  if(ex >= 0) {
    write_g(EXIT_SCOPE_I);
    write_g(ex);
  }

  /*------------------------------------------------*
   * Generate pairing instructions, if appropriate. *
   *------------------------------------------------*/

  /*--------------------*
   * Case of a small n. *
   *--------------------*/

  if(n > 0 && n < 128) {

    /*-----------------------*
     * Case PAIR_I GOTO_I(l) *
     *-----------------------*/

    if(c == GOTO_I) {
      write_g(PAIR_GOTO_I);
      p++;
      write_g(*(p++));
      write_g(n);
    }

    /*-------------------------------------------*
     * Case LLABEL_I(l) SPLIT_I: Goto the split. *
     *-------------------------------------------*/

    else if(c == LLABEL_I && *(p+2) == SPLIT_I) {
      write_g(PAIR_GOTO_I);
      write_g(*(p+1));
      write_g(n);
    }

    /*--------------------------------------------*
     * Case PAIR_I FETCH_I(x) REV_(TAIL)_APPLY_I. *
     *--------------------------------------------*/

    else if(c == FETCH_I || c == G_FETCH_I) {
      int arg = p[1];
      int afterc = p[2];
      if(afterc == REV_APPLY_I || afterc == REV_TAIL_APPLY_I) {
	write_g(c);
	write_g(arg);
	if(afterc == REV_APPLY_I) write_g(REV_PAIR_APPLY_I);
	else                      write_g(REV_PAIR_TAIL_APPLY_I);
	write_g(n);
	p += 3;
      }
      else if(n == 1) write_g(PAIR_I);
      else {write_g(MULTIPAIR_I); write_g(n);}
    }
    else if(n == 1) {write_g(PAIR_I);}
    else {write_g(MULTIPAIR_I); write_g(n);}
  }

  /*--------------------*
   * Case of a large n. *
   *--------------------*/

  else if (n > 1) {
    while(n > 0) {
      int m = min(n, 127);
      write_g(MULTIPAIR_I);
      write_g(m);
      n -= m;
    }
  }

  return p;
}


/*****************************************************************
 *			HANDLE_LET_INSTRUCTION			 *
 *****************************************************************
 * p points to code that begins with some kind of let		 *
 * instruction.  Write code for this sequence of instructions,   *
 * doing replacements as necessary, introducing LET_AND_LEAVE_I, *
 * FINAL_LET_I and FINAL_LET_AND_LEAVE_I instructions.		 *
 *								 *
 * Parameter p points to the byte that holds the let	 	 *
 * instruction.  e points to the address just after the end	 *
 * of the code array.						 *
 *								 *
 * The return value is a pointer to the first instruction	 *
 * that did not get generated.					 *
 *****************************************************************/

PRIVATE CODE_PTR handle_let_instruction(CODE_PTR p, CODE_PTR e)
{
  CODE_PTR pp;
  int skip, line, suppress;
  UBYTE let_instr, orig_let_instr;
  char name_start = ' ';

  let_instr = orig_let_instr = p[0];

  /*-----------------------------------------------------*
   * Skip the name and type code.			 *
   * name_start is the first character of the name.	 *
   * (Follows the offset byte.) 			 *
   *							 *
   * pp points to the instruction that follows RELET_I	 *
   * etc. instruction, or to the name that follows a	 *
   * LET_I instruction.					 *
   *-----------------------------------------------------*/

  pp = p+2;
  if(orig_let_instr != RELET_I) {

    /*-------------------------------------*
     * Find out what the name starts with. *
     *-------------------------------------*/
    
    name_start = *pp;

    /*---------------------*
     * Skip over the name. *
     *---------------------*/
    
    while(pp < e && *pp != '\0') pp++;
    pp++; 				/* Skip the null character */

    /*--------------------*
     * Skip the type code *
     *--------------------*/

    skip_to_endlet(&pp, e, 0);
  }

  /*----------------------------------*
   * Skip over a HERMIT_I POP_I pair. *
   *----------------------------------*/

  skip = line = 0;
  if(pp < e && *pp == HERMIT_I && pp[1] == POP_I) {
    pp += 2;
    skip = 2;
  }

  /*--------------------------------*
   * Skip over LINE_I instructions. *
   *--------------------------------*/

  while(pp < e && *pp == LINE_I) {
    pp++;
    line = toint(next_int_m(&pp));
    skip += 1 + CODED_INT_SIZE;
  }

  /*-------------------------------------------------------*
   * Find out instruction kind to use.  Default is current *
   * value of let_instr. 				   *
   *-------------------------------------------------------*/
  
  if(pp < e && pp[1] == p[1]) {
    if     (*pp == FETCH_I) {
      let_instr = (orig_let_instr != RELET_I) 
	            ? LET_AND_LEAVE_I
	            : RELET_AND_LEAVE_I;
    }
    else if(*pp == FINAL_FETCH_I) {
      let_instr = (orig_let_instr != RELET_I) 
	            ? FINAL_LET_AND_LEAVE_I
	            : FINAL_RELET_AND_LEAVE_I;
    }
  }

  /*-------------------------------------------------------------*
   * Suppress output if a FINAL_LET_AND_LEAVE_I to an internally *
   * generated variable.  Can just do nothing instead. 		 *
   *-------------------------------------------------------------*/

  suppress = FALSE;
  if(let_instr == FINAL_LET_AND_LEAVE_I && name_start == '.') {
    suppress = TRUE;
  }

  /*---------------------------------------------------------*
   * Write the instruction, or, if suppressed, skip over it. *
   *---------------------------------------------------------*/

  if(!suppress){
    write_g(let_instr);            /* LET_I or DEAD_LET_I or
				      LET_AND_LEAVE_I or
				      FINAL_LET_AND_LEAVE_I
				      or RELET_I or RELET_AND_LEAVE_I
				      or FINAL_RELET_AND_LEAVE_I */
    p++;
    write_g(*(p++));	           /* Offset */
    if(orig_let_instr != RELET_I) {
      copy_to_g(&p, e, 0);         /* Name   */
      skip_to_endlet(&p, e, 1);    /* Type stuff */
    }
  }
  else {
    p += 2;                        /* LET_I(k) */
    if(orig_let_instr != RELET_I) {
      while(*p != '\0') p++;       /* Name */
      p++;			   /* Zero byte at end of name */
      skip_to_endlet(&p, e, 0);    /* Type stuff */
    }
  }

  /*----------------------------------------------------------*
   * Skip over extra skipped instructions, and write a LINE_I *
   * instruction if necessary, for a _LEAVE_ instruction.     *
   *----------------------------------------------------------*/

  if(   let_instr == LET_AND_LEAVE_I
     || let_instr == FINAL_LET_AND_LEAVE_I
     || let_instr == RELET_AND_LEAVE_I
     || let_instr == FINAL_RELET_AND_LEAVE_I) {
    p += skip + 2;  /* Skip over HERMIT_I, POP_I, LINE_I(n)
		       and FETCH_I(k) or FINAL_FETCH_I(k) */
    
    if(line != 0) write_line_instr(line);
  }

  return p;
}


/*****************************************************************
 *			IMPROVE_CODE_OUT_G			 *
 *****************************************************************
 * Output the contents of the array code, up to but not including*
 * *endloc.  Some code improvement is done.   			 *
 * Set *endloc to code * when finished.				 *
 *****************************************************************/

PRIVATE void improve_code_out_g(CODE_PTR code, CODE_PTR *endloc)
{
  CODE_PTR p, e;
  int inst;

# ifdef DEBUG
    if(trace_gen > 1) {
      trace_t(243, code);
      for(p = code; p < *endloc; p++) fprintf(TRACE_FILE, "%d\n", toint(*p));
    }
# endif

  /*---------------------------------------------------------------*
   * Clear memory about past LINE_I instructions, since this is a  *
   * new declaration. 						   *
   *---------------------------------------------------------------*/

  last_line_number_generated = -1;

  /*----------------------*
   * Scan the code array. *
   *----------------------*/

  e = *endloc;
  p = code;
  while(p < e) {
    inst = *p;
    switch(instinfo[inst].class) {

      /*-------------------------------------------------*
       **************** NO_PARAM_INST ********************
       *-------------------------------------------------*/

      case NO_PARAM_INST:

	/*---------------------------------------------------------*
	 * Delete NOOP_I instructions.  They might have been	   *
	 * introduced by other improvements.			   *
	 *---------------------------------------------------------*/

        if(inst == NOOP_I) {
	  p++;
	  break;
        }

	/*---------------------------------------------------------*
         * Move END_ATOMIC_I across TRY_I and THEN_I instructions. *
	 * This is done in-place, and then the instructions are	   *
	 * considered again, unless there is no THEN_I or TRY_I	   *
         * instruction after the END_ATOMIC_I.			   *
	 *---------------------------------------------------------*/

	if(inst == END_ATOMIC_I) {
	  if(move_atomic(p, e)) continue;
	  else goto do_default;
        }


        /*---------------------------------------------------------*
         * Replace NOT_I NOT_I by nothing. 			   *
         *---------------------------------------------------------*/

        if(inst == NOT_I) {
	  if(p+1 < e && *(p+1) == NOT_I) p += 2;
	  else goto do_default;
	  break;
        }

        /*---------------------------------------------------------*
         * Replace HERMIT_I POP_I by nothing, even with		   *
         * an intervening NAME_I instruction.  Replace 		   *
         * HERMIT_I END_ATOMIC_I POP_I by END_ATOMIC_I. 	   *
         * Similar for other end instructions between  		   *
         * HERMIT_I and POP_I.  If HERMIT_I got deleted, then	   *
         * reprocess.						   *
         *---------------------------------------------------------*/

        if(inst == HERMIT_I) {
	  if(replace_hermit_pop(p,e)) continue;
          else goto do_default;
	}

	/*---------------------------------------------------------*
	 * Replace NONNIL_FORCE_I SPLIT_I by SPLIT_I.		   *
	 *---------------------------------------------------------*/

        if(inst == NONNIL_FORCE_I) {
	  if(p+1 < e && *(p+1) == SPLIT_I) {
	    write_g(SPLIT_I);
	    p += 2;
	  }
	  else goto do_default;
	  break;
	}

	/*----------------------------------------------*
	 * Replace SPLIT_I SWAP_I POP_I by RIGHT_I.	*
	 * Replace SPLIT_I PAIR_I by NONNIL_FORCE_I. 	*
	 *----------------------------------------------*/

        if(inst == SPLIT_I) {
	  if(p+2 < e && p[1] == SWAP_I && p[2] == POP_I) {
	    p += 3;
	    write_g(RIGHT_I);
	    break;
	  }
	  else if(p+1 < e && p[1] == PAIR_I) {
	    p += 2;
	    write_g(NONNIL_FORCE_I);
	    break;
	  }
	  else goto do_default;
	}

	/*----------------------------------------------------------------*
	 * Replace a string of n RIGHT_I instructions by MULTI_RIGHT_I n. *
	 *----------------------------------------------------------------*/

        if(inst == RIGHT_I) {
	  int n = 1;
	  p++;
	  while(*p == RIGHT_I && p < e && n < 255) {n++; p++;}
	  if(n == 1) write_g(RIGHT_I);
	  else {
	    write_g(MULTI_RIGHT_I);
	    write_g(n);
	  }
	  break;
	}

	/*---------------------------------------------------------*
	 * Replace sequences that begin with a sequence of	   *
	 * n PAIR_I instructions:				   *
	 *						 	   *
	 * ... SPLIT_I  (let PAIR_I SPLIT_I cancel)		   *
	 * ... Fetch REV_APPLY_I (use Fetch REV_APPLY_I n)	   *
	 * ... GOTO_I   (use PAIR_GOTO_I n)			   *
	 * ...          (use MULTI_PAIR_I n)			   *
	 *							   *
	 * Handle intervening LINE_I and EXIT_SCOPE_I		   *
	 * instructions.					   *
	 *---------------------------------------------------------*/

        if(inst == PAIR_I) {
          p = handle_pair_instruction(p, e);	  
	  break;
	}

	/*----------------------*
	 * Replace		*
	 *   LLABEL_I l1	*
	 *   RETURN_I		*
	 *   LLABEL_I l2	*
	 *   RETURN_I		*
	 *   ...		*
	 * by			*
	 *   LLABEL_I l1	*
	 *   LLABEL_I l2	*
	 *   ...		*
	 *   RETURN_I		*
	 *----------------------*/

        if(inst == RETURN_I) {
	  while(p+3 < e && *(p+1) == LLABEL_I && *(p+3) == RETURN_I) {
	    write_g(LLABEL_I);
	    write_g(*(p+2));
	    p += 3;
	  }
	  write_g(RETURN_I);
	  p++;
	  break;
	}

        /*---------------------------------------------*
         * Nothing special: Just copy the instruction. *
         *---------------------------------------------*/

	goto do_default;  /* Last case: Just copy this instruction byte. */

      /*-------------------------------------------------*
       ******************** LINE_INST ********************
       *-------------------------------------------------*/

      /*----------------------------------------------------------*
       * Replace consecutive LINE_I instructions by the last one. *
       *----------------------------------------------------------*/

      case LINE_INST:
	while(p + (CODED_INT_SIZE+1) < e && p[CODED_INT_SIZE+1] == LINE_I) {
	  p += CODED_INT_SIZE + 1;
	}
	p++;  /* Skip the LINE_I instruction */
	write_line_instr(toint(next_int_m(&p)));
	break;

      /*-------------------------------------------------*
       ******************** DEF_INST *********************
       *-------------------------------------------------*/

      case DEF_INST:
	write_g(*(p++));		/* DEF_I		*/
	write_g(*(p++));  		/* Continuation local label  */
	write_g(*(p++));  		/* Offset 		*/
	copy_to_g(&p, e, 0);        	/* Name     		*/
	skip_to_endlet(&p, e, 1);       /* Type stuff           */
	write_g(*(p++));  		/* Env size byte 	*/
	break;

      /*----------------------------------------------------*
       ***************** LLABEL_ENV_INST ********************
       *----------------------------------------------------*/

      case LLABEL_ENV_INST:
	write_g(*(p++));		/* FUNCTION_I, etc. 	*/
	write_g(*(p++));		/* Local label 		*/
	skip_to_endlet(&p, e, 1);	/* Type stuff 		*/
	write_g(*(p++));		/* env size byte 	*/
	break;

#     ifdef NEVER
      case LLABEL_ENV_NUM_INST:
	write_g(*(p++));		/* the instruction etc. */
	write_g(*(p++));		/* Byte parameter	*/
	write_g(*(p++));		/* Local label		*/
	skip_to_endlet(&p, e, 1);	/* Type stuff		*/
	write_g(*(p++));		/* env size byte	*/
	break;
#     endif

      /*-------------------------------------------------*
       ****************** LET_INST ***********************
       ****************** RELET_INST *********************
       *-------------------------------------------------*/

      /*--------------------------------------------------------*
       * Introduce LET_AND_LEAVE_I, FINAL_LET_AND_LEAVE_I, etc. *
       *--------------------------------------------------------*/

      case LET_INST:
      case RELET_INST:
	p = handle_let_instruction(p, e);
	break;

      /*--------------------------------------------------------*
       ******************** BYTE_GLABEL_INST ********************
       *--------------------------------------------------------*/

      case BYTE_GLABEL_INST:
	write_g(*(p++));
	if(p+CODED_INT_SIZE < e) copy_int_g(&p);
        if(p < e) write_g(*(p++));
	break;

      /*--------------------------------------------------------*
       ******************** EXC_INST          *******************
       ******************** GLABEL_PARAM_INST *******************
       ******************** GLABEL_TYPE_INST  *******************
       ******************** LONG_NUM_PARAM_INST *****************
       *--------------------------------------------------------*/

      case EXC_INST:
      case GLABEL_PARAM_INST:
      case GLABEL_TYPE_INST:
      case LONG_NUM_PARAM_INST:
	write_g(*(p++));
	if(p+CODED_INT_SIZE < e) copy_int_g(&p);
	break;

      /*--------------------------------------------------------*
       ******************** BYTE_LLABEL_INST ********************
       *--------------------------------------------------------*/

      case BYTE_LLABEL_INST:
	if(inst == TRY_I) {
	  CODE_PTR thenloc, thenlochold;
	  UBYTE mode = p[2];
	  if((mode != TRY_F && mode != TRYEACH_F) ||
	     (thenloc = check_for_quick_try_g(p+3, e)) == NULL) {
	    goto write_three_bytes;
	  }
	  write_g(QUICK_TRY_I);
	  write_g(p[1]);
	  thenlochold = thenloc;
	  code_out_g(p+3, &thenlochold);
	  write_g(QUICK_THEN_I);
	  p = thenloc + 1;
	  break;
	}
	else goto write_three_bytes;

      /*-----------------------------------------------------*
       ****************** LONG_LLABEL_INST *******************
       *-----------------------------------------------------*/

      case LONG_LLABEL_INST:
        if(p[1] != 0) {		/* Ignore LONG_LLABEL_I(0). */
	  write_g(LONG_LLABEL_I);
	  write_g(p[1]);
	}
	p += 2;
	break;

      /*------------------------------------------------*
       ******************* REST *************************
       *------------------------------------------------*/

      case TY_PREF_INST:
      case TWO_BYTE_PARAMS_INST:
      case TWO_BYTE_TYPE_INST:
      write_three_bytes:
	write_g(*(p++));
        /* No break: continue with remaining bytes. */

      case LLABEL_INST:
      case LLABEL_PARAM_INST:
      case ENTER_INST:
      case BYTE_PARAM_INST:
      case BYTE_TYPE_INST:
      case PREF_INST:
      /*write_two_bytes:*/
	write_g(*(p++));
        /* No break: continue with last byte. */

      default:
      do_default:
	write_g(*(p++));
	break;
    }
  }
  *endloc = code;
}


/****************************************************************
 *			COPY_INT_G				*
 ****************************************************************
 * Copy a coded integer from *p to genf, updating p to point	*
 * just after the coded integer.				*
 ****************************************************************/

PRIVATE void copy_int_g(CODE_PTR *p)
{
  register int i;
  register CODE_PTR q;

  q = *p;
  for(i = 0; i < CODED_INT_SIZE; i++) write_g(*(q++));
  *p = q;
}
  

/****************************************************************
 *			SKIP_TO_ENDLET				*
 ****************************************************************
 * Skip over instructions until an END_LET_I is found.  If 	*
 * do_write is true, then write the instructions encountered, 	*
 * including the END_LET_I.  Don't scan beyond address e.  	*
 * Set *p to the address just after the END_LET_I.		*
 ****************************************************************/

PRIVATE void skip_to_endlet(CODE_PTR *p, CODE_PTR e, Boolean do_write)
{
  CODE_PTR q;
  int inst;

# ifdef DEBUG
    if(trace_gen > 1) trace_t(244);
# endif

  q = *p;
  while(q < e && *q != END_LET_I) {

#   ifdef DEBUG
      if(trace_gen > 1 && !do_write) {
	trace_t(245, toint(*q));
      }
#   endif

    inst = *q;
    switch(instinfo[inst].class) {
      case TWO_BYTE_TYPE_INST:
        if(do_write) write_g(inst);
	q++;
	/* No break - continue with next case. */

      case BYTE_TYPE_INST:
        if(do_write) write_g(*q);
	q++;
	/* No break - continue with next case. */

      case NO_TYPE_INST:
        if(do_write) write_g(*q);
	q++;
	break;
      
      case GLABEL_TYPE_INST:
        if(do_write) write_g(inst);
	q++;
	if(do_write) copy_int_g(&q);
	else {q += CODED_INT_SIZE;}
	break;

      default:
	die(33, (char *)(LONG)(inst));
    }
  }

  if(do_write) write_g(END_LET_I);
  *p = q + 1;

# ifdef DEBUG
    if(trace_gen > 1) trace_t(246);
# endif
}


/****************************************************************
 *			CHECK_FOR_QUICK_TRY_G			*
 ****************************************************************
 * Scan ahead from q to the next occurrence of a THEN_I.	*
 * if any instruction that cannot occur in the condition of a	*
 * quick try is encountered, then return NULL.  Otherwise, 	*
 * return the address of the THEN_I instruction.  If address e 	*
 * is encountered before either a THEN_I or TRY_I, then return 	*
 * NULL.							*
 ****************************************************************/

PRIVATE CODE_PTR check_for_quick_try_g(CODE_PTR q, CODE_PTR e)
{
  CODE_PTR p;
  int inst;

# ifdef DEBUG
    if(trace_gen > 1) {
      trace_t(247);
    }
# endif

  p = q;
  while(p < e && (inst = *p) != THEN_I) {

    if(!instinfo[inst].qtry_ok) {
#     ifdef DEBUG
	if(trace_gen) {
	  trace_t(248, toint(*p));
	}
#     endif

      return NULL;
    }

    switch(instinfo[inst].class) {
      case LLABEL_PARAM_INST:
      case LLABEL_INST:
      case BYTE_PARAM_INST:
      case BYTE_TYPE_INST:
      case PREF_INST:
      case RELET_INST:
     	  p += 2;
	  break;

      case BYTE_LLABEL_INST:
      case TWO_BYTE_PARAMS_INST:
      case TWO_BYTE_TYPE_INST:
      case TY_PREF_INST:
	  p += 3;
	  break;

      case LONG_NUM_PARAM_INST:
      case GLABEL_PARAM_INST:
      case GLABEL_TYPE_INST:
      case EXC_INST:
      case LINE_INST:
	  p += 1 + CODED_INT_SIZE;
	  break;

      case NO_PARAM_INST:
      case NO_TYPE_INST:
      case END_LET_INST:
	  p++;
	  break;


      case LET_INST:
	  p += 2;			/* LET_I and offset */
	  while(*p != '\0') p++;	/* name */
	  p++;                          /* the 0 at the end of name */
	  skip_to_endlet(&p, e, 0);	/* Type stuff */
	  break;
	
      default: 
#         ifdef DEBUG
	    if(trace_gen) {
	      trace_t(248, toint(*p));
	    }
#         endif

	  return NULL;
    }
  }

  if(*p != THEN_I) return NULL;

# ifdef DEBUG
    if(trace_gen > 1) {
      trace_t(249, p);
    }
# endif

  return p;
}


/****************************************************************
 *			COPY_TO_G				*
 ****************************************************************
 * Write the characters of array *p, up to and including e, 	*
 * or until quit_char is seen.  Write quit_char.  Set *p 	*
 * to point to the first character that is not printed.		*
 ****************************************************************/

PRIVATE void copy_to_g(CODE_PTR *p, CODE_PTR e, int quit_char)
{
  while(*p < e && **p != quit_char) write_g(*((*p)++));
  write_g(*((*p)++));
}
