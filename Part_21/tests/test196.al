type FOO = int32 range 0 ... 100;

FOO fred(void) {
  return(1000);
}

public void main(void) {
  int32 y= fred();
}
