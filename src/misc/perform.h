/****************************************************************
 * File:    misc/perform.h
 * Purpose: Define initial table sizes, sizes of other structures
 *          and  other parameters whose values only affect performance.
 * Author:  Karl Abrahamson
 ****************************************************************/

/********************************************************************/
/********************************************************************/
/*              Constants that affect the compiler                  */
/********************************************************************/
/********************************************************************/


/*==================================================================*
 * STRING_COLLAPSE_LENGTH is the length of the longest string that  *
 * will be remembered by the compiler, so that two identical	    *
 * strings will only be generated once.				    *
 ********************************************************************/

#define STRING_COLLAPSE_LENGTH 50


/*==================================================================*
 * MAX_STRING_LEN is the initial maximum length of a string 	    *
 * constant (space allocated).					    *
 * More space will be allocated if needed.                          *
 ********************************************************************/

#define MAX_STRING_LEN	2000	


/*==================================================================*
 * INIT_CTCS_SIZE is the initial number of types, families, genera  *
 * and communities (total) permitted, before reallocation.	    *
 ********************************************************************/

#define INIT_CTCS_SIZE 512	


/*==================================================================*
 * INIT_VCTCS_SIZE is the initial number of genera and communities  *
 * before reallocation.						    *
 ********************************************************************/

#define INIT_VCTCS_SIZE 64


/*======================================================================*
 * AWAIT_SIZE_CUTOFF is the maximum number of nodes in the body		*
 * of an await expression such that the await expression body will	*
 * be double compiled, once for lazy mode and once for eager mode.	*
 * Double compiling improves speed, but increases code size. 		*
 * Expressions larger than AWAIT_SIZE_CUTOFF as await bodies will	*
 * always be lazy.  							*
 ************************************************************************/

#define AWAIT_SIZE_CUTOFF  100


/*==================================================================*
 * MAX_NUM_SCOPES is the initial size of the array that holds	    *
 * information on scopes in expressions. It is the number of	    *
 * distinct scopes in a declaration before reallocation is needed.  *
 ********************************************************************/

#define MAX_NUM_SCOPES	32


/*==================================================================*
 * GLOB_CODE_ARRAY_SIZE is the initial size (in bytes) of the array *
 * that is used to hold preamble code for declarations.		    *
 * EXEC_CODE_ARRAY_SIZE is the initial size (in bytes) of the array *
 * that is used to hold the executable code for declarations.       *
 ********************************************************************/

#define GLOB_CODE_ARRAY_SIZE  2000 
#define EXEC_CODE_ARRAY_SIZE  5000



/********************************************************************/
/********************************************************************/
/*              Constants that affect the interpreter               */
/********************************************************************/
/********************************************************************/


/************************************************************************
 * SUMMARY_LINES_PER_PAGE is the number of lines per page that will be  *
 * shown by the debugger on a page.					*
 ************************************************************************/

#define SUMMARY_LINES_PER_PAGE 30

/************************************************************************
 * TYPE_STORE_SIZE is the initial size of the array that holds 		*
 * temporary types during constructions of types.  (This array is	*
 * reallocated when necessary.)						*
 ************************************************************************/

#define TYPE_STORE_SIZE       100

/************************************************************************
 * TYPE_STACK_SIZE is the initial size of the stack that is used during *
 * type construction.  (This array is reallocated when necessary.)	*
 ************************************************************************/

#define TYPE_STACK_SIZE       100


/*==================================================================*
 * INPUT_BUFFER_SIZE is the number of characters read at once when  *
 * reading a file.  It should ideally be two less than a power of   *
 * 2, and should not be larger than BINARY_BLOCK_SIZE - 2.	    *
 ********************************************************************/

#define INPUT_BUFFER_SIZE	62

/*==================================================================*
 * INIT_EXECUTES_SIZE is the initial size of the array that holds   *
 * execute declarations before they are processed. 		    *
 ********************************************************************/

#define INIT_EXECUTES_SIZE 32


/*==================================================================*
 * INIT_ENV_DESCR_SIZE is the initial size of the array of 	    *
 * environment descriptors.  This is how many different kinds of    *
 * local environment are supported before reallocation.  There is   *
 * an environment kind for each scope that has local ids. 	    *
 ********************************************************************/

#define INIT_ENV_DESCR_SIZE 512


/*======================================================================*
 * MAX_TYPE_CODE_SIZE is the initial number of bytes allocated to arrays*
 * that hold type building instructions.                                *
 ************************************************************************/

#define MAX_TYPE_CODE_SIZE 512


/*==================================================================*
 * INIT_LAZY_TYPE_INSTRS_SIZE is the initial size of the array of   *
 * type instruction pointers for lazy instructions.  This is how    *
 * many different functions and lazy expressions are supported	    *
 * before reallocation.				 		    *
 ********************************************************************/

#define INIT_LAZY_TYPE_INSTRS_SIZE 512


/*==================================================================*
 * OUTER_BINDINGS_SIZE_INIT is the initial size of the array that   *
 * stores entity id bindings in the outer scope.  This is the       *
 * maximum number of entity ids a program can have before this 	    *
 * array is reallocated.			           	    *
 ********************************************************************/

#define OUTER_BINDINGS_SIZE_INIT 1024


/*==================================================================*
 * CONSTANTS_SIZE_INIT is the initial size of the array that stores *
 * constants.  The array will be reallocated if more than this      *
 * many constants occur in a program.		                    *
 ********************************************************************/

#define CONSTANTS_SIZE_INIT	256


/*==================================================================*
 * NUM_STACK_CELLS is the number of cells allocated at once for the *
 * data stack of the interpreter. If you don't understand this,     *
 * don't change it.						    *
 ********************************************************************/

#define NUM_STACK_CELLS 7


/*======================================================================*
 * QLAZY_COUNT_INIT is approximately half the number of times in a row  *
 * that an await will not be lazy before choosing to be lazy. If you    *
 * make this larger, you might also want to make RTS_ENTS_INIT_SIZE     *
 * larger.  We want RTS_ENTS_INIT_SIZE > 10*QLAZY_COUNT_INIT to prevent *
 * reallocation of garbage collector registration table at deep awaits. *
 ************************************************************************/

#define QLAZY_COUNT_INIT     9


/*======================================================================*
 * INIT_LOCATIONG_SIZE is the initial estimate of the number of global  *
 * labels that a package contains.  STD_LOCATIONG_SIZE should be an     *
 * upper bound on the number of global labels in standard.aso.  (If it	*
 * isn't, only performance is affected, and not very much.)		*
 ************************************************************************/

#define INIT_LOCATIONG_SIZE 50
#define STD_LOCATIONG_SIZE  256


/*======================================================================*
 * INIT_MAX_NUM_PACKAGES is the initial size of an array that holds     *
 * an entry for each package.  The array is reallocated if necessary.   *
 ************************************************************************/

#define INIT_MAX_NUM_PACKAGES   30


/*======================================================================*
 * When there are several threads executing, the interpreter tries to   *
 * do nonblocking input, so that one thread will not block when another *
 * tries to read from an empty pipe.  That can, unfortunately, lead to  *
 * busy-waiting.  When all threads are blocked, and some thread is 	*
 * waiting for a blocked file, the interpreter will sleep for		*
 * SLEEP_SECONDS seconds before trying again.				*
 ************************************************************************/

#define SLEEP_SECONDS 1


/*======================================================================*
 * NOTE: The following time quantities are just initial values of       *
 * counters that control local timing.  They do not give global         *
 * control over timing.                                                 *
 ************************************************************************/

/*======================================================================*
 * SLICE_TIME is the normal slice time when doing multiprocessing.      *
 * It is measured in instructions, not in any real time unit.		*
 ************************************************************************/

#define SLICE_TIME              600


/*======================================================================*
 * The internal merge function is used when doing system calls.  It     *
 * merges two lazy lists.					        *
 *									*
 * MERGE_MAX_TIME is the most time used in internal merge function	*
 * before switching to another list.  MERGE_MIN_TIME is the smallest	*
 * such time, and should be positive to keep merge from squeezing the	*
 * time down too much when many merges are nested. 			*
 *									*
 * Time is measured in instructions, not in real time units.		*
 ************************************************************************/

#define MERGE_MAX_TIME		400
#define MERGE_MIN_TIME		100


/*======================================================================*
 * TREE_EVAL_TIME determines the time used for evaluating the leftmost	*
 * child of a tree that represents a list.  This keeps recursion from	*
 * going too deep along left-paths of trees, since the trees are	*
 * rebalanced upon return from a time-out. 				*
 ************************************************************************/

#define TREE_EVAL_TIME		200


/*======================================================================*
 * LIST_PRIM_TIME is the time for evaluation of list primitives such    *
 * as ## that would like to be lazy, but can be more efficient if done  *
 * in an eager way and then timed-out if they run too long.             *
 ************************************************************************/

#define LIST_PRIM_TIME 		200


/*======================================================================*
 * SHORT_APPLY_TIME is the time for function shortApply.		*
 ************************************************************************/

#define SHORT_APPLY_TIME	400


/*======================================================================*
 * MAX_UNKNOWN_PATH_COMPRESS tells how much of a path of consecutive	*
 * unknowns is compressed.  This controls the size of an array, and	*
 * should not be excessively large. 					*
 ************************************************************************/

#define MAX_UNKNOWN_PATH_COMPRESS 20


/*======================================================================*
 * HANDLE_PENDING_INTERVAL is the number of instructions that the	*
 * interpreter executes between tests to see if a message from		*
 * the user is waiting, or if another process wants a chance.  It	*
 * is only used for MSWIN implementation. 				*
 ************************************************************************/

#ifdef MSWIN
# define HANDLE_PENDING_INTERVAL 10
#endif


/***********************************************************************/
/***********************************************************************/
/*   Constants that affect garbage collection and storage allocation   */
/***********************************************************************/
/***********************************************************************/


/*=====================================================================*
 * The memory allocator maintains a collection of blocks that are used *
 * for getting small chunks of memory.				       *
 *								       *
 * ALLOC_SMALL_MAX is the maximum number of bytes that will be         *
 * taken from the small blocks at once.                                *
 ***********************************************************************/

#define ALLOC_SMALL_MAX		64


/*=====================================================================*
 * The garbage collector maintains some tables of variables that are   *
 * held by C functions.						       *
 *								       *
 * RTS_ENTS_INIT_SIZE is the initial size if the registration table    *
 * for the garbage collector.  RTS_PENTS_INIT_SIZE is the initial      *
 * size of the pointer registration table for the garbage collector.   *
 ***********************************************************************/

#define RTS_ENTS_INIT_SIZE 	100
#define RTS_PENTS_INIT_SIZE	60


/*=========================================================================*
 * FILE_ENT_BLOCK_SIZE is the number of file entities in one allocated     *
 * block.  It should be fairly small.					   *
 ***************************************************************************/

#define FILE_ENT_BLOCK_SIZE     5


/*=========================================================================*
 * Doubles (real numbers) are stored in blocks holding 			   *
 * SMALL_REAL_BLOCK_SIZE real numbers each.  The block also has a long	   *
 * integer for keeping track of the memory.				   *
 *									   *
 * SMALL_REAL_BLOCK_SIZE must be less than LONG_BITS, since a single   	   *
 * long integer is used to hold mark bits for all of the numbers in a	   *
 * block.								   *
 *									   *
 * SMALL_REAL_BLOCK_SIZE*DBL_BYTES + 2*LONG_BYTES must be less than or     *
 * equal to the alignment size for small real blocks, DBL_BLOCK_ALIGN,     *
 * defined in the machine-dependent files.				   *
 *									   *
 * Don't change this without changing marking of doubles in gc/gc.c and	   *
 * looking at machData/smgc.h and machData/lggc.h.			   *
 ***************************************************************************/

#define SMALL_REAL_BLOCK_SIZE  (LONG_BITS - ((2*LONG_BYTES)/DBL_BYTES))


/*=========================================================================*
 * GC_BLOCK_SIZE is the size, in bytes, of a block allocated by the garbage*
 * collector for its own use, less one word used to link blocks together.  *
 ***************************************************************************/

#define GC_BLOCK_SIZE (1024 - PTR_BYTES)         


/*=========================================================================*
 * The following constants indicate how many of each kind of object should *
 * be allocated initially before allocation of the same kind of object	   *
 * triggers a garbage collection.  Later garbage collections wait until	   *
 * at least this many, and possibly more, have been allocated.		   *
 ***************************************************************************/


# define SMALL_REAL_GET_NUM  SMALL_REAL_BLOCK_SIZE

# define ENT_GET_NUM	     ENT_BLOCK_SIZE

# define BINARY_GET_NUM	     BINARY_BLOCK_SIZE

# define FILE_GET_NUM        FILE_ENT_BLOCK_SIZE


/*=========================================================================*/

/************************************************************************
 * The following constants have two definitions, depending on whether   *
 * the garbage collector is in normal mode or in test mode.  The first  *
 * list of values, with comments, is for normal mode.  After that, the	*
 * values for test mode are given.					*
 ************************************************************************/

#ifndef GCT

/*=========================================================================*
 * GET_BEFORE_GC_INIT is the number of bytes to allocate before  	   *
 * first garbage collection.						   *
 ***************************************************************************/

# define GET_BEFORE_GC_INIT  65536L


/*=========================================================================*
 * MEM_BLOCK_SIZE is the number of bytes in a memory block.  This is the   *
 * number of bytes allocated at once by the basic allocator in allocate.c, *
 * and should be at least as large as other block sizes.  It should be	   *
 * a multiple of 2*LONG_BYTES.				         	   *
 ***************************************************************************/

# define MEM_BLOCK_SIZE      16384


/*=========================================================================*
 * Some blocks of memory are allocated for chopping up into small pieces.  *
 * SMALL_BLOCK_ALLOC_SIZE is the number of bytes allocated to such a	   *
 * block.								   *
 ***************************************************************************/

# define SMALL_BLOCK_ALLOC_SIZE 4096 


/*=========================================================================*
 * ENT_BLOCK_SIZE is the number of entities allocated at once.  If small   *
 * entities are used, then entities are one LONG each, and there is one	   *
 * additional LONG in an entity block, so there are 			   *
 * LONG_BYTES*(ENT_BLOCK_SIZE+1) bytes in an entity block.	   	   *
 *									   *
 * We require that an entity block be no larger than a binary block.  So   *
 * we compute ENT_BLOCK_SIZE from BINARY_BLOCK_SIZE.			   *
 ***************************************************************************/

# define ENT_BLOCK_SIZE       (BINARY_BLOCK_SIZE/ENTITY_BYTES)


/*=========================================================================*
 * ENT_BLOCK_GRAB_SIZE is the maximum number of entities actually	   *
 * taken at once from a block when alloating arrays.  Arrays cannot be	   *
 * larger than this. (If longer, an array will be made into a two level    *
 * array.  ENT_BLOCK_GRAB_SIZE should not be too large a fraction          *
 * of ENT_BLOCK_SIZE, since otherwise a great deal of a block might be     *
 * wasted.								   *
 ***************************************************************************/

# define ENT_BLOCK_GRAB_SIZE  (ENT_BLOCK_SIZE >> 1)


/*=========================================================================*
 * BINARY_BLOCK_SIZE is the number of bytes allocated in a binary data	   *
 * block.  								   *
 ***************************************************************************/

# define BINARY_BLOCK_SIZE   (MEM_BLOCK_SIZE - (2*PTR_BYTES))


/*=========================================================================*
 * BINARY_BLOCK_GRAB_SIZE is the maximum number of bytes allocated at once *
 * in a single binary block.  It must be less than 0x3ffc.		   *
 ***************************************************************************/

# define BINARY_BLOCK_GRAB_SIZE   ((BINARY_BLOCK_SIZE - SHORT_BYTES)/2)


#else


/* The same constants as above, but for test operation. */

# define GET_BEFORE_GC_INIT  	1024   
# define MEM_BLOCK_SIZE	     	4096   
# define SMALL_BLOCK_ALLOC_SIZE 4096
# define ENT_BLOCK_SIZE      	255   
# define ENT_BLOCK_GRAB_SIZE 	128   
# define BINARY_BLOCK_SIZE	(1024 - 2*PTR_BYTES)
# define BINARY_BLOCK_GRAB_SIZE 128

#endif





