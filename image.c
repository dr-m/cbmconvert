/*
** $Id$
**
** Disk image management
**
** Copyright © 1993-1998 Marko Mäkelä
** 1581 disk image management by Pasi Ojala
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
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

#include "output.h"
#include "input.h"

/* Disk geometry information structure */
typedef struct {
  ImageType type; /* disk image type identifier */
  size_t blocks; /* disk image size */
  char formatID; /* format specifier */
  unsigned BAMblocks; /* number of BAM blocks */
  unsigned dirtrack; /* directory track number */
  unsigned tracks; /* number of disk tracks */
  unsigned *sectors; /* number of sectors per track */
  unsigned *interleave; /* sector interleaves (number of sectors to advance) */
} DiskGeometry;

/* Disk directory entry */
typedef struct {
  BYTE nextTrack;	/* pointer to next directory block */
  BYTE nextSector;	/* (only available in the first dirent of the block) */
  BYTE type;		/* file type */
  BYTE firstTrack;	/* pointer to first */
  BYTE firstSector;	/* file data block */
  BYTE name[16];	/* file name */
  BYTE ssTrack;		/* pointer to first side sector block */
  BYTE ssSector;	/* (relative files only) */
  BYTE recordLength;	/* relative file record length */
#define infoTrack ssTrack	/* info block track of GEOS file */
#define infoSector ssSector	/* info block sector of GEOS file */
#define isVLIR recordLength	/* GEOS file format: 1=VLIR, 0=sequential */
  struct {
    BYTE type;		/* GEOS file type */
    BYTE year;		/* GEOS file time stamp: year */
    BYTE month;		/* GEOS file time stamp: month */
    BYTE day;		/* GEOS file time stamp: day */
    BYTE hour;		/* GEOS file time stamp: hour */
    BYTE minute;	/* GEOS file time stamp: minute */
  } geos;
  BYTE blocksLow;	/* file's total block count, low byte */
  BYTE blocksHigh;	/* file's total block count, high byte */
} DirEnt;

/* CP/M disk directory entry */
typedef struct {
  BYTE area; /* user area 0-0xF or 0xE5 (unused entry) */
  BYTE basename[8];
  /* basename[0]..basename[3]: bit7=1 for user defined attributes 1 to 4 */
  /* basename[4]..basename[7]: bit7=1 for system interface attributes (BDOS) */
  BYTE suffix[3]; /* file suffix */
  /* suffix[0]: bit7=1 for read-only */
  /* suffix[1]: bit7=1 for system file */
  /* suffix[2]: bit7=1 for archive */
  BYTE extent; /* number of directory extent */
  BYTE unused[2]; /* wasted bytes */
  BYTE blocks;
  /* number of 128-byte blocks in this extent (max $80) */
  BYTE block[16];
  /* file block pointers, in this case 8-bit */
} CpmDirEnt;

#define CPMBLOCK(block,i) \
	(au == 8 ? block[i] : block[2 * (i)] + (block[2 * (i) + 1] << 8))

/* sectors per track */
static unsigned sect1541[] = {
  21, 21, 21, 21, 21, 21, 21, 21, 21, /* tracks  1 .. 9  */
  21, 21, 21, 21, 21, 21, 21, 21,     /* tracks 10 .. 17 */
  19, 19, 19, 19, 19, 19, 19,         /* tracks 18 .. 24 */
  18, 18, 18, 18, 18, 18,             /* tracks 25 .. 30 */
  17, 17, 17, 17, 17                  /* tracks 31 .. 35 */
};

static unsigned sect1571[] = {
  21, 21, 21, 21, 21, 21, 21, 21, 21, /* tracks  1 .. 9  */
  21, 21, 21, 21, 21, 21, 21, 21,     /* tracks 10 .. 17 */
  19, 19, 19, 19, 19, 19, 19,         /* tracks 18 .. 24 */
  18, 18, 18, 18, 18, 18,             /* tracks 25 .. 30 */
  17, 17, 17, 17, 17,                 /* tracks 31 .. 35 */
  21, 21, 21, 21, 21, 21, 21, 21, 21, /* tracks 36 .. 44 */
  21, 21, 21, 21, 21, 21, 21, 21,     /* tracks 45 .. 52 */
  19, 19, 19, 19, 19, 19, 19,         /* tracks 53 .. 59 */
  18, 18, 18, 18, 18, 18,             /* tracks 60 .. 65 */
  17, 17, 17, 17, 17                  /* tracks 66 .. 70 */
};

static unsigned sect1581[] = {
  40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
  40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
  40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
  40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
  40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
  40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
  40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
  40, 40, 40, 40, 40, 40, 40, 40, 40, 40
};

/* interleave per track */
static unsigned int1541[] = {
  10, 10, 10, 10, 10, 10, 10, 10, 10, /* tracks  1 .. 9  */
  10, 10, 10, 10, 10, 10, 10, 10,     /* tracks 10 .. 17 */
   3, 10, 10, 10, 10, 10, 10,         /* tracks 18 .. 24 */
  10, 10, 10, 10, 10, 10,             /* tracks 25 .. 30 */
  10, 10, 10, 10, 10                  /* tracks 31 .. 35 */
};

static unsigned int1571[] = {
  10, 10, 10, 10, 10, 10, 10, 10, 10, /* tracks  1 .. 9  */
  10, 10, 10, 10, 10, 10, 10, 10,     /* tracks 10 .. 17 */
   3, 10, 10, 10, 10, 10, 10,         /* tracks 18 .. 24 */
  10, 10, 10, 10, 10, 10,             /* tracks 25 .. 30 */
  10, 10, 10, 10, 10,                 /* tracks 31 .. 35 */
  10, 10, 10, 10, 10, 10, 10, 10, 10, /* tracks 36 .. 44 */
  10, 10, 10, 10, 10, 10, 10, 10,     /* tracks 45 .. 52 */
   3, 10, 10, 10, 10, 10, 10,         /* tracks 53 .. 59 */
  10, 10, 10, 10, 10, 10,             /* tracks 60 .. 65 */
  10, 10, 10, 10, 10                  /* tracks 66 .. 70 */
};

static unsigned int1581[] = {
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};

/*
** The disk geometry database.  If you add more drives, you will need
** to update the ImageType definition in output.h and all related
** switch statements in this file.
*/
static DiskGeometry diskGeometry[] = {
  {
    Im1541,
    683,
    'A',
    1,
    18,
    elementsof(sect1541),
    sect1541 - 1,
    int1541 - 1
  },
  {
    Im1571,
    1366,
    'A',
    1,
    18,
    elementsof(sect1571),
    sect1571 - 1,
    int1571 - 1
  },
  {
    Im1581,
    3200,
    'D',
    1,		/* BAM blocks are in a separate chain */
    40,
    elementsof(sect1581),
    sect1581 - 1,
    int1581 - 1
  },
};

static const DiskGeometry *getGeometry (ImageType type);
static void FormatImage (Image *image);
static BYTE *getBlock (Image *image, BYTE track, BYTE sector);

static size_t mapInode (BYTE ***buf, Image *image, BYTE track, BYTE sector);
static size_t readInode (BYTE **buf, const Image *image,
			 BYTE track, BYTE sector);
static WrStatus writeInode (Image *image, BYTE track, BYTE sector,
			    const BYTE *buf, size_t size);
static ImStatus deleteInode (Image *image, BYTE track, BYTE sector,
			     bool do_it);

#ifdef DEBUG
static bool isValidDirEnt (const Image *image, const DirEnt *dirent);
#endif

static bool isGeosDirEnt (const DirEnt *dirent);

static DirEnt *getDirEnt (Image *image, const Filename *name);
static Filetype getFiletype (const Image *image, const DirEnt *dirent);
static ImStatus deleteDirEnt (Image *image, DirEnt *dirent);

static WrStatus setupSideSectors (Image *image, DirEnt *dirent, size_t blocks);
static bool checkSideSectors (const Image *image, const DirEnt *dirent);

static bool allocBlock (Image *image, BYTE *track, BYTE *sector);
static bool freeBlock (Image *image, BYTE track, BYTE sector);
static bool isFreeBlock (const Image *image, BYTE track, BYTE sector);
static bool findNextFree (const Image *image, BYTE *track, BYTE *sector);
static int blocksFree (const Image *image);
static bool backupBAM (const Image *image, BYTE **BAM);
static bool restoreBAM (Image *image, BYTE **BAM);

static BYTE **CpmTransTable (Image *image, unsigned *au, unsigned *sectors);
static void CpmConvertName (const CpmDirEnt *dirent, Filename *name);

/* Write a file into a CP/M disk image. */
WrStatus WriteCpmImage (const Filename *name, const BYTE *data,
			size_t length, Image *image, LogCallback *log)
{
  BYTE **trans;
  CpmDirEnt **allocated, *dirent, cpmname;
  unsigned au; /* allocation unit size */
  unsigned sectors; /* number of disk sectors */
  unsigned slot; /* next directory slot */
  unsigned blocksfree; /* number of free blocks */

  if (!name || !data || !image || !image->buf ||
      !(trans = CpmTransTable (image, &au, &sectors)))
    return WrFail;

  if (!(allocated = calloc (2 * sectors / au, sizeof(*allocated)))) {
    free (trans);
    return WrFail;
  }

  if (!(dirent = malloc (au * 8 * sizeof (*dirent)))) {
    free (allocated);
    free (trans);
    return WrFail;
  }

  blocksfree = 2 * (sectors / au - 1);

  /* Convert the file name */
  {
    int i;

    memset (&cpmname, 0, sizeof cpmname);
    memset (cpmname.basename, ' ',
	    sizeof cpmname.basename + sizeof cpmname.suffix);

    /* Convert the file name base */

    for (i = 0; i < sizeof(name->name) &&
	   i < sizeof(cpmname.basename) &&
	   !memchr (".\240", name->name[i], 3);
	 i++)
      if (i && name->name[i] == ' ') /* stop at the first space */
	break;
      else if (name->name[i] >= 0x41 && name->name[i] <= 0x5A)
	cpmname.basename[i] = name->name[i] + 'A' - 0x41; /* upper case only */
      else if (name->name[i] >= 0xC1 && name->name[i] <= 0xDA)
	cpmname.basename[i] = name->name[i] + 'A' - 0xC1;
      else if ((name->name[i] & 0x7F) < 32 || name->name[i] == ' ')
	cpmname.basename[i] = '-'; /* control chars and space */
      else if (name->name[i] < 127)
	cpmname.basename[i] = name->name[i];
      else
	cpmname.basename[i] = '+'; /* graphics characters */

    /* Convert the file name suffix */

    if (name->name[i] != ' ' && ++i < sizeof(name->name)) {
      unsigned j;

      for (j = 0; j < sizeof(cpmname.suffix) && i < sizeof(name->name);
	   i++, j++)
	if ((name->name[i] & 0x7F) == ' ') /* stop at the first space */
	  break;
	else if (name->name[i] >= 0x41 && name->name[i] <= 0x5A)
	  cpmname.suffix[j] = name->name[i] + 'A' - 0x41; /* upper case only */
	else if (name->name[i] >= 0xC1 && name->name[i] <= 0xDA)
	  cpmname.suffix[j] = name->name[i] + 'A' - 0xC1;
	else if ((name->name[i] & 0x7F) < 32)
	  cpmname.suffix[j] = '-'; /* control chars */
	else if (name->name[i] < 127)
	  cpmname.suffix[j] = name->name[i];
	else
	  cpmname.suffix[j] = '+'; /* graphics characters */
    }
  }

  /* Traverse through the directory and determine the amount and
     location of free blocks */
  {
    unsigned d, i;
    bool found = false;

    /* Read the directory entries */
    for (d = au; d--; )
      memcpy (&dirent[d * 8], trans[d], 8 * sizeof (*dirent));

    for (d = slot = 0; d < au * 8; d++) {
      if (dirent[d].area == 0xE5 ||
	  !memcmp (&dirent[d], "\0\0\0\0\0\0\0\0\0\0\0", 12))
	continue;

      if (!memcmp (dirent[d].basename, cpmname.basename,
		   sizeof cpmname.basename + sizeof cpmname.suffix)) {
	if (image->direntOpts == DirEntOnlyCreate) {
	  free (dirent);
	  free (allocated);
	  free (trans);
	  return WrFileExists;
	}

	found = true;
	continue; /* overwrite the file */
      }

      if (d != slot)
	memcpy (&dirent[slot], &dirent[d], (au * 8 - d) * sizeof(*dirent));

      d = slot++;

      for (i = 0; i < rounddiv(dirent[d].blocks, au); i++)
	if (CPMBLOCK(dirent[d].block, i) < 2 ||
	    CPMBLOCK(dirent[d].block, i) >= 2 * sectors / au) {
	  Filename fn;
	  CpmConvertName (&dirent[d], &fn);
	  (*log) (Warnings, &fn,
		  "Illegal block address in block %u of extent 0x%02x",
		  i, dirent[d].extent);
	}
	else if (allocated[CPMBLOCK(dirent[d].block, i)]) {
	  Filename fn;
	  CpmConvertName (&dirent[d], &fn);
	  (*log) (Warnings, &fn, "Sector 0x%02x allocated multiple times",
		  CPMBLOCK(dirent[d].block, i));
	}
	else {
	  allocated[CPMBLOCK(dirent[d].block, i)] = &dirent[d];
	  blocksfree--;
	}
    }

    /* See if the file was found */
    if (!found && image->direntOpts == DirEntDontCreate) {
      free (dirent);
      free (allocated);
      free (trans);
      return WrFail;
    }

    /* Clear the empty directory entries */
    memset (&dirent[slot], 0xE5, (au * 8 - slot) * sizeof(*dirent));

    /* Ensure that enough free space is available */

    if (slot >= 8 * au ||
	length > (8 * au - slot) * au / 2 * 16 * 128 ||
	length > blocksfree * au * 128) {
      free (dirent);
      free (allocated);
      free (trans);
      return WrNoSpace;
    }
  }

  /* Write the file */

  {
    CpmDirEnt *de = NULL;
    unsigned block, blocks;
    unsigned freeblock = 2;

    for (block = 0, blocks = rounddiv(length, 128); blocks;) {
      if (!(block % 128)) { /* advance to next directory slot */
	de = &dirent[slot++];
	memcpy (de, &cpmname, sizeof cpmname);
	de->extent = block / 128;
      }

      /* Copy the blocks */

      {
	unsigned j;

	de->blocks = blocks < 128 ? blocks : 128;
	blocks -= de->blocks;

	for (j = 0; j < de->blocks; j++, block++) {
	  if (!(j % au)) {
	    unsigned k;
	    /* Get next free block */
	    while (allocated[freeblock]) freeblock++;
	    allocated[freeblock] = de;
	    if (au == 8)
	      de->block[j / au] = freeblock;
	    else {
	      de->block[(j / au) * 2] = freeblock & 0xFF;
	      de->block[(j / au) * 2 + 1] = freeblock >> 8;
	    }
	    /* Pad it with ^Z */
	    for (k = 0; k < au / 2; k++)
	      memset (trans[(au / 2) * freeblock + k], 0x1A, 256);
	  }

	  /* Copy the block */
	  memcpy (trans[(au / 2) * freeblock + ((j / 2) % (au / 2))] +
		  128 * (j % 2), data + 128 * block,
		  length >= 128 * (block + 1) ? 128 : length - 128 * block);
	}
      }
    }
  }

  /* Write the directory entries */
  {
    int d = au;
    while (d--)
      memcpy (trans[d], &dirent[d * 8], 8 * sizeof (*dirent));
  }

  free (dirent);
  free (allocated);
  free (trans);
  return WrOK;
}

/* Read all CP/M files from a disk image. */
RdStatus ReadCpmImage (FILE *file, const char *filename,
		       WriteCallback *writeCallback, LogCallback *log)
{
  Image image;
  BYTE **trans;
  unsigned au; /* allocation unit size */
  unsigned sectors; /* number of disk sectors */

  /* determine disk image type from its length */
  {
    const DiskGeometry *geom = NULL;
    size_t length, blocks;
    unsigned i;

    if (fseek (file, 0, SEEK_END)) {
    seekError:
      (*log) (Errors, NULL, "fseek: %s", strerror(errno));
      return RdFail;
    }

    length = ftell (file);

    if (fseek (file, 0, SEEK_SET))
      goto seekError;

    if (length % 256) {
    unknownImage:
      (*log) (Errors, NULL, "Unknown CP/M disk image type");
      return RdFail;
    }

    for (i = 0, blocks = length / 256; i < elementsof(diskGeometry); i++)
      if (diskGeometry[i].blocks == blocks) {
	geom = &diskGeometry[i];
	break;
      }

    if (!geom)
      goto unknownImage;

    /* Initialize the disk image structure. */

    if (!(image.buf = malloc (length))) {
      (*log) (Errors, NULL, "Out of memory");
      return RdFail;
    }

    image.type = geom->type;
    image.dirtrack = geom->dirtrack;
    image.name = NULL;

    if (1 != fread (image.buf, length, 1, file)) {
      (*log) (Errors, NULL, "fread: %s", strerror(errno));
      free (image.buf);
      return RdFail;
    }

    /* Get the CP/M sector translations. */

    if (!(trans = CpmTransTable (&image, &au, &sectors))) {
      free (image.buf);
      goto unknownImage;
    }
  }

  /* Traverse through the directory and extract the files */
  {
    CpmDirEnt *directory;
    unsigned d;
    Filename name;

    for (d = 0; d < au * 8; d++) {
      unsigned i, j, length;

      directory = ((CpmDirEnt*)trans[d / 8]) + (d % 8);

      if (directory->area == 0xE5) continue; /* unused entry */

      CpmConvertName (directory, &name);

      if (directory->extent) {
	(*log) (Warnings, &name,
		"starting with non-zero extent 0x%02x, file ignored",
		directory->extent);
	continue;
      }

      /* search for following extents */
      for (i = d, j = length = 0; i < au * 8; i++) {
	CpmDirEnt *dir = ((CpmDirEnt*)trans[i / 8]) + (i % 8);

	if (memcmp (dir, directory, 12) ||
	    dir->extent != j || dir->blocks > 128)
	  break;

	j++;
	length += dir->blocks;

	if (dir->blocks < 128)
	  break;
      }

      /* j holds the number of directory extents */

      if (!j) {
	(*log) (Warnings, &name, "error in directory entry, file skipped");
	continue;
      }

      if (directory->area)
	(*log) (Warnings, &name, "user area code 0x%02x ignored",
		directory->area);

      length *= 128;

      /* Read the file */
      {
	BYTE *curr, *buf = malloc (length);

	if (!buf) {
	  (*log) (Warnings, &name, "out of memory");
	  d += j - 1;
	  continue;
	}

	for (curr = buf, j += d; d < j; d++) {
	  directory = ((CpmDirEnt*)trans[d / 8]) + (d % 8);

	  for (i = 0; i < directory->blocks; i++) {
	    unsigned sect = (au / 2) * CPMBLOCK(directory->block, i / au) +
	      ((i / 2) % (au / 2));

	    if (sect >= sectors) {
	      (*log) (Errors, &name,
		      "Illegal block address in block %u of extent 0x%02x",
		      i, directory->extent);
	      free (buf);
	      goto FileDone;
	    }

	    memcpy (curr, trans[sect] + 128 * (i % 2), 128);
	    curr += 128;
	  }
	}

	/* Remove trailing EOF characters (only when they are at end of the
	   last block). */
	while (buf[length - 1] == 0x1A) length--;

	switch ((*writeCallback) (&name, buf, length)) {
	case WrOK:
	  break;
	case WrNoSpace:
	  free (buf);
	  free (image.buf);
	  free (trans);
	  return RdNoSpace;
	case WrFail:
	default:
	  free (buf);
	  free (image.buf);
	  free (trans);
	  return RdFail;
	}

	free (buf);
      }

    FileDone:
      d--;
    }
  }

  free (image.buf);
  free (trans);

  return RdOK;
}

/* Write a file into a disk image. */
WrStatus WriteImage (const Filename *name, const BYTE *data,
		     size_t length, Image *image, LogCallback *log)
{
  DirEnt *dirent;

  if (!name || !data || !image || !image->buf || !getGeometry (image->type))
    return WrFail;

  /* See if it is a GEOS file. */
  if (name->type >= DEL && name->type < REL && length > 2 * 254 &&
      !strncmp ((char*)&data[sizeof(DirEnt) + 1],
		" formatted GEOS file ", 21)) {
    size_t len;
    Filename geosname;
    const BYTE *info = &data[254];
    dirent = (DirEnt*)&data[-2];

    /* Read the name from the directory entry. */
    memcpy (geosname.name, dirent->name, sizeof geosname.name);
    geosname.type = getFiletype (image, dirent);
    geosname.recordLength = 0;

    if (!isGeosDirEnt (dirent) || memcmp (info, "\3\25\277", 3))
      goto notGEOS;

    if (dirent->isVLIR) {
      const BYTE *vlir = &data[2 * 254];
      unsigned vlirblock;
      len = 3 * 254;

      for (vlirblock = 0; vlirblock < 127; vlirblock++) {
	unsigned blocks = vlir[2 * vlirblock];
	unsigned lastblocklen = vlir[2 * vlirblock + 1];

	if (!blocks) {
	  if (lastblocklen != 0 && lastblocklen != 0xFF)
	    goto notGEOS;
	}
	else if (lastblocklen < 2)
	  goto notGEOS;
	else
	  len = 254 * (rounddiv(len, 254) + blocks - 1) + lastblocklen - 1;
      }

      if (len > length) {
	(*log) (Warnings, &geosname, "%d bytes too short file", len - length);
	goto notGEOS;
      }
    }
    else
      len = length;

    if ((info[0x42] ^ dirent->type) & 0x8F)
      (*log) (Warnings, &geosname, "file types differ: $%02x $%02x",
	      info[0x42], dirent->type);

    if (info[0x43] != dirent->geos.type)
      (*log) (Warnings, &geosname, "GEOS file types differ: $%02x $%02x",
	      info[0x43], dirent->geos.type);

    if (info[0x44] != dirent->isVLIR)
      (*log) (Warnings, &geosname, "VLIR flags differ: $%02x $%02x",
	      info[0x44], dirent->isVLIR);

    if (len != length)
      (*log) (Warnings, &geosname, "File size mismatch: %d extraneous bytes",
	      length - len);

    if (rounddiv(len, 254) - 1 !=
	dirent->blocksLow + (dirent->blocksHigh << 8)) {
      unsigned blks = rounddiv(len, 254) - 1;
      dirent->blocksLow = blks & 0xFF;
      dirent->blocksHigh = blks >> 8;
      (*log) (Warnings, &geosname, "invalid block count");
    }

    dirent = getDirEnt (image, &geosname);

    if (!dirent)
      return WrNoSpace;

    if (dirent->type) {
      if (image->direntOpts == DirEntOnlyCreate)
	return WrFileExists;

      /* delete the old file */
      if (ImOK != deleteDirEnt (image, dirent)) {
	(*log) (Errors, &geosname, "Could not delete existing file.");
	return WrFail;
      }
    }

    if (blocksFree (image) < rounddiv(len, 254) - 1)
      return WrNoSpace;

    /* set the directory entry parameters */
    memcpy ((BYTE*)dirent + 2, data, sizeof(DirEnt) - 2);
    dirent->type = 0;
    dirent->firstTrack = 0;
    dirent->firstSector = 0;
    dirent->infoTrack = image->dirtrack + 1;
    dirent->infoSector = 0;

    {
      BYTE *oldBAM = NULL;
      WrStatus status;

      /* back up the old BAM */

      if (!backupBAM (image, &oldBAM)) {
	(*log) (Errors, name, "Backing up the BAM failed.");
	return WrFail;
      }

      if (!findNextFree (image, &dirent->infoTrack, &dirent->infoSector))
	return WrNoSpace;

      status = writeInode (image, dirent->infoTrack, dirent->infoSector,
			   &data[254], 254);

      if (status != WrOK) {
	restoreBAM (image, &oldBAM);
	(*log) (Errors, &geosname, "Writing the info sector failed.");
	return status;
      }

      if (dirent->isVLIR) {
	BYTE vlir[254];
	const BYTE *vlirsrc = &data[254 * 2];
	unsigned vlirblock;
	const BYTE *buf = &data[254 * 3];
	BYTE track = dirent->infoTrack;
	BYTE sector = dirent->infoSector;

	memcpy (vlir, vlirsrc, 254);

	for (vlirblock = 0; vlirblock < 127; vlirblock++) {
	  unsigned blocks = vlirsrc[2 * vlirblock];
	  unsigned lastblocklen = vlirsrc[2 * vlirblock + 1];

	  if (blocks) {
	    if (!findNextFree (image, &track, &sector)) {
	      restoreBAM (image, &oldBAM);
	      return WrNoSpace;
	    }

	    vlir[vlirblock * 2] = track;
	    vlir[vlirblock * 2 + 1] = sector;

	    len = 254 * (blocks - 1) + lastblocklen - 1;
	    status = writeInode (image, track, sector, buf, len);

	    if (status != WrOK) {
	      restoreBAM (image, &oldBAM);
	      (*log) (Errors, &geosname, "Writing a VLIR node failed.");
	      return status;
	    }

	    buf += 254 * blocks;
	  }
	}

	dirent->firstTrack = dirent->infoTrack;
	dirent->firstSector = dirent->infoSector;

	if (!findNextFree (image, &dirent->firstTrack, &dirent->firstSector)) {
	  restoreBAM (image, &oldBAM);
	  return WrNoSpace;
	}

	status = writeInode (image, dirent->firstTrack, dirent->firstSector,
			     vlir, 254);

	if (status != WrOK) {
	  restoreBAM (image, &oldBAM);
	  (*log) (Errors, &geosname, "Writing the VLIR block failed.");
	  return status;
	}
      }
      else {
	dirent->firstTrack = dirent->infoTrack;
	dirent->firstSector = dirent->infoSector;

	if (!findNextFree (image, &dirent->firstTrack, &dirent->firstSector)) {
	  restoreBAM (image, &oldBAM);
	  return WrNoSpace;
	}

	status = writeInode (image, dirent->firstTrack, dirent->firstSector,
			     &data[254 * 2], length - 254 * 2);

	if (status != WrOK) {
	  restoreBAM (image, &oldBAM);
	  (*log) (Errors, &geosname, "Writing the data sectors failed.");
	  return status;
	}
      }

      free (oldBAM);
    }

    dirent->type = *data;
    return WrOK;

  notGEOS:
    (*log) (Warnings, name, "not a valid GEOS (Convert) file");
  }

  dirent = getDirEnt (image, name);

  if (!dirent)
    return WrNoSpace;

  if (dirent->type) {
    if (image->direntOpts == DirEntOnlyCreate)
      return WrFileExists;

    /* delete the old file */
    if (ImOK != deleteDirEnt (image, dirent)) {
      (*log) (Errors, name, "Could not delete existing file.");
      return WrFail;
    }
  }

  /* Check that there is enough space for the file. */

  if (blocksFree (image) < rounddiv(length, 254) +
      (name->type == REL ? rounddiv(rounddiv(length, 254), 120) : 0))
    return WrNoSpace;

  /* set the file name */
  memcpy (dirent->name, name->name, 16);
  /* set the track and sector of the file */
  dirent->firstTrack = image->dirtrack + 1;
  dirent->firstSector = 0;

  if (!findNextFree (image, &dirent->firstTrack, &dirent->firstSector))
    return WrNoSpace;

  {
    unsigned blocks;
    BYTE *oldBAM = NULL;
    WrStatus status;

    /* set the block count */
    blocks = rounddiv(length, 254);

    if (name->type == REL) {
      /* set the record length for relative files */
      dirent->recordLength = name->recordLength;

      /* adjust the block count */
      blocks += rounddiv(blocks, 120);
    }

    dirent->blocksLow = blocks & 0xFF;
    dirent->blocksHigh = blocks >> 8;

    /* back up the old BAM */

    if (!backupBAM (image, &oldBAM)) {
      (*log) (Errors, name, "Backing up the BAM failed.");
      return WrFail;
    }

    status = writeInode (image, dirent->firstTrack, dirent->firstSector,
			 data, length);

    if (status != WrOK) {
      restoreBAM (image, &oldBAM);
      (*log) (Errors, name, "Writing the data bytes failed.");
      return status;
    }

    switch (name->type) {
    case REL:
      /* set the initial track and sector for the side sectors */
      dirent->ssTrack = image->dirtrack + 1;
      dirent->ssSector = 0;

      status = setupSideSectors (image, dirent, rounddiv(length, 254));

      if (status != WrOK) {
	restoreBAM (image, &oldBAM);
	(*log) (Errors, name, "Could not set up the side sectors.");
	return status;
      }

      /* fall through */

    case DEL:
    case SEQ:
    case PRG:
    case USR:
      free (oldBAM);

      dirent->type = name->type | 0x80;
      return WrOK;

    default:
      restoreBAM (image, &oldBAM);

      (*log) (Errors, name, "Unsupported file type.");
      return WrFail;
    }
  }
}

/* Read all files from a disk image. */
RdStatus ReadImage (FILE *file, const char *filename,
		    WriteCallback *writeCallback, LogCallback *log)
{
  const DiskGeometry *geom = NULL;
  Image image;

  /* determine disk image type from its length */
  {
    size_t length, blocks;
    unsigned i;

    if (fseek (file, 0, SEEK_END)) {
    seekError:
      (*log) (Errors, NULL, "fseek: %s", strerror(errno));
      return RdFail;
    }

    length = ftell (file);

    if (fseek (file, 0, SEEK_SET))
      goto seekError;

    if (length % 256) {
    unknownImage:
      (*log) (Errors, NULL, "Unknown disk image type");
      return RdFail;
    }

    for (i = 0, blocks = length / 256; i < elementsof(diskGeometry); i++)
      if (diskGeometry[i].blocks == blocks) {
	geom = &diskGeometry[i];
	break;
      }

    if (!geom)
      goto unknownImage;

    /* Initialize the disk image structure. */

    if (!(image.buf = malloc (length))) {
      (*log) (Errors, NULL, "Out of memory");
      return RdFail;
    }

    image.type = geom->type;
    image.dirtrack = geom->dirtrack;
    image.name = NULL;

    if (1 != fread (image.buf, length, 1, file)) {
      (*log) (Errors, NULL, "fread: %s", strerror(errno));
      free (image.buf);
      return RdFail;
    }
  }

  /* Traverse through the root directory and extract the files */
  {
    DirEnt **directory = NULL;
    unsigned block;

    if (!(block = mapInode ((BYTE***)&directory, &image, image.dirtrack, 0))) {
      (*log) (Errors, NULL, "Could not read the directory on track %u.",
	      image.dirtrack);
      free (image.buf);
      return RdFail;
    }

    if (block < geom->BAMblocks) {
      (*log) (Errors, NULL, "Directory too short.");
      free (image.buf);
      free (directory);
      return RdFail;
    }

    /* Traverse through the directory */

    for (block = geom->BAMblocks; ; block++) {
      unsigned i;
      Filename name;

      for (i = 0; i < 256 / sizeof (DirEnt); i++) {
	DirEnt *dirent = &directory[block][i];

	memcpy (name.name, dirent->name, 16);
	name.type = getFiletype (&image, dirent);
	name.recordLength = dirent->recordLength;

	if (isGeosDirEnt (dirent)) {
	  static const char cvt[] = "PRG formatted GEOS file V1.0";
	  size_t length = 0;
	  BYTE *buf;
	  const BYTE
	    *vlir = NULL,
	    *info = getBlock (&image, dirent->infoTrack, dirent->infoSector);

	  if (!info || memcmp (info, "\0\377\3\25\277", 5))
	    goto notGEOS; /* invalid info block */

	  if (dirent->isVLIR) {
	    unsigned vlirblock;
	    vlir = getBlock (&image, dirent->firstTrack, dirent->firstSector);
	    if (!vlir || vlir[0] != 0 || vlir[1] != 0xFF)
	      goto notGEOS;

	    /* see if the VLIR block is valid and determine file length */
	    for (length = 0, vlirblock = 1; vlirblock < 128; vlirblock++)
	      if (!vlir[2 * vlirblock])
		continue;
	      else if (vlir[2 * vlirblock] > geom->tracks ||
		       vlir[2 * vlirblock + 1] >=
		       geom->sectors[vlir[2 * vlirblock]])
		goto notGEOS;
	      else {
		BYTE *b = NULL;
		size_t chainlen = readInode (&b, &image,
					     vlir[2 * vlirblock],
					     vlir[2 * vlirblock + 1]);
		if (!chainlen)
		  goto notGEOS;

		free (b);
		length = 254 * rounddiv(length, 254) + chainlen;
	      }
	  }
	  else {
	    BYTE *b = NULL;
	    length = readInode (&b, &image,
				dirent->firstTrack, dirent->firstSector);
	    if (!length)
	      goto notGEOS;
	    free (b);
	  }

	  /* convert the GEOS file name and type */
	  {
	    unsigned j;

	    for (j = 0; j < sizeof name.name; j++)
	      if (name.name[j] >= 'A' && name.name[j] <= 'Z')
		name.name[j] = name.name[j] - 'A' + 0xC1;
	      else if (name.name[j] >= 'a' && name.name[j] <= 'z')
		name.name[j] = name.name[j] - 'a' + 0x41;

	    name.type = PRG;
	  }

	  if ((info[0x44] ^ dirent->type) & 0x8F)
	    (*log) (Warnings, &name, "file types differ: $%02x $%02x",
		    info[0x44], dirent->type);

	  if (info[0x45] != dirent->geos.type)
	    (*log) (Warnings, &name, "GEOS file types differ: $%02x $%02x",
		    info[0x45], dirent->geos.type);

	  if (info[0x46] != dirent->isVLIR)
	    (*log) (Warnings, &name, "VLIR flags differ: $%02x $%02x",
		    info[0x46], dirent->isVLIR);

	  if (rounddiv(length, 254) + 1 + dirent->isVLIR !=
	      dirent->blocksLow + (dirent->blocksHigh << 8)) {
	    unsigned blks = rounddiv(length, 254) + 1 + dirent->isVLIR;
	    dirent->blocksLow = blks & 0xFF;
	    dirent->blocksHigh = blks >> 8;
	    (*log) (Warnings, &name, "invalid block count");
	  }

	  if (!(buf = calloc ((2 + dirent->isVLIR) * 254 + length, 1))) {
	    (*log) (Errors, &name, "Out of memory");
	    free (image.buf);
	    free (directory);
	    return RdFail;
	  }

	  /* set the Convert header data */
	  memcpy (&buf[0], &dirent->type, length = sizeof(DirEnt) - 2);
	  memcpy (&buf[length], cvt, sizeof cvt); length += sizeof cvt;
	  /* clear the track/sector information from the header */
	  buf[1] = buf[2] = buf[0x13] = buf[0x14] = 0;

	  /* copy the info block */
	  memcpy (&buf[254], &info[2], 254);

	  if (dirent->isVLIR) {
	    unsigned vlirblock;
	    bool ended = false, wasended = false;

	    memcpy (&buf[2 * 254], &vlir[2], 254);

	    for (length = 3 * 254, vlirblock = 1; vlirblock < 128; vlirblock++)
	      if (vlir[2 * vlirblock]) {
		BYTE *b = NULL;
		size_t chainlen = readInode (&b, &image,
					     vlir[2 * vlirblock],
					     vlir[2 * vlirblock + 1]);

		if (!chainlen || !b) {
		  (*log) (Errors, &name, "unable to read VLIR chain!");
		  break;
		}

		length = 254 * rounddiv(length, 254);
		memcpy (&buf[length], b, chainlen);
		length += chainlen;
		free (b);

		if (ended && !wasended) {
		  (*log) (Warnings, &name, "false EOF in VLIR sector");
		  wasended = true;
		}

		/* Set the VLIR block information: amount of blocks */
		buf[(253 + vlirblock) * 2] = rounddiv(chainlen, 254);
		/* amount of used bytes in the last block of this chain */
		buf[(253 + vlirblock) * 2 + 1] =
		  chainlen % 254 ? chainlen % 254 + 1 : 0xFF;
	      }
	      else {
		switch (vlir[2 * vlirblock + 1]) {
		case 0:
		  ended = true;
		  break;

		case 0xFF:
		  if (ended && !wasended) {
		    (*log) (Warnings, &name, "false EOF in VLIR sector");
		    wasended = true;
		  }
		  break;

		default:
		  buf[(253 + vlirblock) * 2] = 0;
		  buf[(253 + vlirblock) * 2 + 1] = ended ? 0 : 0xFF;

		  (*log) (Warnings, &name,
			  "invalid VLIR pointer $00%02x, "
			  "corrected to $00%02x",
			  vlir[2 * vlirblock + 1],
			  buf[(253 + vlirblock) * 2 + 1]);
		  break;
		}
	      }
	  }
	  else {
	    BYTE *b = NULL;
	    size_t len = readInode (&b, &image,
				    dirent->firstTrack, dirent->firstSector);
	    memcpy (&buf[2 * 254], b, len);
	    length = 2 * 254 + len;
	    free (b);
	  }

	  switch ((*writeCallback) (&name, buf, length)) {
	  case WrOK:
	    continue;
	  case WrNoSpace:
	    free (buf);
	    free (image.buf);
	    free (directory);
	    return RdNoSpace;
	  case WrFail:
	  default:
	    free (buf);
	    free (image.buf);
	    free (directory);
	    return RdFail;
	  notGEOS:
	    (*log) (Warnings, &name, "not a valid GEOS file");
	  }
	}
	switch (name.type) {
	  BYTE *buf;
	  size_t length;
	case REL:
	  if (!checkSideSectors (&image, dirent))
	    (*log) (Warnings, &name, "error in side sector data");

	  /* fall through */
	case DEL:
	case SEQ:
	case PRG:
	case USR:
	  buf = NULL;
	  length = readInode (&buf, &image,
			      dirent->firstTrack, dirent->firstSector);
	  if (!length)
	    (*log) (Errors, &name, "could not read file");
	  else {
	    if (name.type != REL && rounddiv(length, 254) !=
		dirent->blocksLow + (dirent->blocksHigh << 8))
	      (*log) (Warnings, &name, "invalid block count");

	    switch ((*writeCallback) (&name, buf, length)) {
	    case WrOK:
	      break;
	    case WrNoSpace:
	      free (buf);
	      free (image.buf);
	      free (directory);
	      return RdNoSpace;
	    case WrFail:
	    default:
	      free (buf);
	      free (image.buf);
	      free (directory);
	      return RdFail;
	    }
	  }

	  free (buf);
	  break;

	case CBM:
	  if(image.type == Im1581) {
	    (*log) (Errors, &name, "skipping partition");
	    /* TODO: Recurse... */
	    break;
	  }

	  /* Fall through (CBM type only supported by the 1581) */

	default:
	  if (dirent->type)
	    (*log) (Errors, &name, "unknown file type $%02x, skipping",
		    dirent->type);
	}
      }

      if (!directory[block]->nextTrack)
	break;
    }

    free (directory);
  }

  free (image.buf);
  return RdOK;
}

/* Open an existing disk image or create a new one. */
ImStatus OpenImage (const char *filename, Image **image,
		    ImageType type, DirEntOpts direntOpts)
{
  FILE *f;
  const DiskGeometry *geom;

  /* The image buffer may not have been allocated. */
  if (!image || *image)
    return ImFail;

  if (!(geom = getGeometry (type)))
    return ImFail;

  /* Allocate and initialize the image structure. */

  if (!(*image = calloc (1, sizeof (**image))))
    return ImFail;

  if (!((*image)->buf = malloc (geom->blocks * 256))) {
  Failed:
    free ((*image)->name);
    free ((*image)->buf);
    free (*image);
    *image = NULL;
    return ImFail;
  }

  if (!((*image)->name = malloc (strlen (filename) + 1)))
    goto Failed;

  strcpy ((char*)(*image)->name, filename);
  (*image)->type = type;
  (*image)->direntOpts = direntOpts;
  (*image)->dirtrack = geom->dirtrack;

  if (!(f = fopen (filename, "rb"))) {
    if (errno != ENOENT) /* It is OK if the file was not found. */
      goto Failed;

    /* Initialize the image */
    FormatImage (*image);
  }
  else {
    /* Read in the disk image */

    if (1 != fread ((*image)->buf, geom->blocks * 256, 1, f) ||
	EOF != fgetc (f)) {
      fclose (f);
      goto Failed;
    }

    fclose (f);
  }

  (*image)->partTops[(*image)->dirtrack - 1] = geom->tracks;
  (*image)->partBots[(*image)->dirtrack - 1] = 1;
  (*image)->partUpper[(*image)->dirtrack - 1] = 0;

  return ImOK;
}

/* Write back a disk image. */
ImStatus CloseImage (Image *image)
{
  FILE *f;
  const DiskGeometry *geom;

  if (!image || !image->buf || !(geom = getGeometry (image->type)))
    return ImFail;

  if (!(f = fopen ((char*)image->name, "wb")))
    return errno == ENOSPC ? ImNoSpace : ImFail;

  if (1 != fwrite (image->buf, geom->blocks * 256, 1, f)) {
    fclose (f);
    return errno == ENOSPC ? ImNoSpace : ImFail;
  }

  fclose (f);
  free (image->buf);
  image->buf = NULL;
  return ImOK;
}

/* Determine disk image geometry. */
static const DiskGeometry *getGeometry (ImageType type)
{
  int i;

  for (i = 0; i < elementsof(diskGeometry); i++)
    if (diskGeometry[i].type == type)
      return &diskGeometry[i];

  return NULL;
}

/* Format disk image. */
static void FormatImage (Image *image)
{
  const DiskGeometry *geom;
  const char id1 = '9'; /* disk ID characters */
  const char id2 = '8';
  const char title[] = "CBMCONVERT   2.0"; /* disk title */

  if (!image || !image->buf || !(geom = getGeometry (image->type)))
    return;

  /* Clear all sectors */
  memset (image->buf, 0, geom->blocks * 256);

  switch (image->type) {
    BYTE track, sector;
    BYTE *BAM;
  case ImUnknown:
    return;
  case Im1541:
    /* Initialize the BAM */
    if (!(BAM = getBlock (image, image->dirtrack, 0)))
      return;

    /* set the track/sector links */
    BAM[0] = image->dirtrack;
    BAM[1] = 1;
    BAM[0x100] = 0; /* first directory block */
    BAM[0x101] = 0xFF;
    /* set the format identifier */
    BAM[2] = geom->formatID;
    BAM[3] = 0x00;
    /* set the disk title */
    memcpy (&BAM[0x90], title, 16);
    /* pad the disk header */
    memset (&BAM[0xA0], 0xA0, 11);
    /* set the format specifier */
    BAM[0xA5] = '2';
    BAM[0xA6] = geom->formatID;
    /* set the disk ID */
    BAM[0xA2] = id1;
    BAM[0xA3] = id2;
    /* free all blocks */
    memset (&BAM[4], 0xFF, geom->tracks << 2);

    for (track = 1; track <= geom->tracks; track++) {
      /* set amount of free blocks on each track */
      BAM[track << 2] = sector = geom->sectors[track];
      /* allocate non-existent blocks */
      for (; sector < 24; sector++)
	BAM[(track << 2) + 1 + (sector >> 3)] &= ~(1 << (sector & 7));
    }

    /* Allocate the BAM and directory entries. */
    track = image->dirtrack;
    sector = 0;
    allocBlock (image, &track, &sector);
    track = BAM[0];
    sector = BAM[1];
    allocBlock (image, &track, &sector);
    break;

  case Im1571:
    /* Initialize the BAM */
    if (!(BAM = getBlock (image, image->dirtrack, 0)))
      return;

    /* set the track/sector links */
    BAM[0] = image->dirtrack;
    BAM[1] = 1;
    BAM[0x100] = 0; /* first directory block */
    BAM[0x101] = 0xFF;
    /* set the format identifier */
    BAM[2] = geom->formatID;
    BAM[3] = 0x80;
    /* set the disk title */
    memcpy (&BAM[0x90], title, 16);
    /* pad the disk header */
    memset (&BAM[0xA0], 0xA0, 11);
    /* set the format specifier */
    BAM[0xA5] = '2';
    BAM[0xA6] = geom->formatID;
    /* set the disk ID */
    BAM[0xA2] = id1;
    BAM[0xA3] = id2;
    /* free all blocks */
    memset (&BAM[4], 0xFF, geom->tracks << 2);

    for (track = 1; track <= 35; track++) {
      /* set amount of free blocks on each track */
      BAM[track << 2] = sector = geom->sectors[track];
      /* allocate non-existent blocks */
      for (; sector < 24; sector++)
	BAM[(track << 2) + 1 + (sector >> 3)] &= ~(1 << (sector & 7));
    }
    /* second side */
    for (track = 0; track < 35; track++) {
      /* set amount of free blocks on each track */
      BAM[0xDC + track + 1] = sector = geom->sectors[track + 1];
      /* allocate non-existent blocks */
      for (; sector < 24; sector++)
	BAM[(track * 3) + (sector >> 3)] &= ~(1 << (sector & 7));
    }

    /* Allocate the BAM and directory entries. */
    track = image->dirtrack;
    sector = 0;
    allocBlock (image, &track, &sector);
    track = BAM[0];
    sector = BAM[1];
    allocBlock (image, &track, &sector);
    track = image->dirtrack + 35;
    sector = 0;
    allocBlock (image, &track, &sector);
    break;

  case Im1581:
    /* Initialize the Header block */
    if (!(BAM = getBlock (image, image->dirtrack, 0)))
      return;

    image->partTops[image->dirtrack - 1] = geom->tracks;
    image->partBots[image->dirtrack - 1] = 1;
    image->partUpper[image->dirtrack - 1] = 0;

    /* set the track/sector link to the first directory block */
    BAM[0] = image->dirtrack;
    BAM[1] = 3;
    BAM[0x100] = image->dirtrack; /* BAM block 1 */
    BAM[0x101] = 2;
    BAM[0x200] = 0; /* BAM block 2 */
    BAM[0x201] = 0xFF;
    BAM[0x300] = 0;
    BAM[0x301] = 0xFF; /* first directory block */

    /* set the format identifier */
    BAM[2] = geom->formatID;
    BAM[3] = 0;

    /* set the disk title */
    memcpy (&BAM[0x4], title, 16);
    /* pad the disk header */
    memset (&BAM[0x14], 0xA0, 11);
    /* set the format specifier */
    BAM[0x19] = '3';
    BAM[0x1a] = geom->formatID;
    /* set the disk ID */
    BAM[0x16] = id1;
    BAM[0x17] = id2;

    /* BAM block 1 */
    BAM = getBlock (image, image->dirtrack, 1);

    BAM[2] = geom->formatID;
    BAM[3] = ~geom->formatID;
    BAM[4] = id1; /* Disk ID */
    BAM[5] = id2;
    BAM[6] = 192; /* I/O byte */
    BAM[7] = 0;   /* Auto loader flag */

    for (track = image->partBots[image->dirtrack - 1];
	 track <= image->partTops[image->dirtrack - 1] && track <= 40;
	 track++) {
      BYTE *tmp = BAM + 16 + (track - 1) * 6;

      /* set amount of free blocks on each track */

      tmp[0] = (track == image->dirtrack) ? 36 : 40;
      /* 4 reserved blocks on dirtrack */
      tmp[1] = (track == image->dirtrack) ? 0xf0 : 0xff;
      /* ditto */
      tmp[2] = tmp[3] = tmp[4] = tmp[5] = 0xff;
    }

    /* BAM block 2 */
    BAM = getBlock (image, BAM[0], BAM[1]);

    BAM[2] = geom->formatID;
    BAM[3] = ~geom->formatID;
    BAM[4] = id1; /* Disk ID */
    BAM[5] = id2;
    BAM[6] = 192; /* I/O byte (copy) */
    BAM[7] = 0;   /* Auto loader flag (copy) */

    for (track = image->partTops[image->dirtrack - 1];
	 track >= image->partBots[image->dirtrack - 1] && track > 40;
	 track--) {
      BYTE *tmp = BAM + 16 + (track - 41) * 6;

      /* set amount of free blocks on each track */

      tmp[0] = 40;
      tmp[1] = tmp[2] = tmp[3] = tmp[4] = tmp[5] = 0xff;
    }

    break;
  }
}

/*
** Get a pointer to the block in the specified track and sector.
*/
static BYTE *getBlock (Image *image, BYTE track, BYTE sector)
{
  const DiskGeometry *geom;
  int t, b;

  if (!image || !image->buf || !(geom = getGeometry (image->type)))
    return NULL;

  if (track < 1 || track > geom->tracks || sector >= geom->sectors[track])
    return NULL; /* illegal track or sector */

  for (t = 1, b = 0; t < track; t++)
    b += geom->sectors[t];

  b += sector;

  return &image->buf[b << 8];
}

/*
** Get a block pointer table to all blocks in the file
** starting at the specified track and sector.
** Return the number of blocks mapped (0 on failure).
*/
static size_t mapInode (BYTE ***buf, Image *image, BYTE track, BYTE sector)
{
  const DiskGeometry *geom;
  int t, s;
  size_t size;
  BYTE *block;

  if (!buf || *buf ||
      !image || !image->buf || !(geom = getGeometry (image->type)))
    return 0;

  /* Determine the number of blocks. */
  for (t = track, s = sector, size = 0; t; size++) {
    if (size > geom->blocks)
      return 0; /* endless file */

    if (!(block = getBlock (image, t, s)))
      return 0;

    if (isFreeBlock (image, t, s))
      return 0;

    t = block[0];
    s = block[1];
  }

  /* Set up the block pointer table. */

  if (!(*buf = malloc (size * sizeof **buf)))
    return 0;

  for (t = track, s = sector, size = 0; t; size++) {
    if (!((*buf)[size] = getBlock (image, t, s))) {
      free (*buf);
      *buf = NULL;
      return 0;
    }

    t = (*buf)[size][0];
    s = (*buf)[size][1];
  }

  return size;
}

/*
** Read the file starting at the specified track and sector
** to a buffer.  Return the file length, or 0 on error.
*/
static size_t readInode (BYTE **buf, const Image *image,
			 BYTE track, BYTE sector)
{
  const DiskGeometry *geom;
  int t, s;
  size_t size;

  if (!buf || *buf ||
      !image || !image->buf || !(geom = getGeometry (image->type)))
    return 0;

  /* Determine the file size. */
  for (t = track, s = sector, size = 0; t; size += 254) {
    const BYTE *block;

    if (size > 254 * geom->blocks)
      return 0; /* endless file */

    if (!(block = getBlock ((Image*)image, t, s)))
      return 0;

    if (isFreeBlock (image, t, s))
      return 0;

    t = block[0];
    s = block[1];
  }

  if (s < 2)
    return 0; /* The last byte pointer must be at least 2. */

  size += s - 255;
  if (!(*buf = malloc (size)))
    return 0;

  /* Read the file. */
  for (t = track, s = sector, size = 0; t; size += 254) {
    const BYTE *block = getBlock ((Image*)image, t, s);
    t = block[0];
    s = block[1];
    memcpy (&(*buf)[size], &block[2], t ? 254 : s - 1);
  }

  return size += s - 255;
}

/*
** Write the file to the disk, starting from the specified track and sector.
*/
static WrStatus writeInode (Image *image, BYTE track, BYTE sector,
			    const BYTE *buf, size_t size)
{
  BYTE t, s;
  size_t count;
  BYTE *oldBAM = NULL;

  if (!buf || !image || !image->buf)
    return WrFail;

  /* Make a copy of the BAM. */

  if (!backupBAM (image, &oldBAM))
    return WrFail;

  /* Write the file. */
  for (t = track, s = sector, count = 0; count < size; count += 254) {
    BYTE *block;

    if (!(block = getBlock (image, t, s))) {
      restoreBAM (image, &oldBAM);
      return WrFail;
    }

    if (!allocBlock (image, &t, &s)) {
      restoreBAM (image, &oldBAM);
      return WrNoSpace;
    }

    if (count + 254 < size) { /* not yet last block */
      block[0] = t;
      block[1] = s;
      memcpy (&block[2], &buf[count], 254);
    }
    else {
      block[0] = 0;
      block[1] = size - count + 1;
      memcpy (&block[2], &buf[count], size - count);
    }
  }

  free (oldBAM);
  return WrOK;
}

/*
** Wipe out and delete the file starting at the specified track and sector.
*/
static ImStatus deleteInode (Image *image, BYTE track, BYTE sector, bool do_it)
{
  int t, s;

  if (!image || !image->buf)
    return ImFail;

  /* Make sure that the whole file has been allocated. */
  for (t = track, s = sector; t; ) {
    BYTE *block;

    if (!(block = getBlock (image, t, s)))
      return ImFail;

    if (isFreeBlock (image, t, s))
      return ImFail;

    t = block[0];
    s = block[1];
  }

  if (do_it) {
    /* Free the space allocated by the file. */

    for (t = track, s = sector; t; ) {
      BYTE *block = getBlock (image, t, s);
      freeBlock (image, t, s);
      t = block[0];
      s = block[1];
      /* clear the block */
      memset (block, 0, 256);
    }
  }

  return ImOK;
}

static DirEnt *getDirEnt (Image *image, const Filename *name)
{
  const DiskGeometry *geom;
  DirEnt **directory = NULL, *dirent = NULL;
  unsigned block, i;
  int freeslotBlock = -1, freeslotEntry = -1;

  if (!name || !image || !image->buf || !(geom = getGeometry (image->type)))
    return NULL;

  /* Read the current directory. */

  if (!(block = mapInode ((BYTE***)&directory, image, image->dirtrack, 0)))
    return NULL;

  /* Check that the directory is long enough to hold the BAM blocks
     and at least one directory sector */
  if (block < geom->BAMblocks) {
    free (directory);
    return NULL;
  }

  /* Search for the name in the directory. */
  for (block = geom->BAMblocks; ; block++) {
    for (i = 0; i * sizeof (DirEnt) < (directory[block]->nextTrack ? 256 :
				       directory[block]->nextSector);
	 i++) {
      dirent = &directory[block][i];
      
      if (freeslotBlock == -1 && !dirent->type) {
	/* null file type => unused slot */
	freeslotBlock = block;
	freeslotEntry = i;
      }

      if (!memcmp (dirent->name, name->name, 16)) {
	free (directory);
	return dirent;
      }
    }

    if (!directory[block]->nextTrack)
      break;
  }

  /* The name was not found in the directory. */

  if (image->direntOpts == DirEntDontCreate) {
    free (directory);
    return NULL;
  }

  if (freeslotBlock == -1) {
    /* Append a directory entry */

    if (i < 256 / sizeof (DirEnt)) {
      /* grow the directory by growing its last sector */

      directory[block]->nextSector = sizeof (DirEnt) *
	(1 + directory[block]->nextSector / sizeof (DirEnt));
      freeslotBlock = block;
      freeslotEntry = i;
    }
    else {
      /* allocate a new directory block */

      BYTE track, sector;
      BYTE t, s;

      track = image->dirtrack;
      sector = geom->BAMblocks;

      if (!findNextFree (image, &track, &sector)) {
	free (directory);
	return NULL;
      }

      t = directory[block]->nextTrack = track;
      s = directory[block]->nextSector = sector;

      if (!allocBlock (image, &t, &s)) {
	directory[block]->nextTrack = 0;
	directory[block]->nextSector = 0xFF;
	free (directory);
	return NULL;
      }

      /* Remap the directory from the disk image */

      free (directory);
      directory = NULL;

      if (!mapInode ((BYTE***)&directory, image, image->dirtrack, 0))
	return NULL;

      block++;

      /* initialize the new directory block */
      memset (directory[block], 0, 256);
      directory[block]->nextSector = 0xFF;

      freeslotBlock = block;
      freeslotEntry = 0;
    }
  }

  /* Clear the directory entry. */

  dirent = &directory[freeslotBlock][freeslotEntry];

  if (freeslotEntry)
    memset (dirent, 0, sizeof (DirEnt));
  else
    memset (&dirent->type, 0, sizeof (DirEnt) - 2);

  free (directory);
  return dirent;
}

#ifdef DEBUG
static bool isValidDirEnt (const Image *image, const DirEnt *dirent)
{
  const DiskGeometry *geom;

  if (!image || !image->buf || !(geom = getGeometry (image->type)))
    return false;

  if ((BYTE*)dirent < image->buf ||
      (BYTE*)dirent > &image->buf[geom->blocks << 8] ||
      ((BYTE*)dirent - image->buf) % sizeof(DirEnt))
    return false;

  return true;
}
#endif

static bool isGeosDirEnt (const DirEnt *dirent)
{
  Filetype type = dirent->type & 0x8F;

  return type >= DEL && type < REL && dirent->geos.type != 0 &&
    (dirent->isVLIR == 0 || dirent->isVLIR == 1);
}

static Filetype getFiletype (const Image *image, const DirEnt *dirent)
{
  Filetype type;

#ifdef DEBUG
  if (!isValidDirEnt (image, dirent))
    return 0;
#endif

  type = dirent->type & 0x8F;

  if (type < DEL || type > (image->type == Im1581 ? CBM : REL))
    return 0; /* illegal file type */

  return type;
}

static ImStatus deleteDirEnt (Image *image, DirEnt *dirent)
{
  ImStatus status;

#ifdef DEBUG
  if (!isValidDirEnt (image, dirent))
    return ImFail;
#endif

  if (isGeosDirEnt (dirent)) {
    /* Check if the inodes can be deleted. */
    if (ImOK != deleteInode (image, dirent->firstTrack,
			     dirent->firstSector, false) ||
	ImOK != deleteInode (image, dirent->infoTrack,
			     dirent->infoSector, false))
      return ImFail;

    if (dirent->isVLIR) {
      unsigned vlirblock;
      const BYTE *vlir = getBlock (image, dirent->firstTrack,
				   dirent->firstSector);

      for (vlirblock = 1; vlirblock < 128; vlirblock++)
	if (vlir[2 * vlirblock] &&
	    ImOK != deleteInode (image, vlir[2 * vlirblock],
				 vlir[2 * vlirblock + 1], false))
	  return ImFail;

      /* Delete the VLIR nodes. */
      for (vlirblock = 1; vlirblock < 128; vlirblock++)
	if (vlir[2 * vlirblock])
	  deleteInode (image, vlir[2 * vlirblock],
		       vlir[2 * vlirblock + 1], true);
    }

    /* Delete the info block and the file. */
    deleteInode (image, dirent->infoTrack, dirent->infoSector, true);
    deleteInode (image, dirent->firstTrack, dirent->firstSector, true);
    dirent->type = 0;
    return ImOK;
  }
  else if (getFiletype (image, dirent) == REL &&
	   (ImOK != deleteInode (image, dirent->firstTrack,
				 dirent->firstSector, false) ||
	    ImOK != deleteInode (image, dirent->ssTrack,
				 dirent->ssSector, true)))
    return ImFail;

  status = deleteInode (image, dirent->firstTrack, dirent->firstSector, true);

  if (status == ImOK)
    /* nuke the directory entry */
    dirent->type = 0;

  return status;
}

/*
** Set up the side sector file for relative files.
*/
static WrStatus setupSideSectors (Image *image, DirEnt *dirent, size_t blocks)
{
  int sscount;

#ifdef DEBUG
  if (!isValidDirEnt (image, dirent) || getFiletype (image, dirent) != REL)
    return WrFail;
#endif

  if(image->type == Im1581)
    return WrFail; /* TODO: super side sector setup etc. */

  sscount = rounddiv(blocks, 120);

  if (sscount < 1)
    return WrFail;

  if (sscount > 6 || blocksFree (image) < sscount)
    /* too many side sector blocks */
    return WrNoSpace;

  if (!findNextFree (image, &dirent->ssTrack, &dirent->ssSector))
    return WrNoSpace;

  {
    BYTE *buf;
    WrStatus status;
    size_t sslength = 14 + 254 * (sscount - 1) + 2 * (blocks % 120);

    if (!(buf = calloc (sslength, 1)))
      return WrFail;

    status = writeInode (image, dirent->ssTrack, dirent->ssSector,
			 buf, sslength);
    free (buf);

    if (status != ImOK)
      return status;
  }

  {
    BYTE **sidesect = NULL, **datafile = NULL;
    BYTE ss, ssentry, i, track, sector;

    if (blocks != mapInode (&datafile, image,
			    dirent->firstTrack, dirent->firstSector))
      return WrFail;

    if (sscount != mapInode (&sidesect, image,
			     dirent->ssTrack, dirent->ssSector)) {
      free (datafile);
      return WrFail;
    }

    for (ss = 0; ss < sscount; ss++) {
      sidesect[ss][2] = ss;
      sidesect[ss][3] = dirent->recordLength;

      sidesect[ss][4] = dirent->ssTrack;
      sidesect[ss][5] = dirent->ssSector;

      for (i = 1; i < sscount; i++) {
	sidesect[ss][4 + i * 2] = sidesect[i - 1][0];
	sidesect[ss][5 + i * 2] = sidesect[i - 1][1];
      }
    }

    track = dirent->firstTrack;
    sector = dirent->firstSector;

    for (ss = ssentry = 0; track; ssentry++) {
      ss = ssentry / 120;

      if (ss >= sscount)
	return WrFail;

      sidesect[ss][16 + (ssentry % 120) * 2] = track;
      sidesect[ss][17 + (ssentry % 120) * 2] = sector;

      track = datafile[ssentry][0];
      sector = datafile[ssentry][1];
    }

    free (datafile);
    free (sidesect);
  }

  return WrOK;
}

/*
** Check if the side sectors of a relative file are OK
*/
static bool checkSideSectors (const Image *image, const DirEnt *dirent)
{
  unsigned ss, ssentry, sscount, i;
  BYTE **sidesect = NULL, **datafile = NULL;
  BYTE track, sector;

#ifdef DEBUG
  if (!isValidDirEnt (image, dirent))
    return false;
#endif

  if (getFiletype (image, dirent) != REL)
    return false;

  /* Map the data file and the side sectors. */

  if (!(i = mapInode (&datafile, (Image*)image,
		      dirent->firstTrack, dirent->firstSector)))
    return false;

  if (!(sscount = mapInode (&sidesect, (Image*)image,
			    dirent->ssTrack, dirent->ssSector))) {
    free (datafile);
    return false;
  }

  /* Check the block counts */
  if (sscount != rounddiv(i, 120) ||
      i + sscount != dirent->blocksLow + (dirent->blocksHigh << 8) ||
      i != 120 * (sscount - 1) + (sidesect[sscount - 1][1] - 15) / 2) {
  Failed:
    free (datafile);
    free (sidesect);
    return false;
  }

  /* Check the side sector links */

  for (ss = 0; ss < sscount; ss++) {
    if (sidesect[ss][2] != ss ||
	sidesect[ss][3] != dirent->recordLength ||
	sidesect[ss][4] != dirent->ssTrack ||
	sidesect[ss][5] != dirent->ssSector)
      goto Failed;

    for (i = 1; i < sscount; i++)
      if (sidesect[ss][4 + i * 2] != sidesect[i - 1][0] ||
	  sidesect[ss][5 + i * 2] != sidesect[i - 1][1])
	goto Failed;
  }

  /* Check the links to the data file */

  track = dirent->firstTrack;
  sector = dirent->firstSector;

  for (ss = ssentry = 0; track; ssentry++) {
    ss = ssentry / 120;

    if (ss >= sscount ||
	sidesect[ss][16 + (ssentry % 120) * 2] != track ||
	sidesect[ss][17 + (ssentry % 120) * 2] != sector)
      goto Failed;

    track = datafile[ssentry][0];
    sector = datafile[ssentry][1];
  }

  free (datafile);
  free (sidesect);

  return true;
}

/*
** Allocate the block at the specified track and sector.
** Set the track and sector to the next block candidate.
*/
static bool allocBlock (Image *image, BYTE *track, BYTE *sector)
{
  const DiskGeometry *geom;

  if (!track || !sector ||
      !image || !image->buf || !(geom = getGeometry (image->type)))
    return false;

  if (*track < 1 || *track > geom->tracks || *sector >= geom->sectors[*track])
    return false; /* illegal track or sector */

  switch (image->type) {
    BYTE *BAM;

  case ImUnknown:
    return false;
  case Im1571:
    if (*track > 35) {
      BYTE tr = *track - 35;
      BYTE *BAM2;

      if (!(BAM = getBlock ((Image*)image, image->dirtrack, 0)) ||
	  !(BAM2 = getBlock ((Image*)image, 35 + image->dirtrack, 0)))
	return false;

      if (!(BAM2[((tr - 1) * 3) + (*sector >> 3)] & (1 << (*sector & 7))))
	return false; /* already allocated */

      /* decrement the count of free sectors per track */
      BAM[0xDC + tr]--;
      /* allocate the block */
      BAM2[((tr - 1) * 3) + (*sector >> 3)] &= ~(1 << (*sector & 7));

      /* find next free block */
      findNextFree (image, track, sector);
      return true;
    }
    /* fall through: track <= 35 */
  case Im1541:
    if (!(BAM = getBlock ((Image*)image, image->dirtrack, 0)))
      return false;

    if (!(BAM[(*track << 2) + 1 + (*sector >> 3)] & (1 << (*sector & 7))))
      return false; /* already allocated */

    /* decrement the count of free sectors per track */
    BAM[*track << 2]--;
    /* allocate the block */
    BAM[(*track << 2) + 1 + (*sector >> 3)] &= ~(1 << (*sector & 7));

    /* find next free block */
    findNextFree (image, track, sector);

    return true;

  case Im1581:
    {
      BYTE **BAMblocks = NULL;
      BYTE offset;

      if(*track > image->partTops[image->dirtrack - 1] ||
	 *track < image->partBots[image->dirtrack - 1])
	return false;

      if (2 != mapInode (&BAMblocks, image, image->dirtrack, 1)) {
	if (BAMblocks) free (BAMblocks);
	return false;
      }

      offset = *track;
      if(offset > 40) {
	BAM = BAMblocks[1];
	offset -= 40;
      }
      else
	BAM = BAMblocks[0];

      free (BAMblocks);

      if (!(BAM[16 + (offset - 1) * 6 + (*sector >> 3) + 1] &
	    (1 << (*sector & 7))))
	return false; /* already allocated */

      BAM[16 + (offset - 1) * 6] -= 1;
      BAM[16 + (offset - 1) * 6 + (*sector >> 3) + 1] &= ~(1 << (*sector & 7));

      /* find next free block */
      findNextFree (image, track, sector);

      return true;
    }
  }

  return false;
}

/*
** Free the block at the specified track and sector.
*/
static bool freeBlock (Image *image, BYTE track, BYTE sector)
{
  const DiskGeometry *geom;

  if (!image || !image->buf || !(geom = getGeometry (image->type)))
    return false;

  if (track < 1 || track > geom->tracks || sector >= geom->sectors[track])
    return false; /* illegal track or sector */

  if (isFreeBlock (image, track, sector))
    return false; /* already freed */

  switch (image->type) {
    BYTE *BAM;

  case ImUnknown:
    return false;
  case Im1571:
    if (track > 35) {
      BYTE tr = track - 35;
      BYTE *BAM2;

      if (!(BAM = getBlock ((Image*)image, image->dirtrack, 0)) ||
	  !(BAM2 = getBlock ((Image*)image, 35 + image->dirtrack, 0)))
	return false;

      /* increment the count of free sectors per track */
      BAM[0xDC + tr]++;
      /* free the block */
      BAM2[((tr - 1) * 3) + (sector >> 3)] |= (1 << (sector & 7));
      return true;
    }
    /* fall through: track <= 35 */
  case Im1541:
    if (!(BAM = getBlock ((Image*)image, image->dirtrack, 0)))
      return false;

    /* increment the count of free sectors per track */
    BAM[track << 2]++;
    /* free the block */
    BAM[(track << 2) + 1 + (sector >> 3)] |= (1 << (sector & 7));
    return true;

  case Im1581:
    {
      BYTE **BAMblocks = NULL;

      if(track > image->partTops[image->dirtrack - 1] ||
	 track < image->partBots[image->dirtrack - 1])
	return false;

      if (2 != mapInode (&BAMblocks, image, image->dirtrack, 1)) {
	if (BAMblocks) free (BAMblocks);
	return false;
      }

      if(track > 40) {
	BAM = BAMblocks[1];

	track -= 40;
      }
      else
	BAM = BAMblocks[0];

      free (BAMblocks);

      BAM[16 + (track - 1) * 6] += 1;
      BAM[16 + (track - 1) * 6 + (sector >> 3) + 1] |= (1 << (sector & 7));
      return true;
    }
  }

  return false;
}

/*
** Determine if the block at the specified track and sector is free.
*/
static bool isFreeBlock (const Image *image, BYTE track, BYTE sector)
{
  const DiskGeometry *geom;

  if (!image || !image->buf || !(geom = getGeometry (image->type)))
    return false;

  if (track < 1 || track > geom->tracks || sector >= geom->sectors[track])
    return false; /* illegal track or sector */

  switch (image->type) {
    const BYTE *BAM;

  case ImUnknown:
    return false;
  case Im1571:
    if (track > 35) {
      track -= 36;

      if (!(BAM = getBlock ((Image*)image, image->dirtrack + 35, 0)))
	return false;

      return true && BAM[(track * 3) + (sector >> 3)] & (1 << (sector & 7));
    }
    /* fall through: track <= 35 */
  case Im1541:
    if (!(BAM = getBlock ((Image*)image, image->dirtrack, 0)))
      return false;

    return true && BAM[(track << 2) + 1 + (sector >> 3)] & (1 << (sector & 7));

  case Im1581:
    if(track > image->partTops[image->dirtrack - 1] ||
       track < image->partBots[image->dirtrack - 1])
      return false;

    if (!(BAM = getBlock ((Image*)image, image->dirtrack, 1)))
      return false;

    if(track > 40) {
      if (!(BAM = getBlock ((Image*)image, BAM[0], BAM[1])))
	return false;

      track -= 40;
    }

    return true && (BAM[16 + (track - 1) * 6 + (sector >> 3) + 1] &
		    (1 << (sector & 7)));
  }

  return false;
}

/*
** Find the next free block that is closest to the specified track and sector.
*/
static bool findNextFree (const Image *image, BYTE *track, BYTE *sector)
{
  const DiskGeometry *geom;
  int t, s, i;
  t = *track;
  s = *sector;

  if (!image || !image->buf || !(geom = getGeometry (image->type)))
    return false;

  if (t < 1 || t > geom->tracks || s < 0 || s >= geom->sectors[t])
    return false;

  if (t >= image->dirtrack) {
    /* search from the current track upwards */

    for (; t <= image->partTops[image->dirtrack - 1]; t++)
      for (i = geom->sectors[t]; i; i--) {
	if (isFreeBlock (image, t, s)) {
	  *track = t;
	  *sector = s;
	  return true;
	}
	s += geom->interleave[t];
	s %= geom->sectors[t];
      }

    /* search from lower tracks (from the directory track downwards) */

    for (t = image->dirtrack - 1;
	 t >= image->partBots[image->dirtrack - 1]; t--)
      for (i = geom->sectors[t]; i; i--) {
	if (isFreeBlock (image, t, s)) {
	  *track = t;
	  *sector = s;
	  return true;
	}
	s += geom->interleave[t];
	s %= geom->sectors[t];
      }
  }
  else {
    /* search from the current track downwards */

    for (; t >= image->partBots[image->dirtrack - 1]; t--)
      for (i = geom->sectors[t]; i; i--) {
	if (isFreeBlock (image, t, s)) {
	  *track = t;
	  *sector = s;
	  return true;
	}
	s += geom->interleave[t];
	s %= geom->sectors[t];
      }

    /* search from upper tracks (from the directory track upwards) */

    for (t = image->dirtrack + 1;
	 t <= image->partTops[image->dirtrack - 1]; t++)
      for (i = geom->sectors[t]; i; i--) {
	if (isFreeBlock (image, t, s)) {
	  *track = t;
	  *sector = s;
	  return true;
	}
	s += geom->interleave[t];
	s %= geom->sectors[t];
      }

    /* last resort: search from the directory track */
    t = image->dirtrack;

    for (i = geom->sectors[t]; i; i--) {
      if (isFreeBlock (image, t, s)) {
	*track = t;
	*sector = s;
	return true;
      }
      s += geom->interleave[t];
      s %= geom->sectors[t];
    }
  }

  return false;
}

/*
** Determine the number of free blocks on the disk image.
*/
static int blocksFree (const Image *image)
{
  unsigned sum = 0;
  const DiskGeometry *geom;

  if (!image || !image->buf || !(geom = getGeometry (image->type)))
    return 0;

  switch (image->type) {
    const BYTE *BAM;
    BYTE track;

  case ImUnknown:
    return 0;

  case Im1541:
    if (!(BAM = getBlock ((Image*)image, image->dirtrack, 0)))
      return 0;

    for (track = 1; track <= geom->tracks; track++)
      sum += BAM[track << 2];

    return sum;

  case Im1571:
    if (!(BAM = getBlock ((Image*)image, image->dirtrack, 0)))
      return 0;

    for (track = 1; track <= 35; track++)
      sum += BAM[track << 2] + BAM[0xDC + track];

    return sum;

  case Im1581:
    {
      BYTE **BAMblocks = NULL;

      if (2 != mapInode (&BAMblocks, (Image*)image, image->dirtrack, 1)) {
	if (BAMblocks) free (BAMblocks);
	return 0;
      }

      for (track = image->partBots[image->dirtrack - 1];
	   track <= image->partTops[image->dirtrack - 1] && track <= 40;
	   track++)
	sum += BAMblocks[0][16 + (track - 1)*6];

      for (track = image->partTops[image->dirtrack - 1];
	   track >= image->partBots[image->dirtrack - 1] && track > 40;
	   track--)
	sum += BAMblocks[1][16 + (track - 41) * 6];

      free (BAMblocks);

      return sum;
    }
  }

  return 0;
}

/*
** Make a back-up copy of the disk image's Block Availability Map.
*/
static bool backupBAM (const Image *image, BYTE **BAM)
{
  const DiskGeometry *geom;

  if (!BAM || *BAM ||
      !image || !image->buf || !(geom = getGeometry (image->type)))
    return false;

  switch (image->type) {
    const BYTE *bamblock;

  case ImUnknown:
    return false;
  case Im1541:
    if (!(bamblock = getBlock ((Image*)image, image->dirtrack, 0)))
      return false;

    if (!(*BAM = malloc (geom->tracks << 2)))
      return false;

    memcpy (*BAM, &bamblock[4], geom->tracks << 2);

    return true;

  case Im1571:
    if (!(bamblock = getBlock ((Image*)image, image->dirtrack, 0)))
      return false;

    if (!(*BAM = malloc (geom->tracks << 2)))
      return false;

    memcpy (*BAM, &bamblock[4], 35 << 2);
    memcpy (BAM[35 << 2], &bamblock[0xDD], 35);

    if (!(bamblock = getBlock ((Image*)image, 35 + image->dirtrack, 0))) {
      free (*BAM);
      *BAM = NULL;
      return false;
    }

    memcpy (BAM[35 * 5], bamblock, 35 * 3);
    return true;

  case Im1581:
    {
      BYTE **bamblocks = NULL;

      if (2 != mapInode (&bamblocks, (Image*)image, image->dirtrack, 1)) {
	if (bamblocks) free (bamblocks);
	return false;
      }

      if (!(*BAM = malloc (2 << 8)))
	return false;

      memcpy (*BAM, bamblocks[0], 256);
      memcpy (*BAM + 256, bamblocks[1], 256);
      free (bamblocks);
      return true;
    }
  }

  return false;
}

/*
** Restore a back-up copy of the disk image's Block Availability Map.
*/
static bool restoreBAM (Image *image, BYTE **BAM)
{
  const DiskGeometry *geom;

  if (!BAM || !*BAM ||
      !image || !image->buf || !(geom = getGeometry (image->type)))
    return false;

  switch (image->type) {
  case ImUnknown:
    return false;
  case Im1541:
    {
      BYTE *bamblock;

      if (!(bamblock = getBlock (image, image->dirtrack, 0)))
	return false;

      memcpy (&bamblock[4], *BAM, geom->tracks << 2);
      free (*BAM);
      *BAM = NULL;

      return true;
    }
  case Im1571:
    {
      BYTE *bamblock;

      if (!(bamblock = getBlock (image, image->dirtrack, 0)))
	return false;

      memcpy (&bamblock[4], *BAM, 35 << 2);
      memcpy (&bamblock[0xDD], BAM[35 << 2], 35);

      if (!(bamblock = getBlock (image, 35 + image->dirtrack, 0)))
	return false;

      memcpy (bamblock, BAM[35 * 5], 35 * 3);
      free (*BAM);
      *BAM = NULL;

      return true;
    }
  case Im1581:
    {
      BYTE **bamblocks = NULL;

      if (2 != mapInode (&bamblocks, image, image->dirtrack, 1)) {
	if (bamblocks) free (bamblocks);
	return false;
      }

      memcpy (bamblocks[0], *BAM, 256);
      memcpy (bamblocks[1], *BAM + 256, 256);

      free (bamblocks);
      free (BAM);
      *BAM = NULL;

      return true;
    }
  }

  return false;
}

/*
** Generate a CP/M translation table.
*/
static BYTE **CpmTransTable (Image *image, unsigned *au, unsigned *sectors)
{
  BYTE **table = NULL, *trackbuf;
  const DiskGeometry *geom;

  unsigned track = 1, sector = 10, sectorcount = 2;
  unsigned block;

  if (!image || !au || !image->buf ||
      !(geom = getGeometry (image->type)))
    return NULL;

  /* Set the allocation unit size and the number of sectors. */

  switch (geom->blocks) {
  case 683: /* 1541 */
    *au = 8;
    if ((table = calloc (*sectors = 680, sizeof (*table))))
      for (block = 0, trackbuf = image->buf; block < *sectors; block++) {
	table[block] = &trackbuf[sector << 8];
	sector = (sector + 5) % geom->sectors[track];

	if (++sectorcount == geom->sectors[track]) {
	  trackbuf += geom->sectors[track++] << 8;

	  if (track == geom->dirtrack) {
	    sectorcount = 1;
	    sector = 5;
	  } else {
	    sectorcount = 0;
	    sector = 0;
	  }
	}
      }

    break;
  case 1366: /* 1571 */
    *au = 8;
    if ((table = calloc (*sectors = 1360, sizeof (*table))))
      for (block = 0, trackbuf = image->buf; block < *sectors; block++) {
	table[block] = &trackbuf[sector << 8];
	sector = (sector + 5) % geom->sectors[track];

	if (++sectorcount == geom->sectors[track]) {
	  trackbuf += geom->sectors[track++] << 8;

	  if (track == 36) {
	    sectorcount = 2;
	    sector = 10;
	  } else if (track % 36 == geom->dirtrack) {
	    sectorcount = 1;
	    sector = 5;
	  } else {
	    sectorcount = 0;
	    sector = 0;
	  }
	}
      }

    break;
  case 3200: /* 1581 */
    *au = 16;
    sector = sectorcount = 0;
    if ((table = calloc (*sectors = 3180, sizeof (*table))))
      for(block = 0, trackbuf = image->buf; block < *sectors; block++) {
	table[block] = &trackbuf[sector << 8];
	sector = (sector + 1) % geom->sectors[track];

	if (++sectorcount == geom->sectors[track]) {
	  trackbuf += geom->sectors[track++] << 8;
	  sectorcount = sector = (track == geom->dirtrack) ? 20 : 0;
	}
      }
    break;
  }

  return table;
}

/*
** Convert the CP/M directory entry to a PETSCII file name.
*/
static void CpmConvertName (const CpmDirEnt *dirent, Filename *name)
{
  char cpmname[13];
  int i, j;

  for (i = 0; i < sizeof(dirent->basename); i++)
    cpmname[i] = dirent->basename[i] & 0x7f;

  while (cpmname[i - 1] == ' ') i--;

  for (cpmname[i++] = '.', j = 0; j < sizeof(dirent->suffix); j++)
    cpmname[i++] = dirent->suffix[j] & 0x7f;

  while (cpmname[i - 1] == ' ') i--;
  if (cpmname[i - 1] == '.') i--;
  cpmname [i] = 0;

  /* Convert the ASCII name to PETSCII name */
  for (i = 0; cpmname[i] && i < sizeof(name->name); i++) {
    if (cpmname[i] >= 'A' && cpmname[i] <= 'Z')
      name->name[i] = cpmname[i] - 'A' + 0xC1;
    else if (cpmname[i] >= 'a' && cpmname[i] <= 'z')
      name->name[i] = cpmname[i] - 'a' + 0x41;
    else
      name->name[i] = cpmname[i];
  }
  while (i < sizeof(name->name))
    name->name[i++] = 0xA0; /* pad with shifted spaces */

  name->type = PRG;
  name->recordLength = 0;
}
