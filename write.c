/**
 * @file write.c
 * Writes files with converted names
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

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

#include "output.h"

/** Convert a PETSCII file name to a printable null-terminated ASCII string.
 * @param name		the PETSCII file name
 * @param newname	(output) the converted file name
 * @return		false on out of memory
 */
static bool
filename2char (const struct Filename* name, char** newname)
{
  int i = sizeof(name->name);
  unsigned char* c;

  if (!newname)
    return false;

  /* free the old string */
  if (*newname) free (*newname);
  *newname = 0;

  /* search for shifted spaces at the end */
  while (--i && name->name[i] == 0xA0);

  /* copy the PETSCII filename */
  if (!(*newname = malloc (i + 2)))
    return false;
  memcpy (*newname, name->name, i + 1);
  (*newname)[i + 1] = 0;

  for (c = (unsigned char*)*newname; *c; c++) {
    if (*c == '/') /* map slash */
      *c = '.';
    else if (*c >= 0x41 && *c <= 0x5A) /* convert lower case letters */
      *c += 'a' - 0x41;
    else if (*c >= 0xC1 && *c <= 0xDA) /* convert upper case letters */
      *c += 'A' - 0xC1;
    else if ((*c & 127) < 32) /* convert control chars */
      *c = '-';
    else if (*c > 0xDA) /* convert graphics characters */
      *c = '+';
  }

  return true;
}

/** Determine whether a character is a wovel in English
 * @param c	the character
 * @return	true if c is a wovel
 */
static bool
isWovel (unsigned char c)
{
  return !!memchr ("AEIOU", c, 5);
}

/** A character that was removed to make a file name ISO 9660 compliant */
#define REMOVED '-'

/** Truncate the file name in-place to ISO9660 compliant format.
 * @param name	a null-terminated string that is at least 1 character long
 *		(excluding the terminating NUL character)
 * @return	the length of the truncated string,
 *		excluding the terminating NUL character
 */
static int
TruncateName (unsigned char* name)
{
  unsigned char* c;
  int len, efflen;

  for (c = name; *c; c++) {
    if (*c >= 'a' && *c <= 'z')     /* Lower case chars are OK */
      continue;
    else if (*c >= '0' && *c <= '9')/* So are numbers */
      continue;
    else if (*c >= 'A' && *c <= 'Z')/* Convert characters to lower case */
      *c -= 'A' - 'a';
    else if (*c >= 0xC1 && *c <= 0xDA) /* Convert upper case PETSCII chars */
      *c -= 0xC1 - 'a';
    else
      *c = '_';                     /* Convert anything else to underscore */
  }

  efflen = len = c - name;

  if (efflen > 8) {
    /* Remove underscores from the end */
    for (c = &name[len]; c > name; c--) {
      if (*c == '_') {
	*c = REMOVED;
	if (--efflen <= 8) break;
      }
    }
  }

  if (efflen > 8) {
    /* remove wovels from the end */
    int i;

    /* Search for the first non-wovel */
    for (i = 0; i < len && isWovel (name[i]); i++);

    if (i < len) {
      for (c = &name[len]; c > &name[i]; c--)
	if (isWovel (*c)) {
	  *c = REMOVED;
	  if (--efflen <= 8) break;
	}
    }
  }

  if (efflen > 8) {
    /* remove letters from the end */
    for (c = &name[len]; c > name; c--)
      if (*c >= 'A' && *c <= 'Z') {
	*c = REMOVED;
	if (--efflen <= 8) break;
      }
  }

  if (efflen > 8) {
    /* remove anything from the end */

    for (c = &name[len]; c > name; c--)
      if (*c && *c != REMOVED) {
	*c = REMOVED;
	if (--efflen <= 8) break;
      }
  }

  if (!efflen) {
    /* create a file name for empty file names */
    name[0] = '_';
    name[1] = 0;
  }
  else {
    /* remove removed characters */
    unsigned char* t;

    for (c = t = name; *c; c++)
      if (*c != REMOVED)
	*t++ = *c;

    *t = 0;
  }

  return efflen;
}

/** Return a file name suffix corresponding to a Commodore file type
 * @param filename	the Commodore file name
 * @return		a corresponding file name suffix
 */
static const char*
filesuffix (const struct Filename* filename)
{
  /** Suffix for relative files */
  static char relsuffix[5];

  switch (filename->type) {
  case DEL:
    return ".del";
  case SEQ:
    return ".seq";
  case PRG:
    return ".prg";
  case USR:
    return ".usr";
  case REL:
    sprintf (relsuffix, ".l%02X", filename->recordLength & 0xFF);
    return relsuffix;
  case CBM:
    return ".cbm";
  }

  return "";
}

/** Write data to a file
 * @param data		the contents of the file
 * @param length	length of the file contents
 * @param newname	(output) the converted file name
 * @param name		native (PETSCII) name of the file
 * @param log		Call-back function for diagnostic output
 * @return		status of the operation
 */
static enum WrStatus
do_it (const byte_t* data,
       size_t length,
       char** newname,
       const struct Filename* name,
       log_t log)
{
  FILE* f;

  if (!(f = fopen (*newname, "wb"))) {
    (*log) (Errors, name, "fopen: %s", strerror (errno));
    return errno == ENOSPC ? WrNoSpace : WrFail;
  }

  if (length != fwrite (data, 1, length, f)) {
    (*log) (Errors, name, "fwrite: %s", strerror (errno));
    fclose (f);
    return errno == ENOSPC ? WrNoSpace : WrFail;
  }

  fclose (f);

  return WrOK;
}

/** Write a file in raw format
 * @param name		native (PETSCII) name of the file
 * @param data		the contents of the file
 * @param length	length of the file contents
 * @param newname	(output) the converted file name
 * @param log		Call-back function for diagnostic output
 * @return		status of the operation
 */
enum WrStatus
WriteNative (const struct Filename* name,
	     const byte_t* data,
	     size_t length,
	     char** newname,
	     log_t log)
{
  struct stat statbuf;
  char* filename;
  int i;

  if (!filename2char (name, newname))
    return WrFail;

  if (!(filename = malloc (strlen (*newname) + (4 + 5 + 1)))) {
    free (filename);
    return WrFail;
  }

  /* try the plain filename */
  sprintf (filename, "%s%.4s", *newname, filesuffix (name));

  if (stat (filename, &statbuf)) {
    free (*newname);
    *newname = filename;
    return do_it (data, length, newname, name, log);
  }

  for (i = 0; i < 10000; i++) {
    sprintf (filename, "%s~%d%.4s", *newname, i, filesuffix (name));
    if (stat (filename, &statbuf)) { /* found an available file name */
      free (*newname);
      *newname = filename;
      return do_it (data, length, newname, name, log);
    }
  }

  (*log) (Errors, name, "out of file name space");
  return WrFail;
}

/** Write a file in PC64 format (.P00, .S00 etc.)
 * @param name		native (PETSCII) name of the file
 * @param data		the contents of the file
 * @param length	length of the file contents
 * @param newname	(output) the converted file name
 * @param log		Call-back function for diagnostic output
 * @return		status of the operation
 */
enum WrStatus
WritePC64 (const struct Filename* name,
	   const byte_t* data,
	   size_t length,
	   char** newname,
	   log_t log)
{
  char* filename,* c;
  int i;
  struct stat statbuf;

  if (!filename2char (name, newname))
    return WrFail;

  if (!(filename = malloc (TruncateName ((unsigned char*)*newname) + 4 + 1)))
    return WrFail;

  i = sprintf (filename, "%s%.4s", *newname, filesuffix (name));

  if (name->type == REL) /* Fix the suffix for relative files */
    filename[i - 3] = 'r';

  c = &filename[i - 2];

  for (i = 0; i < 100; i++) {
    sprintf (c, "%02d", i);
    if (stat (filename, &statbuf)) { /* found an available file name */
      FILE* f;

      free (*newname);
      *newname = filename;

      if (!(f = fopen (*newname, "wb"))) {
	(*log) (Errors, name, "fopen: %s", strerror (errno));
	return errno == ENOSPC ? WrNoSpace : WrFail;
      }

      if (1 != fwrite ("C64File", 8, 1, f) ||
	  1 != fwrite (name->name, 16, 1, f) ||
	  EOF == fputc (0, f) ||
	  EOF == fputc (name->recordLength, f) ||
	  length != fwrite (data, 1, length, f)) {
	(*log) (Errors, name, "fwrite: %s", strerror (errno));
	fclose (f);
	return errno == ENOSPC ? WrNoSpace : WrFail;
      }

      fclose (f);

      return WrOK;
    }
  }

  (*log) (Errors, name, "out of file name space");
  return WrFail;
}

/** Write a file in raw format, using ISO 9660 compliant filenames
 * @param name		native (PETSCII) name of the file
 * @param data		the contents of the file
 * @param length	length of the file contents
 * @param newname	(output) the converted file name
 * @param log		Call-back function for diagnostic output
 * @return		status of the operation
 */
enum WrStatus
Write9660 (const struct Filename* name,
	   const byte_t* data,
	   size_t length,
	   char** newname,
	   log_t log)
{
  char* filename;
  int i;
  struct stat statbuf;

  if (!filename2char (name, newname))
    return WrFail;

  if (!(filename = malloc (TruncateName ((unsigned char*)*newname) + 4 + 1)))
    return WrFail;

  /* try the basic file name */
  sprintf (filename, "%s%.4s", *newname, filesuffix (name));
  if (stat (filename, &statbuf)) {
    free (*newname);
    *newname = filename;
    return do_it (data, length, newname, name, log);
  }

  /* try with .000-style file names */
  for (i = 0; i < 1000; i++) {
    sprintf (filename, "%s.%03u", *newname, i);
    if (stat (filename, &statbuf)) { /* found an available file name */
      free (*newname);
      *newname = filename;
      return do_it (data, length, newname, name, log);
    }
  }

  (*log) (Errors, name, "out of file name space");
  return WrFail;
}
