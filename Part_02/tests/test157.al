#include <stdio.ah>

type FOO = struct {
  int32 val,
  FOO *next
};

const FOO jim=  { 2, NULL };
FOO fred= { 1, NULL };


public void main(void) {
  fred.next = &jim;
  fred.val = 5;
  fred = const;
  fred.val = 6;
}
