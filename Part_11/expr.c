// Expression handling for the alic compiler.
// (c) 2025 Warren Toomey, GPL3

#include "alic.h"
#include "proto.h"

// Perform a binary operation on two AST trees and
// return the resulting AST tree.
ASTnode *binop(ASTnode * l, ASTnode * r, int op) {
  ASTnode *this = mkastnode(op, l, NULL, r);
  this->rvalue= true;

  // Add a type from the children
  add_type(this);
  return (this);
}

// Perform a unary operation on an AST tree
// and return the resulting AST tree.
ASTnode *unarop(ASTnode * l, int op) {
  ASTnode *this= mkastnode(op, l, NULL, NULL);
  this->type = l->type;
  this->rvalue= true;
  return(this);
}

// Given an ASTnode representing an expression
// and a type, widen the node to match the given type
ASTnode *widen_expression(ASTnode *e, Type *type) {
  ASTnode *newnode;

  // Widen the expression's type if required
  newnode= widen_type(e, type, e->op);
  if (newnode == NULL)
    fatal("Incompatible types %s vs %s\n",
        get_typename(e->type), get_typename(type));
  return(newnode);
}
