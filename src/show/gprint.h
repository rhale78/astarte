/****************************************************************
 * File:    show/gprint.h
 * Purpose: Print on either standard output or curses window
 * Author:  Karl Abrahamson
 ****************************************************************/

void gprintf (FILE *where, char *fmt, ...);
int  gprint_str(FILE* where, char* s, int k);
void ggprintf(FILE_OR_STR *f, char *fmt, ...);
