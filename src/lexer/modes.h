/****************************************************************
 * File:    lexer/modes.h
 * Purpose: Definitions of declaration modes.
 * Author:  Karl Abrahamson
 ****************************************************************/


/****************************************************************
 *			 DEFINE MODES				*
 ****************************************************************
 * These are used as the attributes of define options.		*
 * They are coded by bit positions.				*
 ****************************************************************/

/*----------------------------------------------------------------*
 * Modes that are passed on to interpreter. The mask values are   *
 * used in the interpreter, where modes are very simple.  The	  *
 * mask is two to the mode power.  For example, DEFAULT_MODE is 4,*
 * so DEFAULT_MODE_MASK is 2^4 = 16.				  *
 *								  *
 * The first three of the following should not be relevant to	  *
 * var exprs.  They use those three bits for storing the kind of  *
 * variable.							  *
 *----------------------------------------------------------------*/

#define FORCE_MODE			0
#define FORCE_MODE_MASK       		1

#define PRIMITIVE_MODE			1
#define PRIMITIVE_MODE_MASK		2

#define COPY_MODE			2
#define COPY_MODE_MASK			4

#define OVERRIDES_MODE			3
#define OVERRIDES_MODE_MASK  		8

#define DEFAULT_MODE			4
#define DEFAULT_MODE_MASK    		0x10

#define UNDERRIDES_MODE   		5
#define UNDERRIDES_MODE_MASK 		0x20

#define IRREGULAR_MODE			6
#define IRREGULAR_MODE_MASK		0x40

/*--------------------------------------------------*
 * Modes that are significant only to the compiler. *
 *--------------------------------------------------*/

#define ASSUME_MODE			8
#define ASSUME_MODE_MASK     		0x100L

#define NOAUTO_MODE			9
#define NOAUTO_MODE_MASK		0x200L

#define NOEQUAL_MODE			10
#define NOEQUAL_MODE_MASK    		0x400L

#define AHEAD_MODE			11
#define AHEAD_MODE_MASK			0x800L

#define NODESCRIP_MODE			12
#define NODESCRIP_MODE_MASK		0x1000L

#define DANGEROUS_MODE			13
#define DANGEROUS_MODE_MASK		0x2000L

#define NODOLLAR_MODE			14
#define NODOLLAR_MODE_MASK		0x4000L

/* INCOMPLETE_MODE is shared with NOCAST_MODE */

#define INCOMPLETE_MODE			15
#define INCOMPLETE_MODE_MASK		0x8000L
#define NOCAST_MODE			15
#define NOCAST_MODE_MASK		0x8000L

#define NO_EXPORT_MODE			16
#define NO_EXPORT_MODE_MASK		0x10000L

#define NO_EXPECT_MODE			17
#define NO_EXPECT_MODE_MASK		0x20000L

#define PRIVATE_MODE			18
#define PRIVATE_MODE_MASK		0x40000L

#define STRONG_MODE			19
#define STRONG_MODE_MASK		0x80000L

#define IMPORTED_MODE			20
#define IMPORTED_MODE_MASK		0x100000L

#define PARTIAL_NO_EXPECT_MODE		21
#define PARTIAL_NO_EXPECT_MODE_MASK  	0x200000L

#define HIDE_MODE			22
#define HIDE_MODE_MASK			0x400000L

#define PATTERN_MODE			23
#define PATTERN_MODE_MASK		0x800000L

#define MISSING_MODE			24
#define MISSING_MODE_MASK		0x1000000L

#define RANKED_MODE			25
#define RANKED_MODE_MASK		0x2000000L

#define TRANSPARENT_MODE		26
#define TRANSPARENT_MODE_MASK		0x4000000L

#define SAFE_MODE			27
#define SAFE_MODE_MASK			0x8000000L

#define PARTIAL_MODE			28
#define PARTIAL_MODE_MASK		0x10000000L

#define NOPULL_MODE			29
#define NOPULL_MODE_MASK		0x20000000L

#define PROTECTED_MODE			30
#define PROTECTED_MODE_MASK		0x40000000L

#define IMPERATIVE_MODE			31
#define IMPERATIVE_MODE_MASK		0x80000000L

/****************************************************************
 * The following definitions tell which modes can occur with	*
 * which declarations. 						*
 ****************************************************************/
	
/*--------------------------------*
 * Modes for import declarations. *
 *--------------------------------*/

#define IMPORT_DCL_MODES       	( 0)

/*---------------------------------*
 * Modes for species declarations. *
 *---------------------------------*/

#define TYPE_DCL_MODES		( OVERRIDES_MODE_MASK\
				| UNDERRIDES_MODE_MASK\
				| DEFAULT_MODE_MASK\
				| ASSUME_MODE_MASK\
			        | PRIVATE_MODE_MASK\
				| PROTECTED_MODE_MASK\
				| NOEQUAL_MODE_MASK\
				| NODOLLAR_MODE_MASK\
				| IMPERATIVE_MODE_MASK\
				| RANKED_MODE_MASK\
				| TRANSPARENT_MODE_MASK\
				| PARTIAL_MODE_MASK\
				| NOCAST_MODE_MASK\
			        | NOPULL_MODE_MASK)

/*---------------------------------------------*
 * Modes for genus and community declarations. *
 *---------------------------------------------*/

#define CG_DCL_MODES		(TRANSPARENT_MODE_MASK)

/*--------------------------------*
 * Modes for relate declarations. *
 *--------------------------------*/

#define RELATE_DCL_MODES	( NO_EXPECT_MODE_MASK\
				| PARTIAL_NO_EXPECT_MODE_MASK\
				| AHEAD_MODE_MASK)

/*---------------------------------*
 * Modes for default declarations. *
 *---------------------------------*/

#define DEFAULT_DCL_MODES       ( DANGEROUS_MODE_MASK)

/*----------------------------------------*
 * Modes for expect species declarations. *
 *----------------------------------------*/

#define EXPECT_TYPE_MODES       ( NOEQUAL_MODE_MASK\
				| NODOLLAR_MODE_MASK\
				| IMPERATIVE_MODE_MASK\
			        | IMPORTED_MODE_MASK\
				| TRANSPARENT_MODE_MASK\
				| PARTIAL_MODE_MASK\
				| NOPULL_MODE_MASK)

/*-----------------------------------*
 * Modes for exception declarations. *
 *-----------------------------------*/

#define EXCEPTION_DCL_MODES	( PRIVATE_MODE_MASK\
				| PROTECTED_MODE_MASK)

/*-----------------------------------------*
 * Modes for let and define declarations. *
 *-----------------------------------------*/

#define DEFINE_DCL_MODES	( OVERRIDES_MODE_MASK\
				| UNDERRIDES_MODE_MASK\
				| DEFAULT_MODE_MASK\
				| COPY_MODE_MASK\
				| IRREGULAR_MODE_MASK\
				| ASSUME_MODE_MASK\
				| PRIVATE_MODE_MASK\
				| PROTECTED_MODE_MASK\
				| NO_EXPORT_MODE_MASK)

/*-------------------------------*
 * Modes for bring declarations. *
 *-------------------------------*/

#define BRING_DCL_MODES		(DEFINE_DCL_MODES\
				| SAFE_MODE_MASK)

/*-----------------------------------------------------*
 * Modes for expect declarations that expect entities. *
 *-----------------------------------------------------*/

#define EXPECT_DCL_MODES	( ASSUME_MODE_MASK\
				| PRIVATE_MODE_MASK\
				| PROTECTED_MODE_MASK\
				| INCOMPLETE_MODE_MASK\
				| AHEAD_MODE_MASK\
				| OVERRIDES_MODE_MASK\
				| UNDERRIDES_MODE_MASK\
				| DEFAULT_MODE_MASK\
				| IRREGULAR_MODE_MASK\
				| MISSING_MODE_MASK\
				| PATTERN_MODE_MASK\
				| NO_EXPORT_MODE_MASK\
				| IMPORTED_MODE_MASK\
				| NOAUTO_MODE_MASK\
				| STRONG_MODE_MASK\
			        | NODESCRIP_MODE_MASK)

/*---------------------------------*
 * Modes for missing declarations. *
 *---------------------------------*/

#define MISSING_DCL_MODES       ( STRONG_MODE_MASK\
				| IMPORTED_MODE_MASK\
				| HIDE_MODE_MASK)

/*---------------------------------*
 * Modes for pattern declarations. *
 *---------------------------------*/

#define PATTERN_DCL_MODES	( INCOMPLETE_MODE_MASK\
				| UNDERRIDES_MODE_MASK)

/*--------------------------------*
 * Modes for abbrev declarations. *
 *--------------------------------*/

#define ABBREV_DCL_MODES	( NO_EXPORT_MODE_MASK)

/*--------------------------------*
 * Modes for assume declarations. *
 *--------------------------------*/

#define ASSUME_DCL_MODES	( NO_EXPORT_MODE_MASK)

/*-------------------------------------*
 * Modes for description declarations. *
 *-------------------------------------*/

#define DESCRIPTION_DCL_MODES	( AHEAD_MODE_MASK\
				| IMPORTED_MODE_MASK)

/*-------------------------------------*
 * Modes for operator declarations.    *
 *-------------------------------------*/

#define OPERATOR_DCL_MODES	(OVERRIDES_MODE_MASK)

#define SET_MODE(x,m) set_mode(&(x), m)

/********** Variables *************/

extern MODE_TYPE		this_mode;
extern MODE_TYPE		propagate_mode;
extern MODE_TYPE		null_mode;
extern MODE_TYPE		defopt_mode;

/*********** Prototypes ****************/

void 		handle_simple_mode	(MODE_TYPE *mode, char *id, 
					 Boolean lowok);
void		check_modes		(LONG allowed_modes, int line);
void		transfer_mode		(void);
void 		add_var_mode		(MODE_TYPE *mode, char *id);
void 		add_var_list_mode	(MODE_TYPE *mode, char *id, 
					 STR_LIST *ids);
Boolean 	has_mode		(const MODE_TYPE *mode, int mode_bit);
LONG 		get_define_mode		(MODE_TYPE *mode);
STR_LIST* 	get_mode_noexpects	(MODE_TYPE *mode);
STR_LIST* 	get_mode_visible_in	(MODE_TYPE *mode);
void 		add_mode		(MODE_TYPE *mode, int mode_bit);
MODE_TYPE* 	simple_mode		(int mode_bit);
MODE_TYPE* 	copy_mode		(MODE_TYPE *mode);
void 		do_copy_mode		(MODE_TYPE *A, MODE_TYPE *B);
Boolean 	mode_equal		(const MODE_TYPE *A, 
					 const MODE_TYPE *B);
void 		modify_mode		(MODE_TYPE *A, MODE_TYPE *B,
					 Boolean isvar);
void		print_mode		(MODE_TYPE *mode, int n);
void 		init_modes		(void);

/* From allocors.c: */

void 			bump_mode		(MODE_TYPE *r);
void 			drop_mode		(MODE_TYPE *r);
void 			set_mode		(MODE_TYPE **x, MODE_TYPE *r);
