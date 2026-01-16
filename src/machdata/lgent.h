/**********************************************************************
 * File:    machdata/lgent.h
 * Purpose: Describe entities, with large representation.
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
 *		 STORAGE OF ENTITIES				*
 ****************************************************************/

/************************************************************************
 * This describes entities with the large representation.		*
 *									*
 * Entities are stored in a structure having a tag and a value field.	*
 * The leftmost bit of the tag is a mark bit for garbage collection.	*
 * The remaining bits are the actual tag.				*
 ************************************************************************/

/*--------------------------*
 * Miscellaneous constants. *
 *--------------------------*/

#ifdef LONG_IS_64_BITS
# define MAX_BOX_NUMBER  0x3fffffffffffffffL    
				       /* largest possible box number       */
#else
# define MAX_BOX_NUMBER  0x3fffffffL    /* largest possible box number      */
#endif


/****************************************************************
 *			CONSTRUCTORS AND DESTRUCTORS		*
 ****************************************************************/

/************************************************************************
 * Getting the tag field.  						*
 *									*
 * BASICTAG(e) is the tag of e. 					*
 *									*
 * GCTAG(e) is the same as BASICTAG(e), but masks out the mark bit,	*
 * which might be 1 during garbage collection, but is always 0 during	*
 * normal computation. 							*
 *									*
 * TAG(e) is the same as BASICTAG(e), but does a check when the garbage *
 * collector is in test mode, to see whether the entity has been	*
 * collected.  								*
 ************************************************************************/

/************************************
 * See gc/gc.c for function gctag.  *
 ************************************/

#define BASICTAG(e)   	   ((e).tag)
#define GCTAG(e)	   ((e).tag & 0x7f)
#ifdef GCTEST
  extern int gctag(ENTITY);
# define TAG(e)		   gctag(e)
#else
# define TAG(e)    	   BASICTAG(e)
#endif
#define ITAG(e) 	   ((int)TAG(e))

/************************************************************************
 * ENT_EQ(x,y) is true if x and y are equal.  				*
 *									*
 * ENT_NE(x,y) is true if x and y are not equal.  			*
 *									*
 * ENT_EQ and ENT_NE must not be used inside the garbage collector, 	*
 * since they assume that the mark bit is 0.  Use ENT_FEQ and ENT_FNE	*
 * in the garbage collector. 						*
 ************************************************************************/

#define ENT_EQ(x,y) (((x).tag == (y).tag) && ((x).val == (y).val))
#define ENT_NE(x,y) (((x).tag != (y).tag) || ((x).val != (y).val))
#define ENT_FEQ(x,y) ((GCTAG(x) == GCTAG(y)) && ((x).val == (y).val))
#define ENT_FNE(x,y) ((GCTAG(x) != GCTAG(y)) || ((x).val != (y).val))

/************************************************************************
 * IS_TRUE(x) is true if x = true.  This should only be used when x	*
 * is known to be either true or false.				        *
 *									*
 * IS_NIL(x) is true if x = nil.  This should only be used when it is	*
 * known that x is a list or pair.					*
 ************************************************************************/

#define IS_TRUE(x) (VAL(x) != 0)
#define IS_NIL(x)  (TAG(x) == NOREF_TAG)

/************************************************************************
 * Getting the value field. 						*
 *									*
 * VAL(e) is the value of e when the value is not a pointer.		*
 *									*
 * CHVAL(e) is a character value.  					*
 *									*
 * NRVAL(e) and IVAL(e) are for the special case where the tag 		*
 * is known to be NOREF_TAG.						*
 *									*
 * ENTVAL(e) is the (word) pointer stored in an entity. 		*
 *									*
 * BIVAL(e) is a pointer to a binary chunk.				*
 *									*
 * CTLVAL(e) is a pointer to a control					*
 *									*
 * CONTVAL(e) is a pointer to a continuation.				*
 *									*
 * SRVAL(e) is a pointer to a SMALL_REAL.				*
 *									*
 * LRVAL(e) is a pointer to a LARGE_REAL.				*
 *									*
 * TYPEVAL(e) is a pointer to a type.					*
 *									*
 * FILEENT_VAL(e) is a pointer to a struct file_ent			*
 ************************************************************************/

#define VAL(e)     ((ULONG) ((e).val))
#define CHVAL(e)   ((char) VAL(e))
#define NRVAL(e)   ((e).val)
#define IVAL(e)    ((e).val)
#define ENTVAL(e)  ((ENTITY *) ((e).val))

#define BIVAL(e)  ((CHUNKPTR) 	   	    ((e).val))
#define CTLVAL(e) ((CONTROL *)		    ((e).val))
#define CONTVAL(e) ((CONTINUATION *) 	    ((e).val))
#define SRVAL(e)  ((SMALL_REAL *)           ((e).val))
#define LRVAL(e)  ((LARGE_REAL *)    ((e).val))
#define TYPEVAL(e) ((TYPE *)		    ((e).val))
#define CSTRVAL(e) ((char *)		    ((e).val))
#define FILEENT_VAL(e) ((struct file_entity *) ((e).val))

/************************************************************************
 * Getting parts of an entity that is a list 				*
 *									*
 * HEAD and TAIL work for PAIR_TAG, TRIPLE_TAG and QUAD_TAG. 		*
 *									*
 * PAIR_TAIL     works only for PAIR_TAG 				*
 *									*
 * BLOCK_TAIL    works only for TRIPLE_TAG and QUAD_TAG. 		*
 ************************************************************************/

#define HEAD(l)	      (ENTVAL(l)[0])
#define PAIR_TAIL(l)  (ENTVAL(l)[1])
#define BLOCK_TAIL(l) make_ent(TAG(l) - 1, ((long)(ENTVAL(l) + 1)))
#define TAIL(l)	      ((TAG(l) == PAIR_TAG) ? PAIR_TAIL(l) : BLOCK_TAIL(l))

/****************************************************************
 * Building an entity.						*
 *								*
 * ENTU(n) is a NOREF_TAG version of n.				*
 *								*
 * ENTCH(n) is an unsigned, NOREF_TAG version of character n.   *
 *								*
 * ENTI(n) is a small integer with value n.			*
 *								*
 * ENTB(n) is a box with number n.				*
 *								*
 * ENTG(k) is a GLOBAL_TAG entity with value k.			*
 *								*
 * ENTP(t,p) is an entity storing tag t and pointer p.		*
 ****************************************************************/

#define ENTU(val)       make_ent(0, (LONG)(ULONG)(val))
#define ENTCH(val)      make_ent(0, (LONG)(unsigned char)(val))
#define ENTB(val)	make_ent(BOX_TAG, (LONG)(val))
#define ENTG(val)	make_ent(GLOBAL_TAG, ((LONG)(val)))
#define ENTI(val)      	make_ent(0, (LONG)(val))
#define ENTP(tag,val)   make_ent((tag), ((LONG)(val)))


/************************************************
 * Misc. operations on entities   		*
 *						*
 * NOT(e) 	is not(e) where e is		*
 *		a Boolean entity.		*
 *						*
 * ENT_ADD(e,n)	is the result of adding		*
 *              n to the VAL field of e.	*
 *		Be careful, since this		*
 *		does not always do the		*
 *		same thing in the large		*
 *		and small representations. 	*
 ************************************************/


#define NOT(e) 		make_ent(NOREF_TAG, 1 - VAL(e))
#define ENT_ADD(e,n)	make_ent(TAG(e), VAL(e) + (n))


/****************************************************************
 * 			STORAGE OF SMALL INTEGERS		*
 ****************************************************************/

#ifdef LONG_IS_64_BITS
# define SMALL_INT_DIGITS 18	        /* Number of decimal digits        */
					/* guaranteed storable in a small  */
					/* int entity                      */

# define SMALL_INT_MAX	0x3fffffffffffffffL
					/* Largest value storable in a     */
					/* small int entity.               */
					/* NOTE: the low order 		   */
					/* BITS_IN_INTCELL digits must be  */
					/* 0xffff. 			   */

# define SMALL_INT_MIN   -SMALL_INT_MAX  /* Smallest value storable in a    */
					/* small int entity                */

# define SQRT_SMALL_INT_MAX 0x7fffffff	/* At most sqrt(SMALL_INT_MAX)     */

#else

# define SMALL_INT_DIGITS 9	        /* Number of decimal digits        */
					/* guaranteed storable in a small  */
					/* int entity                      */

# define SMALL_INT_MAX	0x3fffffffL	/* Largest value storable in a     */
					/* small int entity.               */
					/* NOTE: the low order 		   */
					/* BITS_IN_INTCELL digits must be  */
					/* 0xffff. 			   */

# define SMALL_INT_MIN   -SMALL_INT_MAX /* Smallest value storable in a    */
					/* small int entity                */

# define SQRT_SMALL_INT_MAX 0x7fff	/* At most sqrt(SMALL_INT_MAX)     */

#endif

# define SMALL_INT_HIGH_MAX 0x3fff	/* Largest value storable in the   */
					/* highest order intcell in an	   */
					/* entity.			   */

#define INTCELLS_IN_LONG (LONG_BYTES/INTCELL_BYTES)
					/* The number of intcells that fit */
					/* into a LONG.			   */

/****************************************************************
 * 			ENTITY CONSTANTS			*
 ****************************************************************/

extern ENTITY false_ent, true_ent, NOTHING;
#define nil false_ent
#define hermit false_ent
#define zero false_ent
#define one true_ent

/****************************************************************
 * Note: If you add a standard box, be sure to update 		*
 * std_box_content_type and std_box_name in entity.c.		*
 ****************************************************************/

#define DIGITS_PRECISION_BOX_VAL 0
#define PRECISION_BOX_VAL	 1
#define STDIN_BOX_VAL		 2
#define TRUE_STDIN_BOX_VAL	 3
#define STDOUT_BOX_VAL		 4
#define STDERR_BOX_VAL		 5
#define PUT_COMMAS_BOX_VAL	 6
#define FIRST_FREE_BOX_NUM	 7
extern ENTITY DIGITS_PRECISION_BOX, PRECISION_BOX, STDIN_BOX, TRUE_STDIN_BOX;
extern ENTITY STDOUT_BOX, STDERR_BOX, PUT_COMMAS_BOX;


/****************************************************************
 *			FUNCTIONS				*
 ****************************************************************/

ENTITY make_ent		  (int tag, long val);
void   init_basic_entities(void);
