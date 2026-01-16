/***********************************************************************
 * File:    misc/find.h
 * Purpose: Exports from find.c
 * Author:  Karl Abrahamson
 ***********************************************************************/

extern Boolean substring_mode, ignore_case_mode;

void* 	bare_malloc_f	(LONG n);
void 	force_internal1	(char *s);
void 	force_external1	(char *s);
char* 	next_file	(FILE *index_file, char *id, char *knd, 
			 char** found_id);
int 	compare_f	(char* found, char *looking_for);
void	find_description_info(char *id, char *path, 
		      	      void (*searchf)(FILE*, char*, char*, void*),
		      	      void *info);
char* 	asthelp_path	(void);




