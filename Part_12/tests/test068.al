#include <stdio.ah>

type FOO = struct {
  int32 a,
  int16 b
};

FOO jim;

void main(void) {
  FOO *ptr;

  ptr   = &jim;		// Point at the struct
  ptr.a = 5;		// Access the members
  ptr.b = 23;		// through the pointer

  printf("No more ->, we have %d and %d\n", ptr.a, ptr.b);
}
