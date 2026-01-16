/***********************************************************
 * File:    misc/misc.h
 * Purpose: Miscellaneous definitions.  This file should
 *          be included by every file to get the basic
 *          definitions.
 * Author:  Karl Abrahamson
 ************************************************************/

#ifndef MISC_H
#define MISC_H

/************************************************************
 * ASTC_VERSION is a string that is the version number of   *
 * the compiler.  It is reported in compiler listings.	    *
 ************************************************************/

#define ASTC_VERSION "0.8"

#include "../misc/options.h"
#include <stdio.h>
#ifdef UNIX
# ifdef TESTING
#  include "../misc/tstconfig.h"
# else
#  include "../misc/config.h"
# endif
#endif
#include "../misc/files.h"
#include "../misc/limits.h"
#include "../misc/perform.h"
#include "../misc/complain.h"
#include "../misc/types.h"

#define PRIVATE static
#define CONST 

#ifndef TRUE
#  define TRUE		1
#endif
#ifndef FALSE
#  define FALSE		0
#endif
#define NULL_S		((char *) 0)
#ifndef NULL
#  define NULL		0
#endif

/************************************************************************
 * INTERNAL_DIR_SEP_CHAR is the character used internally for 		*
 * separating directories, and ALT_DIR_SEP_CHAR is the alternate 	*
 * character.								*
 ************************************************************************/

#define INTERNAL_DIR_SEP_CHAR	'/'
#define INTERNAL_DIR_SEP_STR	"/"
#define ALT_DIR_SEP_CHAR	'\\'

/************************************************************************
 * NAME_SEP_CHAR is a character that is used to separate the		*
 * package name from the name of an identifier in a package-local	*
 * name.  It must be a character that cannot occur in a symbolic	*
 * or ordinary identifier, including identifiers that are 		*
 * created in the parser for temporaries.				*
 *									*
 * NAME_SEP_STR is the same as NAME_SEP_CHAR, but as a string constant. *
 ************************************************************************/

#define NAME_SEP_CHAR   '.'
#define NAME_SEP_STR	"."

/************************************************************************
 * HIDE_CHAR is a character that is used as the initial character of	*
 * a name to cause it to be hidden in listings.  This is used for 	*
 * internally generated names that the programmer is not interested in.	*
 *									*
 * HIDE_CHAR_STR is the same as HIDE_CHAR, but as a string constant. 	*
 ************************************************************************/

#define HIDE_CHAR	';'
#define HIDE_CHAR_STR	";"
 
/*---------------*
 * Max, Min, Abs *
 *---------------*/

#define max(a,b)        (((a) > (b)) ? (a) : (b))
#define min(a,b)        (((a) < (b)) ? (a) : (b))
#define absval(x)	(((x) >= 0) ? (x) : -(x))

/*-------------------------------------------------*
 * toint, toshort, tolong, tobool, tochar, toSizet *
 * convert to a given type.			   *
 *-------------------------------------------------*/

#define toint(x)  ((int)(x))
#define toshort(x) ((SHORT)(x))
#define tolong(x) ((LONG)(x))
#define tobool(x) ((Boolean)(x))
#define tochar(x) ((char)(x))
#define toSizet(x) ((SIZE_T)(x))

/*-------------*
 * From misc.c *
 *-------------*/

void die		(int, ...);
void fnl		(FILE *f);
void findent		(FILE *f, int n);
void init_err_msgs	(char ***which, char *file_name, int num_lines);
void breakhere		(void);
void flushTrace		(void);

/*----------------*
 * For debugging. *
 *----------------*/

#define tprintf  printf
#define tfprintf fprintf

#endif
