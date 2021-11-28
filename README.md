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

You can build, test, and install the code as follows.
But see below how to create installation packages for
various operating systems.

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

On many Unix-like systems, a single-target 'Unix Makefiles' generator
will be used by default, and the type of the target may be changed
by executing one of the following in the build directory:
```sh
cmake -DCMAKE_BUILD_TYPE=Debug .
cmake --build .
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo .
cmake --build .
```

### Creating an installation package

```sh
mkdir build
cd build
cmake ..
cpack -G RPM
```
The command `cpack --help` will display the list of generators
that are available on your system.

#### Creating a `.msi` file for Microsoft Windows

```sh
mkdir build
cd build
cmake ..
cmake --build . --config Release
cpack -G WIX
```

#### Creating a `.deb` for Debian GNU/Linux, Ubuntu, and similar systems

The `cpack` generator will skip `ctest` or the creation of a separate
package for debug information:
```sh
cpack -G DEB
sudo dpkg -i cbmconvert*.deb
```
You can also initiate a build, run tests and create and install
more conventional packages using the following commands:
```sh
fakeroot dpkg-buildpackage --no-sign
sudo dpkg -i ../cbmconvert*.deb
```

### Multi-target configuration

Starting with CMake 3.17, the `Ninja Multi-Config` generator may be used
to compile multiple types of executables in a single build directory:

```sh
mkdir build
cd build
cmake .. -G 'Ninja Multi-Config'
cmake --build --config Debug .
cmake --build --config RelWithDebInfo .
ctest -C Debug
ctest -C RelWithDebInfo
cmake --install . --config RelWithDebInfo
```

## Further information

For more information, see [cbmconvert.html](cbmconvert.html) and
the manual pages:
```sh
man cbmconvert
man zip2disk
man disk2zip
```
