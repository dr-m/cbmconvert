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
See [separate instructions](PACKAGING.md) how to create installation
packages for various operating systems.

```sh
mkdir build
cd build
cmake ..
cmake --build .
ctest -C Debug
cmake --install .
```
Note: The `-C Debug` option for `ctest` is only needed on
multi-target generators, such as Microsoft Visual Studio
or Ninja Multi-Config.

On many Unix-like systems, a single-target 'Unix Makefiles' generator
will be used by default, and the type of the target may be changed
by executing one of the following in the build directory:
```sh
cmake -DCMAKE_BUILD_TYPE=Debug .
cmake --build .
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo .
cmake --build .
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

## Instrumentation and Test Coverage

It can be useful to run tests on instrumented builds. To do that, you
may adapt one of the `cmake --build` and `ctest` invocations above
and specify some `CMAKE_C_FLAGS`. Examples include:
* `-fanalyzer` (GCC static analysis)
* `-fsanitize=address` (GCC and Clang; environment variable `ASAN_OPTIONS`)
* `-fsanitize=undefined` (GCC and Clang; environment variable `UBSAN_OPTIONS`)
* `-fsanitize=memory` (Clang; environment variable `MSAN_OPTIONS`)
* `--coverage` (GCC code coverage; invoke `gcov` on the `*.gcno` files)

For the sanitizers, you may want to specify a file name prefix `log_path`
for any error messages, instead of having them written via the standard error
to a `ctest` log file:
```sh
UBSAN_OPTIONS=print_stacktrace=1:log_path=ubsan ctest
ASAN_OPTIONS=abort_on_error=1:log_path=asan ctest
```

## Further information

For more information, see [cbmconvert.html](cbmconvert.html) and
the manual pages:
```sh
man cbmconvert
man zip2disk
man disk2zip
```
