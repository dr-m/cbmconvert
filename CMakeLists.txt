CMAKE_MINIMUM_REQUIRED (VERSION 3.14)

PROJECT (cbmconvert)

ADD_EXECUTABLE (cbmconvert main.c util.c read.c write.c lynx.c unark.c unarc.c
  t64.c c2n.c image.c archive.c util.h input.h output.h)
ADD_EXECUTABLE (zip2disk zip2disk.c)
ADD_EXECUTABLE (disk2zip disk2zip.c)

INSTALL(FILES cbmconvert.1 zip2disk.1 disk2zip.1 TYPE MAN)
INSTALL(TARGETS cbmconvert zip2disk disk2zip)
