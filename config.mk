# See LICENSE for copyright/license information

VERSION = 1.0.0

# C compiler
CC = cc
# C compiler flags
CFLAGS = -std=gnu99 -O2 -Wall -Wshadow -Wno-unused-value -static
CFLAGSDEBUG = -std=gnu99 -O0 -Wall -Wshadow
CFLAGSPROFILE = -std=gnu99 -O0 -Wall -Wshadow
# Path
PPATH = /usr
