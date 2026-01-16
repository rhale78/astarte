/*************************************************************************
 * File:    rts/rts.h
 * Purpose: Run time support
 * Author:  Karl Abrahamson
 ************************************************************************/

/*********************** bigint.c *******************************/

/************************************************************************
 * An INTCELL_ARRAY consists of an array of intcells, along with a      *
 * logical size indicating how many entries are in use.			*
 ************************************************************************/

typedef struct {
  intcellptr buff;
  LONG size;
} INTCELL_ARRAY;

/************************************************************************
 * A BIGINT_BUFFER is used to store an INTCELL_ARRAY along with a       *
 * pointer to the binary chunk that is being used.                      *
 ************************************************************************/

 typedef struct {
   INTCELL_ARRAY val;
   CHUNKPTR chunk;
 } BIGINT_BUFFER;

extern INTCELL_ARRAY one_array;

void 	get_bigint_buffer  	(LONG n, BIGINT_BUFFER *bf);
void 	get_temp_bigint_buffer  (LONG n, BIGINT_BUFFER *bf);
void 	free_bigint_buffer 	(BIGINT_BUFFER *buff);
void    free_if_diff	   	(ENTITY x, int nchks, ...);

ENTITY  array_to_int          (int tag, BIGINT_BUFFER *bf);
ENTITY  normalize_array_to_int(BIGINT_BUFFER *aa, int a_tag,
			       LONG prec, LONG *ex);
void    install_int	      (LONG val, intcellptr buf);
void 	const_int_to_array    (ENTITY x, BIGINT_BUFFER *r, int *tag,
			       intcellptr small_buff);
void    int_to_array          (ENTITY a, BIGINT_BUFFER *r, int *tag,
			       LONG shft, LONG size);

int     compare_array      (const INTCELL_ARRAY *a, const INTCELL_ARRAY *b);
void    not_array	   (const INTCELL_ARRAY *a, const INTCELL_ARRAY *b,
			    intcell pad);
void    and_array	   (const INTCELL_ARRAY *a, const INTCELL_ARRAY *b, 
			    const INTCELL_ARRAY *c, intcell pad);
void    xor_array	   (const INTCELL_ARRAY *a, const INTCELL_ARRAY *b, 
			    const INTCELL_ARRAY *c, intcell pad);
void    or_array	   (const INTCELL_ARRAY *a, const INTCELL_ARRAY *b, 
			    const INTCELL_ARRAY *c, intcell pad);

void    add_array          (const INTCELL_ARRAY *a, const INTCELL_ARRAY *b, 
			    const INTCELL_ARRAY *c);
void    subtract_array     (const INTCELL_ARRAY *a, const INTCELL_ARRAY *b, 
			    const INTCELL_ARRAY *c);
void    left_shift         (const INTCELL_ARRAY *a, int k);
void    right_shift        (const INTCELL_ARRAY *a, int k);
void    shift_left_digits  (const INTCELL_ARRAY *a, LONG s);
void    shift_right_digits (const INTCELL_ARRAY *a, LONG s);
void    mult_by_digit	   (const INTCELL_ARRAY *a, intcell d);
void    mult_array         (const INTCELL_ARRAY *a, const INTCELL_ARRAY *b,
			    intcellptr c);
ENTITY  shift_left_blocks  (ENTITY a, LONG k);
void	divide_by_digit    (const INTCELL_ARRAY *a, LONG d,
			    const INTCELL_ARRAY *b, LONG *r);
void    divide_large_int   (ENTITY a, ENTITY b, BIGINT_BUFFER *q,
			    ENTITY *r);
ENTITY  big_gcd            (ENTITY a, ENTITY b);


/**************************** From compare.c ****************************/

int     ast_compare_simple(ENTITY a, ENTITY b);
ENTITY  ast_compare	  (ENTITY a, ENTITY b);
ENTITY  ast_equal	  (ENTITY a, ENTITY b, LONG *time_bound);
ENTITY  ast_lt		  (ENTITY a, ENTITY b);
ENTITY  ast_le		  (ENTITY a, ENTITY b);
ENTITY  ast_gt		  (ENTITY a, ENTITY b);
ENTITY  ast_ge		  (ENTITY a, ENTITY b);
ENTITY  ast_ne		  (ENTITY a, ENTITY b);


/************************** From integer.c *****************************/

ENTITY  ast_make_int	(LONG n);
LONG    get_ival        (ENTITY a, int ex);
LONG    gcd		(LONG a, LONG b);


/************************* From number.c *********************************/

ENTITY compat_stdf	(ENTITY pr, TYPE* t);
ENTITY pred_stdf	(ENTITY x);
ENTITY ast_add		(ENTITY a, ENTITY b);
ENTITY ast_subtract	(ENTITY a, ENTITY b);
ENTITY ast_absdiff	(ENTITY a, ENTITY b);
ENTITY ast_negate	(ENTITY a);
ENTITY ast_mult		(ENTITY a, ENTITY b);
ENTITY ast_divide	(ENTITY a, ENTITY b);
ENTITY ast_reciprocal	(ENTITY a);
void   ast_divide_int	(ENTITY a, ENTITY b, ENTITY *q, ENTITY *r);
ENTITY ast_div		(ENTITY a, ENTITY b);
ENTITY ast_mod		(ENTITY a, ENTITY b);
ENTITY ast_divide_by_2  (ENTITY a);
ENTITY ast_gcd		(ENTITY a, ENTITY b);
ENTITY ast_zerop	(ENTITY x);
int    ast_sign		(ENTITY a);
ENTITY ast_odd		(ENTITY a);
ENTITY ast_sign_as_ent	(ENTITY a);
ENTITY ast_abs		(ENTITY a);
ENTITY ast_power_pos	(ENTITY x, ENTITY p);
ENTITY ast_power	(ENTITY x, ENTITY p);

/************************ From numconv.c ***************************/

ENTITY ast_as			(ENTITY a, ENTITY x, TYPE *ty);
ENTITY ast_general_num		(ENTITY a, TYPE *ty);
ENTITY real_to_rat		(ENTITY a);
ENTITY rat_to_real		(ENTITY a);
ENTITY rat_to_int		(ENTITY a);
ENTITY int_to_rat		(ENTITY a);
ENTITY int_to_large_real        (ENTITY a);
ENTITY int_to_real		(ENTITY a);
ENTITY ast_floor		(ENTITY a);
ENTITY ast_ceiling		(ENTITY a);
ENTITY ast_natural		(ENTITY a);
ENTITY ast_integer		(ENTITY a);
ENTITY ast_real			(ENTITY a);
ENTITY ast_rational		(ENTITY a);
ENTITY wrap_number		(ENTITY x, TYPE *t);


/************************* From prtnum.c ***************************/

charptr ast_num_to_str    (ENTITY a, ENTITY b);
ENTITY  ast_num_to_strng  (ENTITY a);
ENTITY  ast_num_to_string (ENTITY a, ENTITY b);
charptr ast_int_to_hex_str(ENTITY a);
ENTITY  ast_int_to_hex_string(ENTITY a);
ENTITY  ast_str_to_int    (char *s);
ENTITY  ast_hex_to_nat    (ENTITY s);
ENTITY	ast_string_to_nat (ENTITY s);
ENTITY	ast_string_to_int (ENTITY s);
ENTITY  ast_str_to_rat	  (char *s, Boolean rat_result);
ENTITY  ast_string_to_rat (ENTITY s);
ENTITY  ast_string_to_real(ENTITY s);


/*********************** From append.c ********************************/

ENTITY  quick_append			(ENTITY a, ENTITY b);
ENTITY  tree_append			(ENTITY a, ENTITY b);
void    split_append			(ENTITY l, ENTITY *h, ENTITY *t, 
					 ENTITY rr, int mode);
void    split_tree			(ENTITY l, ENTITY *h, ENTITY *t, 
					int mode);
Boolean eval_tree			(ENTITY *loc, ENTITY e, 
					 LONG *time_bound, Boolean weak);


/*********************** From tostring.c *******************************/

void    init_object_names   (void);
ENTITY  bool_to_string      (ENTITY a);
ENTITY  hermit_to_string    (ENTITY a);
ENTITY  exception_to_string (ENTITY a);
ENTITY  comparison_to_string(ENTITY a);
ENTITY  boxflavor_to_string (ENTITY a);
ENTITY  copyflavor_to_string(ENTITY a);
ENTITY  fileMode_to_string  (ENTITY a);
ENTITY  species_dollar_stdf (ENTITY a);

/*********************** From array.c *******************************/

void    split_string    (ENTITY l, ENTITY *h, ENTITY *t, 
			 ENTITY rest, int mode);
void    split_cstring   (ENTITY l, ENTITY *h, ENTITY *t, int mode);
void    split_array	(ENTITY l, ENTITY *h, ENTITY *t, 
			 ENTITY rest, int mode);
ENTITY  array_length	(ENTITY l, LONG *offset);
ENTITY  array_sublist	(ENTITY arr, LONG i, LONG n, LONG *time_bound,
		     	Boolean subscript);
ENTITY  ast_array       (ENTITY a);
ENTITY  ast_place_array (ENTITY a);
ENTITY  ast_wrapped_array(ENTITY n, TYPE *t);
ENTITY  ast_place_array1(LONG n);
ENTITY  ast_pack_stdf   (ENTITY l);
ENTITY  ast_pack 	(ENTITY l, ENTITY len, LONG *l_time);
char*   copy_str	(char *s, ENTITY a, SIZE_T n, ENTITY *tl);
char*   copy_str1	(char *s, ENTITY aa, SIZE_T n, ENTITY *tl, 
			 LONG* chars_copied, Boolean term);
ENTITY  scan_for_help   (ENTITY s, ENTITY l, ENTITY offset, ENTITY sense,
			 ENTITY leadin, LONG *l_time);
ENTITY  scan_for_stdf   (ENTITY s, ENTITY l_info);

/*********************** From product.c *******************************/

#define ARRAY_MIN_BLOCK		4

#define SET_REBALANCE_LIST(x,y,time,weak)\
  x = MEMBER(TAG(y),all_lazy_tags)? rebalance_list(y,time,weak) : y

#define SET_WEAK_REBALANCE_LIST(x,y,time)\
  x = MEMBER(TAG(y),all_weak_lazy_tags)? rebalance_list(y,time,TRUE) : y

#define IN_PLACE_REBALANCE_LIST(x,time,weak)\
  if(MEMBER(TAG(x),all_lazy_tags)) x = rebalance_list(x,time,weak)

#define IN_PLACE_WEAK_REBALANCE_LIST(x,time)\
  if(MEMBER(TAG(x),all_weak_lazy_tags)) x = rebalance_list(x,time,TRUE)

#define ast_split(l,h,t) ast_split1(l,h,t,3)

ENTITY  ast_idf		(ENTITY x);
ENTITY  lazy_left_stdf	(ENTITY x);
ENTITY  lazy_right_stdf	(ENTITY x);
ENTITY  lazy_head_stdf	(ENTITY x);
ENTITY  lazy_tail_stdf	(ENTITY x);
void	ast_split1	(ENTITY l, ENTITY *h, ENTITY *t, int mode);
ENTITY	ast_head	(ENTITY l);
ENTITY	ast_tail	(ENTITY l);
ENTITY	ast_pair	(ENTITY h, ENTITY t);
ENTITY	ast_triple	(ENTITY a, ENTITY b, ENTITY c);
ENTITY  ast_quad	(ENTITY a, ENTITY b, ENTITY c, ENTITY d);
void   	multi_pair_pr	(int k);
ENTITY	ast_length_stdf	(ENTITY l);
ENTITY	ast_length	(ENTITY l, LONG offset, LONG *time_bound);
LONG    index_val       (ENTITY x);
ENTITY	ast_subscript	(ENTITY l, ENTITY n);
ENTITY  ast_subscript1  (ENTITY l, ENTITY nn, LONG *l_time);
ENTITY  ast_sublist1    (ENTITY l, LONG i, LONG n,
			 LONG *time_bound, Boolean force, Boolean subscript);
ENTITY  ast_sublist     (ENTITY l, ENTITY ij);
ENTITY  ast_upto	(ENTITY a, ENTITY b);
ENTITY  ast_downto	(ENTITY a, ENTITY b);
ENTITY  list_to_entlist (STR_LIST *l);
ENTITY  reverse_stdf	(ENTITY x);
ENTITY  ast_reverse	(ENTITY a, ENTITY b, LONG *time_bound);



/*********************** From io.c *************************************/

#ifdef MSWIN
# include "../rts/keyboard.h"
#endif

extern struct filed file_table[];

FILE*   file_from_entity(ENTITY f);
ENTITY	ast_print_list	(FILE *f, ENTITY e, LONG *l_time);
ENTITY  ast_print_str	(FILE *f, ENTITY e, LONG *l_time, LONG max_chars,
			 LONG *chars_printed);
ENTITY  ast_fprint      (ENTITY f, ENTITY e, LONG *l_time);
void    flush_stdout	(void);
ENTITY  flush_file_stdf (ENTITY);
void 	close_all_open_files(void);
ENTITY  ast_fclose      (ENTITY f);
ENTITY  ast_fopen       (ENTITY s, int mode);
ENTITY  ast_fopen_stdf  (ENTITY s, ENTITY modes);
ENTITY  ast_read_file   (int fd, int mode, char *name);
ENTITY  ast_input_stdf  (ENTITY s, ENTITY modes);
ENTITY  ast_input	(ENTITY s, int mode);
ENTITY  file_eval       (ENTITY *f, Boolean *is_infile, LONG *l_time);
ENTITY 	input_eval	(ENTITY *e, int kind, int mode, LONG *l_time);
Boolean file_sublist	(ENTITY *inlist, ENTITY **loc, LONG *skip,
			 LONG *time_bound);
ENTITY  outfile_dollar_stdf(ENTITY e);
void    init_io         (void);

/*********************** From fonts.c ********************************/

#ifdef MSWIN
extern struct fontd font_table[];

ENTITY open_font_stdf	(ENTITY entFilename, ENTITY entFontname,
			 ENTITY entPointHeight);
ENTITY close_font_stdf  (ENTITY f);
void   close_all_open_fonts(void);
#endif


/*********************** From rational.c *****************************/

ENTITY make_rat		(ENTITY a, ENTITY b);
ENTITY ast_make_rat	(ENTITY a, ENTITY b);
ENTITY ast_make_rat1	(ENTITY a);
ENTITY add_rat		(ENTITY a, ENTITY b);
ENTITY subtract_rat	(ENTITY a, ENTITY b);
ENTITY mult_rat		(ENTITY a, ENTITY b);
ENTITY divide_rat	(ENTITY a, ENTITY b);



/*************************** From real.c *****************************/

#ifdef BITS16
# define MAX_PREC 114000L
#else
# define MAX_PREC 480000
#endif

/* Do not reorder the following.  They are used in real.c to indicate *
 * the kind of operation to perform.				      */

#define ADD      0
#define SUBTRACT 1
#define MULTIPLY 2
#define DIVIDE	 3

extern ENTITY ln_base_val;

ENTITY ast_make_real		(DOUBLE x);
void   double_to_floating       (DOUBLE x, ENTITY *m, ENTITY *ex);
ENTITY force_large_real         (ENTITY a, ENTITY expon);
ENTITY round_real		(ENTITY a, LONG prec) ;
ENTITY pull_apart_real		(ENTITY x);
ENTITY op_real              	(ENTITY a, ENTITY b, int op);
ENTITY floor_large_real		(ENTITY a);
ENTITY sqrt_real		(ENTITY a);
void   init_real		(void);
ENTITY exp_real    		(ENTITY x);
ENTITY ln_real     		(ENTITY x);
ENTITY sin_real			(ENTITY x);
ENTITY atan_real		(ENTITY x);

/************************ From bitvec.c ***************************/

ENTITY bv_and		(ENTITY a, ENTITY b);
ENTITY bv_xor		(ENTITY a, ENTITY b);
ENTITY bv_or		(ENTITY a, ENTITY b);
ENTITY bv_mask		(ENTITY nn);
ENTITY bv_shl		(ENTITY a, ENTITY b);
ENTITY bv_shr		(ENTITY a, ENTITY b);
ENTITY bv_min		(ENTITY a);
ENTITY bv_max		(ENTITY a);
ENTITY bv_set_bits	(ENTITY a, ENTITY info);
ENTITY bv_field		(ENTITY v, ENTITY info);


/*********************** From tstrutil.c *********************************/


int     new_temp_str		(LONG n);
int     make_temp_str		(ENTITY s, char **buff);
int     cons_temp_str		(char ch, int k);
int     multicons_temp_str	(char *s, int k);
int     cat_temp_str		(int i, int k);
charptr get_temp_str		(int k);
charptr grab_temp_str		(int k);
charptr temp_str_buffer		(int k);
void    remove_leading   	(int k, char c);
void    remove_trailing   	(int k, char c);
void    init_temp_str_utils	(void);


/*********************** From system.c *********************************/

ENTITY chdir_stdf	(ENTITY s);
ENTITY getenv_stdf	(ENTITY s);
ENTITY do_cmd_stdf      (ENTITY cmd, ENTITY s);
ENTITY do_cmd		(ENTITY cmd, ENTITY s, LONG *l_time);
ENTITY cmdman		(ENTITY pr, ENTITY s, LONG *l_time);
ENTITY date_stdf	(ENTITY herm);
ENTITY seconds_stdf	(ENTITY herm);
ENTITY fstat_stdf	(ENTITY name_as_ent);
ENTITY rm_stdf		(ENTITY s);
ENTITY rmdir_stdf	(ENTITY s);
ENTITY mkdir_stdf	(ENTITY s);
ENTITY rename_stdf	(ENTITY from, ENTITY to);
ENTITY dirlist_stdf	(ENTITY d);
ENTITY getcwd_stdf	(ENTITY herm);
ENTITY os_env_stdf	(ENTITY herm);
