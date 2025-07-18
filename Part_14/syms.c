// Symbol table for the alic compiler
// (c) 2025 Warren Toomey, GPL3

#include "alic.h"
#include "proto.h"

static Scope *Scopehead = NULL;	// Pointer to the most recent scope
static Scope *Globhead = NULL;	// Pointer to the global symbol table

// Initialise the symbol table
void init_symtable(void) {
  Scopehead = (Scope *) Calloc(sizeof(Scope));
  Globhead = Scopehead;
}

// Given a pointer to the head of a symbol list, add
// a new symbol node to the list. If the symbol's name
// is already in the list, return NULL. Otherwise
// return a pointer to the new symbol.
Sym *add_sym_to(Sym ** head, char *name, int symtype, Type * type) {
  Sym *this, *last;

  // Walk the list to see if the symbol is already there.
  // Also point last at the last node in the list
  for (this = last = *head; this != NULL; last = this, this = this->next)
    if (!strcmp(this->name, name))
      return (NULL);

  // Make the new symbol node
  this = (Sym *) Calloc(sizeof(Sym));

  // Fill in the fields
  if (name != NULL)
    this->name = strdup(name);
  else
    this->name = NULL;
  this->symtype = symtype;
  this->type = type;

  // The list is empty: make this the first node
  if (*head == NULL) {
    *head = this;
    return (this);
  }
  // Otherwise append the new node to the list
  last->next = this;
  return (this);
}

// Add a new symbol to the current or the global scope.
// Return a pointer to the symbol
Sym *add_symbol(char *name, int symtype, Type * type, int visibility) {
  Sym *this;

  if (visibility != SV_LOCAL) {
    this = add_sym_to(&(Globhead->head), name, symtype, type);
    if (this != NULL) {
      this->has_addr = true;
      this->visibility = visibility;
    }
  } else {
    this = add_sym_to(&(Scopehead->head), name, symtype, type);
    if (this != NULL)
      this->visibility = visibility;
  }
  return (this);
}

// Find a symbol in any of the scope's symbol lists or return.
// NULL if not found. For now I'm not worried about performance
Sym *find_symbol(char *name) {
  Scope *thisscope;
  Sym *this, *param;

  if (name == NULL)
    return (NULL);

  for (thisscope = Scopehead; thisscope != NULL; thisscope = thisscope->next) {
    for (this = thisscope->head; this != NULL; this = this->next) {
      if (!strcmp(this->name, name))
	return (this);

      // If this is the function we are currently processing,
      // walk the parameter list to find matching symbols.
      // Also check for any exception variable
      if (this == Thisfunction) {
	for (param = this->paramlist; param != NULL; param = param->next)
	  if (!strcmp(param->name, name))
	    return (param);
	if ((this->exceptvar != NULL) && !strcmp(this->exceptvar->name, name))
	  return (this->exceptvar);
      }
    }
  }

  return (NULL);
}

// Start a new scope section on the symbol table.
void new_scope(Sym * func) {
  Scope *thisscope;

  thisscope = (Scope *) Calloc(sizeof(Scope));
  thisscope->next = Scopehead;
  Scopehead = thisscope;
}

// Remove the latest scope section from the symbol table.
void end_scope(void) {
  Scopehead = Scopehead->next;
  if (Scopehead == NULL)
    fatal("Somehow we have lost the global scope!\n");
}

// Given an A_IDENT node, confirm that it
// is a known symbol. Set the node's type
// and return it.
ASTnode *mkident(ASTnode * n) {
  Sym *s = find_symbol(n->strlit);

  if (s == NULL)
    fatal("Unknown variable %s\n", n->strlit);
  if (s->symtype != ST_VARIABLE)
    fatal("Symbol %s is not a variable\n", n->strlit);
  n->type = s->type;
  n->sym = s;
  n->is_const= s->is_const;

  // Assume that this is an rvalue
  n->rvalue = true;
  return (n);
}

// Return is a symbol is an array
bool is_array(Sym * sym) {
  return (sym->symtype == ST_VARIABLE && sym->count > 0);
}

// Dump the symbol table to the debug file
void dumpsyms(void) {
  Sym *this, *memb;

  if (Debugfh == NULL)
    return;
  fprintf(Debugfh, "Global symbol table\n");
  fprintf(Debugfh, "-------------------\n");

  for (this = Globhead->head; this != NULL; this = this->next) {
    fprintf(Debugfh, "%s %s", get_typename(this->type), this->name);

    switch (this->symtype) {
    case ST_FUNCTION:
      // Print out the parameters
      fprintf(Debugfh, "(");
      if (this->paramlist == NULL)
	fprintf(Debugfh, "void");
      else {
	for (memb = this->paramlist; memb != NULL; memb = memb->next) {
	  fprintf(Debugfh, "%s %s", get_typename(memb->type), memb->name);
	  if (memb->next != NULL)
	    fprintf(Debugfh, ", ");
	}
      }
      fprintf(Debugfh, ");");
    }

    fprintf(Debugfh, "\n");
  }
  fprintf(Debugfh, "\n");
}
