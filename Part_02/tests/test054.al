#include <stdio.ah>
#include <unistd.ah>

public void main(void) {
  FILE *outfh= NULL;

  outfh= fopen("fred", "w");
  if (outfh == NULL) {
    printf("Unable to open fred\n");
    return;
  }

  printf("We opened fred!\n");
  fwrite("Does this work?\n", 16, 1, outfh);	// It does!
  fclose(outfh);
  unlink("fred");
}
