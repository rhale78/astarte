/*****************************************************************
 * File:    show/printrts.h
 * Purpose: Print run time stack after trap
 * Author:  Karl Abrahamson
 *****************************************************************/

/*********************** From dumpargs.c **************************/

void    init_dump_args	(void);

/********************** From printrts.c ***************************/

/****************************************************************
 * NUM_TRAP_MSGS should be the number of noncomment lines in 	*
 * trapmsg.txt. 						*
 ****************************************************************/

#define NUM_TRAP_MSGS 52

#define NONE_VIS	0	/* Nothing is currently running */
#define LET_VIS		1	/* About to do a let */
#define RETURN_VIS	2	/* About to return from a call */
#define NAME_VIS	3	/* About to name a function */
#define TRAP_VIS	4	/* Have executed a trap */
#define FAIL_VIS	5	/* Have failed */
#define RESUME_VIS	6	/* About to resume a coroutine */
#define LAZY_VIS	7	/* About to do a lazy evaluation */
#define END_LAZY_VIS	8	/* About to end a lazy evaluation */
#define APPLY_VIS	9	/* About to apply a function */
#define EXEC_BEGIN_VIS  10	/* About to begin an execute */
#define EXEC_END_VIS    11	/* About to end an execute */
#define ASSIGN_VIS	12	/* About to do assign */
#define PRINT_VIS	13	/* About to do print or show! */
#define DONE_VIS	14	/* Computation is finished */

extern char **trap_msg;
extern int  *trap_msg_len;
extern BREAK_INFO 	break_info;
extern FILE*		rts_file;
extern LONG 		rts_file_max_chars;
extern LONG 		rts_file_max_entity_chars;
extern Boolean 		printing_rts, 
			suppress_print_rts_in_abort;

void    read_trap_msgs  		(void);
void 	init_break_info			(void);
Boolean breakable			(void);
void    dump_die			(int n);
Boolean more_wait			(void);
int     print_exception_description	(FILE *f, ENTITY ex);
Boolean print_rts			(ACTIVATION *a, ENTITY ex, int instr, 
					 CODE_PTR pc);
Boolean print_rts_on_file		(FILE *f, ACTIVATION *a, ENTITY ex, 
					 int instr, CODE_PTR pc);
void    deadlock_complain		(void);
ENTITY  show_env_stdf			(ENTITY f);
ENTITY  show_config_stdf		(ENTITY f);
void    record_bxpl_for_print_rts	(ENTITY bxpl, TYPE *ty);
void    get_name_rest_of_line		(char *name, int n, int prev);
void    debug_interact			(void);
void 	vis_break			(int kind);
int     possibly_abort_dialog  		(char *message);

/********************** From showrts.c ***************************/

void    show_config     (void);
void    show_act	(ACTIVATION *a, int *skip_frames, int skip_entries, 
		 	 int l_lines, int l_columns);
void    show_frame_name	(char *name, int skip_frames, int l_columns);
void    show_env	(ENVIRONMENT *env, CODE_PTR pc, int num_entries, 
	      		 int skip, int l_lines, int l_columns);

