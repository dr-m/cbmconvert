/**
 * @file image.c
 * Disk image management
 * @author Marko Mäkelä (marko.makela@nic.funet.fi)
 * @author Pasi Ojala (albert@cs.tut.fi)
 */

/*
** Copyright © 1993-1998,2001 Marko Mäkelä
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
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

#include "output.h"
#include "input.h"

/* Disk geometry information structure */
struct DiskGeometry
{
  /** disk image type identifier */
  enum ImageType type;
  /** disk image size */
  size_t blocks;
  /** format specifier */
  char formatID;
  /** number of BAM blocks */
  unsigned BAMblocks;
  /** directory track number */
  unsigned dirtrack;
  /** number of disk tracks */
  unsigned tracks;
  /** number of sectors per track */
  unsigned* sectors;
  /** sector interleaves (number of sectors to advance) */
  unsigned* interleave;
};

/* Disk directory entry */
struct DirEnt
{
  /** track of next directory block (only the first dirent of the block) */
  byte_t nextTrack;
  /** sector of next directory block (only the first dirent of the block) */
  byte_t nextSector;
  /** Commodore file type */
  byte_t type;
  /** track of the first file data block */
  byte_t firstTrack;
  /** sector of the first file data block */
  byte_t firstSector;
  /** Commodore file name */
  byte_t name[16];
  /** track of the first side sector (relative files only) */
  byte_t ssTrack;
  /** sector of the first side sector (relative files only) */
  byte_t ssSector;
  /** relative file record length */
  byte_t recordLength;
  /** info block track of GEOS file */
#define infoTrack ssTrack
  /** info block sector of GEOS file */
#define infoSector ssSector
  /** GEOS file format: 1=VLIR, 0=sequential */
#define isVLIR recordLength
  /** GEOS time stamp (otherwise unused bytes) */
  struct {
    byte_t type;	/**< GEOS file type */
    byte_t year;	/**< GEOS file time stamp: year */
    byte_t month;	/**< GEOS file time stamp: month */
    byte_t day;		/**< GEOS file time stamp: day */
    byte_t hour;	/**< GEOS file time stamp: hour */
    byte_t minute;	/**< GEOS file time stamp: minute */
  } geos;
  /** file's total block count, least significant byte */
  byte_t blocksLow;
  /** file's total block count, most significant byte */
  byte_t blocksHigh;
};

/** CP/M disk directory entry */
struct CpmDirEnt
{
  /** user area 0-0xF or 0xE5 (unused entry) */
  byte_t area;
  /** file base name (bits 0..6);<br>
   * basename[0]..basename[3]: bit7=1 for user defined attributes 1 to 4;<br>
   * basename[4]..basename[7]: bit7=1 for system interface attributes (BDOS) */
  byte_t basename[8];
  /** file suffix (bits 0..6);<br>
   * suffix[0]: bit7=1 for read-only;<br>
   * suffix[1]: bit7=1 for system file;<br>
   * suffix[2]: bit7=1 for archive */
  byte_t suffix[3];
  /** number of directory extent */
  byte_t extent;
  /** unused bytes */
  byte_t unused[2];
  /** number of 128-byte blocks in this extent (max $80) */
  byte_t blocks;
  /** file block pointers, in this case 8-bit */
  byte_t block[16];
};

/** Calculate the allocation blocks of a CP/M file
 * @param block		the file block pointers
 * @param i		index to the file block pointers
 * @return		the corresponding allocation block
 */
#define CPMBLOCK(block,i) \
	(au == 8 ? block[i] : block[2 * (i)] + (block[2 * (i) + 1] << 8))

/** table of sectors per track on the 1541 */
static unsigned sect1541[] =
{
  21, 21, 21, 21, 21, 21, 21, 21, 21, /* tracks  1 .. 9  */
  21, 21, 21, 21, 21, 21, 21, 21,     /* tracks 10 .. 17 */
  19, 19, 19, 19, 19, 19, 19,         /* tracks 18 .. 24 */
  18, 18, 18, 18, 18, 18,             /* tracks 25 .. 30 */
  17, 17, 17, 17, 17                  /* tracks 31 .. 35 */
};

/** table of sectors per track on the 1571 */
static unsigned sect1571[] =
{
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

/** table of sectors per track on the 1581 */
static unsigned sect1581[] =
{
  40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
  40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
  40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
  40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
  40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
  40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
  40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
  40, 40, 40, 40, 40, 40, 40, 40, 40, 40
};

/** table of interleave per track on the 1541 */
static unsigned int1541[] = {
  10, 10, 10, 10, 10, 10, 10, 10, 10, /* tracks  1 .. 9  */
  10, 10, 10, 10, 10, 10, 10, 10,     /* tracks 10 .. 17 */
   3, 10, 10, 10, 10, 10, 10,         /* tracks 18 .. 24 */
  10, 10, 10, 10, 10, 10,             /* tracks 25 .. 30 */
  10, 10, 10, 10, 10                  /* tracks 31 .. 35 */
};

/** table of interleave per track on the 1571 */
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

/** table of interleave per track on the 1581 */
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

/** The disk geometry database.  If you add more drives, you will need
 * to update the ImageType definition in output.h and all related
 * switch statements in this file.
 */
static struct DiskGeometry diskGeometry[] =
{
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

/** Determine disk image geometry.
 * @param type	the disk image type
 * @return	the corresponding geometry, or NULL if none found
 */
static const struct DiskGeometry*
getGeometry (enum ImageType type)
{
  int i;

  for (i = elementsof(diskGeometry); i--; )
    if (diskGeometry[i].type == type)
      return &diskGeometry[i];

  return 0;
}

/** Get a pointer to the block in the specified track and sector.
 * @param image		the disk image
 * @param track		the track number
 * @param sector	the sector number
 * @return		pointer to the first byte in the sector, or NULL
 */
static byte_t*
getBlock (struct Image* image,
	  byte_t track,
	  byte_t sector)
{
  const struct DiskGeometry* geom;
  int t, b;

  if (!image || !image->buf || !(geom = getGeometry (image->type)))
    return 0;

  if (track < 1 || track > geom->tracks || sector >= geom->sectors[track])
    return 0; /* illegal track or sector */

  for (t = 1, b = 0; t < track; t++)
    b += geom->sectors[t];

  b += sector;

  return &image->buf[b << 8];
}

/** Determine if the block at the specified track and sector is free.
 * @param image		the disk image
 * @param track		the track number
 * @param sector	the sector number
 * @return		true if the block is available
 */
static bool
isFreeBlock (const struct Image* image, byte_t track, byte_t sector)
{
  const struct DiskGeometry* geom;

  if (!image || !image->buf || !(geom = getGeometry (image->type)))
    return false;

  if (track < 1 || track > geom->tracks || sector >= geom->sectors[track])
    return false; /* illegal track or sector */

  switch (image->type) {
    const byte_t* BAM;

  case ImUnknown:
    return false;
  case Im1571:
    if (track > 35) {
      track -= 36;

      if (!(BAM = getBlock ((struct Image*) image, image->dirtrack + 35, 0)))
	return false;

      return true && BAM[(track * 3) + (sector >> 3)] & (1 << (sector & 7));
    }
    /* fall through: track <= 35 */
  case Im1541:
    if (!(BAM = getBlock ((struct Image*) image, image->dirtrack, 0)))
      return false;

    return true && BAM[(track << 2) + 1 + (sector >> 3)] & (1 << (sector & 7));

  case Im1581:
    if(track > image->partTops[image->dirtrack - 1] ||
       track < image->partBots[image->dirtrack - 1])
      return false;

    if (!(BAM = getBlock ((struct Image*) image, image->dirtrack, 1)))
      return false;

    if(track > 40) {
      if (!(BAM = getBlock ((struct Image*) image, BAM[0], BAM[1])))
	return false;

      track -= 40;
    }

    return true && (BAM[16 + (track - 1) * 6 + (sector >> 3) + 1] &
		    (1 << (sector & 7)));
  }

  return false;
}

/** Find the next free block that is closest to the specified track and sector.
 * @param image		the disk image
 * @param track		(input/output) the track number
 * @param sector	(input/output) the sector number
 * @return		true if a block was found
 */
static bool
findNextFree (const struct Image* image,
	      byte_t* track,
	      byte_t* sector)
{
  const struct DiskGeometry* geom;
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

/** Get a block pointer table to all blocks in the file
 * starting at the specified track and sector.
 * @param buf		the block pointer table
 * @param image		the disk image
 * @param track		track number of the file's first block
 * @param sector	sector number of the file's first block
 * @param log		Call-back function for diagnostic output
 * @param dirent	directory entry for diagnostic output (optional)
 * @return		the number of blocks mapped (0 on failure)
 */
static size_t
mapInode (byte_t*** buf, struct Image* image, byte_t track, byte_t sector,
	  log_t log, const struct DirEnt* dirent)
{
  const struct DiskGeometry* geom;
  int t, s;
  size_t size;
  byte_t* block;

  if (!buf || *buf ||
      !image || !image->buf || !(geom = getGeometry (image->type)))
    return 0;

  /* Determine the number of blocks. */
  for (t = track, s = sector, size = 0; t; size++) {
    if (size > geom->blocks)
      return 0; /* endless file */

    if (!(block = getBlock (image, t, s)))
      return 0;

    if (isFreeBlock (image, t, s)) {
      if (!log)
	return 0;
      else {
	struct Filename name;
	if (dirent) {
	  memcpy (name.name, dirent->name, sizeof name.name);
	  name.type = dirent->type;
	  name.recordLength = dirent->recordLength;
	}
	(*log) (Warnings, dirent ? &name : 0,
		"Unallocated block %u,%u reachable from %u,%u",
		t, s, track, sector);
      }
    }

    t = block[0];
    s = block[1];
  }

  /* Set up the block pointer table. */

  if (!(*buf = malloc (size * sizeof **buf)))
    return 0;

  for (t = track, s = sector, size = 0; t; size++) {
    if (!((*buf)[size] = getBlock (image, t, s))) {
      free (*buf);
      *buf = 0;
      return 0;
    }

    t = (*buf)[size][0];
    s = (*buf)[size][1];
  }

  return size;
}

/** Allocate the block at the specified track and sector.
 * Set the track and sector to the next block candidate.
 * @param image		the disk image
 * @param track		(input/output) the track number
 * @param sector	(input/output) the sector number
 * @return		true if a block was allocated
 */
static bool
allocBlock (struct Image* image,
	    byte_t* track,
	    byte_t* sector)
{
  const struct DiskGeometry* geom;

  if (!track || !sector ||
      !image || !image->buf || !(geom = getGeometry (image->type)))
    return false;

  if (*track < 1 || *track > geom->tracks || *sector >= geom->sectors[*track])
    return false; /* illegal track or sector */

  switch (image->type) {
    byte_t* BAM;

  case ImUnknown:
    return false;
  case Im1571:
    if (*track > 35) {
      byte_t tr = *track - 35;
      byte_t* BAM2;

      if (!(BAM = getBlock ((struct Image*) image, image->dirtrack, 0)) ||
	  !(BAM2 = getBlock ((struct Image*) image, 35 + image->dirtrack, 0)))
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
    if (!(BAM = getBlock ((struct Image*) image, image->dirtrack, 0)))
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
      byte_t** BAMblocks = 0;
      byte_t offset;

      if(*track > image->partTops[image->dirtrack - 1] ||
	 *track < image->partBots[image->dirtrack - 1])
	return false;

      if (2 != mapInode (&BAMblocks, image, image->dirtrack, 1, 0, 0)) {
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

/** Format disk image.
 * @param image		the image to be formatted
 */
static void
FormatImage (struct Image* image)
{
  const struct DiskGeometry* geom;
  const char id1 = '9'; /* disk ID characters */
  const char id2 = '8';
  const char title[] = "CBMCONVERT   2.0"; /* disk title */

  if (!image || !image->buf || !(geom = getGeometry (image->type)))
    return;

  /* Clear all sectors */
  memset (image->buf, 0, geom->blocks * 256);

  switch (image->type) {
    byte_t track, sector;
    byte_t* BAM;
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
      byte_t* tmp = BAM + 16 + (track - 1) * 6;

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
      byte_t* tmp = BAM + 16 + (track - 41) * 6;

      /* set amount of free blocks on each track */

      tmp[0] = 40;
      tmp[1] = tmp[2] = tmp[3] = tmp[4] = tmp[5] = 0xff;
    }

    break;
  }
}

/** Read a file starting at the specified track and sector to a buffer
 * @param buf		the buffer
 * @param image		the disk image
 * @param track		track number of the file's first block
 * @param sector	sector number of the file's first block
 * @return		the file length, or 0 on error
 */
static size_t
readInode (byte_t** buf,
	   const struct Image* image,
	   byte_t track, byte_t sector)
{
  const struct DiskGeometry* geom;
  int t, s;
  size_t size;

  if (!buf || *buf ||
      !image || !image->buf || !(geom = getGeometry (image->type)))
    return 0;

  /* Determine the file size. */
  for (t = track, s = sector, size = 0; t; size += 254) {
    const byte_t* block;

    if (size > 254 * geom->blocks)
      return 0; /* endless file */

    if (!(block = getBlock ((struct Image*) image, t, s)))
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
    const byte_t* block = getBlock ((struct Image*) image, t, s);
    t = block[0];
    s = block[1];
    memcpy (&(*buf)[size], &block[2], t ? 254 : s - 1);
  }

  return size += s - 255;
}

/** Make a back-up copy of the disk image's Block Availability Map.
 * @param image		the disk image
 * @param BAM		(output) the BAM backup
 * @return		true on success
 */
static bool
backupBAM (const struct Image* image, byte_t** BAM)
{
  const struct DiskGeometry* geom;

  if (!BAM || *BAM ||
      !image || !image->buf || !(geom = getGeometry (image->type)))
    return false;

  switch (image->type) {
    const byte_t* bamblock;

  case ImUnknown:
    return false;
  case Im1541:
    if (!(bamblock = getBlock ((struct Image*) image, image->dirtrack, 0)))
      return false;

    if (!(*BAM = malloc (geom->tracks << 2)))
      return false;

    memcpy (*BAM, &bamblock[4], geom->tracks << 2);

    return true;

  case Im1571:
    if (!(bamblock = getBlock ((struct Image*) image, image->dirtrack, 0)))
      return false;

    if (!(*BAM = malloc (geom->tracks << 2)))
      return false;

    memcpy (*BAM, &bamblock[4], 35 << 2);
    memcpy (BAM[35 << 2], &bamblock[0xDD], 35);

    if (!(bamblock = getBlock ((struct Image*) image,
			       35 + image->dirtrack, 0))) {
      free (*BAM);
      *BAM = 0;
      return false;
    }

    memcpy (BAM[35 * 5], bamblock, 35 * 3);
    return true;

  case Im1581:
    {
      byte_t** bamblocks = 0;

      if (2 != mapInode (&bamblocks, (struct Image*) image,
			 image->dirtrack, 1, 0, 0)) {
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


/** Restore a back-up copy of the disk image's Block Availability Map.
 * @param image		the disk image
 * @param BAM		the BAM backup
 * @return		true on success
 */
static bool
restoreBAM (struct Image* image, byte_t** BAM)
{
  const struct DiskGeometry* geom;

  if (!BAM || !*BAM ||
      !image || !image->buf || !(geom = getGeometry (image->type)))
    return false;

  switch (image->type) {
  case ImUnknown:
    return false;
  case Im1541:
    {
      byte_t* bamblock;

      if (!(bamblock = getBlock (image, image->dirtrack, 0)))
	return false;

      memcpy (&bamblock[4], *BAM, geom->tracks << 2);
      free (*BAM);
      *BAM = 0;

      return true;
    }
  case Im1571:
    {
      byte_t* bamblock;

      if (!(bamblock = getBlock (image, image->dirtrack, 0)))
	return false;

      memcpy (&bamblock[4], *BAM, 35 << 2);
      memcpy (&bamblock[0xDD], BAM[35 << 2], 35);

      if (!(bamblock = getBlock (image, 35 + image->dirtrack, 0)))
	return false;

      memcpy (bamblock, BAM[35 * 5], 35 * 3);
      free (*BAM);
      *BAM = 0;

      return true;
    }
  case Im1581:
    {
      byte_t** bamblocks = 0;

      if (2 != mapInode (&bamblocks, image, image->dirtrack, 1, 0, 0)) {
	if (bamblocks) free (bamblocks);
	return false;
      }

      memcpy (bamblocks[0], *BAM, 256);
      memcpy (bamblocks[1], *BAM + 256, 256);

      free (bamblocks);
      free (BAM);
      *BAM = 0;

      return true;
    }
  }

  return false;
}

/** Write a file to the disk, starting from the specified track and sector.
 * @param image		the disk image
 * @param track		track number of the first file block
 * @param sector	sector number of the first file block
 * @param buf		the file contents
 * @param size		length of the file contnets
 * @return		status of the operation
 */
static enum WrStatus
writeInode (struct Image* image,
	    byte_t track, byte_t sector,
	    const byte_t* buf, size_t size)
{
  byte_t t, s;
  size_t count;
  byte_t* oldBAM = 0;

  if (!buf || !image || !image->buf)
    return WrFail;

  /* Make a copy of the BAM. */

  if (!backupBAM (image, &oldBAM))
    return WrFail;

  /* Write the file. */
  for (t = track, s = sector, count = 0; count < size; count += 254) {
    byte_t* block;

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

/** Free the block at the specified track and sector.
 * @param image		the disk image
 * @param track		track number of the block
 * @param sector	sector number of the block
 * @return		true on success
 */
static bool
freeBlock (struct Image* image, byte_t track, byte_t sector)
{
  const struct DiskGeometry* geom;

  if (!image || !image->buf || !(geom = getGeometry (image->type)))
    return false;

  if (track < 1 || track > geom->tracks || sector >= geom->sectors[track])
    return false; /* illegal track or sector */

  if (isFreeBlock (image, track, sector))
    return false; /* already freed */

  switch (image->type) {
    byte_t* BAM;

  case ImUnknown:
    return false;
  case Im1571:
    if (track > 35) {
      byte_t tr = track - 35;
      byte_t* BAM2;

      if (!(BAM = getBlock ((struct Image*) image, image->dirtrack, 0)) ||
	  !(BAM2 = getBlock ((struct Image*) image, 35 + image->dirtrack, 0)))
	return false;

      /* increment the count of free sectors per track */
      BAM[0xDC + tr]++;
      /* free the block */
      BAM2[((tr - 1) * 3) + (sector >> 3)] |= (1 << (sector & 7));
      return true;
    }
    /* fall through: track <= 35 */
  case Im1541:
    if (!(BAM = getBlock ((struct Image*) image, image->dirtrack, 0)))
      return false;

    /* increment the count of free sectors per track */
    BAM[track << 2]++;
    /* free the block */
    BAM[(track << 2) + 1 + (sector >> 3)] |= (1 << (sector & 7));
    return true;

  case Im1581:
    {
      byte_t** BAMblocks = 0;

      if(track > image->partTops[image->dirtrack - 1] ||
	 track < image->partBots[image->dirtrack - 1])
	return false;

      if (2 != mapInode (&BAMblocks, image, image->dirtrack, 1, 0, 0)) {
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

/** Wipe out and delete the file starting at the specified track and sector.
 * @param image		the disk image
 * @param track		track number of the first file block
 * @param sector	sector number of the first file block
 * @param do_it		flag: really remove the file
 * @return		status of the operation
 */
static enum ImStatus
deleteInode (struct Image* image,
	     byte_t track, byte_t sector,
	     bool do_it)
{
  int t, s;

  if (!image || !image->buf)
    return ImFail;

  /* Make sure that the whole file has been allocated. */
  for (t = track, s = sector; t; ) {
    byte_t* block;

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
      byte_t* block = getBlock (image, t, s);
      freeBlock (image, t, s);
      t = block[0];
      s = block[1];
      /* clear the block */
      memset (block, 0, 256);
    }
  }

  return ImOK;
}

/** Find the directory corresponding to a file
 * @param image	the disk image
 * @param name	the Commodore file name
 * @return	the corresponding directory entry, or NULL
 */
static struct DirEnt*
getDirEnt (struct Image* image,
	   const struct Filename* name)
{
  const struct DiskGeometry* geom;
  struct DirEnt** directory = 0;
  struct DirEnt* dirent = 0;
  unsigned block, i;
  int freeslotBlock = -1, freeslotEntry = -1;

  if (!name || !image || !image->buf || !(geom = getGeometry (image->type)))
    return 0;

  /* Read the current directory. */

  if (!(block = mapInode ((byte_t***)&directory, image, image->dirtrack, 0,
			  0, 0)))
    return 0;

  /* Check that the directory is long enough to hold the BAM blocks
     and at least one directory sector */
  if (block < geom->BAMblocks) {
    free (directory);
    return 0;
  }

  /* Search for the name in the directory. */
  for (block = geom->BAMblocks; ; block++) {
    for (i = 0; i * sizeof (struct DirEnt) < (directory[block]->nextTrack
					      ? 256
					      : directory[block]->nextSector);
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
    return 0;
  }

  if (freeslotBlock == -1) {
    /* Append a directory entry */

    if (i < 256 / sizeof *dirent) {
      /* grow the directory by growing its last sector */

      directory[block]->nextSector = (sizeof *dirent) *
	(1 + directory[block]->nextSector / sizeof *dirent);
      freeslotBlock = block;
      freeslotEntry = i;
    }
    else {
      /* allocate a new directory block */

      byte_t track, sector;
      byte_t t, s;

      track = image->dirtrack;
      sector = geom->BAMblocks;

      if (!findNextFree (image, &track, &sector)) {
	free (directory);
	return 0;
      }

      t = directory[block]->nextTrack = track;
      s = directory[block]->nextSector = sector;

      if (!allocBlock (image, &t, &s)) {
	directory[block]->nextTrack = 0;
	directory[block]->nextSector = 0xFF;
	free (directory);
	return 0;
      }

      /* Remap the directory from the disk image */

      free (directory);
      directory = 0;

      if (!mapInode ((byte_t***)&directory, image, image->dirtrack, 0, 0, 0))
	return 0;

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
    memset (dirent, 0, sizeof *dirent);
  else
    memset (&dirent->type, 0, (sizeof *dirent) - 2);

  free (directory);
  return dirent;
}

#ifdef DEBUG
/** Determine if a directory entry is valid
 * @param image		the disk image
 * @param dirent	the directory entry
 * @return		true if the directory entry is valid
 */
static bool
isValidDirEnt (const struct Image* image,
	       const struct DirEnt* dirent)
{
  const struct DiskGeometry* geom;

  if (!image || !image->buf || !(geom = getGeometry (image->type)))
    return false;

  if ((byte_t*) dirent < image->buf ||
      (byte_t*) dirent > &image->buf[geom->blocks << 8] ||
      ((byte_t*) dirent - image->buf) % sizeof *dirent)
    return false;

  return true;
}
#endif

/** Determine whether a directory entry represents a GEOS file
 * @param dirent	the directory entry
 * @return		true if it is a GEOS directory entry
 */
static bool
isGeosDirEnt (const struct DirEnt* dirent)
{
  enum Filetype type = dirent->type & 0x8F;

  return type >= DEL && type < REL && dirent->geos.type != 0 &&
    (dirent->isVLIR == 0 || dirent->isVLIR == 1);
}

/** Determine the file type of a directory entry
 * @param image		the disk image
 * @param dirent	the directory entry
 * @return		the file type of the directory entry, or 0
 */
static enum Filetype
getFiletype (const struct Image* image,
	     const struct DirEnt* dirent)
{
  enum Filetype type;

#ifdef DEBUG
  if (!isValidDirEnt (image, dirent))
    return 0;
#endif

  type = dirent->type & 0x8F;

  if (type < DEL || type > (image->type == Im1581 ? CBM : REL))
    return 0; /* illegal file type */

  return type;
}

/** Remove a directory entry and the files it is pointing to
 * @param image		the disk image
 * @param dirent	the directory entry
 * @return		status of the operation
 */
static enum ImStatus
deleteDirEnt (struct Image* image,
	      struct DirEnt* dirent)
{
  enum ImStatus status;

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
      const byte_t* vlir = getBlock (image, dirent->firstTrack,
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

/** Determine the number of free blocks on the disk image.
 * @param image		the disk image
 * @return		the total number of available blocks
 */
static int
blocksFree (const struct Image* image)
{
  unsigned sum = 0;
  const struct DiskGeometry* geom;

  if (!image || !image->buf || !(geom = getGeometry (image->type)))
    return 0;

  switch (image->type) {
    const byte_t* BAM;
    byte_t track;

  case ImUnknown:
    return 0;

  case Im1541:
    if (!(BAM = getBlock ((struct Image*) image, image->dirtrack, 0)))
      return 0;

    for (track = 1; track <= geom->tracks; track++)
      sum += BAM[track << 2];

    return sum;

  case Im1571:
    if (!(BAM = getBlock ((struct Image*) image, image->dirtrack, 0)))
      return 0;

    for (track = 1; track <= 35; track++)
      sum += BAM[track << 2] + BAM[0xDC + track];

    return sum;

  case Im1581:
    {
      byte_t** BAMblocks = 0;

      if (2 != mapInode (&BAMblocks, (struct Image*) image,
			 image->dirtrack, 1, 0, 0)) {
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

/** Set up the side sector file for relative files.
 * @param image		the disk image
 * @param dirent	the directory entry
 * @param blocks	number of data blocks in the file
 * @param log		Call-back function for diagnostic output
 * @return		status of the operation
 */
static enum WrStatus
setupSideSectors (struct Image* image,
		  struct DirEnt* dirent,
		  size_t blocks,
		  log_t log)
{
  int sscount;

#ifdef DEBUG
  if (!isValidDirEnt (image, dirent) || getFiletype (image, dirent) != REL)
    return WrFail;
#endif

  if(image->type == Im1581)
    return WrFail; /* TODO: super side sector setup etc. */

  sscount = rounddiv (blocks, 120);

  if (sscount < 1)
    return WrFail;

  if (sscount > 6 || blocksFree (image) < sscount)
    /* too many side sector blocks */
    return WrNoSpace;

  if (!findNextFree (image, &dirent->ssTrack, &dirent->ssSector))
    return WrNoSpace;

  {
    byte_t* buf;
    enum WrStatus status;
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
    byte_t** sidesect = 0;
    byte_t** datafile = 0;
    byte_t ss, ssentry, i, track, sector;

    if (blocks != mapInode (&datafile, image,
			    dirent->firstTrack, dirent->firstSector,
			    log, dirent))
      return WrFail;

    if (sscount != mapInode (&sidesect, image,
			     dirent->ssTrack, dirent->ssSector,
			     log, dirent)) {
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

/** Check if the side sectors of a relative file are OK
 * @param image		the disk image
 * @param dirent	the directory entry
 * @param log		Call-back function for diagnostic output
 * @return		true if the side sectors pass the integrity checks
 */
static bool
checkSideSectors (const struct Image* image,
		  const struct DirEnt* dirent,
		  log_t log)
{
  unsigned ss, ssentry, sscount, i;
  byte_t** sidesect = 0;
  byte_t** datafile = 0;
  byte_t track, sector;

#ifdef DEBUG
  if (!isValidDirEnt (image, dirent))
    return false;
#endif

  if (getFiletype (image, dirent) != REL)
    return false;

  /* Map the data file and the side sectors. */

  if (!(i = mapInode (&datafile, (struct Image*) image,
		      dirent->firstTrack, dirent->firstSector, log, dirent)))
    return false;

  if (!(sscount = mapInode (&sidesect, (struct Image*) image,
			    dirent->ssTrack, dirent->ssSector,
			    log, dirent))) {
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

/** Generate a CP/M translation table.
 * @param image		the disk image
 * @param au		(output) the size of the allocation unit
 * @param sectors	(output) number of useable sectors
 * @return		the translation table, or NULL on error
 */
static byte_t**
CpmTransTable (struct Image* image,
	       unsigned* au,
	       unsigned* sectors)
{
  byte_t** table = 0;
  byte_t* trackbuf;
  const struct DiskGeometry* geom;

  unsigned track = 1, sector = 10, sectorcount = 2;
  unsigned block;

  if (!image || !au || !image->buf ||
      !(geom = getGeometry (image->type)))
    return 0;

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

/** Convert a CP/M directory entry to a PETSCII file name.
 * @param dirent	the CP/M directory entry
 * @param name		(output) the Commodore file name
 */
static void
CpmConvertName (const struct CpmDirEnt* dirent,
		struct Filename* name)
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

/** Write a file into a CP/M disk image.
 * @param name		native (PETSCII) name of the file
 * @param data		the contents of the file
 * @param length	length of the file contents
 * @param image		the disk image
 * @param log		Call-back function for diagnostic output
 * @return		status of the operation
 */
enum WrStatus
WriteCpmImage (const struct Filename* name,
	       const byte_t* data,
	       size_t length,
	       struct Image* image,
	       log_t log)
{
  byte_t** trans;
  struct CpmDirEnt** allocated;
  struct CpmDirEnt* dirent;
  struct CpmDirEnt cpmname;
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
	if (CPMBLOCK (dirent[d].block, i) < 2 ||
	    CPMBLOCK (dirent[d].block, i) >= 2 * sectors / au) {
	  struct Filename fn;
	  CpmConvertName (&dirent[d], &fn);
	  (*log) (Warnings, &fn,
		  "Illegal block address in block %u of extent 0x%02x",
		  i, dirent[d].extent);
	}
	else if (allocated[CPMBLOCK (dirent[d].block, i)]) {
	  struct Filename fn;
	  CpmConvertName (&dirent[d], &fn);
	  (*log) (Warnings, &fn, "Sector 0x%02x allocated multiple times",
		  CPMBLOCK (dirent[d].block, i));
	}
	else {
	  allocated[CPMBLOCK (dirent[d].block, i)] = &dirent[d];
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
    struct CpmDirEnt* de = 0;
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

/** Read and convert a disk image in C128 CP/M format
 * @param file		the file input stream
 * @param filename	host system name of the file
 * @param writeCallback	function for writing the contained files
 * @param log		Call-back function for diagnostic output
 * @return		status of the operation
 */
enum RdStatus
ReadCpmImage (FILE* file,
	      const char* filename,
	      write_file_t writeCallback,
	      log_t log)
{
  struct Image image;
  byte_t** trans;
  unsigned au; /* allocation unit size */
  unsigned sectors; /* number of disk sectors */

  /* determine disk image type from its length */
  {
    const struct DiskGeometry* geom = 0;
    size_t length, blocks;
    unsigned i;

    if (fseek (file, 0, SEEK_END)) {
    seekError:
      (*log) (Errors, 0, "fseek: %s", strerror(errno));
      return RdFail;
    }

    length = ftell (file);

    if (fseek (file, 0, SEEK_SET))
      goto seekError;

    if (length % 256) {
    unknownImage:
      (*log) (Errors, 0, "Unknown CP/M disk image type");
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
      (*log) (Errors, 0, "Out of memory");
      return RdFail;
    }

    image.type = geom->type;
    image.dirtrack = geom->dirtrack;
    image.name = 0;

    if (1 != fread (image.buf, length, 1, file)) {
      (*log) (Errors, 0, "fread: %s", strerror(errno));
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
    struct CpmDirEnt* directory;
    unsigned d;
    struct Filename name;

    for (d = 0; d < au * 8; d++) {
      unsigned i, j, length;

      directory = ((struct CpmDirEnt*) trans[d / 8]) + (d % 8);

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
	struct CpmDirEnt* dir = ((struct CpmDirEnt*) trans[i / 8]) + (i % 8);

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
	byte_t* curr, *buf = malloc (length);

	if (!buf) {
	  (*log) (Warnings, &name, "out of memory");
	  d += j - 1;
	  continue;
	}

	for (curr = buf, j += d; d < j; d++) {
	  directory = ((struct CpmDirEnt*) trans[d / 8]) + (d % 8);

	  for (i = 0; i < directory->blocks; i++) {
	    unsigned sect = (au / 2) * CPMBLOCK (directory->block, i / au) +
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

/** Write to an image in CBM DOS format
 * @param name		native (PETSCII) name of the file
 * @param data		the contents of the file
 * @param length	length of the file contents
 * @param image		the disk image
 * @param log		Call-back function for diagnostic output
 * @return		status of the operation
 */
enum WrStatus
WriteImage (const struct Filename* name,
	    const byte_t* data,
	    size_t length,
	    struct Image* image,
	    log_t log)
{
  struct DirEnt* dirent;

  if (!name || !data || !image || !image->buf || !getGeometry (image->type))
    return WrFail;

  /* See if it is a GEOS file. */
  if (name->type >= DEL && name->type < REL && length > 2 * 254 &&
      !strncmp ((char*)&data[sizeof (struct DirEnt) + 1],
		" formatted GEOS file ", 21)) {
    size_t len;
    struct Filename geosname;
    const byte_t* info = &data[254];
    dirent = (struct DirEnt*) &data[-2];

    /* Read the name from the directory entry. */
    memcpy (geosname.name, dirent->name, sizeof geosname.name);
    geosname.type = getFiletype (image, dirent);
    geosname.recordLength = 0;

    if (!isGeosDirEnt (dirent) || memcmp (info, "\3\25\277", 3))
      goto notGEOS;

    if (dirent->isVLIR) {
      const byte_t* vlir = &data[2 * 254];
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
    memcpy ((byte_t*)dirent + 2, data, sizeof (struct DirEnt) - 2);
    dirent->type = 0;
    dirent->firstTrack = 0;
    dirent->firstSector = 0;
    dirent->infoTrack = image->dirtrack + 1;
    dirent->infoSector = 0;

    {
      byte_t* oldBAM = 0;
      enum WrStatus status;

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
	byte_t vlir[254];
	const byte_t* vlirsrc = &data[254 * 2];
	unsigned vlirblock;
	const byte_t* buf = &data[254 * 3];
	byte_t track = dirent->infoTrack;
	byte_t sector = dirent->infoSector;

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
    byte_t* oldBAM = 0;
    enum WrStatus status;

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

      status = setupSideSectors (image, dirent, rounddiv(length, 254), log);

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

/** Read and convert a disk image in CBM DOS format
 * @param file		the file input stream
 * @param filename	host system name of the file
 * @param writeCallback	function for writing the contained files
 * @param log		Call-back function for diagnostic output
 * @return		status of the operation
 */
enum RdStatus
ReadImage (FILE* file,
	   const char* filename,
	   write_file_t writeCallback,
	   log_t log)
{
  const struct DiskGeometry* geom = 0;
  struct Image image;

  /* determine disk image type from its length */
  {
    size_t length, blocks;
    unsigned i;

    if (fseek (file, 0, SEEK_END)) {
    seekError:
      (*log) (Errors, 0, "fseek: %s", strerror(errno));
      return RdFail;
    }

    length = ftell (file);

    if (fseek (file, 0, SEEK_SET))
      goto seekError;

    if (length % 256) {
    unknownImage:
      (*log) (Errors, 0, "Unknown disk image type");
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
      (*log) (Errors, 0, "Out of memory");
      return RdFail;
    }

    image.type = geom->type;
    image.dirtrack = geom->dirtrack;
    image.name = 0;

    if (1 != fread (image.buf, length, 1, file)) {
      (*log) (Errors, 0, "fread: %s", strerror(errno));
      free (image.buf);
      return RdFail;
    }
  }

  /* Traverse through the root directory and extract the files */
  {
    struct DirEnt** directory = 0;
    unsigned block;

    if (!(block = mapInode ((byte_t***) &directory, &image,
			    image.dirtrack, 0, log, 0))) {
      (*log) (Errors, 0, "Could not read the directory on track %u.",
	      image.dirtrack);
      free (image.buf);
      return RdFail;
    }

    if (block < geom->BAMblocks) {
      (*log) (Errors, 0, "Directory too short.");
      free (image.buf);
      free (directory);
      return RdFail;
    }

    /* Traverse through the directory */

    for (block = geom->BAMblocks;; block++) {
      unsigned i;
      struct Filename name;

      for (i = 0; i < 256 / sizeof (struct DirEnt); i++) {
	struct DirEnt* dirent = &directory[block][i];

	memcpy (name.name, dirent->name, 16);
	name.type = getFiletype (&image, dirent);
	name.recordLength = dirent->recordLength;

	if (isGeosDirEnt (dirent)) {
	  static const char cvt[] = "PRG formatted GEOS file V1.0";
	  size_t length = 0;
	  byte_t* buf;
	  const byte_t
	    *vlir = 0,
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
		byte_t* b = 0;
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
	    byte_t* b = 0;
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
	  memcpy (&buf[0], &dirent->type, length = sizeof (struct DirEnt) - 2);
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
		byte_t* b = 0;
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
	    byte_t* b = 0;
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
	  byte_t* buf;
	  size_t length;
	case REL:
	  if (!checkSideSectors (&image, dirent, log))
	    (*log) (Warnings, &name, "error in side sector data");

	  /* fall through */
	case DEL:
	case SEQ:
	case PRG:
	case USR:
	  buf = 0;
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

/** Open an existing disk image or create a new one.
 * @param filename	name of 1541 disk image on the host system
 * @param image		address of the disk image buffer pointer
 *			(will be allocated by this function)
 * @param type		type of the disk image
 * @param direntOpts	directory entry handling options
 * @return		Status of the operation
 */
enum ImStatus
OpenImage (const char* filename,
	   struct Image** image,
	   enum ImageType type,
	   enum DirEntOpts direntOpts)
{
  FILE* f;
  const struct DiskGeometry* geom;

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
    *image = 0;
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

/** Write back a disk image.
 * @param image		address of the disk image buffer
 *			(will be deallocated by this function)
 * @return		Status of the operation
 */
enum ImStatus
CloseImage (struct Image* image)
{
  FILE* f;
  const struct DiskGeometry* geom;

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
  image->buf = 0;
  return ImOK;
}
