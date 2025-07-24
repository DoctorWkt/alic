#include <stdio.ah>

public void main(void) {
  int32 mary = 45;
  int32 fred[3]= {5, 2, 3};
  int32 dave[3]= { fred[2] * mary, fred[1] + mary, mary / fred[0] };

  printf("mary is %d\n", mary);
  printf("We have three fred numbers %d %d %d\n",
	fred[0], fred[1], fred[2]);
  printf("We have three dave numbers %d %d %d\n",
	dave[0], dave[1], dave[2]);
}
