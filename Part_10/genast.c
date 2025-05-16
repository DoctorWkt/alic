// Generate code from an AST tree for the alic compiler.
// (c) 2025 Warren Toomey, GPL3

#include "alic.h"
#include "proto.h"

// When we are processing try/catch statement, we
// keep this node which holds the needed information.
// There is a stack of these as try/catch statement
// can be nested.
typedef struct Edetails Edetails;

struct Edetails {
  Sym *sym;			// The variable that catches the exception
  int Lcatch;			// The label starting the catch clause
  bool in_try;			// Are we processing the try clause?
  Edetails *prev;		// The previous node on the stack
};

static Edetails *Ehead= NULL;	// The stack of Edetail nodes

static void gen_IF(ASTnode * n);
static void gen_WHILE(ASTnode * n);
static void gen_local(ASTnode *n);
static int gen_funccall(ASTnode *n);
static void gen_try(ASTnode *n);

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
  case A_LOCAL:
    gen_local(n); return(NOREG);
  case A_FUNCCALL:
    return(gen_funccall(n));
  case A_TRY:
    gen_try(n); return(NOREG);
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
    // Load our value if we are an rvalue
    if (n->rvalue == true)
      return (cgloadvar(n->sym));
    return(NOREG);
  case A_ASSIGN:
    switch (n->right->op) {
    case A_IDENT:
      // We are assigning to an identifier
      return(cgstorvar(lefttemp, n->type, n->right->sym));
    case A_DEREF:
      // We are assigning though a pointer
      return (cgstorderef(lefttemp, righttemp, n->right->type));
    default:
      fatal("Bad A_ASSIGN in genAST()\n");
    }
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
  case A_RETURN:
    cgreturn(lefttemp, Thisfunction->type);
    return (NOREG);
  case A_ABORT:
    cgabort();
    return (NOREG);
  case A_STRLIT:
    label= add_strlit(n->strlit);
    return(cgloadglobstr(label));
  case A_ADDR:
    return(cgaddress(n->sym));
  case A_DEREF:
    // If we are an rvalue, dereference to get the value we point at,
    // otherwise leave it for A_ASSIGN to store through the pointer
    if (n->rvalue == true)
      return(cgderef(lefttemp, value_at(n->left->type)));
    else
      return(lefttemp);
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

  // Get the expression's value
  // on the left if there is one
  if (n->left != NULL) {
    lefttemp  = genAST(n->left);

    // Store this into the local variable
    cgstorvar(lefttemp, n->type, n->sym);
  }

  // Generate any code for the other children
  genAST(n->mid);
  genAST(n->right);
}

// Generate the argument values for a function
// call and then perform the call itself.
// Return any value into a temporary.
//
// XXX This whole function needs a fair bit of refactoring!
//
static int gen_funccall(ASTnode *n) {
  int i, numargs = 0;
  int excepttemp= NOREG;
  int return_temp;
  Litval zero;
  int zerotemp;
  int *arglist = NULL;
  Type **typelist = NULL;
  Sym *func, *param;
  ASTnode *this;
  bool func_throws;

  // Get the matching symbol for the function's name
  func= find_symbol(n->left->strlit);
  if (func == NULL)
    fatal("unknown function %s()\n", n->left->strlit);

  if (func->symtype != ST_FUNCTION)
    fatal("%s is not a function\n", n->left->strlit);

  // Cache if the function throws an exception
  func_throws= (func->exceptvar != NULL);

  // If the function throws an exception, we had better
  // be in a try or catch clause
  if (func_throws && (Ehead == NULL))
    fatal("must call %s() in a try or catch clause\n", n->left->strlit);

  // Walk the expression list to count the number of arguments
  for (this=n->right; this!=NULL; this= this->right) {
    if (this->op == A_GLUE || this->op == A_ASSIGN) numargs++;
  }

  // Check the arg count vs. the function parameter count.
  // Don't do this if the function is variadic (count is -1)
  if ((func->count) != -1 && (numargs != func->count))
    fatal("wrong number of arguments to %s(): %d vs. %d\n",
	n->left->strlit, numargs, func->count);

  if (numargs > 0) {
    // Allocate space to hold the types and
    // temporaries for the expressions
    arglist= (int *)Malloc(numargs * sizeof(int));
    typelist = (Type **)Malloc(numargs * sizeof(Type *));

    if (arglist == NULL || typelist == NULL)
      fatal("out of memory in gen_funccall()\n");

    // Do we have a named expression list?
    if (n->right->op == A_ASSIGN) {
      // Walk the function's parameter list and set count zero for each.
      // We use this as a flag to tell if a param name gets used again.
      for (param= func->memb; param != NULL; param= param->next)
	param->count=0;

      // Walk the function's parameter list again
      for (i=0, param= func->memb; param != NULL; i++, param= param->next) {
        // Find the named expression that matches the parameter name
	for (this=n->right; this != NULL; this= this->right) {
	  if (!strcmp(param->name, this->strlit)) {

	    // See if we have already used this parameter name
	    if (param->count == 1)
	      fatal("parameter %s used multiple times\n", param->name);
	    
	    // Check and, if needed, widen the expression's
	    // type to match the parameter's type.
	    // Generate the code for each expression.
            // Cache the temporary number and the type for each one.
	    // XXX Do some DRY code removal here!!
	    param->count= 1;
	    this->left= widen_expression(this->left, param->type);
	    typelist[i]= this->left->type;
            arglist[i]= genAST(this->left);
	  }
	}
      }
    } else {

      // No, it's only a normal expression list.
      // Walk the expression list again.
      // Check and, if needed, widen the expression's
      // type to match the parameter's type.
      // Don't widen if the function is variadic.
      // Generate the code for each expression.
      // Cache the temporary number and the type for each one.
      param= func->memb;
      for (i=0, this=n->right; this!=NULL; this= this->right, i++) {
        if (this->op == A_GLUE) {
	  // Special printf handling code: widen all flt32s to flt64
	  if (!strcmp(func->name, "printf") && (this->left->type->kind == TY_FLT32))
	    this->left = widen_type(this->left, ty_flt64);
	  if (func->count != -1)
	    this->left= widen_expression(this->left, param->type);
	  typelist[i]= this->left->type;
	  arglist[i]= genAST(this->left);
        } else {
	  // Special printf handling code: widen all flt32s to flt64
	  if (!strcmp(func->name, "printf") && (this->type->kind == TY_FLT32))
	    this = widen_type(this, ty_flt64);
	  if (func->count != -1)
	    this= widen_expression(this, param->type);
	  typelist[i]= this->type;
	  arglist[i]= genAST(this);
        }

	// Move up to the next parameter if not variadic
	if (func->count != -1)
	  param= param->next;
      }
    }
  }

  // If we have an exception variable
  // and the function throws an exception,
  // get its address into a temporary
  if (func_throws) {
    excepttemp= cgaddress(Ehead->sym);

    // Get a literal zero into a temporary
    zero.intval= 0;
    zerotemp= cgloadlit(zero, ty_int32);

    // Set the exception variable's first member to zero
    cgstorderef(zerotemp, excepttemp, ty_int32);
  }

  // Generate the QBE code for the function call
  return_temp= cgcall(func, numargs, excepttemp, arglist, typelist);

  // If we are in a try clause, test if the first
  // member of the exception variable is not zero.
  // If not, jump to the catch clause
  if (func_throws && (Ehead != NULL)  && (Ehead->in_try == true)) {

    // Get the value of the first member in the exception variable
    excepttemp= cgderef(excepttemp, ty_int32);

    // Compare the first member against zero
    excepttemp= cgcompare(A_EQ, excepttemp, zerotemp, ty_int32);

    // Jump if false to the catch label
    cgjump_if_false(excepttemp, Ehead->Lcatch);
  }

  // Otherwise, return any value from the function call
  return(return_temp);
}

static void gen_try(ASTnode *n) {
  int Lcatch, Lend;
  Edetails *this;

  // Generate the labels for the start
  // and end of the catch clause
  Lcatch= genlabel();
  Lend= genlabel();

  // Make an Edetails node for this try statement
  // and fill it in
  this= (Edetails *)Malloc(sizeof(Edetails));
  this->sym= n->sym;
  this->Lcatch= Lcatch;
  this->in_try= true;

  // Push the node on the stack
  this->prev= Ehead;
  Ehead= this;

  // Generate the code for the try clause
  // and jump past the catch clause
  genAST(n->left);
  cgjump(Lend);

  // Output the label for the catch clause,
  // then the catch code, then the end label
  this->in_try= false;
  cglabel(Lcatch);
  genAST(n->right);
  cglabel(Lend);

  // Finally remove the Edetails node
  Ehead= Ehead->prev;
}
