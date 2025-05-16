void printf(...);

void main(void) {
  int8  a= -12;
  int16 b= 0;
  int32 c= 0;
  int64 d= 0;
  flt32 f= 3.1415;
  flt64 g= 0;
  uint8  w= 100;
  uint16 x= 0;
  uint32 y= 0;
  uint64 z= 0;

  printf("%d\n",a);
  b= a; printf("%d\n",b);
  c= b; printf("%d\n",c);
  d= c; printf("%d\n",d);
  printf("%f\n",f); g= f; printf("%f\n", g);
  printf("%d\n",w);
  x= w; printf("%d\n",x);
  y= x; printf("%d\n",y);
  z= y; printf("%d\n",z);
  
  f= a; printf("%f\n",f);
  f= b; printf("%f\n",f);
  f= c; printf("%f\n",f);
  f= d; printf("%f\n",f);
  f= w; printf("%f\n",f);
  f= x; printf("%f\n",f);
  f= y; printf("%f\n",f);
  f= z; printf("%f\n",f);
  
  g= a; printf("%f\n",g);
  g= b; printf("%f\n",g);
  g= c; printf("%f\n",g);
  g= d; printf("%f\n",g);
  g= w; printf("%f\n",g);
  g= x; printf("%f\n",g);
  g= y; printf("%f\n",g);
  g= z; printf("%f\n",g);
}
