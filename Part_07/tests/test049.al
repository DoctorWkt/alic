void printf(...);

void main(void) {
  int8 fred= 5;
  int8 mary= 0;
  int8 *jim= NULL;

  jim= &fred;
  mary= *jim;
  printf("fred is %d\n", fred);
  printf("mary is %d\n", mary);
}
