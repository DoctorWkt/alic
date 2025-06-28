#include <stdio.ah>

enum {
  TY_INT8, TY_INT16, TY_INT32, TY_INT64, TY_FLT32, TY_FLT64,
  TY_VOID, TY_BOOL, TY_USER, TY_STRUCT
};

public void main(void) {
  int32 x= 5;

  x= x + TY_BOOL + 1;
  printf("x is %d\n", x);
}
