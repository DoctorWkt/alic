// Generate code from an AST tree for the alic compiler.
// (c) 2025 Warren Toomey, GPL3

#include "alic.h"
#include "proto.h"

// Given an AST, generate assembly code recursively.
// Return the temporary id with the tree's final value.
int genAST(ASTnode * n) {
  int lefttemp, righttemp;

  // Empty tree, do nothing
  if (n == NULL) return (NOREG);

  // Load the left and right sub-trees into temporaries
  if (n->left)  lefttemp  = genAST(n->left);
  if (n->right) righttemp = genAST(n->right);

  switch (n->op) {
  case A_NUMLIT:
    return (cgloadlit(n->litval, n->type));
  case A_ADD:
    return (cgadd(lefttemp, righttemp, n->type));
  case A_SUBTRACT:
    return (cgsub(lefttemp, righttemp, n->type));
  case A_MULTIPLY:
    return (cgmul(lefttemp, righttemp, n->type));
  case A_DIVIDE:
    return (cgdiv(lefttemp, righttemp, n->type));
  case A_NEGATE:
    return (cgnegate(lefttemp, n->type));
  case A_IDENT:
    return (cgloadvar(n->sym));
  case A_ASSIGN:
    cgstorvar(lefttemp, n->type, n->sym);
    return (NOREG);
  case A_CAST:
    return (cgcast(lefttemp, n->left->type, n->type));
  case A_EQ:
  case A_NE:
  case A_LT:
  case A_GT:
  case A_LE:
  case A_GE:
    return (cgcompare(n->op, lefttemp, righttemp, n->left->type));
  case A_INVERT:
    return (cginvert(lefttemp, n->type));
  case A_AND:
    return (cgand(lefttemp, righttemp, n->type));
  case A_OR:
    return (cgor(lefttemp, righttemp, n->type));
  case A_XOR:
    return (cgxor(lefttemp, righttemp, n->type));
  case A_LSHIFT:
    return (cgshl(lefttemp, righttemp, n->type));
  case A_RSHIFT:
    return (cgshr(lefttemp, righttemp, n->type));
  case A_NOT:
    return (cgnot(lefttemp, n->type));
  }

  // Error
  fatal("genAST() unknown op %d\n", n->op);
  return (NOREG);
}
