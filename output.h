/**
 * @file output.h
 * Definitions for file writing functions
 * @author Marko Mäkelä (marko.makela@nic.funet.fi)
 */

/*
** Copyright © 1993-1997,2001 Marko Mäkelä
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
*/

#ifndef OUTPUT_H
#  define OUTPUT_H

#  include "util.h"

/* File management */

/** Writing status */
enum WrStatus
{
  WrOK,		/**<Success */
  WrNoSpace,	/**<Out of space */
  WrFileExists,	/**<Duplicate file name */
  WrFail	/**<Generic failure */
};

/** Write a file in some format
 * @param name		native (PETSCII) name of the file
 * @param data		the contents of the file
 * @param length	length of the file contents
 * @param newname	(output) the converted file name
 * @param log		Call-back function for diagnostic output
 * @return		status of the operation
 */
typedef enum WrStatus write_t (const struct Filename* name,
			       const byte_t* data,
			       size_t length,
			       char** newname,
			       log_t log);

/** Write a file in raw format */
write_t WriteNative;
/** Write a file in PC64 format (.P00, .S00 etc.) */
write_t WritePC64;
/** Write a file in raw format, using ISO 9660 compliant filenames */
write_t Write9660;

/** Write a file to a disk image
 * @param name		native (PETSCII) name of the file
 * @param data		the contents of the file
 * @param length	length of the file contents
 * @param image		the disk image
 * @param log		Call-back function for diagnostic output
 * @return		status of the operation
 */
typedef enum WrStatus write_img_t (const struct Filename* name,
				   const byte_t* data,
				   size_t length,
				   struct Image* image,
				   log_t log);

/** Write to an image in CBM DOS format */
write_img_t WriteImage;
/** Write to an image in C128 CP/M format */
write_img_t WriteCpmImage;

/* Disk image management */

/** Disk image management status */
enum ImStatus
{
  ImOK,		/**< No errors */
  ImNoSpace,	/**< Out of space on the host system */
  ImFail	/**< Generic failure */
};

/** Open an existing disk image or create a new one.
 * @param filename	name of 1541 disk image on the host system
 * @param image		address of the disk image buffer pointer
 *			(will be allocated by this function)
 * @param type		type of the disk image
 * @param direntOpts	directory entry handling options
 * @return		Status of the operation
 */
enum ImStatus
OpenImage (const char* filename,
	   struct Image** image,
	   enum ImageType type,
	   enum DirEntOpts direntOpts);

/** Write back a disk image.
 * @param image		address of the disk image buffer
 *			(will be deallocated by this function)
 * @return		Status of the operation
 */
enum ImStatus
CloseImage (struct Image* image);

/* Archive file management */

/** Archive management status */
enum ArStatus
{
  ArOK,		/**< Successful operation */
  ArNoSpace,	/**< Out of space */
  ArFail	/**< Generic failure */
};

/** Allocate an archive data structure.
 * @return	a newly allocated empty archive structure
 */
struct Archive*
newArchive (void);

/** Deallocate an archive data structure.
 * @param archive	the archive to be deallocated
 */
void
deleteArchive (struct Archive* archive);

/** Write a file to an archive.
 * @param name		native (PETSCII) name of the file
 * @param data		the contents of the file
 * @param length	length of the file contents
 * @param archive	the archive the file is written to
 * @param log		Call-back function for diagnostic output
 * @return		status of the operation
 */
enum WrStatus
WriteArchive (const struct Filename* name,
	      const byte_t* data,
	      size_t length,
	      struct Archive* archive,
	      log_t log);

/** Write an archive to a file.
 * @param archive	the archive to be written
 * @param filename	host file name of the archive file
 * @return		status of the operation
 */
typedef enum ArStatus write_ar_t (const struct Archive* archive,
				  const char* filename);

/** Write an archive in Lynx format */
write_ar_t ArchiveLynx;
/** Write an archive in Commodore C2N tape format */
write_ar_t ArchiveC2N;

#endif /* OUTPUT_H */
