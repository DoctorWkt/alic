#include <stdio.ah>

type FOO= int32;

type BAR= struct {
  int32 x,
  int8  y,
  int64 z
};

void main(void) {
  printf("size of  int8 is  %d\n", sizeof(int8));
  printf("size of  int16 is %d\n", sizeof(int16));
  printf("size of  int32 is %d\n", sizeof(int32));
  printf("size of  int64 is %d\n", sizeof(int64));
  printf("size of  uint8 is %d\n", sizeof(uint8));
  printf("size of uint16 is %d\n", sizeof(uint16));
  printf("size of uint32 is %d\n", sizeof(uint32));
  printf("size of uint64 is %d\n", sizeof(uint64));
  printf("size of  flt32 is %d\n", sizeof(flt32));
  printf("size of  flt64 is %d\n", sizeof(flt64));
  printf("size of    FOO is %d\n", sizeof(FOO));
  printf("size of    BAR is %d\n", sizeof(BAR));
  printf("size of   bool is %d\n", sizeof(bool));
  printf("size of int8 * is %d\n", sizeof(int8 *));
}
