/****************************************************************
 * File:    lexer/preflex.c
 * Purpose: Support routines for lexer.  These functions are
 *	    concerned with 
 *            (1) the front end of the lexer, where imports 
 *                are dealt with, 
 *            (2) the back end of the lexer, where pushbacks are handled.
 *
 *	    Function yylexx, which is the lexer that
 *	    the parser calls, is defined here.
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

/****************************************************************
 * This file is responsible for managing imports.  It 		*
 * maintains a stack of files being imported, and keeps 	*
 * information about the current input file up to date at the	*
 * start and end of an import.  				*
 *								*
 * The information is kept in data structures that are kept in	*
 * file tables/fileinfo.c.  This file and tables/fileinfo.c are *
 * fairly tightly coupled, and should be viewed together if     *
 * modifications in the way imports are handled are to be made.	*
 *								*
 * This file also handles the back end of the lexer, where	*
 * pushbacks are handled.					*
 ****************************************************************/

#include <string.h>
#ifdef USE_MALLOC_H
#  include <malloc.h>
#else
#  include <stdlib.h>
#endif
#include <ctype.h>
#include "../misc/misc.h"
#include "../tables/tables.h"
#include "../exprs/expr.h"
#include "../lexer/lexer.h"
#include "../lexer/modes.h"
#include "../parser/parser.h"
#include "../parser/tokens.h"
#include "../alloc/allocate.h"
#include "../utils/lists.h"
#include "../utils/hash.h"
#include "../utils/strops.h"
#include "../utils/filename.h"
#include "../error/error.h"
#include "../dcls/dcls.h"
#include "../generate/generate.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/****************************************************************
 *	       		yytext	 				*
 ****************************************************************
 * The current lexeme is stored in yytext.  This array is 	*
 * declared in the automatically generated lexer. 		*
 ****************************************************************/

extern char yytext[];		

/****************************************************************
 *	       		import_path 				*
 *	       		default_import_path 			*
 ****************************************************************
 * Variable import_path holds a list of the directories to 	*
 * search for imports.						*
 *								*
 * default_import_path is the import_path value to start with,  *
 * from compiler.c.						*
 *								*
 * These directory names might be in internal or external form. *
 ****************************************************************/

STR_LIST* default_import_path;
STR_LIST* import_path = NULL;  

/****************************************************************
 *	       		yyin	 				*
 ****************************************************************
 * yyin is the current input file. It is declared in the flex	*
 * lexer file.							*
 ****************************************************************/

extern FILE* yyin;

/****************************************************************
 *			INIT_LEXICAL_L				*
 ****************************************************************
 * Initialize the lexer.  Push the standard package and the     *
 * package from the command line onto the input stack.		*
 * String this_file is the package from the command line.	*
 ****************************************************************/

Boolean init_lexical_l(char *this_file)
{
  /*------------------------------------------------------------*
   * Allocate the initial string_const array.  (It will be	*
   * reallocated if it is not large enough.			*
   *------------------------------------------------------------*/

  string_const      = (char HUGEPTR) alloc(MAX_STRING_LEN + 1);
  string_const_len  = MAX_STRING_LEN;

  /*--------------------*
   * Prepare for input. *
   *--------------------*/

  error_reported = FALSE;

  /*-------------------------------------------------------------*
   * We will start with the standard package being read, and the *
   * argument package sitting on the file stack waiting to be    *
   * resumed.  So we are initially in an import, and set         *
   * import_level = 1.  This suppresses line numbering, since    *
   * no listing is done in imports.  So line number 1 does not   *
   * get printed.  See compiler.c for printing of line number 1. *
   *-------------------------------------------------------------*/

  import_level = 1;

  /*--------------------------------------------*
   * Place the input file on the input stack. 	*
   * If the input file is supposed to start   	*
   * in a long comment, put %<<<< at the front	*
   * of it, to cause the lexer to enter a long	*
   * comment.  (See lexer.lex for handling	*
   * of %<<<<.)					*
   *--------------------------------------------*/

  if(!push_input_file(this_file)) return FALSE;
  if(start_in_comment_mode) unput_str("%<<<<");

  /*------------------------------------------------*
   * Place the standard package on the input stack. *
   *------------------------------------------------*/

  if(!no_load_standard) {
    char* std_name = 
           (char*) BAREMALLOC(STANDARD_AST_NAME_LEN + 6 + strlen(STD_DIR));
    sprintf(std_name, "%s%c%s", STD_DIR, INTERNAL_DIR_SEP_CHAR, 
	    compiling_standard_asi 
	      ? (NULL_AST_NAME ".ast") 
	      : (STANDARD_AST_NAME ".ast"));
    push_input_file(std_name);
    FREE(std_name);
    main_context = INIT_CX;
    gen_code = FALSE;
    if(error_occurred) return FALSE;
    current_package_name = standard_package_name;
    current_import_seq_num = 0;
  }
  else {
    import_level = 0;
  }

  last_tok = last_last_tok = 0;

  if(file_info_st == NULL) return FALSE;
  return TRUE;
}


/****************************************************************
 *		        IMPORT_L				*
 ****************************************************************
 * Handle tokens "import" and "implements".  n is 1 for token   *
 * "import" and 2 for "implements".				*
 ****************************************************************/

void import_l(int n)
{
# ifdef DEBUG
    if(trace_lexical) {
      trace_t(322, import_level);
    }
# endif

  zero_context_l(RBRACE_TOK);
  import_start = n; 
}


/****************************************************************
 *		        END_IMPORT_L				*
 ****************************************************************
 * End an import.						*
 ****************************************************************/

void end_import_l()
{
  /*--------------------------------------------------------------------*
   * Copy tables if the package being ended is an interface package,	*
   * so that the implementation and interface packages have the		*
   * same tables. 							*
   *									*
   * In any event, pop the top file from the file stack.		*
   *--------------------------------------------------------------------*/

  if(file_info_st->is_interface_package) {
    copy_tables_down();
    archive_main_frame();
    pop_input_file(FALSE);
  }
  else {
    pop_input_file(TRUE);
  }

  import_level--;
  copy_expect_types = TRUE;

# ifdef DEBUG
    if(import_level == 0) {
      set_traces(trace_arg);
    }
    if(trace_lexical) trace_t(323, import_level);
# endif

  /*------------------------------------------------------------*
   * Suppress printing of Implementation or %Package by yylexx. *
   *------------------------------------------------------------*/

  no_print_this_token = TRUE;

  /*------------------------------------------------------------*
   * Cancel reports, or, if show_all_reports is true, the do all*
   * remaining reports.  Also, when show_all_reports is true,	*
   * show that we are back to the previous package.		*
   *------------------------------------------------------------*/

  if(show_all_reports) {
    print_reports_p();
    err_print_str("\n--Continue with file %s--\n", current_file_name);
  }
  else if(reports != NULL) {
    free_report_record(reports);
    reports = NULL;
  } 
}


/*******************************************************************
 *			BODY_TOK_L				   *
 *******************************************************************
 * Handle token "implementation".  If this begins the 		   *
 * implementation of the package that is being compiled, archive   *
 * the main export part for second imports.  			   *
 *								   *
 * If this package is being imported, however, 'implementation'    *
 * signals the end of the import.		   		   *
 *******************************************************************/

void body_tok_l(void)
{
  if(import_level == 0) {
    zero_context_l(0); 
    archive_main_frame();
  }
  else {
    end_import_l();
  }
}


/****************************************************************
 *			TRY_SECOND_IMPORT			*
 ****************************************************************
 * Perform a second import of file infileName, except complain	*
 * about a cyclic import if currently reading this file, and	*
 * not in body.							*
 *								*
 * A second import is done when an import is requested of a 	*
 * file that was already imported earlier.  Most of what	*
 * is done here is actually contained in tables/fileinfo.c.	*
 *								*
 * MODE is the mode of the import declaration.  It is a safe    *
 * pointer: it does not live longer than this function call.	*
 ****************************************************************/

PRIVATE void try_second_import(char *infileName, MODE_TYPE *mode)
{
  IMPORT_STACK *p;

  /*----------------------------*
   * Check for a cyclic import. *
   *----------------------------*/

  for(p = file_info_st; p != NULL; p = p->next) {
    if(strcmp(p->file_name, infileName) == 0) {
      if(p->context != BODY_CX || !seen_export) {
	char* ext_name = strdup(nonnull(infileName));
	force_external(ext_name);
	semantic_error1(CYCLIC_IMPORT_ERR, ext_name, 0);
	FREE(ext_name);
	return;
      }
      else break;
    }
  }

  /*----------------------------*
   * Perform the second import. *
   *----------------------------*/

  second_import_tm(package_name_from_file_name(infileName), infileName,
		   mode); 
}


/****************************************************************
 *			GET_IMPORT_NAME				*
 ****************************************************************
 * Return the full path name of a file given by name this_name  *
 * in an import declaration.  This involves adding .ast to the  *
 * name, and finding the right directory.  			*
 *								*
 * Return NULL and set *infile = NULL if no appropriate file 	*
 * can be found.						*
 *								*
 * Try to open the file, putting a file descriptor in infile.   *
 *								*
 * If the file has already been imported, then do a second	*
 * import of that file, return (char *) 1 and set		*
 * *infile = NULL.  If the file is currently being read, and is *
 * not in its body, then complain about a cyclic import rather  *
 * than trying to do a second import.  				*
 *								*
 * File name this_name can be in internal or external form.	*
 * The result, however, is in internal form.			*
 ****************************************************************/

PRIVATE char* get_import_name(char *this_name, FILE **infile)
{
  STR_LIST *dirs;
  char *dir, *infileName, *result;
  int ldir;

  char* aname   = ast_name(this_name, 1);
  int aname_len = strlen(aname);
# ifdef SMALL_STACK
    char* fname    = (char*) BAREMALLOC(MAX_FILE_NAME_LENGTH + 1);
    char* base_dir = (char*) BAREMALLOC(MAX_FILE_NAME_LENGTH + 1);
# else
    char fname[MAX_FILE_NAME_LENGTH + 1];
    char base_dir[MAX_FILE_NAME_LENGTH + 1];
# endif

  if(strlen(aname) > MAX_FILE_NAME_LENGTH) die(17, aname);
  *infile = NULL;
  result  = NULL;
  fname[0] = 0;

  /*------------------------------------------------------------*
   * Get the directory that holds the current file.  It is the	*
   * base directory for imports done in this file.		*
   *------------------------------------------------------------*/

  if(file_info_st == NULL) strcpy(base_dir, ".");
  else dir_prefix(base_dir, NULL, current_file_name);

  /*----------------------------------------------*
   * Get the file name.  There are two options.   *
   *----------------------------------------------*/

  /*--------------------------------------------------------------------*
   * (1) An absolute name should not be altered.  Also, if file_info_st *
   *     is null, then this file is the main file, imported by an 	*
   *     implicit import, and it should be opened   			*
   *     in the current directory, so should not be altered. 		*
   *--------------------------------------------------------------------*/

  if(file_info_st == NULL || is_absolute(aname)) {
    strcpy(fname, aname);
    force_internal(fname);
    install_standard_dir(fname, MAX_FILE_NAME_LENGTH);
    install_home_dir(fname, MAX_FILE_NAME_LENGTH);
  }

  /*--------------------------------------*
   * (2) Search for the import directory. *
   *--------------------------------------*/

  else {
    if(current_import_dir != NULL) {
      SET_LIST(import_path, file_colonsep_to_list(current_import_dir));
    }
    else {
      SET_LIST(import_path, default_import_path);
    }

#   ifdef DEBUG
      if(trace) {
        trace_t(327);
        print_str_list_nl(import_path);
        tracenl();
      }
#   endif

    for(dirs = import_path; 
	dirs != NIL && *infile == NULL; 
	dirs = dirs->tail) {
      dir  = dirs->head.str;
      ldir = strlen(dir);
      if(ldir > MAX_FILE_NAME_LENGTH) die(17, dir);
      strcpy(fname, dir);
      make_absolute(fname, base_dir);

      /*--------------------------------------------*
       * If this directory does not exist, skip it. *
       *--------------------------------------------*/

      if(fname[0] != 0) {
        if(strlen(fname) + aname_len + 1 > MAX_FILE_NAME_LENGTH) {
          die(17, concat_string(fname, 
				concat_string(INTERNAL_DIR_SEP_STR, aname)));
        }
        strcat(fname, INTERNAL_DIR_SEP_STR);
        strcat(fname, aname);
	if(verbose_mode) {
	  fprintf(LISTING_FILE, "\nLooking for file \"%s\"\n", fname);
	  error_reported = TRUE;  /* to cause line to be restarted */
	}
        *infile = fopen_file(fname, TEXT_READ_OPEN);
      }

#     ifdef DEBUG
        if(trace) {
         if(*infile == NULL) {
	   trace_t(328, aname, dir);
         }
         else trace_t(329, fname, *infile);
        }
#     endif

    } /* end for(dirs = ...) */
  } /* end else(search for import dir) */

  /*-------------------------------------*
   * If no file name was found, give up. *
   *-------------------------------------*/

  if(fname[0] == 0) {
    if(verbose_mode) {
      fprintf(LISTING_FILE, "\nCould not find file \"%s\"\n", aname);
      error_reported = TRUE;  /* to cause line to be restarted */
    }
    result = NULL;
    goto out;
  }

  /*-------------------------------------*
   * Get the full path name of the file. *
   *-------------------------------------*/

  force_internal(fname);
  infileName = full_file_name(fname, 0);

  if(infileName == NULL) {
    if(verbose_mode) {
      fprintf(LISTING_FILE, "\nCould not find file \"%s\"\n", aname);
      error_reported = TRUE;  /* to cause line to be restarted */
    }
    *infile = NULL; 
    result = NULL;
    goto out;
  }

  /*----------------------------------------------------------------*
   * Check to see whether this file is already in the import list.  *
   * If so, do a second import of it. 				    *
   *----------------------------------------------------------------*/

  if(str_member(infileName, imported_files) != 0) {
    if(*infile != NULL) {
      fclose(*infile); 
      *infile = NULL;
    }
    if(verbose_mode) {
      fprintf(LISTING_FILE, 
	      "\nFile \"%s\" has already been read.\n"
	      "Rescanning internal form.\n", 
	      infileName);
	  error_reported = TRUE;  /* to cause line to be restarted */
    }
    try_second_import(infileName, &this_mode);
    result = (char *) 1;
    goto out;
  }

  /*------------------------*
   * Try to open this file. *
   *------------------------*/

  if(*infile == NULL) *infile = fopen_file(infileName, TEXT_READ_OPEN);
 
  result = infileName;

 out:

# ifdef SMALL_STACK
    FREE(fname);
    FREE(base_dir);
# endif
  return result;
}


/****************************************************************
 *			TRACE_FILE_NAMES			*
 ****************************************************************
 * Show the files that are currently being imported.  This	*
 * scans the file import stack.					*
 ****************************************************************/

#ifdef DEBUG
PRIVATE void trace_file_names(void)
{
  IMPORT_STACK *p;
  trace_t(330);
  for(p = file_info_st; p != NULL; p = p->next) {
    fprintf(TRACE_FILE, "%s\n", p->file_name);
  }
}
#endif


/****************************************************************
 *			PUSH_INPUT_FILE				*
 ****************************************************************
 * Push file this_file onto the input file stack.  Return true  *
 * on success, false on failure.  A failure will happen if the	*
 * file does not exist, or if the file has already been read.	*
 * In the latter case, a second-import of the file is done.	*
 *								*
 * this_file can be either internal or external format.		*
 ****************************************************************/

Boolean push_input_file(char *this_file)
{
  FILE *infile;
  char *infileName;

# ifdef DEBUG
    if(trace_importation) trace_t(534, this_file);
# endif

  /*---------------------------------------*
   * Get the file name, and open the file. *
   *---------------------------------------*/

  infileName = get_import_name(this_file, &infile);

  /*-----------------------------------------------------------*
   * If can't open, complain if appropriate, and return false. *
   *-----------------------------------------------------------*/

  if(infile == NULL) {

    /*-------------------------------------------------------*
     * If get_import_name returned 1, then it did a second   *
     * import of the file.  Ignore that.  Otherwise, when    *
     * infile is NULL, it was not possible to open the file. *
     *-------------------------------------------------------*/

    if(infileName != (char *) 1) {
      char* ext_name = strdup(nonnull(this_file));
      force_external(ext_name);
      if(file_info_st == NULL || current_import_dir == NULL) {
	semantic_error1(NO_FILE_ERR, ext_name, 0);
      }
      else {
        char* ext_dir = strdup(current_import_dir);
	force_external(ext_dir);
	semantic_error2(NO_FILE_DIR_ERR, ext_name, ext_dir, 0);
	FREE(ext_dir);
      }
      FREE(ext_name);
    }
    return FALSE;
  }

  /*--------------------------------------------------------------------*
   * Push the new file and information about it, and set 		*
   * the current directory for this file.				*
   *									*
   * Note: The package_name and the heads of public_packages,   	*
   * and private_packages are updated in parser.y,			*
   * where the package name is actually read. See importDcl there. 	*
   *--------------------------------------------------------------------*/

  else {
    char* dir;
    SET_LIST(imported_files, str_cons(infileName, imported_files));
    SET_LIST(imported_package_names, 
	     str_cons(standard_package_name, imported_package_names));
    if(file_info_st != NULL) {
      file_info_st->chars_in_line = chars_in_this_line;
      file_info_st->propagate_mode = propagate_mode;
    }
    chars_in_this_line = 0;
    push_file_info_frame(infileName, infile);
    dir = (char*) BAREMALLOC(strlen(infileName) + 1);
    dir_prefix(dir, NULL, infileName);
    chdir_file(dir);
    FREE(dir);
  }

# ifdef DEBUG
   if(trace_lexical) trace_file_names();
# endif

  /*----------------------*
   * Get ready for input. *
   *----------------------*/

  yyin = file_info_st->file;
  yy_switch_to_buffer(file_info_st->lexbuf);

  /*-------------------------------------------------------*
   * If this file is being listed, then indicate the file  *
   * that is being shown.				   *
   *-------------------------------------------------------*/

  if(about_to_load_interface_package) {
    main_context = EXPORT_CX;
    gen_code     = main_gen_code;
  }
  else {
    main_context = IMPORT_CX;
    gen_code     = FALSE;
  }
  if(verbose_mode || should_list()) {
    fprintf(LISTING_FILE, "\n----- Begin file %s\n\n", infileName);
  }
  new_line_l();

  return TRUE;
}


/****************************************************************
 *			POP_INPUT_FILE				*
 ****************************************************************
 * Pop a file from the input stack.  Set up context for the     *
 * new top of the stack.					*
 *								*
 * Archive the popped frame if archive is true.  If archive is  *
 * false, then none of the tables are freed.			*
 ****************************************************************/

void pop_input_file(Boolean archive)
{
  IMPORT_STACK* p = file_info_st;

# ifdef DEBUG
    if(trace_importation) {
      trace_t(535, current_file_name, archive);
    }
# endif

  if((verbose_mode || should_list()) && file_info_st->next != NULL) {
    fprintf(LISTING_FILE, "\n----- End file %s\n", current_file_name);
    fprintf(LISTING_FILE, "----- Resume file %s\n\n", 
	    file_info_st->next->file_name);
    error_reported = TRUE;   /* To cause line to be restarted */
  }

  /*-------------------------------------------*
   * Close the file and delete the lex buffer. *
   *-------------------------------------------*/

  fclose(yyin);
  yy_delete_buffer(p->lexbuf);
  p->file = yyin = NULL;
  p->lexbuf = NULL;

  /*-------------------------------------------------------*
   * Pop the file info stack, and archive it if requested. *
   *-------------------------------------------------------*/

# ifdef DEBUG
    if(trace_importation) {
      trace_t(468,current_file_name,toint(file_info_st->is_interface_package));
    }
# endif

  file_info_st = p->next;
  if(archive) {
    archive_import_frame(p);
  }

  /*----------------------------------------------------------------*
   * Reset seen_package_end, to recognize that the end of an import *
   * is not the end of the main package. 			    *
   *----------------------------------------------------------------*/

  if(file_info_st != NULL) {
    seen_package_end = FALSE;
    if(main_context == EXPORT_CX || main_context == BODY_CX) {
      gen_code = main_gen_code;
    }

#   ifdef DEBUG
      if(trace_lexical) trace_file_names();
#   endif

    /*--------------------------------------------------------------------*
     * Set the input file and input buffer for reading from the previous  *
     * file.  Change the current directory to that of the previous file.  *
     * Clear out any memoizing of tables that would be invalid for this   *
     * file.  								  *
     *--------------------------------------------------------------------*/

    yyin = file_info_st->file;
    yy_switch_to_buffer(file_info_st->lexbuf);
    chars_in_this_line = file_info_st->chars_in_line;
    propagate_mode     = file_info_st->propagate_mode;

    clear_table_memory();

    {char* dir = (char*) BAREMALLOC(strlen(current_file_name) + 1);
     dir_prefix(dir, NULL, current_file_name);
     chdir_file(dir);
     FREE(dir);
    }
  }
}


/****************************************************************
 *			PUSH_BACK_TOKEN				*
 ****************************************************************
 * Push the token back into the input, with attribute lval.  	*
 * Kind should be						*
 *    END_K   if token is an END_TOK or PERIOD_TOK		*
 *    BEGIN_K if token is a begin word				*
 *    0       otherwise.					*
 * name is the print name of this token.  If name = NULL, the	*
 * token will not be echoed when it is reread.			*
 ****************************************************************/

void push_back_token(int token, YYSTYPE lval, int kind, char *name)
{
  register struct unput_tokens_buffer_type *buf;

# ifdef DEBUG
    if(trace_lexical) {
      trace_t(331, token, kind, nonnull(name));
    }
# endif

  if(unput_tokens_top >= MAX_UNPUT_TOKENS) die(18);
  buf = unput_tokens_buffer + unput_tokens_top;
  buf->lval  = lval;
  buf->token = token;
  buf->kind  = kind;
  buf->opc   = opContext;
  buf->name  = id_tb0(name);
  unput_tokens_top++;
}


/****************************************************************
 *		        PAREN_AHEAD				*
 ****************************************************************
 * This function returns 					*
 *								*
 *   1 if the next token is a right parenthesis, comma, 	*
 *     semicolon, bar, equal sign, or 'by'.			*
 *								*
 *   2 if the next token is a period or end marker		*
 *								*
 *   0 otherwise						*
 *								*
 * It does not consume any characters - it pushes back		*
 * anything that was read.					*
 ****************************************************************/

PRIVATE char symbolic_id_chars[] = "#+-*/^!=<>@&~$_\\?'%";

int paren_ahead()
{
  char buffer[MAX_UNPUT - 8], *p;
  int i, c;
  int result = 0;

  i = 0;
  p = buffer;
  c = input_yy();

  /*-------------------*
   * Skip white space. *
   *-------------------*/

  while(i < (MAX_UNPUT - 10) && isspace(c)) {
    *(p++) = c;
    i++;
    c = input_yy();
  }
  *p = c;
  i++;

  /*------------------------------*
   * Look at the next characters. *
   *------------------------------*/

  if(c == ')' || c == ']' || c == '}' || c == ',' || c == ';' || c == '|') {
    result = 1;
  }

  else if(c == '.') result = 2;

  /*--------------------------------------------------------------*
   * If we see an =, check whether this is just = or is the start *
   * of a symbolic id.						  *
   *--------------------------------------------------------------*/

  else if(c == '=') {
    char *pp;
    *(++p) = c = input_yy();
    i++;
    result = TRUE;
    for(pp = symbolic_id_chars; *pp != '\0'; pp++) {
      if(*pp == c) {
        result = 0; 
        break;
      }
    }
  }

  
  else if(c == '%') {
    *(++p) = c = input_yy();
    i++;
    if(c != '%' && c != '"') result = 2;
  }

  /*----------------*
   * Look for 'by'. *
   *----------------*/

  else if(c == 'b') {
    *(++p) = c = input_yy(); 
    i++;
    if(c == 'y') {
      *(++p) = c = input_yy(); 
      i++;
      if(isspace(c)) result = 1;
    }
  }
    
  /*--------------------------------*
   * Push back the characters read. *
   *--------------------------------*/

  while(i > 0) {
    unput_yy(*(p--));
    i--;
  }

  return result;
}




/****************************************************************
 *			YYLEXX					*
 ****************************************************************
 * This is the top level lexer.  It calls yylex, and handles	*
 * tokens that have been unput via push_back_token.		*
 * Also, yylexx echos tokens.					*
 ****************************************************************/

extern int yylex(void);

int yylexx(void)
{
  int tok, kind, j;
  char *name;

  /*------------------------------------------------------*
   * Check if there is a token that has been pushed back. *
   *------------------------------------------------------*/

  if(unput_tokens_top > 0) {
    register struct unput_tokens_buffer_type *buf;

    unput_tokens_top--;
    buf = unput_tokens_buffer + unput_tokens_top;
    yylval = buf->lval;
    tok    = buf->token;
    kind   = buf->kind;
    name   = buf->name;
    opContext = buf->opc;
    force_no_op = 0;
    import_start = 0;
    if(name != NULL) strcpy(yytext, name);
    else yytext[0] = '\0';
    if(tok == BRING_TOK) bring_context = 1;

    if(kind == BEGIN_K) {
      if(tok != LINE_TOK) {
        if(tok != PROC_ID_TOK) push_shadow(tok);
        else push_shadow_str(yylval.ls_at.name);
      }

      /*--------------------*
       * Handle IMPORT_TOK. *
       *--------------------*/

      if(tok == IMPORT_TOK) import_l(1);
      
      /*------------------------------------------------------------*
       * Map begin tokens that have other tokens stand in for them. *
       *------------------------------------------------------------*/

      for(j = 0; token_map[j].tok != 0; j++) {
	if(tok == token_map[j].tok) {
	  yylval.ls_at.attr = token_map[j].attr;
	  tok = token_map[j].new_tok;
	  break;
        }
      }
    } /* end if(kind == BEGIN_K) */

    else if(kind == END_K) {
      if(yylval.ls_at.name != NULL) strcpy(yytext, yylval.ls_at.name);
      j = (tok == END_TOK) ? 0 : 1;
      if((tok = end_word_l(j)) == 0) {
	tok = yylexx();
      }
    }

    else if(kind == RPAREN_K) {
      if((tok = end_word_l(tok)) == 0) tok = yylexx();
    }

    else if(kind == SEPARATOR_K) tok = check_for_paren_l(tok);

    echo_token_l(yytext);
    yytext[0] = '\0';

#   ifdef DEBUG
      if(trace_lexical) {
        trace_t(332, tok, current_line_number);
	trace_t(334);
	print_str_list_nl(current_shadow_stack);
        trace_t(335);
        print_str_list_nl(current_shadow_nums);
      }
#   endif
  } /* end if(unput_tokens_top > 0) */

  /*------------------------------------------------------*
   * If no token pushed back, then get a token via yylex. *
   *------------------------------------------------------*/

  else {
    tok = yylex();

#   ifdef DEBUG
      if(trace_lexical) {
        trace_t(333, 
		(tok != STRING_CONST_TOK) ? yytext : (char *) string_const, 
	       tok, current_line_number);
	trace_t(334);
	print_str_list_nl(current_shadow_stack);
        trace_t(335);
        print_str_list_nl(current_shadow_nums);
      }
#   endif
  }

  last_last_tok    = last_tok;
  last_last_yylval = last_yylval;
  last_tok         = tok;
  last_yylval      = yylval;

  /*---------------------------*
   * Check for unexpected eof. *
   *---------------------------*/

  if(tok == 0 && current_shadow_stack != NULL && !seen_package_end) {
    eof_err();
    SET_LIST(current_shadow_stack, NULL);
    strcpy(yytext, "%Package");
    return handle_endword_l();
  }
 
  /*-------------------------------------------------------------*
   * Recover from panic after reading three tokens successfully. *
   *-------------------------------------------------------------*/

  if(panic_mode) {
    panic_mode++;
    if(panic_mode == 3) panic_mode = 0;
  }

  if(tok != STRING_CONST_TOK) echo_token_l(yytext);
  return tok;
}


/****************************************************************
 *			   SHOULD_LIST				*
 ****************************************************************
 * Return true if a listing is being generated in the current   *
 * context.							*
 ****************************************************************/

Boolean should_list(void)
{
  return gen_listing && 
         (import_level == 0 || 
	  gen_listing == 3 ||
          gen_listing == 2 && main_context == EXPORT_CX);
}


/****************************************************************
 *			   NEW_LINE_L				*
 ****************************************************************
 * Start a new line in the listing file.  The newline character	*
 * has already been printed.  This just updates and prints	*
 * the current line number.					*
 ****************************************************************/

void new_line_l()
{
  current_line_number++;

# ifdef DEBUG
    if(trace_lexical) {
      trace_t(336, current_line_number, import_level, toint(seen_package_end));
    }
# endif

  if(should_list()) {
    chars_in_this_line = 0;
    if(!seen_package_end) {
      fprintf(LISTING_FILE, "%4d ", current_line_number);
    }
  }
}


/****************************************************************
 *			   ECHO_CHAR_L				*
 ****************************************************************
 * Echo character c, if called for.				*
 *								*
 * Note: This function is inlined in lexer.lex.  Modify that    *
 * file if any changes are made here.				*
 ****************************************************************/

void echo_char_l(int c)
{
  if(should_list()) {
    if(c == '\t') echo_tab_l();
    else {
      putc(c, LISTING_FILE);
      chars_in_this_line++;
    }
    err_flags.need_err_nl = TRUE;
  }
}


/****************************************************************
 *			   ECHO_CHAR_LINE_L			*
 ****************************************************************
 * Echo character c, and do a newline_l if c is a newline	*
 * character.							*
 ****************************************************************/

void echo_char_line_l(int c)
{
  echo_char_l(c);
  if(c == '\n') new_line_l();
}


/****************************************************************
 *			   ECHO_TOKEN_L				*
 ****************************************************************
 * Echo lexeme, if in a context where echoing is happening.	*
 * Return TRUE if actually printed anything, FALSE otherwise.	*
 ****************************************************************/

Boolean echo_token_l(char *lexeme)
{
  char c;
  int i, len, start_char;

  if(no_print_this_token) {
    no_print_this_token = FALSE;
    return FALSE;
  }

  len = strlen(lexeme);
  if(should_list()) {
    
    /*--------------------------------------------------------------------*
     * If there was an error previously, restart the line and space over. *
     * But do not do that if there is nothing but white space on the rest *
     * of this line.							  *
     *--------------------------------------------------------------------*/
    
    start_char = 0;
    if(error_reported && LISTING_FILE != NULL) {
      while((c = lexeme[start_char]) == '\n' || c == ' ' || c == '\t') {
	start_char++;
	if(c == ' ') chars_in_this_line++;
	else if(c == '\t') echo_tab_l();
	else {error_reported = FALSE; goto print_it;}
      }
      if(start_char < len) {
	error_reported = FALSE;
	fprintf(LISTING_FILE, "\n%4d ", current_line_number);
	for(i = 0; i < chars_in_this_line; i++) putc(' ', LISTING_FILE);
      }
    }

    /*-------------------*
     * Print the lexeme. *
     *-------------------*/

  print_it:
#   ifdef DEBUG
      if(trace_lexical) trace_t(337);
#   endif

    for(i = 0; i < len; i++) {
      c = lexeme[i];
      if(c == '\t') echo_tab_l();
      else {
	putc(c, LISTING_FILE); 
	chars_in_this_line++;
      }
    }

#   ifdef DEBUG
      if(trace_lexical) tracenl();
#   endif

    err_flags.need_err_nl = TRUE;
    return TRUE;

  } /* end if(should_list()) */

  else {
    for(i = 0; i < len; i++) {
      if(lexeme[i] == '\t') echo_tab_l();
      else chars_in_this_line++;
    }
    return FALSE;
  }
}


/****************************************************************
 *			   ECHO_TAB_L				*
 ****************************************************************
 * Echo a tab character as spaces.				*
 ****************************************************************/

void echo_tab_l()
{ 
  if(should_list() & !error_reported) {
    do {
      putc(' ', LISTING_FILE);
      chars_in_this_line++;
    } while((chars_in_this_line & 7) != 0);
  }
  else {
    do {
      chars_in_this_line++;
    } while((chars_in_this_line & 7) != 0);
    if(error_reported) err_flags.need_err_nl = TRUE;
  }
}


