/********************************************************************
 * File:    misc/config.h
 * Purpose: Define parameters of the local configuration.
 *          This is for Unix.
 *          Edit this file to configure.
 * Author:  Karl Abrahamson
 ********************************************************************/

/************************************************************************
 * BASE_DIR 		is the directory where the Astarte 		*
 *			subdirectories are placed.  If all are in the	*
 *			same subdirectory, then you only need to set	*
 *			BASE_DIR.  No need to edit this file further.	*
 *			Be sure that BASE_DIR ends on a /.		*
 ************************************************************************/

#define BASE_DIR	"/usr/local/lib/astarte/"

/************************************************************************
 * STD_DIR 		is the directory where the standard .ast	*
 *			files are located.  Do not end it with a /.	*
 ************************************************************************/

#define STD_DIR   	BASE_DIR "ast"

/************************************************************************
 * MESSAGE_DIR		is the directory where text files, such as	*
 *			error message files, are kept.  It must end	*
 *			with a /.  					*
 ************************************************************************/

#define MESSAGE_DIR	BASE_DIR "messages/"

/************************************************************************
 * DEBUG_MSG_DIR	is the directory where the debug message files	*
 *			are stored.  It must end with a /.		*
 ************************************************************************/

#define DEBUG_MSG_DIR 	BASE_DIR "messages/"

