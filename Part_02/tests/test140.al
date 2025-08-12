#include <stdio.ah>

type FOO= struct {
  int32 x,
  int32 y
};

public void main(void) {
  FOO fred;
  int32 a;
  int32 *ptr;

  fred.x= 3; fred.y= 4;
  a= 100;
  ptr= &a;        printf("ptr points at %d\n", *ptr);
  ptr= &(fred.y); printf("ptr points at %d\n", *ptr);
}
