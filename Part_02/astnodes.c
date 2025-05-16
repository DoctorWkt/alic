// AST node functions for the alic compiler.
// (c) 2025 Warren Toomey, GPL3

#include "alic.h"
#include "proto.h"

extern FILE *debugfh;

// Build and return a generic AST node
ASTnode *mkastnode(int op, Type * type, bool rvalue,
		   ASTnode * left, ASTnode * mid, ASTnode * right,
		   Sym * sym, uint64_t intval) {
  ASTnode *n;

  // Malloc a new ASTnode
  n = (ASTnode *) malloc(sizeof(ASTnode));
  if (n == NULL) fatal("Unable to malloc in mkastnode()");

  // Copy in the field values and return it
  n->op     = op;
  n->type   = type;
  n->rvalue = rvalue;
  n->left   = left;
  n->mid    = mid;
  n->right  = right;
  n->sym    = sym;
  n->litval.uintval = intval;
  return (n);
}

// Make an AST leaf node
ASTnode *mkastleaf(int op, Type * type, bool rvalue,
		   Sym * sym, uint64_t intval) {
  return (mkastnode(op, type, rvalue, NULL, NULL, NULL, sym, intval));
}

#define NOLABEL 0

// List of AST node names
static char *astname[] = { NULL,
  "ASSIGN", "CAST",
  "ADD", "SUBTRACT", "MULTIPLY", "DIVIDE", "NEGATE",
  "EQ", "NE", "LT", "GT", "LE", "GE", "NOT",
  "AND", "OR", "XOR", "INVERT",
  "LSHIFT", "RSHIFT",
  "NUMLIT", "IDENT", "PRINT", "GLUE", "IF", "WHILE", "FOR"
};

// Given an AST tree, print it out and follow the
// traversal of the tree that genAST() follows
void dumpAST(ASTnode * n, int label, int level) {
  if (n == NULL)
    fatal("NULL AST node\n");

#if 0
  // Reset level to -2 for A_GLUE nodes
  if (n->op == A_GLUE) {
    level -= 2;
  } else {
#endif
    // General AST node handling
    for (int i = 0; i < level; i++) fprintf(debugfh, " ");

    if (n->type != NULL) {
      fprintf(debugfh, "%s ", get_typename(n->type));
    }
    fprintf(debugfh, "%s ", astname[n->op]);

    switch (n->op) {
    case A_NUMLIT:
      if (n->type->kind >= TY_FLT32)
        fprintf(debugfh, "%f", n->litval.dblval);
      else
        fprintf(debugfh, "%ld", n->litval.intval);
      break;
    case A_ASSIGN:
      fprintf(debugfh, "%s", n->sym->name);
      break;
    case A_IDENT:
      if (n->rvalue)
        fprintf(debugfh, "rval %s", n->sym->name);
      else
        fprintf(debugfh, "%s", n->sym->name);
      break;
    }

    fprintf(debugfh, "\n");
#if 0
  }
#endif

  // General AST node handling
  if (n->left)  dumpAST(n->left,  NOLABEL, level + 2);
  if (n->mid)   dumpAST(n->mid,   NOLABEL, level + 2);
  if (n->right) dumpAST(n->right, NOLABEL, level + 2);
}
