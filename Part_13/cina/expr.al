// Expression handling for the alic compiler.
// (c) 2025 Warren Toomey, GPL3

#include "alic.ah"
#include "proto.ah"

// Perform a binary operation on two AST trees and
// return the resulting AST tree.
public ASTnode *binop(ASTnode * l, ASTnode * r, int op) {
  ASTnode *this = mkastnode(op, l, NULL, r);
  this.rvalue = true;

  // Add a type from the children
  add_type(this);
  return (this);
}

// Perform a unary operation on an AST tree
// and return the resulting AST tree.
public ASTnode *unarop(ASTnode * l, int op) {
  ASTnode *this = mkastnode(op, l, NULL, NULL);
  this.ty = l.ty;
  this.rvalue = true;
  return (this);
}

// Given an ASTnode representing an expression
// and a type, widen the node to match the given type
public ASTnode *widen_expression(ASTnode * e, Type * ty) {
  ASTnode *newnode;

  // Widen the expression's type if required
  newnode = widen_type(e, ty, e.op);
  if (newnode == NULL)
    fatal("Incompatible types %s vs %s\n",
	  get_typename(e.ty), get_typename(ty));

  return (newnode);
}
