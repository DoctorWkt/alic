#include <stdio.ah>
void * foo;
type fred= funcptr int32(int32, bool, void *);
type abcd= funcptr int32(const int32, inout bool, void *, ...);

// Two global function pointers
fred ptr1;
abcd ptr2;

// A function that receives a function pointer and calls it
void mysignal(fred aaa) {
  printf("In mysignal(), about to call via aaa\n");
  aaa(100, false, NULL);
}

// A function which matches the fred type
int32 func1(int32 a, bool b, void *c) {
  printf("a is %d b is %d, c is %p\n", a, b, c);
  return(a);
}

public void main(void) {
  fred localptr;

  // Set a function pointer to a function
  ptr1= func1;

  // Copy a function pointer
  localptr= ptr1;

  // Call via a function, then a function pointer
  mysignal(func1);
  mysignal(ptr1);

  // Call via a function, then a function pointer
  func1(23, true, NULL);
  ptr1(33, false, NULL);
  localptr(44, true, NULL);
}
