/*
** $Id$
**
** Convert a 1541 disk image to four Zip-Code files
**
** Copyright © 1993-1997 Marko Mäkelä
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
**
** $Log$
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char out_suffix[] = ".d64";

FILE *infile, *outfile;

int track, sect, max_sect, position;
unsigned char act_track[21 << 8];
int sect_flag[21];
char *inname, *outname, *prog;

int main (int argc, char **argv);
int init_files (const char *filename);
int open_file (int number);
int read_track (void);
int read_sector (void);

/*******************************************************************/
/*  MAIN function                                                  */
/*******************************************************************/

/* Return codes:
** 0 -- OK
** 1 -- RTFM
** 2 -- out of memory
** 3 -- file I/O error
** 4 -- error in ZipCoded files
*/

int main (int argc, char **argv)
{
  for (prog = *argv; *prog++;);
  for (; prog > *argv && *prog != '/'; prog--);
  if (*prog == '/') prog++;

  argv++;

  if (argc != 2 && argc != 3) {
    fprintf (stderr, "ZipCode disk image extractor v1.2\n");
    fprintf (stderr, "Usage: %s zip_image_name [disk_image_name]\n", prog);
    return 1;
  }
 
  outname = (argc == 3) ? argv[1] : NULL;

  switch (init_files (*argv)) {
  case 3:
    fprintf (stderr, "%s: Could not create %s.\n", prog, outname);
    return 3;
  case 2:
    fprintf (stderr, "%s: File %s not found.\n", prog, inname);
    return 3;
  case 1:
    fprintf (stderr, "%s: Out of memory.\n", prog);
    return 2;
  }

  for (track = 1; track <= 35; track++) {
    max_sect = 17 + ((track < 31) ? 1 : 0) + ((track < 25) ? 1 : 0) +
      ((track < 18) ? 2 : 0);

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

    if (read_track ()) {
      fclose (infile);
      fclose (outfile);
      return 4;
    }

    if (max_sect != fwrite (act_track, 256, max_sect, outfile)) {
      fclose (infile);
      fclose (outfile);
      fprintf (stderr, "%s: Error in writing the output file.\n", prog);
      return 3;
    }
  }

  return 0;

OpenError:
  fprintf (stderr, "%s: Error in opening file %s.", prog, inname);
  return 3;
}

/*******************************************************************/
/*  Function: init_files                                           */
/*******************************************************************/

/* Return codes:
** 0 -- OK
** 1 -- out of memory
** 2 -- not all input files found
** 3 -- unable to create output file
*/

int init_files (const char *filename)
{
  int i, flag = 0;

  i = strlen (filename);

  /* allocate memory for filenames */

  if (outname)
    flag = 1;

  if (!(inname = (char *)malloc (i + 3)) ||
	(!outname && !(outname = (char *)malloc (i + sizeof(out_suffix) + 1))))
    return 1;

  /* copy the base filename */

  strcpy (inname, filename);

  if (!flag)
    strcpy (outname, filename);

  /* modify input filename */

  for (; i && inname[i] != '/'; i--);
  position = i ? ++i : i;
  inname[position + 1] = '!';
  for (; (inname[i + 2] = filename[i]); i++);

  /* modify output filename */

  if (!flag)
    strcat (outname, out_suffix);

  /* try to find the input files */

  for (i = 1; i < 5; i++) {
    inname[position] = '0' + (char)i;
    infile = fopen (inname, "rb");

    if (infile)
      fclose (infile);
    else
      return 2;
  }

  /* try to create output file */

  if (!(outfile = fopen (outname, "wb")))
    return 3;

  if (!flag)
    free (outname);

  return 0;
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
  inname[position] = '0' + (char)number;

  if (number > 1)
    fclose (infile);

  if (!(infile = fopen (inname, "rb")) ||
      -1 == fseek (infile, (number == 1) ? 4 : 2, 0))
    return 1;

  return 0;
}

/*******************************************************************/
/*  Function: read_track                                           */
/*******************************************************************/

/* Return codes:
** 0 -- OK
** 1 -- error
*/

int read_track (void)
{
  for (sect = 0; sect < max_sect; sect_flag[sect++] = 0);

  for (sect = 0; sect < max_sect; sect++)
    if (read_sector ())
      return 1;

  return 0;
}

/*******************************************************************/
/*  Function: read_sector                                          */
/*******************************************************************/

/* Return codes:
** 0 -- OK
** 1 -- error
*/

int read_sector (void)
{
  unsigned char trk, sec, len, rep, repnum, chra;
  int i, j, count;

  trk = fgetc (infile);
  sec = fgetc (infile);

  if ((trk & 0x3f) != track || sec >= max_sect || sect_flag[sec] ||
      feof (infile)) {
  Error:
    fprintf (stderr, "%s: Input file %s is corrupted.\n", prog, inname);
    return 1;
  }

  sect_flag[sec] = 1;

  if (trk & 0x80) {
    len = fgetc (infile);
    rep = fgetc (infile);
    count = 0;

    for (i = 0; i < len; i++) {
      if (feof (infile))
	goto Error;
      chra = fgetc (infile);

      if (chra != rep)
	act_track[(sec << 8) + count++] = chra;
      else {
	repnum = fgetc (infile);
	if (feof (infile))
	  goto Error;
	chra = fgetc (infile);
	i += 2;
	for (j = 0; j < repnum; j++)
	  act_track[(sec << 8) + count++] = chra;
      }
    }
  }

  else if (trk & 0x40) {
    if (feof (infile))
      goto Error;
    chra = fgetc (infile);
    for (i = 0; i < 256; i++)
      act_track[(sec << 8) + i] = chra;
  }

  else if (256 != fread (&act_track[sec << 8], 1, 256, infile))
    goto Error;

  return 0;
}
