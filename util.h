/**
 * @file util.h
 * Definitions of data types and utility functions
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

#ifndef UTIL_H
#  define UTIL_H

/**
 * Rounded integer division
 * @param a	the numerator
 * @param b	the denominator
 * @return	a divided by b, rounded up to the next integer value
 */
#  define rounddiv(a,b) ((a + b - 1) / b)
/**
 * Determine the number of elements in an array
 * @param a	an array
 * @return	number of elements in the array
 */
#  define elementsof(a) ((sizeof a) / (sizeof *a))

#  if defined(__MSDOS__) || defined(MSDOS)
/** Directory path separator character */
#    define PATH_SEPARATOR '\\'
#  else
/** Directory path separator character */
#    define PATH_SEPARATOR '/'
#  endif

#  include <limits.h>

/* Common data types */

#  if UCHAR_MAX != 255
#    error "Wrong unsigned char range!"
#  endif
/** A data type of exactly one byte */
typedef unsigned char byte_t;
#  if UINT_MAX < 65535
#    error "Insufficient unsigned int range!"
#  endif
/** An unsigned data type with at least 16 bits of precision */
typedef unsigned int word_t;
#  if UINT_MAX >= 16777216
/** An unsigned data type with at least 24 bits of precision */
typedef unsigned int tbyte_t;
#  else
#    if ULONG_MAX < 16777216
#      error "Insufficient unsigned long range!"
#    endif
/** An unsigned data type with at least 24 bits of precision */
typedef unsigned long int tbyte_t;
#  endif

/** Truth value */
typedef enum
{
  false = 0,	/**< false, binary digit '0' */
  true		/**< true, binary digit '1' */
} bool;

/** Commodore file types */
enum Filetype
{
  DEL = 0x80,	/**< Deleted (sequential) file */
  SEQ,		/**< Sequential data file */
  PRG,		/**< Sequential program file */
  USR,		/**< Sequential data file with user-defined structure */
  REL,		/**< Random-access data file */
  CBM		/**< 1581 partition */
};

/** Commodore file name */
struct Filename
{
  /** The file name, padded with shifted spaces */
  unsigned char name[16];
  /** The file type */
  enum Filetype type;
  /** Record length for random-access (relative) files */
  byte_t recordLength;
};

/** Disk image types */
enum ImageType
{
  ImUnknown,	/**< Unknown or unrecognized image */
  Im1541,	/**< 35-track 1541, 3040 or 4040 disk image */
  Im1571,	/**< 70-track 1571 disk image */
  Im1581	/**< 80-track 1581 disk image */
};

/** Options for getDirEnt () */
enum DirEntOpts
{
  DirEntDontCreate, /**< only try to find the file name */
  DirEntOnlyCreate, /**< only create a new slot */
  DirEntFindOrCreate/**< create the directory entry if it doesn't exist */
};

/** Disk image */
struct Image
{
  /** type of disk image */
  enum ImageType type;
  /** getDirEnt() behaviour */
  enum DirEntOpts direntOpts;
  /** (active) directory track number */
  byte_t dirtrack;
  /** disk image file name on the host system */
  unsigned char* name;
  /** disk image data */
  byte_t* buf;
  /** lower limits of partitions (for the 1581) */
  byte_t partBots[80];
  /** upper limits of partitions (for the 1581) */
  byte_t partTops[80];
  /** parent partitions (for the 1581) */
  byte_t partUpper[80];
};

/** An entry in a file archive */
struct ArchiveEntry
{
  /** Pointer to next archive entry */
  struct ArchiveEntry* next;
  /** The file name of the entry */
  struct Filename name;
  /** Length of the entry in bytes */
  size_t length;
  /** The contents of the entry */
  byte_t* data;
};

/** A file archive */
struct Archive
{
  /** The first archive entry */
  struct ArchiveEntry* first;
  /** The last archive entry */
  struct ArchiveEntry* last;
};

/* Utility functions */

/** Convert a file name to a printable null-terminated string.
 * @param name	the PETSCII file name to be converted
 * @return	the corresponding ASCII file name
 */
const char*
getFilename (const struct Filename* name);

/** Verbosity level of diagnostic output */
enum Verbosity
{
  Errors,	/**< Display only errors; report an error */
  Warnings,	/**< Display errors and warnings; report a warning */
  Everything	/**< Display everything; report an informational message */
};

/** Call-back function for diagnostic output
 * @param verbosity	the verbosity level
 * @param name		the file name associated with the message (or NULL)
 * @param format	printf-like format string followed by arguments
 */
typedef void log_t (enum Verbosity verbosity,
		    const struct Filename* name,
		    const char* format, ...);

#endif /* UTIL_H */
