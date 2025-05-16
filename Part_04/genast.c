// Generate code from an AST tree for the alic compiler.
// (c) 2025 Warren Toomey, GPL3

#include "alic.h"
#include "proto.h"

static void gen_IF(ASTnode * n);
static void gen_WHILE(ASTnode * n);
static void gen_local(ASTnode *n);
static int gen_funccall(ASTnode *n);

// Generate and return a new label number
static int labelid = 1;
int genlabel(void) {
  return (labelid++);
}

// Given an AST, generate assembly code recursively.
// Return the temporary id with the tree's final value.
int genAST(ASTnode * n) {
  int lefttemp, righttemp;
  int label;

  // Empty tree, do nothing
  if (n == NULL) return (NOREG);

  // Do special case nodes before the general processing
  switch (n->op) {
  case A_PRINT:
    righttemp = genAST(n->right);
    label= add_strlit(n->left->strlit);
    cgprint(label, righttemp, n->right->type); return(NOREG);
  case A_LOCAL:
    gen_local(n); return(NOREG);
  case A_FUNCCALL:
    return(gen_funccall(n));
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

// Generate space for a local variable
// and assign its value
void gen_local(ASTnode *n) {
  int lefttemp;

  // Allocate space for the variable
  cgaddlocal(n->type, n->sym);

  // Get the expression's value on the left
  lefttemp  = genAST(n->left);

  // Store this into the local variable
  cgstorvar(lefttemp, n->type, n->sym);

  // and generate any code for the other children
  genAST(n->mid);
  genAST(n->right);
}

// Generate the argument values for a function
// call and then perform the call itself.
// Return any value into a temporary
static int gen_funccall(ASTnode *n) {
  int i, numargs = 0;
  int *arglist = NULL;
  Type **typelist = NULL;
  Sym *func, *param;
  ASTnode *this;

  // Get the matching symbol for the function's name
  func= find_symbol(n->left->strlit);
  if (func == NULL)
    fatal("unknown function %s()\n", n->left->strlit);

  if (func->symtype != ST_FUNCTION)
    fatal("%s is not a function\n", n->left->strlit);


  // Walk the expression list to count the number of arguments
  for (this=n->right; this!=NULL; this= this->right) {
    if (this->op == A_GLUE) numargs++;
  }

  // Check the arg count vs. the function parameter count
  if (numargs != func->count)
    fatal("wrong number of arguments to %s(): %d vs. %d\n",
	n->left->strlit, numargs, func->count);

  if (numargs > 0) {
    // Allocate space to hold the types and
    // temporaries for the expressions
    arglist= (int *)malloc(numargs * sizeof(int));
    typelist = (Type **)malloc(numargs * sizeof(Type *));

    if (arglist == NULL || typelist == NULL)
      fatal("out of memory in gen_funccall()\n");

    // Walk the expression list again.
    // Check and, if needed, widen the expression's
    // type to match the parameter's type.
    // Generate the code for each expression.
    // Cache the temporary number and the type for each one.
    param= func->memb;
    for (i=0, this=n->right; this!=NULL;
			this= this->right, i++, param= param->next) {
      if (this->op == A_GLUE) {
	this->left= widen_expression(this->left, param->type);
	typelist[i]= this->left->type;
	arglist[i]= genAST(this->left);
      } else {
	this= widen_expression(this, param->type);
	typelist[i]= this->type;
	arglist[i]= genAST(this);
      }
    }
  }

  // Generate the QBE code
  cgcall(func, numargs, arglist, typelist);

  return(NOREG);
}
