/******************************************************************
 * File:    keyboard.h
 * Purpose: Handle input from keyboard for MSWIN implementation
 * Author:  Karl Abrahamson
 ******************************************************************/

#define KEYBOARD_BUFFER_SIZE 16

struct keyboard_buffer {
  struct keyboard_buffer* next; /* Next node in chain */
  int front;			/* Index of next char to get,
				   or same as back if no chars
				   currently available. */
  int back;                     /* Index of next available slot
				   to put a character */
  char buffer[KEYBOARD_BUFFER_SIZE];   /* Buffer that holds the chars */
};
extern struct keyboard_buffer* keyboard_buffer_chain;
extern struct keyboard_buffer* keyboard_buffer_end;

