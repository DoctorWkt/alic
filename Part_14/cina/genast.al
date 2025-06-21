// Generate code from an AST tree for the alic compiler.
// (c) 2025 Warren Toomey, GPL3

#include "alic.ah"
#include "proto.ah"

// When we are processing try/catch statement, we
// keep this node which holds the needed information.
// There is a stack of these as try/catch statement
// can be nested.

type Edetails = struct {
  Sym *sym,			// The variable that catches the exception
  int Lcatch,			// The label starting the catch clause
  bool in_try,			// Are we processing the try clause?
  Edetails *prev		// The previous node on the stack
};

Edetails *Ehead = NULL;		// The stack of Edetail nodes

// We keep a stack of jump labels
// for break and continue statements.
type Breaklabel= struct {
  int break_label,
  int continue_label,
  Breaklabel *prev
};

Breaklabel *Breakhead = NULL;	// The stack of Breaklabel nodes

// We keep a stack of "next case"
// labels for switch statements
type Switchlabel = struct {
  int next_label,
  Switchlabel *prev
};

Switchlabel *Switchhead = NULL;	// The stack of Switchlabel nodes

void gen_IF(ASTnode * n);
void gen_WHILE(ASTnode * n, int for_label);
void gen_SWITCH(ASTnode * n);
void gen_local(ASTnode * n);
int gen_funccall(ASTnode * n);
void gen_try(ASTnode * n);
int gen_ternary(ASTnode * n);
int gen_logandor(ASTnode * n);
int gen_cast(ASTnode * n);

// Generate and return a new label number
int labelid = 1;
public int genlabel(void) {
  labelid++;
  return (labelid);
}

// Given an AST, generate assembly code recursively.
// Return the temporary id with the tree's final value.
public int genAST(const ASTnode * n) {
  int lefttemp;
  int righttemp;
  int functemp;
  int temp;
  int label;

  // Empty tree, do nothing
  if (n == NULL)
    return (NOTEMP);

  // Do special case nodes before the general processing
  switch (n.op) {
  case A_LOCAL:
    gen_local(n);
    return (NOTEMP);
  case A_FUNCCALL:
    return (gen_funccall(n));
  case A_TRY:
    gen_try(n);
    return (NOTEMP);
  case A_IF:
    gen_IF(n);
    return (NOTEMP);
  case A_WHILE:
    gen_WHILE(n, 0);
    return (NOTEMP);
  case A_SWITCH:
    gen_SWITCH(n);
    return (NOTEMP);
  case A_LOGOR:
  case A_LOGAND:
    return (gen_logandor(n));
  case A_TERNARY:
    return (gen_ternary(n));
  case A_CAST:
    return (gen_cast(n));
  case A_FOR:
    // Generate the initial code
    genAST(n.right);

    // Generate a label to be used by any 'continue' statement
    label = genlabel();

    // Now call gen_WHILE() using the left and mid children
    gen_WHILE(n, label);
    return (NOTEMP);
  }

  // Load the left and right sub-trees into temporaries.
  // If we are a GLUE node with a true is_short_assign,
  // this means that the right sub-tree is the short assignment
  // in a FOR loop. Insert the continue label from the
  // Breaklabel stack between the sub-trees
  if (n.left != NULL)
    lefttemp = genAST(n.left);

  if ((n.op == A_GLUE) && (n.is_short_assign == true)) {
    if (Breakhead == NULL)
      fatal("NULL Breakhead trying to generate FOR continue label\n");
    cglabel(Breakhead.continue_label);
  }

  if (n.right != NULL)
    righttemp = genAST(n.right);

  // General processing
  switch (n.op) {
  case A_NUMLIT:
    return (cgloadlit(&(n.litval), n.ty));
  case A_ADD:
  case A_ADDOFFSET:
    return (cgadd(lefttemp, righttemp, n.ty));
  case A_SUBTRACT:
    return (cgsub(lefttemp, righttemp, n.ty));
  case A_MULTIPLY:
    return (cgmul(lefttemp, righttemp, n.ty));
  case A_MOD:
    return (cgmod(lefttemp, righttemp, n.ty));
  case A_DIVIDE:
    return (cgdiv(lefttemp, righttemp, n.ty));
  case A_NEGATE:
    return (cgnegate(lefttemp, n.ty));
  case A_IDENT:
    // Load our value if we are an rvalue
    if (n.rvalue == true)
      return (cgloadvar(n.sym));
    return (NOTEMP);
  case A_ASSIGN:
    switch (n.right.op) {
    case A_IDENT:
      // We are assigning to an identifier.
      return (cgstorvar(lefttemp, n.ty, n.right.sym));
    case A_DEREF:
      // We are assigning though a pointer
      return (cgstorderef(lefttemp, righttemp, n.right.ty));
    default:
      fatal("Bad A_ASSIGN in genAST()\n");
    }
  case A_WIDEN:
    functemp = add_strlit(Thisfunction.name, true);
    return (cgcast(lefttemp, n.left.ty, n.ty, functemp));
  case A_EQ:
  case A_NE:
  case A_LT:
  case A_GT:
  case A_LE:
  case A_GE:
    return (cgcompare(n.op, lefttemp, righttemp, n.left.ty));
  case A_INVERT:
    return (cginvert(lefttemp, n.ty));
  case A_AND:
    return (cgand(lefttemp, righttemp, n.ty));
  case A_OR:
    return (cgor(lefttemp, righttemp, n.ty));
  case A_XOR:
    return (cgxor(lefttemp, righttemp, n.ty));
  case A_LSHIFT:
    return (cgshl(lefttemp, righttemp, n.ty));
  case A_RSHIFT:
    return (cgshr(lefttemp, righttemp, n.ty));
  case A_NOT:
    return (cgnot(lefttemp, n.ty));
  case A_GLUE:
    return (NOTEMP);
  case A_RETURN:
    cgreturn(lefttemp, Thisfunction.ty);
    return (NOTEMP);
  case A_ABORT:
    cgabort();
    return (NOTEMP);
  case A_STRLIT:
    label = add_strlit(n.strlit, n.is_const);
    return (cgloadglobstr(label));
  case A_ADDR:
    return (cgaddress(n.sym));
  case A_DEREF:
    // If we are an rvalue, dereference to get the value we point at,
    // otherwise leave it for A_ASSIGN to store through the pointer
    if (n.rvalue == true)
      return (cgderef(lefttemp, value_at(n.left.ty)));
    else
      return (lefttemp);
  case A_BREAK:
    // Make sure we have a label to jump to
    if (Breakhead == NULL)
      fatal("Can only break within a loop\n");
    cgjump(Breakhead.break_label);
    // QBE needs a label after a jump
    cglabel(genlabel());
    return (NOTEMP);
  case A_CONTINUE:
    // Make sure we have a label to jump to
    if (Breakhead == NULL)
      fatal("Can only continue within a loop\n");
    cgjump(Breakhead.continue_label);
    // QBE needs a label after a jump
    cglabel(genlabel());
    return (NOTEMP);
  case A_SCALE:
    // At some point, add an optimisation
    // to use shifts instead of multiply when
    // the scale size is 2, 4 or 8.
    //
    // Get a temp with the size to scale
    temp = cgloadlit(&(n.litval), ty_int64);
    return (cgmul(lefttemp, temp, n.ty));
  case A_FALLTHRU:
    if (Switchhead == NULL)
      fatal("Cannot fallthru when not in a switch statement\n");
    cgjump(Switchhead.next_label);
    // QBE needs a label after a jump
    cglabel(genlabel());
    return (NOTEMP);
  case A_BOUNDS:
    label = add_strlit(n.strlit, n.is_const);
    temp = add_strlit(Thisfunction.name, true);
    return (cgboundscheck(lefttemp, n.count, label, temp));
  case A_VASTART:
    cg_vastart(n);
    return (NOTEMP);
  case A_VAEND:
    cg_vaend(n);
    return (NOTEMP);
  case A_VAARG:
    return(cg_vaarg(n));
  }

  // Error
  fatal("genAST() unknown op %d\n", n.op);
  return (NOTEMP);
}

// Generate the code for an IF statement
// and an optional ELSE clause.
void gen_IF(ASTnode * n) {
  int Lfalse;
  int Lend = 0;
  int t1;

  // Generate two labels: one for the
  // false compound statement, and one
  // for the end of the overall IF statement.
  // When there is no ELSE clause, Lfalse
  // _is_ the ending label!
  Lfalse = genlabel();
  if (n.right != NULL)
    Lend = genlabel();

  // Generate the condition code
  t1 = genAST(n.left);

  // Jump if false to the false label
  cgjump_if_false(t1, Lfalse);

  // Generate the true statement block
  genAST(n.mid);

  // If there is an optional ELSE clause,
  // generate the jump to skip to the end
  if (n.right != NULL) {
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
  if (n.right != NULL) {
    genAST(n.right);
    cglabel(Lend);
  }
}

// Generate the code for a WHILE statement
void gen_WHILE(ASTnode * n, int for_label) {
  Breaklabel *this;
  int Lstart;
  int Lend;
  int t1;

  // Generate the start and end labels
  // and output the start label
  Lstart = genlabel();
  Lend = genlabel();
  cglabel(Lstart);

  // Push the start and end labels on the Breaklabel stack.
  // Use the for_label as the continue label if not zero
  this = Malloc(sizeof(Breaklabel));
  if (for_label != 0)
    this.continue_label = for_label;
  else
    this.continue_label = Lstart;
  this.break_label = Lend;
  this.prev = Breakhead;
  Breakhead = this;

  // Generate the condition code but only
  // if the condition isn't a TRUE node
  if (n.op != A_NUMLIT) {
    t1 = genAST(n.left);

    // Jump if false to the end label
    cgjump_if_false(t1, Lend);
  }

  // Generate the statement block for the WHILE body
  genAST(n.mid);

  // Finally output the jump back to the condition,
  // and the end label
  cgjump(Lstart);
  cglabel(Lend);

  // And pop the Breaklabel node from the stack
  Breakhead = this.prev;
}

// Generate space for a local variable
// and assign its value
void gen_local(ASTnode * n) {
  int lefttemp;
  int size;
  bool makezero = true;
  bool isarray = false;

  // Get the variable's size
  size = n.ty.size;

  // An array? Multiply by the # of elements
  if (is_array(n.sym)) {
    isarray = true;
    size = size * n.count;
  }

  // We have an initialisation
  // no need to zero the space
  if (n.left != NULL)
    makezero = false;

  // Allocate space for the variable
  cgaddlocal(n.ty, n.sym, size, makezero, isarray);

  // Get the expression's value
  // on the left if there is one
  if (n.left != NULL) {
    lefttemp = genAST(n.left);

    // Store this into the local variable
    cgstorvar(lefttemp, n.ty, n.sym);
  }

  // Generate any code for the other children
  genAST(n.mid);
  genAST(n.right);
}

// Generate the argument values for a function
// call and then perform the call itself.
// Return any value into a temporary.
//
int gen_funccall(ASTnode * n) {
  Sym *func;
  Sym *param;
  ASTnode *this;
  ASTnode *node;
  Litval zero;
  Type **typelist = NULL;
  int *arglist = NULL;
  int i;
  int numargs = 0;
  int excepttemp = NOTEMP;
  int return_temp;
  int zerotemp;
  bool func_throws;

  // Get the matching symbol for the function's name
  func = find_symbol(n.left.strlit);
  if (func == NULL)
    fatal("Unknown function %s()\n", n.left.strlit);

  if (func.symtype != ST_FUNCTION)
    fatal("%s is not a function\n", n.left.strlit);

  // Cache if the function throws an exception
  func_throws = (func.exceptvar != NULL);

  // If the function throws an exception, we had better
  // be in a try or catch clause
  if (func_throws && (Ehead == NULL))
    fatal("Must call %s() in a try or catch clause\n", n.left.strlit);

  // Walk the expression list to count the number of arguments
  for (this = n.right; this != NULL; this = this.right) {
    if (this.op == A_GLUE || this.op == A_ASSIGN)
      numargs++;
  }

  // Check the arg count vs. the function parameter count.
  // Allow more arguments if the function is variadic
  if ((numargs < func.count) ||
	((func.is_variadic == false) && (numargs > func.count)))
    fatal("Wrong number of arguments to %s(): %d vs. %d\n",
	  n.left.strlit, numargs, func.count);

  if (numargs > 0) {
    // Allocate space to hold the types and
    // temporaries for the expressions
    arglist = Malloc(numargs * sizeof(int));
    typelist = Malloc(numargs * sizeof(Type *));

    if (arglist == NULL || typelist == NULL)
      fatal("Out of memory in gen_funccall()\n");

    // Do we have a named expression list?
    if (n.right.op == A_ASSIGN) {

      // Can't do this with a variadic function
      if (func.is_variadic == true)
	fatal("Cannot use named argument with a variadic function\n");

      // Walk the function's parameter list and set count zero for each.
      // We use this as a flag to tell if a param name gets used again.
      for (param = func.paramlist; param != NULL; param = param.next)
	param.count = 0;

      // Walk the function's parameter list again
      i=0;
      for (param = func.paramlist; param != NULL; param = param.next) {
	// Find the named expression that matches the parameter name
	for (this = n.right; this != NULL; this = this.right) {
	  if (strcmp(param.name, this.strlit)==0) {

	    // See if we have already used this parameter name
	    if (param.count == 1)
	      fatal("Parameter %s used multiple times\n", param.name);

	    // Check and, if needed, widen the expression's
	    // type to match the parameter's type.
	    // Generate the code for each expression.
	    // Cache the temporary number and the type for each one.
	    param.count = 1;
	    this.left = widen_expression(this.left, param.ty);
	    typelist[i] = this.left.ty;
	    arglist[i] = genAST(this.left);
	  }
	}
	i++;
      }
    } else {

      // No, it's only a normal expression list.
      // Walk the expression list again.
      // Check and, if needed, widen the expression's
      // type to match the parameter's type.
      // Generate the code for each expression.
      // Cache the temporary number and the type for each one.
      param = func.paramlist;
      i = 0;
      for (this = n.right; this != NULL; this = this.right) {
	if (this.op == A_GLUE)
	  node = this.left;
	else
	  node = this;

	// Widen the expression to match the parameter type
	if (param != NULL) {
	  node = widen_expression(node, param.ty);
	} else {
	  // No parameter, so this is a variadic argument.
	  // On x64, widen ints to at least int32 and flt32s to flt64
	  if (is_integer(node.ty) && (node.ty.kind < TY_INT32)) {
	    if (node.ty.is_unsigned == true)
	      node = widen_type(node, ty_uint32, 0);
	    else
	      node = widen_type(node, ty_int32, 0);
	  }
	  if (is_flonum(node.ty) && (node.ty.kind == TY_FLT32))
	    node = widen_type(node, ty_flt64, 0);
	}

	// Put the type and the temporary into the list
	typelist[i] = node.ty;
	arglist[i] = genAST(node);

	// Move up to the next parameter
	if (param != NULL)
	  param = param.next;
	i++;
      }
    }
  }

  // If we have an exception variable
  // and the function throws an exception,
  // get its address into a temporary
  if (func_throws) {
    excepttemp = cgaddress(Ehead.sym);

    // Get a literal zero into a temporary
    zero.intval = 0;
    zerotemp = cgloadlit(&zero, ty_int32);

    // Set the exception variable's first member to zero
    cgstorderef(zerotemp, excepttemp, ty_int32);
  }

  // Generate the QBE code for the function call
  return_temp = cgcall(func, numargs, excepttemp, arglist, typelist);

  // If we are in a try clause, test if the first
  // member of the exception variable is not zero.
  // If not, jump to the catch clause
  if (func_throws && (Ehead != NULL) && (Ehead.in_try == true)) {

    // Get the value of the first member in the exception variable
    excepttemp = cgderef(excepttemp, ty_int32);

    // Compare the first member against zero
    excepttemp = cgcompare(A_EQ, excepttemp, zerotemp, ty_int32);

    // Jump if false to the catch label
    cgjump_if_false(excepttemp, Ehead.Lcatch);
  }

  // Otherwise, return any value from the function call
  return (return_temp);
}

void gen_try(ASTnode * n) {
  int Lcatch;
  int Lend;
  Edetails *this;

  // Generate the labels for the start
  // and end of the catch clause
  Lcatch = genlabel();
  Lend = genlabel();

  // Make an Edetails node for this try statement
  // and fill it in
  this = Malloc(sizeof(Edetails));
  this.sym = n.sym;
  this.Lcatch = Lcatch;
  this.in_try = true;

  // Push the node on the stack
  this.prev = Ehead;
  Ehead = this;

  // Generate the code for the try clause
  // and jump past the catch clause
  genAST(n.left);
  cgjump(Lend);

  // Output the label for the catch clause,
  // then the catch code, then the end label
  this.in_try = false;
  cglabel(Lcatch);
  genAST(n.right);
  cglabel(Lend);

  // Finally remove the Edetails node
  Ehead = Ehead.prev;
}

// Generate the code for a SWITCH statement
void gen_SWITCH(ASTnode * n) {
  int *caselabel;
  int *codelabel;
  int i;
  int Lend;
  int temp;
  int t2;
  Type *ty;
  ASTnode *c;
  Switchlabel *this;

  // Build a Switchlabel node and push it on to
  // the stack of Switchlabels
  this = Malloc(sizeof(Switchlabel));
  this.prev = Switchhead;
  Switchhead = this;

  // Create an array for the case testing labels
  // and an array for the case code labels
  caselabel = Malloc((n.litval.intval + 1) * sizeof(int));
  codelabel = Malloc((n.litval.intval + 1) * sizeof(int));

  // Because QBE doesn't yet support jump tables,
  // we simply evaluate the switch condition and
  // then do successive comparisons and jumps,
  // just like we were doing successive if/elses

  // Generate a label for the end of the switch statement.
  Lend = genlabel();

  // Generate labels for each case. Put the end label
  // in as the entry after all the cases
  i = 0;
  for (c = n.right; c != NULL; c = c.right) {
    caselabel[i] = genlabel();
    codelabel[i] = genlabel();
    i++;
  }
  codelabel[i] = Lend;
  caselabel[i] = Lend;

  // Output the code to calculate the switch condition.
  // Get the type so we can widen the case values
  temp = genAST(n.left);
  ty = n.left.ty;

  // Walk the right-child linked list
  // to generate the code for each case
  i = 0;
  for (c = n.right; c != NULL; c = c.right) {

    // Output the label for this case's test
    cglabel(caselabel[i]);

    // If this is not the default case
    if (c.op != A_DEFAULT) {
      // Jump to the next case test if the value doesn't match the case value
      t2 = cgloadlit(&(c.litval), ty);
      t2 = cgcompare(A_EQ, temp, t2, ty);
      cgjump_if_false(t2, caselabel[i + 1]);

      // Otherwise, jump to the code to handle this case
      cgjump(codelabel[i]);
    }

    // Output the label for this case's code
    cglabel(codelabel[i]);

    // If the case has no body, jump to the following case's body
    if (c.left == NULL) {
      cgjump(codelabel[i + 1]);
    } else {
      // Before we generate the code, update the Switchlabel
      // to have the label for the next case code, in
      // case we do a fallthrough in the body
      Switchhead.next_label = codelabel[i + 1];

      // Generate the case code
      genAST(c.left);

      // Always jump to the end of the switch (no fallthrough)
      cgjump(Lend);
    }
    i++;
  }

  // Now output the end label and pull the Switchlabel from the stack
  cglabel(Lend);
  Switchhead = Switchhead.prev;
  return;
}

// Generate the code for an
// A_LOGAND or A_LOGOR operation
int gen_logandor(ASTnode * n) {

  // Generate several labels
  int Lright = genlabel();
  int Lfalse = genlabel();
  int Ltrue = genlabel();
  int Lend = genlabel();
  int temp;

  if (n.op == A_LOGAND) {
    // Lazy AND evaluation. Do the left side, jump if false.
    // If not, do the right side, jump if false.
    // If we didn't jump, the result is true.
    temp = genAST(n.left);
    cgjump_if_false(temp, Lfalse);
    temp = genAST(n.right);
    cgjump_if_false(temp, Lfalse);
  } else {
    // Lazy OR evaluation. Do the left side. If false,
    // do the right side, else the result is true.
    // If we jumped to Lright, do the right side.
    // If false, the result is false.
    temp = genAST(n.left);
    cgjump_if_false(temp, Lright);
    cgjump(Ltrue);
    cglabel(Lright);
    temp = genAST(n.right);
    cgjump_if_false(temp, Lfalse);
    cglabel(Ltrue);
  }

  cgloadboolean(temp, 1, n.left.ty);
  cgjump(Lend);
  cglabel(Lfalse);
  cgloadboolean(temp, 0, n.left.ty);
  cglabel(Lend);
  return (temp);
}

int gen_ternary(ASTnode * n) {
  int t;
  int expr;
  int result;
  int Lfalse;
  int Lend;

  // Generate two labels: one for the
  // false expression, and one for the
  // end of the overall expression
  Lfalse = genlabel();
  Lend = genlabel();

  // Get a temporary to hold the result of the two expressions
  result = cgalloctemp();

  // Generate the condition code
  t = genAST(n.left);

  // Jump if false to the false label
  cgjump_if_false(t, Lfalse);

  // Generate the true expression and the false label.
  expr = genAST(n.mid);
  cgmove(expr, result, n.mid.ty);
  cgjump(Lend);
  cglabel(Lfalse);

  // Generate the false expression and the end label.
  expr = genAST(n.right);
  cgmove(expr, result, n.right.ty);
  cglabel(Lend);

  return (result);
}

public int genalign(const Type * ty, const int offset) {
  return (cgalign(ty, offset));
}

public void gen_file_preamble(void) {
  cg_file_preamble();
}

public void gen_func_preamble(const Sym * func) {
  cg_func_preamble(func);
}

public void gen_func_postamble(const Type * ty) {
  cg_func_postamble(ty);
}

int gen_cast(ASTnode * n) {
  int exprtemp = genAST(n.left);
  int functemp = add_strlit(Thisfunction.name, true);
  return(cgcast(exprtemp, n.left.ty, n.ty, functemp));
}
