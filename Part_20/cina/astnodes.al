// AST node functions for the alic compiler.
// (c) 2025 Warren Toomey, GPL3

#include "alic.ah"
#include "proto.ah"

// Build and return a generic AST node
public ASTnode *mkastnode(const int op, const ASTnode * left,
		const ASTnode * mid, const ASTnode * right) {
  ASTnode *n;

  // Calloc a new ASTnode
  n = Calloc(sizeof(ASTnode));

  // Copy in the field values and return it
  n.op = op;
  n.left = left;
  n.mid = mid;
  n.right = right;
  n.line = Line;
  return (n);
}

// Make an AST leaf node
public ASTnode *mkastleaf(const int op, const Type * ty, const bool rvalue,
		   const Sym * sym, const int64 intval) {
  ASTnode *n;
  n = mkastnode(op, NULL, NULL, NULL);
  n.ty = ty;
  n.rvalue = rvalue;
  n.sym = sym;
  n.litval.intval = intval;
  n.line = Line;
  return (n);
}

// List of AST node names
const string astname[63] = { NULL,
  "ASSIGN", "WIDEN",
  "ADD", "SUBTRACT", "MULTIPLY", "DIVIDE", "NEGATE",
  "EQ", "NE", "LT", "GT", "LE", "GE", "NOT",
  "AND", "OR", "XOR", "INVERT",
  "LSHIFT", "RSHIFT",
  "NUMLIT", "IDENT", "BREAK", "GLUE", "IF", "WHILE", "FOR",
  "TYPE", "STRLIT", "LOCAL", "FUNCCALL", "RETURN", "ADDR",
  "DEREF", "ABORT", "TRY", "CONTINUE", "SCALE", "A_ADDOFFSET",
  "SWITCH", "CASE", "DEFAULT", "FALLTHRU", "MOD",
  "LOGAND", "LOGNOT", "BEL", "BOUNDS", "TERNARY",
  "VASTART", "VAARG", "VAEND", "CAST",
  "AARRAY", "EXISTS", "UNDEF", "AAFREE",
  "AAITERSTART", "AANEXT", "FUNCITER", "STRINGITER",
  "ARRAYITER"
};

// Given an AST tree, print it out and follow the
// traversal of the tree that genAST() follows
public void dumpAST(const ASTnode * n, int level) {
  int i;

  if (n == NULL)
    fatal("NULL AST node\n");

  // General AST node handling
  foreach i (0 ... level - 1)
    fprintf(Debugfh, " ");

  if (n.ty != NULL) {
    fprintf(Debugfh, "%s ", get_typename(n.ty));
  }

  fprintf(Debugfh, "%s ", astname[n.op]);

  switch (n.op) {
  case A_NUMLIT:
    if (is_flonum(n.ty))
      fprintf(Debugfh, "%f", n.litval.dblval);
    else
      fprintf(Debugfh, "%ld", n.litval.intval);
  case A_LOCAL:
    fprintf(Debugfh, "%s", n.sym.name);
  case A_STRLIT:
    fprintf(Debugfh, "\"%s\"", n.strlit);
  case A_IDENT:
    fprintf(Debugfh, "%s", n.sym.name);
  case A_ADDR:
    fprintf(Debugfh, "%s", n.sym.name);
  case A_FUNCCALL:
    fprintf(Debugfh, "\"%s\"\n", n.left.strlit);
    if (n.right != NULL)
      dumpAST(n.right, level + 2);
    return;
  }

  if (n.is_const == true)
    fprintf(Debugfh, " const ");

  if (n.rvalue == true)
    fprintf(Debugfh, " rval");

  if (n.count > 0)
    fprintf(Debugfh, " count %d", n.count);

  fprintf(Debugfh, "\n");

  // Reset the level if an A_LOCAL node
  if (n.op == A_LOCAL)
    level = level - 2;

  // General AST node handling
  if (n.left != NULL)
    dumpAST(n.left, level + 2);
  if (n.mid != NULL)
    dumpAST(n.mid, level + 2);
  if (n.right != NULL)
    dumpAST(n.right, level + 2);
}

// Is this an integer NUMLIT?
bool is_intlit(const ASTnode * n) {
  if (n == NULL)
    return (false);
  if (n.op == A_NUMLIT && is_integer(n.ty))
    return (true);
  return (false);
}

// Fold an AST tree with a binary operator
// and two integer A_NUMLIT children. Return either 
// the original tree or a new leaf node.
ASTnode *fold2(const ASTnode * n) {
  int64 val;
  int64 leftval;
  int64 rightval;

  // Get the values from each child
  leftval = n.left.litval.intval;
  rightval = n.right.litval.intval;

  // Perform some of the binary operations.
  // For any AST op we can't do, return
  // the original tree.
  switch (n.op) {
  case A_ADD:
    val = leftval + rightval;
  case A_SUBTRACT:
    val = leftval - rightval;
  case A_MULTIPLY:
    val = leftval * rightval;
  case A_DIVIDE:
    // Don't try to divide by zero.
    if (rightval == 0)
      return (n);
    val = leftval / rightval;
  default:
    return (n);
  }

  // Return a leaf node with the new value
  return (mkastleaf(A_NUMLIT, n.ty, true, NULL, val));
}

// Fold an AST tree with a unary operator
// and one integer NUMLIT children. Return either 
// the original tree or a new leaf node.
ASTnode *fold1(const ASTnode * n) {
  int64 val;

  // Get the child value. Do the
  // operation if recognised.
  // Return the new leaf node.
  val = n.left.litval.intval;
  switch (n.op) {
  case A_INVERT:
    val = ~val;
  default:
    return (n);
  }

  // Return a leaf node with the new value
  return (mkastleaf(A_NUMLIT, n.ty, true, NULL, val));
}

// Attempt to do constant folding on
// the AST tree with the root node n
ASTnode *fold(ASTnode * n) {

  if (n == NULL)
    return (NULL);

  // Fold on the left child, then
  // do the same on the right child
  n.left = fold(n.left);
  n.right = fold(n.right);

  // If both children are integer NUMLITS, do a fold2()
  if (is_intlit(n.left)) {
    if (is_intlit(n.right))
      n = fold2(n);
    else
      // If only the left is an integer NUMLIT, do a fold1()
      n = fold1(n);
  }

  // Return the possibly modified tree
  return (n);
}

// Optimise an AST tree by
// constant folding in all sub-trees
public ASTnode *optAST(ASTnode * n) {
  n = fold(n);
  return (n);
}
