/**********************************************************************
 * File:    machdata/smgc.h
 * Purpose: Describe how entities are marked and unmarked for garbage 
 *          collection (for small representation).
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

#define GARBAGE		MAKEENT(RELOCATE_TAG)

/************************************************************************
 * GCMARK(es) sets the mark bit of es.  es should be a pointer to an	*
 * entity.								*
 *									*
 * GCUNMARK(es) clears the mark bit of es.				*
 *									*
 * MARKED(e) is nonzero if e is marked.  e should be an entity.		*
 ************************************************************************/

#define GCMARK(es)	EVS(es) |= 0x20L       
#define GCUNMARK(es)	EVS(es) &= (~ 0x20L)
#define MARKED(e)	(((int)EV(e)) & 0x20)  

/*************************************************************************
 * Doubles are stored in blocks consisting of a mark word, a link to the *
 * next block, and just under LONG_BITS doubles.  The mark word holds    *
 * the mark bits for all of the doubles.  The block begins on a multiple *
 * of 2^DBL_BLOCK_ALIGN.  		 				 *
 *									 *
 * DBLMARK_WORD(x) is the address of the mark word associated with	 *
 * address x.  It can be found by zeroing out the rightmost bits in      *
 * the address, using mask DBL_BLOCK_ALIGN.				 *
 *									 *
 * DBLMARK_SHIFT(x) is the amount to shift a 1 left to reach 		 *
 * the mark bit for x.  It uses DBLMARK_SHIFT_MASK to select part of an  *
 * address.		  						 *
 *									 *
 * DBLMARKED(x) is nonzero if x is marked.  	 			 *
 *									 *
 * DBLMARK(x) sets the mark bit of x. 					 *
 *									 *
 * NOTE: SMALL_REAL_BLOCKSIZE is define in misc/perform.h to be 31.  Be	 *
 * sure to modify it if the numbers here change.			 *
 *************************************************************************/

#define DBLMARK_WORD(x)  ((LONG *)(((LONG) (x)) & (~DBL_BLOCK_ALIGN)))
#define DBLMARK_SHIFT_MASK ((DBL_BLOCK_ALIGN << LOG_DBL_BYTES) & DBL_BLOCK_ALIGN)
#define DBLMARK_SHIFT(x) ((((LONG) (x)) & DBLMARK_SHIFT_MASK) >> LOG_DBL_BYTES)
#define DBLMARK(x)       *DBLMARK_WORD(x) |= 1L << DBLMARK_SHIFT(x)
#define DBLMARKED(x)     (*DBLMARK_WORD(x) & (1L << DBLMARK_SHIFT(x)))

/************************************************************************
 * BINARY_MARKED(p) is nonzero if the binary chunk pointed to by p is	*
 * marked.								*
 *									*
 * BINARY_MARK(p) marks the binary chunk pointed to by pointer p.	*
 *									*
 * BINARY_RELOCATED(p) is nonzero if the binary chunk pointed to by p	*
 * has been relocated.							*
 *									*
 * BINARY_RELOCATE_MARK(p) sets the relocation mark of the binary chunk *
 * pointed to by p.  This indicates that this chunk has been relocated,	*
 * and is now at the address given at the start of the chunk.		*
 *									*
 * BINARY_UNMARK(p) clears the mark and relocation bit of the binary	*
 * chunk pointed to by p.						*
 *									*
 * BINARY_POINTER_AT(p) is the binary pointer stored just after the	*
 * header in the chunk pointed to by p.  It should only used for        *
 * chunks in normal blocks, not in huge blocks.				*
 *									*
 * INSTALL_BINARY_POINTER(r,p) puts pointer p into the chunk pointed	*
 * to by r, just after the header, so that BINARY_POINTER_AT(r) will	*
 * extract p.  It should only be used for chunks in normal binary 	*
 * blocks, not for huge blocks.						*
 *									*
 * COPY_BINARY_POINTER(dest,src) copies the pointer just after the 	*
 * header at src to just after the header at dest.  Both dest and src   *
 * should point to binary chunks in normal binary blocks, not in	*
 * huge blocks.								*
 *									*
 * BINARY_POINTER_IS_NULL(p) is true just when the binary pointer	*
 * stored just after the header of p is NULL.  This should also only	*
 * be used on normal chunks, not on huge chunks.			*
 ************************************************************************/

#define BINARY_MARKED(p)	(*((CHUNKHEADPTR) (p)) & 0x4000)
#define BINARY_RELOCATED(p)	(*((CHUNKHEADPTR) (p)) & 0x8000)
#define BINARY_MARK(p)  	*((CHUNKHEADPTR) (p)) |= 0x4000
#define BINARY_RELOCATE_MARK(p)	*((CHUNKHEADPTR) (p)) |= 0x8000
#define BINARY_UNMARK(p) 	*((CHUNKHEADPTR) (p)) &= 0x3fff

#if (PTR_BYTES <= 2*SHORT_BYTES)

#define BINARY_POINTER_AT(p)\
    ((CHUNKPTR)(\
	 ((LONG)((CHUNKHEADPTR)(p))[1]) +\
	(((LONG)((CHUNKHEADPTR)(p))[2]) << SHORT_BITS)))

#define INSTALL_BINARY_POINTER(r,p)\
	{((CHUNKHEADPTR)(r))[1] = (USHORT)(((LONG)(p)) & USHORT_MASK);\
	 ((CHUNKHEADPTR)(r))[2] = (USHORT)((((LONG)(p)) >> SHORT_BITS)\
                                           & USHORT_MASK);\
	}

#define COPY_BINARY_POINTER(dest,src)\
	{((CHUNKHEADPTR)(dest))[1] = ((CHUNKHEADPTR)(src))[1];\
	 ((CHUNKHEADPTR)(dest))[2] = ((CHUNKHEADPTR)(src))[2];\
	}

#define BINARY_POINTER_IS_NULL(p)\
	 (((CHUNKHEADPTR)(p))[1] == 0 && \
	  ((CHUNKHEADPTR)(p))[2] == 0\
	 )
#else
#define BINARY_POINTER_AT(p)\
    ((CHUNKPTR)(\
	 ((LONG)((CHUNKHEADPTR)(p))[1]) + \
	(((LONG)((CHUNKHEADPTR)(p))[2]) << SHORT_BITS) + \
	(((LONG)((CHUNKHEADPTR)(p))[3]) << 2*SHORT_BITS) + \
	(((LONG)((CHUNKHEADPTR)(p))[4]) << 3*SHORT_BITS)))

#define INSTALL_BINARY_POINTER(r,p)\
      {((CHUNKHEADPTR)(r))[1] = ((LONG)(p)) & USHORT_MASK;\
       ((CHUNKHEADPTR)(r))[2] = ((((LONG)(p)) >> SHORT_BITS) & USHORT_MASK);\
       ((CHUNKHEADPTR)(r))[3] = ((((LONG)(p)) >> 2*SHORT_BITS) & USHORT_MASK);\
       ((CHUNKHEADPTR)(r))[2] = ((((LONG)(p)) >> 3*SHORT_BITS) & USHORT_MASK);\
       }

#define COPY_BINARY_POINTER(dest,src)\
	{((CHUNKHEADPTR)(dest))[1] = ((CHUNKHEADPTR)(src))[1];\
	 ((CHUNKHEADPTR)(dest))[2] = ((CHUNKHEADPTR)(src))[2];\
	 ((CHUNKHEADPTR)(dest))[3] = ((CHUNKHEADPTR)(src))[3];\
	 ((CHUNKHEADPTR)(dest))[4] = ((CHUNKHEADPTR)(src))[4];\
	}

#define BINARY_POINTER_IS_NULL(p)\
	 (((CHUNKHEADPTR)(p))[1] == 0 && \
	  ((CHUNKHEADPTR)(p))[2] == 0 && \
	  ((CHUNKHEADPTR)(p))[3] == 0 &&\
	  ((CHUNKHEADPTR)(p))[4] == 0\
	 )
#endif

