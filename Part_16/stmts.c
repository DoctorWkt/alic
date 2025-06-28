// Statement handling for the alic compiler
// (c) 2025 Warren Toomey, GPL3

#include "alic.h"
#include "proto.h"

// Given an ASTnode v which represents a variable and
// an ASTnode e which holds an expression, return
// an A_ASSIGN ASTnode with both of them
ASTnode *assignment_statement(ASTnode * v, ASTnode * e) {
  ASTnode *this;

  // Widen the expression's type if required
  // and make it an rvalue
  e = widen_expression(e, v->type);

  // Put the variable on the right so that it
  // is done after we get the expression value
  this = mkastnode(A_ASSIGN, e, NULL, v);

  // Ensure that the variable is not an rvalue
  v->rvalue = false;

  // Don't allow assigning to a const identifier
  if (v->is_const)
    fatal("Cannot change a const variable\n");

  // Don't allow assigning to an array symbol
  if ((v->sym != NULL) && is_array(v->sym))
    fatal("Cannot change an array symbol, only its elements\n");

  this->type = v->type;
  return (this);
}

// Given an A_IDENT ASTnode s which represents a typed symbol
// and an ASTnode e which holds an expression (or NULL),
// add the symbol to the symbol table (or err if it exists)
// and also to the ASTnode. Change the ASTnode to be an A_LOCAL.
// Add the expression as the left child if there is one.
// Return the s node.
ASTnode *declaration_statement(ASTnode * s, ASTnode * e) {
  Sym *sym;
  ASTnode *newnode;

  // Can't use bracketed expressions lists in functions
  if ((e != NULL) && (e->op == A_BEL))
    fatal("Cannot use a bracketed expression list in a function\n");

  // See if the symbol already exists
  sym = find_symbol(s->strlit);
  if (sym != NULL)
    fatal("Symbol %s already exists\n", s->strlit);

  // Add the symbol to the symbol table
  sym = add_symbol(s->strlit, ST_VARIABLE, s->type, SV_LOCAL);
  sym->has_addr = true;
  sym->is_const= s->is_const;
  sym->keytype= s->keytype;

  // If the declaration was marked as an array,
  // update the symbol
  if (s->is_array == true) {
    sym->count = s->count;
  }

  // Widen the expression's type if required
  if (e != NULL) {
    newnode = widen_type(e, s->type, 0);
    if (newnode == NULL)
      fatal("Incompatible types %s vs %s\n",
	    get_typename(e->type), get_typename(s->type));
    e = newnode;
  }

  // Add the symbol pointer and the expresson to the s node.
  // Update the node's operation
  s->sym = sym;
  s->left = e;
  s->op = A_LOCAL;
  return (s);
}
