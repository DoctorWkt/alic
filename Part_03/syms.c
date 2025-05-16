// Symbol table for the alic compiler
// (c) 2025 Warren Toomey, GPL3

#include "alic.h"
#include "proto.h"

extern FILE *debugfh;

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

  if (name != NULL) this->name = strdup(name);
  else this->name = NULL;
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

// Start a new scope section on the symbol table.
// It is represented by a symbol with no name.
void new_scope(void) {
  // A NULL name represents the start of a scope
  add_symbol(NULL, false, NULL, 0);
}

// Remove the latest scope section from the symbol table.
void end_scope(void) {
  Sym *this;

  // Search for a symbol with no name
  for (this = Symhead; this != NULL; this = this->next)
    if (this->name == NULL) {
      Symhead= this->next;
      return;
    }
}

// Given an A_IDENT node, confirm that it
// is a known symbol. Set the node's type
// and return it.
ASTnode *mkident(ASTnode *n) {
  Sym *s= find_symbol(n->strlit);

  if (s == NULL)
    fatal("Unknown variable %s\n", n->strlit);
  n->type= s->type;
  n->sym= s;
  return(n);
}

// Generate code for all global symbols
void gen_globsyms(void) {
  Sym *this;

  for (this = Symhead; this != NULL; this = this->next)
    if (this->name != NULL)
      cgglobsym(this);
}

// Dump the symbol table to the debug file
void dumpsyms(void) {
  Sym *this;

  if (debugfh == NULL) return;

  for (this = Symhead; this != NULL; this = this->next) {
    if (this->name == NULL) {
      fprintf(debugfh, "SCOPE separator\n");
    } else {
      fprintf(debugfh, "%s %s\n",
	get_typename(this->type), this->name);
    }
  }
  fprintf(debugfh, "-----\n");
}
