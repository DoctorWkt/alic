// Generate code from an AST tree for the alic compiler.
// (c) 2025 Warren Toomey, GPL3

#include "alic.h"
#include "proto.h"

static void gen_IF(ASTnode * n);
static void gen_WHILE(ASTnode * n);

// Generate and return a new label number
static int labelid = 1;
int genlabel(void) {
  return (labelid++);
}

// Given an AST, generate assembly code recursively.
// Return the temporary id with the tree's final value.
int genAST(ASTnode * n) {
  int lefttemp, righttemp;

  // Empty tree, do nothing
  if (n == NULL) return (NOREG);

  // Do special case nodes before the general processing
  switch (n->op) {
  case A_IF:
    gen_IF(n); return(NOREG);
  case A_WHILE:
    gen_WHILE(n); return(NOREG);
  case A_FOR:
    // Generate the initial code
    genAST(n->right);

    // Now call gen_WHILE() using the left and mid children
    gen_WHILE(n); return(NOREG);
  }

  // Load the left and right sub-trees into temporaries
  if (n->left)  lefttemp  = genAST(n->left);
  if (n->right) righttemp = genAST(n->right);

  // General processing
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
  case A_GLUE:
    return (NOREG);
  case A_PRINT:
    switch (n->left->type->kind) {
      case TY_FLT64:
        cg_printdbl(lefttemp);
        break;
      default:
        cg_printint(lefttemp);
    }
    return (NOREG);
  }

  // Error
  fatal("genAST() unknown op %d\n", n->op);
  return (NOREG);
}

// Generate the code for an IF statement
// and an optional ELSE clause.
static void gen_IF(ASTnode * n) {
  int Lfalse, Lend = 0;
  int t1;

  // Generate two labels: one for the
  // false compound statement, and one
  // for the end of the overall IF statement.
  // When there is no ELSE clause, Lfalse
  // _is_ the ending label!
  Lfalse = genlabel();
  if (n->right)
    Lend = genlabel();

  // Generate the condition code
  t1 = genAST(n->left);

  // Jump if false to the false label
  cgjump_if_false(t1, Lfalse);

  // Generate the true statement block
  genAST(n->mid);

  // If there is an optional ELSE clause,
  // generate the jump to skip to the end
  if (n->right) {
    // QBE doesn't like two jump instructions in a row, and
    // a break at the end of a true IF section causes this.
    // The solution is to insert a label before the IF jump.
    cglabel(genlabel());
    cgjump(Lend);
  }

  // Now the false label
  cglabel(Lfalse);

  // Optional ELSE clause: generate the false
  // statement block and the end label
  if (n->right) {
    genAST(n->right);
    cglabel(Lend);
  }
}

// Generate the code for a WHILE statement
static void gen_WHILE(ASTnode * n) {
  int Lstart, Lend;
  int t1;

  // Generate the start and end labels
  // and output the start label
  Lstart = genlabel();
  Lend = genlabel();
  cglabel(Lstart);

  // Generate the condition code
  t1 = genAST(n->left);

  // Jump if false to the end label
  cgjump_if_false(t1, Lend);

  // Generate the statement block for the WHILE body
  genAST(n->mid);

  // Finally output the jump back to the condition,
  // and the end label
  cgjump(Lstart);
  cglabel(Lend);
}
