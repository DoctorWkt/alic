void printf(...);

void fred(int32 x) {
  printf("fred x is %d\n", x);
  return;
  printf("We should not get here!\n", 0);
}

int32 mary(int32 x) {
  x = x + 3;
  printf("In mary, about to return %d\n", x);
  return(x);
  printf("We should not get here!\n", 0);
}

void main(void) {
  int32 result= 0;

  fred(5);
  result= mary(10);
  printf("main got the result %d\n", result);
}
