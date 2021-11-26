CMAKE_MINIMUM_REQUIRED (VERSION 3.0.2)

PROJECT (cbmconvert)

ADD_EXECUTABLE (cbmconvert main.c util.c read.c write.c lynx.c unark.c unarc.c
  t64.c c2n.c image.c archive.c util.h input.h output.h)
ADD_EXECUTABLE (zip2disk zip2disk.c)
ADD_EXECUTABLE (disk2zip disk2zip.c)

INSTALL(FILES cbmconvert.1 zip2disk.1 disk2zip.1 DESTINATION "share/man/man1")
INSTALL(TARGETS cbmconvert zip2disk disk2zip)

ENABLE_TESTING()
ADD_TEST (cbmconvert_usage ${CMAKE_BINARY_DIR}/cbmconvert)
ADD_TEST (disk2zip_usage ${CMAKE_BINARY_DIR}/disk2zip)
ADD_TEST (zip2disk_usage ${CMAKE_BINARY_DIR}/zip2disk)

SET_TESTS_PROPERTIES(cbmconvert_usage disk2zip_usage zip2disk_usage
  PROPERTIES WILL_FAIL TRUE)

ADD_TEST (NAME empty_files
  COMMAND ${CMAKE_COMMAND}
  -DCBMCONVERT=$<TARGET_FILE:cbmconvert>
  -DZIP2DISK=$<TARGET_FILE:zip2disk>
  -DDISK2ZIP=$<TARGET_FILE:disk2zip>
  -P ${CMAKE_CURRENT_SOURCE_DIR}/empty_files.cmake)

ADD_TEST (NAME small_files
  COMMAND ${CMAKE_COMMAND}
  -DCBMCONVERT=$<TARGET_FILE:cbmconvert>
  -P ${CMAKE_CURRENT_SOURCE_DIR}/small_files.cmake)
