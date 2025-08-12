#include <stdio.ah>

type FOO = struct {
  int32 a,
  int32 b,
  int32 c,
  int32 d
};

type BAR = struct {
  bool flag
};

public void main(void) {

  FOO fred;
  FOO jim;

  fred.a= 25; fred.b= 15; fred.c= 35; fred.d= 28;
  printf("fred is %d %d %d %d\n",
	fred.a, fred.b, fred.c, fred.d);
  jim= fred;
  printf("jim  is %d %d %d %d\n",
	jim.a, jim.b, jim.c, jim.d);
}
