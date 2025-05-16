void printf(...);

void main(void) {
  int64 fred = 34;
  fred= 100;
  printf("%d\n", fred + 2);
  fred= fred + 100;
  printf("%d\n", fred);
}
