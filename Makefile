# See LICENSE for copyright/license information

include config.mk

all:
	${CC} basque.c -o basque ${CFLAGS}

debug:
	${CC} basque.c -o basque -g ${CFLAGSDEBUG}

clean:
	rm -f basque

install: all
	mkdir -p ${DESTDIR}${PPATH}/bin
	cp -f basque ${DESTDIR}${PPATH}/bin
	chmod 755 ${DESTDIR}${PPATH}/bin/basque

uninstall:
	rm -f ${DESTDIR}${PPATH}/bin/basque


.PHONY: all clean install uninstall

