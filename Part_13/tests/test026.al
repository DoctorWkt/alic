#include <stdio.ah>

public void main(void) {
  int32 i = 0;

  for (i = 1; i <= 10; i = i + 1) {
    printf("Counting from one to ten: %d\n",i);
  }
}

void fred(int64 x) {
  printf("This function doesn't get called yet\n", 5);
}
