// Symbol table for the alic compiler
// (c) 2025 Warren Toomey, GPL3

#include "alic.ah"
#include "proto.ah"

Scope *Scopehead = NULL;	// Pointer to the most recent scope
Scope *Globhead = NULL;		// Pointer to the global symbol table

// Initialise the symbol table
public void init_symtable(void) {
  Scopehead = Calloc(sizeof(Scope));
  Globhead = Scopehead;
}

// Given a pointer to the head of a symbol list, add
// a new symbol node to the list. If the symbol's name
// is already in the list, return NULL. Otherwise
// return a pointer to the new symbol.
public Sym *add_sym_to(Sym ** head, char *name, int symtype, Type * ty) {
  Sym *this;
  Sym *last;

  // Walk the list to see if the symbol is already there.
  // Also point last at the last node in the list
  last = *head;
  foreach this (*head, this.next) {
    if (strcmp(this.name, name)==0)
      return (NULL);
    last = this;
  }

  // Make the new symbol node
  this = Calloc(sizeof(Sym));

  // Fill in the fields
  if (name != NULL)
    this.name = strdup(name);
  else
    this.name = NULL;
  this.symtype = symtype;
  this.ty = ty;

  // The list is empty: make this the first node
  if (*head == NULL) {
    *head = this;
    return (this);
  }
  // Otherwise append the new node to the list
  last.next = this;
  return (this);
}

// Add a new symbol to the current or the global scope.
// Return a pointer to the symbol
public Sym *add_symbol(char *name, int symtype, Type * ty, int visibility) {
  Sym *this;

  if (visibility != SV_LOCAL) {
    this = add_sym_to(&(Globhead.head), name, symtype, ty);
    if (this != NULL) {
      this.has_addr = true;
      this.visibility = visibility;
    }
  } else {
    this = add_sym_to(&(Scopehead.head), name, symtype, ty);
    if (this != NULL)
      this.visibility = visibility;
  }
  return (this);
}

// Find a symbol in any of the scope's symbol lists or return.
// NULL if not found. For now I'm not worried about performance
public Sym *find_symbol(char *name) {
  Scope *thisscope;
  Sym *this;
  Sym *param;

  if (name == NULL)
    return (NULL);

  foreach thisscope (Scopehead, thisscope.next) {
    foreach this (thisscope.head, this.next) {
      if (strcmp(this.name, name)==0)
	return (this);

      // If this is the function we are currently processing,
      // walk the parameter list to find matching symbols.
      // Also check for any exception variable
      if (this == Thisfunction) {
	foreach param (this.paramlist, param.next)
	  if (strcmp(param.name, name)==0)
	    return (param);
	if ((this.exceptvar != NULL) && (strcmp(this.exceptvar.name, name)==0))
	  return (this.exceptvar);
      }
    }
  }

  return (NULL);
}

// Start a new scope section on the symbol table.
public void new_scope(Sym * func) {
  Scope *thisscope;

  thisscope = Calloc(sizeof(Scope));
  thisscope.next = Scopehead;
  Scopehead = thisscope;
}

// Remove the latest scope section from the symbol table.
public ASTnode * end_scope(void) {
  Sym *this;
  ASTnode *d=NULL;
  ASTnode *e=NULL;

  // If there are any associative arrays in this scope, free them
  foreach this (Scopehead.head, this.next) {
    if (this.keytype != NULL) {
      e= mkastleaf(A_AAFREE, NULL, false, this, 0);
      if (d==NULL)
        d= e;
      else
        d= mkastnode(A_GLUE, d, NULL, e);
    }
  }

  Scopehead = Scopehead.next;
  if (Scopehead == NULL)
    fatal("Somehow we have lost the global scope!\n");
  return(d);
}

// Given an A_IDENT node, confirm that it
// is a known symbol. Set the node's type
// and return it.
public ASTnode *mkident(ASTnode * n) {
  Sym *s = find_symbol(n.strlit);

  if (s == NULL)
    fatal("Unknown variable %s\n", n.strlit);
  if (s.symtype != ST_VARIABLE)
    fatal("Symbol %s is not a variable\n", n.strlit);
  n.ty = s.ty;
  n.sym = s;
  n.is_const= s.is_const;

  // Assume that this is an rvalue
  n.rvalue = true;
  return (n);
}

// Return is a symbol is an array
public bool is_array(Sym * sym) {
  return (sym.symtype == ST_VARIABLE && sym.count > 0);
}

// Dump the symbol table to the debug file
public void dumpsyms(void) {
  Sym *this;
  Sym *memb;

  if (Debugfh == NULL)
    return;
  fprintf(Debugfh, "Global symbol table\n");
  fprintf(Debugfh, "-------------------\n");

  foreach this (Globhead.head, this.next) {
    fprintf(Debugfh, "%s %s", get_typename(this.ty), this.name);

    switch (this.symtype) {
    case ST_FUNCTION:
      // Print out the parameters
      fprintf(Debugfh, "(");
      if (this.paramlist == NULL)
	fprintf(Debugfh, "void");
      else {
	foreach memb (this.paramlist, memb.next) {
	  fprintf(Debugfh, "%s %s", get_typename(memb.ty), memb.name);
	  if (memb.next != NULL)
	    fprintf(Debugfh, ", ");
	}
      }
      fprintf(Debugfh, ");");
    }

    fprintf(Debugfh, "\n");
  }
  fprintf(Debugfh, "\n");
}
