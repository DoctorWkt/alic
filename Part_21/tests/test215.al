#include <stdio.ah>


type FOO= struct {
  int32 a,
  bool  b,
  flt32 c
};


public void main(void) {
  int32 i;
  int32 mary[5] = { 3, 1, 4, 1, 5 };
  FOO dave= { 13, true, 23.5 };
  FOO fred[3]= {
	{ 1, true,  1.2 },
	{ 2, false, 4.5 },
	{ 3, true,  6.7 }
  };

  printf("sizeof(int32) is %d\n", sizeof(int32));
  printf("sizeof(bool) is %d\n", sizeof(bool));
  printf("sizeof(flt32) is %d\n", sizeof(flt32));

  printf("dave.a is %d\n", dave.a);
  printf("dave.b is %d\n", dave.b);
  printf("dave.c is %f\n", dave.c);

  for (i=0; i < 5; i++) printf("mary[%d] is %d\n", i, mary[i]);

  for (i=0; i < 3; i++) {
    printf("fred[%d].a is %d\n", i, fred[i].a);
    printf("fred[%d].b is %d\n", i, fred[i].b);
    printf("fred[%d].c is %f\n", i, fred[i].c);
  }
}
