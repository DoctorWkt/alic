// String literals list for the alic compiler
// (c) 2025 Warren Toomey, GPL3

#include "alic.h"
#include "proto.h"

static Strlit *Strhead = NULL;	// Linked list of literals

// Add a new string literal to the list
// and return its label number
int add_strlit(char *name) {
  Strlit *this;

  // If it already exists, don't add it
  for (this = Strhead; this != NULL; this = this->next)
    if (!strcmp(this->val, name))
      return(this->label);

  // Make a new Strlit node and link it to the list
  this = (Strlit *) malloc(sizeof(Strlit));
  if (this == NULL)
    fatal("out of memory in add_strlit()\n");

  this->val = strdup(name);
  this->label = genlabel();
  this->next = Strhead;
  Strhead = this;
  return (this->label);
}

// Generate code for all string literals
void gen_strlits(void) {
  Strlit *this;

  for (this = Strhead; this != NULL; this = this->next)
    cgstrlit(this->label, this->val);
}
