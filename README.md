# Basque
Basque intends to be a general-purpose statically-typed weakly-typed procedural programming language, similar to C, but with some improvements. The C Basque compiler currently only statically compiles for Linux x86\_64 ELF, ahead of time, and takes ASCII input. This will serve as a bootstrap compiler for a self-hosting Basque compiler, which will hopefully be more portable.

## Installation
You will need a C compiler and GNU make. Edit config.mk to set compiler and other compilation options.

Run the following command in the root directory (as root user):
```
make install
```
### Vim syntax
To install the vim syntax, put the following in your .vimrc:
```
au BufRead,BufNewFile *.ba set filetype=basque
```
Then run the following command (not as root!):
```
make vim
```

## Command-line options
Run `basque -h` for a list of command-line options. The makefile will also install a man page.

## License
Basque is licensed under the GNU General Public License v3.0.

