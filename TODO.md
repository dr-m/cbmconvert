# Things to be done (feel free to contribute):

## image.c
* `ReadImage()`: Check the BAM
* 1581: changing of subdirectories
* specifying the disk name and ID
* `setupSideSectors()` and `checkSideSectors()`: 1581 support
* `validate (Image *image, bool correct)` needs to be written (also for GEOS),
with output like the following:
```
Unallocated but used blocks:
18,0 18,1 18,3

Allocated but unused blocks:
18,4

Improper free blocks count on tracks:
17 (10, should be 9)
18 (1, should be 3)
```
* use memory-mapped files or sector-level file access (with seeks)

## main.c:
* interactive GNU Readline based interface

## integrate `cbmconvert` with `cbmlink`
* writing files to Commodore memory
* reading files from Commodore memory?
* writing and reading files on Commodore mass storage devices
* disk image access?  maybe not; arbitrary seeks would be slow

## tests
* implement `ctest_coverage()`
* implement `ctest_memcheck()` (for ASAN, MSAN, UBSAN)
