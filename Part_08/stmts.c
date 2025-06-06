// Statement handling for the alic compiler
// (c) 2025 Warren Toomey, GPL3

#include "alic.h"
#include "proto.h"

// Given an ASTnode v which represents a variable and
// an ASTnode e which holds an expression, return
// an A_ASSIGN ASTnode with both of them
ASTnode *assignment_statement(ASTnode * v, ASTnode * e) {

  // Widen the expression's type if required
  e= widen_expression(e, v->type);

  v->rvalue = false;
  v->op = A_ASSIGN;
  v->left = e;
  v->type = v->sym->type;
  return(v);
}

// Given an A_IDENT ASTnode s which represents a typed symbol
// and an ASTnode e which holds an expression, add the symbol
// to the symbol table (or err if it exists) and also to the
// ASTnode. Change the ASTnode to be an A_LOCAL. Then add the
// expression as the left child. Return the s node.
ASTnode *declaration_statement(ASTnode *s, ASTnode * e) {
  Sym *sym;
  ASTnode *newnode;

  // See if the symbol already exists
  sym= find_symbol(s->strlit);
  if (sym != NULL)
    fatal("symbol %s already exists\n", s->strlit);

  // Widen the expression's type if required
  newnode= widen_type(e, s->type);
  if (newnode == NULL)
    fatal("Incompatible types %s vs %s\n",
        get_typename(e->type), get_typename(s->type));
  e = newnode;

  // Add the symbol to the symbol table
  sym = add_symbol(s->strlit, ST_VARIABLE, s->type, false);
  sym->has_addr= true;

  // Add the symbol pointer and the expresson to the s node.
  // Update the node's operation
  s->sym= sym;
  s->left= e;
  s->op= A_LOCAL;
  return(s);
}
