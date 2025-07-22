#include <stdio.ah>

public void main(void) {
  // An associative array keyed by strings
  int32 list[int8 *];
  int32 num;

  // An associative array keyed by int16s
  int8 *place[int16];
  int8 *str;

  // An associatve array keyed by int8s
  int8 fred[int8];
  int8 age;

  // Add some key/value pairs to the list array
  list["foo"]= 3;
  list["bar"]= 100;
  list["jim"]= 56;
  list["xyz"]= -23;

  // Ditto for the place array
  place[3]= "Athens";
  place[7]= "Egypt";
  place[99]= "Paris";
  place[4000]= "Auckland";
  place[408]= "Oslo";

  // Put some ages into the fred list
  fred[3]=  25;
  fred[12]= 38;
  fred[19]= 66;
  fred[21]= 12;
  fred[0]=  99;
  fred[45]= 35;
  fred[13]= 40;

  foreach num (list) printf("num is %d\n", num);
  foreach str (place) printf("A city is %s\n", str);
  foreach age (fred) printf("age is %d\n", age);
}
