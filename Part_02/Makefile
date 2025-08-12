# Define the location of the include directory
# and the location to install the compiler binary

INCDIR=/tmp/alic/include
BINDIR=/tmp/alic/bin
LIBALIC=/tmp/alic/lib/libalic.a
CPU=CPU_`uname -m`

# 64-bit
CFLAGS= -g -Wall -Wno-unused-function -Wno-missing-braces

# PDP-11
#CFLAGS= -O
#LDFLAGS= -i
#CPU=CPU_pdp11

OBJ= astnodes.o expr.o funcs.o genast.o lexer.o main.o \
	misc.o parser.o stmts.o strlits.o syms.o types.o

all: alic il2asm il2text

alic: incdir.h $(OBJ)
	cc -o alic $(LDFLAGS) $(OBJ)

incdir.h:
	echo "#define INCDIR \"$(INCDIR)\"" > incdir.h
	echo "#define BINDIR \"$(BINDIR)\"" >> incdir.h
	echo "#define IL2ASM \"$(BINDIR)/il2asm\"" >> incdir.h
	echo "#define LIBALIC \"$(LIBALIC)\"" >> incdir.h
	echo "#define $(CPU) 1" >> incdir.h

il2asm: il2asm.c il2qbe.c alic.h incdir.h
	cc -o il2asm $(CFLAGS) il2asm.c il2qbe.c

il2text: il2text.c alic.h incdir.h
	cc -o il2text $(CFLAGS) il2text.c

install: alic il2asm il2text
	-mkdir -p $(INCDIR)
	cp -r include/. $(INCDIR)
	-mkdir -p $(BINDIR)
	cp alic il2asm il2text $(BINDIR)
	chmod +x $(BINDIR)/*
	(cd lib; make install)

astnodes.o: astnodes.c alic.h incdir.h

expr.o: expr.c alic.h incdir.h

funcs.o: funcs.c alic.h incdir.h

genast.o: genast.c alic.h incdir.h

lexer.o: lexer.c alic.h incdir.h

main.o: main.c alic.h incdir.h

misc.o: misc.c alic.h incdir.h

parser.o: parser.c alic.h incdir.h

stmts.o: stmts.c alic.h incdir.h

strlits.o: strlits.c alic.h incdir.h

syms.o: syms.c alic.h incdir.h

types.o: types.c alic.h incdir.h

grammar:
	@grep '//-' parser.c | grep -v 'Note:' | sed 's/\/\/-//;s/^ //'

test: install
	(cd tests; make)

clean:
	rm -f alic *.o incdir.h il2asm il2text
	(cd tests; make clean)
	(cd lib; make clean)

tar: clean
	(cd .. ; bsdtar vcf ../palic.tar palic/*.[ch] palic/Makefile palic/include)
