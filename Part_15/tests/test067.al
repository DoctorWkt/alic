#include <stdio.ah>
#include <stdlib.ah>
int8 *list;

public void main(void) {
  printf("Hello world\n");
  list= malloc(23);
  *list= 'x';
  printf("%c\n", *list);
  free(list);
}
