/*
** $Id$
**
** Convert a 1541 disk image to four Zip-Code files
**
** Copyright © 1993-1998 Marko Mäkelä
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

#ifdef MSDOS
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif /* check for Multiple Sklerosis Denial Of Service */

/* convert an ASCII character to a hexadecimal nybble */
#define ASC2HEX(a) (int)\
    ((a < '0' || a > '9') ? (((a & ~32) < 'A' || (a & ~32) > 'F') ? -1 : \
    ((a & ~32) - 'A' + 10)) : (a - '0'))

#define ZCADDR 0x400 /* basic start address of ZipCoded files */

FILE *infile, *outfile;

int track, sect, max_sect, position,
    eveninc = -10, oddinc = 11,
    id1 = '6',  /* the first disk identifier byte */
    id2 = '4';  /* the second disk identifier byte */
unsigned char act_track[21 << 8];
char *inname, *outname, *prog;

int main (int argc, char **argv);
int init_files (const char *filename);
int open_file (int number);
int write_track (void);
int write_sector (void);

/*******************************************************************/
/*  MAIN function                                                  */
/*******************************************************************/

/* Return codes:
** 0 -- OK
** 1 -- RTFM
** 2 -- out of memory
** 3 -- file I/O error
** 4 -- error in disk image
*/

int main (int argc, char **argv)
{
  for (prog = *argv; *prog++;);

  for (; prog > *argv && *prog != PATH_SEPARATOR; prog--);
  if (*prog == PATH_SEPARATOR) prog++;

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
	id1 = ASC2HEX((*argv)[1]) | (ASC2HEX((*argv)[0]) << 4);
	id2 = ASC2HEX((*argv)[3]) | (ASC2HEX((*argv)[2]) << 4);
        if (~255 & (id1 | id2))
          goto Usage;

        if (!(*argv)[4]) goto optloop;
      }
    default: /* process options */
      goto Usage;
    }
  }

  if (argc != 2 && argc != 3) {
  Usage:
    fprintf (stderr, "ZipCode disk image compressor v1.0\n");
    fprintf (stderr, "Usage: %s [options] disk_image_name [zip_image_name]\n",
             prog);
    fprintf (stderr, "Options: -i nnmm: Use $nn $mm (hexadecimal) as disk identifier.\n");
    return 1;
  }

  inname = *argv++;

  switch (init_files (*argv ? *argv : inname)) {
  case 2:
    fprintf (stderr, "%s: File %s not found.\n", prog, inname);
    return 3;
  case 1:
    fprintf (stderr, "%s: Out of memory.\n", prog);
    return 2;
  }

  for (track = 1; track <= 35; track++) {
    max_sect = 17 + (track < 31) + (track < 25) + ((track < 18) << 1);

    if (track == 18 || track == 25) eveninc++, oddinc--;

    switch (track) {
    case 1:
      if (open_file (1))
        goto OpenError;
      break;
    case 9:
      if (open_file (2))
        goto OpenError;
      break;
    case 17:
      if (open_file (3))
        goto OpenError;
      break;
    case 26:
      if (open_file (4))
        goto OpenError;
      break;
    }

    if (max_sect != fread (act_track, 256, max_sect, infile)) {
      fclose (infile);
      fclose (outfile);
      fprintf (stderr, "%s: Error in reading the input file.\n", prog);
      return 4;
    }

    if (write_track()) {
      fclose (infile);
      fclose (outfile);
      return 3;
    }
  }

  fclose (infile);
  fclose (outfile);
  return 0;

OpenError:
  fprintf (stderr, "%s: Error in opening file %s.\n", prog, outname);
  fclose (infile);
  return 3;
}

/*******************************************************************/
/*  Function: init_files                                           */
/*******************************************************************/

/* Return codes:
** 0 -- OK
** 1 -- out of memory
** 2 -- input file not found
*/

int init_files (const char *filename)
{
  int i;

  i = strlen (filename);

  /* allocate memory for the output filenames */

  if (!(outname = (char *)malloc (i + 3)))
    return 1;

  /* copy the base filename */

  strcpy (outname, filename);

  /* modify the filename */

  for (; i && outname[i] != PATH_SEPARATOR; i--);
  position = i ? ++i : i;
  outname[position + 1] = '!';
  while ((outname[i + 2] = filename[i])) i++;

  /* try to open the input file */

  return (!infile && !(infile = fopen (inname, "rb"))) ? 2 : 0;
}

/*******************************************************************/
/*  Function: open_file                                            */
/*******************************************************************/

/* Return codes:
** 0 -- OK
** 1 -- error
*/

int open_file (int number)
{
  outname[position] = '0' + (char)number;

  if (number > 1)
    fclose (outfile);

  if (!(outfile = fopen (outname, "wb")))
    return 1;

  if (number == 1)
    return EOF == fputc ((ZCADDR - 2), outfile) ||
           EOF == fputc ((ZCADDR - 2) >> 8, outfile) ||
           EOF == fputc (id1, outfile) ||
           EOF == fputc (id2, outfile);
  else
    return EOF == fputc (ZCADDR, outfile) ||
           EOF == fputc (ZCADDR >> 8, outfile);
}

/*******************************************************************/
/*  Function: write_track                                          */
/*******************************************************************/

/* Return codes:
** 0 -- OK
** 1 -- error
*/

int write_track (void)
{
  int i;

  for (sect = i = 0; i++ < max_sect; sect += i & 1 ? oddinc : eveninc)
    if (write_sector ())
      return 1;

  return 0;
}

/*******************************************************************/
/*  Function: write_sector                                         */
/*******************************************************************/

/* Return codes:
** 0 -- OK
** 1 -- error
*/

int write_sector (void)
{
  static int counts[256];
  unsigned char *act_sector;

  int i, j, rep, count;

  act_sector = &act_track[(sect << 8)];

  for (i = 0; i < 256; counts[i++] = 0);

  for (i = 0; i < 256; i++)
    if (256 == ++counts[act_sector[i]])
      return EOF == fputc (track | 0x40, outfile) ||
             EOF == fputc (sect, outfile) ||
             EOF == fputc (act_sector[i], outfile);

  for (i = 0; i < 256 && counts[i]; i++);

  if (i > 255)
Uncompressed:
    return EOF == fputc (track, outfile) ||
           EOF == fputc (sect, outfile) ||
           1 != fwrite (act_sector, 256, 1, outfile);

  rep = i;

  for (i = 1, j = count = 0; i < 256; i++) {
    if (act_sector[i] == act_sector[j]) continue;

    count += (i < j + 3) ? i - j : 3;

    j = i;
  }

  count += (i < j + 3) ? i - j : 3;

  if (count > 253) goto Uncompressed;

  if (EOF == fputc (track | 0x80, outfile) || EOF == fputc (sect, outfile) ||
      EOF == fputc (count, outfile) || EOF == fputc (rep, outfile))
    return 1;

  for (i = 1, j = 0;; i++) {
    if (i < 256 && act_sector[i] == act_sector[j]) continue;

    if (i > j + 3) {
      if (EOF == fputc (rep, outfile) || EOF == fputc (i - j, outfile) ||
          EOF == fputc (act_sector[j], outfile))
        return 1;
    } else
      if (1 != fwrite (&act_sector[j], i - j, 1, outfile))
        return 1;

    if (i > 255) break;

    j = i;
  }

  return 0;
}
