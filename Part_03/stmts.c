// Statement handling for the alic compiler
// (c) 2025 Warren Toomey, GPL3

#include "alic.h"
#include "proto.h"

extern FILE *debugfh;
extern Type *ty_flt64;

// Given an ASTnode s which holds a string literal
// and an ASTnode e which holds an expression, return
// an A_PRINT ASTnode to execute printf() with the
// format string followed by the expression
ASTnode *print_statement(ASTnode *s, ASTnode * e) {
  ASTnode *this;

  // If we are printing a flt32, widen it to be a flt64
  if (e->type->kind == TY_FLT32) e = widen_type(e, ty_flt64);

  // Make an A_PRINT AST node
  this= mkastnode(A_PRINT, s, NULL, e);
  this->type = e->type;
  return(this);
}

// Given an ASTnode v which represents a variable and
// an ASTnode e which holds an expression, return
// an A_ASSIGN ASTnode with both of them
ASTnode *assignment_statement(ASTnode * v, ASTnode * e) {
  ASTnode *newnode;

  // Widen the expression's type if required
  newnode= widen_type(e, v->type);
  if (newnode == NULL)
    fatal("Incompatible types %s vs %s\n",
        get_typename(e->type), get_typename(v->type));
  e = newnode;

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

  // Widen the expression's type if required
  newnode= widen_type(e, s->type);
  if (newnode == NULL)
    fatal("Incompatible types %s vs %s\n",
        get_typename(e->type), get_typename(s->type));
  e = newnode;

  // Add the symbol to the symbol table
  sym = add_symbol(s->strlit, false, s->type, 0);

  // Add the symbol pointer and the expresson to the s node.
  // Update the node's operation
  s->sym= sym;
  s->left= e;
  s->op= A_LOCAL;
  return(s);
}
