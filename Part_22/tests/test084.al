#include <stdio.ah>
#include <stdlib.ah>

int32 *ptr;
int32 x;

public void main(void) {

  // Get some memory
  ptr= malloc(1000);

  // Try to set an element's value
  ptr[0]= 3000; x= ptr[0]; printf("Element 0 is %d\n", x);
  ptr[5]= 5432; x= ptr[5]; printf("Element 5 is %d\n", x);
  printf("Element 0 is %d\n", ptr[0]);
  printf("Element 5 is %d\n", ptr[5]);
}
