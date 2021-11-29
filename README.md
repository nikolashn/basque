# Basque
Basque intends to be a general-purpose statically-typed procedural programming language, similar to C, but with some differences and improvements. The C Basque compiler currently only statically compiles for Linux x86\_64 ELF, ahead of time, and takes ASCII input.

## Installation
You will need a C compiler and GNU make. Edit config.mk to set compiler and other compilation options.

Run the following command in the root directory (might need to be run as root user):
```
make install
```

## Command-line options
Run `basque -h` for a list of command-line options.

## License
Basque is licensed under the GNU General Public License v3.0.

