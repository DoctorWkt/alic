#include <stdio.ah>

public void main(void) {
  int32 x;

  // A FOR loop with no intialisation
  x= 1;
  for( ; x < 5; x= x + 1) {
    printf("1st loop: %d\n", x);
  }

  // A FOR loop with no condition, thus true
  for (x= 10; ; x= x + 1) {
    printf("2nd loop: %d\n", x);
    if (x > 16) { break; }
  }

  // A FOR loop with no change code
  for (x= 30; x < 39; ) {
    printf("3rd loop: %d\n", x);
    x= x + 2;
  }

  // Loop forever
  for (;;) {
    printf("4th loop: %d\n", x);
    if (x > 45) { break; }
    x= x + 1;
  }
}
