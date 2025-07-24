# For life, you need the ncurses library installed.
#

all: ttt animals life

../alic:
	(cd ..; make install)

ttt: ../alic ttt.al
	../alic -o ttt ttt.al

animals: ../alic animals.al
	../alic -o animals animals.al

life: ../alic life.al
	../alic -c life.al
	cc -o life life.o -lncurses

clean:
	rm -f ttt animals life *.o
