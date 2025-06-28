all: ttt

../alic:
	(cd ..; make install)

ttt: ../alic ttt.al
	../alic -o ttt ttt.al

clean:
	rm -f ttt
