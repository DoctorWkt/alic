#include <stdio.ah>

int32 fred(int32 a, int32 b, int32 c) {
  int32 result = a * 2 + b * 3 + c;
  return(result);
}

public void main(void) {
  int32 result= 0;
  result= fred(10, 20, 30);
  printf("result is %d\n", result);
  result= fred(c= 30, a= 10, b= 20);
  printf("result is %d\n", result);
  result= fred(b= 20, c= 30, a= 10);
  printf("result is %d\n", result);
}
