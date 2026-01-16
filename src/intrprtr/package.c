/**********************************************************************
 * File:    intrprtr/package.c
 * Purpose: Read in packages for the interpreter.
 * Author:  Karl Abrahamson
 **********************************************************************/

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
 * This file is responsible for reading in .aso files, converting them  *
 * to internal representation, and storing their contents in memory.    *
 ************************************************************************/

#include <string.h>
#include <time.h>
#include <memory.h>
#include <stdarg.h>
#include <stdlib.h>
#ifdef USE_MALLOC_H
#  include <malloc.h>
#endif
#include <ctype.h>
time_t time(time_t *tloc);
#include <sys/stat.h>
#include "../misc/misc.h"
#ifdef UNIX
# include <sys/types.h>
# include <sys/unistd.h>
#endif
#ifdef MSWIN
# include <process.h>
#endif
#include "../parser/tokens.h"
#include "../lexer/modes.h"
#include "../utils/rdwrt.h"
#include "../utils/lists.h"
#include "../utils/strops.h"
#include "../utils/filename.h"
#include "../utils/hash.h"
#include "../alloc/allocate.h"
#include "../classes/classes.h"
#include "../machdata/entity.h"
#include "../machdata/except.h"
#include "../machstrc/machstrc.h"
#include "../evaluate/instruc.h"
#include "../evaluate/evaluate.h"
#include "../intrprtr/intrprtr.h"
#include "../tables/tables.h"
#include "../clstbl/classtbl.h"
#include "../clstbl/meettbl.h"
#include "../clstbl/m_dflt.h"
#include "../rts/rts.h"
#include "../show/gprint.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif
#ifdef MSWIN
# include <windows.h>
#endif

void va_die(int n, va_list args);
void package_die(int n, ...);

PRIVATE int check_mod_time (char *s, char c, time_t aso_mod_time);
PRIVATE int try_to_compile (char *file_name, char *msg);
PRIVATE void allocate_llabel_info(int i);
PRIVATE int handle_old_species(FILE *f, int instr, 
			       PACK_PARAMS *package_params, 
			       Boolean complain, Boolean from_deferral);
PRIVATE int handle_old_abstraction(FILE *f, int instr,
				   PACK_PARAMS *package_params,
				   Boolean complain, Boolean from_deferral);
PRIVATE Boolean process_package_declarations(PACK_PARAMS *package_params,
					     Boolean complain);
PRIVATE void put_int_in_package(package_index n);

/************************************************************************
 *			PUBLIC VARIABLES				*
 ************************************************************************/

/************************************************************************
 *			executes					*
 *			executes_size					*
 *			num_executes					*
 ************************************************************************
 * executes points to an array that holds execute dcls.  As packages 	*
 * are read, the execute dcls encountered are accumulated in this array *
 * In  intrprtr.c/do_executes, the execute dcls are read from this array*
 * and done. executes_size is the physical size, in records, of the 	*
 * array pointed to by executes.  num_executes is the number of records *
 * in executes that are actually occupied.				*
 *									*
 * XREF:								*
 *   Created here, and used in intrprtr.c to go through all executes.	*
 ************************************************************************/

struct execute * executes;
int executes_size, num_executes;


/************************************************************************
 *			package_descr					*
 *			package_descr_size				*
 *			num_packages					*
 ************************************************************************
 * package_descr[i] holds a description of package i (the i-th package	*
 * read).  The description includes the start and end address of the    *
 * array that holds the executable byte-code of this package, the name  *
 * of the package, the name of the file from which the package was read,*
 * and an array that holds information about which instructions were	*
 * compiled from which lines of the source file.  This information is 	*
 * used to produce debugging information.  				*
 *									*
 * package_descr_size is the physical size of the array pointed to by	*
 * package_descr.							*
 *									*
 * num_packages is the number of packages that have been read, or that 	*
 * have been started.  It is used to tell how many entries package_descr*
 * has.									*
 *									*
 * XREF:								*
 *   Created and filled here.						*
 *									*
 *   Used in show/getinfo.c and debug/m_debug.c to get information	*
 *   about packages for displaying.					*
 *									*
 *   Used in show/printrts.c for displaying info in ast.rts.		*
 *									*
 *   Used in evaluate/lazy.c to find the code that computes the		*
 *   value of a global id.						*
 *									*
 *   Used in intrprtr.c to get the location of code for executes.	*
 *									*
 *   Used in tables/m_glob.c for error reporting.			*
 ************************************************************************/

PACKAGE_DESCR * package_descr;
int package_descr_size;
int num_packages = 0;


/************************************************************************
 *			current_pack_params				*
 ************************************************************************
 * current_pack_params holds a description of the package that is 	*
 * currently being read in by read_package.  It includes such 		*
 * information as the name of the package, where this package's code	*
 * start, where the next instruction will be placed, and where in the	*
 * code labels are attached.  It is made public so that the information *
 * about the current package is available to intrprtr.c, m_glob.c and   *
 * m_debug.c when something goes wrong during read_package.		*
 ************************************************************************/

PACK_PARAMS * current_pack_params = NULL;


/************************************************************************
 *			env_descriptors					*
 *			env_descriptors_size				*
 *			next_env_descr_num				*
 ************************************************************************
 * env_descriptors points to an array of pointers.  The i-th entry in	*
 * that array points to a chain that describes an environment, by	*
 * indicating a correspondence between offset from the beginning of the *
 * environment, and the associated identifier and type.  That 		*
 * association can change as the code executes.  For example, at one 	*
 * point offset 1 might hold the binding of x, of type Natural, while	*
 * at another place in the same function offset 1 might hold the binding*
 * of y, of type Char.  This problem is solved by letting the members 	*
 * of the chain each hold a program-counter, an environment offset,	*
 * an identifier string, a type and a pointer to the next member of the	*
 * chain.  Entry (pc,off,name,ty,next) indicates that, when the		* 
 * code reaches program counter pc, offset off is set to hold the binding*
 * of identifier name, of type ty.					*
 * 									*
 * Actually, the type is stored as an array of type instructions that,	*
 * when executed, will place the type on the top of the type stack.  The*
 * program-counter is given as an offset from the start of the code for *
 * the package that contains this function.				*
 *									*
 * An entry in env_descriptors is created for each function read in.	*
 * There are several private variables, below, that are used while	*
 * reading in the packages, and that help manage the construction of	*
 * env_descriptors.  One is next_env_descr_num, which is public.	*
 *									*
 * next_env_descr_num	 is the number of entries that have been made	*
 *			 in env_descriptors.  It is the next index	*
 * 			 that will be used for an environment		*
 *			 descriptor.					*
 *									*
 * env_descriptors_size  holds the current physical size of the array 	*
 *			 pointed to by env_descriptors.			*
 *									*
 * XREF:								*
 *   Used in debug/m_debug.c and show/getinfo.c to get information	*
 *   about environments.						*
 ************************************************************************/

struct env_descr** env_descriptors;  
int 		   next_env_descr_num;
PRIVATE int	   env_descriptors_size;


/************************************************************************
 *			lazy_type_instrs				*
 ************************************************************************
 * lazy_type_instrs points to an array of pointers, each of which	*
 * points to byte-code that computes a type, and leaves the type on the *
 * top of the type stack.  Functions and instructions that create lazy	*
 * objects have, in the .aso file, instructions that produce their	*
 * result type.  Those instructions are added as an entry to		*
 * lazy_type_instrs, and the index of their pointer in lazy_type_instrs	*
 * is stored with the instructions in the internal executable byte-code.*
 *									*
 * XREF: Used in evaluate/evaluate.c to get type instructions for 	*
 * functions, defines and lazy quantities.				*
 ************************************************************************/

CODE_PTR *lazy_type_instrs;


/****************************************************************
 *			PRIVATE VARIABLES			*
 ****************************************************************/

/****************************************************************
 *		        deferred_packages			*
 ****************************************************************
 * deferred_packages is a chain of nodes linked through the	*
 * parent field.  Each node describes a package that has been   *
 * deferred because it could not perform an instruction, along  *
 * with the instruction that could not be done.			*
 ****************************************************************/

PACK_PARAMS* deferred_packages = NULL;

/****************************************************************
 *			package_size				*
 ****************************************************************
 * package_size holds successive sizes of package blocks.  The	*
 * last member should be 0.  When a package is read, its size   *
 * is estimated and an array of that size is allocated.  If the *
 * estimate turns out to be too small, the next size up in this *
 * list is chosen.						*
 ****************************************************************/

#ifdef BITS16
  PRIVATE LONG FARR package_size[] = {256L, 512L, 1024L, 2048L,
				    4096L, 8192L, 16384L, 
				    32768L, 65535L, 0L};
#else
 PRIVATE LONG package_size[] = {128L, 256L, 512L, 768L, 1024L, 1536L, 2048L, 
			      3072L, 4096L, 6144L, 8192L, 12288L, 16384L, 
			      24576L, 32768L, 49512L, 65536L, 99024L, 0L};
#endif


/************************************************************************
 *			already_read_packages				*
 ************************************************************************
 * already_read_packages is a list of the names of the files that have 	*
 * already been read.  In fact, when we just start to read a file, the  *
 * name of that file is put in already_read_packages.  This is used to 	*
 * be sure that a file is read only once.				*
 ************************************************************************/

PRIVATE STR_LIST* already_read_packages = NIL;


/************************************************************************
 *			env_descr_st					*
 *			env_descr_end_st				*
 *			env_descr_stop_label_st				*
 ************************************************************************
 * The following variables are used to manage the construction of 	*
 * env_descriptors.  See env_descriptors, above.		  	*
 *									*
 * env_descr_st		is a stack.  Its top is the index in		*
 *			env_descriptors of the current environment	*
 * 			descriptor in env_descriptors.			*
 *									*
 * env_descr_end_st     is a stack.  Its top is a pointer to last cell 	*
 *			of the current env descriptor chain, or NULL if *
 *			that chain has no entries.  When a new 		*
 *			identifier is added to the environment, it is 	*
 *			added to the end of the chain, and this pointer *
 *			is used to find that end.			*
 *									*
 * env_descr_stop_label_st is a stack.  Its top is a local label number.*
 *			   When a function or other construct that has  *
 *			   its own environment (such as a lazy context)	*
 *			   is started, the label where that context	*
 *			   ends is pushed onto this stack.  When that 	*
 *			   label is encountered, stacks env_descr_st,	*
 *			   env_descr_end_st and env_descr_stop_label_st	*
 *			   are all popped, so that we return to the	*
 *			   embedding environment.			*
 ************************************************************************/

PRIVATE INT_STACK	env_descr_st = NIL;;
PRIVATE INT_STACK	env_descr_stop_label_st = NIL;
PRIVATE LIST*		env_descr_end_st = NIL;


/************************************************************************
 *			lazy_type_instrs_size				*
 *			num_lazy_type_instrs				*
 ************************************************************************
 * lazy_type_instrs_size is the physical size of the array pointed 	*
 * to by lazy_type_instrs.  (See above for lazy_type_instrs.)  		*
 * num_lazy_type_instrs is the number of entries that lazy_type_instrs	*
 * currently has.							*
 ************************************************************************/

PRIVATE int 		num_lazy_type_instrs;
PRIVATE int 		lazy_type_instrs_size;


/************************************************************************
 *			last_instr					*
 ************************************************************************
 * last_instr is a rather ugly way of communicating information from 	*
 * get_type_instrs to read_package.  get_type_instrs stops reading at	*
 * either an ENTER_I or an END_LET_I instruction.  When read_package	*
 * reads type instructions, it sometimes needs to know what instruction	*
 * they ended on.  So get_type_instrs puts the end instruction in	*
 * last_instr.								*
 ************************************************************************/

PRIVATE int last_instr;


/************************************************************************
 *			exception_table					*
 ************************************************************************
 * Each exception has a number, or tag.  That tag is not fixed in the	*
 * .aso files, but must be chosen when packages are loaded in.		*
 * It is possible for the same exception to be declared in several	*
 * packages.  This is because packages that have imported an exception	*
 * from another package still need to be able to refer to that exception.*
 * Each package that refers to a given exception will have a global 	*
 * label instruction followed by an EXCEPTION_DCL_I instruction.  We 	*
 * need to be sure that the same tag is used for all occurrences of the *
 * exception.  When an EXCEPTION_DCL_I instruction is processed, the 	*
 * exception name is looked up in exception_table, to see if the same 	*
 * exception has been encountered before.  If not, then a tag is chosen *
 * for the new exception, and an entry is made in exception_table 	*
 * holding the exception name and tag.  If the exception is already in	*
 * that table, then the tag is just extracted from exception_table.	*
 ************************************************************************/

PRIVATE HASH2_TABLE* exception_table = NULL;

/************************************************************************
 *			llabel_prefix					*
 ************************************************************************
 * When an instruction is preceded by LONG_LLABEL_I(k), llabel_prefix   *
 * is set to k.  When a local label is read, it is combined with	*
 * llabel_prefix.  At the end of each instruction except LONG_LLABEL_I, *
 * llabel_prefix is set to 0.						*
 ************************************************************************/

PRIVATE int llabel_prefix = 0;

/************************************************************************
 *			llabel_info					*
 ************************************************************************
 * Sometimes, local labels are used before they are defined.  When that *
 * happens, an entry is made in these tables so that, when the label    *
 * definition is encountered, all prior references to it can be		*
 * patched.								*
 *									*
 *  llabel_info is an array of pointers.  The idea is that 		*
 *    llabel_info[i] points to an array that gives information about    * 
 *    local labels 256*i, 256*i+1, ..., 256*i+255.  If no labels in	*
 *    that range have been used, then llabel_info[i] is NULL.  It will	*
 *    be allocated when it is needed.					*
 *									*
 *  llabel_info[i]->location[j] is the index of label 256*i+j in the	*
 *    current package.  That is, it tells where that label is located.  *
 *									*
 *  llabel_info[i]->patch_locs[j] is a list of addresses that need	*
 *    patching.								*
 *									*
 *  llabel_info[i]->patch_instr_locs is a list of the addresses of	*
 *    instructions that contain local labels that need patching.	*
 *									*
 * Local labels are stored in instructions as offsets from the		*
 * current instruction.  So to do patching, we examine the lists	*
 * llabel_info[i]->patch_locs[j] and 					*
 * llabel_info[i]->patch_instr_locs[j] in parallel.			*
 * Let [k] indexing indicate indexing into a linked list.		*
 * Then for each i, j and k, backpatching sets the three bytes		*
 * beginning at address llabel_info[i]->patch_locs[j][k]  to		*
 * llabel_info[i]->location[j] - llabel_info[i]->patch_instr_locs[j][k].*
 ************************************************************************/

PRIVATE struct llabel_info_struct {
  package_index location[256];
  LIST*         patch_locs[256];
  LIST*         patch_instr_locs[256];
}* llabel_info[MAX_LOCAL_LABEL_CLASSES];


/********************************************************
 *			INIT_PACKAGE_READER		*
 ********************************************************
 * Initializes package reader.				*
 *							*
 * XREF: Called in intrprtr.c to initialize.		*
 ********************************************************/

void init_package_reader()
{
  /*--------------------------------------------------------------------*
   * Initialize some arrays so that they won't need to be reallocated 	*
   * for a while.							*
   *--------------------------------------------------------------------*/

  int k;

  env_descriptors       = (struct env_descr **) 
		       alloc(INIT_ENV_DESCR_SIZE * sizeof(struct env_descr *));
  env_descriptors_size  = INIT_ENV_DESCR_SIZE;
  next_env_descr_num    = 0;

  lazy_type_instrs      = (CODE_PTR *) 
		          alloc(INIT_LAZY_TYPE_INSTRS_SIZE * sizeof(char *));
  lazy_type_instrs_size = INIT_LAZY_TYPE_INSTRS_SIZE;
  num_lazy_type_instrs  = 0;

  executes              = (struct execute *)
    			  alloc(INIT_EXECUTES_SIZE * sizeof(struct execute));
  executes_size         = INIT_EXECUTES_SIZE;
  num_executes          = 0;

  package_descr		= (PACKAGE_DESCR *)
			  alloc(INIT_MAX_NUM_PACKAGES 
			        * sizeof(PACKAGE_DESCR));
  package_descr_size    = INIT_MAX_NUM_PACKAGES;

  allocate_llabel_info(0);
  for(k = 1; k < MAX_LOCAL_LABEL_CLASSES; k++) {
    llabel_info[k] = NULL;
  }
}

 
/********************************************************
 *			REALLOC_PACKAGE			*
 ********************************************************
 * Move the current package to a larger block.		*
 ********************************************************/

PRIVATE void realloc_package(void)
{
  LONG new_size, old_size;
  int *sizeind;
  CODE_PTR newstart;

  /*-----------------------------------------------*
   * Get the old and new sizees from package_size. *
   *-----------------------------------------------*/

  sizeind = &(current_pack_params->size_index);
  old_size = package_size[*sizeind];
  new_size = package_size[++(*sizeind)];
  if(new_size == 0) {
    package_die(53);
  }

  /*-------------*
   * Reallocate. *
   *-------------*/

  newstart = (CODE_PTR) 
          reallocate((char *) current_pack_params->start, 
		      old_size, new_size, TRUE);

  /*------------------------------------------------------*
   * Update current_pack_params, so that the pointers now *
   * point to the newly allocated array.		  *
   *------------------------------------------------------*/

  current_pack_params->start = newstart;
  current_pack_params->size  = new_size;
}


/********************************************************
 *			FGETSTR1			*
 *			FGETSTR1_IGNORE			*
 ********************************************************
 * Get a null-terminated string from file f, and store  *
 * it into array s as a null-terminated string.  n is 	*
 * the maximum number of characters that will be read,  *
 * excluding the null.  So array s should have at least *
 * n+1 bytes.						*
 *							*
 * fgetstr1_ignore reads a null-terminated string       *
 * from file f, and throws it away.			*
 ********************************************************/

#ifdef DEBUG
PRIVATE void fgetstr1(char *s, int n, FILE *f)
{
  fgets1(s, n, f);
  if(trace_puts) {
    trace_i(194, current_pack_params->num, s);
  }
}

#else

#define fgetstr1 fgets1

#endif

/*-----------------------------------------------------------*/

PRIVATE void fgetstr1_ignore(FILE *f)
{
  int c;

  do {
    c = getc(f);
  } while(c != 0 && c != EOF);
}


/*********************************************************
 *			FGETUC				 *
 *********************************************************
 * Get a single character from file f, and return it as  *
 * an unsigned integer.	This is only used in DEBUG mode. *
 * In nondebug mode, fgetuc is an alias for getc, cast	 *
 * to unsigned int.					 *
 *********************************************************/

#ifdef DEBUG

int fgetuc(FILE *f)
{
  int r = (unsigned) (getc(f));

  if(trace_puts) {
    trace_i(195, current_pack_params->num, r);
  }
  return r;
}

#endif


/********************************************************
 *			FGETINT				*
 ********************************************************
 * Get an integer from f.  The integer is stored in 	*
 * three consecutive bytes, low order byte first.	*
 ********************************************************/

#ifdef DEBUG

PRIVATE package_index fgetint(FILE *f)
{
  package_index n = get_int_m(f);

  if(trace_puts) {
    trace_i(196, current_pack_params->num, tolong(n));
  }
  return n;
}

#else

# define fgetint get_int_m

#endif


/********************************************************
 *			PUT_CHAR_IN_PACKAGE		*
 ********************************************************
 * Put character c at the current location where code   *
 * is being generated, for the current package.  The	*
 * location to put c in is determined from 		*
 * current_pack_params.  If necessary, reallocate the   *
 * package to make room.				*
 ********************************************************/

PRIVATE void put_char_in_package(UBYTE c)
{
  register package_index *current;

# ifdef DEBUG
    if(trace_puts) trace_i(197, toint(c));
# endif

  current = &(current_pack_params->current);
  if(*current >= current_pack_params->size) {
    realloc_package(); 
  }
  current_pack_params->start[(*current)++] = c;
}

#ifdef DEBUG
#define fast_put_char_in_package(c)\
{register package_index *current_index_pcip;\
 if(trace_puts) trace_i(197, toint(c));\
 current_index_pcip = &(current_pack_params->current);\
 if(*current_index_pcip >= current_pack_params->size) {\
   realloc_package(); \
 }\
 current_pack_params->start[(*current_index_pcip)++] = c;\
}
#else
#define fast_put_char_in_package(c)\
{register package_index *current_index_pcip;\
 current_index_pcip = &(current_pack_params->current);\
 if(*current_index_pcip >= current_pack_params->size) {\
   realloc_package(); \
 }\
 current_pack_params->start[(*current_index_pcip)++] = c;\
}
#endif

/********************************************************
 *		PUT_CHAR_IN_PACKAGE_FROM_FILE		*
 ********************************************************
 * Read a character from file f and put it at the	*
 * current location where code is being generated, for  *
 * the current package.  The location to put it in is	*
 * determined from current_pack_params.  If necessary,  *
 * reallocate the package to make room.			*
 ********************************************************/

PRIVATE void put_char_in_package_from_file(FILE *f)
{
  register package_index *current;
  register int c;

  c = getc(f);

# ifdef DEBUG
    if(trace_puts) {
      trace_i(195, current_pack_params->num, c);
      trace_i(197, c);
    }
# endif

  current = &(current_pack_params->current);
  if(*current >= current_pack_params->size) {
    realloc_package(); 
  }
  current_pack_params->start[(*current)++] = (UBYTE) c;
}


/********************************************************
 *			PUT_INT_IN_PACKAGE		*
 ********************************************************
 * put n into the package at the current location, as	*
 * determined from current_pack_params.  The integer	*
 * is stored as three consecutive bytes, low order	*
 * byte first.  If necessary, reallocate the package	*
 * to make room.					*
 ********************************************************/

PRIVATE void put_int_in_package(package_index n)
{
  register package_index *current;

# ifdef DEBUG
    if(trace_puts) trace_i(198, tolong(n));
# endif

  current = &(current_pack_params->current);
  if(*current >= current_pack_params->size - CODED_INT_SIZE) {
    realloc_package();
  }
  *current += put_int_m(current_pack_params->start + *current, n);
}


/********************************************************
 *			INDEX_AT_LABEL			*
 ********************************************************
 * There is a 3-byte integer in the current package at  *
 * the location refered to by global label n.  Return 	*
 * that integer.					*
 ********************************************************/

package_index index_at_label(int n)
{
  CODE_PTR loc =
    current_pack_params->start + current_pack_params->locationg[n];
  return next_int_m(&loc);
}


/********************************************************
 *			INDEX_AT_ADDR			*
 ********************************************************
 * There is a three byte integer stored at address s.   *
 * Return that integer.					*
 ********************************************************/

package_index index_at_addr(CODE_PTR *s)
{
  register package_index n = next_int_m(s);
  CODE_PTR loc = *s + n;
  return next_int_m(&loc);
}


/********************************************************
 *			READING_FILE			*
 ********************************************************
 * Print a message that we are reading the file_name    *
 * of package_params.					*
 ********************************************************/

PRIVATE void reading_file(char *filename)
{
  gprintf(STDERR, "Reading file %s\n", filename);
}


/********************************************************
 *			PACKAGE_DIE			*
 ********************************************************
 * Die, but also indicate the current file being read.  *
 ********************************************************/

void package_die(int n, ...)
{
  va_list args;

  if(current_pack_params != NULL) {
    reading_file(current_pack_params->file_name);
  }
  va_start(args, n);
  va_die(n,args);
}


/********************************************************
 *		   REDO_DEFERRED_INSTRUCTION		*
 ********************************************************
 * Redo the instruction that is indicated in the	* 
 * deferred_instr and deferred_name fields of	        *
 * package_params.					*
 *							*
 * Return true on success, false on failure.		*
 ********************************************************/

PRIVATE Boolean
redo_deferred_instruction(PACK_PARAMS *package_params, Boolean complain)
{
  int status;

  switch(package_params->deferred_instr) {
    case SPECIES_DCL_I:
    case FAMILY_DCL_I:
      status = handle_old_species(package_params->packfile, 
			          package_params->deferred_instr, 
			          package_params, complain, TRUE);
      break;

    case GENUS_DCL_I:
    case COMMUNITY_DCL_I:
      status = handle_old_abstraction(package_params->packfile, 
			              package_params->deferred_instr, 
			              package_params, complain, TRUE);
      break;

    default:
      die(187, package_params->deferred_instr);
      status = 0;
  }

  if(status == 0) return FALSE;
  else if(status == 1) package_die(190);
  return TRUE;
}


/********************************************************
 *	  	WAKE_UP_DEFERRED_PACKAGES		*
 ********************************************************
 * Restart any packages that are waiting for name to    *
 * be defined.						*
 *							*
 * Return true on success, false on failure.		*
 ********************************************************/

PRIVATE Boolean 
wake_up_deferred_packages(char *name, Boolean complain)
{
  PACK_PARAMS **p, *q, *package_params;
  
  package_params = current_pack_params;
  p = &deferred_packages;
  while(*p != NULL) {
    if((*p)->deferred_name == name) {
      q = *p;
      *p = q->parent;
      q->parent = NULL;
      current_pack_params = q;

#     ifdef DEBUG
        if(trace) trace_i(354, q->file_name);
#     endif

      redo_deferred_instruction(q, complain);
      if(!process_package_declarations(q, complain)) return FALSE;
      FREE(q);
      p = &deferred_packages;
    }
    else {
      p = &((*p)->parent);
    }
  }

# ifdef DEBUG
    if(trace) trace_i(353, package_params->file_name);
# endif

  current_pack_params = package_params;
  return TRUE;
}


/********************************************************
 *		   DEFER_REST_OF_PACKAGE		*
 ********************************************************
 * Copy package_params into the deferred_packages	*
 * chain.						*
 ********************************************************/

PRIVATE void 
defer_rest_of_package(PACK_PARAMS *package_params)
{
  PACK_PARAMS* pp = (PACK_PARAMS*) BAREMALLOC(sizeof(PACK_PARAMS));

# ifdef DEBUG
    if(trace) trace_i(355, package_params->file_name);
# endif

  *pp               = *package_params;
  pp->parent        = deferred_packages;
  deferred_packages = pp;
}


/********************************************************
 *			GET_TYPE_INSTRS			*
 ********************************************************
 * Read type building instructions from file f, up to 	*
 * an END_LET_I or ENTER_I instruction.  Return a 	*
 * pointer to a copy of them in the heap.  The 		*
 * instructions in the heap will be terminated by 	*
 * END_LET_I.  Each global label will be translated	*
 * to the value stored at that global label.		*
 *							*
 * If parameter ignore is true, then the type building  *
 * instructions are read, but no memory is allocated    *
 * for them, and NULL is returned.			*
 *							*
 * If there is a problem reading the instructions, then *
 * die (if complain is true) or just return NULL (if    *
 * complain if false).					*
 *							*
 * When get_type_instrs returns, it sets last_instr 	*
 * to the last instruction (END_LET_I or ENTER_I) that	*
 * it read.						*
 *							*
 * reallocate_tcode is used to reallocate the array 	*
 * in the heap where the type-building code is being 	*
 * put.  It doubles the size of the array.		*
 ********************************************************/

PRIVATE void reallocate_tcode(CODE_PTR *tcode, int *tcode_size, 
				 CODE_PTR *t, Boolean in_heap)
{
  CODE_PTR new_space;
  int new_size;

  new_size = 2*(*tcode_size);
  new_space = (CODE_PTR ) 
              reallocate((char *)(*tcode), *tcode_size + 1, new_size + 1,
			 in_heap);
  *t = new_space + (*t - *tcode);
  *tcode_size = new_size;
  *tcode = new_space;
}

/*------------------------------------------------------------*/

PRIVATE CODE_PTR get_type_instrs(FILE *f, Boolean ignore, Boolean complain)
{
  int i, c, tcode_size;
  register int bytes_generated;
  CODE_PTR s, t, tcode;
  Boolean tcode_in_heap;
  UBYTE tcode_init[MAX_TYPE_CODE_SIZE + 1];

  /*--------------------------------------------------------------------*
   * We will start with tcode in the stack, and tcode_in_heap = false.	*
   * This way we avoid allocating too much memory in the heap.   	*
   * if tcode needs to grow, we move it to the heap, and set 	 	*
   * in_heap = true.  This keeps us from using a lot of stack	 	*
   * space, as alloca would if it were used.			 	*
   *--------------------------------------------------------------------*/

  tcode = tcode_init;
  tcode_size = MAX_TYPE_CODE_SIZE;
  tcode_in_heap = FALSE;

  /*----------------------------------------------------------------------*
   * Read the instructions.  t points to the current generation location. *
   * bytes_generated is the number of bytes that have been generated.	  *
   *----------------------------------------------------------------------*/

  t = tcode;
  bytes_generated = 0;
  for(;;) {
    c = getc(f);
#   ifdef DEBUG
      if(trace_puts) {
        trace_i(195, current_pack_params->num, c);
      }
#   endif
    switch(instinfo[c].class) {

	case GLABEL_TYPE_INST:

	  /*---------------------------------------------------------*
	   * Each of these instructions is followed by a three-byte  *
	   * integer that is a global label, when it is read in.  As *
 	   * the instruction is stored internally, the instruction is*
 	   * followed by the index stored in the current package at  *
	   * that global label.					     *
	   *---------------------------------------------------------*/

	  if(!ignore) {

	    /*-------------------------------*
	     * Be sure there is enough room. *
	     *-------------------------------*/

            if(bytes_generated + CODED_INT_SIZE + 1 >= tcode_size) {
	      reallocate_tcode(&tcode, &tcode_size, &t, tcode_in_heap);
	      tcode_in_heap = TRUE;
	    }

	    /*----------------------------------------*
	     * Install the instruction and the index. *
	     *----------------------------------------*/

	     *(t++) = c;
	     i = put_int_m(t, index_at_label(toint(get_int_m(f))));
	     t += i;
	     bytes_generated += i + 1;
	  }

	  else get_int_m(f);

	  break;
	
	case TWO_BYTE_TYPE_INST:

	  /*--------------------------------------------------------*
	   * This instruction is followed by two bytes indicating a *
	   * scope and offset of an environment cell that holds the *
	   * type to be fetched.  Reallocate if necessary, and copy *
	   * the instruction and two bytes.			    *
	   *--------------------------------------------------------*/

	  if(!ignore) {

	    /*-------------------------------*
	     * Be sure there is enough room. *
	     *-------------------------------*/

	    if(bytes_generated + 3 >= tcode_size) {
	      reallocate_tcode(&tcode, &tcode_size, &t, tcode_in_heap);
	      tcode_in_heap = TRUE;
	    }

	    /*--------------------------------------------*
	     * Install the instruction and the two bytes. *
	     *--------------------------------------------*/

	    bytes_generated += 3; 
	    *(t++) = c;
	    *(t++) = fgetuc(f);
	    *(t++) = fgetuc(f);
	  }

	  else {
	    fgetuc(f); fgetuc(f);
	  }

	  break;

        case BYTE_TYPE_INST:

	  /*----------------------------------------------------*
	   * These instructions are followed by a single byte.  *
	   * reallocate if necessary, and copy the instruction  *
	   * and the byte.					*
	   *----------------------------------------------------*/

	  if(!ignore) {
	    if(bytes_generated + 2 >= tcode_size) {
	      reallocate_tcode(&tcode, &tcode_size, &t, tcode_in_heap);
	      tcode_in_heap = TRUE;
	    }
	    bytes_generated += 2; 
	    *(t++) = c;
	    *(t++) = fgetuc(f);
	  }

	  else fgetuc(f);

	  break;

	case NO_TYPE_INST:

	  /*---------------------------------------------*
	   * These instructions have no parameter bytes. *
	   *---------------------------------------------*/

	  if(!ignore) {
	    if(bytes_generated + 1 >= tcode_size) {
	      reallocate_tcode(&tcode, &tcode_size, &t, tcode_in_heap);
	      tcode_in_heap = TRUE;
	    }
	    bytes_generated++; 
	    *(t++) = c;
	  }
	  break;

	case END_LET_INST:
        case ENTER_INST:

	  /*----------------------------------*
	   * These end the type instructions. *
	   *----------------------------------*/

	  last_instr = c;
	  if(!ignore) {
	    bytes_generated++;
	    *t = END_LET_I;
	  }
	  goto copy;  /* Exit the loop. */

	default: 
          if(complain) package_die(54, (char *) c);
	  else return NULL;

    } /* end switch */
  } /* end for */

 copy:

  /*------------------------------------------------------------*
   * If we are to ignore these type instructions, then just 	*
   * return NULL.						*
   *------------------------------------------------------------*/

  if(ignore) return NULL;

  /*------------------------------------------------------------*
   * If the code is still in the stack, copy it into the heap.  *
   * If it is in the heap, just return a pointer to it.		*
   *------------------------------------------------------------*/

  if(!tcode_in_heap) {
    s = (CODE_PTR) alloc(bytes_generated);
    memcpy(s, tcode, bytes_generated);
    return s;
  }
  else return tcode;
}


/****************************************************************
 *			READ_TYPE_INSTRS_G	       		*
 ****************************************************************
 * Read type instructions into type instruction storage, making	*
 * an entry in lazy_type_instrs for them.  Store the index in   *
 * lazy_type_instrs of these instructions into the package, and *
 * return that index.					 	*
 *								*
 * If there is a problem, then die (if complain is true) or     *
 * return -1 (if complain is false).				*
 ****************************************************************/

PRIVATE int read_type_instrs_g(FILE *f, Boolean complain)
{
  LONG new_size;
  int n;

  /*--------------------------------------------*
   * If necessary, reallocate lazy_type_instrs. *
   *--------------------------------------------*/

  if(num_lazy_type_instrs >= lazy_type_instrs_size) {
    new_size = 2*lazy_type_instrs_size;
    lazy_type_instrs = (CODE_PTR *) 
      reallocate((char *)lazy_type_instrs,
	      lazy_type_instrs_size * sizeof(char *),
	      new_size * sizeof(char *), TRUE);
    lazy_type_instrs_size = new_size;
  }

  /*-------------------------------------------------------------*
   * Read the instructions and install them in lazy_type_instrs. *
   *-------------------------------------------------------------*/

  n = num_lazy_type_instrs++;
  lazy_type_instrs[n] = get_type_instrs(f, FALSE, complain);
  put_int_in_package(n);
  if(lazy_type_instrs[n] == NULL) return -1;
  else return n;
}


/********************************************************
 *			EXPORT_RUNTIME_DEFAULT		*
 ********************************************************
 * Install dflt as a default for the genus or 		*
 * community described by ctc into the list of defaults *
 * that are exported by the package described by info   *
 * structure packg.					*
 ********************************************************/

PRIVATE void
export_runtime_default(CLASS_TABLE_CELL* ctc, TYPE *dflt, 
		       PACK_PARAMS *packg)
{
  SET_LIST(packg->dfaults, 
	   int_cons(ctc->num, type_cons(dflt, packg->dfaults)));
}


/********************************************************
 *			PUT_LINE_RECORD			*
 ********************************************************
 * Line n of the source file begins at the current  	*
 * generation location in the current package.  Record	*
 * this information.					*
 ********************************************************/

PRIVATE void put_line_record(int n)
{
  PACKAGE_DESCR* pd = &(package_descr[current_pack_params->num]);
  int m = pd->log_lines_size;
  int k = pd->phys_lines_size;

  /*------------------------------------------*
   * Reallocate the lines array if necessary. *
   *------------------------------------------*/

  if(m >= k) {
    int new_size = k + k;
    struct line_rec* new_lines =
      (struct line_rec*)
      reallocate((char *) pd->lines,
		 k * sizeof(struct line_rec),
		 new_size * sizeof(struct line_rec), TRUE);
    pd->lines = new_lines;
    pd->phys_lines_size = new_size;
  }

  /*------------------------*
   * Record this line data. *
   *------------------------*/

  pd->lines[m].offset = current_pack_params->current;
  pd->lines[m].line = n;
  pd->log_lines_size++;
}


/********************************************************
 *			PUT_ID_DESCRIPTION		*
 ********************************************************
 * Identifier 'name' is being defined at offset 	*
 * pc_offset from the start of the current package.  	*
 *							*
 * File f, if not NULL, contains type instructions 	*
 * terminated by END_LET_I that construct the type of	*
 * identifier 'name'.  Parameter instr is the 		*
 * instruction that defines this identifier.		*
 *							*
 * If instr is a DEF_I, then the type instructions need *
 * to be put into lazy_type_instrs.  In any case, store *
 * all of this information in env_descriptors.		*
 *							*
 * If there is a problem, die (if complain is true) or  *
 * return FALSE (if complain is false).			*
 ********************************************************/

PRIVATE Boolean 
put_id_description(int instr, int offset, char *name, 
		   FILE *f, LONG pc_offset, Boolean complain)
{
  struct env_descr *p, *q;
  int n;
	
  /*----------------------------------------------------*
   * Get a new env descriptor node for this definition. *
   *----------------------------------------------------*/

  p              = (struct env_descr *) alloc_small(sizeof(struct env_descr));
  p->pc_offset   = pc_offset;
  p->env_offset  = offset;
  p->name        = make_perm_str(name);

  /*--------------------------------------------------------------------*
   * If f is not NULL, read the type instructions for this identifier,  *
   * and store them in the type_instrs field of this descriptor record. *
   * For a DEF_I instruction, also store the type instructions in	*
   * lazy_type_instrs.							*
   *--------------------------------------------------------------------*/

  if(f == NULL) p->type_instrs = NULL;
  else if(instr != DEF_I) {
    if((p->type_instrs = get_type_instrs(f, FALSE, complain)) == NULL) {
      return FALSE;
    }
  }
  else {
    n = read_type_instrs_g(f, complain);
    if(n < 0) return FALSE;
    p->type_instrs = lazy_type_instrs[n];
  }

  /*------------------------------------------------------------*
   * Add this record to the end of the list for this env. The 	*
   * stacks env_descr_st and env_descr_end_st tell where to 	*
   * do the addition.						*
   *------------------------------------------------------------*/

  p->next        = NULL;
  q              = top_env_descr(env_descr_end_st);
  n              = toint(top_int(env_descr_st));

# ifdef DEBUG
    if(trace_env_descr) {
      trace_i(199, p->name, offset, pc_offset, n);
    }
# endif

  if(q == NULL) env_descriptors[n] = p;
  else q->next = p;
  env_descr_end_st->head.env_descr = p;
  return TRUE;

}


/****************************************************************
 *			GET_PD_ENTRY				*
 ****************************************************************
 * Return the package descriptor for program counter pc, or	*
 * NULL if there is none.  					*
 ****************************************************************/

PACKAGE_DESCR* 
get_pd_entry(CODE_PTR pc)
{
  int i;
  PACKAGE_DESCR *p;

  for(i = 0; i < num_packages; i++) {
    p = package_descr + i;
    if(p->begin_addr <= pc && pc <= p->end_addr) return p;
  }
  return NULL;
}


/********************************************************
 *		GET_PD_ENTRY_BY_FILE_NAME		*
 ********************************************************
 * Return the package_descr entry for the package whose *
 * file name is filename, or NULL if there is none.	*
 ********************************************************/

PACKAGE_DESCR* 
get_pd_entry_by_file_name(char *filename)
{
  int i;

  for(i = 0; i < num_packages; i++) {
    if(strcmp(package_descr[i].file_name, filename) == 0) {
      return package_descr + i;
    }
  }
  return NULL;
}


/********************************************************
 *		GET_PD_ENTRY_BY_PACKAGE_NAME		*
 ********************************************************
 * Return the package_descr entry for the package whose *
 * package name is pname, or NULL if there is none.	*
 *							*
 * pname can be either the interface or implementation  *
 * name.						*
 ********************************************************/

PACKAGE_DESCR* 
get_pd_entry_by_package_name(char *pname)
{
  int i;

  for(i = 0; i < num_packages; i++) {
    char* impname = package_descr[i].imp_name;
    if(strcmp(package_descr[i].name, pname) == 0 ||
       (impname != NULL && strcmp(impname, pname) == 0)) {
      return package_descr + i;
    }
  }
  return NULL;
}


/********************************************************
 *		ALLOCATE_LLABEL_INFO			*
 ********************************************************
 * Allocate llabel_info[i], and initialize the new	*
 * arrays.						*
 ********************************************************/

PRIVATE void allocate_llabel_info(int i)
{
  llabel_info[i] = (struct llabel_info_struct *)
			   alloc(sizeof(struct llabel_info_struct));
  memset(llabel_info[i], 0, sizeof(struct llabel_info_struct));
}


/********************************************************
 *			GET_LOCAL_LABEL			*
 ********************************************************
 * Read a local label number from file f, and put	*
 * its actual location (as an offset from instr_pc)	*
 * in the current package, at the current location in   *
 * the package.						*
 *							*
 * If the location is not yet known, make an entry in	*
 * the patch table so that the location will be		*
 * inserted at patch time.  curr_pc is the location	*
 * where the patch should be put.  			*
 *							*
 * Return the local label.				*
 ********************************************************/

PRIVATE int get_local_label(FILE *f, LONG instr_pc, LONG curr_pc)
{
  struct llabel_info_struct* this_info;

  /*----------------*
   * Get the label. *
   *----------------*/

  int l = fgetuc(f);

  /*----------------------------------------------*
   * If necessary, allocate space in llabel_info. *
   *----------------------------------------------*/

  if(llabel_info[llabel_prefix] == NULL) {
     allocate_llabel_info(llabel_prefix);
  }
  this_info = llabel_info[llabel_prefix];

  /*----------------------------------------------------------------------*
   * If the label location is known, then just put it in the package now. *
   *----------------------------------------------------------------------*/

  if(this_info->location[l] > 0) {
    put_int_in_package(this_info->location[l] - instr_pc); 
  }

  /*------------------------------------------------------------------*
   * If the label location is not known, then make an entry in the    *
   * patch list so that the location will be installed at patch time. *
   * Put a 0 in the code array as a place holder.		      *
   *------------------------------------------------------------------*/

  else {
    SET_LIST(this_info->patch_locs[l], 
	     int_cons(curr_pc, this_info->patch_locs[l]));
    SET_LIST(this_info->patch_instr_locs[l], 
	     int_cons(instr_pc, this_info->patch_instr_locs[l]));
    put_int_in_package(0);
  }

  /*-------------------------*
   * Return the local label. *
   *-------------------------*/

  return l + (llabel_prefix << 8);
}


/********************************************************
 *			DO_PATCHES			*
 ********************************************************
 * Perform the local label patches that have been	*
 * requested. 						*
 *							*
 * max_local_label is the largest label to be done.	*
 *							*
 * Note: The reference counts of the lists in patch_locs*
 * and patch_instr_locs are all dropped, so they are	*
 * lost.						*
 ********************************************************/

PRIVATE void do_patches(int max_local_label)
{
  int i, i_low, i_high, loc, instr_loc;
  LIST *p, *q;
  struct llabel_info_struct* this_info;

  for(i = 0; i <= max_local_label; i++) {
    i_low  = i & 0xff;
    i_high = i >> 8;
    this_info = llabel_info[i_high];
    for(p = this_info->patch_locs[i_low], 
	q = this_info->patch_instr_locs[i_low]; 
	p != NIL; 
	p = p->tail, q = q->tail) {

      /*----------------------------------------------------------*
       * If this label is undefined, there is something seriously *
       * wrong, since the program should never reference an	  *
       * undefined label.					  *
       *----------------------------------------------------------*/

      if(this_info->location[i_low] == 0) {
        package_die(55, i, current_pack_params->name);
      }
      else {
        loc       = toint(p->head.i);
        instr_loc = toint(q->head.i);
#       ifdef DEBUG
	  if(trace) {
	    trace_i(200, current_pack_params->start + loc,
		    tolong(this_info->location[i_low] - instr_loc));
	  }
#       endif
	put_int_m(current_pack_params->start + loc, 
		  this_info->location[i_low] - instr_loc);
      }
    }

    /*-----------------*
     * Free the lists. *
     *-----------------*/

    drop_list(this_info->patch_locs[i_low]);
    drop_list(this_info->patch_instr_locs[i_low]);
  }
}
 

/********************************************************
 *			POSSIBLY_REALLOC_LOCATIONG	*
 ********************************************************
 * Make sure there are at least n+1 cells in		*
 * pack->locationg.  If not, reallocate it.		*
 ********************************************************/

PRIVATE void possibly_realloc_locationg(PACK_PARAMS *pack, int n)
{
  LONG old_size = pack->locationg_size;
  LONG new_size = old_size;

  if(old_size > n) return;

  while(new_size <= n) new_size += new_size;
  pack->locationg = (package_index *) 
                    reallocate((char *) pack->locationg, 
			       old_size * sizeof(package_index), 
			       new_size * sizeof(package_index), TRUE);
  pack->locationg_size = new_size;
}


/********************************************************
 *			READ_TO_END			*
 ********************************************************
 * Read instructions from file f and copy them to the	*
 * current package, until an END_I instruction is read.	*
 *							*
 * If complain is false and something goes wrong, then  *
 * return false.  Normally, return true.		*
 *							*
 * XREF:						*
 *   This function uses set_start_loc from		*
 *   tables/m_glob.c.  set_start_loc is used for 	*
 *   handling let{copy} declarations.  Read about 	*
 *   set_start_loc in tables/m_glob.c.			*
 ********************************************************/

PRIVATE Boolean 
read_to_end(FILE *f, Boolean complain)
{
  register int c;
  int end_label, max_local_label, let_kind = 0;
  package_index m;
  LONG instr_pc, curr_pc;

# ifdef DEBUG
    LONG initial_pc = current_pack_params->current;
# endif

  /*--------------------------------------------------------------------*
   * We need to clear the patch info for local label location patching. *
   *--------------------------------------------------------------------*/

  max_local_label = end_label = 0;  /* Setting end_label suppresses warning */
  {register int n;
   for(n = 0; n < MAX_LOCAL_LABEL_CLASSES; n++) {
     if(llabel_info[n] != NULL) {
       memset(llabel_info[n], 0, sizeof(struct llabel_info_struct));
     }
   }
  }

  /*-------------------------------------------------*
   * Main loop: read the instructions one at a time. *
   *-------------------------------------------------*/
 
  c = getc(f);
# ifdef DEBUG
    if(trace_puts) {
      trace_i(195, current_pack_params->num, c);
    }
# endif
  while(c != EOF && c != END_I) {

#   ifdef DEBUG
      if(trace_puts) {
	read_instr_names();
	fprintf(TRACE_FILE, "*%s\n", instr_name[c]);
      }
#   endif

    instr_pc = current_pack_params->current;
    curr_pc  = instr_pc + 1;

    /*------------------------------------------------------------------*
     * Put the instruction (c) into the package, and then deal with 	*
     * its parameters, if any. 						*
     *------------------------------------------------------------------*/

    fast_put_char_in_package(c);
    switch(instinfo[c].class) {

	/*------------------------------------------------------------*
	 * The following instructions have three bytes of parameters. *
	 *------------------------------------------------------------*/

	case LONG_NUM_PARAM_INST:
	  put_char_in_package_from_file(f);

	  /* No break: continue with next two bytes. */

	/*----------------------------------------------------------*
	 * The following instructions have two bytes of parameters. *
	 *----------------------------------------------------------*/

	case TWO_BYTE_PARAMS_INST:
        case TWO_BYTE_TYPE_INST:
	case TY_PREF_INST:
	  put_char_in_package_from_file(f);
	  /* No break: continue with next byte. */

	/*-----------------------------------------------------*
	 * The following instructions have one parameter byte. *
	 *-----------------------------------------------------*/

	case BYTE_PARAM_INST:
        case BYTE_TYPE_INST:
	case PREF_INST:
	  put_char_in_package_from_file(f);
	  break;

	/*-----------------------------------------------------*
	 * The following instructions have no parameter bytes. *
	 *-----------------------------------------------------*/

        case NO_PARAM_INST:
        case NO_TYPE_INST:
	  break;


#       ifdef NEVER
        case LLABEL_ENV_NUM_INST:
	  /*---------------------------*
	   * Read the byte parameter.  *
	   *---------------------------*/

	  put_char_in_package_from_file(f);
	  curr_pc++;
          /* No break -- fall through to next case. */
#       endif


	/*----------------------------------------------------------*
	 * The following instructions have a local label parameter, *
	 * and are  followed by					    *
 	 *   (1) An END_LET_I-terminated sequence of type building  *
	 *       instructions,				      	    *
	 *   (2) An environment size byte			    *
	 *   (3) More executable code.			      	    *
	 *----------------------------------------------------------*/

        case LLABEL_ENV_INST:

           /*-----------------------------------*
            * Read and process the local label. *
            *-----------------------------------*/

	   end_label = get_local_label(f, instr_pc, curr_pc);

	   /*------------------------------------------------*
	    * Read and store the type building instructions. *
	    *------------------------------------------------*/

	   if(read_type_instrs_g(f, complain) < 0) return FALSE;

	  /*------------------------------------------*
	   * Get the environment size and descriptor. *
	   *------------------------------------------*/

	  goto get_env_size;    /* Below, under STOP_G_INST. */
	
	/*----------------------------------------------------------*
	 * A STOP_G_I instruction is followed by an environmet size *
	 * byte and the executable code for a function.	    	    *
	 * The following code is shared by instructions that have   *
	 * bodies terminated by label l.  Setting l = -1 indicates  *
	 * that there is no label for a STOP_G_I instruction.	    *
	 *----------------------------------------------------------*/

        case STOP_G_INST:
	  end_label = -1;

	get_env_size:

          /*---------------------------------*
           * Copy the environment size byte. *
           *---------------------------------*/

          {UBYTE mm = fgetuc(f);
	   put_char_in_package(mm);

	   /*-----------------------------------------------------------*
	    * Start a new environment descriptor if this environment 	*
 	    * is nonempty.  Put the index of the environment descriptor *
	    * in the environment descriptor table into the package.	*
	    *-----------------------------------------------------------*/

	   if(mm != 0) {
	     register int n;
	     push_env_descr(env_descr_end_st, NULL);
	     push_int(env_descr_stop_label_st, end_label);
	     n = next_env_descr_num++;
	     put_int_in_package(n);
	     push_int(env_descr_st, n);

#            ifdef DEBUG
	       if(trace_env_descr) trace_i(201, n);
#            endif

	     /*------------------------------------------------*
	      * Reallocate the environment table if necessary. *
	      *------------------------------------------------*/

	     if(n >= env_descriptors_size) {
	       LONG new_size = 2*env_descriptors_size;
	       env_descriptors = (struct env_descr **) 
		 reallocate((char *) env_descriptors, 
			    env_descriptors_size * sizeof(char *),
			    new_size * sizeof(char *), TRUE);
	       env_descriptors_size = new_size;
	     }

	     /*----------------------------------------------*
	      * Add the environment descriptor to the table. *
	      *----------------------------------------------*/

	     env_descriptors[n] = NULL;
	   }

	   /*------------------------------------------------------*
	    * If the environment is empty, put 0 into the package. *
	    *------------------------------------------------------*/

	   else {  /* mm == 0 */
	     put_int_in_package(0);
	   }
	  }
	  break;


	/*------------------------------------------------------*
	 * The followin instructions are followed by a local	*
 	 * label and a one byte parameter.			*
	 *------------------------------------------------------*/

        case BYTE_LLABEL_INST:

	  {register int offset;

	   /*----------------------*
	    * Get the local label. *
	    *----------------------*/

           get_local_label(f, instr_pc, curr_pc);

	   /*-----------------------------*
	    * Get the one byte parameter. *
	    *-----------------------------*/

	   offset = fgetuc(f);
	   put_char_in_package(offset);

	   /*-----------------------------------------------------------*
	    * The one byte parameter of a COROUTINE_I instruction is	*
	    * offset of the coroutine value in the environment.  Make	*
	    * an entry for it.						*
	    *-----------------------------------------------------------*/

	   if(c == COROUTINE_I) {
	     if(!put_id_description(c, offset, ".coroutineVal", NULL, 
				    0L, complain)) {
	       return FALSE;
	     }
	   }
	   break;
	  }

	/*------------------------------------------------------*
	 * The following instructions are followed by a local	*
 	 * label.  A DEF_I is additionally followed by a one	*
	 * byte offset, an identifier (a null-terminated	*
	 * string), a sequence of type- building instructions, 	*
	 * an END_LET_I and an environment size byte.		*
	 *------------------------------------------------------*/

	case DEF_INST:
	case LLABEL_PARAM_INST:

	  get_local_label(f, instr_pc, curr_pc);

	  /*--------------------------------------------------------------*
	   * Handle a DEF_I instruction similarly to a LET_I instruction. *
	   *--------------------------------------------------------------*/

	  if(c != DEF_I) break;
	  /* No break : continue with next case. */

	/*--------------------------------------------------------------*
	 * The following instructions are followed by a one byte	*
	 * offset, an identifier (a null- terminated string), a		*
	 * sequence of type-building instructions, and an END_LET_I.	*
	 * Additionally, a DEF_I instruction, which gets here from	*
	 * above, has an environment size byte.				*
	 *--------------------------------------------------------------*/

	case LET_INST:
	  {char name[MAX_ID_SIZE+1];
	   register char *s;
	   int offset;
	   register int i, n;

	   /*-----------------------------------*
	    * Copy the offset into the package. *
	    *-----------------------------------*/

	   put_char_in_package((offset = fgetuc(f)));

	   /*--------------------------*
	    * Get the name of this id. *
	    *--------------------------*/

	   s = name;
	   n = 0;
	   while((i = fgetuc(f)) != 0) {
	     if(n < MAX_ID_SIZE) {n++; *(s++) = i;}
	   }
	   *s = '\0';

	   /*-----------------------------------------------------------*
	    * Read the type instructions, and put this id description 	*
	    * in table. 						*
	    *-----------------------------------------------------------*/

	   if(!put_id_description(c, offset, name, f, instr_pc, complain)) {
	     return FALSE;
	   }

	   /*-----------------------------------------------------------*
	    * For a DEF_I, copy the environment size to the package, 	*
	    * and start a new environment. That is done above at	*
	    * get_env_size, so we just jump there.			*
	    *-----------------------------------------------------------*/

	   if(c == DEF_I) goto get_env_size;
	   break;
	  }


        /*--------------------------------------------------------------*
         * The following are followed by a one-byte offset. They are	*
         * replaced by corresponding LET instructions in the internal 	*
         * form.							*
         *--------------------------------------------------------------*/

        case RELET_INST:
          switch(c) {
            case RELET_AND_LEAVE_I:
	      let_kind = LET_AND_LEAVE_I;
	      break;

            case FINAL_RELET_AND_LEAVE_I:
	      let_kind = FINAL_LET_AND_LEAVE_I;
	      break;

	    case RELET_I:
	      let_kind = LET_I;
              break;
          }
          current_pack_params->start[instr_pc] = c = let_kind;
	  put_char_in_package_from_file(f);
	  break;


	/*--------------------------------------------------------------*
	 * The following instructions are followed by a global label,	*
	 * where the tag of the exception is stored.  Replace the	*
	 * global label from the .aso file by a byte that holds the	*
	 * tag.								*
	 *--------------------------------------------------------------*/

	case EXC_INST:

	  {/*-----------------------*
	    * Get the global label. *
            *-----------------------*/
	   
	   register int n = toint(fgetint(f));

	   /*---------------------------------------------------*
	    * The global label should exist, but to be on the	*
	    * safe side make sure that global label n is	*
	    * meaningful.					*
	    *---------------------------------------------------*/

	   possibly_realloc_locationg(current_pack_params, n);

	   /*---------------------------------------------------*
	    * Put the tag in the package, by getting the byte 	*
	    * referred to by this global label.			*
	    *---------------------------------------------------*/

	   put_char_in_package(current_pack_params->start[
			         current_pack_params->locationg[n]]);
	   break;
	  }


	/*------------------------------------------------------*
	 * The following instructions are followed by a global	*
 	 * label, which is to be replaced by the index stored	*
	 * at that label.  Included is the special case of a	*
	 * GET_GLOBAL_I instruction, which must put a mark in	*
	 * the poly table for the identifier being fetched, 	*
	 * for m_globals.c:insert_global.			*
         *							*
	 * BYTE_GLABEL_PARAM_INST instructions are additionally	*
	 * followed by another byte.				*
	 *------------------------------------------------------*/

	case BYTE_GLABEL_INST:
	case GLABEL_PARAM_INST:
        case GLABEL_TYPE_INST:

	  {int n = toint(fgetint(f));
	   m = index_at_label(n);
	   put_int_in_package(m);
	   if(c == GET_GLOBAL_I && set_start_loc != NULL) {
	     *set_start_loc = outer_bindings[m].poly_table;
	   }
	   else if(instinfo[c].class == BYTE_GLABEL_INST) {
	     put_char_in_package_from_file(f);
	   }
	   break;
	  }


	/*------------------------------------------------------*
	 * Handle LONG_LLABEL_I by putting the next byte 	*
 	 * into llabel_prefix, and removing LONG_LLABEL_I from	*
	 * the package.		    				*
	 *------------------------------------------------------*/

	case LONG_LLABEL_INST:
	  
	  llabel_prefix = fgetuc(f);
	  current_pack_params->current--;
	  goto after_llabel_prefix_reset;


	/*--------------------------------------------------------------*
	 * Handle a local label by making an entry in llabel_info	*
	 * referring to the current position.	A LLABEL_I instruction	*
	 * should not show up in the run-time code, so 			*
	 * current_pack_params->current is decremented, to back up	*
	 * across the LLABEL_I instruction that was just inserted.	*
	 *--------------------------------------------------------------*/

	case LLABEL_INST:

	  {/*-------------------------------------------------------*
  	    * n is the low order byte of the label.  The high order *
	    * byte is in llabel_prefix.				    *
            *-------------------------------------------------------*/

	   register int n;
	   current_pack_params->current--;
	   n = fgetuc(f);
	   if(llabel_info[llabel_prefix] == NULL) {
	     allocate_llabel_info(llabel_prefix);
	   }
	   llabel_info[llabel_prefix]->location[n] = 
	     current_pack_params->current;

	   /*------------------------------------------------------*
	    * For the remainder of this section, set n to the full *
	    * label (including both low and high order bytes).      *
	    *------------------------------------------------------*/

	   n |=  (llabel_prefix << 8);
	   if(n > max_local_label) max_local_label = n;

	   /*-----------------------------------------------------------*
	    * If this local label signals the end of the current scope	*
	    * then pop the environment descriptor stacks.		*
	    *-----------------------------------------------------------*/

	   if(n == top_int(env_descr_stop_label_st)) {
	     pop(&env_descr_st);
	     pop(&env_descr_end_st);
	     pop(&env_descr_stop_label_st);
	   }

	   break;
	  }


	/*--------------------------------------------------------------*
	 * Remove a LINE_I instruction from the code array, since	*
	 * it should not show up in the run-time code.			*
	 *--------------------------------------------------------------*/

	case LINE_INST:

	  current_pack_params->current--;

	  /*------------------------------------------------------------*
	   * Get the line number, which immediately follows the LINE_I	*
	   * instruction.						*
	   *------------------------------------------------------------*/

	  {register int n = toint(fgetint(f));
# 	   ifdef DEBUG
	     if(trace_puts) trace_i(202, n);
#	   endif

	   /*-------------------------------*
	    * Install the line information. *
	    *-------------------------------*/

	   put_line_record(n);
	  }
	  break;


       default: 

	 /*-----------------------------------------------------*
	  * If the instruction is none of the above, then we 	*
	  * have big trouble.					*
	  *-----------------------------------------------------*/

	 package_die(56, toint(c), current_pack_params->name);

    } /* end switch */

    /*----------------------------------------------------------------*
     * After each instruction except LONG_LLABEL_I, set llabel_prefix *
     * to 0.							      *
     *----------------------------------------------------------------*/

    llabel_prefix = 0;

   after_llabel_prefix_reset:

    /*-------------------------------------*
     * Get the next instruction, and loop. *
     *-------------------------------------*/

    c = fgetuc(f);
  } /* end while(c != EOF...) */

  /*--------------------------------------------------------------------*
   * The STOP_G_I instruction begins a scope, but has no local label	*
   * where the scope is popped.  We do the pop at the end of the	*
   * declaration.							*
   *--------------------------------------------------------------------*/

  pop(&env_descr_st);
  pop(&env_descr_end_st);
  pop(&env_descr_stop_label_st);

  /*--------------------------------------------------------------------*
   * Perform local label patching, installing information about forward	*
   * references to local labels.					*
   *--------------------------------------------------------------------*/

  do_patches(max_local_label);

# ifdef DEBUG
    if(trace) {
      CODE_PTR strt;
      int n;
      strt = current_pack_params->start;
      instr_pc = initial_pc;
      m = current_pack_params->current;
      while(instr_pc < m) {
	fprintf(TRACE_FILE, "%ld: ", instr_pc);
	c = strt[instr_pc];
	n = print_instruction(TRACE_FILE, strt + instr_pc + 1, c);
	instr_pc += n;
      }
    }
# endif

  return TRUE;

}


/********************************************************
 *		END_READ_PACKAGE			*
 ********************************************************
 * This completes the job of reading a package.         *
 * It should only be called after a main read_package,  *
 * not after each import.				*
 *							*
 * Return true on success, false on failure.		*
 ********************************************************/

Boolean end_read_package(Boolean complain)
{
  if(deferred_packages != NULL) {
    if(complain) die(189);
    else return FALSE;
  }
  process_ahead_meets_tm();
  close_classes_p();
  return TRUE;
}


/********************************************************
 *			LOAD_PACKAGE_STDF		*
 ********************************************************
 * Load package s.  This function is called by a running*
 * function that wants to load a package on-the-fly.	*
 ********************************************************/

ENTITY load_package_stdf(ENTITY s)
{
# ifdef SMALL_STACK
    char* name = (char*) BAREMALLOC(MAX_FILE_NAME_LENGTH + 1);
# else
    char name[MAX_FILE_NAME_LENGTH + 1];
# endif
  ENTITY rest;
  Boolean ok;

  /*----------------------------------------------------*
   * Get a null-terminated string from entity s.  If	*
   * the result is too long, fail with exception	*
   * LIMIT_EX.  Otherwise, read the package.  If the	*
   * read fails, fail with exception NO_FILE_EX.	*
   *----------------------------------------------------*/

  copy_str(name, s, MAX_FILE_NAME_LENGTH-4, &rest);
  if(!IS_NIL(rest)) failure = LIMIT_EX;
  else {

    /*--------------------------------------------------*
     * read_package is told not to print a complaint	*
     * if there is a problem.				*
     *--------------------------------------------------*/

    ok = read_package(name, 0, FALSE);
    if(ok == 1) ok = end_read_package(FALSE);
    if(ok != 1) {
      failure = NO_FILE_EX;
      failure_as_entity = qwrap(NO_FILE_EX, s);
    }
  }

  /*------------------------------------------------------------*
   * This function implements an instruction that pushes ()	*
   * onto the stack as its result.				*
   *------------------------------------------------------------*/

# ifdef SMALL_STACK
    FREE(name);
# endif

  return hermit;
}


/********************************************************
 *	         CHECK_IF_OUT_OF_DATE			*
 ********************************************************
 * Check if pkg_name is out of date.  If so, try to     *
 * recompile.  If called for, exit the interpreter if   *
 * no progress can be made.				*
 *							*
 * The return value is FALSE if there was no need to	*
 * recompile and TRUE if a recompilation was done.	*
 *							*
 * When FALSE is returned, *package_bytes is set to the *
 * number of bytes in the package that is to be read.	*
 *							*
 * Parameter f is supplied so that it can be closed if  *
 * necessary.  When FALSE is not returned, f is closed.	*
 ********************************************************/

PRIVATE Boolean 
check_if_out_of_date(char *pkg_name, LONG *package_bytes, FILE *f)
{
  time_t aso_mod_time;
  struct stat stat_buf;
  int cmt_result;

  /*----------------------------------------------------*
   * Get the file status information for the .aso file. *
   *----------------------------------------------------*/

  if(stat_file(pkg_name, &stat_buf) == 0) {
    aso_mod_time  = stat_buf.st_mtime;
    *package_bytes = stat_buf.st_size;
  }
  else {

    /*--------------------------------------------*
     * The file existed before!  Where did it go? *
     *--------------------------------------------*/

    fclose(f);
    package_die(146, pkg_name);
    return FALSE;  /* just to make compiler happy */
  }

  /*------------------------------------------------*
   * Check against the .ast file and the .asi file. *
   *------------------------------------------------*/

  cmt_result = check_mod_time(pkg_name, 't', aso_mod_time);
  if(cmt_result == 0) {
    cmt_result = check_mod_time(pkg_name, 'i', aso_mod_time);
  } 

  if(cmt_result == 2) {
    fclose(f);
    return TRUE;
  }
  else if(cmt_result == 3) {
    fclose(f);
    if(verbose_mode) {
      fprintf(stderr, "Cannot read \"%s\": giving up\n", pkg_name);
    }
    clean_up_and_exit(1);
  }
  return FALSE;
}


/********************************************************
 *		 OPEN_PACKAGE_FOR_READING		*
 ********************************************************
 * Open package pkg_name for reading, and return the    *
 * open file.  If it is not possible to open it,        *
 * return NULL, and complain if parameter complain is   *
 * true.						*
 *							*
 * If this package cannot be opened, and ok_to_compile  *
 * is true, then possibly try to compile it.		*
 * If compilation is done, then return NULL and set     *
 * *did_compile to true.  Normally, *did_compile is     *
 * set to FALSE.					*
 ********************************************************/

PRIVATE FILE* 
open_package_for_reading(char *pkg_name, Boolean complain, 
			 Boolean ok_to_compile, Boolean *did_compile)
{
  FILE *f;

  *did_compile = FALSE;

  if(verbose_mode) {
    fprintf(stderr, "Starting file \"%s\"\n", pkg_name);
  }
  f = fopen_file(pkg_name, BINARY_READ_OPEN);

  if(f == NULL) {
    if(ok_to_compile) {
      int ttc = 
	try_to_compile(pkg_name, "File %s is not compiled.\nCompile?[y|n] ");
      if(ttc == 1) {
	*did_compile = TRUE;
	return NULL;
      }
      else if(ttc == 2) {
	if(verbose_mode) {
	  fprintf(stderr, "Cannot read \"%s\": giving up\n", pkg_name);
	}
	clean_up_and_exit(1);
      }
    }
    if(complain) package_die(146, pkg_name);

#   ifdef DEBUG
      if(trace) trace_i(348, nonnull(pkg_name));
#   endif

    return NULL;
  }

  return f;
}


/****************************************************************
 *			SET_UP_PACKAGE_DESCRIPTION		*
 ****************************************************************
 * Set up description of this package. The number of lines is   *
 * initially estimated as 1/40 of the number of bytes in the    *
 * .aso file.							*
 ****************************************************************/

PRIVATE void 
set_up_package_description(PACK_PARAMS *package_params,
			   LONG package_bytes)
{
  int n;
  int num = package_params->num;
  PACKAGE_DESCR* pd = &(package_descr[num]);

  pd->num           = num;
  pd->begin_addr    = NULL;
  pd->name          = package_params->name;
  pd->imp_name	    = package_params->imp_name;
  pd->file_name     = package_params->file_name;

  n = toint(package_bytes/40);
  if(n < 8) n = 8;
  pd->log_lines_size = 0;
  pd->phys_lines_size = n;
  pd->lines =(struct line_rec*) alloc(n * sizeof(struct line_rec));
}


/****************************************************************
 *			ALLOCATE_PACKAGE_ARRAYS			*
 ****************************************************************
 * Allocate the arrays that will be used by the package		*
 * described by package_params.  We allocate 60% of the		*
 * number of bytes in the .aso file, since that is roughly	*
 * the number of bytes typically used.				*
 *								*
 * *package_bytes tells how many bytes are in the		*
 * .aso file.  If this is 0, it has not been determined, and    *
 * we must get it.						*
 ****************************************************************/

PRIVATE void
allocate_package_arrays(PACK_PARAMS *package_params, 
			LONG *package_bytes)
{
  int ii;
  package_index n;

  /*----------------------------------*
   * Get package_bytes, if necessary. *
   *----------------------------------*/

  if(*package_bytes == 0) {
    struct stat stat_buf;

    if(stat_file(package_params->file_name, &stat_buf) == 0) {
      *package_bytes = stat_buf.st_size;
    }
    else {

      /*--------------------------------------------*
       * The file existed before!  Where did it go? *
       *--------------------------------------------*/

      package_die(146, package_params->file_name);
    }
  }

  /*--------------------------*
   * Get the code array size. *
   *--------------------------*/

  ii = 0;
  while(package_size[ii] != 0 && 5*package_size[ii] < 3*(*package_bytes)) {
    ii++;
  }
  if(package_size[ii] == 0) ii--;

  /*--------------------------------------------------*
   * Allocate the arrays and install the information. *
   *--------------------------------------------------*/

  package_params->size_index = ii;
  package_params->size       = n = package_size[ii];
  package_params->start      = allocate_package(n);
  package_params->locationg  = 
    (package_index *)
    alloc(package_params->locationg_size * sizeof(package_index));
}


/****************************************************************
 *			READ_PACKAGE_HEADER			*
 ****************************************************************
 * Read the package header. The header has the form		*
 *								*
 *    @(#)Astarte byte code version v0pname00			*
 *								*
 * if this is a one-file package whose package name is pname, 	*
 * and has the form						*
 *								*
 *    @(#)Astarte byte code version v0intname0impname0		*
 *								*
 * if this is a two-file package whose interface package has	*
 * name intname and whose implementation package has name	*
 * impname.							*
 *								*
 * The 0 above indicates a byte containing a null character,	*
 * '\0'.  The v indicates the byte-code version number.		*
 *								*
 * Check that the version number is correct.			*
 *								*
 * Install information into package_params to prepare for       *
 * reading the package.						*
 *								*
 * The return value is						*
 *   0   if all went well.					*
 *   1   if the file was recompiled, so the package reader      *
 *       should try again.					*
 *   2   if the header is bad.					*
 *--------------------------------------------------------------*/

PRIVATE int 
read_package_header(char *pkg_name, FILE *f, 
		    PACK_PARAMS *package_params,
		    Boolean complain, Boolean ok_to_compile)
{
  char interface_name[MAX_PACKAGE_NAME_LENGTH + 1];
  char impname[MAX_PACKAGE_NAME_LENGTH + 1];
  char header[40];
  int vers, cnt;

  /*--------------------------------------*
   * Read the initial part of the header. *
   *--------------------------------------*/

  fgets1(header, 39, f);  
  if(strncmp(header, "@(#)Astarte byte code version ", 30) != 0) {
    fclose(f);
    if(!complain) {
#     ifdef DEBUG
        if(trace) trace_i(349, nonnull(pkg_name));
#     endif
      return 2;
    }
    else {
      if(strncmp(header, "ACODE", 5) == 0) {
	cnt = 1;
	goto bad_version; /* below, next paragraph */
      }
      else {
	package_die(147, pkg_name);
      }
    }
  }

  cnt = sscanf(header+30, "%d", &vers);
  if(cnt != 1 || vers != BYTE_CODE_VERSION) {
    fclose(f);
  bad_version:
    if(cnt == 1 && ok_to_compile) {
      int ttc = 
	try_to_compile(pkg_name, 
		       "File %s has an incorrect version.\nCompile?[y|n] ");
      if(ttc == 1) return 1;
    }
    if(complain) package_die(157, pkg_name);     
    else return 2;
  }

  /*--------------------------------------------------------------*
   * Read the package name. If there are separate interface and   *
   * implementation packages, this is the interface package name. *
   *--------------------------------------------------------------*/

  fgets1(interface_name, MAX_PACKAGE_NAME_LENGTH, f);

  /*--------------------------------------------------------*
   * Read the implementation package name, if there is one. *
   *--------------------------------------------------------*/

  fgets1(impname, MAX_PACKAGE_NAME_LENGTH, f);

  /*-----------------------------------------------------------------*
   * Install the information about this package into package_params  *
   * and install package_params at the front of the chain	     *
   * current_package_params.					     *
   *-----------------------------------------------------------------*/

  package_params->parent     = current_pack_params;
  current_pack_params        = package_params;
  package_params->name       = make_perm_str(interface_name);
  package_params->imp_name   = impname[0] == 0 
                                  ? package_params->name 
				  : make_perm_str(impname);
  package_params->file_name  = pkg_name;
  package_params->packfile   = f;
  package_params->current    = 0;
  package_params->num        = num_packages++;
  package_params->imp_offset = -1;
  package_params->dfaults    = NULL;
  package_params->locationg_size = 
    (strcmp(interface_name, STANDARD_AST_NAME) == 0) 
    ? STD_LOCATIONG_SIZE : INIT_LOCATIONG_SIZE;
  
  return 0;
}


/********************************************************
 *		   FINISH_READING_PACKAGE		*
 ********************************************************
 * Handle a STOP_PACKAGE_I instruction.			*
 ********************************************************/

PRIVATE void
finish_reading_package(PACK_PARAMS *package_params)
{
  CODE_PTR end_addr;
  PACKAGE_DESCR *pd;

  package_params->packfile = NULL;

  /*-----------------------------------*
   * Record the package start and end. *
   *-----------------------------------*/
  
  pd = &(package_descr[package_params->num]);
  pd->begin_addr = package_params->start;
  pd->end_addr   = 
    end_addr     = package_params->start + package_params->current;
  pd->dfaults    = package_params->dfaults;  /* ref inherited */
  
  /*---------------------------------------------------*
   * Use, for small blocks, the unused part of		*
   * the array that was allocated to hold the code.	*
   *---------------------------------------------------*/

  give_to_small((char *) end_addr,
		package_params->size - package_params->current);

  /*-----------------------------------------------*
   * Truncate the lines array to its logical size. *
   *-----------------------------------------------*/

  {int log_size = pd->log_lines_size;
   int phys_size = pd->phys_lines_size;

   give_to_small((char *) (pd->lines + log_size),
		 phys_size - log_size);
   pd->phys_lines_size = log_size;
  }

  /*---------------------------------------------*
   * Use the locationg storage for small blocks. *
   *---------------------------------------------*/

  give_to_small((char *) package_params->locationg,
		package_params->locationg_size
		* sizeof(package_index));

  current_pack_params = NULL;

  if(verbose_mode) {
    fprintf(stderr, "Done reading file \"%s\"\n", 
	    package_params->file_name);
  }

# ifdef DEBUG
    if(trace) trace_i(351, nonnull(package_params->file_name));
# endif
}


/********************************************************
 *		   INSTALL_NEW_SPECIES			*
 ********************************************************
 * Process instruction instr, which must be one of	*
 * NEW_SPECIES_DCL_I, NEW_TRANSPARENT_FAMILY_DCL_I or   *
 * NEW_OPAQUE_FAMILY_DCL_I.				*
 *							*
 * Return true on success, false on failure.		*
 * If complain is false, suppress all complaints.	*
 ********************************************************/

PRIVATE Boolean
install_new_species(FILE *f, int instr, Boolean complain)
{
  int tok, n;
  TYPE *arg;
  char *name1, famname[MAX_NAME_LENGTH + 1];
  CLASS_TABLE_CELL *c1;

  /*--------------------------------------------------------*
   * Get the name and table entry of the species or family. *
   *--------------------------------------------------------*/

  if(instr == NEW_SPECIES_DCL_I) {
    tok = TYPE_ID_TOK;
    arg = NULL;
  }
  else {
    tok = FAM_ID_TOK;
    arg = any_type;
  }
  fgetstr1(famname, MAX_NAME_LENGTH, f);
  name1 = stat_str_tb(famname);
  c1    = get_ctc_tm(name1);
  
  /*--------------------------------------------------------*
   * If this type or family is already declared, it is an   *
   * error.  Otherwise, declare it.			     *
   *--------------------------------------------------------*/

  if(c1 != NULL) {
    if(complain) package_die(172, quick_display_name(name1));
    else return FALSE;
  }
  else n = ctc_num(add_tf_tm(name1, arg, 0, 
			     instr == NEW_OPAQUE_FAMILY_DCL_I,
			     0)->ctc);

  /*------------------------------------------------------*
   * Put the table index of this species or family in the *
   * package.						   *
   *------------------------------------------------------*/

  put_int_in_package(n);

  /*------------------------------------------------------------*
   * Wake up any packages that were waiting for this thing	*
   * to be defined.						*
   *------------------------------------------------------------*/

  return wake_up_deferred_packages(name1, complain);
}


/********************************************************
 *		   INSTALL_NEW_ABSTRACTION		*
 ********************************************************
 * Process instruction instr, which must be one of	*
 * NEW_GENUS_DCL_I, NEW_TRANSPARENT_COMMUNITY_DCL_I or  *
 * NEW_OPAQUE_COMMUNITY_DCL_I.				*
 *							*
 * If there are any deferred packages that can be	*
 * awakened because of this new abstraction, then	*
 * finish reading them.					*
 *							*
 * Return true on success, false on failure.		*
 ********************************************************/

PRIVATE Boolean
install_new_abstraction(FILE *f, int instr, Boolean complain)
{
  int tok, n;
  char *name1, gen_name[MAX_NAME_LENGTH + 1];
  CLASS_TABLE_CELL *c1;

  /*----------------------------------------------------------*
   * Get the name and table entry of this genus or community. *
   *----------------------------------------------------------*/

  tok = (instr == NEW_GENUS_DCL_I) ? GENUS_ID_TOK : COMM_ID_TOK;
  fgetstr1(gen_name, MAX_NAME_LENGTH, f);
  name1 = stat_str_tb(gen_name);
  c1 = get_ctc_tm(name1);

  /*-----------------------------------------------*
   * If this was not declared earlier, declare it. *
   * If it was declared earlier, it is an error.   *
   *-----------------------------------------------*/

  if(c1 == NULL) {
    c1 = add_class_tm(name1,tok,2,0,instr == NEW_OPAQUE_COMMUNITY_DCL_I);
    if(c1 == NULL) package_die(178, name1);
  }
  else {
    if(complain) package_die(185, quick_display_name(name1));
    else return FALSE;
  }

  /*-----------------------------------------------------*
   * Put the table index of this genus or community in   *
   * the package.					 *
   *-----------------------------------------------------*/

  n = c1->num;
  put_int_in_package(n);

  /*------------------------------------------------------------*
   * Wake up any packages that were waiting for this thing	*
   * to be defined.						*
   *------------------------------------------------------------*/

  return wake_up_deferred_packages(name1, complain);
}


/********************************************************
 *		   HANDLE_OLD_SPECIES			*
 ********************************************************
 * Process instruction instr, which must be one of	*
 * SPECIES_DCL_I or FAMILY_DCL_I.			*
 *							*
 * If the family or species that is being referred to   *
 * does not exist then it is possible that the species  *
 * will be declared in the interface part of another    *
 * package that was imported earlier, but that is not   *
 * completed yet.  (This can only happen due to a	*
 * cyclic import.)  In that case, this function		*
 * installs information into the current package param  *
 * frame and returns 1 to indicate the need to defer	*
 * the rest of this package.				*
 *							*
 * If from_deferral is true, then this is a redone	*
 * instruction.  Get the name from package_params->	*
 * deferred_name instead of from file f.		*
 *							*
 * Return 						*
 *    0    if there was an error;			*
 *    1    if the rest of this package should be 	*
 *	   deferred;					*
 *    2    on success.					*
 ********************************************************/

PRIVATE int
handle_old_species(FILE *f, int instr, PACK_PARAMS *package_params, 
		   Boolean complain, Boolean from_deferral)
{
  int tok;
  TYPE *arg;
  char *name, famname[MAX_NAME_LENGTH + 1];
  CLASS_TABLE_CELL *c1;

  /*--------------------------------------------------------*
   * Get the name and table entry of the species or family. *
   *--------------------------------------------------------*/
  
  if(instr == SPECIES_DCL_I) {
    tok = TYPE_ID_TOK;
    arg = NULL;
  }
  else {
    tok = FAM_ID_TOK;
    arg = any_type;
  }	   
  if(from_deferral) name = stat_str_tb(package_params->deferred_name);
  else {
    fgetstr1(famname, MAX_NAME_LENGTH, f);
    name  = stat_str_tb(famname);
  }
  c1    = get_ctc_tm(name);

  /*--------------------------------------------------------*
   * If this type or family is already declared, make sure  *
   * it is declared to be the same kind of thing as before. *
   * But do not do this for PAIR@ or FUNCTION@.		    *
   * (PAIR@ is used when declaring the intersection of two  *
   * genera to be PAIR@, and to place PAIR@ in a genus.)    *
   *--------------------------------------------------------*/

  if(c1 != NULL) {
    int c1_code = c1->code;
    if(c1_code != PAIR_CODE && c1_code != FUN_CODE) {
      if(c1_code != MAKE_CODE(tok)) {
	if(!complain) return 0;
	if(instr == SPECIES_DCL_I) {
	  package_die(57, quick_display_name(name));
	}
	else {
	  package_die(58, quick_display_name(name));
	}
      }
    }
  }

  /*--------------------------------*
   * Install this into the package. *
   *--------------------------------*/

  if(c1 == NULL) {
    package_params->deferred_instr = instr;
    package_params->deferred_name  = name;
    return 1;
  }
  else put_int_in_package(c1->num);
  return 2;
}


/********************************************************
 *		   HANDLE_OLD_ABSTRACTION		*
 ********************************************************
 * Process instruction instr, which must be one of	*
 * GENUS_DCL_I or COMMUNITY_DCL_I.			*
 *							*
 * Normally, the return value is TRUE.  There is a      *
 * special case, however, in which the return value	*
 * is FALSE.						* 
 *							*
 * If the abstraction that is being referred to does	*
 * not exist then it is possible that the abstraction	*
 * will be declared in the interface part of another    *
 * package that was imported earlier, but that is not   *
 * completed yet.  (This can only happen due to a	*
 * cyclic import.)  In that case, this function		*
 * installs information into the current package param  *
 * frame and returns 1 to indicate the need to defer	*
 * the rest of this package.				*
 *							*
 * If from_deferral is true, then this is a redone	*
 * instruction.  Get the name from package_params->	*
 * deferred_name instead of from file f.		*
 *							*
 * Return 						*
 *    0    if there was an error;			*
 *    1    if the rest of this package should be 	*
 *	   deferred;					*
 *    2    on success.					*
 ********************************************************/

PRIVATE int
handle_old_abstraction(FILE *f, int instr, PACK_PARAMS *package_params, 
		       Boolean complain, Boolean from_deferral)
{
  int tok;
  char *name, gen_name[MAX_NAME_LENGTH + 1];
  CLASS_TABLE_CELL *c1;

  /*------------------------------------------------------------*
   * Get the name and table entry of this genus or community.	*
   *------------------------------------------------------------*/

  tok = (instr == GENUS_DCL_I) ? GENUS_ID_TOK : COMM_ID_TOK;
  if(from_deferral) name = stat_str_tb(package_params->deferred_name);
  else {
    fgetstr1(gen_name, MAX_NAME_LENGTH, f);
    name  = stat_str_tb(gen_name);
  }
  c1 = get_ctc_tm(name);

  /*----------------------------------------------------*
   * If this was declared earlier, check that the two	*
   * declarations are consistent.			*
   *----------------------------------------------------*/

  if(c1 != NULL) {
    if(c1->code != MAKE_CODE(tok)) {
      if(!complain) return 0;
      if(instr == GENUS_DCL_I) {
	package_die(59, quick_display_name(name));
      }
      else {
	package_die(60, quick_display_name(name));
      }
    }
  }

  /*--------------------------------------*
   * Install the number into the package. *
   *--------------------------------------*/

  if(c1 == NULL) {
    package_params->deferred_instr = instr;
    package_params->deferred_name  = name;
    return 1;
  }
  else put_int_in_package(c1->num);
  return 2;
}


/********************************************************
 *		   PROCESS_DEFAULT_DCL			*
 ********************************************************
 * Read and process a DEFAULT_DCL_I instruction.	*
 *							*
 * If complain is false and something goes wrong, then  *
 * return false.  Normally, return true.		*
 ********************************************************/

PRIVATE Boolean
process_default_dcl(FILE *f, PACK_PARAMS *package_params, Boolean complain)
{
  CODE_PTR default_type_instrs;
  CLASS_TABLE_CELL *defaulting;
  TYPE *tt;
  int nn, closed;

  /*-------------------------------------------------------*
   * Handle an DEFAULT_DCL_I by storing this default 	   *
   * either into the dfaults list for the current package  *
   * or into the class table.				   *
   *							   *
   * This instruction is followed by			   *
   *							   *
   *   (1) The global label of the genus or community A to *
   *       which the default applies.		   	   *
   *							   *
   *   (2) 1 byte holding 1 if this default is done after  *
   *       the extension where A was created and 0 if it   *
   *       is done in the same extension where A was 	   *
   *       created.					   *
   *							   *
   *   (3) The default type, terminated by END_LET_I.	   *
   *-------------------------------------------------------*/

  /*------------------------------------*
   * Get the info from the instruction. *
   *------------------------------------*/

  nn     		= toint(fgetint(f));
  closed 		= fgetuc(f);
  defaulting    	= ctcs[toint(index_at_label(nn))];
  default_type_instrs 	= get_type_instrs(f, FALSE, complain);
  if(default_type_instrs == NULL) return FALSE;
  if(*default_type_instrs == END_LET_I) {
    tt = NULL;
  }
  else {
    clear_type_stk();
    eval_type_instrs(default_type_instrs, NULL);
    tt = pop_type_stk();    /* ref from type stack */
  }

  /*-----------------*
   * Make the entry. *
   *-----------------*/

  if(closed) {
    install_runtime_default_tm(defaulting, tt, package_params);
  }
  else SET_TYPE(defaulting->CTC_DEFAULT, tt);

  export_runtime_default(defaulting, tt, package_params);
  drop_type(tt);
  return TRUE;
}


/********************************************************
 *		   PROCESS_RELATE_DCL			*
 ********************************************************
 * Read and process a RELATE_DCL_I instruction.		*
 ********************************************************/

PRIVATE void
process_relate_dcl(FILE *f)
{
  /*----------------------------------------------------*
   * Declare a relationship between two genera, etc.    *
   * This instruction is followed by two global labels, *
   * indicating A, and B.  Declare A < B in the	        *
   * hierarchy.						*
   *----------------------------------------------------*/

  int nn = toint(fgetint(f));
  int mm = toint(fgetint(f));
  package_index n  = index_at_label(nn);
  package_index m  = index_at_label(mm);

  CLASS_TABLE_CELL* c1 = ctcs[toint(n)];
  CLASS_TABLE_CELL* c2 = ctcs[toint(m)];
  if(!ancestor_tm(c2, c1)) {
    copy_ancestors_tm(c1, c2, NOCHECK_LP);
  }
}


/********************************************************
 *		   PROCESS_MEET_DCL			*
 ********************************************************
 * Read and process a MEET_DCL_I instruction.		*
 ********************************************************/

PRIVATE void
process_meet_dcl(FILE *f)
{
  /*----------------------------------------------------*
   * Declare that the meet of A and B is C.		*
   *							*
   * This instruction is followed by three global	*
   * labels, indicating A, B and C, where 		*
   * A, B and C are genus, community, etc   		*
   * values, so what is stored is an index into ctcs.	*
   *----------------------------------------------------*/

  int nn = toint(fgetint(f));
  int mm = toint(fgetint(f));
  int kk = toint(fgetint(f));
  package_index n = index_at_label(nn);
  package_index m = index_at_label(mm);
  package_index k = index_at_label(kk);
  add_intersection_tm(ctcs[toint(n)], ctcs[toint(m)], ctcs[toint(k)], 
		      NOCHECK_LP);
}


/********************************************************
 *		   PROCESS_AHEAD_MEET_DCL		*
 ********************************************************
 * Read and process a AHEAD_MEET_DCL_I instruction.	*
 ********************************************************/

PRIVATE void
process_ahead_meet_dcl(FILE *f)
{
  /*----------------------------------------------------*
   * Declare that the meet of A and B is C, but make	*
   * this an ahead meet.				*
   *							*
   * This instruction is followed by three global	*
   * labels, indicating A, B and C, where		*
   * A, B and C are identifiers, so what is stored is	*
   * an index into array id_names.			*
   *----------------------------------------------------*/

  int nn = toint(index_at_label(toint(fgetint(f))));
  int mm = toint(index_at_label(toint(fgetint(f))));
  int kk = toint(index_at_label(toint(fgetint(f))));
  add_ahead_meet_tm(id_names[nn], id_names[mm], id_names[kk], 
		    (char*)(-1), NULL);
}


/********************************************************
 *		   PROCESS_IMPORT			*
 ********************************************************
 * Read and process an IMPORT_I instruction.		*
 *							*
 * Return TRUE on success, FALSE on failure.		*
 ********************************************************/

PRIVATE Boolean
process_import(FILE *f, PACK_PARAMS *package_params)
{
# ifdef SMALL_STACK
    char* import_name = (char *) BAREMALLOC(MAX_FILE_NAME_LENGTH + 1);
# else
    char import_name[MAX_FILE_NAME_LENGTH+1];
# endif

  /*----------------------------------------------------*
   * Get the name of the file to read. Replace $ by the *
   * standard directory.				*
   *----------------------------------------------------*/

  fgetstr1(import_name, MAX_FILE_NAME_LENGTH, f);
  install_standard_dir(import_name, MAX_FILE_NAME_LENGTH);
    
  /*---------------------------------------------------*
   * Read the file.  On failure, give back the storage *
   * that was allocated for this package, and return   *
   * FALSE.					       *
   *---------------------------------------------------*/

  if(read_package(import_name, 1, FALSE) != 1) {
    import_err = TRUE;
    give_to_small((char *) package_params->locationg,
		  package_params->locationg_size 
		  * sizeof(package_index));
    give_to_small((char *) package_params->start,
		  package_params->size);

#   ifdef SMALL_STACK
      FREE(import_name);
#   endif

#   ifdef DEBUG
      if(trace) {
        trace_i(352, nonnull(package_params->file_name), nonnull(import_name));
      }
#   endif

    return FALSE;
  }
  else {
#   ifdef DEBUG
      if(trace) {
        trace_i(353, nonnull(package_params->file_name));
      }
#   endif
  }

  /*-------------------------------------------------------*
   * Copy the defaults from the imported package to this   *
   * package.  If this package is in its export part,      *
   * also put them into this package's list of defaults    *
   * to export.						   *
   *-------------------------------------------------------*/

  {LIST *p;
   PACKAGE_DESCR* imp_pd = 
   get_pd_entry_by_file_name(import_name);

   if(imp_pd != NULL) {
     for(p = imp_pd->dfaults; p != NIL; p = p->tail->tail) {
       install_runtime_default_tm(ctcs[p->head.i], 
				  p->tail->head.type,
				  package_params);
       if(package_params->imp_offset < 0) {
	 export_runtime_default(ctcs[p->head.i], 
				p->tail->head.type,
				package_params);
       }
     }
   }
  }

  /*-------------------------------------------------------*
   * Restore current_pack_params to this package. The	   *
   * read_package call above modifies current_pack_params. *
   *-------------------------------------------------------*/

  current_pack_params = package_params;

# ifdef SMALL_STACK
    FREE(import_name);
# endif
  
  return TRUE;
}


/********************************************************
 *		   PROCESS_EXECUTE			*
 ********************************************************
 * Read and process instruction instr, which must be    *
 * one of  EXECUTE_I or HIDDEN_EXECUTE_I.		*
 *							*
 * Return true on success, false on failure.		*
 ********************************************************/

PRIVATE Boolean
process_execute(FILE *f, int instr, PACK_PARAMS *package_params,
		Boolean complain)
{
  /*------------------------------------------------------------*
   * Read the code for an execute dcl, and make an entry for	*
   * it in executes.  First reallocate executes if necessary.	*
   *------------------------------------------------------------*/

  if(num_executes >= executes_size) {
    int new_executes_size = executes_size + executes_size;
    executes = (struct execute *) 
      reallocate((char *) executes, 
		 executes_size*sizeof(struct execute), 
		 new_executes_size*sizeof(struct execute), TRUE);
    executes_size = new_executes_size;
  }
  executes[num_executes].hidden   = (instr == HIDDEN_EXECUTE_I);
  executes[num_executes].packnum  = package_params->num;
  executes[num_executes++].offset = package_params->current;

  /*-----------------------------------------------------*
   * The executable code begins with an environment size *
   * byte, indicating how many globals are needed.  	 *
   * Read that byte and put it in the package.  	 *
   * Then read the executable stuff.			 *
   *-----------------------------------------------------*/

  put_char_in_package_from_file(f);
  return read_to_end(f, complain);
}


/********************************************************
 *		   PROCESS_EXCEPTION_DCL		*
 ********************************************************
 * Read and process an EXCEPTION_DCL_I instruction.     *
 *							*
 * If complain is false and something goes wrong, then  *
 * return false.  Normally, return true.		*
 ********************************************************/

PRIVATE Boolean
process_exception_dcl(FILE *f, Boolean complain)
{
  HASH2_CELLPTR h;
  HASH_KEY u;
  LONG hash;
  int exc_num, mode;
  char exc_name[MAX_NAME_LENGTH+1];

  /*----------------------------------------------------*
   * Read an exception declaration.  It is followed by	*
   *							*
   *   (1) A byte that holds 0 if this exception is not	*
   *       trapped, and a 1 if this exception is trapped.*
   *							*
   *   (2) The null-terminated name of this exception.	*
   *       The name has the form pack:excname, where 	*
   *	   pack is the package where this exception is	*
   *	   declared.					*
   *							*
   *   (3) A three-byte integer giving the length of    *
   *       the description string for this package,     *
   *	   followed by the description string.	 	*
   *							*
   *   (4) Code that builds the type of object that is 	*
   *	   wrapped with the exception.			*
   *----------------------------------------------------*/

  /*--------------------*
   * Get the trap mode. *
   *--------------------*/
  
  mode = fgetuc(f);

  /*-------------------------*
   * Get the exception name. *
   *-------------------------*/

  fgetstr1(exc_name, MAX_NAME_LENGTH, f);

  /*-----------------------------------------------------------*
   * Install or look up this exception in the exception table. *
   *-----------------------------------------------------------*/

  hash  = strhash(exc_name);
  u.str = stat_str_tb1(exc_name,hash);
  h     = insert_loc_hash2(&exception_table, u, hash, eq);

  /*------------------------------------------------------------*
   * If not already there, install it. Read the description	*
   * and the type-building instructions.			*
   *------------------------------------------------------------*/

  if(h->key.num == 0) {
    LONG descr_len;
    h->key.str = u.str;
    if(next_exception > 255) {
      if(complain) package_die(61);
      else return FALSE;
    }
    h->val.num = exc_num = next_exception++;
    if(mode != 0) trap_by_default(exc_num);

    exception_data[exc_num].name = u.str;

    /*---------------------------------*
     * Read the exception description. *
     *---------------------------------*/

    descr_len = get_int_m(f);
    if(descr_len == 0) {
      exception_data[exc_num].descr = "";
    }
    else {
      exception_data[exc_num].descr = allocate_str(descr_len + 1);
      fgetstr1(exception_data[exc_num].descr, (int) descr_len, f);
    }
    
    /*-------------------------*
     * Read the argument type. *
     *-------------------------*/

    {CODE_PTR q = get_type_instrs(f,FALSE,complain);
     if(q == NULL) return FALSE;
     exception_data[exc_num].type_instrs = q;
    }
  }

  /*--------------------------------------------------------*
   * If this exception has already been declared, then just *
   * get its number from the table.  Read the type-building *
   * instructions and the description, but do nothing with  *
   * them.						     *
   *--------------------------------------------------------*/

  else {
    LONG descr_len;
    exc_num = toint(h->val.num);
    descr_len = get_int_m(f);
    if(descr_len != 0) fgetstr1_ignore(f);
    get_type_instrs(f, TRUE, complain);
  }

  /*-------------------------------------------------------------*
   * Write the table index of this exception into the package in *
   * place of the EXCEPTION_DCL_I instruction.			  *
   *-------------------------------------------------------------*/

  put_char_in_package(exc_num);
  return TRUE;
}


/********************************************************
 *		   PROCESS_IRREGULAR_DCL		*
 ********************************************************
 * Read and process an IRREGULAR_DCL_I instruction.     *
 *							*
 * If complain is false and something goes wrong, then  *
 * return false.  Normally, return true.		*
 ********************************************************/

PRIVATE Boolean
process_irregular_dcl(FILE *f, PACK_PARAMS *package_params, 
		      Boolean complain)
{
  CODE_PTR defining_instrs;
  TYPE *tt;
  int n;
  package_index defining;

  /*-------------------------------------------------------*
   * Handle an IRREGULAR_DCL_I by making an entry into the *
   * global id table indicating that this id is irregular. *
   * We must read here:					   *
   *   (1) The global label of the identifier.		   *
   *   (2) The type, terminated by END_LET_I.		   *
   *-------------------------------------------------------*/

  /*---------------------------------------------------*
   * Get the label n where the identifier is stored.   *
   *---------------------------------------------------*/

  n        = toint(fgetint(f));
  defining = index_at_label(n);

  /*--------------------------------------------------------*
   * Read the type-building instructions, and evaluate them *
   * to get the type.				       	     *
   *--------------------------------------------------------*/

  defining_instrs = get_type_instrs(f, FALSE, complain);
  if(defining_instrs == NULL) return FALSE;
  clear_type_stk();
  eval_type_instrs(defining_instrs, NULL);
  tt = pop_type_stk();    /* ref from type stack */

  /*---------------------------------------------*
   * Declare this identifier, of the given type. *
   *---------------------------------------------*/

  insert_global(defining, NULL, tt, package_params->num,
		0, UNDERRIDES_MODE_MASK | IRREGULAR_MODE_MASK, 1);
  drop_type(tt);
  return TRUE;
}


/********************************************************
 *		   PROCESS_DEFINE_DCL			*
 ********************************************************
 * Read and process a DEFINE_DCL_I instruction.		*
 *							*
 * If complain is false and something goes wrong, then  *
 * return false.  Normally, return true.		*
 ********************************************************/

PRIVATE Boolean
process_define_dcl(FILE *f, PACK_PARAMS *package_params, Boolean complain)
{
  CODE_PTR defining_instrs;

  /*------------------------------------------------------------*
   * Handle a define or let declaration.  This instruction 	*
   * is followed by						*
   *								*
   *   	(1) The global label of the identifier X being defined.	*
   *								*
   *   	(2) The mode of this declaration (one byte).		*
   *								*
   * 	(3) A sequence of type building instructions that 	*
   *	    leave the type  of X on the type stack.  		*
   *								*
   *    (4) Possibly an END_LET_I instruction, followed by a 	*
   * 	    sequence of type building instructions.  Can have 	*
   *	    as many of these as desired.  X will be defined	*
   *	    for each of these types.				*
   *								*
   *    (5) An ENTER_I instruction.				*
   *								*
   *	(6) A byte holding the index in env_size of the number  *
   *	    of globals to be defined in part (7).		*
   *								*
   *	(7) A sequence of global environment building 		*
   * 	    instructions.					*
   *								*
   *	(8) A STOP_G_I instruction.				*
   *								*
   *	(9) A byte holding the index in env_size of the number  *
   *	    of locals needed to evaluate the identifier.	*
   *								*
   *	(10) The code to evaluate the global identifier.	*
   *								*
   *	(11) An END_I instruction.				*
   *------------------------------------------------------------*/

  /*-----------------------------------------------------------*
   * Get the label n where identifier X is stored, the id      *
   * being defined, and the mode of the definition.	       *
   *-----------------------------------------------------------*/

  int           n        = toint(fgetint(f));
  int           mode     = fgetuc(f);
  package_index defining = index_at_label(n);

  /*----------------------------------------------------*
   * Read each of the types, and make each declaration. *
   *----------------------------------------------------*/
  
  do {
    /*--------------------------------------------------------*
     * Read the type-building instructions, and evaluate them *
     * to get the type.				       *
     *--------------------------------------------------------*/

    defining_instrs = get_type_instrs(f,FALSE,complain);
    if(defining_instrs == NULL) return FALSE;
    clear_type_stk();
    eval_type_instrs(defining_instrs, NULL);
	     
    /*---------------------------------------------*
     * Declare this identifier, of the given type. *
     *---------------------------------------------*/

    insert_global(defining, defining_instrs, 
		  top_type_stk(), package_params->num,
		  package_params->current, mode, 0);

    /*----------------------------------------------------------*
     * Clean up.  Go back and do another one if the previous	*
     * type ended on an END_LET_I instruction.			*
     *----------------------------------------------------------*/

    clear_type_stk();
  } while (last_instr == END_LET_I);

  /*------------------------------------------------------------*
   * Get the number of globals, and install it into the		*
   * run-time code.						*
   *------------------------------------------------------------*/
  
  put_char_in_package_from_file(f);
	   
  /*------------------------------------------------------------*
   * Read the executable part, and put it into the run-time	*
   * code.							*
   *------------------------------------------------------------*/

  return read_to_end(f, complain);
}


/********************************************************
 *		   PROCESS_PACKAGE_DECLARATIONS		*
 ********************************************************
 * Do the declarations in the package described by      *
 * package_params.  Install the code into its code	*
 * array.						*
 *							*
 * Return true on success, false on failure.		*
 *							*
 * If complain is false, suppress all complaints.	*
 ********************************************************/

PRIVATE Boolean 
process_package_declarations(PACK_PARAMS *package_params, Boolean complain)
{
  FILE* f = package_params->packfile;

  for(;;) {
    int instr = fgetuc(f);

#   ifdef DEBUG
      if(trace) trace_i(204, instr);
#   endif

    failure = -1;
    failure_as_entity = NOTHING;
    switch(instr) {
        case EOF:
        case STOP_PACKAGE_I:

	  /*--------------------------*
	   * The package is finished. *
	   *--------------------------*/

          fclose(f);
	  finish_reading_package(package_params);
	  return TRUE;

        case ID_DCL_I:

	  /*------------------------------------------------------------*
	   * Get the name that follows, create an entry in		*
	   * outer_bindings if necessary, and put this name's index	*
	   * in outer_bindings into the package. 			*
	   *------------------------------------------------------------*/

	  {char idname[MAX_NAME_LENGTH + 1];
	   package_index n;

	   fgetstr1(idname, MAX_NAME_LENGTH, f);
	   n = ent_str_tb(idname);
	   put_int_in_package(n);
	   break;
	  }

        case NAME_DCL_I:

	  /*------------------------------------------------------------*
	   * Get the name that follows, create an entry in the id_names	*
	   * array if necessary, and put this name's index		*
	   * in id_names into the package. 				*
	   *------------------------------------------------------------*/

	  {char idname[MAX_NAME_LENGTH + 1];
	   package_index n;

	   fgetstr1(idname, MAX_NAME_LENGTH, f);
	   n = name_tb(idname);
	   put_int_in_package(n);
	   break;
	  }

	case LABEL_DCL_I:

	  /*------------------------------------------*
	   * Make an entry in the global label table. *
	   *------------------------------------------*/

	  {int n = fgetuc(f);
	   possibly_realloc_locationg(current_pack_params, n);
	   package_params->locationg[n] = package_params->current;
	   break;
	  }

	case LONG_LABEL_DCL_I:

	  /*-----------------------------------------------------*
	   * Make an entry in the global label table.  Here, the *
	   * label is a long integer.				 *
	   *-----------------------------------------------------*/

	  {int n = toint(fgetint(f));
	   possibly_realloc_locationg(current_pack_params, n);
	   package_params->locationg[n] = package_params->current;
	   break;
	  }

        case BEGIN_IMPLEMENTATION_DCL_I:
          package_params->imp_offset = 
	    package_descr[package_params->num].imp_offset = 
	      package_params->current;
	  break;

	case STRING_DCL_I:

	  /*------------------------------------------------------------*
	   * Make an entry in the string table for this string, and	*
	   * store this string's index in that table where the		*
	   * STRING_DCL_I instruction was.				*
	   *------------------------------------------------------------*/

	  {char* word;
	   package_index n;

	   n = get_int_m(f);
	   word = (char*) BAREMALLOC(n+1);
	   fgetstr1(word, n, f);
	   n = string_tb(word);
	   put_int_in_package(n);
	   FREE(word);
	   break;
	  }

	case INT_DCL_I:

	  /*------------------------------------------------------------*
	   * Make an entry in the constant table, and store the index	*
	   * of this constant where the INT_DCL_I instruction was.	*
	   *------------------------------------------------------------*/

	  {char intname[MAX_INT_CONST_LENGTH + 1];
	   package_index n;

	   fgetstr1(intname, MAX_INT_CONST_LENGTH, f);
	   n = const_tb(intname, NAT_CON);
	   put_int_in_package(n);
	   break;
	  }
	   
	case REAL_DCL_I:

	  /*------------------------------------------------------------*
	   * Make an entry in the constant table, and store the index	*
	   * of this constant where the REAL_DCL_I instruction was.	*
	   *------------------------------------------------------------*/

	  {char realname[MAX_REAL_CONST_LENGTH + 1];
	   package_index n;

	   fgetstr1(realname, MAX_REAL_CONST_LENGTH, f);
	   n = const_tb(realname, RAT_CON);
	   put_int_in_package(n);
	   break;
	  }

	case NEW_SPECIES_DCL_I:
	case NEW_TRANSPARENT_FAMILY_DCL_I:
	case NEW_OPAQUE_FAMILY_DCL_I:
	  if(!install_new_species(f, instr, complain)) return FALSE;
	  break;

	case SPECIES_DCL_I:
	case FAMILY_DCL_I:
	  {int status = handle_old_species(f, instr, package_params, 
					   complain, FALSE);
	   if(status == 0) return FALSE;
	   else if(status == 1) {
	     defer_rest_of_package(package_params);
	     return TRUE;
	   }
	   break;
	  }

  	case NEW_GENUS_DCL_I:
  	case NEW_TRANSPARENT_COMMUNITY_DCL_I:
	case NEW_OPAQUE_COMMUNITY_DCL_I:
	  if(!install_new_abstraction(f, instr, complain)) return FALSE;
	  break;

	case GENUS_DCL_I:
	case COMMUNITY_DCL_I:
	  {int status = handle_old_abstraction(f, instr, package_params,
					       complain, FALSE);
	   if(status == 0) return FALSE;
	   else if(status == 1) {
	     defer_rest_of_package(package_params);
	     return TRUE;
	   }
	   break;
	  }

	case DEFAULT_DCL_I:
	  if(!process_default_dcl(f, package_params, complain)) return FALSE;
	  break;

        case RELATE_DCL_I:
	  process_relate_dcl(f);
	  break;

        case MEET_DCL_I:
	  process_meet_dcl(f);
	  break;

        case AHEAD_MEET_DCL_I:
	  process_ahead_meet_dcl(f);
	  break;

        case BEGIN_EXTENSION_DCL_I:
	  break;

        case END_EXTENSION_DCL_I:
	  break;

        case IMPORT_I:
	  if(!process_import(f, package_params)) return FALSE;
	  break;

        case EXECUTE_I:
        case HIDDEN_EXECUTE_I:
	  if(!process_execute(f, instr, package_params, complain)) {
	    return FALSE;
	  }
	  break;

        case EXCEPTION_DCL_I:
	  if(!process_exception_dcl(f, complain)) return FALSE;
	  break;

        case IRREGULAR_DCL_I:
	  if(!process_irregular_dcl(f, package_params, complain)) return FALSE;
	  break;

        case DEFINE_DCL_I:
	  if(!process_define_dcl(f, package_params, complain)) return FALSE;
	  break;

	default: 

	  /*----------------------------------------------------*
	   * If the declaration is none of the above, we have 	*
	   * big trouble.					*
	   *----------------------------------------------------*/

	  if(complain) package_die(62, (char *) instr);
	  else return FALSE;

    } /* end switch */
  } /* end for */
}


/********************************************************
 *			READ_PACKAGE			*
 ********************************************************
 * Try to read the package in file in_pkg_name. 	*
 * Possible behaviors are as follows.			*
 *							*
 *   (1) If unable to open this file then		*
 *         (a) if complain is -1 or 0, then return 0	*
 *	       silently;				*
 *	   (b) if complain is 1, then print a		*
 *	       complaint and abort the program.		*
 *							*
 *   (2) If able to open this file, but reading of the  *
 *	 package encounters an error, then		*
 *         (a) if complain is 0, then return -1		*
 *	       silently;				*
 *	   (b) if complain is nonzero, the complain and	*
 *             abort the program.			*
 *							*
 *   (3) If reading the file is successful, then	*
 *       return 1.					*
 *							*
 * Parameter is_main_pkg is true if this package is the *
 * main package.  When in_main_pkg is true, the name    *
 * of the package is stored in main_package_name.	*
 ********************************************************/

int read_package(char *in_pkg_name, int complain, Boolean is_main_pkg)
{
  Boolean ok_to_compile = TRUE;
  Boolean did_compile;
  FILE *f;
  LONG package_bytes = 0;
  char *pkg_name;
  PACK_PARAMS this_package_params;

  /*----------------------------------*
   * Give up if file name is unknown. *
   *----------------------------------*/

  if(in_pkg_name == NULL) return 0;

  /*------------------------------------------------------*
   * Modify the file name.  It needs to have suffix .aso. *
   *------------------------------------------------------*/

  {int pkg_name_len;
   pkg_name     = in_pkg_name;
   pkg_name_len = strlen(pkg_name);
   if(pkg_name_len > MAX_FILE_NAME_LENGTH - 4) {
     if(complain) package_die(145, pkg_name);
     else return 0;
   }
   pkg_name = aso_name(pkg_name);
  }

# ifdef DEBUG
    if(trace) trace_i(346, nonnull(pkg_name));
# endif

  /*----------------------------------------------------*
   * Check if this package has already been read. If so,*
   * indicate success, and do nothing.			*
   *----------------------------------------------------*/

  if(str_member(pkg_name, already_read_packages) != 0) {
#   ifdef DEBUG
      if(trace) trace_i(347, nonnull(pkg_name));
#   endif
    return 1;
  }

  /*------------------------------------------------------------*
   * Open the package for reading.  If this .aso file cannot    *
   * be opened, then try to compile it.  Return false, and 	*
   * possibly complain, if could not open.			*
   *------------------------------------------------------------*/

 try_again:
  f = open_package_for_reading(pkg_name, complain > 0, ok_to_compile,
			       &did_compile);
  if(f == NULL) {
    if(did_compile) {
      ok_to_compile = FALSE;
      goto try_again;
    }
    else if(complain == 1) die(146, pkg_name);
    else return 0;
  }

  /*------------------------------------------------------------*
   * Record this package as read.  This should be done now, to	*
   * suppress import loops.					*
   *------------------------------------------------------------*/

  SET_LIST(already_read_packages,
	   str_cons(make_perm_str(pkg_name), already_read_packages));

  /*------------------------------------*
   * Possibly reallocate package_descr. *
   *------------------------------------*/

  if(num_packages >= package_descr_size) {
    int new_size  = 2*package_descr_size;
    package_descr = reallocate((char *) package_descr,
			package_descr_size * sizeof(PACKAGE_DESCR),
			new_size * sizeof(PACKAGE_DESCR), TRUE);
    package_descr_size = new_size;
  }

# ifdef DEBUG
    if(trace || trace_env_descr) trace_i(203, pkg_name);
# endif

  /*------------------------------------------------------------*
   * Check if the .aso file is out of date.  If it is, ask the	*
   * user if we should compile it.  				*
   *								*
   * Also, get the size of the .aso file if we bother to get    *
   * its status, to avoid having to do it again later.		*
   *------------------------------------------------------------*/

  if(query_if_out_of_date && ok_to_compile) {
    if(check_if_out_of_date(pkg_name, &package_bytes, f)) {
      ok_to_compile = FALSE;
      goto try_again;
    }
  }

  /*--------------------------------------------------------------------*
   * Read the package header.  If the package has the wrong version	*
   * number, possibly recompile it.  read_package_header returns 1	*
   * if it recompiled.  In that case, start over.			*
   *									*
   * Read_package_header also installs information from the header into *
   * this_package_params.						*
   *--------------------------------------------------------------------*/
   
  {int rh = read_package_header(pkg_name, f, &this_package_params,
			        complain != 0, ok_to_compile);
   if(rh == 1) {
     ok_to_compile = FALSE;
     goto try_again;
   }
   else if(rh == 2) return -1;
   else if(is_main_pkg) main_package_name = this_package_params.name;
  }

  /*---------------------------------------------*
   * Allocate the array for storing the package. *
   *---------------------------------------------*/

  allocate_package_arrays(&this_package_params, &package_bytes);

  /*-----------------------------------------*
   * Set up the description of this package. *
   *-----------------------------------------*/

  set_up_package_description(&this_package_params, package_bytes);

  /*------------------------------------------*
   * Process the declarations in the package. *
   *------------------------------------------*/

# ifdef DEBUG
    if(trace) trace_i(350, nonnull(pkg_name));
# endif

  if(process_package_declarations(&this_package_params, complain != 0)) {
    return 1;
  }
  else return -1;
}


/****************************************************************
 *			YN_QUERY				*
 ****************************************************************
 * Query the user with query q, which can be a format string	*
 * with one %s format filled in by s.  Return TRUE on response	*
 * y, and FALSE on response n.					*
 *								*
 * At an end-of-file, return FALSE.				*
 ****************************************************************/

#ifdef UNIX

Boolean yn_query(char *q, char *s)
{
 int response;

 for(;;) {
    fprintf(stderr, q, s);
    do {
      response = getchar();
    } while (response != EOF && isspace(response));
    if(response == EOF || response == 'n') return FALSE;
    if(response == 'y') return TRUE;
    fprintf(stderr, "Please respond y or n\n");
    do {
      response = getchar();
    } while (response != EOF && response != '\n');
 }
}
#endif

/*-------------------------------------------------------------*/

#ifdef MSWIN
extern HWND MainWindowHandle;
Boolean yn_query(char *q, char *s)
{
  char msg[100], *themsg;
  int ans;
  if(strlen(q) + strlen(s) - 2 >= 100) themsg = q;
  else {
    sprintf(msg, q, s);
    themsg = msg;
  }
  ans = MessageBox(MainWindowHandle, themsg, "Astr", 
		   MB_YESNO | MB_ICONQUESTION);
  return ans == IDYES;
}
#endif


/********************************************************
 *			TRY_TO_COMPILE			*
 ********************************************************
 * Try to compile file file_name, if this action is     *
 * called for.  					*
 *							*
 * Return 						*
 *    0  if no compilation was attempted,		*
 *    1  if a successful compilation was done,		*
 *    2  if compilation was attempted, but it failed.	*
 *							*
 * file_name is a .aso name.  It needs to have the      *
 * .aso extension removed before compiling.		*
 * Only attempt to compile if the equivalent .ast file  *
 * exists.						*
 *							*
 * msg is a format that is used, with the modified      *
 * file name replacing %s, as the query to the user     *
 * when force_make is false.  If force_make is true,	*
 * then the compilation is done without a query.	*
 ********************************************************/

PRIVATE int try_to_compile(char *file_name, char *msg)
{
  /*----------------------------------------------------------*
   * If the .ast file does not exist, then just return false. *
   *----------------------------------------------------------*/

  struct stat stat_buf;
  char* aname = ast_name(file_name, TRUE);

  if(stat_file(aname, &stat_buf) != 0) return 0;

  /*----------------------------*
   * Otherwise, try to compile. *
   *----------------------------*/

  else {
    Boolean result;
    int   n    = strlen(file_name);
    char* name = (char*) BAREMALLOC(n+1);
    char* cmd  = (char*) BAREMALLOC(n + 20);
  
    get_root_name(name, file_name, n+1);
    if(!force_make && !yn_query(msg, name)) {
      result = 0;
    }
    else {
      int status;
      sprintf(cmd, "astc -l %s", name);
      fprintf(stderr, "Compiling %s ...", name);
      status = system(cmd);
      if(status != 0) {
	fprintf(stderr, "Compilation failed\n");
	result = 2;
      }
      else {
	fprintf(stderr, "done\n");
	result = 1;
      }
    }
    FREE(name);
    FREE(cmd);
    return result;
  }
}


/********************************************************
 *			CHECK_MOD_TIME			*
 ********************************************************
 * Let s' be the string obtained by replacing the last 	*
 * character of s by c.  If file s' is more recent than	*
 * aso_mod_time, then try to compile this file, asking  *
 * the user if appropriate.				*
 *							*
 * Return 						*
 *   0 if the file is not out of date			*
 *   1 if the file is out of date, but no attempt was	*
 *     made to compile.					*
 *   2 if compiled successfully				*
 *   3 if tried to compile, but was unsuccessful	*
 *							*
 * Special case: if query_if_out_of_date is false, then *
 * always do nothing and return 0.			*
 ********************************************************/

PRIVATE int 
check_mod_time(char *s, char c, time_t aso_mod_time)
{
  if(!query_if_out_of_date) return 0;
  else {
    time_t mod_time;
    struct stat stat_buf;
    int n = strlen(s);
    char* name = (char*) BAREMALLOC(n + 1);
  
    strcpy(name, s);
    name[n-1] = c;
    if(stat_file(name, &stat_buf) == 0) mod_time = stat_buf.st_mtime;
    else mod_time = 0;
    FREE(name);

    if(mod_time > aso_mod_time) {
#     ifdef UNIX
        int ttc = 
            try_to_compile(s, "File %s is out of date.\nCompile?[y|n] ");
	return ttc + 1;
#     else
        if(yn_query("Warning: file %s is out of date.\nContinue?[y|n] ",s)) {
          return 1;
        }
        else return 3;
#     endif
    }
  }
    
  return 0;
}


