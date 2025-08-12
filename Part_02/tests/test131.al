#include <stdio.ah>
#include <stdlib.ah>

public void main(void) {
  uint64 cnt=3;
  int8 *cptr= "Hello there\n";
  int32 *list= malloc(10 * sizeof(int32));
  int32 *lptr= list;

  printf(cptr);
  cptr = cptr + cnt;
  printf(cptr);

  list[0]= 3; list[1]= 2; list[2] = 8; list[3]= 10;
  list[4]= 9; list[5]= 1; list[6] = 5; list[7]= 20;

  printf("%d\n", *lptr);
  lptr= lptr + cnt;
  printf("%d\n", *lptr);
}
