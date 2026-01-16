/******************************************************************
 * File:    misc/unixl.h
 * Purpose: Set compilation options for Unix with large entities.
 ******************************************************************/

/*********************************
 * Define UNIX to indicate Unix. *
 *********************************/

#define UNIX 1

/**************************
 * Compile as standard C. *
 **************************/

#ifndef __STDC__
# define __STDC__
#endif

/**********************************
 * For gcc, use fixed prototypes. *
 **********************************/

#ifndef __USE_FIXED_PROTOTYPES__
# define __USE_FIXED_PROTOTYPES__
#endif

/************************************************************************
 * USE_ISNAN should be defined to cause isnan() to be used to check     *
 * doubles.								*
 ************************************************************************/

#define USE_ISNAN

/************************************************************************
 * EXTERNAL_DIR_SEP_CHAR is the character that separarates directories	*
 * in  paths.  For Unix, it is a /.  					*
 *									*
 * EXTERNAL_DIR_SEP_STR is similar, but is a string constant.  		*
 ************************************************************************/

#define EXTERNAL_DIR_SEP_STR 	"/"
#define EXTERNAL_DIR_SEP_CHAR 	'/'

/************************************************************************
 * MALLOC is the name of the memory allocator,  REALLOC is the name of  *
 * the memory reallocator, and FREE is the name of the memory		*
 * deallocator.								*
 *									*
 * BAREMALLOC is the name of a memory allocator that should test for    *
 * low memory.  BAREREALLOC is a similar name for REALLOC.		*
 ************************************************************************/

#define MALLOC 		malloc
#define REALLOC 	realloc
#define FREE 		free
#define BAREMALLOC	bare_malloc
#define BAREREALLOC	bare_realloc

/************************************************************************
 * USE_ALLOCA is defined in implementations that support alloca.	*
 ************************************************************************/

#define USE_ALLOCA

/************************************************************************
 * SMALL_STACK is defined in implementations that have a small		*
 * run-time stack.  Don't allocate large arrays on a small stack.	*
 ************************************************************************/

/*#define SMALL_STACK*/

/************************************************************************
 * ASTR_ERR_MSG(msg) is the statement that should be used to display	*
 * error message msg by the interpreter.				*
 ************************************************************************/

# define ASTR_ERR_MSG(msg) fprintf(stderr, msg)

/************************************************************************
 * STDERR is the standard error file.					*
 ************************************************************************/

# define STDERR stderr

/************************************************************************
 * The following are open modes for the fopen call.			*
 ************************************************************************/

# define TEXT_READ_OPEN 	"r"
# define TEXT_WRITE_OPEN 	"w"
# define TEXT_APPEND_OPEN 	"a"

# define BINARY_READ_OPEN 	"r"
# define BINARY_WRITE_OPEN 	"w"
# define BINARY_APPEND_OPEN 	"a"

/**************************************************************************
 * Define USE_SBRK for a Unix version that does storage allocation using  *
 * sbrk, and define USE_MALLOC for a version that uses malloc for storage *
 * allocation. 								  *
 **************************************************************************/

#define USE_SBRK
/*#define USE_MALLOC*/

/**************************************************************************
 * Define SMALL_ENTITIES for entities that are compressed into one word,  *
 * and don't define it for entities that occupy more than one word.  If   *
 * SMALL_ENTITIES is defined, then USE_SBRK must also be defined. 	  *
 **************************************************************************/

/*#define SMALL_ENTITIES*/



