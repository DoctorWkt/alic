#include <stdio.ah>

type FOO = struct {
  int32 err,
  void *details
};

void fred(int32 a) throws FOO *e {
  printf("hello\n");
  if (a < 0) { e.err= 1; abort; }
  printf("fred didn't abort\n");
}

public void main(void) {
  FOO thing;

  try(thing) { fred(5); }
  catch      { printf("fail on 5\n"); }

  printf("We got past the first try/catch\n");
  
  try(thing) { fred(-2); }
  catch      { printf("fail on -2\n"); }

  printf("We got past the second try/catch\n");
}
