# Basque
Basque is a general-purpose, statically-typed, procedural, C-like programming language. The C Basque compiler currently only statically compiles for Linux x86\_64 ELF, ahead of time. This will serve as a bootstrap compiler for a self-hosting Basque compiler, which will hopefully be more portable and complete.

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
Then run `make vim` or `make nvim` (not as root!).

## Command-line options
Run `basque -h` for a list of command-line options. The makefile will also install a man page with further details about these options.

## License
Basque is licensed under the GNU General Public License v3.0.

