all: ttt animals

../alic:
	(cd ..; make install)

ttt: ../alic ttt.al
	../alic -o ttt ttt.al

animals: ../alic animals.al
	../alic -o animals animals.al

clean:
	rm -f ttt animals
