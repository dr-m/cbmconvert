/*
** $Id$
**
** Reads files from native file system
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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "input.h"

static unsigned char ascii2petscii(unsigned char c);

RdStatus ReadNative (FILE *file, const char *filename,
		     WriteCallback *writeCallback, LogCallback *log)
{
  Filename name;
  const char *suffix = NULL;
  unsigned i;
  BYTE *buf;
  WrStatus status;

  /* Get the file base name */
  for (i = strlen (filename); i && filename[i] != PATH_SEPARATOR; i--);

  if (filename[i] == PATH_SEPARATOR) filename += i + 1;

  if (!*filename) {
    filename = "null.prg";
    (*log) (Warnings, NULL, "Null file name, changed to %s", filename);
  }

  i = strlen (filename);

  /* Determine the file type. */

  if (i >= 5) {
    suffix = &filename[i - 4];

    if (!strcmp (suffix, ".del"))
      name.type = DEL;
    else if (!strcmp (suffix, ".seq"))
      name.type = SEQ;
    else if (!strcmp (suffix, ".prg") ||
	     !strcmp (suffix, ".cvt")) /* for GEOS Convert files */
      name.type = PRG;
    else if (!strcmp (suffix, ".usr"))
      name.type = USR;
    else if (!strcmp (suffix, ".rel")) {
      name.type = REL;
      (*log) (Warnings, NULL, "unknown record length");
    }
    else if (suffix[0] == '.' && suffix[1] == 'l') {
      char *endptr = NULL;
      unsigned int recordLength = strtoul (&suffix[2], &endptr, 16);
      if (*endptr)
	goto UnknownType;

      name.type = REL;
      name.recordLength = recordLength;
    }
    else
      goto UnknownType;
  }
  else {
  UnknownType:
    (*log) (Warnings, NULL, "Unknown type, defaulting to PRG");
    name.type = PRG;
    name.recordLength = 0;
    suffix = NULL;
  }

  /* Copy the file name */
  if (suffix) {
    for (i = 0; i < suffix - filename && i < sizeof(name.name); i++)
      name.name[i] = ascii2petscii(filename[i]);

    /* Pad with shifted spaces */
    while (i < sizeof(name.name))
      name.name[i++] = 0xA0;
  }
  else {
    for (i = 0; filename[i] && i < sizeof(name.name); i++)
      name.name[i] = ascii2petscii(filename[i]);

    /* Pad with shifted spaces */
    while (i < sizeof(name.name))
      name.name[i++] = 0xA0;
  }

  /* Determine file length. */
  if (fseek (file, 0, SEEK_END)) {
  seekError:
    (*log) (Errors, NULL, "fseek: %s", strerror(errno));
    return RdFail;
  }

  i = ftell (file);

  if (fseek (file, 0, SEEK_SET))
    goto seekError;

  if (!(buf = malloc (i))) {
    (*log) (Errors, NULL, "Out of memory.");
    return RdFail;
  }

  if (1 != fread (buf, i, 1, file)) {
    (*log) (Errors, NULL, "fread: %s", strerror(errno));
    free (buf);
    return RdFail;
  }

  status = (*writeCallback) (&name, buf, i);

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

RdStatus ReadPC64 (FILE *file, const char *filename,
		   WriteCallback *writeCallback, LogCallback *log)
{
  Filename name;
  const char *suffix = NULL;
  unsigned i;
  BYTE *buf;
  WrStatus status;

  /* Determine file type. */

  i = strlen (filename);
  if (i < 5) {
    (*log) (Errors, NULL, "No PC64 file name suffix");
    return RdFail; /* no suffix */
  }

  suffix = &filename[i - 4];

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
    (*log) (Errors, NULL, "Unknown PC64 file type suffix");
    return RdFail;
  }

  /* Determine file length. */
  if (fseek (file, 0, SEEK_END)) {
  seekError:
    (*log) (Errors, NULL, "fseek: %s", strerror(errno));
    return RdFail;
  }

  i = ftell (file);

  if (fseek (file, 0, SEEK_SET))
    goto seekError;

  if (i < 26) {
    (*log) (Errors, NULL, "short file");
    return RdFail;
  }

  if (!(buf = malloc (i))) {
    (*log) (Errors, NULL, "Out of memory.");
    return RdFail;
  }

  /* Read the file. */

  if (1 != fread (buf, i, 1, file)) {
    (*log) (Errors, NULL, "fread: %s", strerror(errno));
    free (buf);
    return RdFail;
  }

  /* Check the file header. */

  if (memcmp (buf, "C64File", 8)) {
    (*log) (Errors, NULL, "Invalid PC64 header");
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

static unsigned char ascii2petscii(unsigned char c)
{
  if (c >= 'A' && c <= 'Z') /* convert upper case letters */
    c -= 'A' - 0xC1;
  else if (c >= 'a' && c <= 'z') /* convert lower case letters */
    c -= 'a' - 0x41;
  else if ((c & 127) < 32) /* convert control characters */
    c = '-';
  else if (c > 'z') /* convert graphics characters */
    c = '+';

  return c;
}
