#include <stdio.ah>
#include <except.ah>

// Compute and return the greatest common factor between num1 and num2.
// Throw an exception if any input is zero or negative.
int32 gcf(int32 num1, int32 num2) throws Exception *e {
  int32 temp;

  // The language doesn't have () or || yet
  if (num1 <= 0) { e.errno= EDOM; abort; }
  if (num2 <= 0) { e.errno= EDOM; abort; }

  while(num1 != num2) {
    if (num1 > num2) {
      num1 = num1 - num2;
    } else {
      num2 = num2 - num1;
    }
  }

  return(num1);
}

// Print out a nice table of results
void main(void) {
  int32 x;
  int32 y;
  int32 result;
  Exception e;

  // Print out the header
  printf("    |");
  for (y=1; y <= 18; y= y + 1) { printf(" %2d ", y); }
  printf("\n");
  printf("----+");
  for (y=1; y <= 18; y= y + 1) { printf("----"); }
  printf("\n");

  for (x=1; x <= 18; x= x + 1) {
    printf(" %2d |", x);
    for (y=1; y <= 18; y= y + 1) {
      try(e) {
        result= gcf(x, y);
        printf(" %2d ", result);
      }
      catch { printf("GCF failure on inputs %d and %d\n", x, y); }
    }
    printf("\n");
  }

  printf("\n");
  try(e) { result= gcf(0, 3); }
  catch { printf("GCF failure on inputs 0 and 3\n"); }
}
