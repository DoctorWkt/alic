#include <stdio.ah>
#include <stdlib.ah>

type FOO = struct {
  int8 *fullname,
  int8 age
};

public void main(void) {
  FOO* list[char *];
  FOO*  entry;

  // Make some entries. Add each to the list
  entry= malloc(sizeof(FOO));
  entry.fullname= "Fred Bloggs";
  entry.age= 66;
  list["Fred"]= entry;

  entry= malloc(sizeof(FOO));
  entry.fullname= "Jane Eyre";
  entry.age= 21;
  list["Jane"]= entry;

  entry= malloc(sizeof(FOO));
  entry.fullname= "M.C. Escher";
  entry.age= 55;
  list["M.C"]= entry;

  // Get one back
  entry= list["Fred"];
  printf("%d %s\n", entry.age, entry.fullname);
}
