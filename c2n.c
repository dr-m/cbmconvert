/**
 * @file c2n.c
 * Commodore C2N archive extractor and archiver
 * @author Marko Mäkelä (marko.makela@nic.funet.fi)
 */

/*
** Copyright © 2001 Marko Mäkelä
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
#include "output.h"

/** Commodore C2N tape header */
struct c2n_header
{
  /** Header identifier tag */
  byte_t tag;
  /** Least significant byte of the start address of the following block */
  byte_t startAddrLow;
  /** Most significant byte of the start address of the following block */
  byte_t startAddrHigh;
  /** Least significant byte of the end address of the following block */
  byte_t endAddrLow;
  /** Most significant byte of the end address of the following block */
  byte_t endAddrHigh;
  /** Commodore file name, padded with spaces */
  byte_t filename[16];
  /** Unused, padded with spaces */
  byte_t padding[171];
};

/** Commodore C2N tape header identifier tags */
enum c2n_tag
{
  tBasic = 1,      /**< relocatable (BASIC) program */
  tDataBlock = 2,  /**< actual data block of a data file */
  tML = 3,         /**< absolute program */
  tDataHeader = 4, /**< data file header */
  tEnd = 5         /**< end-of-tape header */
};

/** copy a file name from tape header
 * and convert trailing spaces to trailing shifted spaces
 * @param header	the tape header
 * @param name		the file name
 */
static void
header2name (const struct c2n_header* header,
	     struct Filename* name)
{
  unsigned char* c;
  memcpy (name->name, header->filename, sizeof header->filename);
  for (c = name->name + sizeof name->name; c-- > name->name; *c = 0xa0)
    if (*c != 0x20)
      break;
}

/** copy a file name to tape header
 * and convert trailing shifted spaces to trailing spaces
 * @param name		the file name
 * @param header	the tape header
 */
static void
name2header (const struct Filename* name,
	     struct c2n_header* header)
{
  register byte_t* c;
  /* fill the header with spaces */
  memset (header, ' ', sizeof *header);
  /* copy the name */
  memcpy (header->filename, name->name, sizeof header->filename);
  /* convert trailing shifted spaces to spaces */
  for (c = header->filename + sizeof header->filename;
       c-- > header->filename; *c = 0x20)
    if (*c != 0xa0)
      break;
}

/** Read and convert a Commodore C2N tape archive
 * @param file		the file input stream
 * @param filename	host system name of the file
 * @param writeCallback	function for writing the contained files
 * @param log		Call-back function for diagnostic output
 * @return		status of the operation
 */
enum RdStatus
ReadC2N (FILE* file,
	 const char* filename,
	 write_file_t writeCallback,
	 log_t log)
{
  /** name of the file being processed */
  struct Filename name;
  /* clear the file type code (to denote uninitialized file name) */
  name.type = 0;
  /* clear the record length (no relative files on tapes) */
  name.recordLength = 0;

  while (!feof (file)) {
    /** tape header */
    struct c2n_header header;
    /** start address of the file being processed */
    unsigned start;
    /** end address of the file being processed */
    unsigned end;

    if (1 != fread (&header, sizeof header, 1, file)) {
      if (feof (file))
	break;
      (*log) (Errors, name.type ? &name : 0, "fread: %s", strerror (errno));
      return RdFail;
    }

  nextHeader:
    start = (unsigned) header.startAddrLow | header.startAddrHigh << 8;
    end = (unsigned) header.endAddrLow | header.endAddrHigh << 8;

    switch (header.tag) {
    case tBasic:
    case tML:
      header2name (&header, &name);
      name.type = PRG;
      if ((header.tag == tBasic && header.startAddrLow != 1) ||
	  start >= end)
	(*log) (Warnings, &name, "Suspicious addresses 0x%04x..0x%04x",
		start, end);
      break;
    case tDataHeader:
      header2name (&header, &name);
      name.type = SEQ;
      if (start != 0x33c || end != 0x3fc)
	(*log) (Warnings, &name,
		"Suspicious addresses 0x%04x..0x%04x (expected 0x33c..0x3fc)",
		start, end);
      if ((byte_t) (end - start) != 192)
	(*log) (Warnings, name.type ? &name : 0,
		"Block length differs from 192");
      break;
    case tEnd:
      header2name (&header, &name);
      name.type = DEL;
      (*log) (Everything, &name, "Ignoring end-of-tape marker");
      continue;
    default:
      (*log) (Errors, name.type ? &name : 0,
	      "Unknown C2N header code 0x%02x", header.tag);
      return RdFail;
    }

    if (name.type == SEQ) {
      /** number of bytes read so far */
      size_t length = 0;
      /** the data buffer */
      byte_t* buf = 0;

    nextBlock:
      if (1 != fread (&header, sizeof header, 1, file)) {
	if (feof (file))
	  goto writeData;
	(*log) (Errors, &name, "fread: %s", strerror (errno));
	free (buf);
	return RdFail;
      }

      if (header.tag == tDataBlock) {
	byte_t* b = realloc (buf, length + (sizeof header) - 1);
	if (!b) {
	  (*log) (Errors, &name, "Out of memory.");
	  free (buf);
	  return RdFail;
	}
	buf = b;
	memcpy (buf + length, ((byte_t*) &header) + 1, (sizeof header) - 1);
	length += (sizeof header) - 1;
	goto nextBlock;
      }
      else {
	enum WrStatus status;
      writeData:
	if (!length)
	  (*log) (Warnings, &name, "no data");
	status = (*writeCallback) (&name, buf, length);
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

	if (!feof (file))
	  goto nextHeader;
      }
    }
    else {
      byte_t* buf;
      enum WrStatus status;
      size_t readlength, length = (end - start) & 0xffff;

      if (!(buf = malloc (length + 2))) {
	(*log) (Errors, &name, "Out of memory.");
	return RdFail;
      }

      buf[0] = header.startAddrLow;
      buf[1] = header.startAddrHigh;

      if (length != (readlength = fread (&buf[2], 1, length, file))) {
	if (feof (file))
	  (*log) (Warnings, &name, "Truncated file, proceeding anyway");
	if (ferror (file)) {
	  (*log) (Errors, &name, "fread: %s", strerror (errno));
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

/** Write an archive in Commodore C2N tape format
 * @param archive	the archive to be written
 * @param filename	host file name of the archive file
 * @return		status of the operation
 */
enum ArStatus
ArchiveC2N (const struct Archive* archive,
	    const char* filename)
{
  FILE* f;
  struct ArchiveEntry* ae;

  if (!(f = fopen (filename, "wb")))
    return errno == ENOSPC ? ArNoSpace : ArFail;

  for (ae = archive->first; ae; ae = ae->next) {
    /** tape header */
    struct c2n_header header;
    name2header (&ae->name, &header);

    if (ae->name.type == PRG) {
      unsigned end = ae->length;
      if (end < 2)
	continue; /* too short file */

      header.startAddrLow = ae->data[0];
      header.startAddrHigh = ae->data[1];
      end += (unsigned) header.startAddrLow | (header.startAddrHigh << 8);
      header.endAddrLow = end -= 2;
      header.endAddrHigh = end >> 8;
      header.tag = header.startAddrLow == 1 ? tBasic : tML;

      if (1 != fwrite (&header, sizeof header, 1, f) ||
	  ae->length - 2 != fwrite (ae->data + 2, 1, ae->length - 2, f)) {
      fail:
	fclose (f);
	return ArFail;
      }
    }
    else {
      /* convert anything else than programs to data files */
      /** data file header */
      static const byte_t dataheader[] = { tDataHeader, 0x3c, 3, 0xfc, 3 };
      /** number of bytes written */
      unsigned cnt = 0;
      memcpy (&header, dataheader, sizeof dataheader);
      if (1 != fwrite (&header, sizeof header, 1, f))
	goto fail;
      do {
	unsigned next = cnt + (sizeof header) - 1;
	header.tag = tDataBlock;
	if (next > ae->length) {
	  memcpy (&header.startAddrLow, ae->data + cnt, ae->length - cnt);
	  (&header.startAddrLow)[ae->length - cnt] = 0;
	}
	else
	  memcpy (&header.startAddrLow, ae->data + cnt, (sizeof header) - 1);
	if (1 != fwrite (&header, sizeof header, 1, f))
	  goto fail;
	cnt = next;
      } while (cnt < ae->length);
    }
  }

  fclose (f);
  return ArOK;
}
