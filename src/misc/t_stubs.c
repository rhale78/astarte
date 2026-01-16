/**********************************************************************
 * File:    misc/t_stubs.c
 * Purpose: Stubs for the translator.  These are functions that are 
 *          needed by the interpreter, but should do nothing when 
 *          run in the translator, or should not be called.
 * Author:  Karl Abrahamson
 **********************************************************************/

#include "../misc/misc.h"
#include "../evaluate/evaluate.h"

int qwrap_tag(ENTITY a_unused) {return 0;}

