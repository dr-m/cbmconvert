/*
** $Id$
**
** Definitions for file reading functions
**
** Copyright © 1993-1997 Marko Mäkelä
**
**     This program is free software; you can redistribute it and/or modify
**     it under the terms of the GNU General Public License as published by
**     the Free Software Foundation; either version 2 of the License, or
**     (at your option) any later version.
** 
**     This program is distributed in the hope that it will be useful,
**     but WITHOUT ANY WARRANTY; without even the implied warranty of
**     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**     GNU General Public License for more details.
** 
**     You should have received a copy of the GNU General Public License
**     along with this program; if not, write to the Free Software
**     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** $Log$
*/

#ifndef _INPUT_H_
#  define _INPUT_H_

#  include "util.h"
#  include "output.h"

/*******************\
** File management **
\*******************/

/* Call-back function for writing files */
typedef WrStatus WriteCallback (const Filename *name, const BYTE *data,
				size_t length);

typedef enum { RdOK, RdFail, RdNoSpace } RdStatus;

/* Write a file in some format */
typedef RdStatus ReadFunc (FILE *file, const char *filename,
			   WriteCallback *writeCallback,
			   LogCallback *logCallback);

extern ReadFunc ReadNative;/* read a file in native format */
extern ReadFunc ReadPC64;  /* PC64 format (.P00, .S00 etc.) */
extern ReadFunc ReadLynx;  /* Lynx archive */
extern ReadFunc ReadArkive;/* Arkive archive */
extern ReadFunc ReadARC;   /* ARC/SDA archive */
extern ReadFunc ReadT64;   /* C64S tape archive */
extern ReadFunc ReadImage; /* disk image format */
extern ReadFunc ReadCpmImage; /* C128 CP/M disk image format */

/* Parameters:
** file: handle of file to be read
** filename: name of file to be read
** image: disk image buffer (only used by ReadImage)
** writeCallback: function used to write the files
** logCallback: function for displaying logging information
*/
#endif /* _INPUT_H_ */
