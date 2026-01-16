/**********************************************************************
 * File:    alloc/allocate.h
 * Purpose: Exports from directory alloc
 * Author:  Karl Abrahamson
 **********************************************************************/

#ifdef COPYRIGHT
/************************************************************************
 * Copyright (c) 2000 Karl Abrahamson					*
 * All rights reserved.							*
 * See file COPYRIGHT.txt for details.					*
 ************************************************************************/
#endif

/********************* From allocate.c ***********************/

#ifdef USE_SBRK
  extern charptr heap_base;
#endif

extern LONG 	heap_bytes;
extern Boolean 	have_allocated;

void*  alloc		(SIZE_T n);
void*  bd_alloc         (SIZE_T n, LONG mask);
void*  reallocate	(charptr p, SIZE_T m, SIZE_T n, Boolean reuse);
void*  alloc_small      (int n);
void*  bd_alloc_small	(int n, LONG mask);
void   init_alloc	(void);
void   give_to_small    (charptr p, SIZE_T m);
void*  bare_malloc      (SIZE_T n);
void*  bare_realloc     (void* p, SIZE_T n);

#ifdef USE_MALLOC
  void free_all_blocks  (void);
#endif


/********************* From allocors.c **************************/

char*  			allocate_str	(SIZE_T n);

CLASS_UNION_CELL*	allocate_cuc    (void);
void			free_cuc	(CLASS_UNION_CELL *c);
void			free_cucs_fun	(CLASS_UNION_CELL *c);

CLASS_TABLE_CELL* 	allocate_ctc	(void);
void		 	free_ctc	(CLASS_TABLE_CELL *c);
void 			drop_ctc_parts	(CLASS_TABLE_CELL *ctc);

HASH1_TABLE* 		allocate_hash1	(int n);
void			free_hash1	(HASH1_TABLE *t);
HASH2_TABLE* 		allocate_hash2	(int n);
void			free_hash2	(HASH2_TABLE *t);
void			init_hash_alloc (void);

#ifdef TRANSLATOR
GLOBAL_ID_CELL*   	allocate_gic            (void);
void		 	free_gic	        (GLOBAL_ID_CELL *g);
EXPECTATION* 		allocate_expectation	(void);
void			free_expectation	(EXPECTATION *exp);
EXPAND_INFO*		allocate_expand_info	(void);
PATFUN_EXPECTATION* 	allocate_patfun_expectation(void);
PART*      	 	allocate_part        	(void);
struct report_record*	allocate_report_record  (void);
void 			free_report_record      (struct report_record *t);
DEFERRED_DCL_TYPE* 	allocate_deferred_dcl   (void);
void 			free_deferred_dcl       (DEFERRED_DCL_TYPE *t);
IMPORT_STACK*		allocate_import_stack   (void);
void 			free_import_stack	(IMPORT_STACK *p);
ROLE* 			allocate_role		(void);
void 			bump_role		(ROLE *r);
void 			drop_role		(ROLE *r);
void 			set_role		(ROLE **x, ROLE *r);
void 			bump_rtype		(RTYPE r);
void 			drop_rtype		(RTYPE r);
ROLE_CHAIN* 		allocate_role_chain	(void);
DESCRIP_CHAIN* 		allocate_descrip_chain	(void);
void			free_descrip_chain	(DESCRIP_CHAIN*);

MODE_TYPE* 		allocate_mode		(void);
void 			bump_mode		(MODE_TYPE *r);
void 			drop_mode		(MODE_TYPE *r);
void 			set_mode		(MODE_TYPE **x, MODE_TYPE *r);

CHOOSE_INFO* 		allocate_choose_info	(void);
void 			free_choose_info	(CHOOSE_INFO *r);

#define allocate_entpart 	allocate_part
#define allocate_expand_part 	allocate_part
#endif

#ifdef MACHINE
GLOBAL_TABLE_NODE* 	allocate_global_table	(void);
CODE_PTR		allocate_package	(LONG n);
#endif


/********************* From aloclist.c **************************/

#ifndef SET_LIST
# define SET_LIST(x,t)       set_list(&(x), t) 
# define FRESH_SET_LIST(x,t) bump_list(x = t)
# define bmp_list(l)	     (l)->ref_cnt++
#endif

struct list*	allocate_list	(void);
void		set_list	(struct list **x, struct list *t);
void 		bump_list	(struct list *l);
void 		drop_list	(struct list *l);
void		free_list_node  (struct list *l);


/******************** From aloctype.c *************************/

#ifndef SET_TYPE
# define SET_TYPE(x,t)       set_type(&(x), t) 
# define FRESH_SET_TYPE(x,t) bump_type(x = t)
# define bmp_type(t)	     (t)->ref_cnt++
#endif

TYPE* allocate_type	(void);
void free_type		(TYPE *t);
void set_type		(TYPE **x, TYPE *t);
void bump_type		(TYPE *t);
void drop_type		(TYPE *t);


/******************** From alocexpr.c ***************************/


#ifndef SET_EXPR
# define SET_EXPR(x,t)       set_expr(&(x), t) 
# define FRESH_SET_EXPR(x,t) bump_expr(x = t)
# define bmp_expr(e)	     (e)->ref_cnt++	
#endif

EXPR* allocate_expr	(void);
void free_expr		(EXPR *e);
void set_expr		(EXPR **x, EXPR *t);
void bump_expr		(EXPR *x);
void drop_expr		(EXPR *x);


/********************* From gcalloc.c **************************/

/***************************************************************************
 * When holes are created in entity blocks, those holes are classified     *
 * according to size.  FREE_ENTS_SIZE is the number of entity subblock 	   *
 * size classifications.  It should be ceiling(log_2(ENT_BLOCK_SIZE)) 	   *
 ***************************************************************************/

#if (16 < ENT_BLOCK_SIZE && ENT_BLOCK_SIZE <= 32) 
# define FREE_ENTS_SIZE 5
#elif (ENT_BLOCK_SIZE <= 64) 
# define FREE_ENTS_SIZE 6
#elif (ENT_BLOCK_SIZE <= 128)
# define FREE_ENTS_SIZE 7
#elif (ENT_BLOCK_SIZE <= 256)
# define FREE_ENTS_SIZE 8
#elif (ENT_BLOCK_SIZE <= 512)
# define FREE_ENTS_SIZE 9
#elif (ENT_BLOCK_SIZE <= 1024)
# define FREE_ENTS_SIZE 10
#elif (ENT_BLOCK_SIZE <= 2048)
# define FREE_ENTS_SIZE 11
#elif (ENT_BLOCK_SIZE <= 4096)
# define FREE_ENTS_SIZE 12
#elif (ENT_BLOCK_SIZE <= 8192)
# define FREE_ENTS_SIZE 13
#elif (ENT_BLOCK_SIZE <= 16384)
# define FREE_ENTS_SIZE 14
#elif (ENT_BLOCK_SIZE <= 32768)
# define FREE_ENTS_SIZE 15
#elif (ENT_BLOCK_SIZE <= 65536)
# define FREE_ENTS_SIZE 16
#endif

/***************************************************************************
 * When holes are created in binary blocks, those holes are classified     *
 * according to size.  FREE_BINARY_SIZE is the number of binary chunk 	   *
 * size classifications.  It should be					   *
 *  (1) ceiling(log_2(BINARY_BLOCK_SIZE) - 2) 				   *
 *         when PTR_BYTES = 4						   *
 *									   *
 *  (2) ceiling(log_2(BINARY_BLOCK_SIZE) - 3)				   *
 *         when PTR_BYTES = 8						   *
 ***************************************************************************/

#if (16 < BINARY_BLOCK_SIZE && BINARY_BLOCK_SIZE <= 32) 
# define FREE_BINARY_SIZE32 3
#elif (BINARY_BLOCK_SIZE <= 64) 
# define FREE_BINARY_SIZE32 4
#elif (BINARY_BLOCK_SIZE <= 128)
# define FREE_BINARY_SIZE32 5
#elif (BINARY_BLOCK_SIZE <= 256)
# define FREE_BINARY_SIZE32 6
#elif (BINARY_BLOCK_SIZE <= 512)
# define FREE_BINARY_SIZE32 7
#elif (BINARY_BLOCK_SIZE <= 1024)
# define FREE_BINARY_SIZE32 8
#elif (BINARY_BLOCK_SIZE <= 2048)
# define FREE_BINARY_SIZE32 9
#elif (BINARY_BLOCK_SIZE <= 4096)
# define FREE_BINARY_SIZE32 10
#elif (BINARY_BLOCK_SIZE <= 8192)
# define FREE_BINARY_SIZE32 11
#elif (BINARY_BLOCK_SIZE <= 16384)
# define FREE_BINARY_SIZE32 12
#elif (BINARY_BLOCK_SIZE <= 32768)
# define FREE_BINARY_SIZE32 13
#elif (BINARY_BLOCK_SIZE <= 65536)
# define FREE_BINARY_SIZE32 14
#endif

#if(PTR_BYTES == 4) 
# define FREE_BINARY_SIZE FREE_BINARY_SIZE32
#else
# define FREE_BINARY_SIZE (FREE_BINARY_SIZE32 - 1)
#endif

void			gc_init_alloc		(void);
void 			check_heap_size		(void);
BINARY_BLOCK* 		get_avail_block		(void);

extern SMALL_REAL* 	 free_small_reals;
extern SMALL_REAL_BLOCK* small_real_blocks;
extern LONG 		 small_reals_since_last_gc;

#define allocate_large_real() ((LARGE_REAL*) allocate_entity(2))
SMALL_REAL*		 allocate_small_real	(void);

extern ENT_BLOCK* 	  used_blocks;
extern FREE_ENT_DATA FARR free_ent_data[];
extern LONG               ents_since_last_gc;

void 			 set_wheres		(void);
ENTITY* 		 allocate_entity	(LONG n);

extern BINARY_BLOCK* 	used_binary_blocks;
extern BINARY_BLOCK* 	avail_blocks;
extern LONG	        binary_bytes_since_last_gc;
extern FREE_BINARY_DATA FARR free_binary_data[];
extern CHUNKPTR*	huge_binaries_allocated;
extern int		huge_binaries_allocated_size;

#define allocate_bigint_array(n) allocate_binary((n) << 1)
LONG 			binary_chunk_size	(CHUNKPTR chunk);
charptr 		binary_chunk_buff	(CHUNKPTR chunk);
void 			set_binary_wheres	(void);
CHUNKPTR		allocate_binary		(LONG n);
void 			free_huge_binary	(CHUNKPTR p);
void 			free_chunk		(CHUNKPTR c);

#ifdef DEBUG
void 			print_entity_wheres	(void);
void			print_binary_wheres	(void);
void 			print_block_chain	(BINARY_BLOCK *chain);
#endif

extern struct file_entity*    free_file_entities;
extern struct file_ent_block* file_ent_blocks;
extern LONG   		      files_since_last_gc;

struct file_entity*     alloc_file_entity       (void);
void                    free_file_entity	(struct file_entity *f);


/******************* From mrcalloc.c *********************************/

extern STACK* free_stacks;
extern char*  control_allocator;

STACK* 		allocate_stack		(void);
STATE* 		allocate_state		(int kind);
CONTINUATION* 	allocate_continuation	(void);
CONTROL* 	allocate_control	(void);
ACTIVATION* 	allocate_activation	(void);
ENVIRONMENT* 	allocate_env		(int n);
ENVIRONMENT* 	allocate_local_env	(int n);
TRAP_VEC* 	allocate_trap_vec	(void);

void bump_stack				(STACK *s);
void bump_state				(STATE *s);
void bump_control			(CONTROL *s);
void bump_ctl_or_act			(union ctl_or_act p, int kind);
void bump_continuation			(CONTINUATION *a);
void bump_activation			(ACTIVATION *a);
void bump_activation_parts		(ACTIVATION *a);
void bump_activation_parts_except_stack	(ACTIVATION *a);
void bump_activation_parts_except_ccs	(ACTIVATION *a);
void bump_trap_vec			(TRAP_VEC *t);
void bump_env				(ENVIRONMENT *e, int ne);
void bump_env_ref			(ENVIRONMENT *env, int ne);

/*-------------------------------------------------------------------------*
 * Only use the following if you are sure that the bumped ptr is not null. *
 *-------------------------------------------------------------------------*/

#define bmp_stack(s)        (s)->ref_cnt++
#define bmp_state(s)        (s)->ref_cnt++
#define bmp_control(s)      (s)->ref_cnt++
#define bmp_continuation(c) (c)->ref_cnt++
#define bmp_activation(a)   (a)->ref_cnt++

/*----------------------------------------------------------------------*
 * The following is an inline version of bump_ctl_or_act.  It can be 	*
 * used on null poiters.						*
 *----------------------------------------------------------------------*/

#define bmp_ctl_or_act(s,k) {if((k) == CTL_F) bump_control((s).ctl); else bump_activation((s).act);}

void drop_control			(CONTROL *s);
void drop_ctl_or_act			(union ctl_or_act p, int kind);
void drop_stack				(STACK *s);
void drop_state				(STATE *s);
void drop_continuation			(CONTINUATION *a);
void drop_activation_parts 		(ACTIVATION *a);
void drop_activation_parts_except_ccs	(ACTIVATION *a);
void drop_activation			(ACTIVATION *a);
void drop_env				(ENVIRONMENT *env, int ne);
void drop_env_ref			(ENVIRONMENT *env, int ne);
void drop_trap_vec			(TRAP_VEC *tr);

#define SET_CONTROL(x,y)  set_control(&(x), (y))
#define SET_STATE(x,y)    set_state(&(x), (y))
#define SET_TRAP_VEC(x,y) set_trap_vec(&(x), (y));

void set_control			(CONTROL **s, CONTROL *v);
void set_state				(STATE **x, STATE *t);
void set_trap_vec			(TRAP_VEC **x, TRAP_VEC *tv);

void free_continuation			(CONTINUATION *a);
void free_activation 			(ACTIVATION *a);

void badrc				(char *where, int rc, char *p);


/******************* From tempstr.c *********************************/

void  init_temp_strs   (void);
void  alloc_temp_buffer(LONG n, int *ii, int *jj, charptr *buff, 
			LONG *out_size);
void  free_temp_buffer (int i, int j);
