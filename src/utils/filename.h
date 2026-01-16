/**********************************************************************
 * File:    utils/filename.h
 * Purpose: Functions on file names.
 * Author:  Karl Abrahamson
 **********************************************************************/

#include <sys/types.h>
#include <sys/stat.h>
/*#ifdef MSWIN
 typedef unsigned mode_t;
#endif*/

CONST char*   	get_absolute_std_dir	(void);
void    	make_absolute		(char *dir, CONST char *base_dir);
void    	dir_prefix      	(char *dir, char *fname, 
					 CONST char *f);
void    	get_root_name		(char *dest, CONST char *source, 
					 int size);
CONST char*   	full_file_name  	(CONST char *file_name, int kind);
CONST char*   	get_full_file_name	(CONST char *file_name, int kind);
CONST char*   	aso_name        	(CONST char *file_name);
CONST char*   	reduced_aso_name	(CONST char *file_name);
CONST char*   	ast_name        	(CONST char *file_name, 
					 Boolean force_ast);
Boolean 	install_standard_dir	(char *s, int len);
Boolean 	install_home_dir	(char *s, int len);
void    	force_internal  	(char *s);
void    	force_external  	(char *s);
void    	fprint_external 	(FILE *f, CONST char *form, 
					 CONST char *name);
Boolean 	is_absolute     	(CONST char *name);
LIST*   	file_colonsep_to_list	(CONST char *s);

FILE* 		fopen_file		(const char *name, char *mode);
int 		open_file		(const char *name, int flags, mode_t mode);
int 		stat_file		(const char *name, struct stat *buff);
int 		chdir_file		(const char *name);
int 		unlink_file		(const char *name);
int 		rename_file		(const char *name1, const char *name2);
int 		rmdir_file		(const char *name);
int 		mkdir_file		(const char *name);
void    	print_file		(char *filename);
Boolean 	can_read		(const char *file_name);

