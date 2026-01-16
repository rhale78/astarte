/*********************************************************************
 * File:    misc/asd.h
 * Purpose: Handle opening and closing of .asd files.
 * Author:  Karl Abrahamson
 *********************************************************************/

void open_description_file	(char *this_file_ast);
void open_index_file		(char *this_dir, char *fname);
void close_index_files		(Boolean clean);
