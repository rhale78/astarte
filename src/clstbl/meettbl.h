/****************************************************************
 * File:    clstbl/meettbl.h
 * Purpose: Handling of genus intersections.
 * Author:  Karl Abrahamson
 ****************************************************************/

LPAIR	get_link_label_tm 	(int upper, int lower);
void    add_ahead_meet_tm	(char *a, char *b, char *c, 
				 char *l1, char *l2);
void    process_ahead_meets_tm	(void);
void	add_intersection_tm     (CLASS_TABLE_CELL *a, CLASS_TABLE_CELL *b,
				 CLASS_TABLE_CELL *c, LPAIR l);
void 	add_intersection_from_strings_tm(CLASS_TABLE_CELL *a_ctc,
				         CLASS_TABLE_CELL *b_ctc,
				         char *c, char *l1, char *l2);
int     get_intersection_tm     (CLASS_TABLE_CELL *a, CLASS_TABLE_CELL *b);
int     full_get_intersection_tm(CLASS_TABLE_CELL *a, CLASS_TABLE_CELL *b);
CLASS_TABLE_CELL* 
	intersection_ctc_tm	(CLASS_TABLE_CELL *A, CLASS_TABLE_CELL *B);
void    install_joins		(void);
int     get_join		(CLASS_TABLE_CELL *a, CLASS_TABLE_CELL *b);
int     full_get_join		(CLASS_TABLE_CELL *a, CLASS_TABLE_CELL *b);
void	check_associativity_tm  (void);
void init_intersect_table	(void);

#ifdef DEBUG
void print_intersect_table	(void);
void show_meet_table		(void);
void show_join_table		(void);
#endif

