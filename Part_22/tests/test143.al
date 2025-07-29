#include <stdio.ah>

type Litval= struct {
  union {
    int64 intval,               // Signed integer
    uint64 uintval,             // Unsigned integer
    flt64 dblval                // Floating point
  },
  int numtype                   // The type of numerical value
};

type FOO = struct {
  int32 a,
  Litval x,
  int32 b
};

public void main(void) {
  FOO fred;
  int32 *ptr;
  Litval *lptr;

  // fred.a= 34;
  // fred.b= 63;
  fred.x.dblval= 3.13;

  ptr= &(fred.a);
  lptr= &(fred.x);
  printf("lptr points at %f\n", lptr.dblval);
}
