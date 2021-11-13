/**
 * @file disk2zip.c
 * Convert a 1541 disk image to four Zip-Code files
 * @author Marko Mäkelä (marko.makela at iki.fi)
 */

/*
** Copyright © 1993-1998,2001,2006,2021 Marko Mäkelä
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

/** determine whether a character is hexadecimal
 * @param a     the ASCII character
 * @return      nonzero if it is hexadecimal
 */
#define ISHEX(a)                                                        \
((a >= '0' && a <= '9') || ((a & ~32) >= 'A' && (a & ~32) <= 'F'))

/** convert a hexadecimal ASCII character to a number
 * @param a     the ASCII character
 * @return      the numeric interpretation of the character
 */
#define ASC2HEX(a)                                                      \
((a < '0' || a > '9') ? (((a & ~32) < 'A' || (a & ~32) > 'F') ? -1 :    \
                         ((a & ~32) - 'A' + 10)) : (a - '0'))

/** basic start address of ZipCoded files */
#define ZCADDR 0x400

/** the disk image */
static FILE* infile;
/** current output file */
static FILE* outfile;

/** current track number */
static int track;
/** maximum number of sectors in the current track */
static unsigned max_sect;
/** interleave factors */
static int eveninc = -10, oddinc = 11;
/** disk identifier, two bytes */
static unsigned char id[2] = { '6', '4' };
/** decoding buffer for current track */
unsigned char trackbuf[21][256];
/** input file name */
static char* inname;
/** output file name */
static char* outname;
/** first character of the actual file name */
static char* fname;

/** Initialize the files
 * @param filename      base name for the output file
 * @retval 0 on success
 * @retval 1 on out of memory
 * @retval 2 if the input file could not be opened
 */
static int
init_files (const char* filename)
{
  size_t i = strlen (filename);

  /* allocate memory for the output filenames */

  if (!(outname = (char*) malloc (i + 3)))
    return 1;

  /* copy the base filename */

  memcpy (outname, filename, i + 1);

  /* modify the filename */

  for (fname = outname + i;
       fname > outname && *fname != PATH_SEPARATOR; fname--);
  if (fname > outname)
    fname++;
  memmove (fname + 2, fname, i + 1 - (size_t) (fname - outname));
  fname[1] = '!';

  /* try to open the input file */

  return (!infile && !(infile = fopen (inname, "rb"))) ? 2 : 0;
}

/** Open an output file
 * @param number        number of the output file ('1' to '4')
 * @return              0 on success; 1 on error
 */
static int
open_file (char number)
{
  *fname = number;

  if (number > '1')
    fclose (outfile);

  if (!(outfile = fopen (outname, "wb")))
    return 1;

  if (number == '1')
    return
      EOF == fputc ((ZCADDR - 2), outfile) ||
      EOF == fputc ((ZCADDR - 2) >> 8, outfile) ||
      2 != fwrite (id, 1, 2, outfile);
  else
    return EOF == fputc (ZCADDR, outfile) ||
           EOF == fputc (ZCADDR >> 8, outfile);
}

/** Encode a sector
 * @param sect  the sector number
 * @return      0 on success; 1 on failure
 */
static int
write_sector (int sect)
{
  /* a histogram: number of occurrences of different bytes */
  static int histogram[256];
  int i;
  unsigned char* sectbuf = trackbuf[sect];

  memset (histogram, 0, sizeof histogram);

  /* see if the all bytes in the sector are identical */
  for (i = 0; i < 256; i++)
    if (256 == ++histogram[sectbuf[i]])
      return EOF == fputc (track | 0x40, outfile) ||
             EOF == fputc (sect, outfile) ||
             EOF == fputc (sectbuf[i], outfile);

  /* see whether there is a byte that does not occur in the sector */
  for (i = 0; i < 256 && histogram[i]; i++);

  /* if not, store the sector without compression */
  if (i > 255)
Uncompressed:
    return EOF == fputc (track, outfile) ||
           EOF == fputc (sect, outfile) ||
           1 != fwrite (sectbuf, 256, 1, outfile);
  /* otherwise, see if run-length encoding would bring savings */
  else {
    int rep = i, j, count;
    for (i = 1, j = count = 0; i < 256; i++) {
      if (sectbuf[i] == sectbuf[j])
        continue;
      count += (i < j + 3) ? i - j : 3;
      j = i;
    }
    count += (i < j + 3) ? i - j : 3;

    if (count > 253)
      goto Uncompressed;

    /* apply run-length encoding */
    if (EOF == fputc (track | 0x80, outfile) || EOF == fputc (sect, outfile) ||
        EOF == fputc (count, outfile) || EOF == fputc (rep, outfile))
      return 1;

    for (i = 1, j = 0;; i++) {
      if (i < 256 && sectbuf[i] == sectbuf[j])
        continue;
      if (i > j + 3) {
        if (EOF == fputc (rep, outfile) || EOF == fputc (i - j, outfile) ||
            EOF == fputc (sectbuf[j], outfile))
          return 1;
      }
      else if (1 != fwrite (&sectbuf[j], (size_t) (i - j), 1, outfile))
        return 1;

      if (i > 255)
        break;
      j = i;
    }

    return 0;
  }
}

/** Encode a track
 * @return      0 on success; 1 on failure
 */
static int
write_track (void)
{
  unsigned i = 0;
  int sect = 0;

  for (; i++ < max_sect; sect += i & 1 ? oddinc : eveninc)
    if (write_sector (sect))
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
 * @retval 4 on error in the disk image
 */
int
main (int argc, char** argv)
{
  int status;

optloop:
  argv++;

  if (argc > 1 && **argv == '-') {
    switch ((*argv)[1]) {
    case 0:
      infile = stdin;
      break;
    case '-':
      if (!(*argv)[2]) { /* "--" disables processing further options */
        argv++;
        argc--;
        break;
      }
    case 'i':
      if (!(*argv)[2] && argv[1]) { /* "-i" specifies disk identifier */
        argv++;
        argc-=2;
        if (ISHEX ((*argv)[0]) && ISHEX ((*argv)[1]) &&
            ISHEX ((*argv)[2]) && ISHEX ((*argv)[3]) &&
            !(*argv)[4]) {
          id[0] = (unsigned char)
            (ASC2HEX((*argv)[1]) | ASC2HEX((*argv)[0]) << 4);
          id[1] = (unsigned char)
            (ASC2HEX((*argv)[3]) | ASC2HEX((*argv)[2]) << 4);
          goto optloop;
        }
      }
      /* fall through */
    default: /* unknown option */
      goto Usage;
    }
  }

  if (argc != 2 && argc != 3) {
  Usage:
    fputs ("ZipCode disk image compressor v1.0.2\n"
           "Usage: disk2zip [options] disk_image_name [zip_image_name]\n"
           "Options: -i nnmm: Use $nn $mm (hexadecimal) as disk identifier.\n",
           stderr);
    return 1;
  }

  inname = *argv++;

  switch (init_files (*argv ? *argv : inname)) {
  case 2:
    fprintf (stderr, "disk2zip: File %s not found.\n", inname);
    status = 3;
    goto FuncExit;
  case 1:
    fputs ("disk2zip: Out of memory.\n", stderr);
    status = 2;
    goto FuncExit;
  }

  for (track = 1; track <= 35; track++) {
    max_sect = 17U + (track < 31) + (track < 25) + (track < 18) * 2U;

    if (track == 18 || track == 25) eveninc++, oddinc--;

    switch (track) {
    case 1:
      if (open_file ('1')) {
      OpenError:
        fprintf (stderr, "disk2zip: Error in opening file %s.\n", outname);
        status = 3;
        goto FuncExit;
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

    if (max_sect != fread (trackbuf, 256, max_sect, infile)) {
      fputs ("disk2zip: Error in reading the input file.\n", stderr);
      status = 4;
      goto FuncExit;
    }

    if (write_track()) {
      status = 3;
      goto FuncExit;
    }
  }

  status = 0;

FuncExit:
  if (infile != stdin)
    fclose (infile);
  if (outfile)
    fclose (outfile);
  free (outname);
  return status;
}
