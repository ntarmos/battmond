# Simple makefile for battmond.
#
# Author: Nikos Ntarmos <ntarmos@gmail.com>
# URL: https://github.com/ntarmos/battmond/
#

CC?= /usr/bin/gcc

default: battmond

all: battmond

battmond: battmond.c
	${CC} ${CFLAGS} battmond.c -o battmond -lutil

install: battmond
	$(INSTALL) -s battmond ${PREFIX}/sbin/battmond
	$(INSTALL) battmond.sh ${PREFIX}/etc/rc.d/battmond
	$(INSTALL) -c battmond.1 ${PREFIX}/man/man1/battmond.1
	gzip -9f ${PREFIX}/man/man1/battmond.1

clean:
	rm -f battmond

