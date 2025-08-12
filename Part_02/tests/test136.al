#include <stdio.ah>

type BAR = struct {
  int32 a,
  int32 b
};

type FOO = struct {
  int32 x,
  int8 *y,
  BAR *barptr
};

public void main(void) {
  FOO fred;
  BAR jim;
  
  fred.x= 23;
  fred.y= "hello";
  printf("x is %d\n", fred.x);
  fred.barptr= NULL; printf("ptr is %p\n", fred.barptr);
}
