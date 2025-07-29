// Expression handling for the alic compiler.
// (c) 2025 Warren Toomey, GPL3

#include "alic.ah"
#include "proto.ah"

// Perform a binary operation on two AST trees and
// return the resulting AST tree.
public ASTnode *binop(const ASTnode * l, const ASTnode * r, const int op) {
  ASTnode *this = mkastnode(op, l, NULL, r);
  this.rvalue = true;

  // Add a type from the children
  // Propogate up any const attribute
  this.is_const= l.is_const;
  add_type(this);
  return (this);
}

// Perform a unary operation on an AST tree
// and return the resulting AST tree.
public ASTnode *unarop(const ASTnode * l, const int op) {
  ASTnode *this = mkastnode(op, l, NULL, NULL);
  this.ty = l.ty;
  this.is_const= l.is_const;
  this.rvalue = true;
  return (this);
}

// Given an ASTnode representing an expression
// and a type, widen the node to match the given type
public ASTnode *widen_expression(const ASTnode * e, const Type * ty) {
  ASTnode *newnode;

  // Widen the expression's type if required
  newnode = widen_type(e, ty, e.op);
  if (newnode == NULL)
    fatal("Incompatible types %s vs %s\n",
	  get_typename(e.ty), get_typename(ty));

  return (newnode);
}

// Given a symbol, an index expression at the dimension
// indicated by level, return an ASTnode which holds the
// linear index of the expression with correct type.
//
// This get called where prevoffset is the result of the previous
// dimension calculation or NULL.
//
ASTnode *get_ary_offset(const Sym *sym, ASTnode *e, const ASTnode *prevoffset, const int level) {
  ASTnode *b;
  Type *elemtype;
  int offset;

  // Build an A_BOUNDS node with e and the size of this dimension
  if (O_boundscheck == true) {
    b= mkastleaf(A_NUMLIT, ty_int64, true, NULL, sym.dimsize[level]);
    e= binop(e, b, A_BOUNDS);
    e.strlit = sym.name;
  }

  // Get the size of the elements in the array
  elemtype= value_at(sym.ty);
  offset= elemtype.size;

  // Multiply this with the product of the sizes of the remaining dimensions
  offset= offset * get_numelements(sym, level+1);

  if (offset != 1) {
    b= mkastleaf(A_NUMLIT, ty_int64, true, NULL, offset);
    e= binop(e, b, A_MULTIPLY);
  }

  // Add this to any previous get_ary_offset() value
  if (prevoffset != NULL)
    e= binop(e, prevoffset, A_ADD);

  return(e);
}
