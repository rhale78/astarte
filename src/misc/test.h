/******************************************************************
 * File:    misc/test.h
 * Purpose: Set compilation options for testing.
 * Author:  Karl Abrahamson
 ******************************************************************/

/*************************************************************
 * Define STRUCT_ENTITY to force type ENTITY to be a type    *
 * distinct from long int.				     *
 *************************************************************/

/*#define STRUCT_ENTITY*/

/*************************************************************
 * Set TESTING for testing, don't define for production      *
 * version. 						     *
 *************************************************************/

#define TESTING 1

/************************************************************************** 
 * Define DEBUG to get debugging code compiled in.  Otherwise don't 	  *
 * define DEBUG. 							  *
 **************************************************************************/

#define DEBUG  1

/********************************************************************
 * Define YYDEBUG for debugging parser.  Otherwise don't define it. *
 ********************************************************************/

#define YYDEBUG  1

/*************************************************************
 * Define GCTEST to force garbage collection in both the     *
 * compiler and the interpreter into test mode. 	     *
 * This means memory will not be recovered, resulting in a   *
 * MASSIVE memory leak.  Only do this for testing. In this   *
 * mode, many dangling pointer problems will be detected and *
 * reported.						     *
 *************************************************************/

/*#define GCTEST 1*/

/*************************************************************
 * Define GCT to force garbage collection in the interpreter *
 * to be done with high frequency.  This will really slow    *
 * down the interpreter.				     *
 *************************************************************/

/*#define GCT 1*/


