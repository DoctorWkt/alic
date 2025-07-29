#include <stdio.ah>
#include <except.ah>

type footype= funcptr int32(int8, bool) throws Exception *;

void fred(void) { printf("hi\n"); }

int32 mary(int8 a, bool b) throws Exception *e {
  if (a != 0) {
    e.errnum= 4;
    abort;
  }
  return(5);
}

public void main(void ) {
  footype jim;
  footype bar;
  Exception E;

  jim= mary;
  bar= jim;

  try(E) { mary(3, true); }
  catch  { printf("mary() failed\n"); }
  try(E) { jim(3, true); }
  catch  { printf("funcptr jim failed\n"); }
  try(E) { bar(3, true); }
  catch  { printf("funcptr bar failed\n"); }
}
