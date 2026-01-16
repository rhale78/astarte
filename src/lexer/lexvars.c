/****************************************************************
 * File:    lexer/lexvars.c
 * Purpose: Declare variables used by lexer, and to communicate
 *          between lexer and parser.
 * Author:  Karl Abrahamson
 ****************************************************************/

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

#include "../misc/misc.h"
#include "../lexer/lexer.h"

/****************************************************************
 *			imported_files				*
 *			imported_package_names			*
 ****************************************************************
 * imported_files is a list of the names of the files that have *
 * been read.  It is used to prevent reading the same file 	*
 * twice.  The file names in imported_files are in internal	*
 * form, meaning that directories are separated by character	*
 * INTERNAL_DIR_SEP_CHAR.					*
 *								*
 * imported_package_names is parallel to imported_files.  It    *
 * holds the package names of the files that have been read.    *
 * The i-the member of imported_package_names is the name of 	*
 * the file whose name is the i-th string in imported_files.	*
 ****************************************************************/

STR_LIST* imported_files         = NULL;
STR_LIST* imported_package_names = NULL;

/****************************************************************
 *			chars_in_this_line			*
 ****************************************************************
 * chars_in_this_line keeps track of the number of characters   *
 * that have been read in the current line.  It is used, for	*
 * example, to do indent checks.				*
 ****************************************************************/

int chars_in_this_line;

/****************************************************************
 *			open_col				*
 ****************************************************************
 * open_col is the column where an 'open' token starts, when    *
 * that was the most recent token.  This is needed for indent   *
 * checking, since the column of the open token is taken to     *
 * be the start column of the construct.  If the most recent    *
 * token was not 'open', the open_col is -1.			*
 ****************************************************************/

int open_col = -1;

/****************************************************************
 *			LISTING_FILE				*
 *			ERR_FILE				*
 ****************************************************************
 * LISTING_FILE is the file where the listing is written, if    *
 * a listing is being written.  If there is no listing, then	*
 * LISTING_FILE is NULL.					*
 *								*
 * ERR_FILE is the file where error messages are written.  It   *
 * can be the same as LISTING_FILE, or can be different.  If    *
 * ERR_FILE is different, the error messages are written both   *
 * to ERR_FILE and to LISTING_FILE.				*
 ****************************************************************/

FILE* LISTING_FILE = NULL;
FILE* ERR_FILE     = NULL;

/****************************************************************
 *			unput_tokens_buffer			*
 *			unput_tokens_top			*
 ****************************************************************
 * Sometimes the lexer needs to push a full token back into the *
 * input.  unput_tokens_buffer holds information about tokens   *
 * that have been pushed back.  				*
 *								*
 * unput_tokens_top is the number of tokens that are in		*
 * unput_tokens_buffer.						*
 ****************************************************************/

struct unput_tokens_buffer_type unput_tokens_buffer[MAX_UNPUT_TOKENS];
int    unput_tokens_top = 0;

/****************************************************************
 *			last_tok				*
 *			last_last_tok				*
 *			last_popped_tok				*
 *			last_popped_tok_line			*
 *			last_yylval				*
 *			last_last_yylval			*
 ****************************************************************
 * last_tok is the previous token, just before the current one,	*
 * and last_yylval is its attribute.				*
 *								*
 * last_last_tok is the token before last_tok, and		*
 * last_last_yylval is its attribute.				*
 *								*
 * last_tok and last_last_tok are 0 if no previous token is	*
 * available.							*
 *								*
 * last_popped_tok is the token that was most recently popped   *
 * from the shadow stack, or is 0 if no last popped token is    *
 * available.  last_popped_tok_line is the line where 		*
 * last_popped_tok occurred, when last_popped_tok is not 0.	*
 ****************************************************************/

int last_tok = 0, last_last_tok = 0, last_popped_tok = 0;
int last_popped_tok_line;
YYSTYPE last_yylval, last_last_yylval;

/****************************************************************
 *			string_const				*
 *			string_const_len			*
 ****************************************************************
 * string_const points to a buffer that holds a string constant *
 * lexeme.  It is reallocated when necessary, so that long	*
 * strings can be accomodated.					*
 *								*
 * string_const_len is the physical length of the buffer	*
 * pointed to by string_const.					*
 ****************************************************************/

char HUGEPTR string_const;
LONG         string_const_len;

/****************************************************************
 *			abbrev_tok_role				*
 ****************************************************************
 * abbrev_tok_rold is the role associated with a type or	*
 * family macro.  It is needed because there is not room in	*
 * yylval for this information.					*
 ****************************************************************/

ROLE* abbrev_tok_role = NULL;

/****************************************************************
 *			import_level				*
 ****************************************************************
 * import_level is the number of recursive imports being done.  *
 * It is 0 in the main (outer) package.				*
 ****************************************************************/

int import_level = 0;

/************************************************************************
 *			gen_listing					*
 ************************************************************************
 * gen_listing is 							*
 *   0 for no listing,							*
 *									*
 *   1 for a listing that shows just the package being compiled, and    *
 *     just the implementation part when compiling an implemenation	*
 *     package,								*
 *									*
 *   2 for a listing that shows both the interface part and the		*
 *     implemenation part,						*
 *									*
 *   3 for a listing that shows everything that was read.		*
 *									*
 * gen_listing is set when the command line options are processed.	*
 ************************************************************************/

int gen_listing;

/****************************************************************
 *			import_start				*
 ****************************************************************
 * import_start is used, in a rather ugly way, to determine     *
 * when to start reading an imported file.  Normally, 		*
 * import_start holds 0.  When token 'import' is read, 		*
 * import_start is set to 1.  When a string constant is read,   *
 * import_start is checked.  If it is 1, then the string is     *
 * taken to be the name of a file to import.			*
 *								*
 * When token 'implements' is read, import_start is set to 2.	*
 * Again, a string constant is taken to be the name of a file   *
 * to read, but the context is set to EXPORT_CX instead of	* 
 * IMPORT_CX in this case.					*
 *								*
 * Great care must be taken with import_start.  It must be	*
 * set back to 0, so that a string constant will not		*
 * erroneously be taken to be an import, but not before the	*
 * string constant after 'import' is read.  Since we can	*
 * have a declaration of the form				*
 *								*
 *   Import x,y,z from "file"					*
 *								*
 * reading identifiers, commas or the word 'from' must not	*
 * set import_start to 0.					*
 * Other tokens should set import_start to 0.			*
 ****************************************************************/

char import_start;

/************************************************************************
 *			start_in_comment_mode				*
 ************************************************************************
 * start_in_comment_mode is true if the compiler should start reading   *
 * the main package as if in a long comment. 				*
 ************************************************************************/

Boolean start_in_comment_mode = FALSE;

/************************************************************************
 *			recent_begin_from_semicolon			*
 ************************************************************************
 * When a semicolon is read, it is converted to a end/begin pair, and   *
 * recent_begin_from_semicolon is set true.  This makes it possible for *
 * the mode handler in the parser to know that it should keep the mode  *
 * across a semicolon.  For example, if we see				*
 *									*
 *    Let {override}:							*
 *      a = 1;								*
 *      b = 1								*
 *    %Let								*
 *									*
 * recent_begin_from_semicolon will be true when starting declaration   *
 * Let b = 1.								*
 *									*
 * Care must be taken to set recent_begin_from_semicolon back to 0 at   *
 * the right time.							*
 ************************************************************************/

Boolean recent_begin_from_semicolon = FALSE;


/************************************************************************
 *				opContext				*
 *				force_no_op				*
 ************************************************************************
 * opContext is 0 if the next token cannot be a binary operator, and    *
 * 1 if the next token can be a binary operator.			*
 *									*
 * force_no_op is set true to force the next token not to be a binary   *
 * or unary operator, regardless of the value of opContext.		*
 *									*
 * Each time an identifier is processed, force_no_op is cleared.  So    *
 * it should not be used for long-term suppression of binary operators. *
 * long_force_no_op has the same effect as force_no_op, but it is	*
 * not cleared automatically.						*
 ************************************************************************/

Boolean opContext, force_no_op, long_force_no_op = 0;

/************************************************************************
 *				panic_mode				*
 ************************************************************************
 * When the parser encounters a syntax error, it panics, consuming 	*
 * tokens until it is able to recover.  In normal operation, the lexer  *
 * tries to fix up some errors.  (See lexerr.c.)  But when the parser   *
 * is panicing, the lexer should not try to fix errors.  During a       *
 * panic, panic_mode is set TRUE, to tell the lexer not to behave in    *
 * a raw mode.								*
 ************************************************************************/

Boolean panic_mode = FALSE;

/************************************************************************
 *				reading_string				*
 ************************************************************************
 * reading_string is true while reading a string constant.  It is used  *
 * to detect an end-of-file that occurs inside a string constant.	*
 ************************************************************************/

Boolean reading_string = FALSE;

/************************************************************************
 *				seen_package_end			*
 ************************************************************************
 * seen_package_end is true if we have read the %package at the end     *
 * of the package.  It is used to handle errors at the end of a		*
 * package.								*
 ************************************************************************/

Boolean seen_package_end = FALSE;

/************************************************************************
 *			no_line_num					*
 *			no_print_this_token				*
 ************************************************************************
 * These are used to suppress the usual listing actions.  no_line_num   *
 * is set to prevent a line number from being printed when one has      *
 * already been printed.  no_print_this_token is set to suppress	*
 * printing of the lexeme of a token that has already been printed.	*
 ************************************************************************/

Boolean no_line_num = FALSE;
Boolean no_print_this_token = FALSE;

/************************************************************************
 * 				no_load_standard			*
 ************************************************************************
 * no_load_standard is set TRUE if we should not load the standard	*
 * package.								*
 ************************************************************************/

Boolean no_load_standard = FALSE;

/************************************************************************
 *				bring_context				*
 ************************************************************************
 * bring_context is TRUE inside a bring-declaration, and FALSE outside. *
 * Currently, it is set but not used.					*
 ************************************************************************/

Boolean bring_context = FALSE;

/************************************************************************
 *				in_operator_dcl				*
 ************************************************************************
 * in_operator_dcl is TRUE inside an operator declaration, and FALSE	*
 * outside.  It is used to handle (:, which should be treated as	*
 * ( : in an operator declaration, and as the token (: elsewhere.	*
 ************************************************************************/

Boolean in_operator_dcl = FALSE;

/************************************************************************
 *				verbose_mode				*
 ************************************************************************
 * verbose_mode is true if the lexer should tell what it is doing when  *
 * looking for files to compile.					*
 ************************************************************************/

Boolean verbose_mode = FALSE;

