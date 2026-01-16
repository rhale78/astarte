/**********************************************************************
 * File:    intrprtr/intrprtr.h
 * Purpose: Top level interpreter prototypes and definitions.
 * Author:  Karl Abrahamson
 **********************************************************************/


/*--------------------------------------------------------------*
 * INTERACT_WIN is the number of the interaction window, when	*
 * there is one.  TOT_N_WINS is one larger than the largest	*
 * window number.						*
 *--------------------------------------------------------------*/

#define INTERACT_WIN 6
#define TOT_N_WINS 7

/******************** From intvars.c ****************************/

extern char**		  os_environment;
extern char* 		  main_package_name;
extern char*		  main_program_name;
extern char*              main_program_file_name;
extern char FARR 	  ast_rts_msg[];
extern LONG               max_st_depth;
extern LONG		  max_heap_bytes;
extern ACTIVATION         the_act;
extern int                timed_out_kind;
extern union ctl_or_act   timed_out_comp;
extern STATE*             execute_state;
extern TRAP_VEC*          global_trap_vec;
extern  LIST*             runtime_shadow_st;
extern LONG		  precision;
extern int		  default_precision;
extern int		  default_digits_prec;
extern char*              stdin_redirect;
extern char*		  stdout_redirect;
extern char*		  stderr_redirect;
extern ACTIVATION         fail_act;
extern int                fail_instr;
extern ENTITY		  fail_ex;
extern Boolean            query_if_out_of_date;
extern Boolean		  force_stdout_redirect, force_stderr_redirect;
extern Boolean		  force_make;
extern Boolean		  verbose_mode;
extern Boolean            do_tro;
extern Boolean            store_fail_act;
extern Boolean            do_overlap_tests;
extern Boolean            in_visualizer;
extern Boolean            start_visualizer_at_trap;
extern Boolean            import_err;
extern LONG               num_threads;
extern LONG               num_thread_switches;
extern LONG               num_repauses;
extern Boolean            input_was_blocked;
extern int		  program_is_running;

/***************** From intrprtr.c *****************************/

int 	do_executes		(void);
void   	intrupt			(int signo);
void   	handle_interrupt	(void);
void    segviol			(int signo);
ENTITY 	set_stack_limit		(ENTITY e);
ENTITY 	get_stack_limit		(ENTITY e);
ENTITY 	set_heap_limit		(ENTITY e);
ENTITY 	get_heap_limit		(ENTITY e);
void   	clean_up_and_exit	(int val);

/*********************** For package.c *******************************/

struct execute {
  Boolean hidden;
  int packnum;
  LONG offset;
};

/*----------------------------------------------------------------------*
 * A pack_params structure stores information about a package while	*
 * the package is being read in.					*
 *----------------------------------------------------------------------*/

typedef struct pack_params {
  CODE_PTR start;	/* The start address of the package */

  package_index current;/* The index after start where the next byte
			   should be generated. (Same as number of bytes
			   currently in the package.) */

  char* name;           /* The name of this package.  If there are separate
			   interface and implementation packages, this is
			   the name of the interface package. */

  char* imp_name;	/* If there are separate interface and implemenation
			   packages, this is the name of the implementation
			   package.  Otherwise, this is NULL. */

  char* file_name;      /* File that this package is in.  (This is the
			   .aso file) */

  LONG imp_offset;      /* This is the offset of the
			   BEGIN_IMPLEMENTATION_I instruction. This
			   field holds -1 before BEGIN_IMPLEMENTATION_I
			   is read. */

  FILE* packfile;       /* File that is being read */

  long size;		/* The physical size of the block for this package */

  int size_index;	/* The index of size in array package_size */

  int num;		/* The number of this package */  

  long locationg_size;  /* The size of locationg */

  package_index* locationg; /* The offsets from start of the global labels */

  struct pack_params* parent; /* Parameters of package that is importing this
				 package. */

  LIST* dfaults;	/* This is a list of the form [n1,t1,n2,t2,...], 
			   where type ti is given as a default for ctcs[ni].  
			   Earlier members of the list have precedence. */

  int  deferred_instr;  /* The instruction that was deferred, when this
			   node is in deferred_packages.		*/

  char* deferred_name;  /* The name that goes along with deferred_instr. */
} PACK_PARAMS;
  

/*---------------------------------------------------------------*
 * A line_rec structure is used to store information about where *
 * a particular source line occurs in the code of a package.	 *
 *---------------------------------------------------------------*/

struct line_rec {
  package_index offset;  /* Offset from start of package of start of 
			    this line */

  int line;              /* This line number */
};


/*---------------------------------------------------------------*
 * A package_descr structure is used to store information about  *
 * a package after the package has been read in.  It is used for *
 * getting information about the package for debugging purposes. *
 *---------------------------------------------------------------*/

typedef struct package_descr {
  CODE_PTR  begin_addr;	   /* Start address */

  CODE_PTR  end_addr;	   /* One beyond last address in package */

  int       num;           /* Package number */

  char*     name;          /* Package name for this package.  If there are
			      separate interface and implementation 
			      packages, this is the name of the interface
			      package. */

  char*     imp_name;	   /* If there are separate interface and
			      implemenation packages, this is the name
			      of the implementation package.  Otherwise,
			      this is NULL. */

  char*     file_name;     /* File name for this package.  (This is the
			      .aso file name) */

  LONG      imp_offset;    /* If there are separate interface and
			      implementation packages, this is the offset
			      of the BEGIN_IMPLEMENTATION_I instruction. */

  struct line_rec* lines;  /* Array of line record entries, sorted by
			      package offset */

  int phys_lines_size;	   /* Size of array lines */

  int log_lines_size;      /* Number of used cells in lines */

  LIST* dfaults;	   /* As for struct pack_params. */
} PACKAGE_DESCR;


typedef struct type_backpatch {
  int                    instr;       /* deferred instruction */
  char*                  name;        /* type name */
  int			 package_num; /* number of package to be patched */
  package_index          patch_index; /* index to be patched */
  struct type_backpatch* next;        /* link in chain */
} TYPE_BACKPATCH;

typedef struct relate_chain_node {
  int package_num;
  int kind;
  package_index n,m,k;
  struct relate_chain_node *next;
} RELATE_CHAIN_NODE;


/*********************** From package.c *********************************/

#ifdef DEBUG
  int fgetuc(FILE *f);
#else
# define fgetuc(f) ((unsigned)(getc(f)))
#endif

extern struct execute*       executes;
extern int 		     num_executes;
extern struct env_descr**    env_descriptors;
extern CODE_PTR* 	     lazy_type_instrs; 
extern struct pack_params*   current_pack_params;
extern struct package_descr* package_descr;
extern STR_LIST*             already_read_packages;
extern int                   num_packages;
extern int                   next_env_descr_num;

void 		init_package_reader	(void);
package_index 	index_at_label		(int n);
package_index 	index_at_addr		(CODE_PTR *s);
ENTITY		load_package_stdf	(ENTITY s);
int		read_package		(char *s, int complain,
					 Boolean is_main_pkg);
Boolean 	yn_query		(char *q, char *s);
void 		package_die		(int n, ...);
Boolean 	end_read_package	(Boolean complain);

struct package_descr* get_pd_entry		  (CODE_PTR pc);
struct package_descr* get_pd_entry_by_file_name   (char *filename);
struct package_descr* get_pd_entry_by_package_name(char *pname);
