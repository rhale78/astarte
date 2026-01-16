/**********************************************************************
 * File:    misc/complain.h
 *
 * Purpose: Initial points where compiler and interpreter complain.
 *	    It also contains constants that limit the amount written
 *	    to ast.rts.
 *
 *          These can be changed.
 *
 * Author:  Karl Abrahamson
 **********************************************************************/


/************************************************************************
 * The first INTERRUPT_COUNT interrupts (SIGINT) in a row are tallied,  *
 * and are handled by switching to the debugger when they are noticed.  *
 * It can happen that more than INTERRUPT_COUNT interrupts occur before *
 * an interrupt is noticed and handled.  If that occurs, the 		*
 * interpreter is killed in a much less gracious way.			*
 ************************************************************************/

#define INTERRUPT_COUNT 2

/************************************************************************
 * INIT_RTS_FILE_CHARS is a soft limit on the number of characters	*
 * written to ast.rts.  This might be exceeded slightly, but it serves	*
 * as advice on cutting off the dump.					*
 ************************************************************************/

#define INIT_RTS_FILE_MAX_CHARS  1000000

/************************************************************************
 * INIT_RTS_FILE_MAX_ENTITY_CHARS is a soft limit on the number of      *
 * characters written to ast.rts as the value of one item.  This might  *
 * be exceeded slightly, but it serves as advice on cutting off the	*
 * dump.								*
 ************************************************************************/

#define INIT_RTS_FILE_MAX_ENTITY_CHARS  500

/***********************************************************************
 * INIT_MAX_PATMATCH_SUBST_DEPTH is the default nesting depth for      *
 * pattern match substitution and expansion at which the compiler      *
 * suspects an infinite expansion, and stops expanding.		       *
 ***********************************************************************/

#define INIT_MAX_PATMATCH_SUBST_DEPTH 50 

/************************************************************************
 * MAX_ST_DEPTH is the run-time stack depth at which the interpreter   	*
 * complains.  The interpreter will offer the user a chance to 	       	*
 * continue or to abort at this stack depth.  If it continues, it	*
 * doubles the depth at which it complains.				*
 ************************************************************************/

#define MAX_ST_DEPTH    400

/************************************************************************
 * MAX_HEAP_BYTES is the number of bytes in the heap that cause the	*
 * interpreter to complain about the heap size.  The interpreter 	*
 * offers the user a chance to continue or to abort.			*
 ************************************************************************/

#define MAX_HEAP_BYTES  1048576L

/**************************************************************************
 * INIT_MAX_OVLDS is the default maximum number of overloads allowed in   *
 * one declaration. 							  *
 **************************************************************************/

#define INIT_MAX_OVLDS 8
