#include <stdio.ah>

int32 fred[5]= { 3, 1, 4, 1, 5 };
int64 jim;

public void main(void) {
  int32 i;

  printf("bool has size %d\n", sizeof(bool));
  printf("i    has size %d\n", sizeof(i));
  printf("jim  has size %d\n", sizeof(jim));
  printf("fred has %d elements\n", sizeof(fred));

  for (i=0; i < sizeof(fred); i++)
    printf("fred[%d] is %d\n", i, fred[i]);

}
