#include <stdio.ah>

int32 fred[45];

int16 mary;

public void main(void) {
  printf("%d\n", fred[10]);
  fred[10]= 23;
  printf("%d\n", fred[10]);

  printf("mary %d\n", mary);
  mary= -2;
  printf("mary %d\n", mary);
}
