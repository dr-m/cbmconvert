/*
** $Id$
**
** Definitions of data types and utility functions
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

#ifndef _UTIL_H_
#  define _UTIL_H_

#  define rounddiv(a,b) ((a+b-1)/b)
#  define elementsof(a) (sizeof(a)/(sizeof(a[0])))

#  ifndef FALSE
#    define FALSE 0
#    define TRUE 1
#  endif

#  if defined(__MSDOS__) || defined(MSDOS)
#    define PATH_SEPARATOR '\\'
#  else
#    define PATH_SEPARATOR '/'
#  endif

#  include <limits.h>

/* Common data types */

#  if UCHAR_MAX != 255
#    error "Wrong unsigned char range!"
#  endif
typedef unsigned char BYTE; /* guaranteed to be exactly 1 byte */
#  if UINT_MAX < 65535
#    error "Insufficient unsigned int range!"
#  endif
typedef unsigned int WORD; /* at least 16 bits range */
#  if UINT_MAX >= 16777216
typedef unsigned int TBYTE; /* at least 24 bits range */
#  else
#    if ULONG_MAX < 16777216
#      error "Insufficient unsigned long range!"
#    endif
typedef unsigned long int TBYTE;
#  endif

typedef enum { false = 0, true } bool;

typedef enum { DEL = 0x80, SEQ, PRG, USR, REL, CBM } Filetype;

typedef struct {
  unsigned char name[16];
  Filetype type;
  BYTE recordLength;
} Filename;

typedef enum { ImUnknown, Im1541, Im1571, Im1581 } ImageType;

typedef enum {
  DirEntDontCreate, /* only try to find the file name */
  DirEntOnlyCreate, /* only create a new slot */
  DirEntFindOrCreate/* create the directory entry if it doesn't exist */
} DirEntOpts;

typedef struct {
  ImageType type;	/* type of image */
  DirEntOpts direntOpts;/* getDirEnt() options */
  BYTE dirtrack;	/* (active) directory track number */
  unsigned char *name;	/* disk image file name */
  BYTE *buf;		/* disk image data */
  BYTE partBots[80]; /* lower limits of partitions */
  BYTE partTops[80]; /* upper limits of partitions */
  BYTE partUpper[80];/* parent partitions */
} Image;

typedef struct _ArchiveEntry {
  struct _ArchiveEntry *next;
  Filename name;
  size_t length;
  BYTE *data;
} ArchiveEntry;

typedef struct {
  ArchiveEntry *first, *last;
} Archive;

/*********************\
** Utility functions **
\*********************/

/* Convert the file name to a printable null-terminated string. */
const char *getFilename (const Filename *name);

typedef enum { Errors, Warnings, Everything } Verbosity;

/* Call-back function for printing error messages */
typedef void LogCallback (Verbosity verbosity, const Filename *name,
			  const char *format, ...);

#endif /* _UTIL_H_ */
