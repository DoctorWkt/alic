// Symbol table for the alic compiler
// (c) 2025 Warren Toomey, GPL3

#include "alic.h"
#include "proto.h"

Sym *Symhead = NULL;		// Linked list of symbols
static Sym *Globhead = NULL;	// Pointer to first global symbol
				// when we have a local scope
static Sym *Curfunc = NULL;	// Pointer to the function we are processing

// Given a pointer to the head of a symbol list, add
// a new symbol node to the list. If the symbol's name
// is already in the list, return NULL. Otherwise
// return a pointer to the new symbol.
Sym *add_sym_to(Sym **head, char *name, int symtype, Type * type) {
  Sym *this, *here;

  // Walk the list to see if the symbol is already there
  for (this= *head; this != NULL; this= this->next)
    if (!strcmp(this->name, name))
      return(NULL);

  // Make the new symbol node
  this = (Sym *) calloc(1, sizeof(Sym));
  if (this == NULL)
    fatal("out of memory in add_sym_to()\n");

  // Fill in the fields
  if (name != NULL) this->name = strdup(name);
  else this->name = NULL;
  this->symtype= symtype;
  this->type = type;

  // The list is empty: make this the first node
  if (*head==NULL) { *head= this; return(this); }

  // If Globhead is the same as the head, then
  // we are adding the first local symbol.
  // Prepend this to the list
  if (Globhead == *head) {
    this->next= *head; *head= this; return(this);
  }

  // Append the symbol to the list but don't 
  // go past Globhead if it is non-NULL, i.e.
  // when we are adding a local symbol
  for (here= *head; ; here= here->next) {
    if (here->next==NULL || here->next == Globhead) {
      this->next= here->next; here->next= this; return(this);
    }
  }

  // We should never get here. Keep -Wall happy
  fatal("add_sym_to() failed\n");
  return(NULL);
}

// Add a new symbol to the main symbol table.
// Check that a symbol of the same name doesn't already exist
Sym *add_symbol(char *name, int symtype, Type *type) {
  Sym *this;

  this= add_sym_to(&Symhead, name, symtype, type);
  if (this==NULL)
    fatal("symbol %s already exists\n", name);
  return (this);
}

// Find a symbol in the main symbol table given its name or return
// NULL if not found. For now I'm not worried about performance
Sym *find_symbol(char *name) {
  Sym *this, *param;

  if (name==NULL) return(NULL);

  for (this = Symhead; this != NULL; this = this->next) {
    if (!strcmp(this->name, name))
      return (this);

    // If this is the function we are currently processing,
    // walk the parameter list to find matching symbols
    if (this == Curfunc) {
      for (param = this->memb; param != NULL; param = param->next)
        if (!strcmp(param->name, name))
          return (param);
    }
  }

  return (NULL);
}

// Start a new scope section on the symbol table.
void new_scope(Sym *func) {
  Globhead= Symhead;
  Curfunc= func;
}

// Remove the latest scope section from the symbol table.
void end_scope(void) {
  Symhead= Globhead; Globhead= NULL; Curfunc= NULL;
}

// Given an A_IDENT node, confirm that it
// is a known symbol. Set the node's type
// and return it.
ASTnode *mkident(ASTnode *n) {
  Sym *s= find_symbol(n->strlit);

  if (s == NULL)
    fatal("Unknown variable %s\n", n->strlit);
  if (s->symtype != ST_VARIABLE)
    fatal("Symbol %s is not a variable\n", n->strlit);
  n->type= s->type;
  n->sym= s;
  return(n);
}

// Generate code for all global symbols
void gen_globsyms(void) {
  Sym *this;

  for (this = Symhead; this != NULL; this = this->next)
    if (this->name != NULL && this->symtype == ST_VARIABLE)
      cgglobsym(this);
}

// Dump the symbol table to the debug file
void dumpsyms(void) {
  Sym *this, *memb;

  if (Debugfh == NULL) return;
  fprintf(Debugfh, "Global symbol table\n");
  fprintf(Debugfh, "-------------------\n");

  for (this = Symhead; this != NULL; this = this->next) {
    fprintf(Debugfh, "%s %s", get_typename(this->type), this->name);

    switch(this->symtype) {
    case ST_FUNCTION: 
      // Print out the parameters
      fprintf(Debugfh, "(");
      if (this->memb==NULL)
        fprintf(Debugfh, "void");
      else {
	for (memb= this->memb; memb != NULL; memb = memb->next) {
	  fprintf(Debugfh, "%s %s", get_typename(memb->type), memb->name);
	  if (memb->next != NULL) fprintf(Debugfh, ", ");
	}
      }
      fprintf(Debugfh, ");");
    }


    fprintf(Debugfh, "\n");
  }
  fprintf(Debugfh, "\n");
}
