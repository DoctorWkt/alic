// Expression handling for the alic compiler.
// (c) 2025 Warren Toomey, GPL3

#include "alic.h"
#include "proto.h"

// Perform a binary operation on two AST trees and
// return the resulting AST tree.
ASTnode *binop(ASTnode * l, ASTnode * r, int op) {
  ASTnode *this = mkastnode(op, l, NULL, r);
  this->rvalue = true;

  // Add a type from the children.
  // Propogate up any const attribute
  this->is_const = l->is_const;
  add_type(this);
  return (this);
}

// Perform a unary operation on an AST tree
// and return the resulting AST tree.
ASTnode *unarop(ASTnode * l, int op) {
  ASTnode *this = mkastnode(op, l, NULL, NULL);
  this->type = l->type;
  this->is_const = l->is_const;
  this->rvalue = true;
  return (this);
}

// Given an ASTnode representing an expression
// and a type, widen the node to match the given type
ASTnode *widen_expression(ASTnode * e, Type * type) {
  ASTnode *newnode;

  // Widen the expression's type if required
  newnode = widen_type(e, type, e->op);
  if (newnode == NULL)
    fatal("Incompatible types %s vs %s\n",
	  get_typename(e->type), get_typename(type));

  return (newnode);
}

// Given an ASTnode n which is an array and an expression e,
// return an AST tree which holds the value of the element
// at the 'e' index position in the array
ASTnode *get_array_element(ASTnode * n, ASTnode * e) {
  Sym *sym;
  Type *ty;
  ASTnode *off;

  e = widen_type(e, ty_int64, 0);

  // Check that n is a pointer
  ty = n->type;
  if (ty->ptr_depth == 0)
    fatal("Cannot do array access on a scalar\n");

  // Do bounds checking if needed
  if (O_boundscheck == true) {
    // Is this an array not a pointer?
    if (n->op == A_IDENT) {
      sym = find_symbol(n->strlit);
      if (is_array(sym)) {
	// Add the BOUND node with the array's name and size
	e = mkastnode(A_BOUNDS, e, NULL, NULL);
	e->count = sym->count;
	e->strlit = sym->name;
	e->type = ty_int64;
      }
    }
  }
  // Get the "value at" type
  ty = value_at(ty);

  // Make a NUMLIT node with the size of the base type
  off = mkastleaf(A_NUMLIT, ty_uint64, true, NULL, ty->size);

  // Multiply this by the expression's value
  e = binop(e, off, A_MULTIPLY);

  // Add on the array's base.
  // Mark this as a pointer
  e = binop(e, n, A_ADDOFFSET);
  e->type = n->type;
  e->is_const = n->is_const;

  // If this isn't a struct,
  // dereference this address
  // and mark it with the correct type
  if (!is_struct(ty)) {
    e = mkastnode(A_DEREF, e, NULL, NULL);
    e->type = ty;
    e->rvalue = true;
    e->is_const = e->left->is_const;
  }
  return (e);
}
