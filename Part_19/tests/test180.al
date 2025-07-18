#include <stdio.ah>
#include <stdlib.ah>

// Given a number, return a pointer to a contiguous
// list of pointers to factors of that number, with
// NULL the last pointer in the list. Return NULL
// if the number is zero.
uint32 ** factors(uint32 num) {
  // Why 2? 1 is a factor, and we put NULL on the end
  int32 count = 2;
  uint32 trial;
  uint32 **list;

  // 0 has no factors
  if (num == 0) return(NULL);

  // Check the numbers from 2 up to num
  for (trial = 2; trial <= num; trial++)
    if ((num % trial) == 0)
      count++;

  // Allocate that many elements, ensure last is NULL
  list = calloc(count, sizeof(uint32 *));

  // 1 is always a factor
  list[0] = malloc(sizeof(uint32));
  *(list[0]) = 1;

  // Add the other factors to the list
  for ({ trial = 2; count = 1;}; trial <= num; trial++)
    if ((num % trial) == 0) {
      list[count] = malloc(sizeof(uint32));
      *(list[count]) = trial;
      count++;
    }

  // Return the pointer to the list of factor pointers
  return (list);
}

public void main(void) {
  uint32 **list;
  uint32 **element;
  uint32 x;
  uint32 num= 60;

  // Walk the list of pointers
  list= factors(60);
  if (list != NULL) {
    for (element = list; *element != NULL; element++) {
      // Get the value at that pointer
      x = **element;
      printf("%2d is a factor of %d\n", x, num);
      // Free the pointer's value
      free(*element);
    }
    // Free the list
    free(list);
  }
}
