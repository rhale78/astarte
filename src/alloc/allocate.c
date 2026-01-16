/**************************************************************
 * File:    alloc/allocate.c
 * Purpose: Basic free space management routines.  These routines
 *          allocate and parcel out blocks of memory.
 * Author:  Karl Abrahamson
 **************************************************************/

#ifdef COPYRIGHT
/************************************************************************
 * Copyright (c) 2000 Karl Abrahamson					*
 * All rights reserved.							*
 * See file COPYRIGHT.txt for details.					*
 ************************************************************************/
#endif

/****************************************************************
 * The functions defined here form the basic free space       	*
 * management functions.  We do our own, for tighter control  	*
 * than using malloc directly.  Also, the functions here      	*
 * check for errors.					      	*
 *								*
 * Two options are provided.  If USE_SBRK is defined, the 	*
 * memory allocation is done using the sbrk system call.  	*
 * If USE_MALLOC is defined, memory allocation is done using 	*
 * malloc.  One of those preprocessor symbols must be defined.  *
 ****************************************************************/

#include "../misc/misc.h"
#include <memory.h>
#ifdef USE_MALLOC_H
#  include <malloc.h>
#else
#  include <stdlib.h>
#endif
#include "../alloc/allocate.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

#ifdef USE_SBRK
# ifdef UNIX
#  include <unistd.h>
# else
   void* sbrk(int n);
# endif
#endif


/********************************************************
 * 			VARIABLES			*
 ********************************************************/

/************************************************************/
/* The following are used when option USE_SBRK is selected. */
/************************************************************/

#ifdef USE_SBRK

/********************************************************
 *			next_avail_byte			*
 *			end_avail_space			*
 ********************************************************
 * NEXT_AVAIL_BYTE is a pointer to the next byte to get,*
 * in a block of available memory that goes up to, but	*
 * not including, end_avail_space. 			*
 ********************************************************/

PRIVATE charptr next_avail_byte = NULL;

PRIVATE charptr end_avail_space; 

/********************************************************
 *			heap_base			*
 ********************************************************
 * HEAP_BASE is a pointer to the first byte in the	*
 * heap.  It is used as a base for pointers stored	*
 * in entities in the small entity implementation.	*
 * (sment.h).  In that model, pointers are stored as an *
 * offset from heap_base.				*
 ********************************************************/

charptr heap_base = NULL;

#endif


/*************************************************************/
/* The following are used when option USE_MALLOC is selected */
/*************************************************************/

#ifdef USE_MALLOC

/********************************************************
 *			alloc_chain			*
 ********************************************************
 * When USE_MALLOC is used, the allocated blocks are	*
 * chained together.  When the program terminates, all	*
 * allocated blocks are freed (using free) by		*
 * traversing this chain. 				*
 *							*
 * ALLOC_CHAIN points to the first block in the chain.	*
 ********************************************************/

PRIVATE void* alloc_chain = NULL; 
#endif


/*******************************************************************/
/* The following are used for both USE_SBRK and USE_MALLOC options */
/*******************************************************************/

/********************************************************
 *			heap_bytes			*
 ********************************************************
 * HEAP_BYTES is the number of bytes actually in use by *
 * the program from the heap.  This does not count	*
 * blocks that have been allocated but are currently	*
 * fallow.						*
 ********************************************************/

LONG heap_bytes = 0;            

/********************************************************
 *			small_block			*
 *			small_block_size		*
 *			alt_small_blocks		*
 ********************************************************
 * SMALL_BLOCK is a pointer to a block that is used to	*
 * allocate small structures.  It actually points to	*
 * the next byte to allocate.				*
 *							*
 * SMALL_BLOCK_SIZE is the number of remaining bytes	*
 * in small_block. 					*
 *							*
 * ALT_SMALL_BLOCKS is a chain of blocks that are	*
 * available for small blocks. Each block contains, 	*
 * at its top, its size and a link to the next block	*
 * in the chain.					*
 ********************************************************/

PRIVATE charptr small_block      = NULL; 

PRIVATE charptr alt_small_blocks = NULL;

PRIVATE LONG    small_block_size = 0;

/********************************************************
 *			ALLOC_DIE			*
 ********************************************************
 * Die a horrible death because memory ran out.         *
 ********************************************************/

PRIVATE void alloc_die(void)
{
  die(141);
}


/****************************************************************
 *			INIT_ALLOC				*
 ****************************************************************
 * INIT_ALLOC must be called before any storage allocation      *
 * is done, and should only be called once.                     *
 ****************************************************************/

#ifdef USE_SBRK
void init_alloc()
{
  heap_base = next_avail_byte = (charptr) sbrk(MEM_BLOCK_SIZE);
  if(tolong(heap_base) < 0) alloc_die();
  end_avail_space = next_avail_byte + MEM_BLOCK_SIZE;
  init_hash_alloc();
}
#endif

/*------------------------------------------------------------*/

#ifdef USE_MALLOC
void init_alloc()
{
  init_hash_alloc();
}
#endif


/****************************************************************
 *			ALLOC					*
 ****************************************************************
 * Return a pointer to new space of N bytes.  The space will    *
 * begin on a long word boundary.   				*
 ****************************************************************/

#ifdef USE_SBRK
void* alloc(SIZE_T n)
{
  return bd_alloc(n, LONG_ALIGN);
}
#endif

/*------------------------------------------------------------*/

#ifdef USE_MALLOC
void* alloc(SIZE_T n)
{
  register void *s;

  /*--------------------------------------------------------------------*
   * Get from the small block if a small amount of memory is requested. *
   *--------------------------------------------------------------------*/

  if(n < ALLOC_SMALL_MAX) return bd_alloc_small(toint(n), LONG_ALIGN);

  /*--------------------------------------------------------------------*
   * For large blocks, use malloc, and add the block to alloc_chain. 	*
   * Four extra bytes are allocated to hold the chain pointer. 		*
   *--------------------------------------------------------------------*/

# if (MALLOC_ALIGN >= LONG_ALIGN)
    s = MALLOC(n + PTR_BYTES);
# else
    s = MALLOC(n + PTR_BYTES + LONG_ALIGN);
    while((((LONG)(s)) & LONG_ALIGN) != 0) ((charptr)(s))++;
# endif

  if(s == NULL) alloc_die();
  *((void **) s) = alloc_chain;
  alloc_chain    = s;
  return (void *) (((char *)s) + PTR_BYTES);
}
#endif


/****************************************************************
 *			BD_ALLOC				*
 ****************************************************************
 * Return a pointer to new space of N bytes.  The space will    *
 * begin on an address whose 'and' with MASK is 0.              *
 * This is used to get space aligned on a double-word           *
 * boundary, etc.                                               *
 ****************************************************************/

#ifdef USE_SBRK
void* bd_alloc(SIZE_T n, LONG mask)
{
  register charptr s;

  if(n < ALLOC_SMALL_MAX && mask <= ALLOC_SMALL_MAX) {
    return bd_alloc_small(toint(n), mask);
  }

  /*-----------------------------------------------------------*
   * The space will be obtained from the block that begins at  *
   * next_avail_byte, if there is enough there.  Start by      *
   * bumping next_avail_byte up to an appropriate boundary.    *
   *-----------------------------------------------------------*/

  while((tolong(next_avail_byte) & mask) != 0) next_avail_byte++;

  /*----------------------------------------------------------------*
   * If there is not enough room at next_avail_byte, then get more  *
   * memory until there is enough, or memory is exhausted.          *
   *----------------------------------------------------------------*/

  while(tolong(end_avail_space) - tolong(next_avail_byte) < tolong(n)) {
    if((tolong(s = (charptr) sbrk(MEM_BLOCK_SIZE))) < 0) alloc_die();
    if(end_avail_space != s) {
      next_avail_byte = s;
      while((tolong(next_avail_byte) & mask) != 0) next_avail_byte++;
    }
    end_avail_space = s + MEM_BLOCK_SIZE;
  }

  /*-----------------------------*
   * Grab and return the memory. *
   *-----------------------------*/

  s = next_avail_byte;
  next_avail_byte += n;
  return (void *) s;
}
#endif

/*------------------------------------------------------------*/

#ifdef USE_MALLOC
void* bd_alloc(SIZE_T n, LONG mask)
{
  register charptr s;

  if(n < ALLOC_SMALL_MAX && mask <= ALLOC_SMALL_MAX) {
    return bd_alloc_small(n, mask);
  }
  s = (charptr) alloc(n + mask);
  while((tolong(s) & mask) != 0) s++;
  return (void *) s;
}
#endif


/****************************************************************
 *			FREE_ALL_BLOCKS				*
 ****************************************************************
 * Free all of the blocks allocated by MALLOC, by traversing    *
 * alloc_chain.  This is only defined when using malloc.        *
 ****************************************************************/

#ifdef USE_MALLOC
void free_all_blocks(void)
{
  void* q;
  void* p = alloc_chain;

  while(p != NULL) {
    q = *((void **) p);
    FREE(p);
    p = q;
  }
}
#endif


/****************************************************************
 *			ALLOC_SMALL				*
 *			BD_ALLOC_SMALL				*
 ****************************************************************
 * Allocate a block of N bytes from the small memory block.     *
 * For alloc_small, the space will start on a long word 	*
 * boundary.  For bd_alloc_small, the space will begin on a	*
 * boundary whose 'and' with MASK is 0.				*
 ****************************************************************/

void* bd_alloc_small(int n, LONG mask)
{
  void *result;

  /*--------------------------------*
   * Align at a long word boundary. *
   *--------------------------------*/

  while((tolong(small_block) & mask) != 0) {
    small_block++;
    small_block_size--;
  }

  /*-------------------------------------------------------------*
   * Possibly get a new block.  Get it from alt_small_blocks, if *
   * available, or from alloc if not.                            *
   *-------------------------------------------------------------*/

  while(small_block == NULL || small_block_size < n) {
    if(alt_small_blocks != NULL) {
      small_block      = alt_small_blocks;
      small_block_size =           ((LONG *) alt_small_blocks) [1];
      alt_small_blocks = (charptr) ((LONG *) alt_small_blocks) [0];
    }
    else {
      small_block      = (charptr) alloc(SMALL_BLOCK_ALLOC_SIZE);
      small_block_size = SMALL_BLOCK_ALLOC_SIZE;
      break;
    }
  }

  /*-------------------------------*
   * Grab the block and return it. *
   *-------------------------------*/

  result            = small_block;
  small_block      += n;
  small_block_size -= n;
  return result;
}

/*-----------------------------------------------------------*/

void* alloc_small(int n)
{
  return bd_alloc_small(n, LONG_ALIGN);
}


/****************************************************************
 *		       REALLOCATE				*
 ****************************************************************
 * Move the contents of the M bytes starting at P to            *
 * new memory of N bytes.  (Or leave where they are if          *
 * N bytes are available without moving.)  Return a pointer to  *
 * the new location.			  			*
 *								*
 * The new memory is aligned on a long word boundary.		*
 *								*
 * If REUSE is true, then the old space can be reclaimed for	*
 * small blocks.        					*
 ****************************************************************/

#ifdef USE_SBRK
void* reallocate(charptr p, SIZE_T m, SIZE_T n, Boolean reuse)
{
  charptr s;
  Boolean move = FALSE;

  /*--------------------------------------------------------------*
   * If the memory to be reallocated is at the edge of the heap,  *
   * then it might be possible to reallocate by simply grabbing   *
   * some more memory for it.                                     *
   *--------------------------------------------------------------*/

  if(p + m == next_avail_byte) {
    while(!move &&
	  (tolong(end_avail_space - next_avail_byte)) < tolong(n - m)) {
      if((tolong(s = (charptr) sbrk(MEM_BLOCK_SIZE))) < 0) alloc_die();
      if(end_avail_space != s) {
	next_avail_byte = s;
	while((tolong(next_avail_byte) & LONG_ALIGN) != 0) next_avail_byte++;
	move = TRUE;
      }
      end_avail_space = s + MEM_BLOCK_SIZE;
    }
    if(!move) {
      next_avail_byte += n - m;
      return (void *) p;
    }
  }

  /*------------------------------------------------------------------*
   * If it was not possible to extend this memory at the edge of the  *
   * heap, then get new memory, and copy.                             *
   *------------------------------------------------------------------*/

  s = (charptr) alloc(n);
  longmemcpy(s, p, m);

  /*-----------------------------------------*
   * Use the former memory for small blocks, *
   * if allowed. 			     *
   *-----------------------------------------*/

  if(reuse) give_to_small(p,m);

  return s;
}
#endif

/*------------------------------------------------------------*/

#ifdef USE_MALLOC

void* reallocate(char *p, SIZE_T m, SIZE_T n, Boolean reuse)
{
  void *s;

  /*---------------------------------------------------------*
   * Get new memory, copy the old memory in, and use the old *
   * memory for small blocks.                                *
   *---------------------------------------------------------*/

  s = MALLOC(n);
  if(s == NULL) alloc_die();
  longmemcpy(s,p,m);
  if(reuse) give_to_small(p,m);
  return s;
}
#endif


/********************************************************
 *		       GIVE_TO_SMALL			*
 ********************************************************
 * Make the memory block starting at address P and M    *
 * bytes long a block of memory available for small     *
 * blocks.                                              *
 ********************************************************/

void give_to_small(charptr p, SIZE_T m)
{
# ifdef SMALL_ENTITIES

    /*---------------------------------------------------------------*
     * Reject if in low memory.  This is necessary for the small     *
     * entity representation, since otherwise pointer offsets would  *
     * be negative, and the small representation scheme cannot       *
     * handle negative offsets from heap_base.                       *
     *---------------------------------------------------------------*/

    if(p < heap_base) return;
# endif

  /*--------------------------------*
   * Align at a long word boundary. *
   *--------------------------------*/

  while(tolong(p) & LONG_ALIGN) {p++; m--;}
  while(m & LONG_ALIGN) m--;

  /*----------------------------------------------------------*
   * If still large enough, add it to alt_small_blocks chain. *
   *----------------------------------------------------------*/

  if(m >= 2*LONG_BYTES) {
    ((LONG *) p)[0] = (LONG) alt_small_blocks;
    ((LONG *) p)[1] = m;
    alt_small_blocks = p;
  }
}


/********************************************************
 *		       BARE_MALLOC			*
 *		       BARE_REALLOC			*
 ********************************************************
 * Same as MALLOC and REALLOC, but die if result is	*
 * NULL.  These should be used instead of malloc and	*
 * realloc, so that it is not necessary to test every   *
 * use for null result.					*
 ********************************************************/

void* bare_malloc(SIZE_T n)
{
  register void* result = MALLOC(n);
  if(result == NULL) alloc_die();
  return result;
}

/*------------------------------------------------------------*/

void* bare_realloc(void* p, SIZE_T n)
{
  register void* result = REALLOC(p,n);
  if(result == NULL) alloc_die();
  return result;
}

