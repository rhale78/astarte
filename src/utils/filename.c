/**********************************************************************
 * File:    utils/filename.c
 * Purpose: Management of file names.
 * Author:  Karl Abrahamson
 **********************************************************************/

#ifdef COPYRIGHT
/************************************************************************
 * Copyright (c) 2000 Karl Abrahamson					*
 * All rights reserved.							*
 * See file COPYRIGHT.txt for details.					*
 ************************************************************************/
#endif

/************************************************************************
 * This file contains functions that manage file and directory  	*
 * names, for example by extracting pieces of them, and replacing	*
 * local directories by absolute ones.					*
 *									*
 * Also included here are functions that provide interfaces to		*
 * system functions that want file names.  Internally, the directories  *
 * are separated by a fixed character, INTERNAL_DIR_SEP_CHAR (/).       *
 * Externally (when communicating with the operating system) the        *
 * directories are separated by EXTERNAL_DIR_SEP_CHAR.  The functions	*
 * here convert the names to internal or external format as needed.	*
 ************************************************************************/

#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#ifdef SMALL_STACK
#  ifdef USE_MALLOC_H
#    include <malloc.h>
#  endif
#endif
#include "../misc/misc.h"
#ifdef UNIX
# include <unistd.h>
#endif
#ifdef MSWIN
# include <io.h>
# include <dir.h>
#endif
#include "../alloc/allocate.h"
#include "../utils/strops.h"
#include "../utils/filename.h"
#include "../utils/lists.h"
#include "../utils/hash.h"
#include "../tables/tables.h"
#include "../classes/classes.h"
#include "../error/error.h"
#include "../parser/tokens.h"
#include "../parser/parser.h"
#include "../intrprtr/intrprtr.h"
#ifdef TRANSLATOR
# include "../standard/stdids.h"
#endif
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/****************************************************************
 *			absolute_std_dir			*
 ****************************************************************
 * ABSOLUTE_STD_DIR holds the absolute path name of the		*
 * standard directory, where the standard Astarte libraries are *
 * kept.  If this has not yet been computed, ABSOLUTE_STD_DIR	*
 * holds NULL.							*
 ****************************************************************/

PRIVATE char* absolute_std_dir = NULL;

/****************************************************************
 *			COMPUTE_ABSOLUTE_STD_DIR		*
 ****************************************************************
 * Install into ABSOLUTE_STD_DIR the absolute name of STD_DIR,  *
 * if it is not already there.					*
 ****************************************************************/

PRIVATE void compute_absolute_std_dir(void)
{
  if(absolute_std_dir == NULL) {
    absolute_std_dir = full_file_name(STD_DIR, 1);
  }
}


/****************************************************************
 *			GET_ABSOLUTE_STD_DIR			*
 ****************************************************************
 * Return the absolute name of STD_DIR.				*
 ****************************************************************/

CONST char* get_absolute_std_dir(void)
{
  compute_absolute_std_dir();
  return absolute_std_dir;
}


/****************************************************************
 *			BEGINS_WITH_DOLLAR     			*
 ****************************************************************
 * Return true if S begins with $/ or $\, or is just $.  This 	*
 * is used to tell if a file name needs to have $ replaced by   *
 * the standard directory.					*
 ****************************************************************/

PRIVATE Boolean begins_with_dollar(CONST char *s)
{
  return s[0] == '$' &&
         (s[1] == '/' || s[1] == '\\' || s[1] == 0);
}


/****************************************************************
 *			BEGINS_WITH_TILDE     			*
 ****************************************************************
 * Return true if S begins with ~/ or ~\, or is just ~.  This 	*
 * is used to tell if a file name needs to have ~ replaced by   *
 * the home directory.						*
 ****************************************************************/

#ifdef UNIX
PRIVATE Boolean begins_with_tilde(CONST char *s)
{
  return s[0] == '~' &&
         (s[1] == '/' || s[1] == '\\' || s[1] == 0);
}
#endif


/****************************************************************
 *			MAKE_ABSOLUTE       			*
 ****************************************************************
 * DIR contains a directory that is relative to BASE_DIR.       *
 * Change DIR to be the absolute directory name.  Buffer DIR	*
 * must have enough room for the full name.  It should have	*
 * MAX_FILE_NAME_LENGTH + 1 bytes.				*
 *								*
 * $ at the beginning of DIR is replaced by the standard ast	*
 * directory.  ~ is replaced by the home directory of name.     *
 *								*
 * DIR can be in either internal or external form on entry.	*
 * The result (DIR on exit) is in internal form,  meaning that  *
 * directories are separated by INTERNAL_DIR_SEP_CHAR.  	*
 *								*
 * If there is no such directory, then DIR is set to hold an	*
 * empty string.						*
 ****************************************************************/

void make_absolute(char *dir, CONST char *base_dir)
{
  force_internal(dir);
  install_standard_dir(dir, MAX_FILE_NAME_LENGTH);
# ifdef UNIX
    install_home_dir(dir, MAX_FILE_NAME_LENGTH);
# endif

  if(!is_absolute(dir)) {

#   ifdef SMALL_STACK
      char* temp_dir = (char *) BAREMALLOC(MAX_FILE_NAME_LENGTH + 1);
#   else
      char temp_dir[MAX_FILE_NAME_LENGTH + 1];
#   endif

    char *result_dir;

    /*-------------------------------------*
     * If the final name is too long, die. *
     *-------------------------------------*/

    if(strlen(base_dir) + strlen(dir) + 1 > MAX_FILE_NAME_LENGTH) {
      die(17, concat_string(base_dir, 
			    concat_string(INTERNAL_DIR_SEP_STR, dir)));
    }

    /*------------------------------------------------------*
     * Copy the result string into temp_dir, then into dir, *
     * to be sure dir is not corrupted.			    *
     *------------------------------------------------------*/

    sprintf(temp_dir, "%s%c%s", base_dir, INTERNAL_DIR_SEP_CHAR, dir);
    force_internal(temp_dir);
    result_dir = full_file_name(temp_dir, 1);
    if(result_dir != NULL) {
      strcpy(dir, result_dir);
    }
    else dir[0] = 0;

#   ifdef SMALL_STACK
      FREE(temp_dir);
#   endif
  }
}


/****************************************************************
 *			DIR_PREFIX       			*
 ****************************************************************
 * Copy the directory prefix of F to array DIR, and the file    *
 * name to array FNAME.  If FNAME is NULL, only array DIR is	*
 * modified.							*
 *								*
 * Note: F must be in internal form, meaning that the 		*
 * directory separator character is INTERNAL_DIR_SEP_CHAR	*
 ****************************************************************/

void dir_prefix(char *dir, char *fname, CONST char *f)
{
  int len;
  char *p;
  
  len = strlen(f);
  if(len > MAX_FILE_NAME_LENGTH) die(17, f);
  strcpy(dir, f);
  for(p = dir + len - 1; p >= dir && *p != INTERNAL_DIR_SEP_CHAR; p--);
  if(p >= dir) {
    if(fname != NULL) strcpy(fname, p+1);
    *p = '\0';
  }
  else {
    if(fname != NULL) strcpy(fname, f);
    strcpy(dir, ".");
  }
}


/*********************************************************
 *		GET_ROOT_NAME				 *
 *********************************************************
 * Get the root name of the file name in array SOURCE,   *
 * by deleting the extension.  For example, the root 	 *
 * name of "foo.bar" is "foo". Store the result in 	 *
 * array DEST. 						 *
 *							 *
 * It is acceptable for SOURCE to be a path, with a 	 *
 * directory part.  Everything after and including the   *
 * last '.' is removed.	 So if SOURCE is "mxr/foo.bar",  *
 * then DEST will hold "mxr/foo" after this function	 *
 * runs.						 *
 *							 *
 * If there is no dot in SOURCE, then DEST is a copy     *
 * of SOURCE.						 *
 *							 *
 * SIZE is the size of DEST. No more than that many	 *
 * characters will be put into it, including the null	 *
 * terminator.  If DEST cannot hold the full result,     *
 * then the action of this function is indeterminate.	 *
 *********************************************************/

void get_root_name(char *dest, CONST char *source, int size)
{
  char *p;

  strncpy(dest, source, size);
  for(p = dest + strlen(dest) - 1; p >= dest && *p != '.'; p--) {}
  if(p >= dest) *p = '\0';
}


/********************************************************************
 *                       FULL_FILE_NAME                             *
 ********************************************************************
 * Return the absolute name of file FILE_NAME.  For example, if	    *
 * FILE_NAME is "monster.txt" and the current directory is 	    *
 * /home/mydir, then the result is the string 			    *
 * "/home/mydir/monster.txt".  	
 *								    *
 * In the MSWIN implementation, the device is also added.  	    *
 * So if the current directory is c:\home, then full_file_name	    *
 * would yield "c:/home/monster.txt".       			    *
 *								    *
 * Return NULL if we can't find FILE_NAME.  			    *
 *								    *
 * If KIND is 0, file_name is a file.  				    *
 * If KIND is 1, file_name is a directory.			    *
 *								    *
 * NOTE: FILE_NAME must be in internal form, meaning that 	    *
 * directories are separated by INTERNAL_DIR_SEP_CHAR ('/'), 	    *
 * regardless of the operating system conventions.		    *
 * The result is also in internal form.				    *
 *								    *
 * NOTE: The result is in the string constant table, so results of  *
 * full_file_name can be compared for equality using ==.	    *
 *								    *
 * NOTE: Function get_full_file_name, below, is similar to 	    *
 * full_file_name, but it does not require that the name be in	    *
 * internal form, and it handles $/ and ~/ prefixes.  You might	    *
 * want that one instead.					    *
 ********************************************************************/

CONST char* full_file_name(CONST char *file_name, int kind)
{
# ifdef MSWIN
    int olddisk  = -1;  /* Indicate no known device */
    int newdisk;
# endif

# ifdef SMALL_STACK
    char* dir    = (char *) BAREMALLOC(MAX_FILE_NAME_LENGTH + 1);
    char* olddir = (char *) BAREMALLOC(MAX_FILE_NAME_LENGTH + 1);
    char* fname  = (char *) BAREMALLOC(MAX_FILE_NAME_LENGTH + 1);
# else
    char dir   [MAX_FILE_NAME_LENGTH + 1],
         olddir[MAX_FILE_NAME_LENGTH + 1],
         fname [MAX_FILE_NAME_LENGTH + 1];
# endif

  /*--------------------------------------------------------------*
   * When file_name is a file, break it into directory (dir) and  *
   * file (fname).  When file_name is a directory, just copy the  *
   * directory to dir, and set fname to an empty string.	  *
   *--------------------------------------------------------------*/

  if(kind == 0) {
    dir_prefix(dir, fname, file_name);
  }
  else {
    strcpy(dir, file_name);
    fname[0] = 0;
  }

  /*-------------------------------------------------------------*
   * The idea is to change to directory dir, and then ask where  *
   * we are.  But first, remember the current directory, so that *
   * it can be restored.					 *
   *-------------------------------------------------------------*/

  getcwd(olddir, MAX_FILE_NAME_LENGTH);

  /*------------------------------------------------------*
   * For MSWIN, get the device if necessary, and store it *
   * in newdisk.  If a device is specified in file_name,  *
   * then change to that device, remembering the old	  *
   * device in olddisk.	     				  *
   *------------------------------------------------------*/

# ifdef MSWIN
    if(dir[1] == ':') {
      char dirchar = dir[0];
      olddisk = getdisk();
      if('a' <= dirchar && dirchar <= 'z') dirchar += 'A' - 'a';
      newdisk = dirchar - 'A';
      setdisk(newdisk);
    }
# endif

  /*----------------*
   * Change to dir. *
   *----------------*/

  if(chdir_file(dir) != 0) {
#   ifdef SMALL_STACK
      FREE(dir); FREE(olddir); FREE(fname);
#   endif
    return NULL;
  }

  /*------------------------------------------------------*
   * Find out where we are.  This is dir, as a full path. *
   *------------------------------------------------------*/

  getcwd(dir, MAX_FILE_NAME_LENGTH);

  /*------------------------------------------------*
   * Set the current directory back to what it was. *
   *------------------------------------------------*/

# ifdef MSWIN
    if(olddisk >= 0) setdisk(olddisk);
# endif
  chdir(olddir);

  /*----------------------------------------------------*
   * Add the file name, if we want a file and not	*
   * just the directory.  Force the result into 	*
   * internal form.  If there is not enough room, then  *
   * die.						*
   *----------------------------------------------------*/

  if(kind == 0) {
    if(strlen(dir) + strlen(fname) + 1 > MAX_FILE_NAME_LENGTH) {
      die(17, concat_string(dir, concat_string(INTERNAL_DIR_SEP_STR, fname)));
    }
    strcat(dir, INTERNAL_DIR_SEP_STR);
    strcat(dir, fname);
  }
  force_internal(dir);

  /*----------------------*
   * Clean up and return. *
   *----------------------*/

# ifdef SMALL_STACK
    {char* result = string_const_tb0(dir);
     FREE(dir); FREE(olddir); FREE(fname);
     return result;
    }
# else
    return string_const_tb0(dir);
# endif
}


/****************************************************************
 *			GET_FULL_FILE_NAME			*
 ****************************************************************
 * This is the same as full_file_name, above, but it does not   *
 * require its input to be in internal form, and it performs    *
 * substitutions for leading $/ or ~/.				*
 ****************************************************************/

CONST char* get_full_file_name(CONST char *file_name, int kind)
{
  char *result;
  char* file_name_cpy = BAREMALLOC(MAX_FILE_NAME_LENGTH + 1);

  strcpy(file_name_cpy, file_name);
  force_internal(file_name_cpy);
  install_standard_dir(file_name_cpy, MAX_FILE_NAME_LENGTH);
# ifdef UNIX
    install_home_dir(file_name_cpy, MAX_FILE_NAME_LENGTH);
# endif
  result = full_file_name(file_name_cpy, kind);
  FREE(file_name_cpy);
  return result;
}


/****************************************************************
 *			ASO_NAME				*
 ****************************************************************
 * Change file name FILE_NAME to a name that ends on .aso, if 	*
 * it doesn't already, and return the resulting string.  (No	*
 * change is made to FILE_NAME.) The result string is in the	*
 * string table.						*
 *								*
 * NOTE: any .ast or .asi at the end will be changed to .aso.	*
 *								*
 * NOTE: the result string is in internal form.  FILE_NAME can  *
 * be in internal or external form.				*
 ****************************************************************/

CONST char* aso_name(CONST char *file_name)
{
  char *result;
# ifdef SMALL_STACK
    char* name = (char*) BAREMALLOC(MAX_FILE_NAME_LENGTH + 1);
# else
    char name[MAX_FILE_NAME_LENGTH+1];
# endif

  strcpy(name, file_name);
  force_internal(name);

  if(suffix_ignore_case(".aso", file_name)) {
    result = string_const_tb0(file_name);
    goto out;
  }

  if(strlen(file_name) >= MAX_FILE_NAME_LENGTH - 4) {
#   ifdef TRANSLATOR
      semantic_error(FILE_NAME_ERR, 0);
#   endif
    result = "none";
    goto out;
  }

  if(suffix_ignore_case(".ast", file_name) || 
     suffix_ignore_case(".asi", file_name)) {
    name[strlen(file_name) - 4] = '\0';
  }
  strcat(name, ".aso");
  result = string_const_tb0(name);

 out:
# ifdef SMALL_STACK
    FREE(name);
# endif
  return result;
}


/****************************************************************
 *			REDUCED_ASO_NAME			*
 ****************************************************************
 * Return the name of the .aso file for FILE_NAME, but replace  *
 * the standard directory as a prefix by "$".			*
 *								*
 * Note: The result is in internal form.			*
 ****************************************************************/

CONST char* reduced_aso_name(CONST char *file_name)
{
  char* aso = aso_name(file_name);
  compute_absolute_std_dir();
  if(prefix(absolute_std_dir, aso)) {
    return concat_string("$", aso + strlen(absolute_std_dir));    
  }
  else {
    return aso;
  }
}


/****************************************************************
 *			AST_NAME				*
 ****************************************************************
 * Make FILE_NAME into a .ast or .asi file name, and return the	*
 * result.  Parameter FORCE_AST determines how a choice between	*
 * .ast and .asi is made.					*
 *								*
 *   FORCE_AST	CHOICE						*
 *     0        Keep the current extension, if an extension of  *
 *		.ast or .asi is already present.  Otherwise,    *
 *		choose extension .asi, if such a file exists.   *
 *		Otherwise, choose extension .ast.		*
 *								*
 *     1	Keep the current extension, if an extension of  *
 *		.ast or .asi is already present.  Otherwise,	*
 *		choose extension .ast.				*
 *								*
 *     2	Choose .ast always.				*        
 *								*
 * If the resulting file name is too long, return "none".  The  *
 * longest allowed is MAX_FILE_NAME_LENGTH.			*
 *								*
 * NOTE: The result string is in internal form.  file_name can	*
 * be in internal or external form.				*
 ****************************************************************/

CONST char* ast_name(CONST char *file_name, Boolean force_ast)
{
  char *result;
# ifdef SMALL_STACK
    char* name = (char*) BAREMALLOC(MAX_FILE_NAME_LENGTH + 1);
# else
    char name[MAX_FILE_NAME_LENGTH+1];
# endif
  Boolean is_ast, is_asi;
  int len;
  struct stat stat_buf;

  len = strlen(file_name);
  if(len >= MAX_FILE_NAME_LENGTH - 4) {
#   ifdef TRANSLATOR
      semantic_error(FILE_NAME_ERR, 0);
#   endif
    result = "none";
    goto out;
  }

  strcpy(name, file_name);
  force_internal(name);
  if(suffix_ignore_case(".aso", name)) {
    name[len-4] = '\0';
  }
  is_ast = suffix_ignore_case(".ast", name);
  is_asi = suffix_ignore_case(".asi", name);
  if(force_ast > 1 && is_asi) {
    name[len-4] = '\0';
    is_asi = FALSE;
  }

  if(!is_ast && !is_asi) {
    if(force_ast != 0) {
      strcat(name, ".ast");
    }
    else {
      strcat(name, ".asi");
      if(stat(name, &stat_buf) != 0) {
        name[strlen(name)-1] = 't';
      }
    }
  }

  result = string_const_tb0(name);

 out:
# ifdef SMALL_STACK
     FREE(name);
# endif
  return result;
}


/*****************************************************************
 *			INSTALL_STANDARD_DIR			 *
 *****************************************************************
 * If S begins with $, then replace $ by the standard directory. *
 * Put the resulting string back into S.  Buffer S has		 *
 * physical length LEN.      					 *
 *								 *
 * Return true on success, false if there was not enough room to *
 * install the home directory.					 *
 *****************************************************************/

Boolean install_standard_dir(char *s, int len)
{
  register char *src, *dest;
  int s_len, std_dir_len;

  if(!begins_with_dollar(s)) return TRUE;

  /*------------------------------------------*
   * Check whether the directory fits into s. *
   *------------------------------------------*/

  std_dir_len = strlen(STD_DIR);
  s_len       = strlen(s);
  if(std_dir_len + s_len > len) return FALSE;

  /*----------------------------------------------*
   * Push the name back to make room for STD_DIR. *
   * Note that the $ will vanish, so there is one *
   * less character than strlen(s) to worry about.*
   *----------------------------------------------*/

  src = s + s_len;
  dest = src + std_dir_len - 1;
  if(dest >= s + len) dest = s + len - 1;
  while(src > s) {
    *(dest--) = *(src--);
  }  
  
  /*--------------------------------------*
   * Put the standard directory up front. *
   *--------------------------------------*/

  src = STD_DIR;
  dest = s;
  while(*src != 0) *(dest++) = *(src++);
  return TRUE;
}


/*****************************************************************
 *			INSTALL_HOME_DIR			 *
 *****************************************************************
 * If S begins with ~, then replace ~ by the home directory,	 *
 * given by the value of environment variable HOME. 		 *
 *								 *
 * Put the resulting string back into S.  Buffer S has		 *
 * physical length LEN.      					 *
 *								 *
 * Return true on success, false if there was not enough room to *
 * install the home directory.					 *
 *****************************************************************/

#ifdef UNIX
Boolean install_home_dir(char *s, int len)
{
  register char *src, *dest;
  char *home;
  int home_len, s_len;

  if(!begins_with_tilde(s)) return TRUE;

  /*-------------------------*
   * Get the home directory. *
   *-------------------------*/

  home = getenv("HOME");
  if(home == NULL) return TRUE;

  /*------------------------------------------*
   * Check whether the directory fits into s. *
   *------------------------------------------*/

  home_len = strlen(home);
  s_len    = strlen(s);
  if(home_len + s_len > len) return FALSE;

  /*----------------------------------------------*
   * Push the name back to make room for home. 	  *
   * Note that the $ will vanish, so there is one *
   * less character than strlen(s) to worry about.*
   *----------------------------------------------*/

  src = s + strlen(s);
  dest = src + strlen(home) - 1;
  if(dest >= s + len) dest = s + len - 1;
  while(src > s) {
    *(dest--) = *(src--);
  }  
  
  /*--------------------------------------*
   * Put the home directory up front.     *
   *--------------------------------------*/

  src = home;
  dest = s;
  while(*src != 0) *(dest++) = *(src++);
  return TRUE;
}
#endif


/****************************************************************
 *			FORCE_INTERNAL				*
 ****************************************************************
 * Force file name s to internal form, by replacing each	*
 * occurrence of ALT_DIR_SEP_CHAR by INTERNAL_DIR_SEP_CHAR.	*
 ****************************************************************/

void force_internal(char *s)
{
  char* p = s;
  while(*p != 0) {
    if(*p == ALT_DIR_SEP_CHAR) *p = INTERNAL_DIR_SEP_CHAR;
    p++;
  }
}


/****************************************************************
 *			FORCE_EXTERNAL				*
 ****************************************************************
 * Force file name s to external form, by replacing each	*
 * occurrence of INTERNAL_DIR_SEP_CHAR by EXTERNAL_DIR_SEP_CHAR.*
 ****************************************************************/

#if (EXTERNAL_DIR_SEP_CHAR == INTERNAL_DIR_SEP_CHAR)

void force_external(char *s) {
 force_internal(s);
}

#else

void force_external(char *s)
{
  char* p;

  force_internal(s);

  p = s;
  while(*p != 0) {
    if(*p == INTERNAL_DIR_SEP_CHAR) *p = EXTERNAL_DIR_SEP_CHAR;
    p++;
  }
}

#endif


/****************************************************************
 *		        FPRINT_EXTERNAL				*
 ****************************************************************
 * Print file path NAME on file F, in external form, with format*
 * FORM.  That is, use EXTERNAL_DIR_SEP_CHAR to separate	*
 * directories.	NAME should be in internal form.		*
 ****************************************************************/

#if (EXTERNAL_DIR_SEP_CHAR == INTERNAL_DIR_SEP_CHAR) 

void fprint_external(FILE *f, CONST char *form, CONST char *name)
{
  fprintf(f, form, name);
}

#else

void fprint_external(FILE *f, CONST char *form, CONST char *name)
{
  char *ename = strdup(name);
  if(ename != NULL) {
    force_external(ename);
    fprintf(f, form, ename);
    FREE(ename);
  }
}

#endif


/****************************************************************
 *			IS_ABSOLUTE				*
 ****************************************************************
 * Return TRUE if NAME is an absolute file name, and FALSE	*
 * otherwise.							*
 *								*
 * A NAME that begins with $/ or ~/ is considered absolute.	*
 * However, it is necessary to change the $ or ~ to its 	*
 * correct value, and that is not done here.			*
 *								*
 * Note: NAME should be in internal file name form, meaning	*
 * that the directory separator is INTERNAL_DIR_SEP_CHAR.	*
 ****************************************************************/

#ifdef MSWIN
Boolean is_absolute(char *name)
{
  return name[0] == INTERNAL_DIR_SEP_CHAR ||
         begins_with_dollar(name) ||
	 (name[1] == ':' && name[2] == INTERNAL_DIR_SEP_CHAR);
}
#endif

#ifdef UNIX
Boolean is_absolute(CONST char *name)
{
  return name[0] == INTERNAL_DIR_SEP_CHAR ||
         begins_with_tilde(name) ||
         begins_with_dollar(name);
}
#endif

/****************************************************************
 *			FILE_COLONSEP_TO_LIST			*
 ****************************************************************
 * String S is a list of files, separated by colons or 		*
 * semicolons.  Return the list of the files in S.  But		*
 * replace ~/ by $HOME/ and $/ by STD_DIR/ in each file name.	*
 ****************************************************************/

STR_LIST* file_colonsep_to_list(CONST char *s)
{
  LIST *p;
  char* newnam = NULL;
  LIST* names  = colonsep_to_list(s);

  for(p = names; p != NIL; p = p->tail) {
    if(begins_with_dollar(p->head.str) || begins_with_tilde(p->head.str)) {
      if(newnam == NULL) {
        newnam = (char *) BAREMALLOC(MAX_FILE_NAME_LENGTH + 1);
      }
      strcpy(newnam, p->head.str);
      if(install_standard_dir(newnam, MAX_FILE_NAME_LENGTH + 1)) {
#       ifdef UNIX
          if(install_home_dir(newnam, MAX_FILE_NAME_LENGTH + 1)) {
	    p->head.str = string_const_tb0(newnam);
	  }
#       else
          p->head.str = string_const_tb0(newnam);
#       endif
      }
    }
  }

  if(newnam != NULL) FREE(newnam);  
  return names;
}


/*=============================================================*/

#ifdef UNIX
 int rename(const char *path1, const char *path2);
 int rmdir(const char *path);
 int chdir(const char *path);
 int unlink(const char *path);
#endif


/****************************************************************
 *			FOPEN_FILE				*
 ****************************************************************
 * fopen_file(NAME, MODE) is the same as fopen(NAME, MODE), but *
 * it converts NAME to external format.				*
 ****************************************************************/

FILE* fopen_file(const char *name, char *mode)
{
  FILE* result;
  char* cpyname = strdup(name);
  if(cpyname != NULL) {
    force_external(cpyname);
    result = fopen(cpyname, mode);
    FREE(cpyname);
    return result;
  }
  else return NULL;
}


/****************************************************************
 *			OPEN_FILE				*
 ****************************************************************
 * open_file(NAME, FLAGS, MODE) is the same as			*
 * open(NAME, FLAGS,MODE), but it converts NAME to external	*
 * format.							*
 ****************************************************************/

int open_file(const char *name, int flags, mode_t mode)
{
  int result;
  char* cpyname = strdup(name);
  if(cpyname != NULL) {
    force_external(cpyname);
    result = open(cpyname, flags, mode);
    FREE(cpyname);
    return result;
  }
  else return -1;
}


/****************************************************************
 *			STAT_FILE				*
 ****************************************************************
 * stat_file(NAME, BUFF) is the same as stat(NAME, BUFF), 	*
 * but it converts NAME to external format.			*
 ****************************************************************/

int stat_file(const char *name, struct stat *buff)
{
  int result;
  char* cpyname = strdup(name);
  if(cpyname != NULL) {
    force_external(cpyname);
    result = stat(cpyname, buff);
    FREE(cpyname);
    return result;
  }
  else return -1;
}


/****************************************************************
 *			CHDIR_FILE				*
 ****************************************************************
 * chdir_file(NAME) is the same as chdir(NAME), but 		*
 * it converts NAME to external format.				*
 ****************************************************************/

int chdir_file(const char *name)
{
  int result;
  char* cpyname = strdup(name);
  if(cpyname != NULL) {
    force_external(cpyname);
    result = chdir(cpyname);
    FREE(cpyname);
    return result;
  }
  else return -1;
}


/****************************************************************
 *			UNLINK_FILE				*
 ****************************************************************
 * unlink_file(NAME) is the same as unlink(NAME), but 		*
 * it converts NAME to external format.				*
 ****************************************************************/

int unlink_file(const char *name)
{
  int result;
  char* cpyname = strdup(name);
  if(cpyname != NULL) {
    force_external(cpyname);
    result = unlink(cpyname);
    FREE(cpyname);
    return result;
  }
  else return -1;
}


/****************************************************************
 *			RENAME_FILE				*
 ****************************************************************
 * rename_file(NAME1, NAME2) is the same as			*
 * rename(NAME1, NAME2) but it converts names to external	*
 * format.							*
 ****************************************************************/

int rename_file(const char *name1, const char *name2)
{
  int result;
  char* cpyname1 = strdup(name1);
  char* cpyname2 = strdup(name2);
  if(cpyname2 != NULL) {
    force_external(cpyname1);
    force_external(cpyname2);
    result = rename(cpyname1, cpyname2);
    FREE(cpyname1);
    FREE(cpyname2);
    return result;
  }
  else return -1;
}


/****************************************************************
 *			RMDIR_FILE				*
 ****************************************************************
 * rmdir_file(NAME) is the same as rmdir(NAME), but 		*
 * it converts NAME to external format.				*
 ****************************************************************/

int rmdir_file(const char *name)
{
  int result;
  char* cpyname = strdup(name);
  if(cpyname != NULL) {
    force_external(cpyname);
    result = rmdir(cpyname);
    FREE(cpyname);
    return result;
  }
  else return -1;
}


/****************************************************************
 *			MKDIR_FILE				*
 ****************************************************************
 * mkdir_file(NAME) is the same as mkdir(NAME) (or		*
 * MKDIR(name, 0) when there is a mode parameter) , but 	*
 * it converts NAME to external format.				*
 ****************************************************************/

int mkdir_file(const char *name)
{
  int result;
  char* cpyname = strdup(name);
  if(cpyname != NULL) {
    force_external(cpyname);
#   ifdef UNIX
      result = mkdir(cpyname, 0);
#   else
      result = mkdir(cpyname);
#   endif
    FREE(cpyname);
    return result;
  }
  else return -1;
}


/********************************************************
 *			PRINT_FILE			*
 ********************************************************
 * Show the contents of file FILENAME on the standard   *
 * output.  FILENAME is forced to external format	*
 * in-place, so be sure to copy the buffer if 		*
 * necessary.						*
 *							*
 * File FILENAME is treated as a text file.		*
 ********************************************************/

void print_file(char *filename)
{
  int c;
  FILE *fin;

  force_external(filename);
  fin = fopen(filename, TEXT_READ_OPEN);
  if(fin == NULL) return;

  c = getc(fin);
  while(c != EOF) {
    putchar(c);
    c = getc(fin);
  }
}


/********************************************************
 *			CAN_READ			*
 ********************************************************
 * Return true if file file_name exists and is		*
 * readable.						*
 ********************************************************/

Boolean can_read(const char *file_name)
{
  struct stat stat_buf;

  if(stat_file(file_name, &stat_buf) != 0) return FALSE;
  if(stat_buf.st_mode & S_IREAD) return TRUE;
  return FALSE;
}

