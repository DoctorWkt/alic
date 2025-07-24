#include <stdio.ah>
#include <stdlib.ah>
#include <regex.ah>

public void main(void) {
  int8 *str;

  int8 *src=     "This is a string with a date: 12/25/2019";
  int8 *regex=   "([0-9]+)/([0-9]+)/([0-9]+)";
  int8 *replace= "$2/$1/$3";

  foreach str (grep(src, regex))
    printf("%s\n", str);

  str = sed(src, regex, replace);
  if (str != NULL)
    printf("Replaced with %s\n", str);
  else
    printf("No replacement\n");
}
