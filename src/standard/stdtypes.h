/*********************************************************************
 * File:    standard/stdtypes.h
 * Author:  Karl Abrahamson
 * Purpose: Create standard types, varieties, etc.
 *********************************************************************/

extern int   FIRST_STANDARD_GENUS; /* Number of first standard genus or  *
				    * community.			 */
extern int   FIRST_STANDARD_TYPE;  /* Number of first standard type or fam */
extern UBYTE Hermit_num;	/* Number of type () 		*/
extern UBYTE Hermit_std_num;	/* Standard number of type () 	*/
extern UBYTE Char_std_num;      /* Standard number of type Char */
extern int   LAST_TYPE_NUM;	/* Number of last standard type */

extern TYPE* hermit_type;	/* ()				*/
extern TYPE* boolean_type;	/* Boolean			*/
extern TYPE* exception_type;	/* ExceptionSpecies		*/
extern TYPE* char_type;		/* Char				*/
extern TYPE* natural_type;	/* Natural 			*/
extern TYPE* integer_type;	/* Integer 			*/
extern TYPE* rational_type;	/* Rational    			*/
extern TYPE* real_type;		/* Real	    			*/
extern TYPE* outfile_fam;	/* Outfile 			*/
extern TYPE* comparison_type;   /* Comparison = {less, equal, greater} */
extern TYPE* boxflavor_type;    /* BoxFlavor = (nonshared,shared)*/
extern TYPE* copyflavor_type;   /* CopyFlavor = (nonshared,shared,same)*/
extern TYPE* fileMode_type;     /* FileMode = {volatileMode, appendMode, 
				   binaryMode} 			*/
extern TYPE* unknownKey_type;   /* UnknownKey   		*/
extern TYPE* aspecies_type;	/* ASpecies			*/
extern TYPE* list_fam;		/* List 			*/
extern TYPE* box_fam;		/* Box				*/

extern TYPE* WrappedANY_type;		/* Wrapped ANY		*/
extern TYPE* WrappedEQ_type;		/* Wrapped EQ		*/
extern TYPE* WrappedORDER_type;		/* Wrapped ORDER	*/
extern TYPE* WrappedRANKED_type; 	/* Wrapped RANKED	*/
extern TYPE* WrappedENUMERATED_type; 	/* Wrapped ENUMERATED	*/
extern TYPE* WrappedREAL_type;		/* Wrapped REAL		*/
extern TYPE* WrappedRATIONAL_type;	/* Wrapped RATIONAL	*/
extern TYPE* WrappedINTEGER_type;	/* Wrapped INTEGER	*/
extern TYPE* WrappedRRING_type;		/* Wrapped RRING	*/
extern TYPE* WrappedRFIELD_type;	/* Wrapped RFIELD	*/
extern TYPE* WrappedRATRING_type;	/* Wrapped RATRING	*/


extern CLASS_TABLE_CELL *OPAQUE_ctc, *TRANSPARENT_ctc;
extern CLASS_TABLE_CELL *RANKED_ctc, *ENUMERATED_ctc, *EQ_ctc, *ORDER_ctc;
extern CLASS_TABLE_CELL *RRING_ctc, *INTEGER_ctc, *RFIELD_ctc;
extern CLASS_TABLE_CELL *RATRING_ctc, *REAL_ctc, *RATIONAL_ctc;
extern CLASS_TABLE_CELL *List_ctc, *Box_ctc;

extern int std_type_num, std_var_num;

void std_types(void);

