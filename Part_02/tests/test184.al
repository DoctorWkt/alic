#include <stdio.ah>

type FOO = struct { int g, char h, bool i };

int32 mary(FOO *a, int b) { a.g = 100; return(a.g + b); }

int32 fred(inout FOO a, int b) { a.g = 100; return(a.g + b); }

public void main(void) {
  FOO x;
  int y;
  int z;

  x.g= 32; x.h= 23; x.i= false; y= 30;
  printf("mary: x.g %d x.h %d x.i %d\n", x.g, x.h, x.i);
  z= mary(&x, y);
  printf("mary: x.g %d x.h %d x.i %d\n", x.g, x.h, x.i);
  printf("mary: z is %d\n", z);

  x.g= 32; x.h= 23; x.i= false; y= 30;
  printf("fred: x.g %d x.h %d x.i %d\n", x.g, x.h, x.i);
  z= fred(x, y);
  printf("fred: x.g %d x.h %d x.i %d\n", x.g, x.h, x.i);
  printf("fred: z is %d\n", z);
}
