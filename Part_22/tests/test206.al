#include <stdio.ah>

string x = "Hello there";

public void main(void) {
  int8 ch;
  int8 *hidptr;

  if (x != NULL) {
    for (hidptr= x; *hidptr != 0; hidptr++) {
      ch= *hidptr;
      printf("%c\n", ch);
    }
  }

  foreach ch (x) { printf("%c\n", ch); }
}
