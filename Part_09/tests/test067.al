#include <stdio.h>
#include <stdlib.h>
int8 *list;

void main(void) {
  printf("Hello world\n");
  list= malloc(23);
  *list= 'x';
  printf("%c\n", *list);
  free(list);
}
