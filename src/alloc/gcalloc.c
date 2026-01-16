/**************************************************************
 * File:    alloc/gcalloc.c
 * Purpose: Free space allocators and dallocators for types that 
 *          require an interface to the garbage collector, and
 *          support functions.
 * Author:  Karl Abrahamson
 **************************************************************/

#ifdef COPYRIGHT
/************************************************************************
 * Copyright (c) 2000 Karl Abrahamson					*
 * All rights reserved.							*
 * See file COPYRIGHT.txt for details.					*
 ************************************************************************/
#endif

#include <memory.h>
#ifdef USE_MALLOC_H
#  include <malloc.h>
#else
#  include <stdlib.h>
#endif
#include "../misc/misc.h"
#include "../gc/gc.h"
#include "../intrprtr/intrprtr.h"
#include "../evaluate/evaluate.h"
#include "../machdata/entity.h"
#include "../machdata/except.h"
#include "../machdata/gc.h"
#include "../alloc/allocate.h"
#include "../show/printrts.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif


/****************************************************************
 *			CHECK_HEAP_SIZE				*
 ****************************************************************
 * Check space utilization, and query user if it is too high.   *
 *								*
 * If the user wants to continue, then either double the heap   *
 * limit or set it to LONG_MAX, as requested.			*
 ****************************************************************/

void check_heap_size(void)
{
  if(heap_bytes > max_heap_bytes) {
    char message[140];
#   ifdef MSWIN
      sprintf(message,
	      "Limit of %ld bytes on heap size has been reached.\n"
	      "Continue?",
	      heap_bytes);
#   else
      sprintf(message,
	      "\nLimit of %ld bytes on heap size has been reached.\n"
	      "Use -h<n> option to astr to set limit to <n>\n"
	      "Use -h+ to remove limit\n",
	      heap_bytes);
#   endif
    max_heap_bytes += max_heap_bytes;
    if(possibly_abort_dialog(message)) {
      max_heap_bytes = LONG_MAX;
    }
  }
}


/****************************************************************
 *			CALL_FOR_COLLECT			*
 ****************************************************************
 * Handle the case where get_before_gc has gone to 0 or below   *
 * by asking evaluate to do a garbage collection.		*
 *								*
 * call_for_collect should only be called if get_before_gc <= 0.*
 ****************************************************************/

PRIVATE void call_for_collect(void)
{
  /*------------------------------------------------------------------*
   * alloc_phase is set to 1 to indicate that the system is in the    *
   * second phase of allocation, where we are waiting for evaluate    *
   * to do a garbage collection.  perform_gc is set to tell evaluate  *
   * to do the gc.  special_condition must be incremented to get      *
   * evaluate to look at perform_gc.                                  *
   *------------------------------------------------------------------*/

  if(!alloc_phase && !perform_gc) {
    perform_gc  = 1;
    alloc_phase = 1;
    special_condition++;
    get_before_gc = get_before_gc_reset;
  }
}


/*==============================================================*
 *			REALS					*
 *==============================================================*
 *								*
 * Doubles are kept in "small real" blocks.  Here are the 	*
 * variables that control allocation and deallocation of	*
 * doubles.							*
 *								*
 * free_small_reals  -- Points to a chain of available doubles, *
 *			linked though their first word.  All of *
 *			the doubles in the chain are in the 	*
 *			small real blocks.			*
 *								*
 * small_real_blocks -- A chain of blocks holding all doubles.	*
 *			This chain is used in garbage collection*
 *			to traverse all of the small real	*
 *			blocks.					*
 *								*
 * small_reals_since_last_gc -- 				*
 *			The number of small reals allocated 	*
 *			since the last garbage collection.  	*
 *			This is	used to decide whether 		*
 *			allocation of a small real should 	*
 *			trigger a garbage collection.		*
 ****************************************************************/

SMALL_REAL*       free_small_reals          = NULL;
SMALL_REAL_BLOCK* small_real_blocks         = NULL;
LONG              small_reals_since_last_gc = 0;


/**********************************************************************
 *			ALLOCATE_SMALL_REAL_BLOCK		      *
 **********************************************************************
 * Allocate a block that will be used to allocate double-precision    *
 * real numbers.  The block contains mark bits for garbage collection *
 * as well as the doubles themselves.                                 *
 **********************************************************************/

PRIVATE void allocate_small_real_block(void)
{
  int i;
  SMALL_REAL_BLOCK *p;
  SMALL_REAL *s;

# ifdef DEBUG
    if(gctrace || alloctrace) {
      trace_i(0, get_before_gc, alloc_phase);
    }
#   endif

  /*--------------------------------------------------------------*
   * Possibly call for garbage collection. Only garbage collect   *
   * if at least a few small reals have been allocated since the  *
   * last garbage collection, so there might be something to get. *
   *--------------------------------------------------------------*/

  if(get_before_gc <= 0
     && small_reals_since_last_gc >= SMALL_REAL_GET_NUM
     && small_real_blocks != NULL) {
    call_for_collect();
  }

  /*-----------------------------------------------------------------*
   * Get the small real block. It should be aligned according to     *
   * DBL_BLOCK_ALIGN, to make it easy to compute the location of the *
   * mark word.							     *
   *-----------------------------------------------------------------*/

  p                 = (SMALL_REAL_BLOCK *)
		      bd_alloc(sizeof(SMALL_REAL_BLOCK), DBL_BLOCK_ALIGN);
  p->marks	    = 0;
  p->next           = small_real_blocks;
  small_real_blocks = p;

  /*-------------------------------------------------------------*
   * Link the small reals in the block into the free list chain. *
   *-------------------------------------------------------------*/

  for(i = 0; i < SMALL_REAL_BLOCK_SIZE; i++) {
    s                = p->cells + i;
    s->next          = free_small_reals;
    free_small_reals = s;
  }

  /*---------------------------------*
   * Indicate allocation and return. *
   *---------------------------------*/

  heap_bytes += sizeof(SMALL_REAL_BLOCK);
  check_heap_size();
}


/****************************************************************
 *			ALLOCATE_SMALL_REAL			*
 ****************************************************************
 * Allocate a small real (a double-precision real).             *
 ****************************************************************/

SMALL_REAL* allocate_small_real(void)
{
  SMALL_REAL *s;

  /*--------------------------------------------*
   * Get a new block if the free list is empty. *
   *--------------------------------------------*/

  if(free_small_reals == NULL) {
    allocate_small_real_block();
  }

  /*----------------------------------*
   * Grab and return the free memory. *
   *----------------------------------*/

  s                = free_small_reals;
  free_small_reals = s->next;
  get_before_gc   -= sizeof(SMALL_REAL);
  small_reals_since_last_gc++;
  return s;
}


/*=======================================================================*
 *			ENTITIES					 *
 ========================================================================*
 * Entities are allocated in blocks.  Each block has BLOCK_SIZE entities.*
 *									 *
 * Each block is internally carved up into subblocks.			 *
 * Also, the block might have free parts and used parts.  		 *
 *									 *
 * Variables are as follows.  They are shared only with the	 	 *
 * garbage collector (gc/gc.c) and with debug/m_debug.c.                 *
 *                                                                       *
 *  used_blocks  	-- Points to a chain of blocks currently in      *
 *			   use.  This is needed for garbage collection,  *
 *			   to make it possible to traverse all of the    *
 *			   entities that are in use.			 *
 *                                                                       *
 * free_ent_data indicates where the free subblocks can be found. 	 *
 * It stores multiple free-space lists, arranged by subblock size.	 *
 *								         *
 *  free_ent_data[i].free_ents_size					 *
 *			-- The minimum size, in entities, of the         *
 *                         subblocks that are stored in chain		 *
 *			   free_binary_data.free_ents[i].		 *
 *                                                                       *
 *  free_ent_data[i].free_ents 						 *
 *			-- A pointer to a chain of subblocks.    	 *
 *			   Each array in chain free_ent_data[i].free_ents*
 *			   is guaranteed to have at least		 *
 *			   free_ent_data[i].free_ents_size entities in	 *
 *			   it. The first entity in a subblock in         *
 *			   free_ent_data[i].free_ents is a link to the	 *
 *			   next subblock in the list, or contains	 *
 *			   false_ent if it is the last subblock in the	 *
 *			   chain.  The second entity in the subblock is	 *
 *			   the size of this subblock (as an ENTITY).     *
 *                                                                       *
 *  free_ent_data[i].where_ent 						 *
 *			-- This is the smallest j such that  j >= i and	 *
 *			   free_ent_data[j].free_ents is not null.       *
 *                         This is where to look for free space of size  *
 *                         at least free_ent_data[i].free_ents_size.	 *
 *			   If all chains are null, then 		 *
 * 			   free_ent_data[i].where_ent is		 *
 *                         FREE_ENTS_SIZE - 1 for all i.		 *
 *                                                                       *
 *  ents_since_last_gc  -- The number of entities allocated since the    *
 *                         last garbage collection.  This is used to     *
 *			   decide whether to do a garbage collection.    *
 *************************************************************************/

ENT_BLOCK* 		used_blocks        = NULL;
LONG       		ents_since_last_gc = 0;
FREE_ENT_DATA FARR 	free_ent_data[FREE_ENTS_SIZE];


/****************************************************************
 *			SET_WHERES				*
 ****************************************************************
 * set_wheres() sets the free_ent_data[].where_ent array to	*
 * correct indices.   See above for a description of that array.*
 ****************************************************************/

void set_wheres(void)
{
  int last, i;

  last = FREE_ENTS_SIZE - 1;
  free_ent_data[last].where_ent = last;
  for(i = last - 1; i >= 0; i--) {
    if(free_ent_data[i].free_ents != NULL) last = i;
    free_ent_data[i].where_ent = last;
  }
}


/****************************************************************
 *			PRINT_ENTITY_WHERES			*
 ****************************************************************
 * Print information about available entity clusters for	*
 * debugging.							*
 ****************************************************************/

#ifdef DEBUG
void print_entity_wheres(void)
{
  ENTITY *ent;
  int i;
  LONG n;

  trace_i(63);
  for(i = 0; i < FREE_ENTS_SIZE; i++) {
    ent = free_ent_data[i].free_ents;
    n = 0;
    while(ent != NULL) {
      n++;
      if(ENT_EQ(ent[0], false_ent)) ent = NULL;
      else ent = ENTVAL(ent[0]);
    }
    fprintf(TRACE_FILE, "%6d%12ld%10d\n", i, n, free_ent_data[i].where_ent);
  } /* end for(i = ...) */
}
#endif


/*****************************************************************
 *			ALLOCATE_BLOCK				 *
 *****************************************************************
 * Allocate_block allocates a new entity block 	 		 *
 * and places it into the free space list for entities.          *
 *****************************************************************/

PRIVATE void allocate_block(void)
{
  ENT_BLOCK *p;
  ENTITY *q;

  /*-------------------------------------------------------------------*
   * Possibly call for garbage collection.  Be sure that enough        *
   * entities have been allocated since the last garbage collection to *
   * make this worth  while.  (It is possible that other kinds of      *
   * things have been allocated instead.)                              *
   *-------------------------------------------------------------------*/

  if(get_before_gc <= 0
     && ents_since_last_gc >= ENT_GET_NUM
     && used_blocks != NULL) {
    call_for_collect();
  }

# ifdef DEBUG
    if(alloctrace || gctrace) {
      trace_i(2, get_before_gc, alloc_phase);
      trace_i(65);
      print_block_chain((BINARY_BLOCK*) used_blocks);
    }
# endif

  /*-----------------------------------------------------*
   * Get the new block, and add it to chain used_blocks. *
   *-----------------------------------------------------*/

  p           = (ENT_BLOCK*) get_avail_block();
  p->next     = used_blocks;
  used_blocks = p;
  check_heap_size();

  /*--------------------------------------------------------------*
   * Put the new block into the free space chain in the slot for  *
   * largest blocks.  The first cell in the block holds the chain *
   * link, which is set to false to indicate the end of a chain.  *
   * The second cell holds the actual size of this block.         *
   *--------------------------------------------------------------*/

  q    = free_ent_data[FREE_ENTS_SIZE - 1].free_ents = p->cells;
  q[0] = false_ent;
  q[1] = ENTU(ENT_BLOCK_SIZE);

  /*-----------------------------------------------------------------*
   * Initialize the entities to false.  This is important, since     *
   * the garbage collector might need to scan this block later, and  *
   * should not be confused by uninitialized cells.                  *
   *-----------------------------------------------------------------*/

  memset(q + 2, 0, (ENT_BLOCK_SIZE - 2) * sizeof(ENTITY));

  /*----------------------------------------*
   * Set up the where_ent array and return. *
   *----------------------------------------*/

  set_wheres();

# ifdef DEBUG
    if(alloctrace || gctrace) {
      trace_i(65);
      print_block_chain((BINARY_BLOCK*) used_blocks);
    }
# endif
}


/****************************************************************
 *			ALLOCATE_ENTITY				*
 ****************************************************************
 * Return a pointer to an array of N entities.                  *
 ****************************************************************/

ENTITY* allocate_entity(LONG n)
{
  ENTITY HUGEPTR result;
  register ENTITY *q;
  register LONG s;
  register int w;

  /*--------------------------------------------------------------------*
   * Entity arrays are only allocated up to a maximum size.  The 	*
   * interpreter should never ask for a larger array, but check		*
   * anyway.								*
   *--------------------------------------------------------------------*/

  if(n > ENT_BLOCK_GRAB_SIZE) die(1);

# ifdef DEBUG
    if(alloctrace) {
      trace_i(3, n);
      print_entity_wheres();
    }
# endif

  /*---------------------------------------------------------------*
   * Locate an appropriate block.  Allocate a new one if there is  *
   * no appropriate one.  w is the index of the appropriate block. *
   *---------------------------------------------------------------*/

  {register int i;
   for(i = 0; free_ent_data[i].free_ents_size < n; i++) /* Nothing */;
   w = free_ent_data[i].where_ent;
  }
  if(free_ent_data[w].free_ents == NULL) {
    allocate_block();
    w = FREE_ENTS_SIZE - 1;
  }
  result = free_ent_data[w].free_ents;

  /*----------------------------------------------------------------*
   * Get the reduced size s of this subblock, and pointer q to what *
   * is left after removing the first n entities.		    *
   *----------------------------------------------------------------*/

  s = NRVAL(result[1]) - n;         /* size of what is left. */
  q = result + n;                   /* points to what is left */

  /*-----------------------------------*
   * Update garbage collection counts. *
   *-----------------------------------*/

  get_before_gc      -= n * sizeof(ENTITY);
  ents_since_last_gc += n;

# ifdef DEBUG
    if(alloctrace) {
      trace_i(5, w, s, free_ent_data[w].free_ents, q, result);
     }
# endif

  /*----------------------------------------------------------*
   * Move the rest of this block to an appropriate free space *
   * chain and return free space.                             *
   *----------------------------------------------------------*/

  if(s >= free_ent_data[w].free_ents_size) {

    /*-----------------------------------------------*
     * Leave this block where it is, but shorten it. *
     *-----------------------------------------------*/

    free_ent_data[w].free_ents = q;
    q[0] = result[0];
    q[1] = ENTU(s);
    return result;
  }
  else {

    /*-------------------------------------------------------------*
     * Adjust the free space pointer for this list, and adjust the *
     * where_ent if necessary.                                     *
     *-------------------------------------------------------------*/

    if(ENT_EQ(result[0], false_ent)) {
      free_ent_data[w].free_ents = NULL;
      if(w < FREE_ENTS_SIZE - 1) {
	register int k = w;
	register int new_w = free_ent_data[w+1].where_ent;
	while(k >= 0 && free_ent_data[k].free_ents == NULL) {
	  free_ent_data[k].where_ent = new_w;
	  k--;
	}
      }
    }
    else {
      free_ent_data[w].free_ents = ENTVAL(result[0]);
    }

    /*--------------------------------------------------*
     * Move this block to a different free space list,  *
     * if it is large enough. 				*
     *--------------------------------------------------*/

    if(s >= 2) {
      register int j, k;
      for(j = w; j > 0 && free_ent_data[j].free_ents_size > s; j--) {
	/* Nothing */
      }
      q[0] = (free_ent_data[j].free_ents == NULL) 
	        ? false_ent 
                : ENTP(INDIRECT_TAG, free_ent_data[j].free_ents);
      q[1] = ENTU(s);
      free_ent_data[j].free_ents = q;
      free_ent_data[j].where_ent = j;
      for(k = j - 1; k >= 0 && free_ent_data[k].free_ents == NULL; k--) {
	free_ent_data[k].where_ent = j;
      }
    }
    return result;
  }
}


/*=======================================================================*
 *			BINARY DATA					 *
 ========================================================================*
 * Binary data is allocated in blocks.  There are two kinds of binary    *
 * blocks, normal and huge.  Each normal block has BINARY_BLOCK_SIZE     *
 * bytes, and is internally carved up into chunks.   Each huge block has *
 * just one (very large) chunk in it.  Huge blocks are used for large    *
 * binary arrays.		 			 		 *
 *									 *
 * Each chunk begins with a header of one of the following two forms.	 *
 *									 *
 *   (Form 1) The header consists of a single USHORT integer.  We take   *
 *   16 bits in this short.  The leftmost 2 bits are mark bits and the   *
 *   rightmost 14 bits are the size of this chunk, in bytes, excluding   *
 *   the bytes occupied by the header.  This size must be no more than   *
 *   0x3ffb.								 *
 *									 *
 *   (Form 2) The header consists of three USHORT integers, 16 bits each.*
 *   The leftmost two bits of the first of those integers are mark bits. *
 *   The right-hand 14 bits of the first integer contain 0x3fff to       *
 *   signal form 2.  The next two short integers hold the size of this   *
 *   chunk, in bytes, excluding the header.  The size is stored with the *
 *   low order bits in the first integer and high order bits in the next *
 *   integer.								 *
 *									 *
 * The mark bits are 0 except during garbage collection.		 *
 *									 *
 * NOTE: When a chunk is in the free space list, its size (in the 	 *
 * header) is the actual physical size.  The physical size including the *
 * header is always a multiple of LONG_BYTES.   When a chunk is 	 *
 * allocated, its size is the nominal size (the amount requested), which *
 * might not lead to a multiple of LONG_BYTES when the size of the header*
 * is added.  That is corrected for when deallocating a chunk.		 *
 *									 *
 * NOTE: The size of a chunk in a normal block must always be less than  *
 * 16380 (0x3ffc).  Sizes 0x3ffc and 0x3ffd are used to mark unallocated *
 * chunks that are too small to store a pointer, and so cannot be linked *
 * into the free space chain.						 *
 *									 *
 *-----------------------------------------------------------------------*
 *									 *
 * At any given time, some of the chunks in a normal binary block are	 *
 * used, and some are free.	 					 *
 * free_binary_data[k].free_chunks is a chain of chunks, each of size	 *
 * at least free_binary_data[k].free_chunks_size.  Each pointer in	 *
 * the chain points to a chunk, with the four bytes just after the chunk *
 * header pointing to the next chunk in the chain.			 *
 *									 *
 * Variables are as follows.  They are shared only with the	 	 *
 * garbage collector (gc/gc.c) and with debug/m_debug.c.                 *
 *                                                                       *
 *  used_binary_blocks  -- Points to a chain of binary blocks currently  *
 *			   in use.  This is needed for garbage		 *
 *			   collection, to make it possible to traverse   *
 *			   all of the allocated chunks.			 *
 *                                                                       *
 *  avail_blocks	-- Points to chain of blocks that are not        *
 *			   currently in use.  These blocks are used	 *
 * 			   both for entity and binary blocks.            *
 *                                                                       *
 *  free_binary_data[i].free_chunks_size				 *
 *			-- The minimum size, in bytes, of the            *
 *                         subblocks that are stored in chain		 *
 *			   free_binary_data.free_chunks[i].  This	 *
 *			   is the size excluding the header.		 *
 *                                                                       *
 *  free_binary_data[i].free_chunks					 *
 *			-- A pointer to a chain of subblocks.    	 *
 *			   Each chunk in chain				 *
 *			   free_binary_data[i].free_chunks is		 *
 *			   guaranteed to have at least			 *
 *			   free_binary_data[i].free_chunks_size		 *
 *			   data bytes (not counting the two byte 	 *
 * 			   header).  The link is in the four bytes 	 *
 *			   just after the header.			 *
 *                                                                       *
 *  free_binary_data[i].where 						 *
 *			-- the smallest j such that  j >= i and		 *
 *			   free_binary_data[j].free_chunks is not null.  *
 *                         This is where to look for free space of size  *
 *                         at least free_binary_data[i].free_chunks_size.*
 *			   If all chains are null, then 		 *
 * 			   free_binary_data[i].where is			 *
 *                         FREE_BINARY_SIZE-1 for all i.		 *
 *                                                                       *
 *  binary_bytes_since_last_gc  					 *
 *			-- The number of bytes allocated in binary	 *
 * 			   blocks since the last garbage collection.     *
 *			   It is used to decide whether a garbage 	 *
 *			   collection might be productive.		 *
 *									 *
 *  huge_binaries_allocated						 *
 *			-- This points to an array of huge_binaries_-	 *
 *			   allocated_size entries, each of which is 	 *
 *			   either NULL or points to a huge binary block	 *
 *			   of memory that was allocated using malloc.	 *
 *									 *
 *  huge_binaries_allocated_size					 *
 *			-- This is the size of array			 *
 *			   huge_binaries_allocated			 *
 *									 *
 *  huge_binaries_since_last_gc						 *
 *  			-- This is the number of huge binary blocks that *
 *			   have been allocated since the most recent 	 *
 *			   garbage collection.				 *
 *************************************************************************/

BINARY_BLOCK* 		used_binary_blocks 	     = NULL;
BINARY_BLOCK* 		avail_blocks                 = NULL;
LONG      		binary_bytes_since_last_gc   = 0;
FREE_BINARY_DATA FARR 	free_binary_data[FREE_BINARY_SIZE];
CHUNKPTR*		huge_binaries_allocated      = NULL;
int			huge_binaries_allocated_size = 0;
LONG			huge_binaries_since_last_gc  = 0;


/****************************************************************
 *			BINARY_CHUNK_SIZE			*
 ****************************************************************
 * binary_chunk_size(CHUNK) is the nomimal data size, in bytes, *
 * of binary chunk CHUNK.					*
 *								*
 * NOTE: This function should not be called during garbage	*
 * collection, since it assumes that the mark bits are 0.	*
 ****************************************************************/

LONG binary_chunk_size(CHUNKPTR chunk)
{
  register CHUNKHEADPTR ch   = (CHUNKHEADPTR) chunk;
  register LONG         size = *ch;
  if(size == 0x3fff) {
    size = ch[1] + (((LONG)(ch[2])) << SHORT_BITS);
  }
  return size;
}


/****************************************************************
 *			BINARY_CHUNK_BUFF			*
 ****************************************************************
 * binary_chunk_buff(CHUNK) is a pointer to the byte buffer of  *
 * chunk CHUNK.  The buffer is found just after the header.   *
 *								*
 * NOTE: This function should not be called during garbage	*
 * collection, since it assumes that the mark bits are 0.	*
 ****************************************************************/

charptr binary_chunk_buff(CHUNKPTR chunk)
{
  register CHUNKHEADPTR ch   = (CHUNKHEADPTR) chunk;
  register USHORT       size = *ch;
  if(size == 0x3fff) {
    return chunk + 3*SHORT_BYTES;
  }
  else {
    return chunk + SHORT_BYTES;
  }
}


/****************************************************************
 *			SET_BINARY_WHERES			*
 ****************************************************************
 * set_binary_wheres() sets the free_binary_data[].where array 	*
 * to correct indices.    					*
 ****************************************************************/

void set_binary_wheres(void)
{
  int last, i;

  last = FREE_BINARY_SIZE - 1;
  free_binary_data[last].where = last;
  for(i = last - 1; i >= 0; i--) {
    if(free_binary_data[i].free_chunks != NULL) last = i;
    free_binary_data[i].where = last;
  }
}


/****************************************************************
 *			PRINT_BINARY_WHERES			*
 ****************************************************************
 * Print information about free binary blocks for debugging.	*
 ****************************************************************/

#ifdef DEBUG
void print_binary_wheres(void)
{
  int i;
  trace_i(11);
  for(i = 0; i < FREE_BINARY_SIZE; i++) {
    CHUNKPTR q;
    int j;
    fprintf(TRACE_FILE, "%2d %8d %5d %8p", i, 
	   free_binary_data[i].free_chunks_size,
	   free_binary_data[i].where,
           free_binary_data[i].free_chunks);
    q = free_binary_data[i].free_chunks;
    j = 0;
    while(q != NULL && j < 3) {
      CHUNKHEADPTR qq = (CHUNKHEADPTR) q;
      fprintf(TRACE_FILE, "(%d) ", *qq);
      q = BINARY_POINTER_AT(q);
      fprintf(TRACE_FILE, "%8p", q);
      j++;
    }
    tracenl();
  }
  tracenl();
}
#endif


/********************************************************
 *			PRINT_BLOCK_CHAIN		*
 ********************************************************
 * Print the addresses of the blocks in the chain 	*
 * starting at CHAIN.					*
 ********************************************************/

#ifdef DEBUG
void print_block_chain(BINARY_BLOCK *chain)
{
  BINARY_BLOCK *pp;

  for(pp = chain; tolong(pp) > 100; pp = pp->next) {
    fprintf(TRACE_FILE,"%p-%p ", pp, ((char*)pp)+sizeof(BINARY_BLOCK));
  }
  fprintf(TRACE_FILE,"[end %p]\n", pp);
}
#endif


/*********************************************************
 *			GET_AVAIL_BLOCK		         *
 *********************************************************
 * Get an available normal binary block, either from 	 *
 * chain  avail_blocks, or from alloc.  This block can   *
 * be used either as a binary block or as an entity	 *
 * block.						 *
 *********************************************************/

BINARY_BLOCK* get_avail_block(void)
{
  BINARY_BLOCK *result;

  if(avail_blocks != NULL) {
    result       = avail_blocks;
    avail_blocks = result->next;
  }
  else {
#   ifdef SMALL_ENTITIES
      LONG offset;
#   endif

    result = (BINARY_BLOCK *) alloc(sizeof(BINARY_BLOCK));

#   ifdef SMALL_ENTITIES
      offset = ((char*) result) - heap_base;
      if(offset < 0 || offset + sizeof(BINARY_BLOCK) > MAX_SMALLENT_ADDRESS) {
        die(141);
      }
#   endif
  }

  heap_bytes += sizeof(BINARY_BLOCK);
  return result;
}


/*****************************************************************
 *			ALLOCATE_BINARY_BLOCK			 *
 *****************************************************************
 * Allocate_binary_block allocates a new normal binary block and *
 * places its memory into the free space list for binary data.   *
 *								 *
 * This function should only be used when there is no chunk      *
 * of maximum size available.  It sets the chain at		 *
 * free_binary_data[FREE_BINARY_SIZE - 1].free_chunks to a 	 *
 * singleton chain holding the newly allocated block, so	 *
 * anything in that chain would be lost.			 *
 *****************************************************************/

PRIVATE void allocate_binary_block(void)
{
  BINARY_BLOCK *p;
  CHUNKPTR q;
  CHUNKHEADPTR qq;

  /*-------------------------------------------------------------------*
   * Possibly call for garbage collection.  Be sure that enough has    *
   * been allocated since the last garbage collection to make this     *
   * worth while.  (It is possible that other kinds of things have     *
   * been allocated instead.)                                          *
   *-------------------------------------------------------------------*/

  if(get_before_gc <= 0
     && binary_bytes_since_last_gc >= BINARY_GET_NUM 
     && used_binary_blocks != NULL) {
    call_for_collect();
  }

# ifdef DEBUG
    if(alloctrace || gctrace) {
      trace_i(9, get_before_gc, alloc_phase);
      trace_i(66);
      print_block_chain(used_binary_blocks);
    }
# endif

  /*------------------------------------------------------------*
   * Get the new block, and add it to chain used_binary_blocks. *
   * Check to see that we have not allocated too much space in  *
   * the heap.							*
   *------------------------------------------------------------*/

  p       	     = get_avail_block();
  p->next     	     = used_binary_blocks;
  used_binary_blocks = p;
  check_heap_size();

  /*--------------------------------------------------------------*
   * The new block initially holds just one chunk.  Put the new	  *
   * chunk into the free space chain in the slot for largest	  *
   * chunks.  The first SHORT in the block holds actual   	  *
   * size of this chunk, not counting the header.  The next two   *
   * or four shorts hold a pointer to the next block in this 	  *
   * free-chunk chain.  We set the pointer to NULL to indicate    *
   * that it is at the end of the chain.			  *
   *--------------------------------------------------------------*/

  q     = free_binary_data[FREE_BINARY_SIZE - 1].free_chunks = p->cells;
  qq    = (CHUNKHEADPTR) q;
  qq[0] = BINARY_BLOCK_SIZE - SHORT_BYTES;  /* Header has form 1. */
# if (PTR_BYTES <= 2*SHORT_BYTES) 
    qq[1] = qq[2] = 0;
# else
    qq[1] = qq[2] = qq[3] = qq[4] = 0;
# endif

  /*------------------------------------*
   * Set up the where array and return. *
   *------------------------------------*/

  set_binary_wheres();

# ifdef DEBUG
    if(alloctrace || gctrace) {
      trace_i(345);
      print_block_chain(used_binary_blocks);
    }
# endif
}


/****************************************************************
 *			FREE_HUGE_BINARY			*
 ****************************************************************
 * Free huge binary chunk P.					*
 ****************************************************************/

void free_huge_binary(CHUNKPTR p)
{
  int i;

  /*---------------------------------------------------------*
   * Freeing this chunk requires removing its block from the *
   * huge_binaries_allocated chain.			     *
   *							     *
   * Only put half of this chunk's size back when keeping    *
   * track of get_before_gc, since large chunks can otherwise*
   * have an undue effect on garbage collection.	     *
   *---------------------------------------------------------*/

  for(i = 0; i < huge_binaries_allocated_size; i++) {
    if(huge_binaries_allocated[i] == p) {
      LONG chunk_size = 
	     STRING_SIZE(huge_binaries_allocated[i]) + 3*SHORT_BYTES;
      huge_binaries_allocated[i] = NULL;
      heap_bytes                 -= chunk_size;
      get_before_gc              += chunk_size >> 1;
      huge_binaries_since_last_gc--;
      break;
    }
  }

  FREE(p);
}


/****************************************************************
 *			INSTALL_HUGE_BINARY			*
 ****************************************************************
 * Add pointer P to the pointers that are being remembered	*
 * as huge binary arrays that have been allocated using malloc.	*
 ****************************************************************/

PRIVATE void install_huge_binary(CHUNKPTR p)
{
  int i;

  tail_recur:

  /*------------------------------------*
   * Try to install into an empty cell. *
   *------------------------------------*/

  for(i = 0; i < huge_binaries_allocated_size; i++) {
    if(huge_binaries_allocated[i] == NULL) {
      huge_binaries_allocated[i] = p;
      return;
    }
  }

  /*------------------------------------------*
   * If not possible, then we need more room. *
   *------------------------------------------*/

  {int new_size;
   if(huge_binaries_allocated_size == 0) {
     new_size = 16;
     huge_binaries_allocated = 
       (CHUNKPTR*) BAREMALLOC(new_size*sizeof(CHUNKPTR));
   }
   else {
     new_size = 2*huge_binaries_allocated_size;
     huge_binaries_allocated = 
       (CHUNKPTR*) BAREREALLOC(huge_binaries_allocated,
			       new_size*sizeof(CHUNKPTR));
   }
   memset(huge_binaries_allocated + huge_binaries_allocated_size, 0, 
	  (new_size - huge_binaries_allocated_size) * sizeof(CHUNKPTR));
   huge_binaries_allocated_size = new_size;
   goto tail_recur;
  }
}


/****************************************************************
 *			ALLOCATE_HUGE_BINARY			*
 ****************************************************************
 * Return a pointer to a binary chunk that has 			*
 * data size N bytes.			        		*
 ****************************************************************/

PRIVATE CHUNKPTR allocate_huge_binary(LONG n)
{
  LONG true_size = n + 3*SHORT_BYTES;
  CHUNKHEADPTR m = (CHUNKHEADPTR) BAREMALLOC(true_size);

  /*------------------------------------------------------------*
   * Install the block size.  The first short int holds 0x3fff	*
   * to indicate a large chunk.					*
   *								*
   * Only record half of the chunk size in get_before_gc to 	*
   * reduce the effect of large chunks on garbage collection.	*
   *------------------------------------------------------------*/

  m[0] = 0x3fff;
  m[1] = (USHORT)(n & USHORT_MASK);
  m[2] = (USHORT)((n >> SHORT_BITS) & USHORT_MASK);

  install_huge_binary((CHUNKPTR) m);
  heap_bytes += true_size;
  check_heap_size();
  get_before_gc -= true_size >> 1;
  if(get_before_gc <= 0 
     && huge_binaries_since_last_gc > 0) {
    call_for_collect();
  }
  huge_binaries_since_last_gc++;

  return (CHUNKPTR) m;
}


/****************************************************************
 *			FREE_CHUNK				*
 ****************************************************************
 * Free chunk c.						*
 ****************************************************************/

void free_chunk(CHUNKPTR c)
{
  CHUNKHEADPTR chead = (CHUNKHEADPTR) c;

# ifdef DEBUG
   if(alloctrace) {
     trace_i(44, c, STRING_SIZE(c));
     if(alloctrace > 1) print_binary_wheres();
   }
# endif

  /*--------------------------------------------------------*
   * If c is a huge chunk, use free_huge_binary to free it. *
   *--------------------------------------------------------*/

  if(*chead == 0x3fff) free_huge_binary(c);

  /*----------------------------------------------------*
   * If c is a smaller chunk, put it back into the free *
   * space list for its size.				*
   *----------------------------------------------------*/

  else {
    register int i;
    Boolean was_null;

    /*----------------------------------------------------*
     * Calculate the size of this chunk.		  *
     *   The physical size, including the header, is put  *
     *   in nplus.					  *
     *							  *
     *   The physical size of the buffer, not counting	  *
     *   the header, is put in n.			  *
     *							  *
     * Note that the size, *chead, is the nominal size,   *
     * and might need to be corrected upwards so that the *
     * total size, including the header, is a multiple of *
     * LONG_BYTES.					  *
     *----------------------------------------------------*/

    USHORT          n     = *chead;
    register USHORT nplus = n + SHORT_BYTES;

    if(nplus < CHUNK_MIN_SIZE) nplus = CHUNK_MIN_SIZE;
    while((nplus & TRUE_LONG_ALIGN) != 0) nplus++;
    *chead = n = nplus - SHORT_BYTES;
   
    /*--------------------------------------------------*
     * Modify garbage collection counts.  Since		*
     * freeing a chunk can increase fragmentation, we   *
     * only count half of the chunk as being put back.	*
     *--------------------------------------------------*/

    {int half_chunk              = nplus >> 1;
     get_before_gc 		+= half_chunk;
     binary_bytes_since_last_gc -= half_chunk;
    }

    /*--------------------------------------------------*
     * Find the appropriate place to insert this chunk  *
     * in the free space lists.				*
     *--------------------------------------------------*/

    for(i = 0;
	free_binary_data[i].free_chunks_size < n;
        i++) {
     /* Nothing */
    }
    if(free_binary_data[i].free_chunks_size > n) i--;

    /*----------------------------------------*
     * Add this chunk to the free space list. *
     *----------------------------------------*/

    was_null = free_binary_data[i].free_chunks == NULL;
    INSTALL_BINARY_POINTER(c, free_binary_data[i].free_chunks);
    free_binary_data[i].free_chunks = c;

    /*---------------------------------------------------*
     * If this free space list was null, then adjust the *
     * where pointers to indicate that something is here *
     * now.						 *
     *---------------------------------------------------*/

    if(was_null) {
      register int j;
      for(j = i; j >= 0 && free_binary_data[j].where > i; j--) {
	free_binary_data[j].where = i;
      }
    }
  }

# ifdef DEBUG
    if(alloctrace > 1) {
      trace_i(343);
      print_binary_wheres();
    }
# endif

}


/****************************************************************
 *			ALLOCATE_BINARY				*
 ****************************************************************
 * Return a pointer to a binary chunk that has at least N       *
 * data bytes.  Such a chunk always starts at a long word	*
 * boundary.							*
 *								*
 * The size that is indicated in the chunk will be exactly N.	*
 ****************************************************************/

CHUNKPTR allocate_binary(LONG n)
{
  /*------------------------------------------------------------*
   * If this is a very large array, allocate as such.  Don't 	*
   * try to put in a block.					*
   *------------------------------------------------------------*/

  if(n + SHORT_BYTES > BINARY_BLOCK_GRAB_SIZE) {
#   ifdef DEBUG
      if(alloctrace) trace_i(10, n);
#   endif

    return allocate_huge_binary(n);
  }

  /*------------------------------------------------------------*
   * If this is not a very large array, allocate in a block.	*
   *------------------------------------------------------------*/

  else {

    /*------------------------------------------------------------------*
     * We must allocate at least CHUNK_MIN_SIZE bytes, since we		*
     * need room for a pointer after the header.   Also, the total size,*
     * including header, must be a multiple of LONG_BYTES.		*
     *									*
     * n_data is the actual number of data bytes allocated, including   *
     * any padding bytes at the end, but excluding the header,		*
     *									*
     * n_data_plus is n_data plus the size of the header.		*
     *------------------------------------------------------------------*/

    CHUNKPTR result, new_start;
    CHUNKHEADPTR result_as_head_ptr;
    LONG new_size, new_size_minus_header, n_data;
    int w;

    LONG n_data_plus = max(n + SHORT_BYTES, CHUNK_MIN_SIZE);
    while((n_data_plus & TRUE_LONG_ALIGN) != 0) n_data_plus++;
    n_data = n_data_plus - SHORT_BYTES;

#   ifdef DEBUG
      if(alloctrace) {
        trace_i(7, n, n_data);
	if(alloctrace > 1) print_binary_wheres();
      }
#   endif

    /*---------------------------------------------------------------*
     * Locate an appropriate block.  Allocate a new one if there is  *
     * no appropriate one.  w is the index of the appropriate block. *
     *---------------------------------------------------------------*/

    {register int i;
     for(i = 0;
	 free_binary_data[i].free_chunks_size < n_data;
	 i++) {
       /* Nothing */
     }
     w = free_binary_data[i].where;
    }

    /*------------------------------------------------------------*
     * If there is nothing here, then get a new block.  Then the  *
     * result is at the front of the free_chunks chain for this   *
     * free_binary_data entry.					  *
     *------------------------------------------------------------*/

    if(free_binary_data[w].free_chunks == NULL) {
      allocate_binary_block();
      w = FREE_BINARY_SIZE - 1;
    }
    result = free_binary_data[w].free_chunks;

    /*----------------------------------------------------------------*
     * Get the reduced size new_size of this subblock, and pointer    *
     * new_start to what is left after removing the first 	      *
     * n_data_plus bytes.					      *
     *								      *
     * new_size is the number of bytes left over in this chunk.	      *
     *								      *
     * new_size_minus_header is the size of what is left over, minus  *
     * the size of the header.			       	              *
     *								      *
     * new_start is where the left over part starts.		      *
     *----------------------------------------------------------------*/

    result_as_head_ptr    = (CHUNKHEADPTR) result;
    new_size              = *result_as_head_ptr - n_data; 
    new_size_minus_header = new_size - SHORT_BYTES;
    new_start             = result + n_data_plus;

    /*-----------------------------------*
     * Update garbage collection counts. *
     *-----------------------------------*/

    get_before_gc              -= n_data_plus;
    binary_bytes_since_last_gc += n_data_plus;

#   ifdef DEBUG
      if(alloctrace) {
        trace_i(5, w, new_size, free_binary_data[w].free_chunks, 
	        new_start, result);
      }
#   endif

    /*-------------------------------------------------*
     * Install nominal size information for the chunk  *
     * being allocated.				       *
     *-------------------------------------------------*/

    *result_as_head_ptr = (USHORT) n;

    /*----------------------------------------------------------*
     * Move the rest of this block to an appropriate free space *
     * chain and return free space.                             *
     *----------------------------------------------------------*/

    if(new_size_minus_header >= free_binary_data[w].free_chunks_size) {

      /*-----------------------------------------------*
       * Leave this block where it is, but shorten it. *
       *-----------------------------------------------*/

      *((CHUNKHEADPTR)(new_start))    = new_size_minus_header;
      free_binary_data[w].free_chunks = new_start;
      COPY_BINARY_POINTER(new_start, result);
    }

    else /* Must move to another index */ {

      /*--------------------------------------------------------------*
       * Adjust the free space pointer for this chain, and adjust the *
       * where index if necessary.                                    *
       *--------------------------------------------------------------*/

      if(BINARY_POINTER_IS_NULL(result)) {

        /*--------------------------------------------------*
         * Chain w becomes NULL now.  We need to adjust the *
	 * where index.				            *
         *--------------------------------------------------*/

	free_binary_data[w].free_chunks = NULL;
	if(w < FREE_BINARY_SIZE - 1) {
	  register int k     = w;
	  register int new_w = free_binary_data[w+1].where;
	  while(k >= 0 && free_binary_data[k].free_chunks == NULL) {
	    free_binary_data[k].where = new_w;
	    k--;
	  }
	}
      }
      else {
	free_binary_data[w].free_chunks = BINARY_POINTER_AT(result);
      }

      /*--------------------------------------------------*
       * Move this block to a different free space list,  *
       * if it is large enough. 			  *
       *--------------------------------------------------*/

      if(new_size >= CHUNK_MIN_SIZE) {
        register int j, k;
	CHUNKPTR r;
	for(j = w; 
	    j > 0
             && free_binary_data[j].free_chunks_size > new_size_minus_header; 
	    j--) {
	  /* Nothing */
	}

        *((CHUNKHEADPTR)(new_start)) = new_size_minus_header;
	r = free_binary_data[j].free_chunks;
	INSTALL_BINARY_POINTER(new_start, r);
	free_binary_data[j].free_chunks = new_start;
	free_binary_data[j].where = j;

	for(k = j - 1; 
	    k >= 0 && free_binary_data[k].free_chunks == NULL; 
	    k--) {
	  free_binary_data[k].where = j;
	}
      }
 
      else /* The remainder is too small */ {

	/*----------------------------------------------------------*
         * When we have a piece that is too small to hold a pointer *
	 * (for linking into the free space chain) then we mark it  *
         * with a special size.  Size 16380 (0x3ffc) indicates a    *
         * chunk of size 4, and 16381 (0x3ffd) a chunk of size 8.   *
	 *----------------------------------------------------------*/

	if(new_size > 0) {
	  if(new_size == 4) {
            *((CHUNKHEADPTR)(new_start)) = 16380;
          }
	  else if(new_size == 8) {
            *((CHUNKHEADPTR)(new_start)) = 16381;
	  }
	  else die(164, new_size);
	}

      }
    } /* end else(must move to another index) */

#   ifdef DEBUG
      if(alloctrace) {
	trace_i(344, result);
	if(alloctrace > 1) print_binary_wheres();
      }
#   endif

    return result;

  } /* end else(not huge) */
}


/*======================================================================*
 *			FILE RECORDS					*
 =======================================================================*
 * Free file records are kept in a free-space chain.  Space is          *
 * allocated in blocks. Variables are as follows.                       *
 *                                                                      *
 * free_file_entities   -- Available structures of type file_entity.    *
 *									*
 * file_ent_blocks      -- Chain of used blocks.  This is used to find  *
 * 			   all of the file_entity structures that are 	*
 *			   in use.  					*
 *									*
 * files_since_last_gc  -- The number of file_entity structures that    *
 *                         have been allocated since the last garbage   *
 *                         collection.                                  *
 ************************************************************************/

struct file_entity* 	free_file_entities = NULL;
struct file_ent_block* 	file_ent_blocks = NULL;
LONG 			files_since_last_gc = 0;

/****************************************************************
 *			ALLOC_FILE_ENTITY_BLOCK			*
 ****************************************************************
 * Allocate a new block for files.                              *
 ****************************************************************/

PRIVATE void alloc_file_entity_block(void)
{
  struct file_entity *x;
  struct file_ent_block *p;
  int i;

  /*---------------------------------------*
   * Possibly call for garbage collection. *
   *---------------------------------------*/

  if(get_before_gc <= 0 
     && files_since_last_gc >= FILE_GET_NUM 
     && file_ent_blocks != NULL) {
    call_for_collect();
  }

  /*--------------------------------------------------------------*
   * Get a new block, add its components to the free space chain, *
   * and add it to the list of used file blocks.                  *
   *--------------------------------------------------------------*/

  p 		  = alloc(sizeof(struct file_ent_block));
  p->next 	  = file_ent_blocks;
  file_ent_blocks = p;
  heap_bytes     += sizeof(struct file_ent_block);
  check_heap_size();

  for(i = 0; i < FILE_ENT_BLOCK_SIZE; i++) {
    x                  = p->blk + i;
    x->u.next          = free_file_entities;
    x->kind            = NO_FILE_FK;
    x->descr_index     = -1;
    free_file_entities = x;
  }
}
  

/****************************************************************
 *			ALLOC_FILE_ENTITY			*
 ****************************************************************
 * Return a new file node.                                      *
 ****************************************************************/

struct file_entity* alloc_file_entity(void)
{
  struct file_entity *x;

  /*------------------------------------*
   * Allocate a new block if necessary. *
   *------------------------------------*/

  if(free_file_entities == NULL) {
    alloc_file_entity_block();
  }

  /*----------------------------------*
   * Get the node, and record for gc. *
   *----------------------------------*/

  x = free_file_entities;
  free_file_entities = free_file_entities->u.next;
  get_before_gc -= sizeof(struct file_entity);
  files_since_last_gc++;
  x->mark = 0;
  x->descr_index = -1;
  return x;
}


/****************************************************************
 *		        FREE_FILE_ENTITY			*
 ****************************************************************
 * Free file entity node F.                                     *
 ****************************************************************/

void free_file_entity(struct file_entity *f)
{
  f->kind = NO_FILE_FK;
  f->descr_index = -1;

# ifndef GCTEST
    f->u.next = free_file_entities;
    free_file_entities = f;
    files_since_last_gc--;
# else
    f->mark = 100;
# endif
}


/*===================================================================*
 *			GC_INIT_ALLOC				     *
 ====================================================================*
 * Initialize the structures used to allocate memory that is managed *
 * by the garbage collector.                                         *
 *********************************************************************/

void gc_init_alloc(void)
{
  int i;
  LONG s;

  get_before_gc = GET_BEFORE_GC_INIT;

  /*------------------------------------------------------*
   * Entities. The sizes for the free space lists are in  *
   * powers of 2.  The smallest has 2 entities, since     *
   * that many are needed for the bookkeeping of the	  *
   * allocator.						  *
   *------------------------------------------------------*/

  s = 2;
  for(i = 0; i < FREE_ENTS_SIZE; i++) {
    free_ent_data[i].free_ents      = NULL;
    free_ent_data[i].where_ent 	    = 0;
    free_ent_data[i].free_ents_size = s;
    s += s;
  }
  free_ent_data[FREE_ENTS_SIZE - 1].free_ents_size = ENT_BLOCK_SIZE;
  allocate_block();

  /*--------------*
   * Binary Data. *
   *--------------*/

  s = CHUNK_MIN_SIZE;
  for(i = 0; i < FREE_BINARY_SIZE; i++) {
    free_binary_data[i].free_chunks 	 = NULL;
    free_binary_data[i].where 		 = 0;
    free_binary_data[i].free_chunks_size = s - SHORT_BYTES;
    s += s;
  }
  free_binary_data[FREE_BINARY_SIZE - 1].free_chunks_size 
	= BINARY_BLOCK_SIZE - SHORT_BYTES;
  allocate_binary_block();
}



