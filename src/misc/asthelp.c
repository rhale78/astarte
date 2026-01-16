/***********************************************************************
 * File:    misc/asthelp.c
 * Purpose: Find documentation in .asd files.  This file holds the 
 *          main program for asthelp.
 * Author:  Karl Abrahamson
 ***********************************************************************/

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
#include "misc.h"
#include "find.h"

/********************************************************
 *			MAX_DONE_FILES			*
 ********************************************************
 * MAX_DONE_FILES is the maximum number of files that   *
 * hold information about a particular id or pattern, 	*
 * and that are recorded so that duplicate information	*
 * is not printed.  If there is information in more 	*
 * than this many files about id, then it is possible	*
 * that some information will be printed twice.		*
 ********************************************************/

#define MAX_DONE_FILES			30


/****************************************************************
 *			IS_ABSOLUTE_F				*
 ****************************************************************
 * Return TRUE if name is an absolute file name, and FALSE	*
 * otherwise.							*
 *								*
 * Note: name should be in internal file name form, meaning	*
 * that the directory separator is INTERNAL_DIR_SEP_CHAR.	*
 ****************************************************************/

PRIVATE Boolean 
is_absolute_f(char *name)
{
  return name[0] == INTERNAL_DIR_SEP_CHAR ||
	 (name[1] == ':' && name[2] == INTERNAL_DIR_SEP_CHAR);
}


/********************************************************
 *			RECORD_DONE			*
 *			ALREADY_DONE			*
 ********************************************************
 * record_done(name) puts name in done_files. name	*
 * must be in the heap.			 		*
 *							*
 * already_done(name) returns 1 if name has been	*
 * recorded in done_files, and 0 otherwise.		*
 *							*
 * delete_done_files frees the space occupied by the    *
 * file names that are stored in the done_files array.  *
 *							*
 * done_files_top is the number of entries in		*
 * done_files.						*
 ********************************************************/

PRIVATE char* done_files[MAX_DONE_FILES];
PRIVATE char  done_files_top = 0;

PRIVATE void 
record_done(char *name)
{
  if(done_files_top < MAX_DONE_FILES) {
    done_files[done_files_top++] = name;
  }
}

/*----------------------------------------------------------*/

PRIVATE int 
already_done(char *name)
{
  int i;

  for(i = 0; i < done_files_top; i++) {
    if(strcmp(done_files[i], name) == 0) return 1;
  }
  return 0;
}

/*----------------------------------------------------------*/

PRIVATE void 
delete_done_files(void)
{
  int i;
  for(i = 0; i < done_files_top; i++) {
    free(done_files[i]);
  }
}


/********************************************************
 *			COPY_DESCRIPTIONS		*
 ********************************************************
 * Copy all descriptions of identifier id to file	*
 * outfile.  The descriptions are found in file     	*
 * infile. 						*
 *							*
 * The description file contains units of the following *
 * form, one after another.		 		*
 *							*
 *    C<id>						*
 *    <description>0					*
 * 							*
 * where <id> is the identifier being described,   	*
 * <description> is the description of <id>, 0 is a	*
 * null byte, and C is a character that tells what kind *
 * of thing <id> is, with the following possibilities.	*
 *							*
 *   E	an entity identifier				*
 *   S  a species identifier				*
 *   F  a family identifier				*
 *   G  a genus identifier				*
 *   C  a community identifier				*
 *   N  anything else					*
 ********************************************************/

PRIVATE void 
copy_descriptions(FILE *infile, FILE *outfile, char *id)
{
  char this_id[MAX_NAME_LENGTH + 1];
  char *p;
  int c, kind;

  /*--------------*
   * Look for id. *
   *--------------*/

  kind = getc(infile);
  while(kind != EOF) {

     /*------------------------------------*
      * Get an identifier from the infile. *
      *------------------------------------*/

     p = this_id;
     c = getc(infile);
     while(c != EOF && c != '\n') {
       if(p - this_id < MAX_NAME_LENGTH) *(p++) = c;
       c = getc(infile);
     }
     *p = 0;

     /*-------------------------------------------------------*
      * If id has been found, then copy the description to    *
      * outfile.  Otherwise, just skip over this description. *
      *							      *
      * Note: if substring_mode is nonzero, then test for a   *
      * substring instead of equality.			      *
      *-------------------------------------------------------*/

     if(compare_f(this_id, id)) {
       fprintf(outfile, "-------------------------------------------\n");
       fprintf(outfile, " %s", this_id);
       c = getc(infile);
       if(c == ':') putc(' ', outfile);
       else fprintf(outfile, "\n\n");
       while(c != EOF && c != 0) {
	 putc(c, outfile);
	 c = getc(infile);
       }
       putc('\n', outfile);
     }
     else {
       c = getc(infile);
       while(c != EOF && c != 0) c = getc(infile);
     }

     kind = getc(infile);
  }
}


/********************************************************
 *			WRITE_DATA			*
 ********************************************************
 * Read index_file, obtaining all files where		*
 * id occurs.  For each, write the name of the file, 	*
 * and if any descriptions are found, the descriptions,	*
 * on file outfile.					*
 *							*
 * As each file where id occurs is found, it is placed  *
 * in array done_files.  If it was already there, it    *
 * is ignored.						*
 *							*
 * If the file found is f, then the descriptions are    *
 * found in file dir/f.asd.				*
 ********************************************************/

PRIVATE void 
write_data(FILE *index_file, char *dir, char *id, void *outfile_ptr)
{

  FILE* outfile = (FILE*) outfile_ptr;

  char *search_file_name, *found_id;
  FILE *search_file;
  char knd;
# ifdef SMALL_STACK
    char* asd_file_name = (char*) bare_malloc_f(MAX_FILE_NAME_LENGTH + 1);
# else
    char asd_file_name[MAX_FILE_NAME_LENGTH + 1];
# endif

  do {
    search_file_name = next_file(index_file, id, &knd, &found_id);
    if(search_file_name != NULL) {
      if(!already_done(search_file_name)) {
        record_done(search_file_name);

        force_external1(search_file_name);
	fprintf(outfile, 
		"$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n<%s>\n\n",
		search_file_name);
	if(knd >= 'a' && knd != 'e') {
	  fprintf(outfile, " %s defined\n\n", found_id);
	}
        free(found_id);

	force_internal1(search_file_name);
	if(!is_absolute_f(search_file_name)) {
	  strcpy(asd_file_name, dir);
	  strcat(asd_file_name, INTERNAL_DIR_SEP_STR);
	}
        else {
	  asd_file_name[0] = 0;
	}
	strcat(asd_file_name, search_file_name);
	strcat(asd_file_name, ".asd");
	force_external1(asd_file_name);
	search_file = fopen(asd_file_name, "r");
	if(search_file != NULL) {
	  copy_descriptions(search_file, outfile, id);
	  fclose(search_file);
	}
      }
      else {
	free(search_file_name);
	free(found_id);
      }
    }
  } while(search_file_name != NULL);  

# ifdef SMALL_STACK
    free(asd_file_name);
# endif
}


/********************************************************
 * Write all description/file info for identifier ID    *
 * onto file OUTFILE.					*
 *							*
 * PATH is a colon-separated list of directories to     *
 * search for index files.				*
 ********************************************************/

PRIVATE void 
write_description_info(FILE *outfile, char *id, char *path)
{
  find_description_info(id, path, write_data, (void*) outfile);
}


/********************************************************
 *			USAGE_ERR			*
 ********************************************************/

PRIVATE void 
usage_err(void)
{
  fprintf(stderr, 
	  "Usage: asthelp [-i -s] name\n"
          "  -i : ignore case in comparisons\n"
	  "  -s : look for an identifier with name as a substring\n");
}


/********************************************************
 *			HANDLE_OPTIONS			*
 ********************************************************
 * Deal with command line options.  Return the index of *
 * the first item in the command line that is not an    *
 * option.						*
 ********************************************************/

PRIVATE int 
handle_options(int argc, char **argv)
{
  int i = 1;
  while(i < argc - 1) {
    if(strcmp(argv[i], "-s") == 0) {
      substring_mode = 1;
      i++;
    }
    else if(strcmp(argv[i], "-i") == 0) {
      ignore_case_mode = 1;
      i++;
    }
    else return i;
  }

  return i;
}


/********************************************************
 *			MAIN				*
 ********************************************************/

int main(int argc, char **argv)
{
  int nameloc;

  if(argc < 2) {
    usage_err();
    return 1;
  }

  nameloc = handle_options(argc, argv);
  if(argc - 1 != nameloc) {
    usage_err();
    return 1;
  }

  write_description_info(stdout, argv[nameloc], asthelp_path());    
  delete_done_files();
  return 0;
}

