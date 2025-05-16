void printf(...);

void main(void) {
  int32 a= 12;
  int32 b= 10;
  bool  c= false;
  bool  d= a+b < a-b;

  c= a+b > a-b;
  printf("%d\n", c);
  printf("%d\n", d);
}
