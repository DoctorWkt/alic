CFLAGS= -g -Wall -Wno-unused-function
OBJ= astnodes.o cgen.o expr.o funcs.o genast.o lexer.o main.o \
	misc.o parser.o stmts.o strlits.o syms.o types.o

alic: $(OBJ)
	cc -o alic $(CFLAGS) $(OBJ)

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

test: alic
	(cd tests; make)

grammar:
	@grep '//-' parser.c | grep -v 'Note:' | sed 's/\/\/-//;s/^ //'

clean:
	rm -f alic *.o
	(cd tests; make clean)
