CFLAGS= -g -Wall -Wno-unused-function
OBJ= astnodes.o cgen.o expr.o genast.o main.o misc.o parse.o \
	stmts.o strlits.o syms.o types.o

parse: $(OBJ)
	cc -o parse $(CFLAGS) $(OBJ)

astnodes.o: astnodes.c alic.h
	cc -c $(CFLAGS) astnodes.c

cgen.o: cgen.c alic.h
	cc -c $(CFLAGS) cgen.c

expr.o: expr.c alic.h
	cc -c $(CFLAGS) expr.c

genast.o: genast.c alic.h
	cc -c $(CFLAGS) genast.c

parse.c: parse.leg
	leg parse.leg > parse.c

parse.o: parse.c
	cc -c $(CFLAGS) parse.c

main.o: main.c alic.h
	cc -c $(CFLAGS) main.c

misc.o: misc.c alic.h
	cc -c $(CFLAGS) misc.c

syms.o: syms.c alic.h
	cc -c $(CFLAGS) syms.c

types.o: types.c alic.h
	cc -c $(CFLAGS) types.c

test: parse
	(cd tests; make)

clean:
	rm -f parse parse.c *.o
	(cd tests; make clean)
