/**
 * @file unarc.c
 * ARC/SDA (C64/C128) archive extractor
 * @author Chris Smeets
 * @author Marko Mäkelä (marko.makela@nic.funet.fi)
 */

/*
**  Derived from Chris Smeets' MS-DOS utility a2l that converts Commodore
**  ARK, ARC and SDA files to LZH using LHARC 1.12. Optimized for speed,
**  converted to standard C and adapted to the cbmconvert package by
**  Marko Mäkelä.
**
**  Original version: a2l.c       March   1st, 1990   Chris Smeets
**  Unix port:        unarc.c     August 28th, 1993   Marko Mäkelä
**  Restructured for cbmconvert 2.0 and 2.1 by Marko Mäkelä
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <setjmp.h>

#include "input.h"

/** I/O status. 0=ok, or EOF */
static int Status;
/** Current offset from ARC or SDA file's beginning */
static long FilePos;
/** Current archive */
static FILE* fp;
/** Bit Buffer */
static unsigned int BitBuf;
/** checksum */
static unsigned int  crc;
/** used in checksum calculation */
static unsigned char crc2;
/** Huffman codes */
static unsigned long hc[256];
/** Lengths of huffman codes */
static unsigned char hl[256];
/** Character associated with Huffman code */
static unsigned char hv[256];
/** Number of Huffman codes */
static unsigned int  hcount;
/** Run-Length control character */
static unsigned int  ctrl;

/** C64 Archive entry header */
struct entry
{
  /** Version number, must be 1 or 2 */
  unsigned char version;
  /** 0=store, 1=pack, 2=squeeze, 3=crunch,
      4=squeeze+pack, 5=crunch in one pass */
  unsigned char mode;
  /** Checksum */
  unsigned int  check;
  /** Original size. Only three bytes are stored */
  long          size;
  /** Compressed size in CBM disk blocks */
  unsigned int  blocks;
  /** File type. P,S,U or R */
  unsigned char type;
  /** Filename length */
  unsigned char fnlen;
  /** Filename. Only fnlen bytes are stored */
  unsigned char name[17];
  /** Record length if relative file */
  unsigned char rl;
  /** Date archive was created. Same format as in MS-DOS directories */
  unsigned int  date;
};

/** Current C64 Archive entry header */
struct entry entry;

/** Lempel Zev compression string table entry */
struct lz
{
  unsigned int prefix;	/**< Prefix code */
  byte_t ext;		/**< Extension character */
};

/** Lempel Zev compression string table */
struct lz lztab[4096];

/** Lempel Zev stack handling error codes */
enum LZStackErrorType
{
  PushError = 1,	/**< Stack overflow */
  PopError		/**< Stack underflow (out of data) */
};

/** Lempel Zev exception handling structure */
static jmp_buf LZStackError;

/** Lempel Zev stack */
static byte_t stack[512];

/** Set to 0 to reset un-crunch */
static int State = 0;
/** Lempel Zev stack pointer */
static int lzstack = 0;
/** Current code size */
static int cdlen;
/** Last received code */
static int code;
/** Bump cdlen when code reaches this value */
static int wtcl;
/** Copy of wtcl */
static int wttcl;

/** Shell Sort algorithm
 * from "C Programmer's Library" by Purdum, Leslie and Stegemoller
 */
static void
ssort (void)
{
  int m;
  int h,i,j,k;

  m = sizeof hl;

  while (m >>= 1) {
    k = (sizeof hl) - m;
    j = 1;
    do {
      i = j;
      do {
	h = i + m;
	if (hl[h - 1] > hl[i - 1]) {
	  unsigned long t;
	  unsigned char u;
	  t = hc[i - 1], hc[i - 1] = hc[h - 1], hc[h - 1] = t;
	  u = hv[i - 1], hv[i - 1] = hv[h - 1], hv[h - 1] = u;
	  u = hl[i - 1], hl[i - 1] = hl[h - 1], hl[h - 1] = u;
	  i -= m;
	}
	else
	  break;
      } while (i >= 1);
      j += 1;
    } while(j <= k);
  }
}

/** Receive a byte (eight bits) from the input
 * @return	the received byte
 */
static byte_t
GetByte (void)
{
  if (Status == EOF)
    return 0;

  if (feof (fp) || ferror (fp)) {
    Status = EOF;
    return 0;
  }
  else
    Status = 0;

  return (unsigned char) (fgetc (fp) & 0xff);
}

/** Receive a word (sixteen bits) from the input
 * @return	the received word
 */
static word_t
GetWord (void)
{
  word_t u = 0;

  if (Status == EOF)
    return 0;

  if (feof (fp) || ferror (fp)) {
    Status = EOF;
    return 0;
  }
  else {
    Status = 0;
  }

  u = (word_t) fgetc (fp) & 0xff;
  u |= ((word_t) fgetc (fp) & 0xff) << 8;

  return u;
}

/** Receive a three-byte integer (twenty-four bits) from the input
 * @return	the received integer
 */
static tbyte_t
GetThree (void)
{
  tbyte_t u = 0;

  if (Status == EOF || feof (fp) || ferror (fp)) {
    Status = EOF;
    return 0;
  }
  else
    Status = 0;

  u = (tbyte_t) (fgetc (fp) & 0xff);
  u |= ((tbyte_t) fgetc (fp) & 0xff) << 8;
  u |= ((tbyte_t) fgetc (fp) & 0xff) << 16;

  return u;
}

/** Receive a bit from the input
 * @return	the received bit
 */
static bool
GetBit (void)
{
  register int result = BitBuf >>= 1;

  if (result == 1)
    return (bool) (1 & (BitBuf = GetByte() | 0x0100));
  else
    return (bool) (1 & result);
}

/** Fetch a Huffman code and convert it to what it represents
 * @return	the converted code
 */
static byte_t
Huffin (void)
{
  long hcode = 0;
  long mask  = 1;
  int  size  = 1;
  int  now;

  now = hcount;       /* First non=zero Huffman code */

  do {
    if (GetBit ())
      hcode |= mask;

    while( hl[now] == size) {

      if (hc[now] == hcode)
	return hv[now];

      if (--now < 0) {         /* Error in decode table */
	Status = EOF;
	return 0;
      }
    }
    size++;
    mask = mask << 1;
  } while (size < 24);

  Status = EOF;                /* Error. Huffman code too big */
  return 0;
}

/** Fetch ARC64 header.
 * @return	true if header is ok.
 */
static bool
GetHeader (void)
{
  unsigned int  w, i;
  const char LegalTypes[] = "SPUR";
  unsigned long mask;

  if (feof(fp) || ferror(fp))
    return false;
  else
    Status = 0;

  BitBuf        = 2;              /* Clear Bit buffer */
  crc           = 0;              /* checksum */
  crc2          = 0;              /* Used in checksum calculation */
  State         = 0;              /* LZW state */
  ctrl          = 254;            /* Run-Length control character */

  entry.version = GetByte();
  entry.mode    = GetByte();
  entry.check   = GetWord();
  entry.size    = GetThree();
  entry.blocks  = GetWord();
  entry.type    = GetByte();
  entry.fnlen   = GetByte();

  /* Check for invalid header, If invalid, then we've input past the end */
  /* Possibly due to XMODEM padding or whatever */

  if (entry.fnlen > 16)
    return 0;

  for (w=0; w < entry.fnlen; w++)
    entry.name[w] = GetByte();

  entry.name[entry.fnlen] = 0;

  if (entry.version > 1) {
    entry.rl  = GetByte();
    entry.date= GetWord();
  }

  if (Status == EOF)
    return false;

  if (entry.version == 0 || entry.version > 2)
    return false;

  if (entry.version == 1) { /* If ARC64 version 1.xx */
    if (entry.mode > 2)     /* Only store, pack, squeeze */
      return false;
  }
  if (entry.mode == 1)      /* If packed get control char */
    ctrl = GetByte();       /* V2 always uses 0xfe V1 varies */

  if (entry.mode > 5)
    return false;

  if ((entry.mode == 2) || (entry.mode == 4)) { /* if squeezed or squashed */
    hcount = 255;                                 /* Will be first code */

    for (w=0; w<256; w++) {                       /* Fetch Huffman codes */
      hv[w] = w;

      hl[w]=0;
      mask = 1;
      for (i=1; i<6; i++) {
	if (GetBit())
	  hl[w] |= mask;
	mask <<= 1;
      }

      if (hl[w] > 24)
	return false;                             /* Code too big */

      hc[w] = 0;
      if (hl[w]) {
	i = 0;
	mask = 1;
	while (i<hl[w]) {
	  if (GetBit())
	    hc[w] |= mask;
	  i++;
	  mask <<= 1;
	}
      }
      else
	hcount--;
    }
    ssort ();
  }

  return !!strchr (LegalTypes, entry.type);
}

/** Get start of data.  Ignores SDA header.
 * @return	the starting position of useful data within the file
 *		(normally 0), or -1 if not an archive
 */
static long
GetStartPos (void)
{
  int c;                      /* Temp */
  int cpu;                    /* C64 or C128 if SDA */
  int linenum;                /* Sys line number */
  int skip;                   /* Size of SDA header in bytes */

  fseek(fp, 0, SEEK_SET);     /* Goto start of file */
  Status = 0;

  if ( (c=GetByte()) == 2)    /* Probably type 2 archive */
    return 0;                 /* Data starts at offset 0 */

  if (c != 1)                 /* IBM archive, or not an archive at all */
    return -1;

  /* Check if its an SDA */

  GetByte();           /* Skip to line number (which is # of header blocks) */
  GetWord();
  linenum = GetWord();
  c = GetByte();

  if (c != 0x9e)              /* Must be BASIC SYS token */
    return 0;                 /* Else probably type 1 archive */

  c = GetByte();              /* Get SYS address */
  cpu = GetByte();            /* '2' for C64, '7' for C128 */

  skip = (linenum-6)*254;     /* True except for SDA232.128 */

  if ( (linenum==15) && (cpu=='7') )   /* handle the special case */
    skip -= 1;

  return skip;
}

/*
 * Un-Crunch a byte
 *
 * This is pretty straight forward if you have Terry Welch's article
 * "A Technique for High Performance Data Compression" from IEEE Computer
 * June 1984
 *
 * This implemention reserves code 256 to indicate the end of a crunched
 * file, and code 257 was reserved for future considerations. Codes grow
 * up to 12 bits and then stay there. There is no reset of the string
 * table.
 */

/** Push a byte to the Lempel Zev stack
 * @param c	the byte to be pushed
 */
static void
push (byte_t c)
{
  if (lzstack >= sizeof stack)
    longjmp (LZStackError, PushError);
  else
    stack[lzstack++] = c;
}

/** Pop a byte from the Lempel Zev stack
 * @return	the popped byte
 */
static byte_t
pop (void)
{
  if (!lzstack)
    longjmp (LZStackError, PopError);
  else
    return stack[--lzstack];
}

/** Fetch LZ code
 * @return	the fetched code
 */
static int getcode (void)
{
  register int i;
  long blocks;

  code = 0;
  i = cdlen;

  while(i--)
    code = (code << 1) | GetBit ();

  /*  Special case of 1 pass crunch. Checksum and size are at the end */

  if ((code == 256) && (entry.mode == 5)) {
    i = 16;
    entry.check = 0;
    while (i--)
      entry.check = (entry.check << 1) | GetBit ();
    i = 24;
    entry.size = 0;
    while (i--)
      entry.size = (entry.size << 1) | GetBit ();
    i = 16;
    while (i--)                     /* This was never implemented */
      GetBit ();
    blocks = ftell(fp)-FilePos;
    entry.blocks = blocks/254;
    if (blocks % 254)
      entry.blocks++;
  }

  /* Get ready for next time */

  if ((cdlen < 12)) {
    if (!(--wttcl)) {
      wtcl = wtcl << 1;
      cdlen++;
      wttcl = wtcl;
    }
  }

  return code;
}

/** Un-crunch a byte
 * @return	the uncrunched byte
 */
static byte_t
unc (void)
{
  static int  oldcode, incode;
  static byte_t kay;
  static int  omega;
  static unsigned char finchar;
  static int  ncodes;   /* Current # of codes in table */

  switch (State) {

  case 0:                  /* First time. Reset. */
    lzstack = 0;
    ncodes  = 258;         /* 2 reserved codes */
    wtcl    = 256;         /* 256 Bump code size when we get here */
    wttcl   = 254;         /* 1st time only 254 due to resvd codes */
    cdlen   = 9;           /* Start with 9 bit codes */
    oldcode = getcode();

    if (oldcode == 256) {  /* Code 256 is EOF for this entry */
      Status = EOF;        /* (ie. a zero length file) */
      return 0;
    }
    kay = oldcode & 0xff;
    finchar = kay;
    State = 1;
    return kay;

  case 1:
    incode = getcode();

    if (incode == 256) {
      State = 0;
      Status = EOF;
      return 0;
    }

    if (incode >= ncodes) {     /* Undefined code, special case */
      kay = finchar;
      push(kay);
      code = oldcode;
      omega = oldcode;
      incode = ncodes;
    }
    while ( code > 255 ) {      /* Decompose string */
      push(lztab[code].ext);
      code = lztab[code].prefix;
    }
    kay = code;
    finchar = code;
    State = 2;
    return kay;

  case 2:
    if (!lzstack) {             /* Empty stack */
      omega = oldcode;
      if (ncodes < sizeof lztab / sizeof *lztab) {
	lztab[ncodes].prefix = omega;
	lztab[ncodes].ext = kay;
	ncodes++;
      }
      oldcode = incode;
      State = 1;
      return unc();
    }
    else
      return pop();
  }

  Status = EOF;
  return 0;
}

/** Update the checksum
 * @param c	the data to be added to the checksum
 */
static void
UpdateChecksum (int c)
{
  c &= 0xff;

  if (entry.version == 1)     /* Simple checksum for version 1 */
    crc += c;
  else
    crc += (c ^ (++crc2));    /* A slightly better checksum for version 2 */
}

/** Unpack a byte
 * @return	the unpacked byte
 */
static byte_t
UnPack (void)
{
  switch (entry.mode) {

  case 0:             /* Stored */
  case 1:             /* Packed (Run-Length) */
    return GetByte();

  case 2:             /* Squeezed (Huffman only) */
  case 4:             /* Squashed (Huffman + Run-Length) */
    return Huffin();

  case 3:             /* Crunched */
  case 5:             /* Crunched in one pass */
    return unc();

  default:            /* Otherwise ERROR */
    Status = EOF;
    return 0;
  }
}

/** Read and convert an ARC/SDA archive
 * @param file		the file input stream
 * @param filename	host system name of the file
 * @param writeCallback	function for writing the contained files
 * @param log		Call-back function for diagnostic output
 * @return		status of the operation
 */
enum RdStatus
ReadARC (FILE* file,
	 const char* filename,
	 write_file_t writeCallback,
	 log_t log)
{
  fp = file;

  switch (setjmp (LZStackError)) {
  case PopError:
    (*log) (Errors, 0, "Lempel Zev stack underflow");
    return RdFail;
  case PushError:
    (*log) (Errors, 0, "Lempel Zev stack overflow");
    return RdFail;
  }

  {
    long temp;

    if ((temp = GetStartPos()) < 0) {
      (*log) (Errors, 0, "Not a Commodore ARC or SDA.");
      return RdFail;
    }
    else if (fseek (fp, temp, SEEK_SET)) {
      (*log) (Errors, 0, "fseek: %s", strerror(errno));
      return RdFail;
    }
  }

  FilePos = ftell (fp);

  while (GetHeader()) {
    byte_t* buffer;
    byte_t* buf;
    struct Filename name;

    long length = entry.size;

    if (entry.mode == 5) /* If 1 pass crunch size is unknown */
      length = 65536; /* 64kB should be enough for everyone */

    if (!(buf = buffer = malloc (length))) {
      (*log) (Errors, 0, "Out of memory.");
      return RdFail;
    }

    while (buf < &buffer[length]) {
      byte_t c = UnPack ();

      if (Status == EOF)
	break;

      /* If Run Length is needed */

      if (entry.mode != 0 && entry.mode != 2 && c == ctrl) {
	int count = UnPack ();
	c = UnPack ();

	if (Status == EOF)
	  break;

	if (count == 0)
	  count = entry.version == 1 ? 255 : 256;

	while (--count)
	  UpdateChecksum (*buf++ = c);
      }

      UpdateChecksum (*buf++ = c);
    }

    /* Set up the file name information */
    {
      int i;
      for (i = 0; i < entry.fnlen && i < sizeof name.name; i++)
	name.name[i] = entry.name[i];

      /* pad the file name with shifted spaces */

      while (i < sizeof name.name)
	name.name[i++] = 0xA0;

      switch (entry.type) {
      case 'S':
	name.type = SEQ;
	break;
      case 'P':
	name.type = PRG;
	break;
      case 'U':
	name.type = USR;
	break;
      case 'R':
	name.type = REL;
	name.recordLength = entry.rl;
	break;
      default:
	name.type = 0;
	(*log) (Errors, &name, "Unknown type, defaulting to DEL");
	name.type = DEL;
	break;
      }
    }

    if ((crc ^ entry.check) & 0xffff)
      (*log) (Errors, &name, "Checksum error!");

    switch ((*writeCallback) (&name, buffer, buf - buffer)) {
    case WrOK:
      break;
    case WrNoSpace:
      free (buffer);
      return RdNoSpace;
    case WrFail:
    default:
      free (buffer);
      return RdFail;
    }

    free (buffer);

    FilePos += (long)entry.blocks * 254;

    if (fseek (fp, FilePos, SEEK_SET)) {
      (*log) (Errors, 0, "fseek: %s", strerror (errno));
      return RdFail;
    }
  }

  return RdOK;
}
