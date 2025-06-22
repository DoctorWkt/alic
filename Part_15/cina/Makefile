# Define the location of the include directory
# and the location to install the compiler binary

INCDIR=/tmp/alic/include
BINDIR=/tmp/alic/bin
#ASFLAGS= --gstabs+
#LDFLAGS= -gstabs+
LDFLAGS= -static

OBJ= astnodes.o cgen.o expr.o funcs.o genast.o lexer.o main.o \
	misc.o parser.o stmts.o strlits.o syms.o types.o

%.o: %.al
	../alic -S $<
	as -o $@ $(ASFLAGS) $*.s

alica: ../alic incdir.ah $(OBJ)
	cc -o alica $(LDFLAGS) $(OBJ)

../alic:
	(cd ..; make install)

astnodes.o: ../alic incdir.ah alic.ah proto.ah

expr.o: ../alic incdir.ah alic.ah proto.ah

lexer.o: ../alic incdir.ah alic.ah proto.ah

main.o: ../alic incdir.ah alic.ah proto.ah

misc.o: ../alic incdir.ah alic.ah proto.ah

syms.o: ../alic incdir.ah alic.ah proto.ah

types.o: ../alic incdir.ah alic.ah proto.ah

parser.o: ../alic incdir.ah alic.ah proto.ah

incdir.ah:
	echo "#define INCDIR \"$(INCDIR)\"" > incdir.ah

test:
	(cd tests; make)

# Do the triple test. Get alica to compile its own code
triple: alica
	rm -f *.[osq]
	./alica -S astnodes.al
	as -o astnodes.o $(ASFLAGS) astnodes.s
	./alica -S cgen.al
	as -o cgen.o $(ASFLAGS) cgen.s
	./alica -S expr.al
	as -o expr.o $(ASFLAGS) expr.s
	./alica -S funcs.al
	as -o funcs.o $(ASFLAGS) funcs.s
	./alica -S genast.al
	as -o genast.o $(ASFLAGS) genast.s
	./alica -S lexer.al
	as -o lexer.o $(ASFLAGS) lexer.s
	./alica -S main.al
	as -o main.o $(ASFLAGS) main.s
	./alica -S misc.al
	as -o misc.o $(ASFLAGS) misc.s
	./alica -S parser.al
	as -o parser.o $(ASFLAGS) parser.s
	./alica -S stmts.al
	as -o stmts.o $(ASFLAGS) stmts.s
	./alica -S strlits.al
	as -o strlits.o $(ASFLAGS) strlits.s
	./alica -S syms.al
	as -o syms.o $(ASFLAGS) syms.s
	./alica -S types.al
	as -o types.o $(ASFLAGS) types.s
	cc -o alicia $(LDFLAGS) $(OBJ)
	md5sum alica alicia

clean:
	rm -f alica alicia *.[osq] incdir.ah
	(cd tests; make clean)
