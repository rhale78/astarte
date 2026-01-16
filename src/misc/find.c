/***********************************************************************
 * File:    misc/find.c
 * Purpose: Find documentation in .asd files.
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

/************************************************************************
 * This file manages reading and writing of description files.  Some	*
 * of the writing is found in parser/asd.c.				*
 *									*
 * The description files (extension .asd) hold information about 	*
 * what occurs inside .ast and .asi files.  Their format is discussed	*
 * below under function copy_descriptions.				*
 *									*
 * There is a master index in each directory, called index.asx, that	*
 * tells what is in the .asd files in that directory.  The format	*
 * of index.asx is discussed below under function next_file.		*
 *									*
 * It is possible to put the index in another directory.  If the 	*
 * current directory has a file called indexloc.txt, then the content	*
 * of that file is the name of a directory to look in for the index.	*
 * That directory might, in turn, have an indexloc.txt file that	*
 * redirects the index further.						*
 ************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef USE_MALLOC_H
#  include <malloc.h>
#else
#  include <stdlib.h>
#endif
#include <ctype.h>
#include "../misc/misc.h"
#include "../misc/find.h"

/********************************************************
 *			substring_mode			*
 ********************************************************
 * If substring_mode is nonzero, then each subject is   *
 * tested to see whether the selected key is a substring*
 * of it.  If substring_mode is zero, then the key must *
 * be equal to the subject.				*
 ********************************************************/

Boolean substring_mode = 0;

/********************************************************
 *			ignore_case_mode		*
 ********************************************************
 * If ignore_case_mode is true, then ignore the case of *
 * letters in comparisons.				*
 ********************************************************/

Boolean ignore_case_mode = 0;

/********************************************************
 *		       BARE_MALLOC_F			*
 ********************************************************
 * Same as MALLOC, but die if result is NULL. 		*
 ********************************************************/

void* bare_malloc_f(LONG n)
{
  register void* result = MALLOC(n);
  if(result == NULL) exit(1);
  return result;
}

/****************************************************************
 *			FORCE_INTERNAL1				*
 ****************************************************************
 * Force file name s to internal form, by replacing each	*
 * occurrence of ALT_DIR_SEP_CHAR by INTERNAL_DIR_SEP_CHAR.	*
 ****************************************************************/

void force_internal1(char *s)
{
  char* p = s;
  while(*p != 0) {
    if(*p == ALT_DIR_SEP_CHAR) *p = INTERNAL_DIR_SEP_CHAR;
    p++;
  }
}


/****************************************************************
 *			FORCE_EXTERNAL1				*
 ****************************************************************
 * Force file name s to external form, by replacing each	*
 * occurrence of INTERNAL_DIR_SEP_CHAR by EXTERNAL_DIR_SEP_CHAR.*
 ****************************************************************/

void force_external1(char *s)
#if (EXTERNAL_DIR_SEP_CHAR == INTERNAL_DIR_SEP_CHAR)
  {}
#else
  {
    char* p = s;
    while(*p != 0) {
      if(*p == INTERNAL_DIR_SEP_CHAR) *p = EXTERNAL_DIR_SEP_CHAR;
      p++;
    }
  }
#endif


/********************************************************
 *			IGNORE_CASE_CPY_F		*
 ********************************************************
 * Copy string src into buffer dest, but convert 	*
 * letters to lower case.				*
 ********************************************************/

PRIVATE void ignore_case_cpy_f(char* dest, char* src)
{
  char *p, *q;
  for(p = src, q = dest; *p != 0; p++, q++) {
    *q = tolower(*p);
  }
  *q = 0;
}


/********************************************************
 *			COMPARE_F			*
 ********************************************************
 * looking_for and found are null-terminated strings.   *
 * We are looking for a match of string looking_for, 	*
 * and have found string found.				*
 *							*
 * Compare looking_for with found, to see if found      *
 * matches pattern looking_for.  Return true if there   *
 * is a match.						*
 *							*
 * The behavior of this function is influenced by	*
 * substring_mode (check if looking_for is a substring 	*
 * of found) and ignore_case_mode (ignore case of 	*
 * letters).						*
 ********************************************************/

int compare_f(char* found, char *looking_for)
{
  int result;

  /*----------------------------------------------------*
   * If case is to be ignored, copy looking_for and	*
   * found, forcing all letters to lower case.		*
   *----------------------------------------------------*/

  if(ignore_case_mode) {
    char* looking_for_buff = (char*) bare_malloc_f(strlen(looking_for) + 1);
    char* found_buff       = (char*) bare_malloc_f(strlen(found) + 1);
    ignore_case_cpy_f(looking_for_buff, looking_for);
    ignore_case_cpy_f(found_buff, found);
    looking_for = looking_for_buff;
    found       = found_buff;
  }

  /*-------------------------*
   * Perform the comparison. *
   *-------------------------*/

  if(substring_mode) {
    result = strstr(found, looking_for) != NULL;
  }
  else {
    result = strcmp(found, looking_for) == 0;
  }

  if(ignore_case_mode) {
    free(looking_for);
    free(found);
  }

  return result;
}


/********************************************************
 *			NEXT_FILE			*
 ********************************************************
 * File index_file is a file that holds index 		*
 * information about where documentation can be found.  *
 * Its format is					*
 *							*
 *  [<filename1>]					*
 *  C<name1>						*
 *  C<name2>						*
 *  ...							*
 *  [<filename2>]					*
 *  ...							*
 *							*
 * where <filename1> and <filename2> are the names of 	*
 * files, without an extension, C is a character that   *
 * tells what kind of thing is named, and <name1>, etc. *
 * are identifiers.  C is one of the following.		*
 *							*
 *   E,e  an entity identifier				*
 *   S,s  a species identifier				*
 *   F,f  a family identifier				*
 *   G,g  a genus identifier				*
 *   C,c  a community identifier			*
 *   N  anything else					*
 * 							*
 * next_file(index_file, id) returns the next file name	*
 * found in index_file, from the current position, 	*
 * that has information about an identifier that 	*
 * matches id.  If no such file is found, then		*
 * next_file returns NULL.				*
 *							*
 * The comparison of id to the name found in the file   *
 * is under control of the options (-s or -i, for       *
 * substring or ignoring case).  The identifier actually*
 * found in the index file is placed into *found_id.    *
 *							*
 * Variable *knd is set to the character C that 	*
 * introduces identifier *found_id in the index file.	*
 *							*
 * Note: The returned file name and *found_id are both	*
 * allocated in the heap.  They should be freed when    *
 * they are no longer needed.				*
 ********************************************************/

char* next_file(FILE *index_file, char *id, char *knd, char** found_id)
{
# ifdef SMALL_STACK
    char* file_name = (char*) bare_malloc_f(MAX_FILE_NAME_LENGTH + 1);
    char* id_name   = (char*) bare_malloc_f(MAX_NAME_LENGTH + 1);
# else
    char file_name[MAX_FILE_NAME_LENGTH + 1];
    char id_name[MAX_NAME_LENGTH + 1];
# endif
  char *p, *result;
  int c, kind;

  /*--------------------------*
   * First find a '[' or EOF. *
   *--------------------------*/

  c = getc(index_file);
  while(c != '[' && c != EOF) {
    c = getc(index_file);
  }

  /*--------------------------------------------------------*
   * We are now positioned at a file name.  Read successive *
   * file names, looking for one that has s in its list.    *
   *--------------------------------------------------------*/

  while(c != EOF) {

    /*--------------------*
     * Get the file name. *
     *--------------------*/

    c = getc(index_file);
    p = file_name;
    while(c != EOF && c != ']') {
      if(p - file_name < MAX_FILE_NAME_LENGTH) *(p++) = c;
      c = getc(index_file);
    }
    *p = 0;

    /*--------------------------------------------*
     * Now read successive lines, looking for id. *
     *--------------------------------------------*/

    getc(index_file);          /* The newline after ']'.         */
    kind = getc(index_file);   /* Kind character of the first id */
    while(kind != EOF && kind != '[') {

      /*---------------------*
       * Read an identifier. *
       *---------------------*/

      c    = getc(index_file);   /* First character of the id. */
      p    = id_name;
      while(c != EOF && c != '\n') {
	if(p - id_name < MAX_NAME_LENGTH) *(p++) = c;
	c = getc(index_file);
      }
      *p = 0;

      /*----------------------------------------------------------------*
       * Compare this id_name with id. If we have found id, then we 	*
       * we should return the file name.				*
       *								*
       * Note: if substring_mode is nonzero, then do a substring test   *
       * instead of an equality test.					*
       *----------------------------------------------------------------*/

      if(compare_f(id_name, id)) {
	*knd      = kind;
	result    = strdup(file_name);
        *found_id = strdup(id_name);
        goto out;
      }

      kind = getc(index_file);   /* Kind character of the next id, *
				  * or [ or EOF if out of ids.     */

    }
  }

  /*---------------------------------------------------*
   * If we get here, then the search was unsuccessful. *
   *---------------------------------------------------*/

  result    = NULL;
  *found_id = NULL;

 out:

# ifdef SMALL_STACK
    free(file_name);
    free(id_name);
# endif

  return result;
}


/********************************************************
 *			PULL_DIR			*
 ********************************************************
 * extract a prefix of *path up to but not including	*
 * the first ':' or ';', or the end of string, and copy *
 * it to dir.  Set *path to the part of the string that	*
 * follows the ':' or ';', or to the null character at  *
 * the end of path if no ':' or ';' was found.		*
 ********************************************************/

PRIVATE void pull_dir(char *dir, char **path)
{
  char *p, *q;

  p = *path;
  q = dir;
  while(*p != ':' && *p != ';' && *p != 0) {
    *(q++) = *(p++);
  }
  *q = 0;
  if(*p == 0) *path = p;
  else *path = p+1;
}


/********************************************************
 *			FIND_DESCRIPTION_INFO		*
 ********************************************************
 * Find all description/file info for identifier id.    *
 * path is a colon-separated or semicolon-separated     *
 * list of directories to search for index files.       *
 *							*
 * For each index file f in directory dir to search, 	*
 * call searchf(f, dir, id, info).			*
 ********************************************************/

void 
find_description_info(char *id, char *path, 
		      void (*searchf)(FILE*, char*, char*, void*),
		      void *info)
{
  FILE *index_file;
# ifdef SMALL_STACK
    char* index_file_name = (char*) bare_malloc_f(MAX_FILE_NAME_LENGTH + 1);
    char* dir             = (char*) bare_malloc_f(MAX_FILE_NAME_LENGTH + 1);
# else
    char index_file_name[MAX_FILE_NAME_LENGTH + 1];
    char dir[MAX_FILE_NAME_LENGTH + 1];
# endif

  while(*path != 0) {
    pull_dir(dir, &path);

    /*---------------------------------------------------------*
     * If there is an indexloc.txt file in directory dir, then *
     * get dir from there.				       *
     *---------------------------------------------------------*/
  
    strcpy(index_file_name, dir);
    strcat(index_file_name, INTERNAL_DIR_SEP_STR "indexloc.txt");
    force_external1(index_file_name);
    index_file = fopen(index_file_name, TEXT_READ_OPEN);
    if(index_file != NULL) {
      fscanf(index_file, "%s", dir);
      fclose(index_file);
    }

    /*-------------------------------------------*
     * Now open file index.asx in directory dir. *
     *-------------------------------------------*/

    strcpy(index_file_name, dir);
    strcat(index_file_name, INTERNAL_DIR_SEP_STR "index.asx");
    force_external1(index_file_name);
    index_file = fopen(index_file_name, TEXT_READ_OPEN);

    /*---------------------------------------*
     * If this index file exists, search it. *
     *---------------------------------------*/

    if(index_file != NULL) {
      searchf(index_file, dir, id, info);
      fclose(index_file);
    }
  }

# ifdef SMALL_STACK
    free(index_file_name);
    free(dir);
# endif
}


/********************************************************
 *			ASTHELP_PATH			*
 ********************************************************
 * Return the path from environment variable ASTHELP,   *
 * or the default path.				        *
 ********************************************************/

char* asthelp_path(void)
{
  char *path;

  path = getenv("AST_HELP");
  if(path == NULL) {
    path = ".:" STD_DIR;
  }
  return path;
}
