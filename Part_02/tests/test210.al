#include <stdio.ah>

type FOO;

int fred[5][4][3];

public void main(void) {
  int x;

  printf("We have int fred[5][4][3];\n");
  x= sizeof(fred);          printf("sizeof(fred)          is %d\n", x);
  x= sizeof(fred[1]);       printf("sizeof(fred[1])       is %d\n", x);
  x= sizeof(fred[1][1]);    printf("sizeof(fred[1][1])    is  %d\n", x);
  x= sizeof(fred[1][1][1]); printf("sizeof(fred[1][1][1]) is  %d\n", x);
}
