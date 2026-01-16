/*********************************************************
 * File:    prof.h
 * Purpose: Determine locations of files.  (Header for prof.c.)
 * Author:  Karl Abrahamson
 *********************************************************/

#define ASTC_EXE "astc.exe"
#define ASTR_EXE "astr.exe"

extern char* MESSAGE_DIR;
extern char* DEBUG_MSG_DIR;
extern char* STD_DIR;
extern char* BIN_DIR;
extern char* ConsoleFontName;
extern char* ConsoleFontFile;
extern int   ConsoleFontSize;

extern "C" void get_profile(void);
extern "C" void free_profile_strings(void);
