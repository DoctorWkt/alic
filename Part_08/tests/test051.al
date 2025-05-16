void printf(...);

void main(void) {
  flt32 fred= 3.14;
  flt32 mary= 1.0;
  flt32 *jim= NULL;

  jim= &fred;
  mary= *jim;
  printf("fred is %f\n", fred);
  printf("mary is %f\n", mary);
}
