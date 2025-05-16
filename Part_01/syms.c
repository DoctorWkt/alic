// Symbol table for the alic compiler
// (c) 2025 Warren Toomey, GPL3

#include "alic.h"
#include "proto.h"

static Sym *Symhead = NULL;	// Linked list of symbols

// Add a new symbol to the list.
// Check that a symbol of the same name doesn't already exist
Sym *add_symbol(char *name, bool is_static, Type * type, uint64_t initval) {
  Sym *this;

  // Ensure no symbol with this name exists
  for (this = Symhead; this != NULL; this = this->next)
    if (name != NULL && this->name != NULL && !strcmp(this->name, name))
      fatal("symbol %s already exists\n", name);

  // Make a new Sym node and link it to the list
  this = (Sym *) malloc(sizeof(Sym));
  if (this == NULL)
    fatal("out of memory in add_type()\n");

  this->name = strdup(name);
  this->is_static = is_static;
  this->type = type;
  this->initval.uintval = initval;
  this->next = Symhead;
  Symhead = this;
  return (this);
}

// Find a symbol given its name or NULL if not found.
// For now I'm not worried about performance
Sym *find_symbol(char *name) {
  Sym *this;

  for (this = Symhead; this != NULL; this = this->next)
    if (name != NULL && this->name != NULL && !strcmp(this->name, name))
      return (this);
  return (NULL);
}

// Generate code for all global symbols
void gen_globsyms(void) {
  Sym *this;

  for (this = Symhead; this != NULL; this = this->next)
    cgglobsym(this);
}
