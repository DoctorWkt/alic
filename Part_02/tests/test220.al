#include <stdio.ah>

type FOO = struct {
  int32 a,
  flt32 b,
  bool  c,
  int16 d[3]
};

type BAR = struct {
  bool x,
  FOO  y[2]
};

BAR thing = { true, { 1, 2.0, true, 4, 5, 6 }, { 1, 2.0, true, 4, 5, 6 } };

FOO fred  = { 1, 2.0, true,   4, 5, 6   };
FOO fred2 = { 1, 2.0, true, { 4, 5, 6 } };

FOO mary[3] = {
  { 1, 2.0, true, 4, 5, 6 },
  { 1, 2.0, true, 4, 5, 6 },
  { 1, 2.0, true, 4, 5, 6 }  
};

FOO dave[2] = {
  { 1, 2.0, true, { 4, 5, 6 } },
  { 1, 2.0, true, { 4, 5, 6 } }
};

public void main(void) {
  printf("hi\n");
}
