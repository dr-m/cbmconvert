/**
 * @file lynx.c
 * Lynx archive extractor and archiver
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
#include <string.h>
#include <errno.h>

#include "input.h"

/** maximal length of the BASIC header, if any */
#define MAXBASICLENGTH 1024

/** Read and convert a Lynx archive
 * @param file		the file input stream
 * @param filename	host system name of the file
 * @param writeCallback	function for writing the contained files
 * @param log		Call-back function for diagnostic output
 * @return		status of the operation
 */
enum RdStatus
ReadLynx (FILE* file,
	  const char* filename,
	  write_file_t writeCallback,
	  log_t log)
{
  struct Filename name;
  unsigned f, fcount;

  /* File positions */
  long headerPos; /* current header position */
  long headerEnd; /* end of header (start of archive) */
  long archivePos; /* current archive position */

  bool errNoLength = false; /* set if the file length is unknown */

  {
    byte_t* buf;
    int i;
    long length;

    if (!(buf = malloc (MAXBASICLENGTH))) {
    memError:
      (*log) (Errors, 0, "Out of memory.");
      return RdFail;
    }

    length = fread (buf, 1, MAXBASICLENGTH, file);

    if (fseek (file, 0, SEEK_SET)) {
    seekError:
      free (buf);
      (*log) (Errors, 0, "fseek: %s", strerror(errno));
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
      (*log) (Errors, 0, "Not a Lynx archive.");
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
      (*log) (Errors, 0, "Lynx header error.");
      return RdFail;
    }

    if (fseek (file, headerPos, 0)) {
      (*log) (Errors, 0, "fseek: %s", strerror(errno));
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
	    (*log) (Errors, 0, "Too long file name");
	    return RdFail;
	  }

	  name.name[i] = j;
	}

	if (j == 13) break;
      }

      if (!i) {
	(*log) (Warnings, 0, "blank file name");
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

	errNoLength = true;
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

	  errNoLength = true;
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
      byte_t* buf;
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
    (*log) (Warnings, 0, "The last file may be too long.");

  return RdOK;
}

/** Write an archive in Lynx format
 * @param archive	the archive to be written
 * @param filename	host file name of the archive file
 * @return		status of the operation
 */
enum ArStatus
ArchiveLynx (const struct Archive* archive,
	     const char* filename)
{
  FILE* f;
  struct ArchiveEntry* ae;
  int blockcounter;

  static const byte_t basichdr[] = {
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

  static const byte_t lynxhdr[] = "*LYNX BY CBMCONVERT 2.0*";

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
