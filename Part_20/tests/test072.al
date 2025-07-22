#include <stdio.ah>

type FOO = struct {
  int32 err,
  void *details
};

void fred(int32 a) throws FOO *e {
  printf("hello\n");
  if (a < 0) { e.err= 1; abort; }
  printf("foo\n");
}

public void main(void) {
  fred(5);
}
