#include <stdio.ah>

type Litval= struct {
  union {
    int64  intval,		// Signed integer
    uint64 uintval,		// Unsigned integer
    flt64  dblval		// Floating point
  },
  int64 numtype			// Type of number
};

public void main(void) {
  Litval x;
  flt64  fred;

  x.intval= 23;   printf("intval is %d\n", x.intval);
  x.dblval= 23.5; printf("dblval is %f\n", x.dblval);
  fred= x.dblval; printf("fred is %f\n", fred);
}
