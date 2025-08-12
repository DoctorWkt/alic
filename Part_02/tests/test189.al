#include <stdio.ah>
#include <string.ah>

public void main(void) {
  uint64 x;
  uint64 *ptr1;
  uint64 *ptr2;

  ptr1= &x;
  ptr2= &x;

  // Move ptr2 up a few places
  ptr2++; ptr2++; ptr2++; ptr2++; ptr2++; ptr2++;

  x= ptr2 - ptr1;
  printf("x is %d\n", x);
}
