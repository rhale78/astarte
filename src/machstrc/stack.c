/**********************************************************************
 * File:    machstrc/stack.c
 * Purpose: Implement the stack of the interpreter.  The stack is
 *          used to hold intermediate results of computations.
 * Author:  Karl Abrahamson
 **********************************************************************/

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

/****************************************************************
 * The stack holds temporary results, function parameters and 	*
 * function results.  It is represented by a chain of blocks,   *
 * where each block holds a few cells of the stack.		*
 *								*
 * The push and pop functions below work on the_act.stack, so   *
 * they modify the stack of the currently executing activation. *
 * the_stack is a preprocessor symbol defined to be		*
 * the_act.stack.						*
 *								*
 * Storage is managed by reference counts, so you must be 	*
 * careful about getting pointers to the stack.  When you 	*
 * do a push or pop, the top block of the stack is modified     *
 * in-place.  The correct way to set a variable s to the	*
 * current stack in such a way that it is safe to do pushes and *
 * pops on s without bothering the original stack is		*
 *								*
 *	 STACK* s = get_stack();				*
 *								*
 * which makes s a copy of the current stack.  (Only the top    *
 * block in the chain is copied in reality, but that is 	*
 * all you need to copy.)  Use bump_stack and drop_stack as	*
 * required to manage pointers without copying.			*
 *								*
 * The correct way to set the current stack to s and		*
 * simultaneously drop the reference of s is			*
 *								*
 *         make_stack(s);					*
 *								*
 ****************************************************************/

#include "../misc/misc.h"
#include "../utils/lists.h"
#include "../alloc/allocate.h"
#include "../machdata/entity.h"
#include "../machdata/except.h"
#include "../machstrc/machstrc.h"
#include "../evaluate/evaluate.h"
#include "../intrprtr/intrprtr.h"
#include "../show/prtent.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif


/********************************************************
 * 			PUSH_STACK			*
 ********************************************************
 * Push a new cell onto the stack, and return a pointer *
 * to that cell.					*
 ********************************************************/

ENTITY* push_stack(void)
{
# ifdef GCTEST
    if(the_stack->ref_cnt != 1) {
      badrc("stack", the_stack->ref_cnt, (char *)(the_stack));
    }
# endif

  if(the_stack->top != NUM_STACK_CELLS - 1) {
    return the_stack->cells + (++the_stack->top);
  }
  else return hard_push_stack();
}


/********************************************************
 * 			POP_STACK			*
 ********************************************************
 * Pop the stack, and return the entity that was	*
 * on the top before the pop.				*
 ********************************************************/

ENTITY pop_stack(void)
{
# ifdef GCTEST
    if(the_stack->ref_cnt != 1) {
      badrc("stack", the_stack->ref_cnt, (char *) the_stack);
    }
# endif

  if(the_stack->top != 0) {
    return the_stack->cells[the_stack->top--];
  }
  else return *hard_pop_stack();
}


/********************************************************
 * 			HARD_PUSH_STACK			*
 ********************************************************
 * The stack consists of chained blocks.  This function *
 * pushes a new block onto the stack, and returns a     *
 * pointer to the bottom cell in that block.		*
 ********************************************************/

ENTITY* hard_push_stack(void)
{
  register STACK* s = allocate_stack();  /* Ref from allocate_stack */
  s->prev = the_stack;			 /* Takes over the_stack's ref */
  the_stack = s;			 /* Takes over ref from s */
  the_stack->top = 0;
  return s->cells;
}


/********************************************************
 * 			HARD_POP_STACK			*
 ********************************************************
 * Remove the top block from the chain of blocks that	*
 * is the_stack.  Return a pointer to the bottom cell   *
 * of the block that was removed.			*
 *							*
 * This is a VERY DANGEROUS operation, since it returns *
 * a pointer into a structure that has just been put	*
 * back into the free space list.  In both the 		*
 * pop_stack function and the POP_STACK macro, all that *
 * is done is an immediate dereference, so it is safe   *
 * if used that way.  We export it to allow the 	*
 * macro to use it.  					*
 *							*
 * DO NOT USE THIS FUNCTION EXCEPT IN POP_STACK.	*
 ********************************************************/

ENTITY* hard_pop_stack(void)
{
  register STACK *s;
  STACK *old_stack;

  the_stack = s = (old_stack = the_stack)->prev;
  if(s == NULL) {
#   ifdef UNIX
      fprintf(stderr, "!!! stack underflow !!!\n");
#   endif
    failure = LIMIT_EX;
    return old_stack->cells;
  }

  /*------------------------------------------------------------*
   * If the block that is becoming the new top block has more 	*
   * than one reference, then we must copy it, since we need  	*
   * to be able to modify it.  This is part of a lazy copy    	*
   * that is done by get_stack.					*
   *------------------------------------------------------------*/

  if(s->ref_cnt > 1) {
    s->ref_cnt--;
    the_stack = get_stack();
  }

# ifdef DEBUG
    allocated_stacks--;
# endif

# ifdef GCTEST
    old_stack->ref_cnt = -100;
# else
    old_stack->prev = free_stacks;
    free_stacks = old_stack;
# endif

  return old_stack->cells;
}


/********************************************************
 * 			POP_THIS_STACK			*
 ********************************************************
 * Pops the stack pointed to by variable *s and returns *
 * the top of the stack before the pop.  This function  *
 * does not participate in the lazy copy done as part	*
 * of get_stack, so should be used with caution.	*
 ********************************************************/

ENTITY pop_this_stack(STACK **s)
{
  register STACK *ss;
  ENTITY result;

  if((*s)->top != 0) return (*s)->cells[(*s)->top--];
  ss = (*s)->prev;
  if(ss == NULL) return zero;
  else {
    result = (*s)->cells[0];
    *s = ss;
    return result;
  }
}


/********************************************************
 * 			GET_STACK			*
 ********************************************************
 * Return a copy of the_stack.  Only the top block	*
 * is copied.  The other blocks are copied when		*
 * necessary, with the lazy copy done in 		*
 * hard_pop_stack.					*
 ********************************************************/

STACK* get_stack(void)
{
  STACK *s, *cpys;
  register int i;
  register ENTITY *p, *q;

# ifdef DEBUG
    if(trace) trace_i(215);
# endif

  s    = the_stack;
  cpys = allocate_stack();  /* Ref from allocate_stack */
  cpys->top = s->top;
  bump_stack(cpys->prev = s->prev);
  for(p = cpys->cells, q = s->cells, i = s->top; 
      i >= 0; 
      p++,q++,i--) {
    *p = *q;
  }
  return cpys;
}


/********************************************************
 * 			MAKE_STACK			*
 ********************************************************
 * Set the_stack to s.					*
 ********************************************************/

void make_stack(STACK *s)
{
  drop_stack(the_stack); 
  the_stack = s;
  if(s->ref_cnt > 1) {
    s->ref_cnt--;
    the_stack = get_stack();
  }
}


/********************************************************
 * 			INIT_STACK			*
 ********************************************************
 * Set the_stack to a new, empty stack.			*
 ********************************************************/

void init_stack()
{
  the_stack = new_stack();
}


/********************************************************
 * 			NEW_STACK			*
 ********************************************************
 * Return a new, empty stack.				*
 ********************************************************/

STACK* new_stack()
{
  register STACK *s;

  s	      = allocate_stack();
  s->prev     = NULL;
  s->top      = 0;
  *(s->cells) = ENTU(0);
  return s;
}


#ifdef DEBUG
/********************************************************
 *			LONG_PRINT_STACK		*
 ********************************************************
 * Print stack s in long form on the trace file, 	*
 * indented n spaces.					*
 ********************************************************/

void long_print_stack(STACK *s, int n)
{
  struct stack *p;
  int i;

  indent(n); 
  fprintf(TRACE_FILE, "Stack: (%p)\n", s);
  for(p = s; p != NULL; p = p->prev) {
    indent(n); trace_i(40, toint(p->ref_cnt));
    for(i = p->top; i >= 0; i--) {
      long_print_entity(p->cells[i], n+1, 0);
    }
  }
}


/********************************************************
 *			PRINT_STACK			*
 ********************************************************
 * Print stack s in short form on the trace file.	*
 ********************************************************/

void print_stack(STACK *s)
{
  struct stack *p;
  int i;

  fprintf(TRACE_FILE, "stk:");
  for(p = s; p != NULL; p = p->prev) {
    for(i = p->top; i >= 0; i--) {
      fprintf(TRACE_FILE, "(");
      trace_print_entity(p->cells[i]);
      fprintf(TRACE_FILE, ")\n");
    }
  }
  tracenl();
}

#endif
