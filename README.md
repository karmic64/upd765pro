# upd765pro

upd765pro is a command-line utility for the creation of Amstrad CPC .dsk images. Building requires a C compiler (preferably `gcc`), and building the test disk (optional) requires libpng and [sjasmplus](https://z00m128.github.io/sjasmplus/documentation.html).

## Usage

Usage is straightforward: the first argument is the output disk filename, and all arguments following are the names of the filenames to put on the disk. It is possible to specify no input files- doing this will result in a blank disk image.

The attributes of a file can be modified by adding "file arguments" to the filename, each one separated by a comma:
* `ro` indicates the file should be marked read-only.
* `sys` indicates the file should be marked as a system file (will not appear in catalogs).
* `b__` (where `__` is a number) forces the file to start at the given block.

For example, if you execute the tool with the following arguments:

```upd765pro test-dsk.dsk main.bin,ro image.bin,ro,sys```

A disk image called `test-dsk.dsk` will be created. The file `main.bin` will be added to the disk and marked read-only, and the file `image.bin` will be added to the disk and marked as both read-only and a system file.

## Things to note

Input files MUST have a valid AMSDOS header to be used with this tool, as the filename written to the disk image is taken from it. The only exception is the checksum- it can be any value, as the tool corrects it before writing it to the disk image. No other changes are made to the header before writing.

This tool is only capable of writing "Data Only Format" disks, so no CP/M programs.