LIBDIR=/tmp/alic/lib

OBJ= aarrays.o

all: libalic.a

libalic.a: $(OBJ)
	rm -f libalic.a
	ar q libalic.a $(OBJ)

install: libalic.a
	mkdir -p $(LIBDIR)
	cp libalic.a $(LIBDIR)

clean:
	rm -f libalic.a *.o
