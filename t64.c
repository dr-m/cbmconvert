/*
** $Id$
**
** T64 archive extractor
**
** Copyright © 1997 Marko Mäkelä
** Based on the cbmarcs.c file of fvcbm 2.0 by Daniel Fandrich
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
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "input.h"

typedef struct {
  BYTE headerblock[32];
  BYTE minorVersion;
  BYTE majorVersion;
  BYTE maxEntriesLow;
  BYTE maxEntriesHigh;
  BYTE numEntriesLow;
  BYTE numEntriesHigh;
  BYTE padding[26];
} T64Header;

typedef struct {
  BYTE entryType; /* 1 == normal file */
  BYTE fileType;
  BYTE startAddrLow;
  BYTE startAddrHigh;
  BYTE endAddrLow;
  BYTE endAddrHigh;
  BYTE padding1[2];
  BYTE fileOffsetLowest;
  BYTE fileOffsetLower;
  BYTE fileOffsetHigher;
  BYTE fileOffsetHighest;
  BYTE padding2[4];
  BYTE name[16];
} T64Entry;

RdStatus ReadT64 (FILE *file, const char *filename,
		  WriteCallback *writeCallback, LogCallback *log)
{
  unsigned numEntries, entry;

  /* Check the header. */

  {
    T64Header t64header;
    unsigned maxEntries;

    static const char T64Header1[] = "C64 tape image file";
    static const char T64Header2[] = "C64S tape file";
    static const char T64Header3[] = "C64S tape image file";

    if (1 != fread (&t64header, sizeof t64header, 1, file)) {
    freadFail:
      (*log) (Errors, NULL, "fread: %s", strerror(errno));
      return RdFail;
    }

    if (memcmp (t64header.headerblock, T64Header1, sizeof T64Header1 - 1) &&
	memcmp (t64header.headerblock, T64Header2, sizeof T64Header2 - 1) &&
	memcmp (t64header.headerblock, T64Header3, sizeof T64Header3 - 1)) {
      (*log) (Errors, NULL, "Unknown T64 header");
      return RdFail;
    }

    if (t64header.majorVersion != 1 || t64header.minorVersion != 0)
      (*log) (Errors, NULL, "Unknown T64 version, trying anyway");

    maxEntries = ((unsigned)t64header.maxEntriesLow |
		  (t64header.maxEntriesHigh << 8));
    numEntries = ((unsigned)t64header.numEntriesLow |
		  (t64header.numEntriesHigh << 8));

    if (!numEntries) {
      (*log) (Warnings, NULL, "Number of entries set to zero; trying to read the first entry");
      numEntries = 1;
    }
    if (numEntries > maxEntries) {
      (*log) (Errors, NULL, "Error in the number of entries");
      return RdFail;
    }

    (*log) (Everything, NULL, "T64 version %u.%u, %u/%u files",
	    t64header.majorVersion, t64header.minorVersion,
	    numEntries, maxEntries);
  }

  /* Process the files */

  for (entry = 0; entry < numEntries; entry++) {
    T64Entry t64entry;
    Filename name;
    long fileoffset;
    size_t length;

    name.type = PRG;
    name.recordLength = 0;

    if (fseek (file, sizeof(T64Header) + entry * sizeof(T64Entry), SEEK_SET)) {
      (*log) (Errors, NULL, "fseek: %s", strerror(errno));
      return RdFail;
    }

    if (1 != fread (&t64entry, sizeof t64entry, 1, file))
      goto freadFail;

    /* Convert the header. */
    memcpy (name.name, t64entry.name, 16);
    {
      int i;
      /* Convert trailing spaces to shifted spaces. */
      for (i = 16; --i && name.name[i] == ' '; name.name[i] = 0xA0);
    }
    fileoffset = (((long)t64entry.fileOffsetLowest) |
		  (t64entry.fileOffsetLower << 8) |
		  (t64entry.fileOffsetHigher << 16) |
		  (t64entry.fileOffsetHighest << 24));

    length = (((size_t)t64entry.endAddrLow | (t64entry.endAddrHigh << 8)) -
	      ((size_t)t64entry.startAddrLow | (t64entry.startAddrHigh << 8)));

    if (t64entry.entryType != 1 ||
	(t64entry.fileType != 1 && t64entry.fileType != PRG)) {
      (*log) (Errors, &name, "Unknown types %02x %02x, proceeding anyway",
	      t64entry.entryType, t64entry.fileType);
      /* continue; */
    }

    /* Read the file */
    {
      BYTE *buf;
      WrStatus status;
      size_t readlength;

      if (!(buf = malloc (length + 2))) {
	(*log) (Errors, &name, "Out of memory.");
	return RdFail;
      }

      buf[0] = t64entry.startAddrLow;
      buf[1] = t64entry.startAddrHigh;

      if (fseek (file, fileoffset, SEEK_SET)) {
	(*log) (Errors, &name, "fseek: %s", strerror(errno));
	free (buf);
	return RdFail;
      }

      if (length != (readlength = fread (&buf[2], 1, length, file))) {
	if (feof (file))
	  (*log) (Warnings, &name, "Truncated file, proceeding anyway");
	if (ferror (file)) {
	  (*log) (Errors, &name, "fread: %s", strerror(errno));
	  free (buf);
	  return RdFail;
	}
      }

      status = (*writeCallback) (&name, buf, readlength + 2);
      free (buf);

      switch (status) {
      case WrOK:
	break;
      case WrNoSpace:
	return RdNoSpace;
      case WrFail:
      default:
	return RdFail;
      }
    }
  }

  return RdOK;
}
