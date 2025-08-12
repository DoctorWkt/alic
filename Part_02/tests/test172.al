#include <stdio.ah>
#include <stdlib.ah>

int32 fred[5]= { 50, 40, 30, 20, 10 };

type FOO = struct {
  int32 value,
  FOO *next
};

FOO *Head;

public void main(void) {
  int32 i=3;
  int32 idx;
  FOO *this;
  FOO *that;

  // Build a linked list
  Head= malloc(sizeof(FOO)); Head.value= 3;
  this= malloc(sizeof(FOO)); this.value= 4;
  that= malloc(sizeof(FOO)); that.value= 5;
  Head.next= this; this.next= that; that.next= NULL;

  // Iterate over a range
  for (i= 10; i <= 15; i++) printf("%d\n", i);
  foreach i (10 ... 15)     printf("%d\n", i);

  // Walk a linked list
  for (this= Head; this != NULL; this= this.next) printf("%d\n", this.value);
  foreach this (Head, this.next)                  printf("%d\n", this.value);

  // Iterate over elements in an array
  for (idx= 0; idx < sizeof(fred); idx++) printf("%d\n", fred[idx]);
  foreach i (fred)                        printf("%d\n", i);
}
