#include <stdio.ah>
#include <stdlib.ah>

type FOO = struct {
  int8 *word,
  int32 num,
  FOO *next
};

public void main(void) {
  FOO *head;
  FOO *this;
  FOO *that;
  FOO *other;

  // Build a linked list of nodes
  head= malloc(sizeof(FOO));
  this= malloc(sizeof(FOO));
  that= malloc(sizeof(FOO));
  other= malloc(sizeof(FOO));
  head.word= "pear"; head.num= 15;
  this.word= "apple"; this.num= 17;
  that.word= "banana"; that.num= 19;
  other.word= "orange"; other.num= 32;

  head.next= this; this.next= that;
  that.next= other; other.next= NULL;

  // Walk the list but skip 19
  for (this= head; this != NULL; this= this.next) {
    if (this.num == 19) continue;
    printf("%s\n", this.word);
  }

  // Do the same with foreach
  foreach this (head, this.next) {
    if (this.num == 19) continue;
    printf("%s\n", this.word);
  }
}
