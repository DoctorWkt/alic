#include <stdio.ah>

type FOO= struct {
  int32 a,
  int32 b[6],
  int8  c,
  flt32 d
};


int32 mary[5]= { 3, 1, 4, 1, 5 };

public void main(void) {
  int32 fred[5];
  int32 i;
  FOO bar;

  fred[3]= 2; printf("fred: ");
  for (i=0; i < sizeof(fred); i++)
    printf("%d ", fred[i]);
  printf("\nmary: ");
  for (i=0; i < sizeof(mary); i++)
    printf("%d ", mary[i]);
  printf("\n");

  printf("sizeof FOO is %d\n", sizeof(FOO));
  bar.a= 12;
  bar.c= 'E';
  bar.d= 3.14;
  printf("%d %c %f\n", bar.a, bar.c, bar.d);
  
  bar.b[3]= 100; printf("bar.b[]: ");
  for (i=0; i < 6; i++) printf("%d ", bar.b[i]);
  printf("\n");
}
