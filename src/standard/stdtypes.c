/*********************************************************************
 * File:    standard/stdtypes.c
 * Author:  Karl Abrahamson
 * Purpose: Create standard types, genera, etc.
 *********************************************************************/

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

/*********************************************************************
 * This file creates the standard species, genera, communities and   *
 * and families, and creates some variables for the compiler.	     *
 *								     *
 *                    IMPORTANT NOTE				     *
 * 								     *
 * If you add a standard type or family, go to tables/typehash.c and *
 * add a line to init_type_tb to put it into the type table when the *
 * interpreter starts.  The same holds if you add a new standard     *
 * genus or community.						     *
 *********************************************************************/

#include "../misc/misc.h"
#include "../classes/classes.h"
#include "../clstbl/classtbl.h"
#include "../clstbl/stdtytbl.h"
#include "../clstbl/meettbl.h"
#include "../clstbl/typehash.h"
#include "../infer/infer.h"
#include "../standard/stdtypes.h"
#include "../standard/stdids.h"
#include "../parser/parser.h"
#include "../utils/lists.h"


/****************************************************************
 * 			PUBLIC VARIABLES			*
 ****************************************************************/

/************************************************************************
 * Note: every species and family has a number, which is the index into *
 * array ctcs where its table cell is found.  The standard species and  *
 * families also have a "standard number", which is used in 		*
 * constructing it at run-time.						*
 ************************************************************************/

/****************************************************************
 * 			FIRST_STANDARD_GENUS			*
 *			FIRST_STANDARD_TYPE			*
 *			LAST_TYPE_NUM				*
 ****************************************************************
 * FIRST_STANDARD_GENUS is the number (index in ctcs) of first  *
 * the standard genus or community.				*
 *								*
 * FIRST_STANDARD_TYPE is the number of the first standard	*
 * species or family.						*
 *								*
 * LAST_TYPE_NUM is the number of the last standard species	*
 * or family.							*
 ****************************************************************/

int  FIRST_STANDARD_GENUS;
int  FIRST_STANDARD_TYPE;
int  LAST_TYPE_NUM;

/****************************************************************
 *			Hermit_num				*
 *			Hermit_std_num				*
 *			Char_std_num				*
 ****************************************************************
 * These are numbers related to species () and Char.		*
 ****************************************************************/


UBYTE  Hermit_num;	/* Number of type () 			*/
UBYTE  Hermit_std_num;	/* Standard number of type () 		*/
UBYTE  Char_std_num;    /* Standard number of type Char 	*/


/****************************************************************
 * The following are variables that hold standard species.	*
 * and families.						*
 ****************************************************************/


TYPE *hermit_type;	/* ()				*/
TYPE *boolean_type;	/* Boolean			*/
TYPE *exception_type;	/* ExceptionSpecies		*/
TYPE *char_type;	/* Char				*/
TYPE *natural_type;	/* Natural 			*/
TYPE *integer_type;	/* Integer 			*/
TYPE *rational_type;	/* Rational   			*/
TYPE *real_type;	/* Real       			*/
TYPE *comparison_type;  /* Comparison   		*/
TYPE *boxflavor_type;   /* BoxFlavor			*/
TYPE *copyflavor_type;  /* CopyFlavor			*/
TYPE *fileMode_type;    /* FileMode 			*/
TYPE *unknownKey_type;  /* Unknownkey   		*/
TYPE *aspecies_type;	/* ASpecies			*/
TYPE *list_fam;		/* List			 	*/
TYPE *box_fam;		/* Box		   		*/
TYPE *outfile_fam;	/* Outfile 			*/

TYPE *WrappedANY_type;		/* ANY (as a species)		*/
TYPE *WrappedEQ_type;		/* EQ (as a species)		*/
TYPE *WrappedORDER_type;	/* ORDER (as a species)		*/
TYPE *WrappedRANKED_type; 	/* RANKED (as a species)	*/
TYPE *WrappedENUMERATED_type; 	/* ENUMERATED (as a species)	*/
TYPE *WrappedREAL_type;		/* REAL (as a species)		*/
TYPE *WrappedRATIONAL_type;	/* RATIONAL (as a species)	*/
TYPE *WrappedINTEGER_type;	/* INTEGER (as a species)	*/
TYPE *WrappedRRING_type;	/* RRING (as a species)		*/
TYPE *WrappedRFIELD_type;	/* RFIELD (as a species)	*/
TYPE *WrappedRATRING_type;	/* RATRING (as a species)	*/


/****************************************************************
 *		ctc pointers for genera, etc.			*
 ****************************************************************
 * These are pointers to the class table entries for assorted	*
 * genera, commmunities and families.				*
 *								*
 *   RANKED_ctc		for RANKED.				*
 *   EQ_ctc		for EQ.					*
 *   ORDER_ctc		for ORDER				*
 *   ENUMERATED_ctc	for ENUMERATED.				*
 *   INTEGER_ctc	for INTEGER				*
 *   RATRING_ctc	for RATRING				*
 *   RATIONAL_ctc	for RATIONAL				*
 *   RFIELD_ctc		for RFIELD				*
 *   RRING_ctc		for RRING				*
 *   REAL_ctc		for REAL				*
 *   OPAQUE_ctc		for OPAQUE				*
 *   OPAQUE_EQ_ctc      for OPAQUE_EQ				*
 *   OPAQUE_ORDER_ctc   for OPAQUE_ORDER			*
 *   TRANSPARENT_ctc	for TRANSPARENT				*
 *   TRANSPARENT_EQ_ctc	for TRANSPARENT_EQ			*
 *   TRANSPARENT_ORDER_ctc for TRANSPARENT_ORDER		*
 *   List_ctc		Family List				*
 *   Box_ctc		Family Box				*
 ****************************************************************/

CLASS_TABLE_CELL *RANKED_ctc, *ENUMERATED_ctc, *EQ_ctc, *ORDER_ctc;
CLASS_TABLE_CELL *RRING_ctc, *INTEGER_ctc, *RFIELD_ctc;
CLASS_TABLE_CELL *RATRING_ctc, *REAL_ctc, *RATIONAL_ctc;
CLASS_TABLE_CELL *OPAQUE_ctc, *TRANSPARENT_ctc;
CLASS_TABLE_CELL *OPAQUE_EQ_ctc, *TRANSPARENT_EQ_ctc;
CLASS_TABLE_CELL *OPAQUE_ORDER_ctc, *TRANSPARENT_ORDER_ctc;
CLASS_TABLE_CELL *List_ctc, *Box_ctc;

/****************************************************************
 *			std_type_num				*
 *			std_var_num				*
 ****************************************************************
 * The standard species and families are numbered together,	*
 * as are the standard genera and communities.			*
 *								*
 * std_type_num is the standard-number that will be assigned	*
 * to the next standard species or family, when it is created.	*
 *								*
 * std_var_num is the standard-number that will be assigned	*
 * to the next standard genera or community when it is		*
 * created.							*
 ****************************************************************/

int std_type_num = 1;
int std_var_num  = 1;


/****************************************************************
 * 			STD_TYPES				*
 ****************************************************************
 * Create the standard species, families, genera and 		*
 * communities.  Set up the above variables.			*
 ****************************************************************/

void std_types(void)
{

/****************************************************************
 *			IMPORTANT NOTE				*
 *--------------------------------------------------------------*
 * If you modify this function, you will almost certainly 	*
 * need to recompile all .ast/.asi files. 			*
 ****************************************************************/

  CLASS_TABLE_CELL
	*Hermit_ctc,    	*Char_ctc,
	*Boolean_ctc,   	*Exception_ctc,
	*Natural_ctc,		*Integer_ctc,
	*Rational_ctc,  	*Real_ctc,
	*Outfile_ctc,		*UnknownKey_ctc,
	*Comparison_ctc, 	*FileMode_ctc,
	*BoxFlavor_ctc,		*ASpecies_ctc,
        *CopyFlavor_ctc;

  TYPE *dfault;

  building_standard_types = TRUE;
  FIRST_STANDARD_TYPE     = next_class_num;

  /*------------------------------------------------------------*
   ******************* Standard Families ************************
   *------------------------------------------------------------*/

  /*------------------------------------------------------------*
   * standard_fam(a,s,arg,opaque) creates a standard family	*
   * called std_type_ids[s], with domain arg.  The family is	*
   * opaque if opaque is true.  The family (without arg) is 	*
   * stored into a.						*
   *								*
   * NOTE: Since building_standard_type is true, standard_fam	*
   * does not place a family beneath OPAQUE or TRANSPARENT.	*
   * We must do that explicitly.				*
   *------------------------------------------------------------*/

  List_ctc     = standard_fam(&list_fam, LIST_TYPE_ID, any_type, 0);
  Box_ctc      = standard_fam(&box_fam,   BOX_TYPE_ID, any_type, 1);
  Outfile_ctc  = standard_fam(&outfile_fam, OUTFILE_TYPE_ID, any_type, 1);

  /*-------------------------------------------------------------*
   ******************** Standard Species  ************************
   *-------------------------------------------------------------*/

  /*------------------------------------------------------------*
   * standard_type(a,s) creates type std_type_id[s], stores it  *
   * in a, and returns its class table entry. 			*
   *------------------------------------------------------------*/

  Hermit_ctc    = standard_type(&hermit_type,    HERMIT_TYPE_ID);
  Char_ctc      = standard_type(&char_type,      CHAR_TYPE_ID);
  Exception_ctc = standard_type(&exception_type, EXCEPTION_TYPE_ID);
  ASpecies_ctc  = standard_type(&aspecies_type,  ASPECIES_TYPE_ID);
  Boolean_ctc   = standard_type(&boolean_type,   BOOLEAN_TYPE_ID);
  UnknownKey_ctc = standard_type(&unknownKey_type, UNKNOWNKEY_TYPE_ID);
  Natural_ctc 	= standard_type(&natural_type,   NATURAL_TYPE_ID);
  Integer_ctc 	= standard_type(&integer_type,   INTEGER_TYPE_ID);
  Rational_ctc 	= standard_type(&rational_type,  RATIONAL_TYPE_ID);
  Real_ctc 	= standard_type(&real_type,      REAL_TYPE_ID);
  FileMode_ctc  = standard_type(&fileMode_type,  FILEMODE_TYPE_ID);
  BoxFlavor_ctc  = standard_type(&boxflavor_type, BOXFLAVOR_TYPE_ID);
  CopyFlavor_ctc = standard_type(&copyflavor_type, COPYFLAVOR_TYPE_ID);
  Comparison_ctc = standard_type(&comparison_type, COMPARISON_TYPE_ID);
  LAST_TYPE_NUM  = Comparison_ctc->num;

  Hermit_num     = Hermit_ctc->num;
  Hermit_std_num = Hermit_ctc->std_num;
  Char_std_num   = Char_ctc->std_num;

  /*------------------------------------------------------------*
   *********************** Constructors *************************
   *------------------------------------------------------------*/

  /*------------------------------------------------------------*
   * The constructors are for pattern matching completeness     *
   * testing.  They tell the constructors that are available	*
   * for certain species and families.				*
   *------------------------------------------------------------*/

  bump_list(List_ctc->constructors = 
	    str_cons(std_id[NIL_ID], 
		     str_cons(std_id[CONS_SYM], NIL)));

  bmp_list(Boolean_ctc->constructors =
	    str_cons(std_id[FALSE_ID], str_cons(std_id[TRUE_ID], NIL)));

  bmp_list(BoxFlavor_ctc->constructors =
	    str_cons(std_id[NONSHARED_ID], str_cons(std_id[SHARED_ID], NIL)));

  bmp_list(CopyFlavor_ctc->constructors =
	    str_cons(std_id[NONSHARED_ID], 
		     str_cons(std_id[SHARED_ID], 
			      str_cons(std_id[SAME_ID], NIL))));

  bmp_list(Comparison_ctc->constructors =
	    str_cons(std_id[EQUAL_ID],
		     str_cons(std_id[GREATER_ID], 
			      str_cons(std_id[LESS_ID], NIL))));

  bmp_list(FileMode_ctc->constructors =
	   str_cons(std_id[VOLATILEMODE_ID],
		    str_cons(std_id[APPENDMODE_ID],
			     str_cons(std_id[BINARYMODE_ID], NIL))));

  bmp_list(Hermit_ctc->constructors = str_cons(std_id[HERMIT_TYPE_ID], NIL));


  /*---------------------------------------------------------------*
   ****************** Standard Genera and Communities **************
   *---------------------------------------------------------------*/

  /*-----------------------------------------------------------------*
   * standard_genus(s,d,m,e) creates genus s, with default species d.*
   * Use d = NULL_T if there is no default species.		     *
   *								     *
   * If m != NULL, then m is a member of s, to be used in  	     *
   * giving examples.                                                *
   *								     *
   * Parameter e should be					     *
   *    0  if s is not extensible				     *
   *    1  if s is extensible, but not explicitly extensible by	     *
   *       programs,						     *
   *    2  if s is explicitly extensible.			     *
   *								     *
   * standard_genus(s,t,e) returns the class table entry of genus s. *
   *								     *
   * standard_comm is similar to standard_genus, but creates a       *
   * community.							     *
   *								     *
   * NOTE: Since building_standard_type is true, standard_comm does  *
   * not place a community beneath OPAQUE or TRANSPARENT.  We must   *
   * do that explicitly.					     *
   *								     *
   * Note: Each of the dangerous defaults is set to an appropriate   *
   * secondary species.  However, since secondary species have not   *
   * yet been defined, we temporarily set the defaults to (), and    *
   * change them below.						     *
   *-----------------------------------------------------------------*/

  FIRST_STANDARD_GENUS = next_class_num;
  dfault = hermit_type;  /* Modified below - see comment above. */

  ctcs[0]->dangerous = 1;
  EQ_ctc	= standard_genus(EQG_TYPE_ID,	    dfault, natural_type, 2);
  EQ_ctc->dangerous = 1;
  ORDER_ctc	= standard_genus(ORDERG_TYPE_ID,    dfault, natural_type, 2);
  ORDER_ctc->dangerous = 1;
  RANKED_ctc    = standard_genus(RANKEDG_TYPE_ID,   dfault, natural_type, 1);
  RANKED_ctc->dangerous = 1;
  RANKED_ctc->partial = 1;
  ENUMERATED_ctc = standard_genus(ENUMERATEDG_TYPE_ID, dfault,
				  boolean_type, 1);
  ENUMERATED_ctc->dangerous = 1;
  ENUMERATED_ctc->partial = 1;

  REAL_ctc	= standard_genus(REALG_TYPE_ID,     natural_type,
				 natural_type, 0);
  RATIONAL_ctc  = standard_genus(RATIONALG_TYPE_ID, natural_type,
				 natural_type, 0);
  INTEGER_ctc	= standard_genus(INTEGERG_TYPE_ID,  natural_type,
				 natural_type, 0);
  RRING_ctc	= standard_genus(RRINGG_TYPE_ID,    integer_type,
				 integer_type, 0);
  RFIELD_ctc	= standard_genus(RFIELDG_TYPE_ID,   rational_type,
				 rational_type, 0);
  RATRING_ctc   = standard_genus(RATRINGG_TYPE_ID,  integer_type,
				 integer_type, 0);

  OPAQUE_ctc	        = standard_comm(OPAQUE_TYPE_ID,   NULL, 
					box_fam, 1, 1);
  OPAQUE_ctc->dangerous = 1;
  TRANSPARENT_ctc       = standard_comm(TRANSPARENT_TYPE_ID, NULL, 
					list_fam, 1, 0);
  TRANSPARENT_ctc->dangerous = 1;
  OPAQUE_EQ_ctc	        = standard_comm(OPAQUE_EQ_TYPE_ID,  NULL, 
					NULL, 2, 1);
  OPAQUE_ORDER_ctc      = standard_comm(OPAQUE_ORDER_TYPE_ID,   NULL, 
					NULL, 2, 1);
  TRANSPARENT_EQ_ctc    = standard_comm(TRANSPARENT_EQ_TYPE_ID, NULL, 
					list_fam, 2, 0);
  TRANSPARENT_EQ_ctc->dangerous = 1;
  TRANSPARENT_ORDER_ctc = standard_comm(TRANSPARENT_ORDER_TYPE_ID, NULL, 
					list_fam, 2, 0);
  TRANSPARENT_ORDER_ctc->dangerous = 1;

  /*------------------------------------*
   * Wrapped species and families.	*
   *------------------------------------*/

  WrappedANY_type 	= wrap_tf(vctcs[0]);
  WrappedEQ_type  	= wrap_tf(EQ_ctc);
  WrappedORDER_type 	= wrap_tf(ORDER_ctc);
  WrappedRANKED_type	= wrap_tf(RANKED_ctc);
  WrappedENUMERATED_type = wrap_tf(ENUMERATED_ctc);
  WrappedREAL_type 	= wrap_tf(REAL_ctc);
  WrappedRATIONAL_type 	= wrap_tf(RATIONAL_ctc);
  WrappedINTEGER_type 	= wrap_tf(INTEGER_ctc);
  WrappedRRING_type 	= wrap_tf(RRING_ctc);
  WrappedRFIELD_type 	= wrap_tf(RFIELD_ctc);
  WrappedRATRING_type 	= wrap_tf(RATRING_ctc);

  /*--------------------------------------------------------------*
   ******* Adjust dangerous defaults to wrapped species. **********
   *--------------------------------------------------------------*/

  bmp_type(ctcs[0]->CTC_DEFAULT = hermit_type);
  SET_TYPE(EQ_ctc->CTC_DEFAULT, WrappedEQ_type);
  SET_TYPE(ORDER_ctc->CTC_DEFAULT, WrappedORDER_type);
  SET_TYPE(RANKED_ctc->CTC_DEFAULT, WrappedRANKED_type);
  SET_TYPE(ENUMERATED_ctc->CTC_DEFAULT, WrappedENUMERATED_type);

  /*---------------------------------------------------------------------*
   ********** Relationships among standard types and varieties ***********
   *---------------------------------------------------------------------*/

  /*--------------------------------------------------------------*
   * copy_ancestors_tm(A,B,NOCHECK_LP) declares A to be a member  *
   * or subset of B.  A and B are the class table entries. 	  *
   *--------------------------------------------------------------*/

  copy_ancestors_tm(List_ctc,		EQ_ctc,		NOCHECK_LP);
  copy_ancestors_tm(Pair_ctc,		EQ_ctc,		NOCHECK_LP);
  copy_ancestors_tm(ORDER_ctc, 		EQ_ctc,		NOCHECK_LP);
  copy_ancestors_tm(TRANSPARENT_EQ_ctc,	EQ_ctc,		NOCHECK_LP);
  copy_ancestors_tm(OPAQUE_EQ_ctc,	EQ_ctc,		NOCHECK_LP);
  copy_ancestors_tm(List_ctc,		ORDER_ctc,	NOCHECK_LP);
  copy_ancestors_tm(Pair_ctc,		ORDER_ctc,	NOCHECK_LP);
  copy_ancestors_tm(REAL_ctc, 		ORDER_ctc,	NOCHECK_LP);
  copy_ancestors_tm(RANKED_ctc,         ORDER_ctc,      NOCHECK_LP);
  copy_ancestors_tm(TRANSPARENT_ORDER_ctc, ORDER_ctc,   NOCHECK_LP);
  copy_ancestors_tm(OPAQUE_ORDER_ctc,   ORDER_ctc,      NOCHECK_LP);
  copy_ancestors_tm(RATIONAL_ctc,       REAL_ctc,	NOCHECK_LP);
  copy_ancestors_tm(RRING_ctc,          REAL_ctc,       NOCHECK_LP);
  copy_ancestors_tm(RATRING_ctc,        RATIONAL_ctc,   NOCHECK_LP);
  copy_ancestors_tm(INTEGER_ctc,	RATIONAL_ctc,   NOCHECK_LP);
  copy_ancestors_tm(Integer_ctc,	INTEGER_ctc,	NOCHECK_LP);
  copy_ancestors_tm(Natural_ctc,	INTEGER_ctc,	NOCHECK_LP);
  copy_ancestors_tm(Integer_ctc,	RATRING_ctc,    NOCHECK_LP);
  copy_ancestors_tm(Rational_ctc,	RATRING_ctc,    NOCHECK_LP);
  copy_ancestors_tm(RFIELD_ctc,		RRING_ctc,	NOCHECK_LP);
  copy_ancestors_tm(RATRING_ctc,	RRING_ctc,	NOCHECK_LP);
  copy_ancestors_tm(Rational_ctc,	RFIELD_ctc,	NOCHECK_LP);
  copy_ancestors_tm(Real_ctc,		RFIELD_ctc,	NOCHECK_LP);
  copy_ancestors_tm(ENUMERATED_ctc,	RANKED_ctc,	NOCHECK_LP);
  copy_ancestors_tm(Natural_ctc,	RANKED_ctc,	NOCHECK_LP);
  copy_ancestors_tm(Char_ctc,           ENUMERATED_ctc, NOCHECK_LP);
  copy_ancestors_tm(Hermit_ctc,     	ENUMERATED_ctc, NOCHECK_LP);
  copy_ancestors_tm(Boolean_ctc,     	ENUMERATED_ctc, NOCHECK_LP);
  copy_ancestors_tm(BoxFlavor_ctc,      ENUMERATED_ctc, NOCHECK_LP);
  copy_ancestors_tm(CopyFlavor_ctc,     ENUMERATED_ctc, NOCHECK_LP);
  copy_ancestors_tm(Comparison_ctc,     ENUMERATED_ctc, NOCHECK_LP);
  copy_ancestors_tm(FileMode_ctc,       ENUMERATED_ctc, NOCHECK_LP);

  copy_ancestors_tm(Box_ctc,		OPAQUE_ctc,	NOCHECK_LP);
  copy_ancestors_tm(Outfile_ctc,	OPAQUE_ctc,	NOCHECK_LP);
  copy_ancestors_tm(OPAQUE_EQ_ctc,      OPAQUE_ctc,	NOCHECK_LP);
  copy_ancestors_tm(OPAQUE_ORDER_ctc,   OPAQUE_EQ_ctc,	NOCHECK_LP);
  copy_ancestors_tm(TRANSPARENT_EQ_ctc, TRANSPARENT_ctc, NOCHECK_LP);
  copy_ancestors_tm(TRANSPARENT_ORDER_ctc, TRANSPARENT_EQ_ctc, NOCHECK_LP);
  copy_ancestors_tm(List_ctc,		TRANSPARENT_ORDER_ctc,NOCHECK_LP);
  
  altered_hierarchy = TRUE;

  /*---------------------------------------------------------------*
   ***** Intersections among standard genera and communities  ******
   *---------------------------------------------------------------*/

  /*--------------------------------------------------------------------*
   * add_intersection_tm(A,B,C,NOCHECK_LP) declares C to be the 	*
   * intersection of A and B.  A, B and C are each class table entries. *
   *--------------------------------------------------------------------*/

  add_intersection_tm(RRING_ctc,  RATIONAL_ctc, RATRING_ctc,    NOCHECK_LP);
  add_intersection_tm(INTEGER_ctc,RATRING_ctc,  Integer_ctc,    NOCHECK_LP);
  add_intersection_tm(RRING_ctc,  INTEGER_ctc, 	Integer_ctc,	NOCHECK_LP);
  add_intersection_tm(RATRING_ctc,RFIELD_ctc,   Rational_ctc,   NOCHECK_LP);
  add_intersection_tm(RATIONAL_ctc, RFIELD_ctc, Rational_ctc,   NOCHECK_LP);
  add_intersection_tm(RANKED_ctc, REAL_ctc,     Natural_ctc,	NOCHECK_LP);
  add_intersection_tm(RANKED_ctc, RATIONAL_ctc, Natural_ctc,	NOCHECK_LP);
  add_intersection_tm(RANKED_ctc, INTEGER_ctc,  Natural_ctc,	NOCHECK_LP);
  add_intersection_tm(EQ_ctc,     TRANSPARENT_ctc, TRANSPARENT_EQ_ctc, 
		      NOCHECK_LP);
  add_intersection_tm(ORDER_ctc,  TRANSPARENT_ctc, TRANSPARENT_ORDER_ctc, 
		      NOCHECK_LP);
  add_intersection_tm(EQ_ctc,     OPAQUE_ctc, OPAQUE_EQ_ctc, NOCHECK_LP);
  add_intersection_tm(ORDER_ctc,  OPAQUE_ctc, OPAQUE_ORDER_ctc, NOCHECK_LP);
  add_intersection_tm(TRANSPARENT_EQ_ctc, ORDER_ctc, TRANSPARENT_ORDER_ctc,
		      NOCHECK_LP);
  add_intersection_tm(OPAQUE_EQ_ctc, ORDER_ctc, OPAQUE_ORDER_ctc,
		      NOCHECK_LP);

  building_standard_types = FALSE;
}
