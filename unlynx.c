/*
** $Id$
**
** Lynx archive extractor
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
#include <string.h>
#include <errno.h>

#include "input.h"

/* maximal length of the BASIC header, if any */
#define MAXBASICLENGTH 1024

RdStatus ReadLynx (FILE *file, const char *filename,
		   WriteCallback *writeCallback, LogCallback *log)
{
  Filename name;
  unsigned f, fcount;

  /* File positions */
  long headerPos; /* current header position */
  long headerEnd; /* end of header (start of archive) */
  long archivePos; /* current archive position */

  bool errNoLength = FALSE; /* set if the file length is unknown */

  {
    BYTE *buf;
    int i;
    long length;

    if (!(buf = malloc (MAXBASICLENGTH))) {
    memError:
      (*log) (Errors, NULL, "Out of memory.");
      return RdFail;
    }

    length = fread (buf, 1, MAXBASICLENGTH, file);

    if (fseek (file, 0, SEEK_SET)) {
    seekError:
      free (buf);
      (*log) (Errors, NULL, "fseek: %s", strerror(errno));
      return RdFail;
    }

    /* skip the BASIC header, if any */
    for (i = 4; i < MAXBASICLENGTH && i < length; i++)
      if (!(memcmp (&buf[i - 4], "\0\0\0\15", 4))) {
	/* skip the BASIC header */
	if (fseek (file, i, SEEK_SET))
	  goto seekError;
	break;
      }

    free (buf);
  }

  /* Determine number of blocks and files */
  {
    char lynxhdr[25];
    unsigned blkcount;

    if (3 != fscanf (file, " %u  %24c\15 %u%*2[ \15]",
		     &blkcount, lynxhdr, &fcount) ||
	!blkcount ||
	!strstr (lynxhdr, "LYNX") ||
	!fcount) {
      (*log) (Errors, NULL, "Not a Lynx archive.");
      return RdFail;
    }

    /* Set the file pointers. */
    headerPos = ftell (file);
    headerEnd = archivePos = 254 * blkcount;
  }

  /* start extracting files */

  for (f = 0; f++ < fcount;) {
    unsigned length, blocks;

    if (headerPos >= headerEnd) {
    hdrError:
      (*log) (Errors, NULL, "Lynx header error.");
      return RdFail;
    }

    if (fseek (file, headerPos, 0)) {
      (*log) (Errors, NULL, "fseek: %s", strerror(errno));
      return RdFail;
    }

    /* read the file header information */
    {
      unsigned i;
      int j;

      /* read the file name */
      for (i = 0; i < 17; i++) {
	j = fgetc(file);

	switch (j) {
	case EOF:
	  goto hdrError;

	case 13: /* file name terminator */
	  break;

	default: /* file name character */
	  if (i > 15) {
	    (*log) (Errors, NULL, "Too long file name");
	    return RdFail;
	  }

	  name.name[i] = j;
	}

	if (j == 13) break;
      }

      if (!i) {
	(*log) (Warnings, NULL, "blank file name");
      }

      /* pad the rest of the file name with shifted spaces */
      for (; i < 16; i++)
	name.name[i] = 0xA0;
    }

    {
      char filetype;
      unsigned len;
      bool notLastFile = f < fcount;

      /* set the file type */
      if (2 != fscanf (file, " %u \015%c\015", &blocks, &filetype))
	goto hdrError;

      if (!fscanf (file, " %u%*2[ \015]", &len)) {
	/* Unspecified file length */
	if (filetype == 'R' || !notLastFile)
	  /* The length must be known for relative files */
	  /* and for all but the last file. */
	  goto hdrError;

	errNoLength = TRUE;
	len = 255;
      }

      length = len;

      name.recordLength = 0;

      switch (filetype) {
	int sidesectors;

      default:
	name.type = 0;
	(*log) (Errors, &name, "Unknown type, defaulting to DEL");
	/* fall through */
      case 'D':
	name.type = DEL;
	break;
      case 'S':
	name.type = SEQ;
	break;
      case 'P':
	name.type = PRG;
	break;
      case 'U':
	name.type = USR;
	break;
      case 'R':
	name.type = REL;
	name.recordLength = length;

	/* Thanks to Peter Schepers <schepers@ist.uwaterloo.ca>
	   for pointing out the error in the original formula. */
	sidesectors = (blocks + 119) / 121;

	if (!sidesectors ||
	    blocks < 121 * sidesectors - 119 ||
	    blocks > 121 * sidesectors)
	  goto hdrError; /* negative length file */

	blocks -= sidesectors; 
	/* Lynx is stupid enough to store the side sectors in the file. */
	archivePos += 254 * sidesectors;

	if (!(fscanf (file, " %u \015", &length))) {
	  if (notLastFile)
	    goto hdrError;

	  errNoLength = TRUE;
	  length = 255;
	}

	if (!name.recordLength)
	  (*log) (Warnings, &name, "zero record length");

	break;
      }

      if ((blocks && length < 2) || (length == 1) || (!blocks && length)) {
	(*log) (Errors, &name, "illegal length, skipping file");
	(*log) (Errors, &name,
		"FATAL: the archive may be corrupted from this point on!");
	continue;
      }

      if (blocks)
	length += (unsigned)blocks * 254 - 255;
      else
	length = 0;

      if (name.type == REL && name.recordLength && length % name.recordLength)
	(*log) (Warnings, &name, "non-integer record count");
    }

    headerPos = ftell (file);

    /* Extract the file */

    {
      BYTE *buf;
      size_t readlength;

      if (fseek (file, archivePos, SEEK_SET)) {
	(*log) (Errors, &name, "fseek: %s", strerror(errno));
	return RdFail;
      }

      if (!(buf = malloc (length)))
	goto memError;

      if (length != (readlength = fread (buf, 1, length, file))) {
	if (feof (file)) {
	  (*log) (Warnings, &name, "Truncated file, proceeding anyway");
	}
	if (ferror (file)) {
	  free (buf);
	  (*log) (Errors, &name, "fread: %s", strerror(errno));
	  return RdFail;
	}
      }

      archivePos += 254 * blocks;

      switch ((*writeCallback) (&name, buf, readlength)) {
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

  if (errNoLength)
    (*log) (Warnings, NULL, "The last file may be too long.");

  return RdOK;
}
