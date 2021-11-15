# cbmconvert: create, extract and convert 8-bit Commodore binary archives

`cbmconvert` extracts files from most known archive file formats that
are used on 8-bit Commodore computer platforms and writes them to
several different formats, including some formats used by some
emulators.

## Examples

To convert Lynx archive files (`*.lnx`) into 1541 disk image(s):
```sh
cbmconvert -D4 image.d64 -l *.lnx
```
To extract the individual files of a number of 1541 disk images, you
could execute the following commands in the Bourne Again shell:
```bash
for i in *.d64
do
  mkdir "${i%.d64}"
  cd "${i%.d64}"
  cbmconvert -d ../"$i"
  cd ..
done
```

## Motivation

There are many archiving programs for the Commodore 64 and other 8-bit
Commodore computers, most of which are incompatible with archiving
programs on other systems.

It is faster and more convenient to convert files with native code
running on a 32-bit or 64-bit processor than by 8-bit 6502 code
running in an old computer or an emulator.

## Requirements

* [CMake](https://cmake.org) 3.0.2 or later
* At least 32-bit C compiler, compliant to to ISO/IEC 9899:1990 (C90) or later
* A POSIX-like operating system (including Microsoft Windows)

## Installation

You can build, test, and install the code as follows:

```sh
mkdir build
cd build
cmake ..
cmake --build .
ctest -C Debug
cmake --install .
```
Note: The `-C Debug` option for `ctest` is only needed on
multi-target generators, such as Microsoft Visual Studio.
On a single-target generator, such as Unix Makefiles or Ninja,
you may specify `-DCMAKE_BUILD_TYPE=RelWithDebInfo` to the first
command.

For more information, see [cbmconvert.html](cbmconvert.html) and
the manual pages:
```sh
man cbmconvert
man zip2disk
man disk2zip
```
