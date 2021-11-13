/**
 * @file zip2disk.c
 * Convert four Zip-Code files to a 1541 disk image
 * @author Marko Mäkelä (marko.makela at iki.fi)
 * @author Paul David Doherty (h0142kdd@rz.hu-berlin.de)
 */

/*
** Copyright © 1993-1997,2001,2006,2021 Marko Mäkelä
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
#endif /* MSDOS */

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
 * @param filename      the base file name
 * @retval 0 on success
 * @retval 1 on out of memory
 * @retval 2 if not all input files could be opened
 * @retval 3 if no output could be created
 */
static int
init_files (const char* filename)
{
  /* flag: was an output file name already specified? */
  int outflag = !!outname;
  size_t i = strlen (filename);

  if (!(inname = malloc (i + 3)) ||
      (!outname && !(outname = malloc (i + sizeof out_suffix))))
    return 1;

  /* copy the base filename */
  memcpy (inname, filename, i + 1);

  if (!outflag) {
    memcpy (outname, filename, i);
    memcpy (outname + i, out_suffix, sizeof out_suffix);
  }

  /* modify input filename */

  for (fname = inname + i;
       fname > inname && *fname != PATH_SEPARATOR; fname--);
  if (fname > inname)
    fname++;
  memmove (fname + 2, fname, i + 1 - (size_t) (fname - inname));
  fname[1] = '!';

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
 * @param number        ASCII number of the input file
 * @return              0 on success; 1 on error
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
 * @return      0 on success; 1 on failure
 */
static int
read_sector (void)
{
  int trk, sec, ch;

  trk = fgetc (infile);
  sec = fgetc (infile);

  if ((unsigned) (trk & 0x3f) != track || sec < 0 ||
      (unsigned) sec >= max_sect ||
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

    if (len == EOF || esc == EOF)
      goto Error;

    while (len--) {
      ch = fgetc (infile);
      if (ch == EOF)
        goto Error;
      if (ch != esc) {
        trackbuf[sec][count++] = (unsigned char) ch;
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
        memset (trackbuf[sec] + count, ch, (unsigned) repnum);
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
 * @return      0 on success; 1 on failure
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
 * @param argc  number of command-line arguments
 * @param argv  contents of command-line arguments
 * @retval 0 on success
 * @retval 1 on usage error
 * @retval 2 on out of memory
 * @retval 3 on input or output error
 * @retval 4 on ZipCode format error
 */
int
main (int argc, char** argv)
{
  int status;

  if (argc != 2 && argc != 3) {
    fputs ("ZipCode disk image extractor v1.2.2\n"
           "Usage: zip2disk zip_image_name [disk_image_name]\n", stderr);
    return 1;
  }

  outname = (argc == 3) ? argv[2] : 0;

  switch (init_files (argv[1])) {
  case 3:
    fprintf (stderr, "zip2disk: Could not create %s.\n", outname);
    status = 3;
    goto ErrExit;
  case 2:
    fprintf (stderr, "zip2disk: File %s not found.\n", inname);
    status = 3;
    goto ErrExit;
  case 1:
    fputs ("zip2disk: Out of memory.\n", stderr);
    status = 2;
    goto ErrExit;
  }

  for (track = 1; track <= 35; track++) {
    max_sect = 17U + ((track < 31) ? 1U : 0U) + ((track < 25) ? 1U : 0U) +
      ((track < 18) ? 2U : 0U);

    switch (track) {
    case 1:
      if (open_file ('1')) {
      OpenError:
        fprintf (stderr, "zip2disk: Error in opening file %s.\n", inname);
        status = 3;
        goto ErrExit;
      }
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
      status = 4;
      goto ErrExit;
    }

    if (max_sect != fwrite (trackbuf, 256, max_sect, outfile)) {
      fclose (infile);
      fclose (outfile);
      fputs ("zip2disk: Error in writing the output file.\n", stderr);
      status = 3;
      goto ErrExit;
    }
  }

  fclose (infile);
  fclose (outfile);
  status = 0;

ErrExit:
  free (inname);
  return status;
}
