#include <stdio.ah>

// Check we can do a proto with inout
public void fred(inout int32 x, int32 y);
public void fred(inout int32 x, int32 y) { x= x + y; }

public void main(void) { printf("hi\n"); }
