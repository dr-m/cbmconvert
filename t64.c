/**
 * @file t64.c
 * T64 archive extractor
 * @author Marko Mäkelä (marko.makela@nic.funet.fi)
 */

/*
** Copyright © 1997,2001,2003 Marko Mäkelä
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
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "input.h"

/** A C64S T64 file header */
struct t64header
{
  /** Header identifier block (magic cookie) */
  byte_t headerblock[32];
  /** Minor version number */
  byte_t minorVersion;
  /** Major version number */
  byte_t majorVersion;
  /** Least significant byte of the maximum number of entries */
  byte_t maxEntriesLow;
  /** Most significant byte of the maximum number of entries */
  byte_t maxEntriesHigh;
  /** Least significant byte of the actual number of entries */
  byte_t numEntriesLow;
  /** Most significant byte of the actual number of entries */
  byte_t numEntriesHigh;
  /** Unused */
  byte_t padding[26];
};

/** A T64 directory entry */
struct t64entry
{
  /** 1 == normal file */
  byte_t entryType;
  /** Commodore file type (1 or $82 for PRG) */
  byte_t fileType;
  /** Least significant byte of the file's start address */
  byte_t startAddrLow;
  /** Most significant byte of the file's start address */
  byte_t startAddrHigh;
  /** Least significant byte of the file's end address */
  byte_t endAddrLow;
  /** Most significant byte of the file's end address */
  byte_t endAddrHigh;
  /** Unused */
  byte_t padding1[2];
  /** Least significant byte of the file offset */
  byte_t fileOffsetLowest;
  /** Less significant byte of the file offset */
  byte_t fileOffsetLower;
  /** More significant byte of the file offset */
  byte_t fileOffsetHigher;
  /** Most significant byte of the file offset */
  byte_t fileOffsetHighest;
  /** Unused */
  byte_t padding2[4];
  /** Commodore file name */
  byte_t name[16];
};

/** Read and convert a tape archive of the C64S emulator
 * @param file		the file input stream
 * @param filename	host system name of the file
 * @param writeCallback	function for writing the contained files
 * @param log		Call-back function for diagnostic output
 * @return		status of the operation
 */
enum RdStatus
ReadT64 (FILE* file,
	 const char* filename,
	 write_file_t writeCallback,
	 log_t log)
{
  unsigned numEntries, entry;

  /* Check the header. */

  {
    struct t64header t64header;
    unsigned maxEntries;

    static const char T64Header1[] = "C64 tape image file";
    static const char T64Header2[] = "C64S tape file";
    static const char T64Header3[] = "C64S tape image file";

    if (1 != fread (&t64header, sizeof t64header, 1, file)) {
    freadFail:
      (*log) (Errors, 0, "fread: %s", strerror (errno));
      return RdFail;
    }

    if (memcmp (t64header.headerblock, T64Header1, sizeof T64Header1 - 1) &&
	memcmp (t64header.headerblock, T64Header2, sizeof T64Header2 - 1) &&
	memcmp (t64header.headerblock, T64Header3, sizeof T64Header3 - 1)) {
      (*log) (Errors, 0, "Unknown T64 header");
      return RdFail;
    }

    if (t64header.majorVersion != 1 || t64header.minorVersion != 0)
      (*log) (Errors, 0, "Unknown T64 version, trying anyway");

    maxEntries = ((unsigned) t64header.maxEntriesLow |
		  (t64header.maxEntriesHigh << 8));
    numEntries = ((unsigned) t64header.numEntriesLow |
		  (t64header.numEntriesHigh << 8));

    if (!numEntries) {
      (*log) (Warnings, 0,
	      "Number of entries set to zero; trying to read the first entry");
      numEntries = 1;
    }
    else if (numEntries > maxEntries) {
      (*log) (Errors, 0, "Error in the number of entries");
      return RdFail;
    }

    (*log) (Everything, 0, "T64 version %u.%u, %u/%u files",
	    t64header.majorVersion, t64header.minorVersion,
	    numEntries, maxEntries);
  }

  /* Process the files */

  for (entry = 0; entry < numEntries; entry++) {
    struct t64entry t64entry;
    struct Filename name;
    long fileoffset;
    size_t length;

    name.type = PRG;
    name.recordLength = 0;

    if (fseek (file,
	       sizeof (struct t64header) + entry * sizeof t64entry,
	       SEEK_SET)) {
      (*log) (Errors, 0, "fseek: %s", strerror (errno));
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
    fileoffset =
      (long) t64entry.fileOffsetLowest |
      t64entry.fileOffsetLower << 8 |
      t64entry.fileOffsetHigher << 16 |
      t64entry.fileOffsetHighest << 24;

    length =
      (((size_t) t64entry.endAddrLow | t64entry.endAddrHigh << 8) -
       ((size_t) t64entry.startAddrLow | t64entry.startAddrHigh << 8)) &
      0xFFFF;

    if (t64entry.entryType != 1) {
    unknown:
      (*log) (Errors, &name,
	      "Unknown entry type 0x%02x 0x%02x, assuming PRG",
	      t64entry.entryType, t64entry.fileType);
    }
    else if (t64entry.fileType != 1) {
      unsigned filetype = t64entry.fileType & 0x8F;
      if (filetype >= DEL && filetype <= USR)
	name.type = filetype;
      else
	goto unknown;
    }

    /* Read the file */
    {
      byte_t* buf;
      enum WrStatus status;
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
