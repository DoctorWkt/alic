#include <stdio.ah>
#include <stdlib.ah>
#include <string.ah>

public void main(void) {
  char *fred= "hello there";
  char *mary;

  printf("fred says %s\n", fred);
  mary= strdup(fred);
  printf("mary says %s\n", mary);

  // Make mary bigger
  mary= realloc(mary, 100);

  mary[11]= '!';
  mary[12]= 0;
  printf("mary says %s\n", mary);
}
