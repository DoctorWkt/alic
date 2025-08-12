#include <stdio.ah>
#include <stdlib.ah>

int32 *intlist[3]=  { NULL, NULL, NULL };
int32 mary= 23;
int32 *ipointer;

flt32 *fltlist[3]=  { NULL, NULL, NULL };
flt32 jim= 23.0;
flt32 *fpointer;

public void main(void) {
  ipointer= &mary; intlist[0]= &mary;
  printf("%d\n", mary);
  printf("%d\n", *ipointer); printf("%d\n", *intlist[0]);

  fpointer= &jim;
  fltlist[0]= &jim;
  printf("%f\n", jim);
  printf("%f\n", *fpointer); printf("%f\n", *fltlist[0]);
}
