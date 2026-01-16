/**********************************************************************
 * File:    machdata/sment.h
 * Purpose: Describe entities, with small representation.
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
 * This describes entities with the small representation.		*
 *									*
 * Entities are stored in a long int, and have three fields.  		*
 * left-to-right, (high-order to low-order) they are:			*
 *									*
 *         bits          	  value					*
 *         ----         	  ------				*
 *         VALSIZE   (26 or 58)   data of this entity			*
 *									*
 *                   1  	  mark bit for garbage collection	*
 *									*
 *         TAGSIZE-1 (5)          tag					*
 *									*
 ************************************************************************/


/*----------------------------------------*
 * Fields and field extraction constants. *
 *----------------------------------------*/

#ifdef LONG_IS_64_BITS

# define VALSIZE 	58             /* Number of bits in value           */

# define VALMASK 	0x03ffffffffffffffL    
				       /* Mask to extract value after shift */

# define ENTPMASK        0x0ffffffffffffff8L
				       /* Mask to apply to pointer offset   */
				       /* from heap_base                    */

# define MARKBIT_MASKOUT 0xffffffffffffffdfL
				       /* Mask to apply to an entity to     */
				       /* mask out the mark bit.	    */

#define ENTPSHIFT       3              /* Amount to shift an entity ptr     */
				       /* (Must shift over TAGSIZE bits, but*/
				       /* must also supply zero bits on the */
				       /* end, to convert between a word    */
				       /* address and a byte address.       */
				       /* After shifting an entity right    */
				       /* by ENTPSHIFT bits, you mask by    */
				       /* ENTPMASK, and get an		    */
				       /* offset from heap_base.            */
#else

# define VALSIZE 	26             /* Number of bits in value           */

# define VALMASK 	0x03ffffffL    /* Mask to extract value after shift */

# define ENTPMASK       0x0ffffffcL    /* Mask to apply to pointer offset   */
				       /* from heap_base                    */

# define MARKBIT_MASKOUT 0xffffffdfL   /* Mask to apply to an entity to     */
				       /* mask out the mark bit.	    */

#define ENTPSHIFT       4              /* Amount to shift an entity ptr     */
				       /* (Must shift over TAGSIZE bits, but*/
				       /* must also supply zero bits on the */
				       /* end, to convert between a word    */
				       /* address and a byte address.       */
				       /* After shifting an entity right    */
				       /* by ENTPSHIFT bits, you mask by    */
				       /* ENTPMASK, and get an		    */
				       /* offset from heap_base.            */
#endif

#define TAGSIZE         6              /* Size of tag and mark bit          */

#define VALINCR         0x40L          /* Amount to add to increment value  */

#define TAGMASK		0x1f           /* Mask to extract tag               */


/*----------------------------------------------------------------------*
 * Miscellaneous constants. 						*
 *									*
 *   MAX_BOX_NUMBER 	  is the largest possible box number.		*
 *									*
 *   MAX_SMALLENT_ADDRESS is the largest possible entity address, as	*
 *			  stored as an offset from heap_base in an	*
 *			  entity.					*
 *----------------------------------------------------------------------*/

#ifdef LONG_IS_64_BITS
# define MAX_BOX_NUMBER  	0x03ffffffffffffffL
# define MAX_SMALLENT_ADDRESS 	0x03ffffffffffffffL
#else
# define MAX_BOX_NUMBER  	0x03ffffffL
# define MAX_SMALLENT_ADDRESS 	0x03ffffffL
#endif


/****************************************************************
 *			CONSTRUCTORS AND DESTRUCTORS		*
 ****************************************************************/

/************************************************************************
 * Getting the tag field.  						*
 *									*
 * BASICTAG(e) is the tag of e. 					*
 *									*
 * GCTAG(e) is the same as BASICTAG(e), but masks out the mark bit.	*
 * For this implementation of entities, there is no difference between	*
 * GCTAG and BASICTAG, but there is a difference in lgent.h. 		*
 *									*
 * TAG(e) is the same as BASICTAG(e), but does a check when the garbage *
 * collector is in test mode, to see whether the entity has been	*
 * collected.  								*
 ************************************************************************/

/************************************
 * See gc/gc.c for function gctag.  *
 ************************************/

#define BASICTAG(e)   	   (((int)EV(e)) & TAGMASK)
#define GCTAG(e)	   BASICTAG(e)
#ifdef GCTEST
  extern int gctag(ENTITY);
# define TAG(e)		   gctag(e)
#else
# define TAG(e)    	   BASICTAG(e)
#endif

/************************************************************************
 * ENT_EQ(x,y) is true if x and y are equal.  				*
 *									*
 * ENT_NE(x,y) is true if x and y are not equal.  			*
 *									*
 * ENT_EQ and ENT_NE must not be used inside the garbage collector, 	*
 * since they assume that the mark bit is 0.  Use ENT_FEQ and ENT_FNE	*
 * in the garbage collector. 						*
 ************************************************************************/

#define ENT_EQ(x,y)	   (EV(x) == EV(y))
#define ENT_NE(x,y)	   (EV(x) != EV(y))
#define ENT_FEQ(x,y)	   ((EV(x) & MARKBIT_MASKOUT) == (EV(y) & MARKBIT_MASKOUT))
#define ENT_FNE(x,y)	   ((EV(x) & MARKBIT_MASKOUT) != (EV(y) & MARKBIT_MASKOUT))

/************************************************************************
 * IS_TRUE(x) is true if x = true.  This should only be used when x	*
 * is known to be either true or false.				        *
 *									*
 * IS_NIL(x) is true if x = nil.  This should only be used when it is	*
 * known that x is a list or pair.					*
 ************************************************************************/

#define IS_TRUE(x) (EV(x) != EV(false_ent))
#define IS_NIL(x)  (TAG(x) == NOREF_TAG)

/************************************************************************
 * Getting the value field. 						*
 *									*
 * VAL(e) is the value of e when the value is not a pointer, as an	*
 * unsigned long int.							*
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
 *									*
 * CSTRVAL(e) is the char* pointer stored in e.				*
 ************************************************************************/

#define VAL(e)     ((ULONG) ((EV(e) >> TAGSIZE) & VALMASK))
#define CHVAL(e)   ((char) NRVAL(e))
#define NRVAL(e)   (EV(e) >> TAGSIZE)
#define IVAL(e)    NRVAL(e)
#define ENTVAL(e)  ((ENTITY *) (((EV(e) >> ENTPSHIFT) & ENTPMASK) + heap_base))

#define BIVAL(e)  ((CHUNKPTR)        	    ENTVAL(e))
#define CTLVAL(e) ((CONTROL *)		    ENTVAL(e))
#define CONTVAL(e) ((CONTINUATION *) 	    ENTVAL(e))
#define SRVAL(e)  ((SMALL_REAL *)           ENTVAL(e))
#define LRVAL(e)  ((LARGE_REAL *)    	    ENTVAL(e))
#define TYPEVAL(e) ((TYPE *)		    ENTVAL(e))
#define CSTRVAL(e) ((char*)		    ENTVAL(e))
#define FILEENT_VAL(e) ((struct file_entity *) ENTVAL(e))

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
#define BLOCK_TAIL(l) MAKEENT(EV(l) + (VALINCR - 1))  
#define TAIL(l)	      ((TAG(l) == PAIR_TAG) ? PAIR_TAIL(l) : BLOCK_TAIL(l))

/****************************************************************
 * Building an entity.  					*
 *								*
 * ENTU(n) is a NOREF_TAG version of n.  			*
 *								*
 * ENTCH(n) is an unsigned, NOREF_TAG version of character n.   *
 *								*
 * ENTI(n) is a small integer with value n.  			*
 *								*
 * ENTB(n) is a box with number n.  				*
 *								*
 * ENTG(k) is a GLOBAL_TAG entity with value k.			*
 *								*
 * ENTP(t,p) is an entity storing tag t and entity pointer p. 	*
 * p MUST be in the heap.					*
 ****************************************************************/

#define ENTU(val)       MAKEENT(((ULONG)(val)) << TAGSIZE)
#define ENTCH(val)      MAKEENT(((LONG)(unsigned char)(val)) << TAGSIZE)
#define ENTB(val)	MAKEENT(BOX_TAG | (((LONG)(val)) << TAGSIZE))
#define ENTG(val)	MAKEENT(GLOBAL_TAG | (((LONG)val) << TAGSIZE))
#define ENTI(val)      	MAKEENT(((LONG)(val)) << TAGSIZE)
#define ENTP(tag,val)   MAKEENT((tag) | ((((char *) (val)) - heap_base) << ENTPSHIFT))

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

#define NOT(e)          MAKEENT(trueval - EV(e))      
#define ADDENT(e,n) 	MAKEENT(EV(e) + ((LONG)(n))) 
#define ENT_ADD(e,n)	ADDENT(e, ((LONG)(n)) << TAGSIZE)


/****************************************************************
 * 			STORAGE OF SMALL INTEGERS		*
 ****************************************************************/

#ifdef LONG_IS_64_BITS
# define SMALL_INT_DIGITS 17	        /* Number of decimal digits        */
					/* guaranteed storable in a small  */
					/* int entity                      */

# define SMALL_INT_MAX	0x01ffffffffffffffL	
					/* Largest value storable in a     */
					/* small int entity.               */
					/* NOTE: the low order             */
					/* BITS_IN_INTCELL digits must by  */
					/* 0xffff.                         */

# define SMALL_INT_MIN   -SMALL_INT_MAX /* Smallest value storable in a    */
					/* small int entity                */

# define SQRT_SMALL_INT_MAX 268435455L	/* At most sqrt(SMALL_INT_MAX)     */

#else
# define SMALL_INT_DIGITS 7	        /* Number of decimal digits        */
					/* guaranteed storable in a small  */
					/* int entity                      */

# define SMALL_INT_MAX	0x01ffffffL	/* Largest value storable in a     */
					/* small int entity.               */
					/* NOTE: the low order             */
					/* BITS_IN_INTCELL digits must by  */
					/* 0xffff.                         */

# define SMALL_INT_MIN   -SMALL_INT_MAX /* Smallest value storable in a    */
					/* small int entity                */

# define SQRT_SMALL_INT_MAX 5792	/* At most sqrt(SMALL_INT_MAX)     */

#endif

#define SMALL_INT_HIGH_MAX 0x1ff	/* Largest value storable in the   */
					/* highest order intcell in an	   */
					/* entity.			   */

#define INTCELLS_IN_LONG (LONG_BYTES/INTCELL_BYTES)
					/* The number of intcells that fit */
					/* into a LONG.			   */



/****************************************************************
 * 			ENTITY CONSTANTS			*
 ****************************************************************/

#define false_ent   MAKEENT(0)
#define falseval    0
#define true_ent    MAKEENT(VALINCR)
#define trueval     VALINCR
#define nil         false_ent
#define nilval      0
#define hermit      false_ent
#define hermitval   0
#define zero        false_ent
#define zeroval     0
#define one	    MAKEENT(VALINCR)
#define oneval	    VALINCR
#define NOTHING     MAKEENT(INDIRECT_TAG)    /* non-entity value */

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
#define DIGITS_PRECISION_BOX 	(ENTB(DIGITS_PRECISION_BOX_VAL))
#define PRECISION_BOX 		(ENTB(PRECISION_BOX_VAL))
#define STDIN_BOX	     	(ENTB(STDIN_BOX_VAL))
#define TRUE_STDIN_BOX		(ENTB(TRUE_STDIN_BOX_VAL))
#define STDOUT_BOX		(ENTB(STDOUT_BOX_VAL))
#define STDERR_BOX		(ENTB(STDERR_BOX_VAL))
#define PUT_COMMAS_BOX		(ENTB(PUT_COMMAS_BOX_VAL))

#ifdef DEBUG
ENTITY make_ent(LONG l);
#endif

