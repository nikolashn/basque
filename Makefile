# See LICENSE for copyright/license information

include config.mk

all:
	${CC} basque.c -o basque ${CFLAGS}

debug:
	${CC} basque.c -o basque -g ${CFLAGSDEBUG}

clean:
	rm -f basque

install: all
	mkdir -p ${PPATH}/bin
	cp -f basque $${PPATH}/bin
	chmod 755 ${PPATH}/bin/basque
	cp basque.1 ${PPATH}/share/man/man1/basque.1

uninstall:
	rm -f ${PPATH}/bin/basque
	rm -f ${PPATH}/share/man/man1/basque.1


.PHONY: all clean install uninstall

