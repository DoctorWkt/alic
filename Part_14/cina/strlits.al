// String literals list for the alic compiler
// (c) 2025 Warren Toomey, GPL3

#include "alic.ah"
#include "proto.ah"

Strlit *Strhead = NULL;	// Linked list of literals

// Add a new string literal to the list
// and return its label number
public int add_strlit(const char *name, const bool is_const) {
  Strlit *this;

  // If it already exists, don't add it
  for (this = Strhead; this != NULL; this = this.next)
    if ((strcmp(this.val, name)==0) && (this.is_const == is_const))
      return (this.label);

  // Make a new Strlit node and link it to the list
  this = Malloc(sizeof(Strlit));

  this.val = strdup(name);
  this.label = genlabel();
  this.is_const= is_const;
  this.next = Strhead;
  Strhead = this;
  return (this.label);
}

// Generate code for all string literals
public void gen_strlits(void) {
  Strlit *this;

  for (this = Strhead; this != NULL; this = this.next)
    cgstrlit(this.label, this.val, this.is_const);
}
