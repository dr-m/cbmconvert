/**
 * @file zip2disk.c
 * Convert four Zip-Code files to a 1541 disk image
 * @author Marko Mäkelä (marko.makela@nic.funet.fi)
 * @author Paul David Doherty (h0142kdd@rz.hu-berlin.de)
 */

/*
** Copyright © 1993-1997,2001 Marko Mäkelä
** Original version © 1993 Paul David Doherty (h0142kdd@rz.hu-berlin.de)
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

#ifdef MSDOS
/** Directory path separator character */
#define PATH_SEPARATOR '\\'
#else
/** Directory path separator character */
#define PATH_SEPARATOR '/'
#endif /* check for Multiple Sklerosis Denial Of Service */

/** Suffix for output file names */
static const char out_suffix[] = ".d64";

/** Input file */
static FILE* infile;
/** Output file */
static FILE* outfile;

/** number of sectors on the current track */
static unsigned max_sect;
/** current track number */
static unsigned track;
/** decoding buffer for current track */
static unsigned char trackbuf[21][256];
/** flag: which sectors on the current track have been decoded? */
static int trackbuf_decoded[21];
/** input file name */
static char* inname;
/** first character of the actual file name */
static char* fname;

/** output file name */
static char* outname;

/** Initialize the files
 * @param filename	the base file name
 * @return		0 on success;<br>
 *			1 on out of memory;<br>
 *			2 if not all input files could be opened;<br>
 *			3 if no output could be created<br>
 */
static int
init_files (const char* filename)
{
  /* flag: was an output file name already specified? */
  int outflag = !!outname;
  int i = strlen (filename);

  if (!(inname = malloc (i + 3)) ||
      (!outname && !(outname = malloc (i + sizeof out_suffix))))
    return 1;

  /* copy the base filename */
  memcpy (inname, filename, i);

  if (!outflag) {
    memcpy (outname, filename, i);
    memcpy (outname + i, out_suffix, sizeof out_suffix);
  }

  /* modify input filename */

  for (fname = inname + i;
       fname > inname && *fname != PATH_SEPARATOR; fname--);
  if (fname > inname)
    fname++;
  fname[1] = '!';
  memcpy (fname + 2, filename + (fname - inname), i);

  /* try to find the input files */

  for (*fname = '1'; *fname < '5'; (*fname)++) {
    infile = fopen (inname, "rb");

    if (infile)
      fclose (infile);
    else
      return 2;
  }

  infile = 0;

  /* try to create output file */

  if (!(outfile = fopen (outname, "wb"))) {
    return 3;
  }

  return 0;
}

/** Open an input file
 * @param number	ASCII number of the input file
 * @return		0 on success; 1 on error
 */
static int
open_file (char number)
{
  *fname = number;

  if (infile)
    fclose (infile);

  if (!(infile = fopen (inname, "rb")) ||
      -1 == fseek (infile, (number == '1') ? 4 : 2, 0))
    return 1;

  return 0;
}

/** Decode a sector
 * @return	0 on success; 1 on failure
 */
static int
read_sector (void)
{
  int trk, sec, ch;

  trk = fgetc (infile);
  sec = fgetc (infile);

  if ((trk & 0x3f) != track || sec < 0 || sec >= max_sect ||
      trackbuf_decoded[sec]) {
  Error:
    fprintf (stderr, "zip2disk: Input file %s is corrupted.\n", inname);
    return 1;
  }

  trackbuf_decoded[sec] = 1;

  /* RLE compressed sector */
  if (trk & 0x80) {
    /* number of decompressed bytes */
    int count = 0;
    /* length of the compressed stream, and the escape character */
    int len = fgetc (infile), esc = fgetc (infile);

    while (len--) {
      ch = fgetc (infile);
      if (ch != esc) {
	trackbuf[sec][count++] = ch;
	if (count > 256)
	  goto Error;
      }
      else if (len >= 2) {
	/* escape character: get the number of repetitions */
	int repnum = fgetc (infile);
	/* get the repeated character */
	ch = fgetc (infile);
	if (repnum < 0 || repnum + count > 256 || ch == EOF)
	  goto Error;
	memset (trackbuf[sec] + count, ch, repnum);
	count += repnum;
	len -= 2;
      }
      else
	goto Error;
    }

    if (count != 256)
      goto Error;
  }
  /* whole sector filled with a single character */
  else if (trk & 0x40) {
    if ((ch = fgetc (infile)) == EOF)
      goto Error;
    memset (trackbuf[sec], ch, 256);
  }
  /* no compression */
  else if (256 != fread (trackbuf[sec], 1, 256, infile))
    goto Error;

  return 0;
}

/** Decode a track
 * @return	0 on success; 1 on failure
 */
static int
read_track (void)
{
  unsigned s;
  memset (trackbuf_decoded, 0, sizeof trackbuf_decoded);

  for (s = max_sect; s--; )
    if (read_sector ())
      return 1;

  return 0;
}

/** Main program
 * @param argc	number of command-line arguments
 * @param argv	contents of command-line arguments
 * @return	0 on success;<br>
 *		1 on usage error;<br>
 *		2 on out of memory;<br>
 *		3 on input or output error;<br>
 *		4 on ZipCode format error
 */
int
main (int argc, char** argv)
{
  if (argc != 2 && argc != 3) {
    fputs ("ZipCode disk image extractor v1.2.1\n"
	   "Usage: zip2disk zip_image_name [disk_image_name]\n", stderr);
    return 1;
  }

  outname = (argc == 3) ? argv[2] : 0;

  switch (init_files (argv[1])) {
  case 3:
    fprintf (stderr, "zip2disk: Could not create %s.\n", outname);
    return 3;
  case 2:
    fprintf (stderr, "zip2disk: File %s not found.\n", inname);
    return 3;
  case 1:
    fputs ("zip2disk: Out of memory.\n", stderr);
    return 2;
  }

  for (track = 1; track <= 35; track++) {
    max_sect = 17 + ((track < 31) ? 1 : 0) + ((track < 25) ? 1 : 0) +
      ((track < 18) ? 2 : 0);

    switch (track) {
    case 1:
      if (open_file ('1'))
	goto OpenError;
      break;
    case 9:
      if (open_file ('2'))
	goto OpenError;
      break;
    case 17:
      if (open_file ('3'))
	goto OpenError;
      break;
    case 26:
      if (open_file ('4'))
	goto OpenError;
      break;
    }

    if (read_track ()) {
      fclose (infile);
      fclose (outfile);
      return 4;
    }

    if (max_sect != fwrite (trackbuf, 256, max_sect, outfile)) {
      fclose (infile);
      fclose (outfile);
      fputs ("zip2disk: Error in writing the output file.\n", stderr);
      return 3;
    }
  }

  return 0;

OpenError:
  fprintf (stderr, "zip2disk: Error in opening file %s.\n", inname);
  return 3;
}
