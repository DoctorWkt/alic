// AST node functions for the alic compiler.
// (c) 2025 Warren Toomey, GPL3

#include "alic.h"
#include "proto.h"

extern FILE *debugfh;

// Build and return a generic AST node
ASTnode *mkastnode(int op, ASTnode * left, ASTnode * mid, ASTnode * right) {
  ASTnode *n;

  // Malloc a new ASTnode
  n = (ASTnode *) calloc(1, sizeof(ASTnode));
  if (n == NULL)
    fatal("Unable to calloc in mkastnode()");

  // Copy in the field values and return it
  n->op = op;
  n->left = left;
  n->mid = mid;
  n->right = right;
  return (n);
}

// Make an AST leaf node
ASTnode *mkastleaf(int op, Type * type, bool rvalue,
		   Sym * sym, uint64_t intval) {
  ASTnode *n;
  n = mkastnode(op, NULL, NULL, NULL);
  n->type = type;
  n->rvalue = rvalue;
  n->sym = sym;
  n->litval.intval = intval;
  return (n);
}

// Recursively free an AST tree
void freeAST(ASTnode * n) {
  if (n == NULL)
    return;

  freeAST(n->left);
  freeAST(n->mid);
  freeAST(n->right);

  if (n->strlit != NULL)
    free(n->strlit);
  free(n);
}


// List of AST node names
static char *astname[] = { NULL,
  "ASSIGN", "CAST",
  "ADD", "SUBTRACT", "MULTIPLY", "DIVIDE", "NEGATE",
  "EQ", "NE", "LT", "GT", "LE", "GE", "NOT",
  "AND", "OR", "XOR", "INVERT",
  "LSHIFT", "RSHIFT",
  "NUMLIT", "IDENT", "PRINT", "GLUE", "IF", "WHILE", "FOR",
  "TYPE", "STRLIT", "LOCAL", "FUNCCALL"
};

// Given an AST tree, print it out and follow the
// traversal of the tree that genAST() follows
void dumpAST(ASTnode * n, int level) {
  if (n == NULL)
    fatal("NULL AST node\n");

  // General AST node handling
  for (int i = 0; i < level; i++)
    fprintf(debugfh, " ");

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
  case A_LOCAL:
    fprintf(debugfh, "%s", n->sym->name);
    break;
  case A_IDENT:
    if (n->rvalue)
      fprintf(debugfh, "rval %s", n->sym->name);
    else
      fprintf(debugfh, "%s", n->sym->name);
    break;
  case A_PRINT:
  case A_FUNCCALL:
    fprintf(debugfh, "\"%s\"\n", n->left->strlit);
    if (n->right) dumpAST(n->right, level + 2);
    return;
  }

  fprintf(debugfh, "\n");

  // Reset the level if an A_LOCAL node
  if (n->op == A_LOCAL) level -= 2;

  // General AST node handling
  if (n->left)
    dumpAST(n->left, level + 2);
  if (n->mid)
    dumpAST(n->mid, level + 2);
  if (n->right)
    dumpAST(n->right, level + 2);
}
