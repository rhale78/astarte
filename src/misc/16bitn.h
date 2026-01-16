/********************************************************************
 * File:    misc/16bit.h
 * Purpose: Define integer types and related constants.
 *          This file describes
 *
 *            Architecture: 16 bit processor
 *
 *            Alignment:    no alignment
 *
 *            Sizes:        double    = 64 bits
 *			    long int  = 32 bits
 *                          short int = 16 bits
 *                          char      = 8 bits
 *			    
 * Author:  Karl Abrahamson
 ********************************************************************/


/****************************************************************
 *			MACHINE LIMITS				*
 ****************************************************************/

#include <limits.h>

/****************************************************************
 *			INTEGER TYPES				*
 ****************************************************************
 * The requirements on the integer types are as follows.	*
 *								*
 *    Boolean		>= 1 bit				*
 *    SBYTE		>= 8 bits (signed)			*
 *    UBYTE		>= 8 bits (unsigned)			*
 *    SHORT		>= 16 bits (signed)			*
 *    USHORT		>= 16 bits (unsigned)			*
 *    INT32		>= 32 bits (signed)			*
 *    UINT32		>= 32 bits (unsigned)			*
 *    LONG		>= 32 bits, >= pointer size (signed)	*
 *    ULONG		>= 32 bits, >= pointer size (signed)	*
 ****************************************************************/

typedef signed char	   SBYTE;
typedef unsigned char      UBYTE;
typedef short int 	   SHORT;
typedef unsigned short int USHORT;
typedef long int	   INT32;
typedef unsigned long int  UINT32;
typedef long int 	   LONG;
typedef unsigned long int  ULONG;

#define BYTE_MAX	127		/* Largest value of a SBYTE	*/
#define UBYTE_MAX	255		/* Largest value of a UBYTE	*/

#define SHORT_BITS	16		/* Bits in a SHORT  		*/
#define LOG_SHORT_BITS  4               /* Log base 2(SHORT_BITS)	*/
#define SHORT_BYTES	2		/* Bytes in a SHORT 		*/
#ifndef SHORT_MAX
# define SHORT_MAX	32767		/* Largest value of a SHORT 	*/
#endif
#ifndef USHORT_MAX
# define USHORT_MAX	65535		/* Largest value of a USHORT 	*/
#endif

#define LONG_BITS	32		/* Bits in a LONG 		*/
#define LOG_LONG_BITS	5		/* Log base 2(LONG_BITS)	*/
#define LONG_BYTES	4		/* Bytes in a LONG 		*/
#ifndef LONG_MAX
# define LONG_MAX       2147483647L	/* Max value of a LONG		*/
#endif
#ifndef ULONG_MAX
# define ULONG_MAX	4294967295L	/* Max value of a ULONG		*/
#endif

/*----------------------------------------------------------------------*
 * SHORT_ALIGN is a mask such that x & SHORT_ALIGN is 0 if x is		*
 * an address where a SHORT can be stored.				*
 *									*
 * TRUE_SHORT_ALIGN is a mask such that x & TRUE_SHORT_ALIGN is 0 if	*
 * x is an address that is a multiple of SHORT_BYTES.			*
 *									*
 * LONG_ALIGN is a mask such that x & LONG_ALIGN is 0 if x is		*
 * an address where a LONG can be stored.				*
 *									*
 * TRUE_LONG_ALIGN is a mask such that x & TRUE_LONG_ALIGN is 0 if	*
 * x is an address that is a multiple of LONG_BYTES.			*
 *----------------------------------------------------------------------*/

#define SHORT_ALIGN		0
#define TRUE_SHORT_ALIGN	1
#define LONG_ALIGN		0
#define TRUE_LONG_ALIGN		3

/*--------------------------------------------------------------*
 * USHORT_MASK is a mask to select the low order short int from	*
 * a LONG or INT32						*
 *								*
 * LOG_LONG_BITS_MASK is a mask that selects the LOG_WORD_BITS	*
 * low order bits of a LONG.					*
 *								*
 * LONG_SIGN_MASKOUT is a mask that selects all but the sign	*
 * bit of a LONG.						*
 *--------------------------------------------------------------*/

#define USHORT_MASK		0xffffL
#define LOG_LONG_BITS_MASK 	0x1f
#define LONG_SIGN_MASKOUT	0x7fffffffL

/****************************************************************
 *			DOUBLES					*
 ****************************************************************
 * DOUBLE is the type of double precision reals.		*
 ****************************************************************/

typedef double DOUBLE;

#define DBL_BYTES	8	/* Bytes in a DOUBLE     */
#define LOG_DBL_BYTES   3	/* Log base 2(DBL_BYTES) */

/*----------------------------------------------------------------------*
 * DBL_ALIGN is a mask such that x & DBL_ALIGN is 0 if x is		*
 * an address where a DOUBLE can be stored.				*
 *									*
 * TRUE_DBL_ALIGN is a mask such that x & TRUE_DBL_ALIGN is 0 if  	*
 * address x is a multiple of DBL_BYTES.				*
 *									*
 * DBL_BLOCK_ALIGN should be a mask so that x & DBL_BLOCK_ALIGN is 0 if *
 * address x is a multiple of DBL_BYTES*LONG_BITS.			*
 *----------------------------------------------------------------------*/

#define DOUBLE_ALIGN		0
#define TRUE_DBL_ALIGN		7
#define DBL_BLOCK_ALIGN		0xFFL

#ifndef DBL_DIG
# define DBL_DIG 15		/* Decimal digits of precision in a double */
#endif
#ifndef DBL_MAX
# define DBL_MAX 1.0E300	/* Max value of a double */
#endif

/*--------------------------------------------*
 * LONG_MAX_AS_DOUBLE is LONG_MAX as a double *
 *--------------------------------------------*/

#define LONG_MAX_AS_DOUBLE 		((DOUBLE) LONG_MAX)


/****************************************************************
 *			INTCELLS				*
 ****************************************************************
 * An intcell is used to hold one "digit" of a large integer.	*
 * It must be the case that 					*
 * INTCELL_MAX*INTCELL_MAX <= ULONG_MAX.			*
 *								*
 * An intcellptr is a pointer to an intcell capable of being	*
 * used to index into a large array.				*
 ****************************************************************/

typedef USHORT	 intcell;
typedef intcell* intcellptr;

#define INTCELL_BITS	 SHORT_BITS	/* Bits in an intcell 		*/
#define LOG_INTCELL_BITS LOG_SHORT_BITS	/* Log base 2(INTCELL_BITS)	*/
#define INTCELL_BYTES	 SHORT_BYTES	/* BYtes in an intcell 		*/
#define INTCELL_MAX	 USHORT_MAX	/* Largest value of an intcell	*/
#define SQRT_INTCELL_MAX 255		/* floor(sqrt(INTCELL_MAX))	*/

/*--------------------------------------------------------------*
 * INT_BASE is the base of large integers.  It should be	*
 * INTCELL_MAX + 1, or 2^{INTCELL_BITS}.			*
 *--------------------------------------------------------------*/

#define INT_BASE (INTCELL_MAX + 1)

/*--------------------------------------------------------------*
 * ALMOST_SQ_BASE is INT_BASE*INT_BASE/4. 			*
 *--------------------------------------------------------------*/

#define ALMOST_SQ_BASE 0x3fffffff

/*--------------------------------------------------------------*
 * INTCELL_TO_BYTE(x) converts a count x of intcells to a count *
 * of bytes.  So INTCELL_TO_BYTE(x) = x * INTCELL_BYTES.	*
 *--------------------------------------------------------------*/

#define INTCELL_TO_BYTE(x) ((x)<<1)

/*--------------------------------------------------------------*
 * BYTE_TO_INTCELL(x) converts a count x of bytes to a count *
 * of intcells.  So BYTE_TO_INTCELL(x) = x / INTCELL_BYTES.	*
 *--------------------------------------------------------------*/

#define BYTE_TO_INTCELL(x) ((x)>>1)

/*--------------------------------------------------------------*
 * DEC_DIGITS_IN_INTCELL is the number of decimal digits needed *
 * to write INTCELL_MAX.					*
 *--------------------------------------------------------------*/

#define DEC_DIGITS_IN_INTCELL 5

/*--------------------------------------------------------------*
 * HALF_DIGIT_PLUS+BIT is 2*sqrt(INTCELL_MAX _ 1).		*
 *--------------------------------------------------------------*/

#define HALF_DIGIT_PLUS_BIT (0x200)

/*---------------------------------------------------------------*
 * HEX_DIGITS_IN_DIGIT is the number of hexadecimal digits that  *
 * are used to represent one intcell.				 *
 *---------------------------------------------------------------*/

#define HEX_DIGITS_IN_DIGIT 4

/*---------------------------------------------------------------*
 * HEX_TO_DIGIT(k) is the number of intcells needed to store     *
 * a hexadecimal number that is k digits long (in base INT_BASE).*
 *---------------------------------------------------------------*/

#define HEX_TO_DIGIT(k) ((k)>>2)

/*---------------------------------------------------------------*
 * DECIMAL_TO_DIGIT(p) is the number of internal digits that are *
 * used when the number of decimal digits of precision for	 *
 * floating point calculations is set to p.  If 		 *
 * DECIMAL_TO_DIGIT(p) is d, then it must be the case that	 *
 * 10^p -1 <= (INTCELL_MAX + 1)^d.  That is, d intcells are	 *
 * capable of holding a p-digit decimal number.			 *
 *---------------------------------------------------------------*/

#define DECIMAL_TO_DIGIT(p) (((p) + 1) >> 2)

/*--------------------------------------------------------------*
 * A ULONG must be capable of storing two intcells, side by	*
 * side.							*
 *								*
 * INTCELL_MASK is a mask to select the low intcell part from	*
 * a ULONG.							*
 *								*
 * INTCELL_RESTMASK is a mask to select the high intcell part	*
 * of a ULONG.							*
 *								*
 * INTCELL_HIGHBIT_MASK is a mask to select the high bit of 	*
 * an intcell.							*
 *								*
 * INTCELL_MASK1 is INTCELL_MASK & ~INTCELL_HIGHBIT_MASK.	*
 *--------------------------------------------------------------*/

#define INTCELL_MASK		(0xffffL)
#define INTCELL_RESTMASK 	(0xffff0000L)
#define INTCELL_HIGHBIT_MASK 	(0x8000)
#define INTCELL_MASK1   	(0x7fff)


/*----------------------------------------------------------------------*
 * CHUNK_MIN_SIZE is the smallest multtiple of LONG_BYTES that is not	*
 * smaller than SHORT_BYTES + PTR_BYTES.				*
 ************************************************************************/

#define CHUNK_MIN_SIZE 8


/****************************************************************
 *			ENTITIES				*
 ****************************************************************
 * LARGE_ENTITY_BYTES is the number of bytes occupied by an 	*
 * ENTITY in the large option.  It is the number of bytes used	*
 * by the following structure, taking into account the need	*
 * for alignment.						*
 * 								*
 * struct {							*
 *   LONG val;							*
 *   UBYTE tag;							*
 * };								*
 *								*
 * ENTITY_BYTES is the number of bytes in an entity.  It 	*
 * depends on whether large or small entities are chosen.	*
 ****************************************************************/

#define LARGE_ENTITY_BYTES 5

#ifdef SMALL_ENTITIES
# define ENTITY_BYTES LONG_BYTES
#else
# define ENTITY_BYTES LARGE_ENTITY_BYTES
#endif

/****************************************************************
 *			POINTERS				*
 *			MEMORY					*
 ****************************************************************
 * A pointer is a pointer that is capable of indexing into	*
 * a large array.						*
 ****************************************************************/

#define PTR_BYTES	4	/* Bytes in a pointer 		*/
#define PTR_SHORTS	2	/* SHORTS in a pointer 		*/

/*--------------------------------------------------------------*
 * HUGEPTR is a modifier for a pointer that has correct pointer *
 * semantics for large array indexing.  It should be * along	*
 * with the modifier.						*
 *								*
 * FARR is a modifier for an array that indicates the array	*
 * is large, and should be stored in its own segment.  It 	*
 * should be nothing at all for flat memories.			*
 *--------------------------------------------------------------*/

#define HUGEPTR huge *
#define FARR    far

/*----------------------------------------------------------------------*
 * For the 16-bit implementation, we use longmemcpy and longmemset to   *
 * do memory operations on large arrays.  For 32-bit implementations,   *
 * they are just memcpy and memset.					*
 *----------------------------------------------------------------------*/

void longmemcpy(void HUGEPTR dest, void HUGEPTR src, long n);
void longmemset(void HUGEPTR s, int c, long n);
void longstrcpy(char HUGEPTR dest, char hugeptr src); 

/*----------------------------------------------------------------------*
 * MALLOC_ALIGN is a mask such that, if an address x is returned by	*
 * MALLOC, then x & MALLOC_ALIGN = 0.					*
 *----------------------------------------------------------------------*/

#define MALLOC_ALIGN 0


