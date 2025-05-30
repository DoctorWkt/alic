#include <stdio.ah>
#include <stdlib.ah>

// A linked list of integers
type Intlist= struct {
  int32 val,
  Intlist *next
};

void main(void) {
  Intlist *head= NULL;
  Intlist *this;

  // Build three nodes and make a list.
  // We don't have sizeof() yet, so guess.
  this= malloc(16); this.val= 5; head= this;
  this= malloc(16); this.val= 4; this.next= head; head= this;
  this= malloc(16); this.val= 3; this.next= head; head= this;

  printf("Head has value %d\n", head.val);
  printf("Next has value %d\n", head.next.val);
  printf("Last has value %d\n", head.next.next.val);

  // See if we can assign to it
  head.next.next.val= 20;
  printf("Last has new value %d\n", head.next.next.val);
}
