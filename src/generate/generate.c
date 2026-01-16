/******************************************************************
 * File:    generate/generate.c
 * Purpose: generate code
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
 * This file contains general functions for generating the .aso file.	*
 * The bulk of the code is generated in genexec.c.			*
 ************************************************************************/

#include <string.h>
#ifdef USE_MALLOC_H
#  include <malloc.h>
#else
#  include <stdlib.h>
#endif
#include "../misc/misc.h"
#include "../alloc/allocate.h"
#include "../parser/tokens.h"
#include "../utils/rdwrt.h"
#include "../utils/strops.h"
#include "../utils/filename.h"
#include "../utils/hash.h"
#include "../utils/lists.h"
#include "../alloc/allocate.h"
#include "../exprs/expr.h"
#include "../classes/classes.h"
#include "../unify/unify.h"
#include "../error/error.h"
#include "../evaluate/instruc.h"
#include "../tables/tables.h"
#include "../parser/parser.h"
#include "../standard/stdfuns.h"
#include "../generate/generate.h"
#include "../generate/prim.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/********************************************************************
 * MAX_NUM_LOCAL_LABELS is the maximum allowed number of local 	    *
 * labels.  It must be 256*MAX_LOCAL_LABEL_CLASSES - 1.		    *
 ********************************************************************/

#define MAX_NUM_LOCAL_LABELS   (256*MAX_LOCAL_LABEL_CLASSES-1)

/****************************************************************
 *			PUBLIC VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			aso_file				*
 ****************************************************************
 * aso_file is the name of the file where the generated		*
 * byte-code is placed.  It is set in 				*
 * generate.c:init_generate_g.  It is NULL if code is not	*
 * being generated.  At cleanup when there were errors		*
 * (parser/compiler.c) aso_file is unlinked.			*
 ****************************************************************/

char* aso_file = NULL;	

/****************************************************************
 *			genf					*
 ****************************************************************
 * genf is the file on which to write generated code, or is	*
 * NULL if code is not being generated.  It is the open FILE*	*
 * for aso_file. 						*
 ****************************************************************/

FILE* genf = NULL;	

/****************************************************************
 *			next_llabel				*
 ****************************************************************
 * next_llabel is the next local label value to use. It is set	*
 * to 0 in gendcl.c when a declaration is started. 		*
 ****************************************************************/

int next_llabel;	

/****************************************************************
 *			glob_code_array				*
 *			glob_code_array_size			*
 ****************************************************************
 * glob_code_array points to an array that is used to generate	*
 * the code that is the preamble to a declaration (before	*
 * the executable code).  glob_code_array_size tells the	*
 * physical size of glob_code_array.  (When more bytes are	*
 * needed, glob_code_array is reallocated, so it can be		*
 * expected to change.) 					*
 ****************************************************************/

CODE_PTR glob_code_array;
SIZE_T 	 glob_code_array_size;  

/****************************************************************
 *			glob_genloc				*
 *			glob_genlast				*
 ****************************************************************
 * glob_genloc points to the next byte in glob_code_array	*
 * where generated code should be placed.  glob_genlast points	*
 * to the byte just after the last byte of array 		*
 * glob_code_array. 						*
 ****************************************************************/

CODE_PTR glob_genloc;
CODE_PTR glob_genlast;	  

/****************************************************************
 *			exec_code_array				*
 *			exec_code_array_size			*
 ****************************************************************
 * exec_code_array and exec_code_array_size are similar to	*
 * glob_code_array and glob_code_array_size, but are used	*
 * to hold the executable code of a declaration. 		*
 ****************************************************************/

CODE_PTR exec_code_array;
SIZE_T 	 exec_code_array_size;  
	
/****************************************************************
 *			exec_genloc				*
 *			exec_genlast				*
 ****************************************************************
 * exec_genloc and exec_genlast are similar to glob_genloc and	*
 * glob_genlast, but are for the exec_code_array array. 	*
 ****************************************************************/

CODE_PTR exec_genloc;
CODE_PTR exec_genlast;    

/****************************************************************
 *			current_code_array			*
 *			current_code_array_size			*
 ****************************************************************
 * current_code_array points to the code array that is		*
 * currently being generated into.  It is either the same as    *
 * exec_code_array or glob_code_array.  current_code_array_size *
 * is the physical size of current_code_array.			*
 ****************************************************************/

CODE_PTR current_code_array;
SIZE_T   current_code_array_size;

/****************************************************************
 *			genloc					*
 *			genlast					*
 ****************************************************************
 * genloc points to either glob_genloc or exec_genloc.  The	*
 * variable pointed to is the one that controls current code	*
 * generation.  For example, if genloc points to glob_genloc,	*
 * then code is currently being generated in glob_code_array. 	*
 *								*
 * genloc can be NULL, to indicate that code is currently being *
 * generated directly into file genf, rather than into a code   *
 * array.							*
 *								*
 * genlast points to either glob_genlast or exec_genlast.	*
 ****************************************************************/

CODE_PTR *genloc;
CODE_PTR *genlast;	  

/****************************************************************
 *			last_label_loc				*
 ****************************************************************
 * last_label_loc points to the byte just after the most	*
 * recently generated LLABEL_I instruction, or is NULL if no	*
 * LLABEL_I instruction has yet been generated in the current	*
 * declaration.  It is used to prevent generating two LLABEL_I	*
 * instructions in a row.  Instead, we just use the same	* 
 * label for both purposes. 					*
 ****************************************************************/

CODE_PTR last_label_loc;  

/****************************************************************
 *			envloc_st				*
 ****************************************************************
 * Stack envloc_st holds, on its top, the next environment	*
 * offset to be used in the current environment.  For example,	*
 * if the top of envloc_st is 3, then the next let will be to	*
 * offset 3.  This stack is pushed onto when a new environment	*
 * is entered, and popped when that environment is left. 	*
 ****************************************************************/

INT_STACK envloc_st = NULL; 

/****************************************************************
 *			numlocals_st				*
 ****************************************************************
 * The top of this stack holds the number of cells that are	*
 * needed in the current environment. A new cell is pushed onto *
 * this stack is pushed when a new environment is entered, and	*
 * popped when the environment is left. 			*
 ****************************************************************/

INT_STACK numlocals_st = NULL; 

/****************************************************************
 *			env_id_st				*
 ****************************************************************
 * The top of env_id_st is a list of the EXPR nodes of the	*
 * identifiers that are currently in the local environment.  It	*
 * is used so that, when an identifier goes out of scope, its	*
 * OFFSET and bound fields can be cleared, indicating that it	*
 * is not currently bound. 					*
 ****************************************************************/

LIST_STACK env_id_st = NULL;  

/****************************************************************
 *			gen_code				*
 *			main_gen_code				*
 ****************************************************************
 * gen_code is true when code should be generated, and is false *
 * when code generation should be suppressed (such as when 	*
 * importing a package).					*
 *								*
 * main_gen_code is true if code should be generated while 	*
 * reading the main package.					*
 ****************************************************************/

Boolean gen_code = FALSE;
Boolean main_gen_code = FALSE;


/****************************************************************
 *			PRIVATE VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			next_glabel				*
 ****************************************************************
 * next_glabel is the next global label number to use. 		*
 ****************************************************************/

PRIVATE int next_glabel;		


/****************************************************************
 *			ENV_SIZE				*
 ****************************************************************
 * Include the env_size array from the interpreter.  It is 	*
 * needed in order to decide how large environment bytes should *
 * be.								*
 ****************************************************************/

#include "../machstrc/envsize.c"


/*==============================================================
		CODE_ARRAY_HANDLING
 ===============================================================*/

/****************************************************************
 *			FINGER_CODE_ARRAY			*
 ****************************************************************
 * Return the offset of the next instruction to be generated    *
 * in the current code array.					*
 ****************************************************************/

package_index finger_code_array(void)
{
  return *genloc - current_code_array;
}


/****************************************************************
 *			RETRACT_CODE_TO				*
 ****************************************************************
 * Retract all code generated since finger was obtained by	*
 * finger_code_array.						*
 ****************************************************************/

void retract_code_to(package_index finger)
{
  *genloc = current_code_array + finger;
}


/****************************************************************
 *			INIT_GLOBAL_CODE_ARRAY			*
 *			SELECT_GLOBAL_CODE_ARRAY		*
 ****************************************************************
 * init_global_code_array prepares global_code_array for	*
 * code generation.						*
 *								*
 * select_global_code_array begins generating in 		*
 * global_code_array.  You must do init_global_code_array	*
 * before generating in it.					*
 ****************************************************************/

void init_global_code_array(void)
{
  glob_genloc  = glob_code_array;
  glob_genlast = glob_code_array + glob_code_array_size;
}

/*-----------------------------------------------------------*/

void select_global_code_array(void)
{
# ifdef DEBUG
    if(trace_gen) trace_t(232);
# endif

  current_code_array      = glob_code_array;
  current_code_array_size = glob_code_array_size;
  genloc                  = &glob_genloc;
  genlast                 = &glob_genlast;
}


/****************************************************************
 *			INIT_EXEC_CODE_ARRAY			*
 *			SELECT_EXEC_CODE_ARRAY			*
 ****************************************************************
 * init_exec_code_array prepares exec_code_array for		*
 * code generation.						*
 *								*
 * select_exec_code_array begins generating in exec_code_array.	*
 * You must call init_exec_code_array before selecting this 	*
 * array.							*
 ****************************************************************/

void init_exec_code_array(void)
{
  exec_genloc  = exec_code_array;
  exec_genlast = exec_code_array + exec_code_array_size;
}

/*------------------------------------------------------------*/

void select_exec_code_array(void)
{
# ifdef DEBUG
    if(trace_gen) trace_t(233);
# endif

  current_code_array      = exec_code_array;
  current_code_array_size = exec_code_array_size;
  genloc                  = &exec_genloc;
  genlast                 = &exec_genlast;
}


/****************************************************************
 *			REALLOCATE_CODE_ARRAY			*
 ****************************************************************
 * Reallocate the code array determined by genloc and genlast.	*
 * Update appropriate pointers to reflect reallocation.		*
 ****************************************************************/

PRIVATE void reallocate_code_array_help(CODE_PTR *code_array, 
				       SIZE_T *code_array_size,
				       CODE_PTR *this_genloc, 
				       CODE_PTR *this_genlast)
{
  SIZE_T new_size = 2 * (*code_array_size);
  CODE_PTR new_code_array = (CODE_PTR) 
                     reallocate((char *) *code_array,
				*code_array_size,
				new_size, TRUE);

  *code_array_size        = new_size;
  *this_genloc            = new_code_array + (*this_genloc - *code_array);
  *this_genlast           = new_code_array + *code_array_size;
  *code_array             = new_code_array;
  last_label_loc	  = new_code_array + 
			    (last_label_loc - current_code_array);
  current_code_array      = new_code_array;
  current_code_array_size = new_size;
}

/*---------------------------------------------------------*/

PRIVATE void reallocate_code_array(void)
{
  if(genloc == &exec_genloc) {
    reallocate_code_array_help(&exec_code_array, &exec_code_array_size,
			       &exec_genloc, &exec_genlast);
  }
  else if(genloc == &glob_genloc) {
    reallocate_code_array_help(&glob_code_array, &glob_code_array_size,
			       &glob_genloc, &glob_genlast);
  }
  else die(30);
}


/****************************************************************
 *			WRITE_CODE_ARRAY			*
 ****************************************************************
 * Write the contents of code array code, up to but not 	*
 * including address endaddr.					*
 ****************************************************************/

void write_code_array(CODE_PTR code, CODE_PTR endaddr)
{
  CODE_PTR p;
  for(p = code; p < endaddr; p++) write_g(*p);
}


/****************************************************************
 *			WRITE_GLOBAL_CODE_ARRAY			*
 ****************************************************************
 * Write the contents of the global code array.			*
 ****************************************************************/

void write_global_code_array(void)
{
  write_code_array(glob_code_array, glob_genloc);
}


/*==============================================================
		SINGLE INSTRUCTION GENERATION
 ===============================================================*/

/****************************************************************
 *			GEN_G					*
 ****************************************************************
 * Generate one-byte instruction i.  If genloc != NULL, then	*
 * put i at genloc, advance genloc, and return the offset from  *
 * the start of the current code array where i was put.  If	*
 * genloc is NULL, then write i to the code file, and return 0.	*
 ****************************************************************/

package_index gen_g(int i)
{
  if(genloc == NULL) {
    write_g(i);
    return 0;
  }

# ifdef DEBUG
    if(trace_gen) {
      trace_t(196, i, *genloc);
    }
# endif

  if(*genloc >= *genlast) {
    reallocate_code_array();
  }

  {register CODE_PTR star_genloc = *genloc;
   register package_index result = star_genloc - current_code_array;
   *star_genloc = i;
   (*genloc)++;
   return result;
  }
}


/****************************************************************
 *			WRITE_G					*
 ****************************************************************
 * Write byte n to the code file genf, provided we are		*
 * currently generating code.					*
 ****************************************************************/

void write_g(int n)
{
  if(gen_code) {
#   ifdef DEBUG
      if(trace_gen) {trace_t(197, n);}
#   endif
    putc(n, genf);
  }
}


/************************************************************************
 *			GENP_G					        *
 ************************************************************************
 * Generate parameterized instruction i, where i is one byte, and	*
 * param is a three-byte integer.  If genloc is not NULL, then		*
 * generate to genloc, and return the offset from the current code 	*
 * array where the three-byte  parameter starts.			*
 * If genloc is NULL, write directly to the code file and return 0.	*
 ************************************************************************/

package_index genP_g(int i, LONG param)
{
  register CODE_PTR loc;
  register package_index instr_index;

  if(genloc == NULL) {
    write_g(i);
    write_int_m(param, genf);
    return 0;
  }
  else {
    instr_index = gen_g(i);
    loc = *genloc;
    if(loc + CODED_INT_SIZE >= *genlast) {
      reallocate_code_array();
    }
    gen_int_m(genloc, param);
    return instr_index + 1;
  }
}


/****************************************************************
 *			GEN_STR_G				*
 ****************************************************************
 * Generate string s, including the null character at the end.  *
 * Put it at genloc if genloc is not NULL, and			*
 * write it to the code file if genloc = NULL.			*
 ****************************************************************/

void gen_str_g(char *s)
{
  SIZE_T len;

# ifdef DEBUG
    if(trace_gen) {
      trace_t(198, s);
    }
# endif

  if(genloc == NULL) {
    fprintf(genf, "%s", s);
    write_g(0);
    return;
  }

  len = strlen(s) + 1;
  if(*genloc + len >= *genlast) {
    reallocate_code_array();
  }

  strcpy((char *) *genloc, s);
  *genloc += len;
}


/****************************************************************
 *			LABEL_G					*
 *			GET_LABEL_G				*
 *			GEN_LONG_LABEL_G			*
 *			BEGIN_LABELED_INSTR_G			*
 ****************************************************************
 * begin_labeled_instr_g() generates LONG_LLABEL_I(0), and 	*
 * returns the offset from the start of the current code array  *
 * of the byte  occupied by the 0.				*
 *								*
 * get_label_g() returns the next available local label, which  *
 * might be larger than 255. 					*
 *								*
 * gen_long_label_g(l) generates LONG_LLABEL_I(l_high), where	*
 * l_high is the high-order byte of l.				*
 *								*
 * label_g(high,low) gets the next available local label, and   *
 * generates LLABEL_I(l), where l is the low byte of that	*
 * local label.  It also stores l into the current code array   *
 * at offset low.  The high order byte of the label is stored   *
 * at offset high from the start of the current code array.	*
 * If high and/or low are 0, no store is done.			*
 * label_g returns the chosen label.				*
 *								*
 * Note: if there is already a local label for the current	*
 * location, then that local label is returned, and no LLABEL_I	*
 * instruction is generated.					*
 ****************************************************************/

int get_label_g()
{
  if(next_llabel >= MAX_NUM_LOCAL_LABELS) {
    semantic_error(LABEL_ERR, 0);
    return 0;
  }
  return next_llabel++;
}

/*-----------------------------------------------------------------*/

void gen_long_label_g(int l)
{
  if(l > 0xff) {
    gen_g(LONG_LLABEL_I);
    gen_g(l >> 8);
  }
}

/*-----------------------------------------------------------------*/

package_index begin_labeled_instr_g(void)
{
  gen_g(LONG_LLABEL_I);
  return gen_g(0);
}

/*-----------------------------------------------------------------*/

int label_g(package_index high, package_index low)
{
  register int l, high_byte, low_byte;

  /*------------------------------------------------------------*
   * If the most recently generated LLABEL_I instruction is the *
   * immediately prior instruction, then we can just use the    *
   * label generated in that instruction.			*
   *------------------------------------------------------------*/

  if(last_label_loc == *genloc) {
#    ifdef DEBUG
       if(trace_gen) {
	 trace_t(199, last_label_loc, *genloc);
       }
#    endif
     l = next_llabel - 1;
     high_byte = l >> 8;
     low_byte = l & 0xff;
  }

  /*--------------------------------------------------------------------*
   * Otherwise, get a new label, and generate the LLABEL_I instruction. *
   * Record this instruction in last_label_loc. 			*
   *--------------------------------------------------------------------*/

  else {
    l = get_label_g();
    high_byte = l >> 8;
    low_byte = l & 0xff;
    if(high_byte != 0) {
      gen_g(LONG_LLABEL_I);
      gen_g(high_byte);
    }
    gen_g(LLABEL_I); 
    gen_g(low_byte);
    last_label_loc = *genloc;
  }

  if(low != 0) current_code_array[low] = low_byte;
  if(high != 0) current_code_array[high] = high_byte;
  return l;
}


/****************************************************************
 *			GLABEL_G				*
 ****************************************************************
 * Get a new global label, and generate a GLABEL_I instruction  *
 * for it.  Return the number of the label generated.		*
 ****************************************************************/

int glabel_g()
{
# ifdef DEBUG
    if(trace_gen) {
      trace_t(200, next_glabel);
    }
# endif

  /*-----------------------------------------------------*
   * Write either a short or long label dcl instruction. *
   *-----------------------------------------------------*/

  if(next_glabel < 256) {
    write_g(LABEL_DCL_I); write_g(next_glabel);
  }
  else {
    write_g(LONG_LABEL_DCL_I);
    write_int_m(next_glabel, genf);
  }
  return next_glabel++;
}


/****************************************************************
 *			GEN_LET_G				*
 ****************************************************************
 * Generate a LET_I instruction to bind id to the top of the    *
 * stack.  							*
 *								*
 * Set the OFFSET field of id to the correct value, according   *
 * to the offset where id is placed. Return one larger than the *
 * offset where id is stored.					*
 ****************************************************************/

int gen_let_g(EXPR *id)
{
  int i;

  gen_g(LET_I);
  i = incr_env_g();
  gen_g(i-1);
  gen_str_g(name_tail(id->STR));
  generate_type_g(id->ty, 0);
  gen_g(END_LET_I);
  id->OFFSET = i;
  note_id_g(id);

# ifdef DEBUG
    if(trace_locals) {
      trace_t(201, id->STR, i-1);
    }
# endif

  return i;
}


/****************************************************************
 *			GEN_NAME_INSTR_G			*
 ****************************************************************
 * Generate either a NAME_I or SNAME_I instruction for name.    *
 * Parameter kind tells which instruction to generate.		*
 * If kind is STRING_DCL_I, then generate an SNAME_I		*
 * instruction.  Otherwise, generate a NAME_I instruction.	*
 *								*
 * Note: if no declaration of kind 'kind' has been done for     *
 * this name, then one will done here.				*
 ****************************************************************/

void gen_name_instr_g(int kind, char *name)
{
  int instr_index, instr;

  if(kind == STRING_DCL_I) {
    name        = name_tail(name);
    instr       = SNAME_I;
    instr_index = string_const_loc(name, STRING_DCL_I);
  }
  else {
    instr       = NAME_I;
    instr_index = id_loc(name, kind);
  }

  /*-------------------------------------------------------------------*
   * Generate the instruction by generating an appropriate declaration *
   * of name, and refering to the index of that declaration in the     *
   * NAME_I or SNAME_I instruction.				       *
   *-------------------------------------------------------------------*/

  genP_g(instr, instr_index);
}


/****************************************************************
 *			NOTE_ID_G				*
 ****************************************************************
 * Push id onto the stack of ids in the current environment.    *
 ****************************************************************/

void note_id_g(EXPR *id)
{
  EXPR *ssid;

  ssid = skip_sames(id);
  push_expr(env_id_st->head.list, ssid);

# ifdef DEBUG
    if(trace_gen > 1) {
      LIST *p;
      EXPR *ph;
      trace_t(202);
      for(p = env_id_st->head.list; p != NIL; p = p->tail) {
	ph = p->head.expr;
	trace_t(203, ph->STR, toint(ph->OFFSET) - 1, ph);
      }
      tracenl();
    }
# endif
}


/*==============================================================
			ENVIRONMENT HANDLING
 ===============================================================*/

/****************************************************************
 *			SET_OFFSETS_G				*
 ****************************************************************
 * Set the offsets of the identifiers in lists a and b to the 	*
 * next available positions in the environment.	 This is used	*
 * when a and b are identifier lists stored at an OPEN_E node,	*
 * to cause the same offset to be used for each copy of an	*
 * identifier.							*
 *								*
 * Normally, identifiers at corresponding positions in a and b	*
 * are given corresponding offsets.  If one of the lists starts	*
 * with a special identifier indicating a universal list, then	*
 * only the identifiers in the other list are set.		*
 ****************************************************************/

PRIVATE void set_offsets1_g(EXPR_LIST *a)
{
  LIST *p;
  EXPR *e;

  /*---------------------------------------------*
   * Ignore a list that matches all identifiers. *
   *---------------------------------------------*/

  if(a->head.expr->STR[0] == 1) return;

  /*----------------------------------------------*
   * Assign offsets to the identifiers in list a. *
   *----------------------------------------------*/

  for(p = a; p != NIL; p = p->tail) {
    e = p->head.expr;
    if(e->OFFSET == 0) e->OFFSET = incr_env_g();
    note_id_g(e);
  }
}

/*-----------------------------------------------------------------*/

void set_offsets_g(EXPR_LIST *a, EXPR_LIST *b)
{
  LIST *p, *q;
  short int *poff, *qoff;

  if(a == NIL || b == NIL) return;

  /*------------------------------------------------------------*
   * If either a or b is a universal list, just set the offsets *
   * in the other one.						*
   *------------------------------------------------------------*/

  if     (a->head.expr->STR[0] == 1) set_offsets1_g(b);
  else if(b->head.expr->STR[0] == 1) set_offsets1_g(a);
  else {
    for(p = a, q = b; p != NIL; p = p->tail, q = q->tail) {

      /*---------------------------------------------------------*
       * Check the current offsets.  Sometimes, one has been set *
       * already, and we need to honor its choice.  The offset   *
       * is already set if it is nonnegative.			 *
       *---------------------------------------------------------*/

      poff = &(p->head.expr->OFFSET);
      qoff = &(q->head.expr->OFFSET);
      if(*poff > 0) {
	if(*qoff > 0 && *qoff != *poff) die(31);
	*qoff = *poff;
      }
      else if(*qoff > 0) *poff = *qoff;

      /*----------------------------------------------------------*
       * If neither offset is already set, get the next available *
       * offset, and use it for both identifiers.		  *
       *----------------------------------------------------------*/

      else *poff = *qoff = incr_env_g();
      note_id_g(p->head.expr);
      note_id_g(q->head.expr);
    }
  }
}
 

/****************************************************************
 *			UN_ENV_SIZE_G				*
 ****************************************************************
 * Return the smallest i such that env_size[i] >= n.  Force to  *
 * 1 if env_size[0] >= n, the current scope is nonempty and 	*
 * local is true.  local should be true for a local environment *
 * and false for a global environment.				*
 *								*
 * If n is too large, so that no environment size is large 	*
 * enough, then return 0, and report an error.			*
 ****************************************************************/

int un_env_size_g(int n, Boolean local)
{
  register int i;

  for(i = 0; i < ENV_SIZE_SIZE && env_size[i] < n; i++);

  if(i == ENV_SIZE_SIZE) {
    semantic_error(LOCAL_ENV_TOO_LARGE_ERR, 0);
    return 0;
  }

  if(local && i == 0 && scopes[current_scope].nonempty) return 1;

  return i;
}


/****************************************************************
 *			INCR_ENV_G				*
 ****************************************************************
 * Add a location to the current environment. Return one larger *
 * than the number of the new location.				*
 ****************************************************************/

int incr_env_g()
{
  LONG *numlocals_top, *envloc_top;
  int envtop;

  /*-------------------------------------------------------------*
   * The offset to use next is on the top of envloc_st.  We must *
   * increment that, if it is not so large that environments of  *
   * that size are not supported.				 *
   *-------------------------------------------------------------*/

  envloc_top    = &(envloc_st->head.i);
  numlocals_top = &(numlocals_st->head.i);
  if(*envloc_top < ENV_MAX) {
    (*envloc_top)++;
    envtop = toint(*envloc_top);

    /*-----------------------------------------------------------*
     * Keep track of the largest value that has occurred on the  *
     * top of envloc_st, on the top of numlocals_st.		 *
     *-----------------------------------------------------------*/

    if(*numlocals_top < envtop) *numlocals_top = envtop;

#   ifdef DEBUG
      if(trace_gen > 1) {
	trace_t(211, envtop - 1);
      }
#   endif

    return envtop;
  }

  /*------------------------------------------*
   * If too many locals are needed, complain. *
   *------------------------------------------*/

  semantic_error(LOCAL_IDS_ERR, 0);
  return 0;
}


/****************************************************************
 *			POP_BINDINGS_G				*
 ****************************************************************
 * Unbind all identifiers that are beyond envmark in the	*
 * current environment.						*
 ****************************************************************/

PRIVATE void pop_bindings_g(int envmark)
{
  EXPR_LIST *p;
  EXPR *e;

  for(p = env_id_st->head.list; p != NIL; p = p->tail) {
    e = p->head.expr;
    if(e->OFFSET > envmark) {
      e->OFFSET = 0;
      e->bound = 0;
    }
    else break;
  }
  SET_LIST(env_id_st->head.list, p);

# ifdef DEBUG
    if(trace_gen > 1) {
      trace_t(214);
      for(p = env_id_st->head.list; p != NIL; p = p->tail) {
	e = p->head.expr;
	trace_t(203, e->STR, toint(e->OFFSET) - 1, e);
      }
      tracenl();
    }
# endif

}


/****************************************************************
 *			TRUE_PUSH_SCOPE_G			*
 ****************************************************************
 * Push information on a new environment onto the environment   *
 * stacks.  This is used when entering a scope that has an	*
 * environment node associated with it.				*
 *								*
 * The new scope is not that of an irregular function, so	*
 * increment irregular_gen.					*
 ****************************************************************/

void true_push_scope_g()
{
  push_int(envloc_st, 0);
  push_int(numlocals_st, 0);
  push_list(env_id_st, NIL);
  irregular_gen++;
}


/****************************************************************
 *			TRUE_POP_SCOPE_G			*
 ****************************************************************
 * Pop a scope that has an environment node associated with it. *
 * This undoes the action of true_push_scope_g.			*
 *								*
 * You should follow this with an update of current_scope.	*
 ****************************************************************/

void true_pop_scope_g()
{
  pop(&envloc_st);
  pop(&numlocals_st);
  pop_bindings_g(-1);
  pop(&env_id_st);
  irregular_gen--;
}


/****************************************************************
 *			POP_ENV_SCOPE_G				*
 ****************************************************************
 * Pop the environment description back to envmark, but		*
 * do not generate an EXIT_SCOPE_I instruction.			*
 ****************************************************************/

void pop_env_scope_g(int envmark)
{
# ifdef DEBUG
    if(trace_gen > 1) {
      trace_t(212, envmark - 1);
    }
# endif

  pop_bindings_g(envmark);
  envloc_st->head.i = envmark;
}


/****************************************************************
 *			POP_SCOPE_G				*
 ****************************************************************
 * Pop the scope back to envmark, and generate an EXIT_SCOPE_I  *
 * instruction to bring the scope back that far.		*
 ****************************************************************/

void pop_scope_g(int envmark)
{
  LONG *envloc_top;

  envloc_top = &(envloc_st->head.i);
  if(*envloc_top != envmark) {
#   ifdef DEBUG
      if(trace_gen > 1) {
        trace_t(213, *envloc_top, envmark);
      }
#   endif
    gen_g(EXIT_SCOPE_I);
    gen_g(envmark);
  }
  pop_env_scope_g(envmark);
}


/*==============================================================
		INITIALIZATION AND FINALIZATION
 ===============================================================*/

/****************************************************************
 *			GENERATE_HEADER_G			*
 ****************************************************************
 * Generate the introductory part of an aso file, including the *
 * package name(s).  The introductory part has the form		*
 *								*
 *    @(#)Astarte byte code version v0pname00			*
 *								*
 * if this is a one-file package whose package name is pname,   *
 * and has the form						*
 *								*
 *    @(#)Astarte byte code version v0intname0impname0		*
 *								*
 * if this is a two-file package whose interface package has	*
 * name intname and whose implementation package has name	*
 * impname.							*
 *								*
 * The 0 above indicates a byte containing a null character,	*
 * '\0'.  The v indicates the byte-code version number.		*
 ****************************************************************/

PRIVATE void generate_header_g(void)
{
# ifdef DEBUG
    if(trace_gen) {
      trace_t(215, nonnull(current_package_name), 
	      nonnull(main_package_imp_name));
    }
# endif

  fprintf(genf, "@(#)Astarte byte code version %d%c%s%c", 
	  BYTE_CODE_VERSION, 0, current_package_name, 0);
  if(main_package_imp_name != NULL) {
    fprintf(genf, "%s", main_package_imp_name);
  }
  write_g(0);
}


/****************************************************************
 *			END_PACKAGE_G				*
 ****************************************************************
 * Generate the trailer, consisting of a STOP_PACKAGE_I		*
 * instruction and a newline.					*
 ****************************************************************/

void end_package_g(void)
{
  write_g(STOP_PACKAGE_I);
  write_g('\n');
}


/****************************************************************
 *			INIT_GENERATE_G				*
 ****************************************************************
 * Prepare to generate code.  file_name is the name of the      *
 * file being compiled.  The things done here are		*
 *								*
 *  Create code arrays for generating code.			*
 *								*
 *  Open the .aso file.						*
 *								*
 *  Write the header into the .aso file.			*
 *								*
 *  Generate standard functions into the .aso file, if this	*
 *    is the standard package (standard.asi).			*
 *								*
 *  Initialize the global label for generation to 1, and	*
 *    clear out the generation data in the string table.	*
 ****************************************************************/

#include <signal.h>

void init_generate_g(char *file_name)
{
  char *name;

  /*---------------------------------------*
   * Indicate that we are generating code. *
   *---------------------------------------*/

  gen_code = main_gen_code = TRUE;

  /*-------------------------------------------*
   * Allocate the global and exec code arrays. *
   *-------------------------------------------*/

  glob_code_array      = (CODE_PTR ) alloc(GLOB_CODE_ARRAY_SIZE);
  exec_code_array      = (CODE_PTR ) alloc(EXEC_CODE_ARRAY_SIZE);
  glob_code_array_size = GLOB_CODE_ARRAY_SIZE;
  exec_code_array_size = EXEC_CODE_ARRAY_SIZE;

  /*-------------------------------------------------------------*
   * Open the .aso file.  It is always in the current directory, *
   * since the current directory has been set to the directory   *
   * that contains the .ast file.  So strip off any leading      *
   * directory in file_name before opening.			 *
   *-------------------------------------------------------------*/

  {int   len   = strlen(file_name);
   char* dir   = (char*) BAREMALLOC(len + 3);
   char* fname = (char*) BAREMALLOC(len + 1);
   dir_prefix(dir, fname, file_name);
   aso_file = name = aso_name(fname);

#  ifdef DEBUG
     if(trace) trace_t(216, name);
#  endif

   genf     = fopen_file(name, BINARY_WRITE_OPEN);
   if(genf == NULL) die(144, name);
   FREE(dir);
   FREE(fname);
  }

  /*------------------------------------------------------*
   * Be sure that an interrupt will unlink the .aso file. *
   *------------------------------------------------------*/

  signal(SIGINT, intrupt);

  /*-------------------------------------------------------------------*
   * Generate the package header, and initialize the next global label *
   * to be used.						       *
   *-------------------------------------------------------------------*/

  generate_header_g();
  next_glabel = 1;

  /*-----------------------------------------------------------------*
   * In the string table, each string has a data item indicating the *
   * global label where that string is declared.  If the string has  *
   * not been declared, then the data item should be NO_VALUE.       *
   * So set all data items to NO_VALUE now.			     *
   *-----------------------------------------------------------------*/

  clear_string_table_data();

  /*--------------------------------------------------------------------*
   * The standard package contains the definitions of all things that   *
   * are declared in standard/stdfuns.c.  So generate them now.	 In	*
   * any event, drop the reference to the chain of standard functions   *
   * that are used by gen_standard_funs, so that the memory can be	*
   * recovered.								*
   *--------------------------------------------------------------------*/

  if(compiling_standard_asi) gen_standard_funs(&std_fun_descr);
  SET_EXPR(std_fun_descr, NULL_E);
}
