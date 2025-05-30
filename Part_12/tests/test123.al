#include <stdio.ah>

type BAR = struct {
  bool boo,
  int32 i32,
  int16 i16,
  flt32 f32
};

type FOO = struct {
  int32 x,
  BAR   bar,
  int16 y[4],
  int32 z
};

FOO fred = {
	10,
	{ true, 32, 88, 3.14 },
	{ 3, 4, 5, 6 },
	100
};

void main(void) {
  int32 i;

  printf("fred.x is %d\n", fred.x);
  printf("fred.bar.boo is %d\n", fred.bar.boo);
  printf("fred.bar.i32 is %d\n", fred.bar.i32);
  printf("fred.bar.i16 is %d\n", fred.bar.i16);
  printf("fred.bar.f32 is %f\n", fred.bar.f32);
  for (i=0; i < 4; i++) printf("fred.y[%d] is %d\n", i, fred.y[i]);
  printf("fred.z is %d\n", fred.z);

}
