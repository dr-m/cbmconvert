/*
** $Id$
**
** Arkive archive extractor
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

typedef struct {
  BYTE filetype;
  BYTE lastSectorLength;
  BYTE name[16];
  BYTE recordLength;
  BYTE unknown[6];
  BYTE sidesectCount;
  BYTE sidesectLastLength;
  BYTE blocksLow;
  BYTE blocksHigh;
} ArkiveEntry;

RdStatus ReadArkive (FILE *file, const char *filename,
		     WriteCallback *writeCallback, LogCallback *log)
{
  Filename name;
  ArkiveEntry entry;
  int f, fcount;

  /* File positions */
  long headerPos; /* current header position */
  long archivePos; /* current archive position */

  if (EOF == (fcount = fgetc (file))) {
  hdrError:
    (*log) (Errors, NULL, "File header read failed: %s", strerror(errno));
    return RdFail;
  }

  headerPos = ftell (file);
  archivePos = 254 * rounddiv(headerPos + fcount * sizeof entry, 254);

  /* start extracting files */

  for (f = 0; f++ < fcount;) {
    size_t length;
    unsigned blocks;

    if (fseek (file, headerPos, SEEK_SET) ||
	1 != fread (&entry, sizeof entry, 1, file))
      goto hdrError;

    headerPos += sizeof entry;

    /* copy file name */
    memcpy (name.name, entry.name, 16);

    /* copy the record length */
    name.recordLength = entry.recordLength;

    /* determine file length */
    blocks = entry.blocksLow + (entry.blocksHigh << 8);
    length = 254 * blocks + entry.lastSectorLength - 255;

    /* determine file type */

    switch (entry.filetype & ~0x38) {
    case DEL:
    case SEQ:
    case PRG:
      name.type = entry.filetype & ~0x38;
      break;

    case REL:
      name.type = REL;

      if (!name.recordLength)
	(*log) (Warnings, &name, "zero record length");

      {
	unsigned sidesectCount, sidesectLastLength;

	sidesectCount = (blocks + 119) / 121;
	sidesectLastLength = 15 + 2 * ((blocks - sidesectCount) % 120);

	if (entry.sidesectCount != sidesectCount ||
	    entry.sidesectLastLength != sidesectLastLength) {
	  (*log) (Errors, &name, "improper side sector length");
	  (*log) (Errors, &name, "Following files may be totally wrong!");
	}

	length = (blocks - sidesectCount) * 254 - 255 +
	  entry.lastSectorLength;
      }
      
      break;

    default:
      name.type = 0;
      (*log) (Errors, &name, "Unknown type, defaulting to DEL");
      name.type = DEL;
      break;
    }

    /* read the file */

    {
      BYTE *buf;

      if (fseek (file, archivePos, SEEK_SET)) {
	(*log) (Errors, &name, "fseek: %s", strerror(errno));
	return RdFail;
      }

      if (!(buf = malloc (length))) {
	(*log) (Errors, &name, "Out of memory.");
	return RdFail;
      }

      if (length != fread (buf, 1, length, file)) {
	free (buf);
	(*log) (Errors, &name, "fread: %s", strerror(errno));
	return RdFail;
      }

      archivePos += 254 * blocks;

      if (name.type == REL)
	/* Arkive stores the last side sector, */
	/* wasting 254 bytes */
	/* for each relative file. */
	archivePos -= 254 * (entry.sidesectCount - 1);

      switch ((*writeCallback) (&name, buf, length)) {
      case WrOK:
	break;
      case WrNoSpace:
	free (buf);
	return RdNoSpace;
      case WrFail:
      default:
	free (buf);
	return RdFail;
      }

      free (buf);
    }
  }

  return RdOK;
}
