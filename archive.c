/*
** $Id$
**
** File archive creator
**
** Copyright © 1998 Marko Mäkelä
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
#include <string.h>
#include <errno.h>

#include "output.h"

/* Create a new archive data structure. */
Archive *newArchive (void)
{
  return calloc (sizeof (Archive), 1);
}
/* Delete the archive data structure. */
void deleteArchive (Archive *archive)
{
  while (archive->first) {
    ArchiveEntry *ae = archive->first->next;
    free (archive->first->data);
    free (archive->first);
    archive->first = ae;
  }

  free (archive);
}

WrStatus WriteArchive (const Filename *name, const BYTE *data,
		       size_t length, Archive *archive, LogCallback *log)
{
  ArchiveEntry *ae;

  switch (name->type) {
  default:
    (*log) (Errors, name, "Unsupported file type.");
    return WrFail;
  case DEL:
  case SEQ:
  case PRG:
  case USR:
  case REL:
    break;
  }

  /* check for duplicate file names */
  for (ae = archive->first; ae; ae = ae->next)
    if (!memcmp (&ae->name, name, sizeof (Filename)))
      return WrFileExists;

  if (!(ae = malloc (sizeof (*ae)))) {
    (*log) (Errors, name, "Out of memory.");
    return WrNoSpace;
  }

  if (!(ae->data = malloc (length))) {
    free (ae);
    (*log) (Errors, name, "Out of memory.");
    return WrNoSpace;
  }

  memcpy (&ae->name, name, sizeof (*name));
  memcpy (ae->data, data, length);
  ae->length = length;
  ae->next = NULL;

  if (archive->last)
    archive->last->next = ae;
  else
    archive->first = ae;

  archive->last = ae;

  return WrOK;
}

ArStatus ArchiveLynx (const Archive *archive, const char *filename)
{
  FILE *f;
  ArchiveEntry *ae;
  int blockcounter;

  static const BYTE basichdr[] = {
    0x01, 0x08, 0x5b, 0x08, 0x0a, 0x00, 0x97, 0x35,
    0x33, 0x32, 0x38, 0x30, 0x2c, 0x30, 0x3a, 0x97,
    0x35, 0x33, 0x32, 0x38, 0x31, 0x2c, 0x30, 0x3a,
    0x97, 0x36, 0x34, 0x36, 0x2c, 0xc2, 0x28, 0x31,
    0x36, 0x32, 0x29, 0x3a, 0x99, 0x22, 0x93, 0x11,
    0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x22,
    0x3a, 0x99, 0x22, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x55, 0x53, 0x45, 0x20, 0x4c, 0x59, 0x4e, 0x58,
    0x20, 0x54, 0x4f, 0x20, 0x44, 0x49, 0x53, 0x53,
    0x4f, 0x4c, 0x56, 0x45, 0x20, 0x54, 0x48, 0x49,
    0x53, 0x20, 0x46, 0x49, 0x4c, 0x45, 0x22, 0x3a,
    0x89, 0x31, 0x30, 0x00, 0x00, 0x00, 0x0d
  };

  static const BYTE lynxhdr[] = "*LYNX BY CBMCONVERT 2.0*";

  {
    unsigned filecnt;
    /* Count the files. */
    for (filecnt = 0, ae = archive->first; ae; filecnt++)
      ae = ae->next;
    if (!filecnt)
      return ArFail;

    /* Write the Lynx header. */

    if (!(f = fopen (filename, "wb")))
      return errno == ENOSPC ? ArNoSpace : ArFail;

    if (1 != fwrite (basichdr, sizeof basichdr, 1, f)) {
      fclose (f);
      return errno == ENOSPC ? ArNoSpace : ArFail;
    }

    /* This is a bit overestimating the header size. */
    blockcounter = rounddiv(sizeof basichdr + 20 + sizeof lynxhdr +
			    36 * filecnt, 254);

    fprintf (f, " %u  %s\15 %u \15", blockcounter, lynxhdr, filecnt);
  }

  if (ferror (f))
    return errno == ENOSPC ? ArNoSpace : ArFail;

  /* Write the Lynx directory. */
  for (ae = archive->first; ae; ae = ae->next) {
    int i;
    /* Write the file name.  Replace CRs with periods. */
    for (i = 0; i < sizeof(ae->name.name) && i < 16; i++)
      putc (ae->name.name[i] == 13 ? '.' : ae->name.name[i], f);

    i = rounddiv(ae->length, 254);

    fprintf (f, "\15 %u\15%c\15",
	     ae->name.type == REL ? i + rounddiv(i, 120) : i,
	     "DSPUR"[ae->name.type & 7]);
    if (ae->name.type == REL)
      fprintf (f, " %u \15", ae->name.recordLength);

    fprintf (f, " %u \15",
	     (unsigned)(ae->length % 254 ?
			(ae->length - 254 * (i - 1) + 1) : 255));
  }

  if (ferror (f))
    return errno == ENOSPC ? ArNoSpace : ArFail;

  /* Write the files. */

  for (ae = archive->first; ae; ae = ae->next) {
    unsigned blocks = rounddiv(ae->length, 254);

    /* Reserve space for the side sectors. */
    if (ae->name.type == REL)
      blockcounter += (blocks + 119) / 121;

    /* Write the file. */
    if (fseek (f, blockcounter * 254, SEEK_SET) ||
	ae->length != fwrite (ae->data, 1, ae->length, f)) {
      fclose (f);
      return ArFail;
    }

    blockcounter += blocks;
  }

  fclose (f);
  return ArOK;
}

