/*
** $Id$
**
** Commodore file format converter
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
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#include "input.h"
#include "output.h"

static WriteFunc *writeFunc =
#ifdef _WRITE_PC64_DEFAULT_
 WritePC64;
#else
 WriteNative;
#endif
static WriteImageFunc *writeImageFunc = WriteImage;
static Image *image = NULL;
static WriteArchiveFunc *writeArchiveFunc = ArchiveLynx;
static Archive *archive = NULL;
static const char *archiveFilename = NULL;
static Verbosity verbosityLevel = Warnings;
static const char *currentFilename = NULL;

static enum { Never, Sometimes, Always } changeDisks = Sometimes;

int main (int argc, char **argv);

static WriteCallback writeFile;
static LogCallback writeLog;
static const char *imageType (ImageType im);

int main (int argc, char **argv)
{
  ReadFunc *readFunc = ReadNative;
  FILE *file;
  char *prog = *argv; /* name of the program */
  int retval = 0; /* return status */

  /* process the option flags */
  for (argv++; argc > 1 && **argv == '-'; argv++, argc--) {
    char *opts = *argv;

    if (!strcmp (opts, "--")) { /* disable processing further options */
      argv++;
      argc--;
      break;
    }

    while (*++opts) /* process all flags */
      switch (*opts) {
      case 'v':
	switch (opts[1]) {
	case 'v':
	case '2':
	  verbosityLevel = Everything;
	  break;
	case 'w':
	case '1':
	  verbosityLevel = Warnings;
	  break;
	case '0':
	  verbosityLevel = Errors;
	  break;
	default:
	  goto Usage;
	}
	opts++;
	break;

      case 'i':
	switch (opts[1]) {
	case '0':
	  changeDisks = Never;
	  break;
	case '1':
	  changeDisks = Sometimes;
	  break;
	case '2':
	  changeDisks = Always;
	  break;
	default:
	  goto Usage;
	}
	opts++;
	break;

      case 'n':
	readFunc = ReadNative;
	break;
      case 'p':
	readFunc = ReadPC64;
	break;
      case 'a':
	readFunc = ReadARC;
	break;
      case 'k':
	readFunc = ReadArkive;
	break;
      case 'l':
	readFunc = ReadLynx;
	break;
      case 't':
	readFunc = ReadT64;
	break;
      case 'd':
	readFunc = ReadImage;
	break;
      case 'c':
	readFunc = ReadCpmImage;
	break;
      case 'I':
	writeFunc = Write9660;
	break;
      case 'P':
	writeFunc = WritePC64;
	break;
      case 'N':
	writeFunc = WriteNative;
	break;
      case 'L':
	if (image || archive || argc <= 2)
	  goto Usage;

	archive = newArchive();
	writeArchiveFunc = ArchiveLynx;
	archiveFilename = *++argv;argc--;
	break;
      case 'C':
      case 'D':
	if (archive)
	  goto Usage;

	if (argc > 2) {
	  ImageType im = Im1541;

	  switch (opts[1]) {
	  case '4':
	    im = Im1541;
	    break;
	  case '7':
	    im = Im1571;
	    break;
	  case '8':
	    im = Im1581;
	    break;
	  default:
	    goto Usage;
	  }

	  writeFunc = NULL;
	  writeImageFunc = *opts == 'C' ? WriteCpmImage : WriteImage;

	  opts++;
	  argc--;
	  argv++;

	  {
	    DirEntOpts dopts = DirEntOnlyCreate;
	    if (opts[1] == 'o') {
	      dopts = DirEntFindOrCreate;
	      opts++;
	    }

	    if (OpenImage (*argv, &image, im, dopts) != ImOK) {
	      fprintf (stderr, "Could not open the %s%s image '%s'.\n",
		       writeImageFunc == WriteCpmImage ? "CP/M " : "",
		       imageType (im), *argv);
	      return 2;
	    }
	  }
	}
	else
	  goto Usage;

	break;

      default:
	goto Usage;
      }
  }

  if (argc < 2) {
  Usage:
    fprintf (stderr, "cbmconvert 2.0 - Commodore archive converter\n");
    fprintf (stderr, "Usage: %s [options] file(s)\n", prog);

    fputs ("Options: -I: Create ISO 9660 compliant file names.\n"
	   "         -P: Output files in PC64 format.\n"
	   "         -N: Output files in native format.\n"
	   "         -L: Output files in Lynx format.\n"
	   "         -C4 imagefile: Write to a 1541 CP/M disk image.\n"
	   "         -C4o imagefile: Ditto, overwriting existing files.\n"
	   "         -C7[o] imagefile: Write to a 1571 CP/M disk image.\n"
	   "         -C8[o] imagefile: Write to a 1581 CP/M disk image.\n"
	   "         -D4[o] imagefile: Write to a 1541 disk image.\n"
	   "         -D7[o] imagefile: Write to a 1571 disk image.\n"
	   "         -D8[o] imagefile: Write to a 1581 disk image.\n"
	   "\n"
	   "         -i2: Switch disk images on out of space or duplicate file name.\n"
	   "         -i1: Switch disk images on out of space.\n"
	   "         -i0: Never switch disk images.\n"
	   "\n"
	   "         -n: input files in native format.\n"
	   "         -p: input files in PC64 format.\n"
	   "         -a: input files in ARC/SDA format.\n"
	   "         -k: input files in Arkive format.\n"
	   "         -l: input files in Lynx format.\n"
	   "         -t: input files in T64 format.\n"
	   "         -c: input files in C128 CP/M disk image format.\n"
	   "         -d: input files in disk image format.\n"
	   "\n"
	   "         -v2: Verbose mode.  Display all messages.\n"
	   "         -v1: Display warnings in addition to errors.\n"
	   "         -v0: Display error messages only.\n"
	   "         --: Stop processing any further options.\n",
	   stderr);

    return 1;
  }

  /* Process the files. */

  for (; --argc; argv++) {
    RdStatus status;
    currentFilename = *argv;

    if (!(file = fopen (currentFilename, "rb"))) {
      fprintf (stderr, "fopen '%s': %s\n", currentFilename, strerror(errno));
      retval = 2;
      continue;
    }

    status = (*readFunc) (file, *argv, writeFile, writeLog);
    fclose (file);

    switch (status) {
    case RdOK:
      writeLog (Everything, NULL, "Archive extracted.");
      break;

    case RdNoSpace:
      writeLog (Errors, NULL, "out of space.");
      if (image || archive)
	goto write;
      else
	return 3;

    default:
      writeLog (Errors, NULL, "unexpected error.");
      retval = 4;
      if (image || archive)
	goto write;
      else
	return retval;
    }
  }

write:
  if (image) {
    switch (CloseImage (image)) {
    case ImOK:
      writeLog (Everything, NULL, "Wrote image file \"%s\"", image->name);
      break;

    case ImNoSpace:
      writeLog (Errors, NULL, "Out of space while writing image file \"%s\"!",
		image->name);
      return 3;

    default:
      writeLog (Errors, NULL, "Unexpected error while writing image \"%s\"!",
		image->name);
      return 4;
    }

    free (image);
    image = NULL;
  }

  if (archive) {
    switch ((*writeArchiveFunc) (archive, archiveFilename)) {
    case ArOK:
      writeLog (Everything, NULL, "Wrote archive file \"%s\"",
		archiveFilename);
      break;

    case ArNoSpace:
      writeLog (Everything, NULL,
		"Out of space while writing archive file \"%s\"!",
		archiveFilename);
      return 3;

    default:
      writeLog (Everything, NULL,
		"Unexpected error while writing image \"%s\"!",
		archiveFilename);
      return 4;
    }

    deleteArchive (archive);
    archive = NULL;
  }

  if (verbosityLevel == Everything)
    fprintf (stderr, "%s: all done\n", prog);

  return retval;
}

static WrStatus writeFile (const Filename *name,
			   const BYTE *data, size_t length)
{
  WrStatus status = WrFail;

  if (!image && !writeFunc)
    return status;

  if (!length) {
    writeLog (Errors, name, "Not writing zero length file");
    return WrFail;
  }

  if (image) {
    status = (*writeImageFunc) (name, data, length, image, writeLog);
    switch (status) {
    case WrOK:
      writeLog (Everything, name, "Wrote %u bytes to image \"%s\"",
	      length, image->name);
      return WrOK;
    case WrFail:
      writeLog (Errors, name, "Write failed!");
      return WrFail;
    case WrFileExists:
      if (changeDisks < Always) {
	writeLog (Errors, name, "non-unique file name!");
	return WrFileExists;
      }
      /* non-unique file name, fall through */
    case WrNoSpace:
      if (changeDisks < Sometimes) {
	writeLog (Errors, name, "out of space!");
	return WrNoSpace;
      }

      /* try to open a new disk image */
      writeLog (Warnings, name,
		status == WrFileExists
		? "non-unique file name, changing disk images..."
		: "out of space, changing disk images...");
      switch (CloseImage (image)) {
	unsigned char *c;
      case ImNoSpace:
	writeLog (Errors, name, "out of space");
	free (image);
	image = NULL;
	return WrNoSpace;
      case ImFail:
	writeLog (Errors, name, "failed");
	free (image);
	image = NULL;
	return WrFail;
      case ImOK:
	writeLog (Everything, name, "wrote old image \"%s\"", image->name);
	/*
	** Update the file name.  If there is a number in the first
	** component of the file name (excluding any directory component),
	** increment it.
	*/
	for (c = image->name; *c; c++);
	for (; c >= image->name && *c != PATH_SEPARATOR; c--);
	for (c++; *c && *c != '.'; c++);
	while (--c >= image->name)
	  if (*c >= '0' && *c < '9') {
	    (*c)++;
	    break;
	  }
	  else if (*c == '9')
	    *c = '0';
	  else
	    goto notUnique;

	if (c < image->name) {
	notUnique:
	  writeLog (Errors, name, "Could not generate unique image file name");
	  free (image);
	  image = NULL;
	  return WrFail;
	}

	writeLog (Everything, name, "Continuing to image \"%s\"...",
		  image->name);
	{
	  char *filename = (char*)image->name;
	  ImageType type = image->type;
	  DirEntOpts direntOpts = image->direntOpts;

	  free (image);
	  image = NULL;

	  status = OpenImage (filename, &image, type, direntOpts);
	}

	switch (status) {
	case ImOK:
	  status = (*writeImageFunc) (name, data, length, image, writeLog);

	  if (status == WrOK)
	    writeLog (Everything, name, "OK, wrote %u bytes to image \"%s\"",
		      length, image->name);
	  else
	    writeLog (Errors, name, "%s while writing to \"%s\", giving up.",
		      status == WrNoSpace ? "out of space" :
		      status == WrFileExists ? "duplicate file name" :
		      "failed",
		      image->name);

	  return status;

	default:
	  writeLog (Errors, name, "%s while creating image \"%s\"",
		    status == ImNoSpace ? "out of space" : "failed",
		    image->name);
	  return status;
	}
      }
    }
  }
  else if (archive) {
    status = WriteArchive (name, data, length, archive, writeLog);
    switch (status) {
    case WrOK:
      writeLog (Everything, name, "Wrote %u bytes to archive \"%s\"",
	      length, archiveFilename);
      return WrOK;
    case WrFail:
      writeLog (Errors, name, "Write failed!");
      return WrFail;
    case WrFileExists:
      writeLog (Errors, name, "non-unique file name!");
      return WrFileExists;
    case WrNoSpace:
      writeLog (Errors, name, "out of space!");
      return WrNoSpace;
    }
  }
  else {
    char *newname = NULL;

    status = (*writeFunc) (name, data, length, &newname, writeLog);

    if (status == WrOK)
      writeLog (Everything, name, "Writing %u bytes to \"%s\"",
		length, newname);
    else
      writeLog (Errors, name, "%s while writing to \"%s\"",
		status == ImNoSpace ? "out of space" : "failed",
		newname);

    free (newname);
    return status;
  }

  return status;
}

static void writeLog (Verbosity verbosity, const Filename *name,
		      const char *format, ...)
{
  static Filename oldname;

  if (verbosityLevel >= verbosity) {
    va_list ap;

    if (currentFilename) {
      fprintf (stderr, "`%s':\n", currentFilename);
      currentFilename = NULL;
    }

    fputs ("  ", stderr);

    if (name) {
      if (memcmp(name, &oldname, sizeof oldname))
	fprintf (stderr, "`%s':\n    ", getFilename (name));
      else
	fputs ("  ", stderr);
      memcpy(&oldname, name, sizeof oldname);
    }

    va_start(ap, format);
    vfprintf (stderr, format, ap);
    va_end(ap);

    fputc ('\n', stderr);
  }
}

static const char *imageType (ImageType im)
{
  switch (im) {
  case ImUnknown:
    return "(unknown)";
  case Im1541:
    return "1541";
  case Im1571:
    return "1571";
  case Im1581:
    return "1581";
  }

  return NULL;
}
