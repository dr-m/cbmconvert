/*
** $Id$
**
** Definitions for file writing functions
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

#ifndef _OUTPUT_H_
#  define _OUTPUT_H_

#  include "util.h"

/*******************\
** File management **
\*******************/

typedef enum { WrOK, WrNoSpace, WrFileExists, WrFail } WrStatus;

/* Write a file in some format */
typedef WrStatus WriteFunc (const Filename *name, const BYTE *data,
			    size_t length, char **newname,
			    LogCallback *logCallback);

WriteFunc WriteNative;/* Write a file in native format */
WriteFunc WritePC64;  /* PC64 format (.P00, .S00 etc.) */
WriteFunc Write9660;  /* native format, ISO 9660 compliant filenames */

/* Write a file to a disk image */
typedef WrStatus WriteImageFunc (const Filename *name, const BYTE *data,
				 size_t length, Image *image,
				 LogCallback *logCallback);

WriteImageFunc WriteImage;    /* CBM DOS disk image format */
WriteImageFunc WriteCpmImage; /* C128 CP/M disk image format */

/* Parameters:
** name: native (PETSCII) name of file to be written
** data: file bytes to be written
** length: length of file
** image: disk image buffer (WriteImage)
** newname: converted file name (WriteFuncs)
*/

/*************************\
** Disk image management **
\*************************/

typedef enum { ImOK, ImNoSpace, ImFail } ImStatus;

/* Open an existing disk image or create a new one. */
ImStatus OpenImage (const char *filename, Image **image,
		    ImageType type, DirEntOpts direntOpts);

/* Parameters:
** filename: name of 1541 disk image on the host system
** image: address of the disk image buffer pointer
** (will be allocated by this function)
** type: type of the disk image
*/

/* Write back a disk image. */
ImStatus CloseImage (Image *image);

/* Parameters:
** image: address of the disk image buffer pointer
** (will be freed by this function)
*/

/***************************\
** Archive file management **
\***************************/

typedef enum { ArOK, ArNoSpace, ArFail } ArStatus;

/* Create a new archive data structure. */
Archive *newArchive (void);
/* Delete the archive data structure. */
void deleteArchive (Archive *archive);

/* Write a file to an archive. */
WrStatus WriteArchive (const Filename *name, const BYTE *data,
		       size_t length, Archive *archive, LogCallback *log);

/* Write the archive to a Lynx file. */
typedef ArStatus WriteArchiveFunc (const Archive *archive,
				   const char *filename);

WriteArchiveFunc ArchiveLynx;

#endif /* _OUTPUT_H_ */
