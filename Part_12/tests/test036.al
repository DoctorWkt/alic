void printf(...);

void fred(int32 a, int32 b, flt32 c) {
  printf("fred has a value %d\n", a);
  printf("fred has b value %d\n", b);
  printf("fred has c value %f\n", c);
}

void main(void) {
  fred(10, 20, 30.0);
  fred(c= 30.5, a= 11, b= 19);
  fred(c= 30.5, a= 11, a= 19);
}
