# Creating installation packages

## Unix-like systems (`Unix Makefiles`)

```sh
mkdir build
cd build
cmake ..
cpack -G RPM -C Release
```
The command `cpack --help` will display the list of generators
that are available on your system.

## Microsoft Windows

In the Microsoft Visual Studio installer, be sure to install the
components for CMake integration.

You will also need the following:
* [Git](https://git-scm.com/download/win)
* [CMake](https://cmake.org/download/)
* [WiX Toolset](https://wixtoolset.org/releases/)

Execute the following in the Visual Studio Developer Command Prompt
(a specially configured `cmd.exe` or Powershell).

```sh
mkdir build
cd build
cmake ..
cmake --build . --config Release
cpack -G WIX
```

## Debian GNU/Linux, Ubuntu, and similar systems

The `cpack` generator will skip `ctest` or the creation of a separate
package for debug information:
```sh
cpack -G DEB -C Release
sudo dpkg -i cbmconvert*.deb
```
You can also initiate a build, run tests and create and install
more conventional packages using the following commands:
```sh
dpkg-buildpackage --no-sign
sudo dpkg -i ../cbmconvert*.deb
```
