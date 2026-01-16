/**********************************************************************
 * File:    utils/hash.h
 * Purpose: Describe implementation of hash tables
 * Author:  Karl Abrahamson
 **********************************************************************/

/****************************************************************
 *			HASH SIZE				*
 ****************************************************************
 * hash_size is an array holding HASH_SIZE_SIZE successively    *
 * larger sizes for hash tables.  See hash2.c for defn.		*
 ****************************************************************/

#define HASH_SIZE_SIZE 21

extern SIZE_T FARR hash_size[];


/****************************************************************
 *			INTHASH					*
 ****************************************************************
 * Use inthash(n) as a hash value for a long integer n.		*
 ****************************************************************/

#define inthash(n) (n)


/****************************************************************
 *			LOAD FACTOR				*
 ****************************************************************
 * The load factor of unary tables is LFA1/LFB1, and the load	*
 * factor of binary tables is LFA2/LFB2.  It must be the case	*
 * that LFA1 < LFB1 and LFA2 < LFB2.				*
 ****************************************************************/

#define LFA1 3
#define LFB1 4
#define LFA2 3
#define LFB2 4


/****************************************************************
 *			SECOND_HASH				*
 ****************************************************************
 * SECOND_HASH(h,size) is a second hash value obtained from h.  *
 * It must be positive, and less than size.			*
 ****************************************************************/

#define SECOND_HASH(h, size) (toSizet((((h) / (size)) % ((size) - 1)) + 1))


/****************************************************************
 *			FUNCTIONS				*
 ****************************************************************/

LONG 		combine3	 (int k, LONG a, LONG b);

LONG		strhash		 (CONST char *k);
LONG        	lphash		 (LPAIR t);

Boolean		eq		 (HASH_KEY s, HASH_KEY t);
Boolean		equalstr	 (HASH_KEY s, HASH_KEY t);

HASH1_TABLE* 	create_hash1	 (int n);
HASH1_TABLE*    copy_hash1	 (HASH1_TABLE* t);
HASH1_CELLPTR  	locate_hash1     (HASH1_TABLE *t, HASH_KEY key, LONG hv, 
			          Boolean (*eqf)(HASH_KEY,HASH_KEY));
HASH1_CELLPTR 	insert_loc_hash1 (HASH1_TABLE **t, HASH_KEY key, LONG hv, 
			          Boolean (*eqf)(HASH_KEY,HASH_KEY));
void            insert_str_hash1 (HASH1_TABLE **tbl, CONST char *s);
Boolean		str_mem_hash1    (HASH1_TABLE *tbl, CONST char *s);
LIST* 		key_list	 (HASH2_TABLE *tbl);
LONG		num_bindings_hash1(HASH1_TABLE *t);
void 		scan_hash1	 (HASH1_TABLE *t, void (*f)(HASH1_CELLPTR ));
void 		delete_str_hash1 (HASH1_TABLE *tbl, CONST char *s);

HASH2_TABLE* 	create_hash2	 (int n);
HASH2_TABLE* 	copy_hash2	 (HASH2_TABLE* t);
HASH2_CELLPTR  	locate_hash2	 (HASH2_TABLE *t, HASH_KEY key, LONG hv, 
			      	  Boolean (*eqf)(HASH_KEY,HASH_KEY));
HASH2_CELLPTR 	insert_loc_hash2 (HASH2_TABLE **t, HASH_KEY key, LONG hv, 
			          Boolean (*eqf)(HASH_KEY,HASH_KEY));
void		clear_data_hash2 (HASH2_TABLE *t, HASH2_VAL d);
LONG		num_bindings_hash2(HASH2_TABLE *t);
void 		scan_hash2	 (HASH2_TABLE *t, void (*f)(HASH2_CELLPTR ));
void 		scan_and_clear_hash2(HASH2_TABLE **tbl, 
				     void (*f)(HASH2_CELLPTR));


void		sort_int_hash2         (HASH2_TABLE *t);
void		sort_profile_val_hash2 (HASH2_TABLE *t);
void		sort_int_val_hash2     (HASH2_TABLE *t);
void		sort_str_hash2         (HASH2_TABLE *t);

