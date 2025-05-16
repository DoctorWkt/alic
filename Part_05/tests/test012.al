void main(void) {
  bool fred = true;
  bool jim = false;
  bool mary = 17 != 14;
  int32 x = mary;
  int32 y = false;
  bool dave = fred;

  printf("%d\n",fred);
  printf("%d\n",jim);
  jim = 3 > 2;
  printf("%d\n",jim);
  printf("%d\n",mary);
  printf("%d\n",x);
  printf("%d\n",y);
  printf("%d\n",dave);
}
