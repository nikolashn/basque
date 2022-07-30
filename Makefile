# See LICENSE for copyright/license information

include config.mk
CFILES = basque.c bltin/bltin.c bltin/u64tostr.c bltin/sys.c bltin/memcopy.c common/stack.c common/reg.c common/u64tostr.c common/types.c common/hashtable.c common/im.c common/parser.c common/elf.c common/dynarr.c common/lexeme.c common/exitmsg.c common/symtable.c common/ctr.c common/func.c elf64.c lexer.c parser/common.c parser/exp.c parser/parse.c parser/handle.c parser/op.c parser/stmt.c

all:
	${CC} ${CFILES} -o basque ${CFLAGS}

debug:
	${CC} ${CFILES} -o basque -g ${CFLAGSDEBUG}

profile:
	${CC} ${CFILES} -o basque -g ${CFLAGSPROFILE} -pg

clean:
	rm -f basque

vim:
	cp editor/basque.vim ~/.vim/syntax/

nvim:
	cp editor/basque.vim ~/.config/nvim/syntax/

install: all
	mkdir -p ${PPATH}/bin
	cp -f basque ${PPATH}/bin
	chmod 755 ${PPATH}/bin/basque
	cp basque.1 ${PPATH}/share/man/man1/basque.1

uninstall:
	rm -f ${PPATH}/bin/basque
	rm -f ${PPATH}/share/man/man1/basque.1


.PHONY: all debug profile clean vim install uninstall

