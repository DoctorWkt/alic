type FILE;		// An opaque type
type char= int8 *;	// A type alias
void printf(...);

void main(void) {
  char y= 64;
  FILE *fred = NULL;
  int8 x= 32;
  printf("Hello world %d %d\n", x, y);
  x = x + y;
  printf("x is now %d\n", x);
}
