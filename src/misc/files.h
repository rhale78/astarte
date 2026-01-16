/********************************************************************
 * File:    misc/files.h
 * Purpose: Define names of standard files.
 * Author:  Karl Abrahamson
 ********************************************************************/

/************************************************************************
 * NULL_AST_NAME 	is the name of the file that contains the	*
 *			empty package, needed for compiling the		*
 *			standard package.  Don't include .ast in this 	*
 *			name.						*
 ************************************************************************/

# define NULL_AST_NAME  	"null"

/************************************************************************
 * STANDARD_AST_NAME    is the name of the file that contains standard 	*
 *			package.  It must be the same as the name of 	*
 *			the package, as written in the file.  Don't 	*
 *			include .ast in this name.			*
 *									*
 * STANDARD_AST_NAME_LEN is the length of STANDARD_AST_NAME.		*
 ************************************************************************/

# define STANDARD_AST_NAME  	"standard"
# define STANDARD_AST_NAME_LEN 	8

/************************************************************************
 * ALT_STANDARD_AST_NAME is an alternate form of the standard package 	*
 *			name.  It should be different from 		*
 *			STANDARD_AST_NAME, and cannot be used for user 	*
 *			package names.					*
 ************************************************************************/

# define ALT_STANDARD_AST_NAME 	"Standard"

/************************************************************************
 * ASTC_OPTION_FILE	is the file that contains options for astc.	*
 *			It is used to show the options.			*
 ************************************************************************/

# define ASTC_OPTION_FILE	"astc.txt"

/************************************************************************
 * ASTR_OPTION_FILE	is the file that contains options for astr.	*
 *			It is used to show the options.			*
 ************************************************************************/

# define ASTR_OPTION_FILE	"astr.txt"

/************************************************************************
 * ERR_MSGS_FILE	is the file that contains compiler error	*
 *			messages.  It is usually called messages.txt.	*
 ************************************************************************/

# define ERR_MSGS_FILE		"messages.txt"

/************************************************************************
 * NOVICE_ERR_MSGS_FILE	is the file that contains compiler error	*
 *			messages for novice mode.  It is usually	*
 *			called novice.txt.				*
 ************************************************************************/

# define NOVICE_ERR_MSGS_FILE	"novice.txt"

/************************************************************************
 * SYNTAX_FORM_FILE	is the file that contains syntax forms for	*
 *			the compiler to report.				*
 * ALT_SYNTAX_FORM_FILE is the file that contains syntax forms for	*
 * 			special constructs whose tokens are defined	*
 *			in parser/tokmod.h.				*
 ************************************************************************/

# define SYNTAX_FORM_FILE	"syntax.txt"
# define ALT_SYNTAX_FORM_FILE	"asyntax.txt"

/************************************************************************
 * DIE_MSG_FILE		is the file that contains messages that are 	*
 *			printed when the compiler or interpreter die 	*
 *			due to an internal error, or because a file is 	*
 *			missing.  It is usually called die.txt.		*
 ************************************************************************/

# define DIE_MSG_FILE   	"die.txt"

/************************************************************************
 * ERR_FILE_NAME,	The MSWIN version sometimes needs a place	*
 * DEATH_FILE_NAME	to dump error messages.  ERR_FILE_NAME is it,	*
 *			located in directory MESSAGE_DIR.  When the	*
 *			compiler dies due to an internal error, it	*
 *			creates file DEATH_FILE_NAME, in directory	*
 *			MESSAGE_DIR.					*
 ************************************************************************/

#define ERR_FILE_NAME		"error.txt"
#define DEATH_FILE_NAME		"death.txt"

/************************************************************************
 * TRAP_MSG_FILE	is the file that contains messages that are	*
 *			printed when the interpreter traps.  It is	*
 *			usually called trapmsg.txt.			*
 ************************************************************************/

# define TRAP_MSG_FILE  	"trapmsg.txt"

/************************************************************************
 * INSTR_NAME_FILE      is the file that contains names of instructions.*
 *			It is usually called instname.txt.  This file,  *
 *			and the next two, are used when the interpreter *
 *			traps an exception, to print the name of the 	*
 *			instruction that caused the trap.		*
 *			They are also used by the disassembler.		*
 ************************************************************************/

# define INSTR_NAME_FILE  	"instname.txt"

/************************************************************************
 * UN_INSTR_NAME_FILE   is the file that contains names of instructions *
 *			that follow instruction UNARY_PREFIX_I.  It is	*
 *			usually called unname.txt.			*
 ************************************************************************/

# define UN_INSTR_NAME_FILE  	"unname.txt"

/************************************************************************
 * BIN_INSTR_NAME_FILE  is the file that contains names of intsructions	*
 *			that follow instructin BINARY_PREFIX_I.  It is	*
 *			usually called binname.txt.			*
 ************************************************************************/

# define BIN_INSTR_NAME_FILE 	"binname.txt"

/************************************************************************
 * DEBUG_MSG_S_FILE	is the file dmsgs.txt (only used when		*
 *			DEBUG is defined).				*
 ************************************************************************/

# define DEBUG_MSG_S_FILE   	"dmsgs.txt"

/************************************************************************
 * DEBUG_MSG_T_FILE	is the file dmsgt.txt (only used when		*
 *			DEBUG is defined).				*
 ************************************************************************/

# define DEBUG_MSG_T_FILE   	"dmsgt.txt"

/************************************************************************
 * DEBUG_MSG_I_FILE	is the file dmsgi.txt (only used when		*
 *			DEBUG is defined).				*
 ************************************************************************/

# define DEBUG_MSG_I_FILE   	"dmsgi.txt"

/************************************************************************
 *			Other files					*
 ************************************************************************/

/************************************************************************
 * AST_RTS 		is the file where the run-time stack us dumped	*
 *			on error.  It will be in the current directory.	*
 ************************************************************************/

#define AST_RTS			"ast.rts"



