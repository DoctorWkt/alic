// Expression handling for the alic compiler.
// (c) 2025 Warren Toomey, GPL3

#include "alic.h"
#include "proto.h"

// Perform a binary operation on two AST trees and
// return the resulting AST tree.
// the two sub-trees are expressions of built-in type.
ASTnode *binop(ASTnode * l, ASTnode * r, uint op) {
  ASTnode *this = mkastnode(op, l, NULL, r);
  this->rvalue= true;

  // Don't try to add a type to a GLUE node :-)
  if (op != A_GLUE)
    add_type(this);
  return (this);
}

// Perform a unary operation on an AST tree
// and return the resulting AST tree.
ASTnode *unarop(ASTnode * l, uint op) {
  ASTnode *this= mkastnode(op, l, NULL, NULL);
  this->type = l->type;
  this->rvalue= true;
  return(this);
}
