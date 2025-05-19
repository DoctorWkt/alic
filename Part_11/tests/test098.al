#include <stdio.ah>

void main(void) {
  int32 x;

  for (x=1; x <= 9; x= x + 1) {
    switch(x) {
      case  3: printf("case 3\n");
      case  4:
      case  5: 
      case  6: printf("case %d\n",x);
	       if (x < 6) {
      	         printf("fallthru to ...\n");
	         fallthru;
	       }
               printf("case 6 does not fall through!\n");
      case  7: printf("case 7\n");
      default: printf("case %d, default\n", x);
    }
  }
}
