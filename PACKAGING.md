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
mkdir build
cd build
cmake ..
cpack -G DEB -C Release
sudo dpkg -i cbmconvert*.deb
```
You can also initiate a build, run tests and create and install
more conventional packages using the following commands:
```sh
dpkg-buildpackage --no-sign
sudo dpkg -i ../cbmconvert*.deb
```
If you invoking `dpkg-buildpackage` on an arbitrary source code revision
and not a release, you will have to apply a patch like this in order to
bypass some checks:
```diff
diff --git a/debian/source/format b/debian/source/format
index 163aaf8..89ae9db 100644
--- a/debian/source/format
+++ b/debian/source/format
@@ -1 +1 @@
-3.0 (quilt)
+3.0 (native)
diff --git a/debian/changelog b/debian/changelog
index ffbec85..d4786f2 100644
--- a/debian/changelog
+++ b/debian/changelog
@@ -1,4 +1,4 @@
-cbmconvert (2.1.5-1) unstable; urgency=medium
+cbmconvert (2.1.5) unstable; urgency=medium
 
   * Maintenance release, with regression tests and bug fixes.
 
```
