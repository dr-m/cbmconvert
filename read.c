/**
 * @file read.c
 * Reads files from native file system
 * @author Marko Mäkelä (marko.makela at iki.fi)
 */

/*
** Copyright © 1993-1997,2001,2006,2021 Marko Mäkelä
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
#include <errno.h>
#include <string.h>

#include "input.h"

/** Convert an ASCII character to PETSCII
 * @param c     the ASCII character to be converted
 * @return      a corresponding PETSCII character, or '+' if no conversion
 */
static unsigned char
ascii2petscii (unsigned char c)
{
  if (c >= 'A' && c <= 'Z') /* convert upper case letters */
    c -= (unsigned char) ('A' - 0xC1);
  else if (c >= 'a' && c <= 'z') /* convert lower case letters */
    c -= (unsigned char) ('a' - 0x41);
  else if ((c & 127) < 32) /* convert control characters */
    c = '-';
  else if (c == 0xa0); /* do not touch shifted spaces */
  else if (c > 'z') /* convert graphics characters */
    c = '+';

  return c;
}

/** Read a file in the native format of the host system
 * @param file          the file input stream
 * @param filename      host system name of the file
 * @param writeCallback function for writing the contained files
 * @param log           Call-back function for diagnostic output
 * @return              status of the operation
 */
enum RdStatus
ReadNative (FILE* file,
            const char* filename,
            write_file_t writeCallback,
            log_t log)
{
  struct Filename name;
  const char* suffix = 0;
  size_t i;
  byte_t* buf;
  static byte_t dummy;
  enum WrStatus status;

  /* Get the file base name */
  for (i = strlen (filename); i && filename[i] != PATH_SEPARATOR; i--);

  if (filename[i] == PATH_SEPARATOR) filename += i + 1;

  if (!*filename) {
    filename = "null.prg";
    (*log) (Warnings, 0, "Null file name, changed to %s", filename);
  }

  i = strlen (filename);

  /* Determine the file type. */
  name.recordLength = 0;

  if (i >= 3 && filename[i - 2] == ',') {
    switch (filename[i - 1]) {
    case 'd': case 'D':
      name.type = DEL; break;
    case 's': case 'S':
      name.type = SEQ; break;
    case 'p': case 'P':
      name.type = PRG; break;
    case 'u': case 'U':
      name.type = USR; break;
    default:
      goto UnknownType;
    }
    suffix = &filename[i - 2];
  }
  else if (i >= 5 && (filename[i - 4] == '.' || filename[i - 4] == ',')) {
    suffix = &filename[i - 3];

    if (!strcmp (suffix, "del") || !strcmp (suffix, "DEL"))
      name.type = DEL;
    else if (!strcmp (suffix, "seq") || !strcmp (suffix, "SEQ"))
      name.type = SEQ;
    else if (!strcmp (suffix, "prg") || !strcmp (suffix, "PRG") ||
             !strcmp (suffix, "cvt") || !strcmp (suffix, "CVT"))
      name.type = PRG; /* CVT for GEOS Convert files */
    else if (!strcmp (suffix, "usr"))
      name.type = USR;
    else if (!strcmp (suffix, "rel") || !strcmp (suffix, "REL")) {
      name.type = REL;
      (*log) (Warnings, 0, "unknown record length");
    }
    else if (*suffix == 'l') {
      char* endptr = 0;
      unsigned long recordLength = strtoul (suffix + 1, &endptr, 16);
      if (*endptr || recordLength > 254)
        goto UnknownType;

      name.type = REL;
      name.recordLength = (unsigned char) recordLength;
    }
    else
      goto UnknownType;

    suffix--;
  }
  else {
  UnknownType:
    (*log) (Warnings, 0, "Unknown type, defaulting to PRG");
    name.type = PRG;
    suffix = 0;
  }

  /* Copy the file name */
  if (suffix) {
    for (i = 0;
         i < (unsigned) (suffix - filename) && i < sizeof(name.name);
         i++)
      name.name[i] = ascii2petscii((unsigned char) filename[i]);
  }
  else {
    for (i = 0; filename[i] && i < sizeof(name.name); i++)
      name.name[i] = ascii2petscii((unsigned char) filename[i]);
  }

  memset (&name.name[i], 0xA0/* shifted space */, (sizeof name.name) - i);

  /* Determine file length. */
  if (fseek (file, 0, SEEK_END)) {
  seekError:
    (*log) (Errors, 0, "fseek: %s", strerror(errno));
    return RdFail;
  }
  else {
    long l = ftell (file);
    if (l < 0)
      goto seekError;
    i = (size_t) l;
  }

  if (fseek (file, 0, SEEK_SET))
    goto seekError;

  if (i == 0)
    buf = &dummy;
  else if (!(buf = malloc (i))) {
    (*log) (Errors, 0, "Out of memory.");
    return RdFail;
  }
  else if (1 != fread (buf, i, 1, file)) {
    (*log) (Errors, 0, "fread: %s", strerror(errno));
    free (buf);
    return RdFail;
  }

  status = (*writeCallback) (&name, buf, i);

  if (i)
    free (buf);

  switch (status) {
  case WrOK:
    return RdOK;
  case WrNoSpace:
    return RdNoSpace;
  case WrFail:
  default:
    return RdFail;
  }
}

/** Read a PC64 file (.P00, .S00 etc.)
 * @param file          the file input stream
 * @param filename      host system name of the file
 * @param writeCallback function for writing the contained files
 * @param log           Call-back function for diagnostic output
 * @return              status of the operation
 */
enum RdStatus
ReadPC64 (FILE* file,
          const char* filename,
          write_file_t writeCallback,
          log_t log)
{
  struct Filename name;
  const char* suffix = 0;
  unsigned i;
  byte_t* buf;
  enum WrStatus status;

  /* Determine file type. */

  {
    size_t s = strlen (filename);
    if (s < 5) {
      (*log) (Errors, 0, "No PC64 file name suffix");
      return RdFail; /* no suffix */
    }
    suffix = &filename[s - 4];
  }

  if (1 == sscanf (suffix, ".d%02u", &i))
    name.type = DEL;
  else if (1 == sscanf (suffix, ".s%02u", &i))
    name.type = SEQ;
  else if (1 == sscanf (suffix, ".p%02u", &i))
    name.type = PRG;
  else if (1 == sscanf (suffix, ".u%02u", &i))
    name.type = USR;
  else if (1 == sscanf (suffix, ".r%02u", &i))
    name.type = REL;
  else {
    (*log) (Errors, 0, "Unknown PC64 file type suffix");
    return RdFail;
  }

  /* Determine file length. */
  if (fseek (file, 0, SEEK_END)) {
  seekError:
    (*log) (Errors, 0, "fseek: %s", strerror(errno));
    return RdFail;
  }
  else {
    long l = ftell (file);
    if (l < 0)
      goto seekError;
    i = (unsigned) l;
  }

  if (fseek (file, 0, SEEK_SET))
    goto seekError;

  if (i < 26) {
    (*log) (Errors, 0, "short file");
    return RdFail;
  }

  if (!(buf = malloc (i))) {
    (*log) (Errors, 0, "Out of memory.");
    return RdFail;
  }

  /* Read the file. */

  if (1 != fread (buf, i, 1, file)) {
    (*log) (Errors, 0, "fread: %s", strerror(errno));
    free (buf);
    return RdFail;
  }

  /* Check the file header. */

  if (memcmp (buf, "C64File", 8)) {
    (*log) (Errors, 0, "Invalid PC64 header");
    free (buf);
    return RdFail;
  }

  memcpy (name.name, &buf[8], 16);
  name.recordLength = buf[25];

  status = (*writeCallback) (&name, &buf[26], i - 26);
  free (buf);

  switch (status) {
  case WrOK:
    return RdOK;
  case WrNoSpace:
    return RdNoSpace;
  case WrFail:
  default:
    return RdFail;
  }
}
