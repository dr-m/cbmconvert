/**
 * @file input.h
 * Definitions for file reading functions
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

#ifndef INPUT_H
#  define INPUT_H

#  include "util.h"
#  include "output.h"

/* File management */

/** Call-back function for writing files
 * @param name		native (PETSCII) name of the file
 * @param data		the contents of the file
 * @param length	length of the file contents
 * @return		status of the operation
 */
typedef enum WrStatus write_file_t (const struct Filename* name,
				    const byte_t* data,
				    size_t length);

/** Status of a conversion operation */
enum RdStatus
{
  RdOK,		/**< Success */
  RdFail,	/**< Generic input or output failure */
  RdNoSpace	/**< Not enough space for the converted output */
};

/** Read and convert a file
 * @param file		the file input stream
 * @param filename	host system name of the file
 * @param writeCallback	function for writing the contained files
 * @param log		Call-back function for diagnostic output
 * @return		status of the operation
 */
typedef enum RdStatus read_file_t (FILE* file,
				   const char* filename,
				   write_file_t writeCallback,
				   log_t log);

/** Read and convert a raw file */
read_file_t ReadNative;
/** Read and convert a PC64 file (.P00, .S00 etc.) */
read_file_t ReadPC64;
/** Read and convert a Lynx archive */
read_file_t ReadLynx;
/** Read and convert an Arkive archive */
read_file_t ReadArkive;
/** Read and convert an ARC/SDA archive */
read_file_t ReadARC;
/** Read and convert a tape archive of the C64S emulator */
read_file_t ReadT64;
/** Read and convert a Commodore C2N tape archive */
read_file_t ReadC2N;
/** Read and convert a disk image in CBM DOS format */
read_file_t ReadImage;
/** Read and convert a disk image in C128 CP/M format */
read_file_t ReadCpmImage;

#endif /* INPUT_H */
