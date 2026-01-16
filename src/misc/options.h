/******************************************************************
 * File:    misc/options.h
 * Purpose: Set compilation options
 ******************************************************************/

#ifndef OPTIONS_H
#define OPTIONS_H

/*****************************************************************
 **             TO CONFIGURE THE INSTALLATION			**
 **								**
 **   1. Read and edit this file down to the line		**
 **      saying don't edit below here.				**
 **								**
 **   2. Edit file config.h to determine where the files	**
 **      can be found.						**
 *****************************************************************/

/********************************************************
 *              Operating System	                *
 *		Storage management			*
 *		Entity Implementation			*
 ********************************************************
 * Uncomment a line that selects the operating system,  *
 * the entity implementation scheme and the storage     *
 * management function.					*
 *							*
 * The options are					*
 *   OS:  Unix, MSWIN					*
 *   ***NOTE: MSWIN implementation is not currently     *
 *            working.  Only Unix is supported at this  *
 *            point.					*
 *							*
 *   Entity implementation: small or large.  		*
 *     If you use the small entity representation, then *
 *     you must use sbrk for memory management, and the *
 *     amount of memory that you can access might be    *
 *     less than with the large entity representation.  *
 *     							*
 *     The small entity representation results in 	*
 *     a more efficient interpreter.			*
 *							*
 *   Memory management: sbrk or malloc.			*
 *     You can use sbrk if it is available, or you can  *
 *     go through malloc.				*
 *							*
 * If you do not know what will work, use malloc and	*
 * large entities.					*
 ********************************************************/

#include "../misc/unixs.h"	/* Unix, small entities, sbrk */

/*#include "../misc/unixl.h"*/	/* Unix, large entities, sbrk */

/*#include "../misc/unixm.h"*/	/* Unix, large entities, malloc */

/*#include "../misc/win16.h"*/	/* MS Windows 3.1, large entities, malloc */

/********************************************************
 *			Architecture			*
 ********************************************************
 * Uncomment a line describing the architecture.	*
 *							*
 *  The word size is the size of an int.		*
 *							*
 *  Aligned is selected if a k byte quantity (long int, *
 *  for example) must be stored at an address that is	*
 *  a multiple of k.					*
 ********************************************************/

/*#include "../misc/64bita.h"*/		/* 64 bit, aligned */

#include "../misc/32bita.h"		/* 32 bit, aligned */

/*#include "../misc/32bitn.h"*/		/* 32 bit, unaligned */

/*#include "../misc/16bitn.h"*/		/* 16 bit, unaligned */

/********************************************************
 *			Testing?			*
 *			Debugging?			*
 ********************************************************
 * Uncomment one of the following.			*
 ********************************************************/

/*#include "../misc/test.h"*/	/* Testing, debugging */

/*#include "../misc/debug.h"*/	/* Debugging code compiled in, but 
				   no special testing */

#include "../misc/prod.h"	/* No testing or debugging */


/****************************************************************
 *                     Consistency patches                      *
 ****************************************************************
 * Do not edit below this line.  We just make sure that nothing *
 * really strange has been requested.				*
 ****************************************************************/

/*---------------------------------------------------------------*
 * Must have exactly one of USE_SBRK and USE_MALLOC.  Here, we   *
 * give preference to USE_MALLOC.				 *
 *---------------------------------------------------------------*/

#ifdef USE_MALLOC
#  undef USE_SBRK
#else
#  if (!defined USE_SBRK)
#    define USE_MALLOC
#  endif
#endif

/*----------------------------------------------------------------*
 * Cannot have SMALL_ENTITIES if also have USE_MALLOC -- they are *
 * not consistent with one another.				  *
 *----------------------------------------------------------------*/

#ifdef USE_MALLOC
#  undef SMALL_ENTITIES
#endif

#endif /* defined OPTIONS_H */

