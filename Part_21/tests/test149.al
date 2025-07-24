#include <stdio.ah>

const int32 x = 5;

type FOO = struct {
  int32 a,
  int16 b,
  flt32 c
};

const FOO fred = { 1, 2, 3.14 };

type BAR = struct {
  int32 a,
  const int16 b,
  flt32 c
};

BAR mary = { 1, 2, 3.14 };

char *name= "Fred Bloggs";
char *name2= const "Fred Bloggs";
const char *name3= const "Fred Bloggs";

public void main(void) {
  const flt32 y = 14.3 * 3;
  BAR jane;
  const FOO dave;

  printf("Looks like we can parse const!\n");
  printf("%d %f\n", x, y);
  printf("%d %d %f\n", fred.a, fred.b, fred.c);
  printf("%d %d %f\n", mary.a, mary.b, mary.c);
}
