#include <stdio.h>
#include <unistd.h>

void main(void) {
  FILE *outfh= NULL;
  FILE *infh= NULL;
  char *buf= "                       ";

  outfh= fopen("fred", "w");
  if (outfh == NULL) {
    printf("Unable to open fred\n");
    return;
  }

  printf("We opened fred!\n");
  fwrite("Does this work?\n", 16, 1, outfh);	// It does!
  fclose(outfh);

  infh= fopen("fred", "r");
  if (infh == NULL) {
    printf("Unable to read fred\n");
    return;
  }

  fgets(buf, 20, infh);
  printf(buf);
  fclose(infh);
  unlink("fred");
}
