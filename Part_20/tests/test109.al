#include <stdio.ah>
#include <stdlib.ah>

int32  fred[8]=     { 2, 3, 0, 0, 0, 0, 0, 5 };
bool   kim[4]=      { true, false, true, false };
FILE  *flist[4]=    { NULL, NULL, NULL, NULL };
flt32 *fltlist[3]=  { NULL, NULL, NULL };
char   charlist[6]= { 'a', 'b', 'c', 'd', 'e', 'f' };

int32 dave;
flt32 mary= 23.5;
int32 bill= 33;

public void main(void) {
  int32 i;

  printf("dave is %d, mary is %f, bill is %d\n", dave, mary, bill);
  printf("fred[1] is %d\n", fred[1]);

  if (kim[2] == true) printf("kim[2] is true\n");
  else printf("kim[2] is false\n");

  if (flist[2] == NULL) printf("flist[2] is NULL\n");
  else printf("flist[2] is not NULL\n");

  printf("%c%c\n", charlist[1], charlist[4]);
  for (i=0; i < 6; i++) printf("%c", charlist[i]); printf("\n");

  fred[1]= -5; printf("fred[1] is %d\n", fred[1]);
  kim[3]= false;
  // fltlist[0] = &mary;
  charlist[0]= 'A'; charlist[1]= 'B'; charlist[2]= 'C'; charlist[3]= 'D';
  for (i=0; i < 6; i++) printf("%c", charlist[i]); printf("\n");
}
