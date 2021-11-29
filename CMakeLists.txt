CMAKE_MINIMUM_REQUIRED (VERSION 3.0.2)

SET (VERSION 2.1.4)
CONFIGURE_FILE (version.cmake version.h)
INCLUDE_DIRECTORIES (${CMAKE_CURRENT_BINARY_DIR})

IF(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  # Setting build type to RelWithDebInfo as none was specified.
  # Must occur before PROJECT
  SET(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING
   "Choose the type of build, options are: None Debug Release RelWithDebInfo
 MinSizeRel ..."
   FORCE)
  # Set the possible values of build type for cmake-gui
  SET_PROPERTY(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
   "None" "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
ENDIF()

IF (CMAKE_VERSION VERSION_GREATER_EQUAL 3.12)
  PROJECT (cbmconvert VERSION ${VERSION}
    DESCRIPTION "Create and convert Commodore 8-bit binary file archives"
    HOMEPAGE_URL "https://github.com/dr-m/cbmconvert/")
ELSEIF (CMAKE_VERSION VERSION_GREATER_EQUAL 3.9)
  PROJECT (cbmconvert VERSION ${VERSION}
    DESCRIPTION "Create and convert Commodore 8-bit binary file archives")
ELSE()
  PROJECT (cbmconvert VERSION ${VERSION})
ENDIF()

SET (CPACK_PACKAGE_DESCRIPTION
"cbmconvert extracts files from most known archive file formats that
are used on 8-bit Commodore computer platforms and writes them to
several different formats, including some formats used by some emulators.")
SET (CPACK_PACKAGE_CONTACT "marko.makela@iki.fi")
SET (CPACK_DEBIAN_PACKAGE_SECTION "otherosfs")
SET (CPACK_DEBIAN_PACKAGE_DEPENDS "libc6")
SET (CPACK_WIX_PRODUCT_GUID "aa32b470-5a32-51e2-9bb3-f0827715bee0")
SET (CPACK_WIX_UPGRADE_GUID "8f269924-924b-51fe-bc67-c87444c15aa6")
SET (CPACK_PACKAGE_INSTALL_DIRECTORY "cbmconvert")
INCLUDE (CPack)

ADD_EXECUTABLE (cbmconvert main.c util.c read.c write.c lynx.c unark.c unarc.c
  t64.c c2n.c image.c archive.c util.h input.h output.h)
ADD_EXECUTABLE (zip2disk zip2disk.c)
ADD_EXECUTABLE (disk2zip disk2zip.c)

IF (WIN32)
  INSTALL(FILES cbmconvert.html DESTINATION ".")
ELSE()
  INSTALL(FILES cbmconvert.html DESTINATION "share/doc/cbmconvert")
  INSTALL(FILES cbmconvert.1 zip2disk.1 disk2zip.1
    DESTINATION "share/man/man1")
ENDIF()
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
