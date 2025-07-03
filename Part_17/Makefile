# Define the location of the include directory
# and the location to install the compiler binary

INCDIR=/tmp/alic/include
BINDIR=/tmp/alic/bin
LIBALIC=/tmp/alic/lib/libalic.a

# If you want to see the BNF-style grammar for alic, do
# $ make grammar
#
# If you want to run the tests, do $ make test
#
# Otherwise, use $ make or $make clean

CFLAGS= -g -Wall -Wno-unused-function -Wno-missing-braces
OBJ= astnodes.o cgen.o expr.o funcs.o genast.o lexer.o main.o \
	misc.o parser.o stmts.o strlits.o syms.o types.o

alic: incdir.h $(OBJ)
	cc -o alic $(CFLAGS) $(OBJ)

incdir.h:
	echo "#define INCDIR \"$(INCDIR)\"" > incdir.h
	echo "#define LIBALIC \"$(LIBALIC)\"" >> incdir.h

install: alic
	mkdir -p $(INCDIR)
	rsync -a include/. $(INCDIR)
	mkdir -p $(BINDIR)
	cp alic $(BINDIR)
	chmod +x $(BINDIR)/alic
	(cd lib; make install)

astnodes.o: astnodes.c alic.h
	cc -c $(CFLAGS) astnodes.c

cgen.o: cgen.c alic.h
	cc -c $(CFLAGS) cgen.c

expr.o: expr.c alic.h
	cc -c $(CFLAGS) expr.c

genast.o: genast.c alic.h
	cc -c $(CFLAGS) genast.c

lexer.o: lexer.c alic.h
	cc -c $(CFLAGS) lexer.c

main.o: main.c alic.h
	cc -c $(CFLAGS) main.c

misc.o: misc.c alic.h
	cc -c $(CFLAGS) misc.c

parser.o: parser.c alic.h
	cc -c $(CFLAGS) parser.c

syms.o: syms.c alic.h
	cc -c $(CFLAGS) syms.c

types.o: types.c alic.h
	cc -c $(CFLAGS) types.c

test: install
	(cd tests; make)

triple:
	(cd cina; make triple)

grammar:
	@grep '//-' parser.c | grep -v 'Note:' | sed 's/\/\/-//;s/^ //'

clean:
	rm -f alic *.o incdir.h
	(cd tests; make clean)
	(cd lib; make clean)
	(cd cina; make clean)
