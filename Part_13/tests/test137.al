#include <stdio.ah>

// AST node types
enum {
  A_ASSIGN = 1, A_CAST,
  A_ADD, A_SUBTRACT, A_MULTIPLY, A_DIVIDE, A_NEGATE,
  A_EQ, A_NE, A_LT, A_GT, A_LE, A_GE, A_NOT
};

public void main(void) {
  int32 x= A_ADD;

  switch(x) {
  case A_ADD:
    printf("It's an add\n");
  default:
    printf("It's not an add\n");
  }
}
