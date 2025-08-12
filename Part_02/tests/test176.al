#include <stdio.ah>

public void main(void) {
  int8 *state= "NSW";

  switch(state) {
    case "ACT": printf("Canberra\n");
    case "QLD": printf("Brisbane\n");
    case "NSW": printf("Sydney\n");
    default:    printf("Somewhere else\n");
  }
}
