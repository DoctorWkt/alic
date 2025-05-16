void fred(int32 x) {
  printf("fred has argument x=%d\n", x);
}

void main(void) {
  int32 a=3;
  int32 b=4;
  printf("main %d\n", a+b);
  fred(a+b);
}
