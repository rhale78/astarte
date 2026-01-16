/**********************************************************************
 *    File:    misc/limits.h
 *    Purpose: Compiled in limits
 *    Author:  Karl Abrahamson
 **********************************************************************/

/************************************************************************
 * 			LEXICAL						*
 ************************************************************************/

/************************************************************************
 * MAX_UNPUT is the maximum number of characters that can be held    	*
 * in the lexer's push-back buffer at once.  It should be at least	*
 * as large as the maximum number of characters allowed between		*
 * two consecutive tokens in a single declaration, where it might be	*
 * necessary to scan ahead to see whether the next symbol is a ) or 	*
 * other end-of-expression marker.					*
 ************************************************************************/

#define MAX_UNPUT 	1000

/************************************************************************
 * MAX_UNPUT_TOKENS is the maximum number of tokens that can be held    *
 * in the push-back buffer at once.  Very few pushbacks of tokens are   *
 * done by the lexer.							*
 ************************************************************************/

#define MAX_UNPUT_TOKENS 4

/************************************************************************
 * MAX_ID_SIZE is the maximum length of an identifier.  This can	*
 * also be called MAX_NAME_LENGTH.					*
 ************************************************************************/

#define MAX_ID_SIZE     	256
#define MAX_NAME_LENGTH         MAX_ID_SIZE


/************************************************************************
 *			PARSER						*
 ************************************************************************/

/******************************************************************
 			TYPES
 ******************************************************************/

/****************************************************************
 * MAX_NUM_VARIETIES is the maximum number of genera and 	*
 * communities, combined, that can be defined.  This MUST be a	*
 * multiple of LONG_BITS.  It must not exceed MAX_NUM_SPECIES.	*
 ****************************************************************/

#define MAX_NUM_VARIETIES 256

/****************************************************************
 * MAX_NUM_SPEDIES is the maximum total number of species, 	*
 * families, genera and communities, combined, that can be	*
 * defined.  Do not change this without also changing the way	*
 * LTYPE values are stored.  (They are currently stored packed	*
 * three to a word, ten bits each.)				*
 ****************************************************************/

#define MAX_NUM_SPECIES 1024

/****************************************************************
 * ANCESTORS_SIZE is the number of LONGs used to represent a	*
 * bit-set giving the ancestors of a genus or community.	*
 ****************************************************************/

#define ANCESTORS_SIZE	(MAX_NUM_VARIETIES/LONG_BITS)

/********************************************************************
 			CODE GENERATION
 ********************************************************************/

/********************************************************************
 * At most 256*MAX_LOCAL_LABEL_CLASSES - 1 different local labels   *
 * can be used in one declaration.				    *
 ********************************************************************/

#define MAX_LOCAL_LABEL_CLASSES 8

/************************************************************************
 		MACHINE: IDENTIFIERS AND STRINGS
 ************************************************************************/

/************************************************************************
 * MAX_FILE_NAME_LENGTH is the maximum number of characters that can	*
 * occur in a full file name, including directory.			*
 ************************************************************************/

#define MAX_FILE_NAME_LENGTH    1024

/************************************************************************
 * MAX_SIMPLE_FILE_NAME_LENGTH is the maximum number of characters that *
 * can occur in a full file name, excluding directory.			*
 ************************************************************************/

#define MAX_SIMPLE_FILE_NAME_LENGTH 128

/************************************************************************
 * MAX_PACKAGE_NAME_LENGTH is the maximum number of characters that 	*
 * can occur in a package name.						*
 ************************************************************************/

#define MAX_PACKAGE_NAME_LENGTH 128

/************************************************************************
 * MAX_OVERLAP_MESSAGE_LENGTH is the maximum number of characters that 	*
 * can occur in a message indicating that two definitions overlap.	*
 ************************************************************************/

#define MAX_OVERLAP_MESSAGE_LENGTH 1500

/************************************************************************
 * MAX_LAZY_NAME_LENGTH is the maximum length of a string that 		*
 * describes a lazy object.  (Such strings are created by function	*
 * view.)								*
 ************************************************************************/

#define MAX_LAZY_NAME_LENGTH    (MAX_NAME_LENGTH + 15)

/************************************************************************
 * MAX_INT_CONST_LENGTH is the maximum length of a string that 		*
 * is an integer constant.						*
 ************************************************************************/

#define MAX_INT_CONST_LENGTH    256

/************************************************************************
 * MAX_REAL_CONST_LENGTH is the maximum length of a string that 	*
 * is a real/rational constant.						*
 ************************************************************************/

#define MAX_REAL_CONST_LENGTH   256

/************************************************************************
 * MAX_TYPE_STR_LEN is the maximum length of a string that describes	*
 * a type.								*
 ************************************************************************/

#define MAX_TYPE_STR_LEN	2000

/************************************************************************
 * MAX_GPRINT_LEN is the maximum length of a single string that is 	*
 * printed to a window in the interpreter.				*
 *									*
 * MAX_GPRINT_LEN should be at least MAX_FILE_NAME_LENGTH + 14		*
 ************************************************************************/

#define MAX_GPRINT_LEN		MAX_FILE_NAME_LENGTH + 20  

/*************************************************************************
 *		MACHINE: EXECUTES AND PACKAGES			 	 *
 *************************************************************************
 * See code generation for MAX_NUM_LOCAL_LABELS.			 *
 *************************************************************************/

/*************************************************************************
 *		MACHINE: MISC						 *
 *************************************************************************/

/************************************************************************
 * PRINT_ENT_TIME is the time permitted to print one object via view.	*
 ************************************************************************/

#define PRINT_ENT_TIME	10000

/************************************************************************
 * MAX_OPEN_FILES is the maximum number of files, not counting		*
 * stdin, stderr and stdout, that the interpreter can have open at	*
 * once.								*
 ************************************************************************/

#define MAX_OPEN_FILES  8 

/************************************************************************
 * MAX_OPEN_FONTS is the maximum number of fonts that the interpreter	*
 * can have open at once.						*
 ************************************************************************/

#define MAX_OPEN_FONTS	16
