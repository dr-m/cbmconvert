/**
 * @file util.c
 * Utility functions
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
#include <string.h>
#include "util.h"

/** Convert a file name to a printable null-terminated string.
 * @param name	the PETSCII file name to be converted
 * @return	the corresponding ASCII file name
 */
const char*
getFilename (const struct Filename* name)
{
  static char buf[sizeof(name->name) + 5];
  int i;

  if (!name)
    return 0;

  /* remove trailing shifted spaces */

  for (i = sizeof(name->name); name->name[--i] == 0xA0; );
  buf [++i] = 0;
  while (i--)
    if (name->name[i] >= 0x41 && name->name[i] <= 0x5A)
      buf[i] = name->name[i] - 0x41 + 'a';
    else if (name->name[i] >= 0xC1 && name->name[i] <= 0xDA)
      buf[i] = name->name[i] - 0xC1 + 'A';
    else if (name->name[i] >= 0x61 && name->name[i] <= 0x7A)
      buf[i] = name->name[i] - 0x61 + 'A';
    else if (name->name[i] >= 0x20 && name->name[i] <= 0x5F)
      buf[i] = name->name[i];
    else
      buf[i] = '_'; /* non-ASCII character */

  switch (name->type) {
  case DEL:
    strcat (buf, ",del");
    break;
  case SEQ:
    strcat (buf, ",seq");
    break;
  case PRG:
    strcat (buf, ",prg");
    break;
  case USR:
    strcat (buf, ",usr");
    break;
  case REL:
    sprintf (buf + strlen(buf), ",l%02X", name->recordLength);
    break;
  case CBM:
    strcat (buf, ",cbm");
    break;
  }

  return buf;
}
