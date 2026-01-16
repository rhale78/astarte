/*********************************************************************
 * File:    misc/asd.c
 * Purpose: Handle opening and closing of .asd files.
 * Author:  Karl Abrahamson
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

#include <string.h>
#ifdef USE_MALLOC_H
#  include <malloc.h>
#else
#  include <stdlib.h>
#endif
#include "../misc/misc.h"
#include "../alloc/allocate.h"
#include "../utils/strops.h"
#include "../utils/filename.h"
#include "../tables/tables.h"
#include "../parser/parser.h"
#include "../parser/asd.h"

/****************************************************************
 *			OPEN_DESCRIPTION_FILE			*
 ****************************************************************
 * Set up description_file.  this_file_ast is the name of the   *
 * .ast file.							*
 ****************************************************************/

void open_description_file(char *this_file_ast)
{
  int   fname_size = strlen(this_file_ast) + 5;
  char* fname = (char*) BAREMALLOC(fname_size);

  /*-----------------------------------------------------------------*
   * Get the name of the ".asd" file by stripping the extension from *
   * fname and adding ".asd".  Open the description file.            *
   *-----------------------------------------------------------------*/

   get_root_name(fname, this_file_ast, fname_size - 4);
   strcat(fname, ".asd");
   description_file = fopen_file(fname, "w");
   FREE(fname);
}


/****************************************************************
 *			OPEN_INDEX_FILE				*
 ****************************************************************
 * Set up index_file and index_file_dir.  this_dir is the name  *
 * of the directory that contains the file being compiled.      *
 * fname is the simple name, without directory, of the .ast     *
 * file.							*
 *								*
 * this_dir can either be in internal or external form.		*
 ****************************************************************/

void open_index_file(char *this_dir, char *fname)
{
# ifdef SMALL_STACK
    char* dir = (char*) BAREMALLOC(MAX_FILE_NAME_LENGTH + 1);
# else
    char dir[MAX_FILE_NAME_LENGTH + 1];
# endif
  FILE *indloc_file;

  /*---------------------------------------------------------------------*
   * Put the directory of the index file into dir.  If file indexloc.txt *
   * exists, then its contents is the directory to put into dir.  If     *
   * file indexloc.txt does not exist, then copy this_dir into dir.      *
   *---------------------------------------------------------------------*/

  if(strlen(this_dir) + strlen(fname) + 13 > MAX_FILE_NAME_LENGTH) {
    die(17, this_dir);
  }
  sprintf(dir, "%s%s", this_dir, INTERNAL_DIR_SEP_STR "indexloc.txt");
  indloc_file = fopen_file(dir, "r");

  if(indloc_file != NULL) {
    char* p;
    fgets(dir, MAX_FILE_NAME_LENGTH - 7, indloc_file);
    p = strchr(dir, '\n');
    if(p != NULL) *p = 0;
  }
  else {
    strcpy(dir, this_dir);
  }

  /*----------------------------------------------------------------*
   * If the directory came from indexloc.txt, then it is relative.  *
   * Make it absolute.						     *
   *----------------------------------------------------------------*/

  make_absolute(dir, this_dir);

  /*------------------------------------------------------------------*
   * Now we have the index file directory.  Put it in index_file_dir. *
   * Open file indx.asx in that directory.  Index information will be *
   * written there.  When the compiler is finished, information from  *
   * index.asx will be appended to indx.asx, and then indx.asx will   *
   * be renamed index.asx.					       *
   *------------------------------------------------------------------*/

  index_file_dir = string_const_tb0(dir);
  strcat(dir, INTERNAL_DIR_SEP_STR "indx.asx");
  index_file = fopen_file(dir, "w");
  if(index_file != NULL) {
    char fname2[MAX_SIMPLE_FILE_NAME_LENGTH + 1];
    get_root_name(fname2, fname, MAX_SIMPLE_FILE_NAME_LENGTH);
    sprintf(dir, "%s%c%s", this_dir, INTERNAL_DIR_SEP_CHAR, fname2);
    root_file_name = get_full_file_name(dir, 0);
    fprintf(index_file, "[%s]\n", root_file_name);
  }

# ifdef SMALL_STACK
    FREE(dir);
# endif
}


/****************************************************************
 *			CLOSE_INDEX_FILES			*
 ****************************************************************
 * Close files index_file and description_file.  Before closing *
 * index_file, however, copy the existing index file index.asx  *
 * to the end of index_file, deleting any parts that refer to   *
 * this file.							*
 *								*
 * If clean is TRUE, then the compilation is terminating	*
 * cleanly.  If not, then the computation is giving up, and we  *
 * should not install information into the index file.		*
 ****************************************************************/

void close_index_files(Boolean clean)
{
  int c;

  /*-----------------------------------------------------------------*
   * Close the description file.  If nothing was written, unlink it. *
   *-----------------------------------------------------------------*/

  if(description_file != NULL) {
    fclose(description_file);
    description_file = NULL;
    if(!wrote_description) {
#     ifdef SMALL_STACK
        char* description_file_name = 
	        (char*) BAREMALLOC(MAX_FILE_NAME_LENGTH + 1);
#     else
        char description_file_name[MAX_FILE_NAME_LENGTH + 1];
#     endif

      get_root_name(description_file_name, main_file_name, 
		    MAX_FILE_NAME_LENGTH - 4);
      strcat(description_file_name, ".asd");
      unlink_file(description_file_name);

#     ifdef SMALL_STACK
        FREE(description_file_name);
#     endif
    }
  }

  /*---------------------------*
   * Finish up the index file. *
   *---------------------------*/

  if(index_file != NULL) {
    FILE* old_index_file;
#   ifdef SMALL_STACK
      char* old_index_file_name = (char*) BAREMALLOC(MAX_FILE_NAME_LENGTH + 1);
      char* new_index_file_name = (char*) BAREMALLOC(MAX_FILE_NAME_LENGTH + 1);
#   else
      char old_index_file_name[MAX_FILE_NAME_LENGTH + 1];
      char new_index_file_name[MAX_FILE_NAME_LENGTH + 1];
#   endif

    /*----------------------------------------------------------*
     * For a clean end, install index information into the	*
     * real index file.  Currently, it is in a temporary 	*
     * index file.						*
     *----------------------------------------------------------*/

    if(clean) {

      /*-------------------------*
       * Get the old index file. *
       *-------------------------*/

      strcpy(old_index_file_name, index_file_dir);
      strcpy(new_index_file_name, old_index_file_name);
      strcat(old_index_file_name, INTERNAL_DIR_SEP_STR "index.asx");
      strcat(new_index_file_name, INTERNAL_DIR_SEP_STR "indx.asx");
      old_index_file = fopen_file(old_index_file_name, "r");

      /*----------------------------------------------------------*
       * If we can see an old index file, then copy its contents  *
       * to the current index file, skipping entries for the      *
       * current file. 						  *
       *----------------------------------------------------------*/

      if(old_index_file != NULL) {
#       ifdef SMALL_STACK
          char* fname = (char*) BAREMALLOC(MAX_FILE_NAME_LENGTH + 1);
#       else
	  char fname[MAX_FILE_NAME_LENGTH + 1];
#       endif

	c = getc(old_index_file);
	while(c != EOF) {
	  char* p;

	  /*---------------------------------------------------------*
	   * Read the entry for one file.  c should be '['.  If not, *
	   * stop copying.					     *
	   *---------------------------------------------------------*/

	  if(c != '[') break;

	  p = fname;

	  /*-------------------------------------*
	   * Read the file name. It is in [...]. *
	   *-------------------------------------*/

	  while(TRUE) {
	    c = getc(old_index_file);
	    if(c == ']' || c == EOF) break;
	    if(p - fname < MAX_FILE_NAME_LENGTH) (*p++) = c;
	  }
	  *p = 0;
	  getc(old_index_file);   /* The EOL char */

	  /*------------------------------------------------------*
	   * If this file is the file that we have just compiled, *
	   * then ignore it -- skip ahead to a '[' or EOF.	  *
	   *------------------------------------------------------*/

	  if(strcmp(fname, root_file_name) == 0) {
	    do {
	      c = getc(old_index_file);
	    } while (c != '[' && c != EOF);
	  }

	  /*----------------------------------------------------------*
	   * If this file is not the file that we have just compiled, *
	   * then copy its entries to index_file. 		      *
	   *----------------------------------------------------------*/

	  else {
	    fprintf(index_file, "[%s]\n", fname);
	    while(TRUE) {
	      c = getc(old_index_file);
	      if(c == '[' || c == EOF) break;
	      fprintf(index_file, "%c", c);
	    }
	  }
	} /* end while(c != EOF) */

	/*------------------------------------------------------*
	 * Close old_index_file, and unlink the old index file. *
	 *------------------------------------------------------*/
	
	fclose(old_index_file);
	unlink_file(old_index_file_name);

#       ifdef SMALL_STACK
          FREE(fname);
#       endif
      } /* end if(old_index_file != NULL) */

      /*------------------------------------------------------------*
       * Now that we have copied the old index file to the new one, *
       * close the index file, and rename the index file to be      *
       * what the old index file was called. 			    *
       *------------------------------------------------------------*/

      fclose(index_file);  
      index_file = NULL;
      rename_file(new_index_file_name, old_index_file_name);

    } /* end if(clean) */

    else /* not clean */ {

      /*----------------------------------*
       * Delete the temporary index file. *
       *----------------------------------*/

      strcpy(new_index_file_name, index_file_dir);
      strcat(new_index_file_name, INTERNAL_DIR_SEP_STR "indx.asx");
      fclose(index_file);
      index_file = NULL;
      unlink_file(new_index_file_name);
    }

#   ifdef SMALL_STACK
      FREE(old_index_file_name);
      FREE(new_index_file_name);
#   endif

  } /* end if(index_file != NULL) */
}
