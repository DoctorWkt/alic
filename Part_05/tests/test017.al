void main(void) {
  int32 a= 0;
  int32 b= 0;
  a = 1 << 4;
  printf("%d\n", a);
  a = 64 >> 3;
  printf("%d\n", a);
  a = 64; b = 3;
  a = a >> b;
  printf("%d\n", a);
}
