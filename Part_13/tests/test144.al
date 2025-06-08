#include <stdio.ah>
#include <string.ah>

public void main(void) {
  char *a= "hello";
  char *b= "hello";

  if (!strcmp(a,b))
    printf("The strings match\n");
}
