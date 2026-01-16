/**********************************************************************
 * File:    gc/gc.c
 * Purpose: Garbage collector
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

/**************************************************************************
 *                THE GARBAGE COLLECTOR                                   *
 **************************************************************************
 *                                                                        *
 * The garbage collector manages storage of entities in the heap, strings,*
 * file records, big-int nodes, real numbers (doubles), state structures, *
 * and those controls and continuations that are directly referenced by   *
 * entities.            						  *
 *									  *
 * Initially, and after a garbage collection, get_before_gc is set        *
 * to the number of bytes to allocate until the next garbage collection,  *
 * and alloc_phase is set to 0.  Each allocation decreases get_before_gc  *
 * by the number of bytes allocated.  When get_before_gc becomes 0,       *
 * a flag (perform_gc) is set to tell evaluate to perform                 *
 * a garbage collection.  get_before_gc is restored to its former value,  *
 * and alloc_phase is set to 1.  If get_before_gc reaches 0 again before  *
 * the next garbage collection is done by evaluate, a collection is       *
 * forced.                                                                *
 *                                                                        *
 * The garbage collector can either compactify storage or not.  If called *
 * by evaluate, it compactifies storage.  If called anyplace else, it     *
 * does not compactify, since the relocation that occurs with             *
 * compactification will bother some functions.                           *
 *                                                                        *
 * The garbage collector has five phases: 				  *
 *									  *
 *   1. Marking. During the mark phase, the mark bits of accessible	  *
 *      things are set.  						  *
 *									  *
 *	An exception are entities that are located in non-garbage-	  *
 *	collected storage, such as in stacks and environments. 		  *
 *	Anything that those entities refer to is marked, but their own	  *
 *	mark bits are not set.  That way they don't need to be unmarked   *
 *	later.  							  *
 *									  *
 *	There is one exception to the exception.  Entities pointed to	  *
 *	by pointers in array rts_pents get marked (and later unmarked)    *
 *	regardless of where they are in storage.			  *
 *                                                                        *
 *   2. Collecting and Compactifying.  During this phase, garbage	  *
 *      collected storage is scanned and unmarked things are recovered.   *
 *	If compactification is called for, it is done by copying entities *
 *      to new blocks, and the new blocks are scanned again to relocate	  *
 *      pointers to their new locations.  The mark bits in the main	  *
 *      entity storage are cleared during the collect phase.		  *
 *      When compactification is called for, nonshared boxes are moved to *
 *      a contiguous range, and entities in main entity storage that refer*
 *      to boxes are made to refer to the new box numbers.		  *
 *                                                                        *
 *   3. Rebuilding.  The allocator free space lists are built.  If	  *
 *      compactification is not called for, these are built from the	  *
 *      existing entity blocks, for entity storage.  If compactification  *
 *	is called for, then the entity free space lists are null, so that *
 *      an allocation will get a new block from the free block list.      *
 *      The free block list is not rebuilt here.			  *
 *									  *
 *   4. Further unmarking and relocating.  Structures such as states, 	  *
 *      stacks, environments and continuations are unmarked.  Entities	  *
 *      that they refer to are relocated, if compactification is being	  *
 *	done.  States are rebuilt, so that boxes are relocated and	  *
 *	inaccessible boxes are deleted from states.  This requires	  *
 *      allocating from main entity storage, so must be done after	  *
 *      phase 3.				  			  *
 *									  *
 *   5. Recovering blocks.  When compactification is called for, the	  *
 *      free entity block list is built now.  It holds the blocks that    *
 *      held entities that were moved to new blocks in phase 2.  These	  *
 *      blocks could not be modified before now, since they have held	  *
 *      relocation information that was needed in phase 4.		  *
 *      								  *
 *                   IMPORTANT NOTE                                       *
 * 									  *
 * It is important that C functions that are holding entities register    *
 * their variables with the garbage collector, so that those variables    *
 * will be scanned during the mark phase, and the storage that they refer *
 * to will not be lost.  See below for details on how to do that.         *
 **************************************************************************/

#include <math.h>
#include <memory.h>
#ifdef USE_MALLOC_H
#  include <malloc.h>
#else
#  include <stdlib.h>
#endif
#include "../misc/misc.h"
#ifdef UNIX
# include <unistd.h>
#endif
#ifdef MSWIN
# include <io.h>
# include <windows.h>
# include "../win/font.h"
#endif
#include "../utils/lists.h"
#include "../utils/hash.h"
#include "../alloc/allocate.h"
#include "../machdata/entity.h"
#include "../machdata/gc.h"
#include "../intrprtr/intrprtr.h"
#include "../evaluate/evaluate.h"
#include "../rts/rts.h"
#include "../machstrc/machstrc.h"
#include "../tables/tables.h"
#include "../gc/gc.h"
#include "../show/prtent.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif


/*********************************************************************
 *			PUBLIC VARIABLES			     *
 *********************************************************************/

/****************************************************************
 *			force_file_cut				*
 ****************************************************************
 * When a nonvolatile infile is seen, it is normally given two  *
 * chances, and if not accessed by the third collection, its    *
 * value is removed from memory, forcing a reread if accessed   *
 * again.  If force_file_cut is true, then such files are	*
 * removed now, without giving them another chance.		*
 ****************************************************************/

Boolean force_file_cut = FALSE;

/****************************************************************
 *			compactifying				*
 ****************************************************************
 * compactifying is true when performing a compactifying GC.	*
 ****************************************************************/

Boolean compactifying = 0;    

/****************************************************************
 *			alloc_phase				*
 ****************************************************************
 * 0 in phase 0, 1 in phase 1.  See above. 			*
 ****************************************************************/

Boolean alloc_phase = 0;      

/****************************************************************
 *			get_before_gc				*
 *			get_before_gc_reset			*
 ****************************************************************
 * get_before_gc is the number of bytes to get before next	*
 * garbage collection 						*
 *								*
 * get_before_gc_reset is the value to reset get_before_gc to	*
 * when changing alloc_phase from 0 to 1. 			*
 ****************************************************************/

LONG get_before_gc;           
LONG get_before_gc_reset;     


/****************************************************************
 *			suppress_compactify			*
 ****************************************************************
 * suppress_compactify is nonzero if the garbage collector 	*
 * should not compactify.  It is set when a program requests to *
 * suppress compactification, and during a call to print_rts.   *
 ****************************************************************/

int suppress_compactify = 0; 

/*********************************************************************
 *			PRIVATE VARIABLES			     *
 *********************************************************************/

/****************************************************************
 *			in_gc					*
 ****************************************************************
 * in_gc is true when doing a garbage collection.  It is used   *
 * in gctag,  which is only used when testing the garbage   	*
 * collector.  It is also used to suppress a garbage collection *
 * when already doing a garbage collection.			*
 ****************************************************************/

PRIVATE Boolean in_gc = FALSE;

/****************************************************************
 *			used_bytes				*
 *			huge_binary_bytes			*
 *			free_bytes				*
 ****************************************************************
 * used_bytes is the number of bytes in use by the program.	*
 *								*
 * huge_binary_bytes is the number of bytes from used_bytes	*
 * that are in huge binary chunks.				*
 *								*
 * free_bytes is the number of bytes available (and already	*
 * allocated from the operating system).			*
 ****************************************************************/

PRIVATE LONG used_bytes;              
PRIVATE LONG huge_binary_bytes;
PRIVATE LONG free_bytes;              

/****************************************************************
 *			seen_activations			*
 ****************************************************************
 * During the mark phase, this accumulates the activations that *
 * have been marked. Only activations that are not part of a	*
 * control in seen_controls are in this list.			*
 ****************************************************************/

PRIVATE LIST* seen_activations; 

/****************************************************************
 *			seen_controls				*
 ****************************************************************
 * During the mark phase, this accumulates the controls that	*
 * have been marked.  Only controls that are not part of an	*
 * activation in seen_activations or a control in seen_controls	*
 * are in this list.						*
 ****************************************************************/

PRIVATE LIST* seen_controls;    

/****************************************************************
 *			relocation_blocks			*
 *			relocation_binary_blocks		*
 ****************************************************************
 * relocation_blocks points to the chain of blocks that hold	*
 * relocation information during compactification.  They are	*
 * blocks that need to be put in avail_blocks at the end	*
 * of garbage collection.					*
 *								*
 * relocation_binary_blocks is similar.  It holds binary blocks	*
 * that hold relocation information.				*
 ****************************************************************/

PRIVATE ENT_BLOCK*    relocation_blocks        = NULL;
PRIVATE BINARY_BLOCK* relocation_binary_blocks = NULL;

/****************************************************************
 *			gc_blocks				*
 *			gc_free_blocks				*
 *			gc_free_ptr				*
 *			gc_this_block_bytes_left		*
 ****************************************************************
 * These variables are used for allocating storage during	*
 * garbage collection.  Special arrangements need to be made 	*
 * to avoid interfering with garbage collection. 		*
 ****************************************************************/

PRIVATE GC_BLOCK* gc_blocks   		   = NULL;
PRIVATE GC_BLOCK* gc_free_blocks       	   = NULL;
PRIVATE char*     gc_free_ptr		   = NULL;
PRIVATE LONG      gc_this_block_bytes_left = 0;


/*------------------------------------------------*
 * Here are prototypes for the PRIVATE functions. *
 *------------------------------------------------*/

PRIVATE void mark_all_gc		(void);
PRIVATE void mark_activation_gc		(ACTIVATION *a, Boolean record);
PRIVATE void mark_activations_gc	(LIST *l);
PRIVATE void mark_continuation_gc	(CONTINUATION *c, Boolean full);
PRIVATE void mark_env_gc		(ENVIRONMENT *env);
PRIVATE void mark_stack_gc		(STACK *s);
PRIVATE void mark_states_gc		(LIST *s);
PRIVATE void mark_statep_list_gc        (LIST *s);
PRIVATE void mark_control_gc		(CONTROL *c, Boolean record);
PRIVATE void mark_controls_gc		(LIST *l);
PRIVATE void mark_ents_list_gc		(LIST *l);
PRIVATE void fully_mark_entity_array_gc (ENTITY* a, LONG n);
PRIVATE void mark_entp_array_gc		(ENTITY **a, int n);
PRIVATE void mark_entpp_array_gc	(ENTITY ***a, int n);
PRIVATE void mark_standard_ents_gc	(void);
PRIVATE void mark_constants_gc		(void);
PRIVATE void mark_globals_gc		(void);
PRIVATE void mark_array_gc		(ENTITY *a);
PRIVATE void unmark_gc			(void);
PRIVATE void unmark_entpp_array_gc	(ENTITY ***a, int n);
PRIVATE void unmark_activation_gc	(ACTIVATION *a);
PRIVATE void unmark_activations_gc	(LIST *l);
PRIVATE void unmark_continuation_gc	(CONTINUATION *c);
PRIVATE void relocate_stack_gc		(STACK *s);
PRIVATE void unmark_stack_gc		(STACK *s);
PRIVATE void relocate_env_gc		(ENVIRONMENT *env);
PRIVATE void unmark_env_gc		(ENVIRONMENT *env);
PRIVATE void unmark_control_gc		(CONTROL *c);
PRIVATE void collect_gc			(void);
PRIVATE void collect_entities_gc	(void);
PRIVATE void free_piece_gc		(ENTITY *r, ENTITY *q);
PRIVATE void collect_binaries_gc	(void);
PRIVATE void free_binary_piece_gc	(CHUNKPTR r, CHUNKPTR q);
PRIVATE void collect_huge_binaries_gc	(void);
PRIVATE void collect_continuations_gc   (void);
PRIVATE void collect_controls_gc	(void);
PRIVATE void collect_small_reals_gc     (void);
PRIVATE void collect_files_gc		(void);
PRIVATE void compactify_gc		(void);
PRIVATE void do_compactification_gc	(void);
PRIVATE void do_binary_compactification_gc(void);
PRIVATE void do_relocation_gc		(void);
PRIVATE void move_relocation_blocks_to_avail_gc(BINARY_BLOCK** rel_blocks);
PRIVATE void set_up_for_next_gc(void);


/********************************************************
 *			GC		                *
 ********************************************************
 * Perform garbage collection.  Do compactification if  *
 * should_compactify is true.                           *
 ********************************************************/

void gc(Boolean should_compactify)
{
  Boolean show_begin_end;

  /*------------------------------*
   * Do nothing if already in gc. *
   *------------------------------*/

  if(in_gc) {
    return;
  }

  /*--------------------------------------------*
   * Set flags for entry to garbage collection. *
   *--------------------------------------------*/

  /*------------------------------------------------------------*
   * In test mode, we don't compactify.  Also, don't compactify *
   * if compactification is suppressed.				*
   *------------------------------------------------------------*/

# ifndef GCTEST
    compactifying = (suppress_compactify) ? 0 : should_compactify;
# endif

  in_gc = TRUE;
  show_begin_end = FALSE;

# ifdef DEBUG
    if(gctrace || trace || alloctrace || gcshow || smallgctrace) {
      show_begin_end = TRUE;
      trace_i(121, compactifying);
      if(gctrace) {
	trace_i(65);
	print_block_chain((BINARY_BLOCK*) used_blocks);
	trace_i(66);
	print_block_chain(used_binary_blocks);
	trace_i(123);
	print_gcend_info();
      }
    }
# endif

  /*----------------*
   * Initialization *
   *----------------*/

  gc_free_blocks     = gc_blocks;
  gc_free_ptr        = NULL;
  gc_this_block_bytes_left = 0;
  marked_boxes       = NULL;
  free_boxsets       = NULL;
  seen_activations   = NIL;
  seen_controls      = NIL;
  seen_states        = NULL;

  /*---------------------------*
   * Phase 1.  Do the marking. *
   *---------------------------*/

  mark_all_gc();

  /*----------------------------------------------------------*
   * Phases 2 and 3. Collect the garbage and rebuild the free *
   * space chains.					      *
   *----------------------------------------------------------*/

# ifdef DEBUG
    if(gctrace) trace_i(126);
# endif

  if(compactifying) compactify_gc();
  else collect_gc();

  /*---------------------------------------------------*
   * Phase 4. Remove marks and do further relocation.  *
   *---------------------------------------------------*/

# ifdef DEBUG
    if(gctrace) trace_i(127);
# endif

  unmark_gc();

  /*-----------------------------------------------------*
   * Phase 5. Rebuild free block chains and schedule the *
   * next garbage collection. 				 *
   *-----------------------------------------------------*/

  if(compactifying) {
     move_relocation_blocks_to_avail_gc((BINARY_BLOCK**)(&relocation_blocks));
     move_relocation_blocks_to_avail_gc(&relocation_binary_blocks);

#    ifdef DEBUG
       if(gctrace > 1) {
         trace_i(272);
	 trace_i(65);
         print_block_chain((BINARY_BLOCK*) used_blocks);
	 trace_i(66);
	 print_block_chain(used_binary_blocks);
         trace_i(273);
         print_block_chain(avail_blocks);
       }
#    endif
  }
  heap_bytes = used_bytes;
  set_up_for_next_gc();

# ifdef DEBUG
    if(show_begin_end) trace_i(128, failure);
    if(gctrace) {
      trace_i(129, get_before_gc, free_bytes, used_bytes);
    }
    if(gctrace || smallgctrace) print_gcend_info();
# endif

  /*--------------------------------------------------------------*
   * End garbage collection.  Check whether the heap has grown so *
   * large that the user should be notified.			  *
   *--------------------------------------------------------------*/

  in_gc = FALSE;
  check_heap_size();
}


/*=====================================================================*
 *			SHADOWING THE C RUN TIME STACK		       *
 *=====================================================================*/

/*************************************************************************
 * C functions can have local variables that refer to garbage-collected  *
 * storage.  If there is any possibility of garbage collection occurring *
 * during the processing of such a function, then the garbage collector  *
 * needs to know about those local variables. 	Since the garbage 	 *
 * collector is called by evaluate, only functions that call evaluate    *
 * (possibly indirectly) need to register their variables with the       *
 * garbage collector.							 *
 *									 *
 * It is safe not to register a variable of type ENTITY that you know    *
 * MUST hold a small integer or other value that does not refer to any   *
 * garbage collected storage.  						 *
 *									 *
 * rts_ents, rts_pents, rts_bigints and rts_states hold information 	 *
 * about function local variables.   					 *
 *									 *
 * rts_ents points to an array of pointers to entities that are held by  *
 * C functions in local variables.                                       *
 * Do not put pointers to entities in the heap in array rts_ents.        *
 * These pointers should be addresses in the run-time stack, or there    *
 * will be trouble in unmarking things.                                  *
 *                                                                       *
 * rts_pents holds pointers to ENTITY pointers held by C functions.      *
 *                                                                       *
 * rts_bigints holds pointers to BIGINT pointers held by C functions.    *
 *                                                                       *
 * rts_states holds pointers to STATE pointers held by C functions.      *
 *									 *
 * rts_activations holds pointers to activations held by C functions.    *
 *									 *
 * rts_controls holds pointers to controls held by C functions.		 *
 *************************************************************************/

ENTITY** rts_ents;                /* Points to an array that holds addresses
				     of variables in the C run-time stack */

ENTITY*** rts_pents;		  /* Points to an array that holds addresses
				     of variables in the C run-time stack */

PRIVATE int rts_ents_size  = 0;    /* Physical size of array *rts_ents */

PRIVATE int rts_pents_size = 0;	  /* Physical size of array *rts_pents */

int num_rts_ents  = 0;	          /* Number of entities currently in
				     *rts_ents */

int num_rts_pents = 0;		  /* Number of pointers currently in
				     *rts_pents */

LIST* rts_activations = NIL;
LIST* rts_controls    = NIL;

/************************************************************************
 * List rts_states holds pointers to cells that hold states that are    *
 * being held by C functions.  These pointers should point to locations *
 * in the C run-time stack.                                             *
 ************************************************************************/

LIST* rts_states = NIL;


/****************************************************************
 *		       REALLOCATION FUNCTIONS  			*
 ****************************************************************/

PRIVATE void reallocate_rts_ents(void)
{
  int new_size = 2*rts_ents_size;
  rts_ents = (ENTITY **) reallocate((char *) rts_ents,
				    rts_ents_size * sizeof(ENTITY *),
				    new_size * sizeof(ENTITY *), TRUE);
  rts_ents_size = new_size;
}

/*----------------------------------------------------------------*/

PRIVATE void reallocate_rts_pents(void)
{
  int new_size = 2*rts_pents_size;
  rts_pents = (ENTITY ***) reallocate((char *) rts_pents,
				    rts_pents_size * sizeof(ENTITY **),
				    new_size * sizeof(ENTITY **), TRUE);
  rts_pents_size = new_size;
}


/************************************************************************
 *		REG1, REG2, REG3, REG1_PARAM, REG2_PARAM		*
 ************************************************************************
 * These functions are used to register C variables of type             *
 * ENTITY with the garbage collector.  For example, reg1(&x)            *
 * registers variable x.  reg2(&x,&y) registers x and y.  		*
 *									*
 * Functions reg1, reg2 and reg3 should be used to register 		*
 * uninitialized variables.  They set the variables to false.		*
 *									*
 * Functions reg1_param and reg2_param should be used to register	*
 * variables that already have a value.  				*
 *									*
 * These functions are separated, and the code   			*
 * expanded, for efficiency.                                            *
 *									*
 * Each of the registration values returns a value that should be used  *
 * as a parameter to macro unreg (defined in gc.h) to unregister the    *
 * variables.  So you do something like this.				*
 *									*
 *   REG_TYPE mark = reg1(&x);						*
 *   ....								*
 *   unreg(mark);							*
 ************************************************************************/

REG_TYPE reg1(ENTITY *x)
{
  if(num_rts_ents + 1 >= rts_ents_size) reallocate_rts_ents();
  *x = false_ent;
  rts_ents[num_rts_ents] = x;
  return num_rts_ents++;
}

/*----------------------------------------------------------------*/

REG_TYPE reg2(ENTITY *x, ENTITY *y)
{
  register REG_TYPE result;

  if(num_rts_ents + 2 >= rts_ents_size) reallocate_rts_ents();
  *x = false_ent;
  *y = false_ent;
  result = num_rts_ents;
  rts_ents[result]   = x;
  rts_ents[result+1] = y;
  num_rts_ents = result + 2;
  return result;
}

/*----------------------------------------------------------------*/

REG_TYPE reg3(ENTITY *x, ENTITY *y, ENTITY *z)
{
  register REG_TYPE result;

  if(num_rts_ents + 3 >= rts_ents_size) reallocate_rts_ents();
  *x = false_ent;
  *y = false_ent;
  *z = false_ent;
  result = num_rts_ents;
  rts_ents[result]   = x;
  rts_ents[result+1] = y;
  rts_ents[result+2] = z;
  num_rts_ents = result + 3;
  return result;
}

/*----------------------------------------------------------------*/

REG_TYPE reg1_param(ENTITY *x)
{
  if(num_rts_ents + 1 >= rts_ents_size) reallocate_rts_ents();
  rts_ents[num_rts_ents] = x;
  return num_rts_ents++;
}

/*----------------------------------------------------------------*/

REG_TYPE reg2_param(ENTITY *x, ENTITY *y)
{
  register REG_TYPE result;

  if(num_rts_ents + 2 >= rts_ents_size) reallocate_rts_ents();
  result = num_rts_ents;
  rts_ents[result]   = x;
  rts_ents[result+1] = y;
  num_rts_ents = result + 2;
  return result;
}


/************************************************************************
 *		REG1_PTR, REG1_PTRPARAM					*
 ************************************************************************
 * These functions are used to register C variables of type             *
 * ENTITY*.  reg1_ptr initializes the variable to NULL, and should      *
 * be used to register an uninitialized variable.  reg1_ptrparam        *
 * is used to register a variable that already has a value.             *
 ************************************************************************/

REGPTR_TYPE reg1_ptr(ENTITY **x)
{
  if(num_rts_pents + 1 >= rts_pents_size) reallocate_rts_pents();
  *x = NULL;
  rts_pents[num_rts_pents] = x;
  return num_rts_pents++;
}

/*----------------------------------------------------------------*/

REGPTR_TYPE reg1_ptrparam(ENTITY **x)
{
  if(num_rts_pents + 1 >= rts_pents_size) reallocate_rts_pents();
  rts_pents[num_rts_pents] = x;
  return num_rts_pents++;
}


/****************************************************************
 *			REG_STATE				*
 ****************************************************************
 * reg_state(&s) is used to register a variable of type STATE*  *
 * with the garbage collector.  It sets s = NULL.		*
 * 								*
 * reg_state_param(&s) is similar to reg_state(&s), but it	*
 * does not set s = NULL.  It should be used when s already has	*
 * a STATE* value.		                                *
 *								*
 * unreg_state(l) sets the state registration list back to l.   *
 * Use								*
 *    STATE_REG mark = reg_state(&s);				*
 *    ...							*
 *    unreg_state(mark);					*
 ****************************************************************/

LIST* reg_state(STATE **s)
{
  LIST *result;
  register HEAD_TYPE u;

  result = rts_states;
  *s = NULL;
  u.states = s;
  SET_LIST(rts_states, general_cons(u,rts_states,STATES_L));
  return result;
}

/*----------------------------------------------------------------*/

LIST* reg_state_param(STATE **s)
{
  LIST *result;
  register HEAD_TYPE u;

  result = rts_states;
  u.states = s;
  SET_LIST(rts_states, general_cons(u,rts_states,STATES_L));
  return result;
}


/****************************************************************
 *			REG_ACTIVATION				*
 ****************************************************************
 * reg_activation(a) is used to register an activation with the *
 * garbage collector so that the garbage collector will mark it.*
 * It returns a mark that should be passed to unreg_activation  *
 * to unregister the activation.				*
 * 								*
 * unreg_activation(mark) unregisters all activations that were *
 * registered since getting mark from reg_activation.		*
 ****************************************************************/

LIST* reg_activation(ACTIVATION *a)
{
  LIST *result;
  register HEAD_TYPE u;

  result = rts_activations;
  u.act  = a;
  SET_LIST(rts_activations, general_cons(u, rts_activations, ACT_L));
  return result;
}


/****************************************************************
 *			REG_CONTROL				*
 ****************************************************************
 * reg_control(a) is used to register a control with the        *
 * garbage collector so that the garbage collector will mark it.*
 * It returns a mark that should be passed to unreg_control     *
 * to unregister the control.					*
 * 								*
 * unreg_control(mark) unregisters all controlss that were 	*
 * registered since getting mark from reg_control.		*
 ****************************************************************/

LIST* reg_control(CONTROL *c)
{
  LIST *result;
  register HEAD_TYPE u;

  result    = rts_controls;
  u.control = c;
  SET_LIST(rts_controls, general_cons(u, rts_controls, CONTROL_L));
  return result;
}


/****************************************************************
 *			NOTE_CONTROL, FUN_CONTS			*
 ****************************************************************
 * The garbage collector needs to know about continuations that *
 * are referred to by entities with tag FUNCTION_TAG, and about *
 * controls that are referred to by entities with tags LAZY_TAG *
 * and LAZY_LIST_TAG.  						*
 *								*
 * fun_conts points to a chain of continuations that is		*
 * linked through the continuation field.  The continuations	*
 * in chain fun_conts are those held by function entities.	*
 * It is accessed directly, to add something to it.		*
 *								*
 * lazy_controls is a chain of controls in lazy entities. 	*
 * Actually, each node in lazy_controls can hold several	*
 * controls, for efficiency.					*
 *								*
 * lazy_controls_top_size tells how many of the slots in the	*
 * first node of chain lazy_controls are currently occupied.    *
 *								*
 * note_control(c) adds control c to lazy_controls.  		*
 ****************************************************************/

CONTINUATION* fun_conts = NULL;

#define CONTROL_REG_SIZE 15

struct control_reg {
  CONTROL *cells[CONTROL_REG_SIZE];
  struct control_reg *next;
};

PRIVATE struct control_reg* lazy_controls     = NULL;
PRIVATE struct control_reg* free_control_regs = NULL;
PRIVATE int 		    lazy_controls_top_size;

/*----------------------------------------------------------------*/

PRIVATE struct control_reg* allocate_control_reg(void)
{
  struct control_reg *p;

  p = free_control_regs;
  if(p != NULL) free_control_regs = p->next;
  else p = (struct control_reg *) alloc_small(sizeof(struct control_reg));
  return p;
}

/*----------------------------------------------------------------*/

PRIVATE void free_control_reg(struct control_reg *p)
{
  p->next           = free_control_regs;
  free_control_regs = p;
}


/*----------------------------------------------------------------*/

void note_control(CONTROL *c)
{
  struct control_reg *p;

  if(lazy_controls == NULL || lazy_controls_top_size == CONTROL_REG_SIZE) {
    p = allocate_control_reg();
    p->next = lazy_controls;
    lazy_controls = p;
    lazy_controls_top_size = 0;
  }
  lazy_controls->cells[lazy_controls_top_size++] = c;
  c->ref_cnt++;
}


/*=====================================================================*
 *		MARKING                                        	       *
 *=====================================================================*
 * The mark functions generally mark something and everything that is  *
 * accessible from it.  However, if something is already marked, it    *
 * does not need to be remarked, nor does anything accessible from it  *
 * need to be marked.                                                  *
 ***********************************************************************/

/********************************************************
 *			MARK_ALL_GC		        *
 ********************************************************
 * Mark everything that is accessible.                  *
 ********************************************************/

PRIVATE void mark_all_gc(void)
{
  LIST *p;

# ifdef DEBUG
    if(gctrace) trace_i(124);
# endif

  mark_activation_gc(&the_act, TRUE);
  if(fail_act.program_ctr != NULL) {
    mark_activation_gc(&fail_act, TRUE);
  }

# ifdef DEBUG
    if(gctrace > 1) trace_i(125);
# endif

  for(p = runtime_shadow_st; p != NIL; p = p->tail) {
    mark_activation_gc(p->head.act, TRUE);
  }

  mark_activations_gc(rts_activations);
  mark_controls_gc(rts_controls);
  mark_entp_array_gc(rts_ents, num_rts_ents);
  mark_entpp_array_gc(rts_pents, num_rts_pents);
  mark_statep_list_gc(rts_states);
  mark_state_gc(&execute_state);
  mark_state_gc(&initial_state);
  mark_standard_ents_gc();
  mark_constants_gc();
  mark_globals_gc();
}


/********************************************************
 *			MARK_ENTITY_GC		        *
 ********************************************************
 * Mark entity e and everything to which it refers.	*
 ********************************************************/

void mark_entity_gc(ENTITY *e)
{
# ifdef DEBUG
    if(gctrace > 1) {
      trace_i(130, e, toint(GCTAG(*e)), toint(MARKED(*e)));
      if(GCTAG(*e) == INT_TAG) fprintf(TRACE_FILE," (%ld)\n", IVAL(*e));
      else tracenl();
    }
# endif

# ifdef GCTEST
    if(GCTAG(*e) == RELOCATE_TAG) die(34);
# endif

  if(MARKED(*e)) return;
  GCMARK(e);
  mark_entity_parts_gc(e);
}


/****************************************************************
 *			MARK_INDIRECTION_PARTS	        	*
 ****************************************************************
 * Find the entity at the end of a chain of indirects.          *
 * Replace each indirection in the chain by a direct reference  *
 * to the end of the chain where appropriate, or by an indirect *
 * reference to the end of the chain.                           *
 * Return  a pointer to the end of the chain.                   *
 ****************************************************************/

PRIVATE ENTITY* mark_indirection_parts(ENTITY *e)
{
  ENTITY new_ent;
  register ENTITY* p = ENTVAL(*e);
  int marked = MARKED(*e);

  /*----------------------------*
   * Find the end of the chain. *
   *----------------------------*/

  while(GCTAG(*p) == INDIRECT_TAG && VAL(*p) != 0) p = ENTVAL(*p);

  /*----------------------------------------------------*
   * Get the replacement, and put it in *e.  Be careful *
   * to restore the original mark status of *e, since   *
   * mark_entity_parts_gc is used to mark the parts of  *
   * some things (things not in the heap) that are not  *
   * themselves marked.                                 *
   *----------------------------------------------------*/

  new_ent = indirect_replacement(p);
  GCMARK(&new_ent);
  p  = ENTVAL(*e);
  *e = new_ent;
  if(!marked) GCUNMARK(e);

  while(GCTAG(*p) == INDIRECT_TAG  && VAL(*p) != 0) {
    register ENTITY* q = ENTVAL(*p);
    *p = new_ent;
    p  = q;
  }
  return p;
}


/************************************************************
 *			MARK_ENTITY_PARTS_GC	            *
 ************************************************************
 * Mark the thing or things that *ee points to.  Don't mark *
 * *e.  Do safe substitutions that might save memory.       *
 ************************************************************/

void mark_entity_parts_gc(ENTITY *ee)
{
  int tag;
  register ENTITY* e = ee;

  for(;;) {
    tag = GCTAG(*e);

#   ifdef DEBUG
      if(gctrace > 1) trace_i(131, e, tag);
#   endif

    switch(tag) {
      case NOREF_TAG:
      case GLOBAL_TAG:
      case TYPE_TAG:
      case CSTR_TAG:
	return;

      case RELOCATE_TAG:
#       ifdef GCTEST
	  die(34);
#       endif
	return;

      case FILE_TAG:
	/*--------------------------------------------------------------*
	 * The low order bit of the mark field of a struct file_entity	*
         * value tells whether this node has been done by gc yet.	*
         * The next two bits up tell the status, as follows.    	*
         * 								*
         *    2     This node has survived two garbage collections 	*
         *          without being used.  It is probably not going	*
         *          to be used again.                           	*
         *								*
         *    1     This node has survived one garbage collection     	*
         *	    without being used.				 	*
         * 								*
         *    0     This node has been used since the previous  	*
         *	    gc.							*
         *								*
	 * When marking a nonvolatile file, replace the file's value  	*
	 * by true, forcing the file to be reread, if it has 		*
	 * survived two garbage collections without being used, 	*
         * unless the file's value is nil.  This saves the space	*
         * occupied by files.                                   	*
	 *								*
	 * If force_file_cut is true, then remove nonvolatile file      *
         * contents even if it would normally be too early to do so.    *
	 *--------------------------------------------------------------*/

	{struct file_entity* fent = FILEENT_VAL(*e);
	 if((fent->mark & 1) == 0) {
	   fent->mark += 3;  /* Set the mark, and also increment the number
				of times seen. */
	   if(fent->kind == INFILE_FK) {
	     if(IS_VOLATILE_FM(fent->mode) || 
		((fent->mark & 6) != 6 && !force_file_cut)) {
	       e = &(fent->u.file_data.val);
	       continue;  /* tail recur on &(fent->u.file_data.val). */
	     }
	     else if(ENT_FNE(fent->u.file_data.val, nil)) {
	       fent->u.file_data.val = true_ent;
	       fent->mark = 1;
	     }
	   }
	 }
	 return;
	}

      case SMALL_REAL_TAG:
	{SMALL_REAL* sr = SRVAL(*e);
	 DBLMARK(sr);
	 return;
	}

      case BOX_TAG:
	mark_box_gc(VAL(*e));
	return;

      case INDIRECT_TAG:
	if(VAL(*e) == 0) /* NOTHING */ return;

	/*--------------------------------------------------------------*
	 * Find the entity at the end of a chain of indirects.          *
	 * Replace each indirection in the chain by a direct reference  *
	 * to the end of the chain where appropriate. 			*
	 *--------------------------------------------------------------*/

	e = mark_indirection_parts(e);
	break;

      case FAIL_TAG:
      case QWRAP0_TAG:
      case QWRAP1_TAG:
      case QWRAP2_TAG:
      case QWRAP3_TAG:
      case PLACE_TAG:
	e = ENTVAL(*e);
	break;

      case LAZY_TAG:
      case LAZY_LIST_TAG:
	mark_control_gc(CTLVAL(*e), FALSE);
	return;

      case LAZY_PRIM_TAG:
	{ENTITY* p = ENTVAL(*e);
	 mark_entity_gc(p);
	 mark_entity_gc(p + 1);
	 e = p + 2;
	 break;
	}

      case BIGPOSINT_TAG:
      case BIGNEGINT_TAG:
      case STRING_TAG:
#       ifdef DEBUG
	  if(gctrace > 1) {
	    trace_i(43, BIVAL(*e), STRING_SIZE(BIVAL(*e)) & 0x3fff);
	  }
#       endif
	BINARY_MARK(BIVAL(*e));
	return;

      case ARRAY_TAG:
	mark_array_gc(ENTVAL(*e));
	return;

      case FUNCTION_TAG:
	mark_continuation_gc(CONTVAL(*e), FALSE);
	return;

      /*------------------------------------------------------------*
       * It is fairly important to do the cases below in such a way *
       * that mark_entity_parts_gc tail recurs (by looping) on the  *
       * part that will generally be the largest, to avoid deep     *
       * recursion.                                                 *
       *------------------------------------------------------------*/

      case DEMON_TAG:
      case PAIR_TAG:	/* Also RATIONAL_TAG */
      case GLOBAL_INDIRECT_TAG:
      case WRAP_TAG:
      case APPEND_TAG:
      case LARGE_REAL_TAG:
	{ENTITY* p = ENTVAL(*e);
	 mark_entity_gc(p);
	 e = p + 1;
	 break;
	}

      case TREE_TAG:
	{ENTITY* p = ENTVAL(*e);
	 mark_entity_gc(p);
	 mark_entity_gc(p + 2);
	 mark_entity_gc(p + 3);
	 mark_entity_gc(p + 4);
	 e = p + 1;
	 break;
	}

      case TRIPLE_TAG:
	{ENTITY* p = ENTVAL(*e);
	 mark_entity_gc(p);
	 mark_entity_gc(p+1);
	 e = p + 2;
	 break;
	}

      case QUAD_TAG:
	{ENTITY* p = ENTVAL(*e);
	 mark_entity_gc(p);
	 mark_entity_gc(p+1);
	 mark_entity_gc(p+2);
	 e = p + 3;
	 break;
	}

      default:
	die(35, (char *) GCTAG(*e));
    }

#   ifdef DEBUG
      if(gctrace > 1) {
	trace_i(132, e, toint(GCTAG(*e)), toint(MARKED(*e)));
      }
#   endif

    if(MARKED(*e)) return;
    GCMARK(e);
  }
}


/*********************************************************
 *			MARK_CONTINUATION_GC	         *
 *********************************************************
 * If continuation cc is not yet marked, then mark it and*
 * its parts.  If full is false, though, do not mark     *
 * c's continuation.                                     *
 *********************************************************/

PRIVATE void mark_continuation_gc(CONTINUATION *cc, Boolean full)
{
  register CONTINUATION* c = cc;

  while(c != NULL) {
    if(c->mark) return;

#   ifdef DEBUG
      if(gctrace > 1) trace_i(133, c);
#   endif

    c->mark = 1;
    mark_env_gc(c->env);
    if(c->exception_list != NIL) mark_ents_list_gc(c->exception_list);
    if(!full) return;
    c = c->continuation;
  }
}


/***********************************************************
 *			MARK_ENV_GC		           *
 ***********************************************************
 * Mark environment envp and everything accessible from it.*
 ***********************************************************/

PRIVATE void mark_env_gc(ENVIRONMENT *envp)
{
  register ENVIRONMENT *env = envp;
  register int i;
  struct envcell *cell;

  while(env != NULL) {
    if(env->mark) return;

#   ifdef DEBUG
      if(gctrace > 1) trace_i(134, env);
#   endif

    env->mark = 1;
    cell = env->cells;
    for(i = env->most_entries - 1; i >= 0; i--) {
      mark_entity_parts_gc(&(cell[i].val));
    }
    env = env->link;
  }
}


/********************************************************
 *			MARK_STACK_GC		        *
 ********************************************************
 * Mark stack ss and everything accessible from it.     *
 ********************************************************/

PRIVATE void mark_stack_gc(STACK *ss)
{
  register STACK* s = ss;
  register int i;
  ENTITY *cells;

  while(s != NULL) {
    if(s->mark) return;

#   ifdef DEBUG
      if(gctrace > 1) trace_i(135, s);
#   endif

    s->mark = 1;
    cells = s->cells;
    for(i = s->top; i >= 0; i--) {
      mark_entity_parts_gc(cells+i);
    }
    s = s->prev;
  }
}


/********************************************************
 *			MARK_STATES_GC		        *
 ********************************************************
 * Mark all of the states in list ss. The marked states *
 * are added to seen_states, along with the list cell   *
 * variable that contains them.                         *
 ********************************************************/

PRIVATE void mark_states_gc(LIST *ss)
{
  register LIST* s = ss;

# ifdef DEBUG
    if(gctrace > 1) trace_i(136);
# endif

  while(s != NIL) {
    if(s->mark) return;
    s->mark = 1;
    mark_state_gc(&(s->head.state));
    s = s->tail;
  }

}


/********************************************************
 *			MARK_STATEP_LIST_GC	        *
 ********************************************************
 * Mark all of the states referred to indirectly by     *
 * pointers in list ss.                                 *
 ********************************************************/

PRIVATE void mark_statep_list_gc(LIST *ss)
{
  register LIST* s = ss;

# ifdef DEBUG
    if(gctrace > 1) trace_i(137);
# endif

  while(s != NIL) {
    mark_state_gc(s->head.states);
    s = s->tail;
  }

}


/*********************************************************
 *			MARK_ACTIVATION_GC	         *
 *********************************************************
 * Mark activation aa, and everything accessible from it.*
 * If record is true, then add a to seen_activations if  *
 * it is not already marked.                             *
 *********************************************************/

PRIVATE void mark_activation_gc(ACTIVATION *aa, Boolean record)
{
  LIST *cr;
  register ACTIVATION* a = aa;

  if(a == NULL) return;
  if(a->actmark) {
#   ifdef DEBUG
      if(gctrace > 1) trace_i(138, a);
#   endif
    return;
  }

# ifdef DEBUG
    if(gctrace > 1) trace_i(139, a);
# endif

  a->actmark = 1;
  if(record) {
    seen_activations = gc_cons((char *) a, seen_activations);
  }

  mark_stack_gc(a->stack);
  mark_state_gc(&(a->state_a));
  mark_states_gc(a->state_hold);
  mark_control_gc(a->control, FALSE);
  mark_env_gc(a->env);
  cr = a->coroutines;
  if(cr != NIL) {
    mark_activations_gc(cr->head.list);
    mark_activations_gc(cr->tail);
  }
  mark_ents_list_gc(a->exception_list);
  mark_continuation_gc(a->continuation, TRUE);

# ifdef DEBUG
    if(gctrace > 1) trace_i(140, a);
# endif
}


/********************************************************
 *			MARK_ACTIVATIONS_GC	        *
 ********************************************************
 * Mark the activations in list l.                      *
 ********************************************************/

PRIVATE void mark_activations_gc(LIST *l)
{
  register LIST *p;

  for(p = l; p != NIL; p = p->tail) {
    mark_activation_gc(p->head.act, FALSE);
  }
}


/********************************************************
 *			MARK_CONTROLS_GC	        *
 ********************************************************
 * Mark each of the controls in list l.  Do not add     *
 * then to seen_controls. 				*
 ********************************************************/

PRIVATE void mark_controls_gc(LIST *l)
{
  LIST *p;
  for(p = l; p != NIL; p = p->tail) {
    mark_control_gc(p->head.control, FALSE);
  }
}


/********************************************************
 *			MARK_CONTROL_GC		        *
 ********************************************************
 * Mark control c and everything accessible from it.    *
 * If record is true, add c to seen_controls if it is   *
 * not already marked.                                  *
 ********************************************************/

PRIVATE void mark_control_gc(CONTROL *c, Boolean record)
{
  int info;

  while(c != NULL) {
    if(c->mark) return;

#   ifdef DEBUG
      if(gctrace > 1) trace_i(141, c);
#   endif

#   ifdef GCTEST
      CTLKIND(c);
#   endif

    c->mark = 1;
    if(record) {
      seen_controls = gc_cons((char *) c, seen_controls);
    }
    record = FALSE;
    info = c->info;

    /*---------------------------------*
     * Mark the right child (or child) *
     *---------------------------------*/

    if((info & KIND_CTLMASK) < MARK_F) {
      if((info & RCHILD_CTLMASK) == CTL_F) {
	mark_control_gc(c->right.ctl, FALSE);
      }
      else mark_activation_gc(c->right.act, FALSE);
    }

    /*---------------------------------*
     * Mark the left child (or parent) *
     *---------------------------------*/

    if((info & LCHILD_CTLMASK) == CTL_F) c = c->left.ctl;  /* tail recur */
    else {
      mark_activation_gc(c->left.act, FALSE);
      return;
    }
  }
}


/********************************************************
 *			MARK_ENTS_LIST_GC	        *
 ********************************************************
 * Mark the entities in list l.                         *
 ********************************************************/

PRIVATE void mark_ents_list_gc(LIST *l)
{
  register LIST *p;

  for(p = l; p != NIL && !p->mark; p = p->tail) {
    p->mark = 1;
    mark_entity_gc(p->head.ents);
  }
}


/********************************************************
 *			MARK_ARRAY_GC		        *
 ********************************************************
 * An array header describes what might be a subarray   *
 * of a larger array.  This function marks all of the   *
 * entities in header hd and the array described by hd, *
 * not just in the subarray, but from the beginning of  *
 * the subarray to the end of the array that contains   *
 * it.  This is important, because if a larger part of  *
 * the same containing array is to be marked, the mark  *
 * on a member of the current subarray might suppress   *
 * further marking of the larger array.                 *
 ********************************************************/

PRIVATE void mark_array_gc(ENTITY *hd)
{
  LONG len, f;
  register int tag;
  ENTITY *p, *refhd;

  /*--------------------------------------------------------------*
   * hd[0] contains the length of the array.  hd[1] refers to the *
   * start of the array.  Mark both, and get the length and the   *
   * kind, which is the tag hd[1].                                *
   *--------------------------------------------------------------*/

  GCMARK(hd);
  GCMARK(hd+1);
  len = IVAL(hd[0]);
  tag = GCTAG(hd[1]);

  /*--------------------------------------------------------------------*
   * Mark an array of nonshared boxes.  In this case, hd[1] is the	*
   * number of the first box in the array.  Update marked_boxes.        *
   *--------------------------------------------------------------------*/

  if(tag == BOX_TAG) {
    f = VAL(hd[1]);
    marked_boxes = mark_box_range_gc(f, f + len - 1, marked_boxes);
  }

  /*------------------------------------------------------------------*
   * Mark an array of shared boxes. In this case, hd[1] points to the *
   * first cell in the array and hd[3] is the array of     	      *
   * which this is a subarray.  We need to mark that array.           *
   * hd[2] is the follower.  It needs to be marked.		      *
   *								      *
   * An array header hd is presumed to be marked if hd[3] is marked.  *
   *------------------------------------------------------------------*/

  else if(tag == PLACE_TAG) {
    refhd = ENTVAL(hd[3]);
    if(!MARKED(refhd[3])) {
      GCMARK(hd+3);
      GCMARK(refhd);
      GCMARK(refhd + 1);
      GCMARK(refhd + 3);
      len = IVAL(refhd[0]);
      fully_mark_entity_array_gc(ENTVAL(refhd[1]), len);

      /*----------------------------------------------------*
       * We must mark the follower of this head, and also   *
       * the follower of the head to which this one refers, *
       * since we are marking that one now.		    *
       *----------------------------------------------------*/

      mark_entity_gc(refhd + 2);
      mark_entity_gc(hd + 2);
    }
  }

  /*----------------------------------------------------------------------*
   * Mark a general array.  In this case, hd[3] is the number of entities *
   * beyond the end of this array that need to be marked to finish the    *
   * enclosing array.  We always mark a complete suffix of the full	  *
   * array.  Hence, when a marked member is encountered, we can stop	  *
   * marking.	                                                          *
   *----------------------------------------------------------------------*/

  else if(tag == INDIRECT_TAG) {
    int hd3_tag = GCTAG(hd[3]);
    GCMARK(hd+3);
    if(hd3_tag != NOREF_TAG) die(167, hd3_tag);
    len += IVAL(hd[3]);  /* length to mark */
    p = ENTVAL(hd[1]);
    while(len > 0) {
      if(MARKED(*p)) break;
      mark_entity_gc(p++);
      len--;
    }
    mark_entity_gc(hd + 2);
  }

  else if(tag == STRING_TAG) {
    BINARY_MARK(BIVAL(hd[1]));
    GCMARK(hd+1);
    GCMARK(hd+3);
    mark_entity_gc(hd+2);
  }

  else /* tag == ENTITY_TOP_TAG */ {
    fully_mark_entity_array_gc(ENTVAL(hd[1]), len);
    mark_entity_gc(hd + 2);
  }
}


/********************************************************
 *		FULLY_MARK_ENTITY_ARRAY_GC	        *
 ********************************************************
 * Mark the entities of in array a, of size n.	        *
 ********************************************************/

PRIVATE void fully_mark_entity_array_gc(ENTITY* a, LONG n)
{
  ENTITY* p = a;
  while(n > 0) {
    mark_entity_gc(p++);
    n--;
  }
}


/********************************************************
 *			MARK_ENTP_ARRAY_GC	        *
 ********************************************************
 * Mark the parts of the thing pointed to by the n      *
 * ENTITY* values in array a.                           *
 ********************************************************/

PRIVATE void mark_entp_array_gc(ENTITY **a, int n)
{
  register int i;

# ifdef DEBUG
    if(gctrace > 1) trace_i(143, n);
# endif

  for(i = 0; i < n; i++) {
    mark_entity_parts_gc(a[i]);
  }

# ifdef DEBUG
    if(gctrace > 1) trace_i(144);
# endif
}


/********************************************************
 *			MARK_ENTPP_ARRAY_GC	        *
 ********************************************************
 * Mark the entities pointed to indirectly by the n     *
 * ENTITY** values in array a.                          *
 ********************************************************/

PRIVATE void mark_entpp_array_gc(ENTITY ***a, int n)
{
  register int i;

  for(i = 0; i < n; i++) {
    register ENTITY *p = *(a[i]);
    if(p != NULL) mark_entity_gc(p);
  }
}


/********************************************************
 *			MARK_STANDARD_ENTS_GC	        *
 ********************************************************
 * Mark the things accessible from entities defined in  *
 * entity.c.                                            *
 ********************************************************/

PRIVATE void mark_standard_ents_gc(void)
{
  int i;

# ifdef DEBUG
    if(gctrace > 1) trace_i(147);
# endif

  for(i = 0; i < FIRST_FREE_BOX_NUM; i++) {
    mark_box_gc(i);
  }
  mark_entity_parts_gc(&failure_as_entity);
  mark_entity_parts_gc(&last_exception);
  mark_entity_parts_gc(&fail_ex);
  mark_entity_parts_gc(&a_large_int);
  mark_entity_parts_gc(&zero_rat);
  mark_entity_parts_gc(&one_rat);
  mark_entity_parts_gc(&ten_rat);
  mark_entity_parts_gc(&zero_real);
  mark_entity_parts_gc(&one_real);
  mark_entity_parts_gc(&ten_real);
  mark_entity_parts_gc(&half_real);
  mark_entity_parts_gc(&dollar_fixp_low);
  mark_entity_parts_gc(&infloop_timeout);
  mark_entity_parts_gc(&large_one_real);
  mark_entity_parts_gc(&large_ten_real);
  mark_entity_parts_gc(&stdout_file);
  mark_entity_parts_gc(&stderr_file);
  mark_entity_parts_gc(&newline);
  mark_entity_parts_gc(&divide_by_zero_ex);
  for(i = 0; i < TEN_TO_MAX; i++) {
    mark_entity_parts_gc(ten_to_p + i);
  }

# ifdef DEBUG
    if(gctrace > 1) trace_i(148);
# endif
}


/********************************************************
 *			MARK_CONSTANTS_GC	        *
 ********************************************************
 * Mark the things accessible from contants, in the     *
 * constant table.  Also mark the contants held by      *
 * real.c.						*
 ********************************************************/

PRIVATE void mark_constants_gc()
{
  LONG i;

# ifdef DEBUG
    if(gctrace > 1) trace_i(149);
# endif

  for(i = 0; i < next_const; i++) {
    mark_entity_parts_gc((ENTITY *)(constants + i));
  }
  mark_entity_parts_gc(&ln_base_val);

# ifdef DEBUG
    if(gctrace > 1) trace_i(150);
# endif
}


/**********************************************************
 *			MARK_GLOBALS_GC		          *
 **********************************************************
 * Mark the things accessible from entities in the global *
 * mono-table.                                            *
 **********************************************************/

PRIVATE void mark_val_gc(HASH2_CELLPTR h)
{
  mark_entity_parts_gc(&(h->val.entity));
}

/*---------------------------------------------------*/

PRIVATE void mark_globals_gc()
{
  int i;

# ifdef DEBUG
    if(gctrace > 1) trace_i(151);
# endif

  for(i = 0; i < next_ent_num; i++) {
    scan_hash2(outer_bindings[i].mono_table, mark_val_gc);
  }

# ifdef DEBUG
    if(gctrace > 1) trace_i(153);
# endif
}


/*=====================================================================
			UNMARKING
  =====================================================================*/

/********************************************************
 *			UNMARK_GC		        *
 ********************************************************
 * Unmark everything that is not unmarked during the    *
 * collect phase.  In the process, do relocations of	*
 * things found during unmarking, if compactifying.	*
 ********************************************************/

PRIVATE void unmark_gc()
{
  LIST *p;

  if(compactifying) rebuild_states_gc();
  else unmark_seen_states_gc();

  for(p = seen_activations; p != NIL; p = p->tail) {
    unmark_activation_gc(p->head.act);
  }
  for(p = seen_controls; p != NIL; p = p->tail) {
    unmark_control_gc(p->head.control);
  }
  unmark_entpp_array_gc(rts_pents, num_rts_pents);
}


/********************************************************
 *			UNMARK_ENTPP_ARRAY_GC	        *
 ********************************************************
 * Unmark the entities pointed to indirectly by the n   *
 * members of array a.                                  *
 ********************************************************/

PRIVATE void unmark_entpp_array_gc(ENTITY ***a, int n)
{
  register int i;
  for(i = 0; i < n; i++) {
    register ENTITY *p = *(a[i]);
    if(p != NULL) GCUNMARK(p);
  }
}


/********************************************************
 *			UNMARK_LIST_GC	        	*
 ********************************************************
 * Unmark the list nodes in list l.  This function does *
 * not unmark the members of the list, since that is    *
 * presumed to be done elsewhere.			*
 ********************************************************/

PRIVATE void unmark_list_gc(LIST *l)
{
  LIST *p;

  for(p = l; p != NIL && p->mark; p = p->tail) {
    p->mark = 0;
  }
}

 
/********************************************************
 *			UNMARK_ACTIVATION_GC	        *
 ********************************************************/

PRIVATE void relocate_ents_list_gc(ENTS_LIST *l);

PRIVATE void unmark_activation_gc(ACTIVATION *a)
{
  LIST *cr;

  if(a != NULL && a->actmark) {
    a->actmark = 0;

#   ifdef DEBUG
      if(gctrace > 1) trace_i(154, a);
#   endif

    unmark_continuation_gc(a->continuation);
    unmark_stack_gc(a->stack);
    unmark_control_gc(a->control);
    unmark_env_gc(a->env);
    if(compactifying) relocate_ents_list_gc(a->exception_list);
    unmark_list_gc(a->exception_list);
    cr = a->coroutines;
    if(cr != NIL) {
      unmark_activations_gc(cr->head.list);
      unmark_activations_gc(cr->tail);
    }

    /*--------------------------------------------------------*
     * States are unmarked by scanning seen_states.  Here, we *
     * just unmark the list nodes in the state_hold list.     *
     *--------------------------------------------------------*/

    unmark_list_gc(a->state_hold);
  }
}


/********************************************************
 *			UNMARK_ACTIVATIONS_GC	        *
 ********************************************************/

PRIVATE void unmark_activations_gc(LIST *l)
{
  register LIST *p;

  for(p = l; p != NIL; p = p->tail) {
    unmark_activation_gc(p->head.act);
  }
}


/********************************************************
 *			UNMARK_CONTINUATION_GC	        *
 ********************************************************/

PRIVATE void unmark_continuation_gc(CONTINUATION *c)
{
  while(c != NULL && c->mark) {
#   ifdef DEBUG
      if(gctrace > 1) trace_i(155, c);
#   endif

    c->mark = 0;
    unmark_env_gc(c->env);
    if(compactifying) relocate_ents_list_gc(c->exception_list);
    unmark_list_gc(c->exception_list);
    c = c->continuation;
  }
}


/********************************************************
 *			RELOCATE_STACK_GC		*
 ********************************************************
 * Relocate each entity in stack cell s.                *
 ********************************************************/

PRIVATE void relocate_stack_gc(STACK *s)
{
  register int i;
  ENTITY *cells;

# ifdef DEBUG
    if(gctrace > 1) trace_i(276);
# endif

  cells = s->cells;
  for(i = s->top; i >= 0; i--) {
    relocate_gc(cells+i);
  }
}


/********************************************************
 *			UNMARK_STACK_GC		        *
 ********************************************************
 * Unmark stack ss, and relocate the entities in it if  *
 * the collect phase compactified.                      *
 ********************************************************/

PRIVATE void unmark_stack_gc(STACK *ss)
{
  register STACK* s = ss;
  while(s != NULL && s->mark) {
#   ifdef DEBUG
      if(gctrace > 1) trace_i(156, s);
#   endif

    s->mark = 0;
    if(compactifying) relocate_stack_gc(s);
    s = s->prev;
  }
}


/********************************************************
 *			RELOCATE_ENV_GC		        *
 ********************************************************
 * Relocate all cells in environment env.               *
 ********************************************************/

PRIVATE void relocate_env_gc(ENVIRONMENT *env)
{
  register int i;
  struct envcell *cell;

# ifdef DEBUG
    if(gctrace > 1) trace_i(277);
# endif

  cell = env->cells;
  for(i = env->most_entries - 1; i >= 0; i--) {
    relocate_gc(&(cell[i].val));
  }
}


/********************************************************
 *			UNMARK_ENV_GC		        *
 ********************************************************
 * Unmark environment envp, and environments accessible *
 * from it.  Also relocate entities in the environment, *
 * if the collect phase compactified.                   *
 ********************************************************/

PRIVATE void unmark_env_gc(ENVIRONMENT *envp)
{
  register ENVIRONMENT* env = envp;
  while(env != NULL && env->mark) {

#   ifdef DEBUG
      if(gctrace > 1) trace_i(157, env);
#   endif

    env->mark = 0;
    if(compactifying) relocate_env_gc(env);
    env = env->link;
  }
}


/********************************************************
 *			UNMARK_CONTROL_GC	        *
 ********************************************************/

PRIVATE void unmark_control_gc(CONTROL *c)
{
  int info;

  while(c != NULL && c->mark) {
#   ifdef DEBUG
      if(gctrace > 1) trace_i(158, c);
#   endif

    c->mark = 0;
    info = c->info;

    /*------------------------------------*
     * Unmark the right child (or child). *
     *------------------------------------*/

    if((info & KIND_CTLMASK) < MARK_F) {
      if((info & RCHILD_CTLMASK) == CTL_F) unmark_control_gc(c->right.ctl);
      else unmark_activation_gc(c->right.act);
    }

    /*------------------------------------*
     * Unmark the left child (or parent). *
     *------------------------------------*/

    if((info & LCHILD_CTLMASK) == CTL_F) c = c->left.ctl;  /* tail recur */
    else {
      unmark_activation_gc(c->left.act);
      return;
    }
  }
}



/*=====================================================================
			COLLECTING
 *=====================================================================
 * The following functions are used to perform collection without     *
 * compactification.						      *
 **********************************************************************/


/********************************************************
 *			COUNT_AVAIL_BLOCKS_GC		*
 ********************************************************
 * Add as much to free_bytes as there is space in list  *
 * avail_blocks.                                        *
 ********************************************************/

PRIVATE void count_avail_blocks(void)
{
  register BINARY_BLOCK *p;
  for(p = avail_blocks; p != NULL; p = p->next) {
    free_bytes += BINARY_BLOCK_SIZE;
  }
}

/********************************************************
 *			SET_UP_FOR_NEXT_GC		*
 ********************************************************
 * Set get_before_gc, etc. for the next collection.	*
 ********************************************************/

PRIVATE void set_up_for_next_gc(void)
{
  count_avail_blocks();
  alloc_phase = 0;

# ifdef GCT
    get_before_gc = get_before_gc_reset = GET_BEFORE_GC_INIT;
# else
    /*----------------------------------------------------*
     * Set get_before_gc to				  *
     * max(used_bytes, free_bytes/3, GET_BEFORE_GC_INIT). *
     *----------------------------------------------------*/

    get_before_gc = free_bytes/3;
    if(get_before_gc < used_bytes - huge_binary_bytes) {
      get_before_gc = used_bytes - huge_binary_bytes;
    }
    if(get_before_gc < GET_BEFORE_GC_INIT) get_before_gc = GET_BEFORE_GC_INIT;
    get_before_gc_reset = get_before_gc;
# endif
}


/*********************************************************
 *			FINISH_COLLECT_GC		 *
 *********************************************************
 * Perform that part of the collect phase after handling *
 * the entity heap. Then set get_before_gc, etc. in      *
 * preparation for next garbage collection.              *
 *********************************************************/

PRIVATE void finish_collect_gc(void)
{
  collect_continuations_gc();
  collect_controls_gc();
  collect_small_reals_gc();
  collect_files_gc();
  small_reals_since_last_gc =
    ents_since_last_gc =
    binary_bytes_since_last_gc =
    files_since_last_gc = 0;
}


/********************************************************
 *			COLLECT_GC		        *
 ********************************************************
 * Perform the collect phase, without compactification. *
 ********************************************************/

PRIVATE void collect_gc()
{
  free_bytes = used_bytes = huge_binary_bytes = 0;
  collect_entities_gc();
  set_wheres();
  collect_binaries_gc();
  collect_huge_binaries_gc();
  set_binary_wheres();
  finish_collect_gc();
}


/********************************************************
 *			COLLECT_ENTITIES_GC	        *
 ********************************************************
 * Collect the entity heap.  This function is only used *
 * when not compactifying.	                        *
 ********************************************************/

PRIVATE void collect_entities_gc(void)
{
  ENT_BLOCK *p;
  register ENTITY *q;
  ENTITY *endq, *cluster_start;
  int i;
# ifdef DEBUG
    Boolean printed_first;
# endif

# ifdef DEBUG
    if(gctrace > 1) {
      trace_i(159);
      trace_i(65);
      print_block_chain((BINARY_BLOCK*) used_blocks);
    }
# endif

  /*----------------------------*
   * Clear the free_ents table. *
   *----------------------------*/

  for(i = 0; i < FREE_ENTS_SIZE; i++) {
    free_ent_data[i].free_ents = NULL;
  }

  /*-----------------------*
   * Scan each used block. *
   *-----------------------*/

  for(p = used_blocks; p != NULL; p = p->next) {

    /*-------------------------------*
     * Scan the cells of this block. *
     *-------------------------------*/

    q    = p->cells;
    endq = q + ENT_BLOCK_SIZE;
    while(q < endq) {

#     ifdef DEBUG
	printed_first = FALSE;
	if(gctrace > 1  && MARKED(*q)) {
	  trace_i(160, q);
	  printed_first = TRUE;
	}
#     endif

      /*---------------------------------------------------------*
       * Scan over a cluster of marked entities, unmarking each. *
       *---------------------------------------------------------*/

      while(q < endq && MARKED(*q)) {
	GCUNMARK(q++) ;
	used_bytes += sizeof(ENTITY);
      }

#     ifdef DEBUG
	if(gctrace > 1 && printed_first) fprintf(TRACE_FILE,"%p\n", q-1);
#     endif

      /*----------------------------------------------------------*
       * Pointer cluster_start points to the start of the cluster *
       * of unmarked entities that we are about to collect.	  *
       *----------------------------------------------------------*/

      cluster_start = q;

#     ifdef DEBUG
	printed_first = FALSE;
	if(gctrace > 1 && q < endq && !MARKED(*q)) {
	  trace_i(161, q);
	  printed_first = TRUE;
	}
#     endif

      /*----------------------------------------------------------------*
       * Scan over a cluster of unmarked entities, and add that cluster *
       * to the free space.   						*
       *----------------------------------------------------------------*/

      while(q < endq && !MARKED(*q)) {
#       ifdef GCTEST
	  *q = GARBAGE;
#       else
	  *q = false_ent;
#	endif
	q++;
      }

#     ifdef DEBUG
	if(gctrace > 1 && printed_first) fprintf(TRACE_FILE,"%p\n", q - 1);
#     endif

#     ifndef GCTEST
	if(q - cluster_start > 1) free_piece_gc(cluster_start, q);
#     endif
    }
  }

# ifdef DEBUG
    if(gctrace) {
      trace_i(162);
      trace_i(65);
      print_block_chain((BINARY_BLOCK*) used_blocks);
    }
# endif
}


/********************************************************
 *			FREE_PIECE_GC		        *
 ********************************************************
 * Add memory from pointer r to pointer q-1 to the free *
 * space lists.                                         *
 ********************************************************/

PRIVATE void free_piece_gc(ENTITY *r, ENTITY *q)
{
  register int n, i;
  ENTITY **p;

  n = toint(q - r);
  free_bytes += n * sizeof(ENTITY);
  for(i = 1; 
      i < FREE_ENTS_SIZE && free_ent_data[i].free_ents_size <= n; 
      i++) {
    /* Nothing */
  }
  p = &(free_ent_data[i-1].free_ents);  
  r[0] = (*p == NULL) ? false_ent : ENTP(INDIRECT_TAG, *p);
  r[1] = ENTU(n);
  *p = r;

# ifdef DEBUG
    if(gctrace > 1) {
      trace_i(163,  n, r, q, i-1, free_ent_data[i-1].free_ents);
    }
# endif
}


/********************************************************
 *			COLLECT_BINARIES_GC	        *
 ********************************************************
 * Collect the binary heap.  This function is only used *
 * when not compactifying.	                        *
 ********************************************************/

PRIVATE void collect_binaries_gc(void)
{
  BINARY_BLOCK *p;
  register CHUNKPTR q;
  CHUNKPTR endq, cluster_start;
  int i, cluster_size;
# ifdef DEBUG
    Boolean printed_first;
# endif

# ifdef DEBUG
    if(gctrace > 1) {
      trace_i(152);
      trace_i(66);
      print_block_chain(used_binary_blocks);
    }
# endif

  /*------------------------------*
   * Clear the free_chunks table. *
   *------------------------------*/

  for(i = 0; i < FREE_BINARY_SIZE; i++) {
    free_binary_data[i].free_chunks = NULL;
  }

  /*-----------------------*
   * Scan each used block. *
   *-----------------------*/

  for(p = used_binary_blocks; p != NULL; p = p->next) {

    /*-------------------------------*
     * Scan the cells of this block. *
     *-------------------------------*/

    q    = p->cells;
    endq = q + BINARY_BLOCK_SIZE;
    while(q < endq) {

#     ifdef DEBUG
	printed_first = FALSE;
	if(gctrace > 1 && BINARY_MARKED(q)) {
	  trace_i(160, q);
	  printed_first = TRUE;
	}
#     endif

      /*---------------------------------------------------------*
       * Scan over a cluster of marked chunks, unmarking each.   *
       * Each chunk has a size in its first two bytes telling 	 *
       * how many bytes that chunk contains, excluding those two *
       * bytes.  It is important to unmark *q before getting the *
       * size since the mark bit is in those two bytes, and will *
       * corrupt the size.					 *
       *---------------------------------------------------------*/

      while(q < endq && BINARY_MARKED(q)) {
	register CHUNKHEADPTR qq;
	register USHORT nn, size;

        BINARY_UNMARK(q);
	qq = (CHUNKHEADPTR) q;
	nn = *qq;                   /* Nominal size of this chunk */

        /*----------------------------------------------------------*
         * size is the actual size of this chunk, in bytes, 	    *
	 * including the header.  The allocator puts 16380 in nn if *
	 * the size is 4, and 16381 if the size is 8.		    *
	 *----------------------------------------------------------*/

	if(nn >= 16380) {
	  size = (nn == 16380) ? 4 : 8;
	}
	else {
          size = nn + SHORT_BYTES;
	  if(size < CHUNK_MIN_SIZE) size = CHUNK_MIN_SIZE;
	  while((size & TRUE_LONG_ALIGN) != 0) size++;
	}

	used_bytes += size;
	q          += size;
      }

#     ifdef DEBUG
	if(gctrace > 1 && printed_first) fprintf(TRACE_FILE,"%p\n", q-1);
#     endif

      /*----------------------------------------------------------*
       * Pointer cluster_start points to the start of the cluster *
       * of unmarked chunks that we are about to collect.	  *
       * 							  *
       * cluster_size accumulates the total size of the cluster.  *
       *----------------------------------------------------------*/

      cluster_start = q;
      cluster_size  = 0;

#     ifdef DEBUG
	printed_first = FALSE;
	if(gctrace > 1 && q < endq && !BINARY_MARKED(q)) {
	  trace_i(161, q);
	  printed_first = TRUE;
	}
#     endif

      /*----------------------------------------------------------------*
       * Scan over a cluster of unmarked chunks, and add that cluster   *
       * to the free space. 						*
       *----------------------------------------------------------------*/

      while(q < endq && !BINARY_MARKED(q)) {
        register CHUNKHEADPTR qq = (CHUNKHEADPTR) q;
	register USHORT nn = *qq;
        register USHORT size;

        /*----------------------------------------------------------*
         * size is the actual size of this chunk, including the	    *
         * header.  The allocator puts 16380 in nn if the size is   *
  	 * 4, and 16381 if the size is 8.			    *
	 *----------------------------------------------------------*/


	if(nn >= 16380) {
	  size = (nn == 16380) ? 4 : 8;
	}
	else {
          size = nn + SHORT_BYTES;
	  if(size < CHUNK_MIN_SIZE) size = CHUNK_MIN_SIZE;
	  while((size & TRUE_LONG_ALIGN) != 0) size++;
	}

	cluster_size += size;
	q            += size;
      }

#     ifdef DEBUG
	if(gctrace > 1 && printed_first) fprintf(TRACE_FILE,"%p\n", q - 1);
#     endif

#     ifndef GCTEST
	if(q - cluster_start >= CHUNK_MIN_SIZE) {
	  free_binary_piece_gc(cluster_start, q);
	}
#     endif
    }
  }

# ifdef DEBUG
    if(gctrace) {
      trace_i(164);
      trace_i(66);
      print_block_chain(used_binary_blocks);
    }
# endif
}


/********************************************************
 *			FREE_BINARY_PIECE_GC		*
 ********************************************************
 * Add memory from pointer r to pointer q-1 to the free *
 * space lists.                                         *
 ********************************************************/

PRIVATE void free_binary_piece_gc(CHUNKPTR r, CHUNKPTR q)
{
  register int i, n_data, n_with_header;
  CHUNKPTR *p;
  CHUNKHEADPTR rr = (CHUNKHEADPTR) r;

  n_with_header  = toint(q - r);
  n_data         = n_with_header - SHORT_BYTES;
  free_bytes    += n_with_header;
  *rr            = n_data;

  for(i = 1; 
      i < FREE_BINARY_SIZE 
        && free_binary_data[i].free_chunks_size <= n_data; 
      i++) {
    /* Nothing */
  }

  p  = &(free_binary_data[i-1].free_chunks);  
  INSTALL_BINARY_POINTER(r, *p);
  *p = r;

# ifdef DEBUG
    if(gctrace > 1) {
      trace_i(165,  n_data, r, q, i-1, free_binary_data[i-1].free_chunks);
    }
# endif
}


/********************************************************
 *		  COLLECT_HUGE_BINARIES_GC  	        *
 ********************************************************
 * Collect huge binary chunks that have not been marked.*
 ********************************************************/

PRIVATE void collect_huge_binaries_gc(void)
{
  int i;

  for(i = 0; i < huge_binaries_allocated_size; i++) {
    register CHUNKPTR p = huge_binaries_allocated[i];
    if(p != NULL) {
      if(!BINARY_MARKED(p)) {
        FREE(p);
        huge_binaries_allocated[i] = NULL;
      }
      else {
	LONG size;
	BINARY_UNMARK(p);
	size               = STRING_SIZE(p) + 3*SHORT_BYTES;
	huge_binary_bytes += size;
        used_bytes        += size;
      }
    }

  }
}


/********************************************************
 *			COLLECT_FILES_GC  	        *
 ********************************************************
 * Collect the file heap.                               *
 ********************************************************/

PRIVATE void collect_files_gc()
{
  struct file_ent_block *feb;
  struct file_entity *fent;
  int i, index;

# ifndef GCTEST
    free_file_entities = NULL;
# endif

  for(feb = file_ent_blocks; feb != NULL; feb = feb->next) {
    for(i = 0; i < FILE_ENT_BLOCK_SIZE; i++) {
      fent = feb->blk + i;
      if((fent->mark & 1) == 0) {  /* Not marked */
	index = fent->descr_index;
	if(index >= 0) {

#         ifdef MSWIN
	  if(fent->kind == FONT_FK) {
	    int handle = font_table[index].handle;
	    if(handle > 0) {
	      closeFont(fent->u.font_data.fileName, (HFONT) handle);
	    }
	    font_table[index].handle = -1;
	  }
	  else
#         endif

	  {
	    FILE* f = file_table[index].file;
	    int fd = file_table[index].fd;
	    if(fd > 2) {
	      if(f != NULL) fclose(f); else close(fd);
	    }
	    file_table[index].file = NULL;
	    file_table[index].fd   = -1;
	  }
	}
	free_file_entity(fent);
      }

      else {  /* marked */
	fent->mark &= 0xfe;  /* Clear the mark bit. */
	if(compactifying && fent->kind == INFILE_FK) {
	  relocate_gc(&fent->u.file_data.val);
	}
      }
    }
  }
}


/********************************************************
 *			COLLECT_CONTINUATIONS_GC        *
 ********************************************************
 * Collect the continuations that have been recorded as *
 * referred to by function entities, but which are not  *
 * marked.  Rebuild fun_conts with the continuations    *
 * that are marked.                                     *
 ********************************************************/

PRIVATE void collect_continuations_gc()
{
  register CONTINUATION *p;
  CONTINUATION *q;

  p         = fun_conts;
  fun_conts = NULL;

# ifdef DEBUG
    if(gctrace > 1) trace_i(167);
# endif

  /*-----------------------------------------------------*
   * Note that the exception_list field of all of these  *
   * continuations must be NULL, so we can ignore it.    *
   *-----------------------------------------------------*/

  while(p != NULL) {
    q = p->continuation;
    if(p->mark) {
      p->mark = 0;
      unmark_env_gc(p->env);
      p->continuation = fun_conts;
      fun_conts = p;
    }
    else {
      drop_env(p->env, p->num_entries);
      free_continuation(p);
    }
    p = q;
  }
}


/********************************************************
 *			COLLECT_CONTROLS_GC	        *
 ********************************************************
 * Collect the controls that have been recorded as      *
 * referred to by lazy entities, but which are not      *
 * marked.  Rebuild lazy_controls with the controls     *
 * that are marked.                                     *
 ********************************************************/

PRIVATE void collect_controls_gc()
{
  struct control_reg *p, *q;
  int k, top_size;
  CONTROL *c;
# ifdef DEBUG
    LONG used_cnt = 0, free_cnt = 0;
# endif

# ifdef DEBUG
    if(gctrace > 1) trace_i(168);
# endif

  p             = lazy_controls;
  top_size      = lazy_controls_top_size;
  lazy_controls = NULL;

  while(p != NULL) {
    for(k = top_size - 1; k >= 0; k--) {
      c = p->cells[k];
      if(c->mark) {
#       ifdef DEBUG
	  used_cnt++;
#       endif
	unmark_control_gc(c);
	note_control(c);
      }
      else {
#       ifdef GCTEST
          c->ref_cnt = -100;
#       else
#         ifdef DEBUG
	    free_cnt++;
#         endif
	  c->ref_cnt = 1;
	  drop_control(c);
#       endif
      }
    }
    q = p->next;
    free_control_reg(p);
    p = q;
    top_size = CONTROL_REG_SIZE;
  }

# ifdef DEBUG
    if(gctrace > 1) trace_i(169, used_cnt, free_cnt);
# endif
}


/********************************************************
 *			COLLECT_SMALL_REALS_GC          *
 ********************************************************
 * Collect the heap holding doubles.                    *
 ********************************************************/

PRIVATE void collect_small_reals_gc()
{
  int i;
  LONG marks;
  SMALL_REAL_BLOCK *p;
  SMALL_REAL *q;

# ifdef DEBUG
    if(gctrace > 1) trace_i(170);
# endif

  /*----------------------------------------*
   * Clear the small reals free space list. *
   *----------------------------------------*/

  free_small_reals = NULL;

  /*------------------------------------------------------------*
   * Now collect unmarked small reals, and add them to the 	*
   * free space list.						*
   *------------------------------------------------------------*/

  for(p = small_real_blocks; p != NULL; p = p->next) {
    marks = p->marks;
    for(i = 1; i <= SMALL_REAL_BLOCK_SIZE; i++) {
      q = p->cells + i - 1;
      if((marks & (1L << i)) == 0) {
#       ifndef GCTEST
	  q->next          = free_small_reals;
	  free_small_reals = q;
#       endif
      }
    }
    p->marks = 0;
  }
}

/*=============================================================
			COMPACTIFYING
  =============================================================
  The following are used to do collection with compactification.
 *****************************************************************/

/********************************************************
 *			COMPACTIFY_GC		        *
 ********************************************************
 * Collect and compactify marked entities.              *
 ********************************************************/

PRIVATE void compactify_gc(void)
{
  free_bytes = used_bytes = huge_binary_bytes = 0;

  /*----------------------------------------------------*
   * Add relocation information to marked_boxes.	*
   *----------------------------------------------------*/

  next_box_number = compactify_boxset(marked_boxes);

  /*---------------------------------------------------------*
   * Do the compactification.  This involves moving entities *
   * to new blocks, replacing them by RELOCATE_TAG entities  *
   * where they formerly were.				     *
   *---------------------------------------------------------*/

  do_compactification_gc();
  do_binary_compactification_gc();
  collect_huge_binaries_gc();

  /*---------------------------------------------------------*
   * Now relocate pointers that are pointing to RELOCATE_TAG *
   * entities, making them point to the new locations of the *
   * entities. The following does not handle entities that   *
   * are in stacks, environments, states, etc.		     *
   *---------------------------------------------------------*/

  do_relocation_gc();

  /*--------------------------------------------------------*
   * Finish the collection and set up the free space table. *
   *--------------------------------------------------------*/

  set_wheres();
  set_binary_wheres();
  finish_collect_gc();
}


/********************************************************
 *	  MOVE_RELOCATION_BLOCKS_TO_AVAIL_GC		*
 ********************************************************
 * Move the relocation_blocks chain *rel_blocks to	*
 * avail_blocks.					*
 ********************************************************/

PRIVATE void move_relocation_blocks_to_avail_gc(BINARY_BLOCK** rel_blocks)
{
  register BINARY_BLOCK* p = *rel_blocks;
  register BINARY_BLOCK* s;

  while(p != NULL) {
    s            = p->next;
    p->next      = avail_blocks;
    avail_blocks = p;
    p            = s;
  }
  *rel_blocks = NULL;
}


/********************************************************
 *			MOVE_ENTITIES		        *
 ********************************************************
 * Move n entities from source to dest, leaving         *
 * RELOCATE_TAG entities behind in the source array,    *
 * pointing to the corresponding positions in the dest  *
 * array.                                               *
 ********************************************************/

PRIVATE void move_entities(ENTITY *dest, ENTITY *source, int n)
{
  register ENTITY *p = dest;
  register ENTITY *q = source;

# ifdef DEBUG
    if(gctrace > 1) {
      trace_i(268, n, source, dest);
    }
# endif

  while((n--) > 0) {

#   ifdef DEBUG
      if(gctrace > 1) trace_i(278, q, p);
#   endif

    *p = *q;
    *(q++) = ENTP(RELOCATE_TAG, (p++));
  }
}


/********************************************************
 *			DO_COMPACTIFICATION_GC 	        *
 ********************************************************
 * do_compactification_gc() moves the currently used    *
 * entities to fresh blocks, and puts RELOCATE_TAG      *
 * values where they previously were.  Consecutive      *
 * marked entities remain consecutive after relocation, *
 * so that arrays are preserved.                        *
 ********************************************************/

PRIVATE void do_compactification_gc(void)
{
  ENT_BLOCK *first, *second, *p, *s;
  register ENTITY *q;
  ENTITY *endq, *r, *first_current, *second_current;
  int first_room, second_room, k;

# ifdef DEBUG
    if(gctrace > 1) {
      trace_i(267);
      trace_i(65);
      print_block_chain((BINARY_BLOCK*) used_blocks);
      trace_i(273);
      print_block_chain(avail_blocks);
    }
# endif

  /*----------------------------*
   * Clear the free_ents table. *
   *----------------------------*/

  for(k = 0; k < FREE_ENTS_SIZE; k++) {
    free_ent_data[k].free_ents = NULL;
  }

  /*-----------------------------------------------------------------*
   * Get two empty blocks from avail_blocks, or allocate them        *
   * if necessary.  Set first_room to the number of slots available  *
   * in first, and first_current to the location where new entities  *
   * will be added to first.  Do similarly for second_room and       *
   * second_current.  When moving a cluster of entities, we try to   *
   * put in the second block.  If that is not possible, then we put  *
   * in the first block.  That way, the second block remains viable  *
   * as a place to put small clusters when a large cluster comes     *
   * along.  If neither the first nor the second block has room, we  *
   * get a new first block, retire the second block as full, and     *
   * the old first block now the second block.			     *
   *-----------------------------------------------------------------*/

  first          = (ENT_BLOCK*) get_avail_block();
  first_current  = first->cells;
  second         = (ENT_BLOCK*) get_avail_block();
  second_current = second->cells;
  second->next   = NULL;
  first->next    = second;
  first_room     = second_room = ENT_BLOCK_SIZE;

  /*-------------------------------------------------------*
   * Scan used_blocks, moving entities to the free blocks. *
   *-------------------------------------------------------*/

  for(p = used_blocks; p != NULL; p = p->next) {
    q    = p->cells;
    endq = q + ENT_BLOCK_SIZE;
    while(q < endq) {

      /*------------------------------*
       * Skip over unmarked entities. *
       *------------------------------*/

      while(q < endq && !MARKED(*q)) q++;
      if(q == endq) break;

      /*--------------------------------------------------------*
       * Count the length of the next cluster of used entities. *
       *--------------------------------------------------------*/

      r = q;
      while(r < endq && MARKED(*r)) {r++;}
      k = toint(r - q);

      /*------------------------------------------------*
       * The following loop goes through at most twice. *
       *------------------------------------------------*/

      for(;;) {

	/*------------------------------------------------------------*
	 * If this cluster fits into the second block, move it there. *
	 *------------------------------------------------------------*/

	if(k <= second_room) {
#         ifdef DEBUG
	    if(gctrace > 1) trace_i(269);
#         endif

	  move_entities(second_current, q, k);
	  second_room    -= k;
	  second_current += k;
	  break;
	}

	/*--------------------------------------------------------------*
	 * Otherwise, if the cluster fits into the first block, move it *
	 * there. 							*
	 *--------------------------------------------------------------*/

	if(k <= first_room) {
#         ifdef DEBUG
	    if(gctrace > 1) trace_i(270);
#         endif

	  move_entities(first_current, q, k);
	  first_room    -= k;
	  first_current += k;
	  break;
	}

	/*-----------------------------------------------------------------*
	 * Otherwise, we need a new block. The current first block becomes *
	 * the new second block, and the new block becomes the new         *
	 * first block. Zero out any unused part of second, and add        *
	 * it to the free space lists.                                     *
	 *-----------------------------------------------------------------*/

#       ifdef DEBUG
	  if(gctrace > 1) trace_i(271);
#       endif

	memset(second_current, 0, second_room * sizeof(ENTITY));
	if(second_room > 1) {
	  free_piece_gc(second_current, second_current + second_room);
	}
	used_bytes += (((LONG) second_current) - ((LONG) second->cells));
	s          	= (ENT_BLOCK*) get_avail_block();
	s->next 	= second = first;
	second_current 	= first_current;
	second_room 	= first_room;
	first 		= s;
	first_current 	= first->cells;
	first_room 	= ENT_BLOCK_SIZE;
      }

      /*--------------------------------------------------*
       * Move q beyond the current cluster, and continue. *
       *--------------------------------------------------*/

      q = r;
    }
  }

  /*------------------------------------------------------------*
   * Zero out the unused part of first and second, and add them *
   * to the free space lists.  					*
   *------------------------------------------------------------*/

  memset(second_current, 0, second_room * sizeof(ENTITY));
  if(second_room > 1) {
    free_piece_gc(second_current, second_current + second_room);
  }
  used_bytes += (((LONG) second_current) - ((LONG) second->cells));

  memset(first_current, 0, first_room * sizeof(ENTITY));
  if(first_room > 1) {
    free_piece_gc(first_current, first_current + first_room);
  }
  used_bytes += (((LONG) first_current) - ((LONG) first->cells));

  /*------------------------------------------------------------*
   * Move the old used_blocks chain to relocation_blocks, and 	*
   * set used_blocks to first, which is now the chain of used 	*
   * blocks.							*
   *------------------------------------------------------------*/

  relocation_blocks = used_blocks;
  used_blocks       = first;
}


/********************************************************
 *			MOVE_CHUNK		        *
 ********************************************************
 * Move the binary chunk at source to dest, leaving	*
 * behind a relocation chunk pointing to dest.		*
 ********************************************************/

PRIVATE void move_chunk(CHUNKPTR dest, CHUNKPTR source, int size)
{
# ifdef DEBUG
   if(gctrace > 1) trace_i(247, size, source, dest);
# endif

  memcpy(dest, source, size);
  BINARY_RELOCATE_MARK(source);
  BINARY_UNMARK(dest);
  INSTALL_BINARY_POINTER(source, dest);
}


/****************************************************************
 *		DO_BINARY_COMPACTIFICATION_GC 	        	*
 ****************************************************************
 * do_binary_compactification_gc() moves the currently used    	*
 * binary chunks to fresh blocks, and puts relocation blocks    *
 * where they previously were.  				*
 ****************************************************************/

PRIVATE void do_binary_compactification_gc(void)
{
  BINARY_BLOCK *first, *second, *p, *s;
  register CHUNKPTR q;
  CHUNKPTR endq, first_current, second_current;
  int first_room, second_room, k;

# ifdef DEBUG
    if(gctrace > 1) {
      trace_i(166);
      trace_i(66);
      print_block_chain((BINARY_BLOCK*) used_binary_blocks);
      trace_i(273);
      print_block_chain(avail_blocks);
    }
# endif

  /*------------------------------*
   * Clear the free_chunks table. *
   *------------------------------*/

  for(k = 0; k < FREE_BINARY_SIZE; k++) {
    free_binary_data[k].free_chunks = NULL;
  }

  /*-----------------------------------------------------------------*
   * Get two empty blocks from avail_blocks, or allocate them        *
   * if necessary.  Set first_room to the number of bytes available  *
   * in first, and first_current to the location where new bytes     *
   * will be added to first.  Do similarly for second_room and       *
   * second_current.  When moving a cluster of chunks, we try to     *
   * put in the second block.  If that is not possible, then we put  *
   * in the first block.  That way, the second block remains viable  *
   * as a place to put small clusters when a large cluster comes     *
   * along.  If neither the first nor the second block has room, we  *
   * get a new first block, retire the second block as full, and     *
   * the old first block now the second block.			     *
   *-----------------------------------------------------------------*/

  first          = get_avail_block();
  first_current  = first->cells;
  second         = get_avail_block();
  second_current = second->cells;
  second->next   = NULL;
  first->next    = second;
  first_room     = second_room = BINARY_BLOCK_SIZE;

  /*------------------------------------------------------------*
   * Scan used_binary_blocks, moving chunks to the free blocks. *
   *------------------------------------------------------------*/

  for(p = used_binary_blocks; p != NULL; p = p->next) {
    q    = p->cells;
    endq = q + BINARY_BLOCK_SIZE;

    /*--------------------------------------------------*
     * Scan through the chunks, skipping over 		*
     * unmarked chunks and relocating marked ones.	*
     *--------------------------------------------------*/

    while(q < endq) {
      register USHORT is_marked = BINARY_MARKED(q);
      register CHUNKHEADPTR qq;
      register USHORT nn, size;

      qq = (CHUNKHEADPTR) q;
      BINARY_UNMARK(q);
      nn = *qq;  		/* Nominal size of this chunk */

      /*----------------------------------------------------------*
       * size is the actual size of this chunk, including	  *
       * the header.  The allocator puts 16380 in nn if the size  *
       * is 4, and 16381 if the size is 8.			  *
       *----------------------------------------------------------*/

      if(nn >= 16380) {
	size = (nn == 16380) ? 4 : 8;
      }
      else {
        size = nn + SHORT_BYTES;
	if(size < CHUNK_MIN_SIZE) size = CHUNK_MIN_SIZE;
	while((size & TRUE_LONG_ALIGN) != 0) size++;
      }

#     ifdef DEBUG
        if(gctrace > 1) trace_i(246, size, is_marked, q);
#     endif

      /*---------------------------------*
       * Move the chunk if it is marked. *
       *---------------------------------*/

      if(is_marked) {

	/*------------------------------------------------*
	 * The following loop goes through at most twice. *
	 *------------------------------------------------*/

	for(;;) {

	  /*----------------------------------------------------------*
	   * If this chunk fits into the second block, move it there. *
	   *----------------------------------------------------------*/

	  if(size <= second_room) {
#           ifdef DEBUG
	      if(gctrace > 1) trace_i(269);
#           endif

	    move_chunk(second_current, q, size);
	    second_room    -= size;
	    second_current += size;
	    break;
	  }

	  /*-------------------------------------------------------------*
	   * Otherwise, if this chunk fits into the first block, move it *
	   * there. 							   *
	   *-------------------------------------------------------------*/

	  if(size <= first_room) {
#           ifdef DEBUG
	      if(gctrace > 1) trace_i(270);
#           endif

	    move_chunk(first_current, q, size);
	    first_room    -= size;
	    first_current += size;
	    break;
	  }

	  /*-----------------------------------------------------------------*
	   * Otherwise, we need a new block. The current first block becomes *
	   * the new second block, and the new block becomes the new         *
	   * first block. Add any unused part of the first block             *
	   * to the free space lists.                                        *
	   *-----------------------------------------------------------------*/

#         ifdef DEBUG
	    if(gctrace > 1) trace_i(271);
#         endif

	  if(second_room >= CHUNK_MIN_SIZE) {
	    free_binary_piece_gc(second_current, second_current + second_room);
	  }
	  used_bytes += (((LONG) second_current) - ((LONG) second->cells));
	  s              = get_avail_block();
	  s->next        = second = first;
	  second_current = first_current;
	  second_room    = first_room;
	  first 	   = s;
	  first_current  = first->cells;
	  first_room     = BINARY_BLOCK_SIZE;
	} /* end for(;;) */
      } /* end if(is_marked) */

      q += size;

    } /* end while(q < endq) */
  } /* end for(p = used_binary_blocks...) */

  /*--------------------------------------------*
   * Add the unused part of first and second 	*
   * to the free space lists.  			*
   *--------------------------------------------*/

  if(second_room >= CHUNK_MIN_SIZE) {
    free_binary_piece_gc(second_current, second_current + second_room);
  }
  used_bytes += (((LONG) second_current) - ((LONG) second->cells));

  if(first_room >= CHUNK_MIN_SIZE) {
    free_binary_piece_gc(first_current, first_current + first_room);
  }
  used_bytes += (((LONG) first_current) - ((LONG) first->cells));

  /*-------------------------------------------------------------*
   * Move the old used_blocks chain to relocation_binary_blocks, *
   * and set used_binary_blocks to first, which is now the chain *
   * of used binary blocks.					 *
   *-------------------------------------------------------------*/

  relocation_binary_blocks = used_binary_blocks;
  used_binary_blocks       = first;
}


/***********************************************************
 *			RELOCATE_GC		           *
 ***********************************************************
 * Relocate cell q, if it points to a RELOCATE_TAG entity. *
 * That is, replace *q by an entity that refers to the     *
 * new location of what *q used to refer to.               *
 *							   *
 * NOTE: inlined in do_relocation_gc, below.		   *
 ***********************************************************/

void relocate_gc(ENTITY *q)
{
  register ENTITY qval = *q;
  register int    tag  = GCTAG(qval);

  /*------------------------------------------------------*
   * Check for a pointer to an entity.  It might point to *
   * a cell that has a RELOCATE_TAG value in it.	  *
   *------------------------------------------------------*/

  if(MEMBER(tag, entp_tags)) {
    if(tag != INDIRECT_TAG || VAL(qval) != 0) {
      register ENTITY* p = ENTVAL(qval);
      register ENTITY pval = *p;
      if(GCTAG(pval) == RELOCATE_TAG) {

#       ifdef DEBUG
	  if(gctrace > 1) {
	    trace_i(274, p, ENTVAL(pval), tag);
	  }
#       endif

	*q = ENTP(tag, ENTVAL(pval));
      }
    }
  }

  /*-----------------------------------------------------*
   * Check for a pointer to a binary chunk.  		 *
   *-----------------------------------------------------*/

  else if(MEMBER(tag, binary_ptr_tags)) {
    register CHUNKPTR chunk = BIVAL(qval);
    if(BINARY_RELOCATED(chunk)) {
      register CHUNKPTR new_loc = BINARY_POINTER_AT(chunk);

#     ifdef DEBUG
	if(gctrace > 1) {
	  trace_i(274, chunk, new_loc, tag);
	}
#     endif

      *q = ENTP(tag, new_loc);
    }
  }

  /*-----------------------------------------------------*
   * Check for a box.  The box value might have changed. *
   *-----------------------------------------------------*/

  else if(tag == BOX_TAG) {
    relocate_box_gc(q);
  }
}


/***********************************************************
 *			RELOCATE_PTR_GC		           *
 ***********************************************************
 * Relocate cell *q, if it points to a RELOCATE_TAG entity.*
 * This function differs from relocate_gc in that q is     *
 * a pointer to a pointer to a entity here.                *
 ***********************************************************/

void relocate_ptr_gc(ENTITY **q)
{
  register ENTITY* r = *q;
  if(r == NULL) return;

  {register ENTITY rval = *r;
   if(GCTAG(rval) == RELOCATE_TAG) {
     *q = ENTVAL(rval);
   }
  }
}


/********************************************************
 *			RELOCATE_ENTS_LIST_GC 	        *
 ********************************************************
 * Relocate all pointers in list l.                     *
 ********************************************************/

PRIVATE void relocate_ents_list_gc(ENTS_LIST *l)
{
  ENTS_LIST *p;
  for(p = l; p != NIL; p = p->tail) {
    relocate_ptr_gc(&(p->head.ents));
  }
}


/********************************************************
 *			RELOCATE_MONO_TABLE_GC 	        *
 ********************************************************
 * Relocate the entities stored in the tables stored    *
 * in outer_bindings.                                   *
 ********************************************************/

PRIVATE void reloc_mono(HASH2_CELLPTR h)
{
  relocate_gc(&(h->val.entity));
}

/*----------------------------------------------------------------*/

PRIVATE void relocate_mono_table_gc(void)
{
  int i;

# ifdef DEBUG
    if(gctrace > 1) trace_i(279);
# endif

  for(i = 0; i < next_ent_num; i++) {
    scan_hash2(outer_bindings[i].mono_table, reloc_mono);
  }
}


/****************************************************************
 *			DO_RELOCATION_GC 	        	*
 ****************************************************************
 * do_relocation_gc replaces each entity in the heap            *
 * that has been relocated by its new value, pointing to        *
 * the new location. It also unmarks each entity.               *
 *                                                              *
 * do_relocation_gc also relocates entities that are            *
 * in the constant table, those that are defined                *
 * in entity.c, and those that have been registered             *
 * with the garbage collector in rts_ents and rts_pents.        *
 *                                                              *
 * do_relocation_gc is not concerned with entities that         *
 * are in stacks, states or environments.  They are relocated   *
 * during the final unmark phase (unmark_gc).                   *
 ****************************************************************/

PRIVATE void do_relocation_gc(void)
{
  int i;
  ENT_BLOCK *p;
  ENTITY *q, *endq;

  /*------------------------------*
   * Do rellocations of the heap. *
   *------------------------------*/

  for(p = used_blocks; p != NULL; p = p->next) {
    for(q = p->cells, endq = q + ENT_BLOCK_SIZE; q < endq; q++) {
      GCUNMARK(q);

      /*----------------------------------------*
       * relocate_gc(q), inlined for efficiency *
       *----------------------------------------*/

      {register ENTITY qval = *q;
       register int tag     = GCTAG(qval);
       if(MEMBER(tag, entp_tags)) {
	 if(tag != INDIRECT_TAG || ENT_FNE(qval, NOTHING)) {
	   register ENTITY *r = ENTVAL(qval);
	   register ENTITY rval = *r;
	   if(GCTAG(rval) == RELOCATE_TAG) {

#            ifdef DEBUG
	       if(gctrace > 1) {
		trace_i(274, r, ENTVAL(rval), tag);
	       }
#            endif

	     *q = ENTP(tag, ENTVAL(rval));
	   }
	 }
       }

       else if(MEMBER(tag, binary_ptr_tags)) {
         register CHUNKPTR chunk = BIVAL(qval);
         if(BINARY_RELOCATED(chunk)) {
           register CHUNKPTR new_loc = BINARY_POINTER_AT(chunk);

#          ifdef DEBUG
	     if(gctrace > 1) {
	       trace_i(274, chunk, new_loc, tag);
	     }
#          endif

           *q = ENTP(tag, new_loc);
         }
       }

       else if(tag == BOX_TAG) {
	 relocate_box_gc(q);
       }

      }
    }
  }

  /*-----------------------------*
   * Do relocations of rts_ents. *
   *-----------------------------*/

# ifdef DEBUG
    if(gctrace > 1) trace_i(280);
# endif

  for(i = 0; i < num_rts_ents; i++) {
    relocate_gc(rts_ents[i]);
  }

  /*------------------------------*
   * Do relocations of rts_pents. *
   *------------------------------*/

# ifdef DEBUG
    if(gctrace > 1) trace_i(281);
# endif

  for(i = 0; i < num_rts_pents; i++) {
    relocate_ptr_gc(rts_pents[i]);
  }

  /*---------------------*
   * Relocate constants. *
   *---------------------*/

# ifdef DEBUG
    if(gctrace > 1) trace_i(282);
# endif

  for(i = 0; i < next_const; i++) {
    relocate_gc((ENTITY *)(constants + i));
  }

  /*-----------------------------*
   * Relocate standard entities. *
   *-----------------------------*/

# ifdef DEBUG
    if(gctrace > 1) trace_i(283);
# endif

  relocate_gc(&failure_as_entity);
  relocate_gc(&last_exception);
  relocate_gc(&fail_ex);
  relocate_gc(&a_large_int);
  relocate_gc(&zero_rat);
  relocate_gc(&one_rat);
  relocate_gc(&ten_rat);
  relocate_gc(&zero_real);
  relocate_gc(&one_real);
  relocate_gc(&ten_real);
  relocate_gc(&half_real);
  relocate_gc(&dollar_fixp_low);
  relocate_gc(&infloop_timeout);
  relocate_gc(&large_one_real);
  relocate_gc(&large_ten_real);
  relocate_gc(&newline);
  relocate_gc(&stdout_file);
  relocate_gc(&stderr_file);
  relocate_gc(&divide_by_zero_ex);
  for(i = 0; i < TEN_TO_MAX; i++) {
    relocate_gc(ten_to_p + i);
  }

  /*-------------------------------*
   * Relocate constants in real.c. *
   *-------------------------------*/

  relocate_gc(&ln_base_val);

  /*-------------------*
   * Relocate globals. *
   *-------------------*/

  relocate_mono_table_gc();
}


/*=============================================================
			UTILITIES
  =============================================================
  The following are utilities for the garbage collector.
 **************************************************************/

/********************************************************
 *			GC_ALLOC		        *
 ********************************************************
 * Allocate n bytes for the garbage collector.          *
 *							*
 * These bytes must be allocated from space that is	*
 * dedicated to the garbage collector, or there will	*
 * be chaos.						*
 ********************************************************/

void* gc_alloc(int n)
{
  GC_BLOCK *p;
  char *result;

  if(gc_this_block_bytes_left < n) {
    if(gc_free_blocks == NULL) {
      p = (GC_BLOCK *) alloc(sizeof(GC_BLOCK));
      p->next     = gc_blocks;
      gc_blocks   = p;
      gc_free_ptr = p->cells;
    }
    else {
      gc_free_ptr    = gc_free_blocks->cells;
      gc_free_blocks = gc_free_blocks->next;
    }
    gc_this_block_bytes_left = GC_BLOCK_SIZE;
  }

  result = gc_free_ptr;
  gc_free_ptr += n;
  gc_this_block_bytes_left -= n;
  return (void *) result;
}


/*********************************************************
 *			GC_CONS			         *
 *********************************************************
 * Return cons(h,t), but use gc_alloc to get the memory. *
 *********************************************************/

LIST* gc_cons(char *h, LIST *t)
{
  LIST* c     = (LIST *) gc_alloc(sizeof(LIST));
  c->kind     = 0;
  c->head.str = h;
  c->tail     = t;
  return c;
}


/***********************************************************
 *			GCTAG			           *
 ***********************************************************
 * Return the tag of e, but error if e is marked.  NOTE:   *
 * This is NOT the GCTAG macro.  Rather, it is used in the *
 * TAG macro.  Yes, confusing.   It should probably be     *
 * renamed.                                                *
 ***********************************************************/

#ifdef GCTEST

int gctag(ENTITY e)
{
  int tag = GCTAG(e);
  if(in_gc) return tag;
  if(tag == RELOCATE_TAG) die(36);
  if(MARKED(e)) {
    trace_i(171, tag);
    in_gc = TRUE;
    trace_print_entity(e);
    tracenl();
    die(37);
  }
  return tag;
}

#endif

/********************************************************
 *		SUPPRESS_COMPACTIFY_STDF		*
 ********************************************************
 * Suppress_compactification if x is true, and turn	*
 * compactification on if x is false.  Return hermit.	*
 ********************************************************/

ENTITY suppress_compactify_stdf(ENTITY x)
{
  if(VAL(x)) suppress_compactify++;
  else if(suppress_compactify > 0) suppress_compactify--;
  return hermit;
}


/********************************************************
 *			INIT_GC			        *
 ********************************************************
 * Set up the arrays used for registering entities and  *
 * entity pointers.                                     *
 ********************************************************/

void init_gc()
{
  rts_ents       = (ENTITY **) alloc(RTS_ENTS_INIT_SIZE * sizeof(ENTITY *));
  rts_ents_size  = RTS_ENTS_INIT_SIZE;
  rts_pents      = (ENTITY ***) alloc(RTS_PENTS_INIT_SIZE * sizeof(ENTITY **));
  rts_pents_size = RTS_PENTS_INIT_SIZE;
}
