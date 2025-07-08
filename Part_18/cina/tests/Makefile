all: runtests out err test001.al
	./runtests

stop: runtests out err test001.al
	./runtests stop

out:
	ln -s ../../tests/out .

err:
	ln -s ../../tests/err .

test001.al:
	ln -s ../../tests/*.al .

clean:
	rm -f bin *.[qs] trial *.o foo out err *.al
