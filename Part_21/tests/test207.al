#include <stdio.ah>
#include <stdlib.ah>

string x = "Hello there";
int32 list[5]= { 1, 2, 3, 4, 7 };

type FOO = struct {
  int32 a,
  FOO *next
};

// Given a number, return a pointer to a contiguous
// list of pointers to factors of that number, with
// NULL the last pointer in the list. Return NULL
// if the number is zero.
uint32 ** factors(uint32 num) {
  // Why 2? 1 is a factor, and we put NULL on the end
  int32 count = 2;
  uint32 trial;
  uint32 **flist;

  // 0 has no factors
  if (num == 0) return(NULL);

  // Check the numbers from 2 up to num
  for (trial = 2; trial <= num; trial++)
    if ((num % trial) == 0)
      count++;

  // Allocate that many elements, ensure last is NULL
  flist = calloc(count, sizeof(uint32 *));

  // 1 is always a factor
  flist[0] = malloc(sizeof(uint32));
  *(flist[0]) = 1;

  // Add the other factors to the list
  for ({ trial = 2; count = 1;}; trial <= num; trial++)
    if ((num % trial) == 0) {
      flist[count] = malloc(sizeof(uint32));
      *(flist[count]) = trial;
      count++;
    }

  // Return the pointer to the list of factor pointers
  return (flist);
}

public void main(void) {
  int8 ch;
  int8 *hidptr;
  int32 elem;
  uint32 f;
  FOO *head;
  FOO *foo1;
  FOO *foo2;
  FOO *foo3;
  FOO *foo4;
  FOO *this;

  // Build a FOO list
  head= malloc(sizeof(FOO));
  foo1= malloc(sizeof(FOO));
  foo2= malloc(sizeof(FOO));
  foo3= malloc(sizeof(FOO));
  foo4= malloc(sizeof(FOO));
  head.a= 5; head.next= foo1;
  foo1.a= 6; foo1.next= foo2;
  foo2.a= 7; foo2.next= foo3;
  foo3.a= 8; foo3.next= foo4;
  foo4.a= 9; foo4.next= NULL;
  
  // break, continue work here
  foreach elem (list) {
    if (elem == 4) break;
    if (elem == 2) continue;
    printf("%d\n", elem);
  }
  printf("\n");

  // break, continue work here
  foreach elem (1 ... 10) {
    if (elem == 6) break;
    if (elem == 3) continue;
    printf("%d\n", elem);
  }
  printf("\n");

  // break, continue work here
  foreach this (head, this.next) {
    if (this.a == 6) continue;
    if (this.a == 9) continue;
    printf("%d\n", this.a);
  }
  printf("\n");

  // break, continue work here
  foreach ch (x) {
    if (ch == 'l') continue;
    if (ch == 'r') break;
    printf("%c\n", ch);
  }
  printf("\n");

  // break, continue work here
  foreach f (factors(60)) {
    if (f == 3) continue;
    if (f == 15) break;
    printf("%2d is a factor of 60\n", f);
  }
}
