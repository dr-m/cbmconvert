/**
 * @file archive.c
 * A collection of files
 * @author Marko Mäkelä (marko.makela@nic.funet.fi)
 */

/*
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
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "output.h"

/** Allocate an archive data structure.
 * @return	a newly allocated empty archive structure
 */
struct Archive*
newArchive (void)
{
  return calloc (sizeof (struct Archive), 1);
}
/** Deallocate an archive data structure.
 * @param archive	the archive to be deallocated
 */
void deleteArchive (struct Archive* archive)
{
  while (archive->first) {
    struct ArchiveEntry* ae = archive->first->next;
    free (archive->first->data);
    free (archive->first);
    archive->first = ae;
  }

  free (archive);
}

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
	      log_t log)
{
  struct ArchiveEntry* ae;

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
    if (!memcmp (&ae->name, name, sizeof (struct Filename)))
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
  ae->next = 0;

  if (archive->last)
    archive->last->next = ae;
  else
    archive->first = ae;

  archive->last = ae;

  return WrOK;
}
