/**********************************************************************
 * File:    utils/rdwrt.h
 * Purpose: Functions for putting and getting integers
 * Author:  Karl Abrahamson
 **********************************************************************/

/************************************************************************
 * MAX_BYTES_OF_INT is the maximum number of bytes put_int can advance  *
 * s. 									*
 *									*
 * CODED_INT_SIZE is the number of bytes used to represent a multi-byte *
 * integer in a .aso file.						*
 ************************************************************************/

#define CODED_INT_SIZE 3		/* Number of bytes used to code */
					/* an int in package.           */

LONG  		get_int_m	(FILE *f);
void 		write_int_m	(package_index n, FILE *f);
void 		gen_int_m	(CODE_PTR *c, package_index n);
int  		put_int_m	(CODE_PTR s, package_index n);
LONG	  	next_int_m	(CODE_PTR *s);
