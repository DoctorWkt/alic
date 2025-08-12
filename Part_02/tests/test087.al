#include <stdio.ah>

public void main(void) {
  int32 x;

  for (x=1; x <= 9; x= x + 1) {
    switch(x) {
      case  3: printf("case 3\n");
      case  5: 
      case  6: printf("case %d, fallthru to ...\n",x);
	       fallthru;
      case  7: printf("case 7\n");
      default: printf("case %d, default\n", x);
    }
  }
}
