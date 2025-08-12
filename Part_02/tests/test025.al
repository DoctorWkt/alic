#include <stdio.ah>

public void main(void) {
  int8   a = 0;
  uint8  b = 0;
  int16  c = 0;
  uint16 d = 0;

  printf("%d\n",a);
  for (a= 126;   a != -126;   a = a + 1) { printf("%d\n",a); }
  for (b= 254;   b != 2;      b = b + 1) { printf("%d\n",b); }
  for (c= 32766; c != -32766; c = c + 1) { printf("%d\n",c); }
  for (d= 65532; d != 2;      d = d + 1) { printf("%d\n",d); }
}
