/**********************************************************************
 * File:    machdata/entity.h
 * Purpose: Describe representation of entities.
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
 *			TAGS					*
 ****************************************************************/

/************************************************************************
 * NOTE: There are some important restrictions on the values of these 	*
 * tags.								*
 *									*
 * 1. Do not change NOREF_TAG, since ENTU does not install a tag. 	*
 *									*
 * 2. Keep WRAP_TAG -- QWRAP3_TAG contiguous and in the given order.	*
 *									*
 * 3. We require NOREF_TAG < BIGNEGINT_TAG, BIGPOSINT_TAG < RATIONAL_TAG*
 *    < SMALL_REAL_TAG < LARGE_REAL_TAG.  This order is used in        	*
 *    function up_convert in rts/number.c.				*
 *									*
 * Tags that are grouped together below have the same value.		*
 *									*
 * See entity.doc for details of entities. 				*
 ************************************************************************/


#define NOREF_TAG		0	/* Bool, char, nil, etc.          */
#define INT_TAG			0	/* Natural, integer               */

#define GLOBAL_TAG		1	/* Index in global_bindings       */

#define GLOBAL_INDIRECT_TAG 	2	/* Entity(2) (val, type)          */

#define INDIRECT_TAG		3	/* Entity                         */

#define LAZY_LIST_TAG		4	/* UP_CONTROL			  */

#define LAZY_TAG		5	/* DOWN_CONTROL                   */

#define LAZY_PRIM_TAG		6       /* Entity(3): A kind and two args */

#define DEMON_TAG		7	/* Entity(2)			  */

#define FAIL_TAG		8	/* Entity	                  */

#define BIGNEGINT_TAG		9	/* Binary chunk                   */

#define BIGPOSINT_TAG		10	/* Binary chunk                   */

#define PAIR_TAG		11	/* Entity(2)                      */
#define RATIONAL_TAG		11	/* Entity(2) (num,demon)	  */

#define TRIPLE_TAG		12	/* Entity(3)                      */

#define QUAD_TAG		13	/* Entity(4) (see below if changed)*/

#define TREE_TAG		14	/* Entity(5) (see entity.doc)     */

#define ARRAY_TAG		15	/* Entity(2-4) (see entity.doc)     */

#define APPEND_TAG		16	/* Entity(2) (left,right)         */

#define SMALL_REAL_TAG		17	/* SMALL_REAL                     */

#define LARGE_REAL_TAG		18	/* Entity(2) (man, ex)            */

#define WRAP_TAG		19	/* Entity(2) (tag, val)           */

#define QWRAP0_TAG		20	/* Entity (implicitly WRAP(0,e))  */
#define ENTITY_TOP_TAG		20	/* Entity (see entity.doc)	  */

#define QWRAP1_TAG		21	/* Entity (implicitly WRAP(1,e))  */

#define QWRAP2_TAG		22	/* Entity (implicitly WRAP(2,e))  */

#define QWRAP3_TAG		23	/* Entity (implicilty WRAP(3,e))  */

#define PLACE_TAG		24	/* Entity                         */

#define BOX_TAG			25	/* A box number                   */

#define FILE_TAG		26      /* ptr to file_entity             */

#define FUNCTION_TAG		27	/* Continuation                   */

#define TYPE_TAG		28	/* Type                           */

#define CSTR_TAG		29	/* A null-terminated string	  */

#define STRING_TAG		30	/* Binary chunk			  */

#define RELOCATE_TAG            31      /* Relocated entity               */

#define MAX_ENTS_IN_PAIR_BLOCK  4       /* QUAD_TAG is largest block tag  */

/************************************************************************
 * There are two representations available: small and large.		*
 * The small representation uses a long integer (32 or 64 bits), 	*
 * consisting of 5 tag bits, one mark bit and 26 or 58 data bits.	*
 * The large representation uses a structure holding a long integer 	*
 * and a tag byte consisting of 7 tag bits and one mark bit		*
 *									*
 * IMPORTANT: the small representation requires that memory be		*
 * allocated in the heap in contiguous (low) (virtual) memory addresses,*
 * in ascending order.  If this is not true, then the large		*
 * representation should be used.					*
 ************************************************************************/

#ifdef SMALL_ENTITIES
# include "../machdata/sment.h"
#else
# include "../machdata/lgent.h"
#endif


/****************************************************************
 *			OPTIONAL				*
 ****************************************************************
 * noValue is the value noValue, defined in standard.ast.       *
 * make_optional(a) is value(a), as defined in standard.ast. 	*
 ****************************************************************/

#define noValue zero
#define make_optional(a) qwrap(1,a)


/****************************************************************
 *			STORAGE OF BIG INTEGERS, 		*
 *			REALS AND STRINGS			*
 ****************************************************************/

/*------------------------------------------------------*
 * BIGINT_SIZE(x) is the data size (in intcells) of	*
 *		  binary chunk x.			*
 *							*
 * STRING_SIZE(x) is the data size (in bytes) of binary	*
 *		  chunk x.				*
 *							*
 * BIGINT_BUFF(x) is a pointer to the digits buffer	*
 *		  of a large integer, given by binary	*
 *		  chunk x.				*
 *							*
 * STRING_BUFF(x) is a pointer to the byte buffer of	*
 *		  string x.				*
 *------------------------------------------------------*/

#define BIGINT_SIZE(x) BYTE_TO_INTCELL(binary_chunk_size(x))
#define STRING_SIZE(x) (binary_chunk_size(x))
#define BIGINT_BUFF(x) ((intcellptr)(binary_chunk_buff(x)))
#define STRING_BUFF(x) (binary_chunk_buff(x))

/****************************************************************
 *			ADDITIONAL CONSTANTS			*
 ****************************************************************/

#define ALL_TRAP 1000	/* Value used to indicate trap all. */

/****************************************************************
 *			STANDARD ENTITIES			*
 ****************************************************************/

extern ENTITY ten, zero_rat, one_rat, ten_rat, a_large_int,
	      bad_ent, zero_real, one_real,
	      ten_real, half_real, dollar_fixp_low, large_ten_real,
	      large_one_real, newline, infloop_timeout,
	      stdout_file, stderr_file, true_stdin_box,
	      divide_by_zero_ex;

#define TEN_TO_MAX 24
extern ENTITY ten_to_p[TEN_TO_MAX];

extern ULONG standard_input;

/****************************************************************
 *			TAG SETS				*
 ****************************************************************/

extern intset int_tags, real_tags, lazy_tags, all_lazy_tags, full_eval_tags;
extern intset no_copy_tags, always_store_tags, all_weak_lazy_tags;
extern intset entp_tags, binary_ptr_tags;

/****************************************************************
 *			STANDARD BOX INFO			*
 ****************************************************************/

extern TYPE* std_box_content_type[];
extern char* std_box_name[];


/****************************************************************
 *			FUNCTIONS				*
 ****************************************************************/

ENTITY ten_int_to	(LONG n);
ENTITY ten_real_to   	(LONG n);
Boolean is_lazy         (ENTITY e);
Boolean is_lazy_file	(ENTITY e);
Boolean has_lazy_parts  (ENTITY e);
void   init_entity	(void);
int    tagf		(ENTITY e);
ENTITY *entvalf		(ENTITY e);
ENTITY make_fail_ent	(void);
ENTITY remove_indirection(ENTITY e);
ENTITY make_indirect	(ENTITY *loc);
ENTITY get_global	(char *name, TYPE *t);
void  init_state	(void);


