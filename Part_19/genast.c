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

static Edetails *Ehead = NULL;	// The stack of Edetail nodes

static Breaklabel *Breakhead = NULL;	// The stack of Breaklabel nodes

// We keep a stack of "next case"
// labels for switch statements
typedef struct Switchlabel Switchlabel;
struct Switchlabel {
  int next_label;
  Switchlabel *prev;
};

static Switchlabel *Switchhead = NULL;	// The stack of Switchlabel nodes

static void gen_IF(ASTnode * n);
static void gen_WHILE(ASTnode * n, int forlabel);
static void gen_SWITCH(ASTnode * n);
static void gen_local(ASTnode * n);
static int gen_funccall(ASTnode * n);
static void gen_try(ASTnode * n);
static int gen_ternary(ASTnode * n);
static int gen_logandor(ASTnode * n);
static int gen_cast(ASTnode * n);
static int gen_aarray(ASTnode * n, int exprtemp, Type *ty);
static int gen_exists(ASTnode * n);
static int gen_undef(ASTnode * n);
static int gen_aaiterstart(ASTnode * n);
static int gen_aanext(ASTnode * n);

// Generate and return a new label number
static int labelid = 1;
int genlabel(void) {
  labelid++;
  return (labelid);
}

// Given an AST, generate assembly code recursively.
// Return the temporary id with the tree's final value.
int genAST(ASTnode * n) {
  int lefttemp, righttemp;
  int functemp;
  int temp;
  int label;
  Breaklabel *this;

  // Empty tree, do nothing
  if (n == NULL)
    return (NOTEMP);

  // Do special case nodes before the general processing
  switch (n->op) {
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
  case A_AARRAY:
    return (gen_aarray(n, NOTEMP, NULL));
  case A_UNDEF:
    return (gen_undef(n));
  case A_EXISTS:
    return (gen_exists(n));
  case A_AAFREE:
    return (cg_free_aarray(n->sym));
  case A_FUNCITER:
    // Add a Breaklabel node
    this = (Breaklabel *) Malloc(sizeof(Breaklabel));
    this->continue_label= genlabel();
    this->break_label= genlabel();
    this->prev = Breakhead;
    Breakhead = this;
    cg_funciterator(n, this);
    Breakhead = this->prev;
    return(NOTEMP);
  case A_STRINGITER:
    // Add a Breaklabel node
    this = (Breaklabel *) Malloc(sizeof(Breaklabel));
    this->continue_label= genlabel();
    this->break_label= genlabel();
    this->prev = Breakhead;
    Breakhead = this;
    cg_stringiterator(n, this);
    // Remove the Breaklabel node
    Breakhead = this->prev;
    return(NOTEMP);
  case A_FOR:
    // Generate the initial code
    genAST(n->right);

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
  if (n->left)
    lefttemp = genAST(n->left);

  if ((n->op == A_GLUE) && (n->is_short_assign == true)) {
    if (Breakhead == NULL)
      fatal("NULL Breakhead trying to generate FOR continue label\n");
    cglabel(Breakhead->continue_label);
  }

  if (n->right)
    righttemp = genAST(n->right);

  // General processing
  switch (n->op) {
  case A_NUMLIT:
    return (cgloadlit(&(n->litval), n->type));
  case A_ADD:
    return (cgadd(lefttemp, righttemp, n->type));
  case A_ADDOFFSET:
    // Do a runtime check on a string's length
    if (n->type == ty_string) {
      functemp = add_strlit(Thisfunction->name, true);
      cg_stridxcheck(lefttemp, righttemp, functemp);
    }
    return (cgadd(lefttemp, righttemp, n->type));
  case A_SUBTRACT:
    return (cgsub(lefttemp, righttemp, n->type));
  case A_MULTIPLY:
    return (cgmul(lefttemp, righttemp, n->type));
  case A_MOD:
    return (cgmod(lefttemp, righttemp, n->type));
  case A_DIVIDE:
    return (cgdiv(lefttemp, righttemp, n->type));
  case A_NEGATE:
    return (cgnegate(lefttemp, n->type));
  case A_IDENT:
    // Load our value if we are an rvalue
    if (n->rvalue == true)
      return (cgloadvar(n->sym));
    return (NOTEMP);
  case A_ASSIGN:
    return(gen_assign(lefttemp, righttemp, n));
  case A_WIDEN:
    functemp = add_strlit(Thisfunction->name, true);
    return (cgcast(lefttemp, n->left->type, n->type, functemp));
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
    return (NOTEMP);
  case A_RETURN:
    // If the return type has a range, check the value
    if (has_range(Thisfunction->type)) {
      functemp = add_strlit(Thisfunction->name, true);
      cgrangecheck(lefttemp, Thisfunction->type, functemp);
    }
    cgreturn(lefttemp, Thisfunction->type);
    return (NOTEMP);
  case A_ABORT:
    cgabort();
    return (NOTEMP);
  case A_STRLIT:
    label = add_strlit(n->strlit, n->is_const);
    return (cgloadglobstr(label));
  case A_ADDR:
    return (cgaddress(n->sym));
  case A_DEREF:
    // If we are an rvalue, dereference to get the value we point at,
    // otherwise leave it for A_ASSIGN to store through the pointer
    if (n->rvalue == true)
      return (cgderef(lefttemp, value_at(n->left->type)));
    else
      return (lefttemp);
  case A_BREAK:
    // Make sure we have a label to jump to
    if (Breakhead == NULL)
      lfatal(n->line, "Can only break within a loop\n");
    cgjump(Breakhead->break_label);
    // QBE needs a label after a jump
    cglabel(genlabel());
    return (NOTEMP);
  case A_CONTINUE:
    // Make sure we have a label to jump to
    if (Breakhead == NULL)
      lfatal(n->line, "Can only continue within a loop\n");
    cgjump(Breakhead->continue_label);
    // QBE needs a label after a jump
    cglabel(genlabel());
    return (NOTEMP);
  case A_SCALE:
    // At some point, add an optimisation
    // to use shifts instead of multiply when
    // the scale size is 2, 4 or 8.
    //
    // Get a temp with the size to scale
    temp = cgloadlit(&(n->litval), ty_int64);
    return (cgmul(lefttemp, temp, n->type));
  case A_FALLTHRU:
    if (Switchhead == NULL)
      lfatal(n->line, "Cannot fallthru when not in a switch statement\n");
    cgjump(Switchhead->next_label);
    // QBE needs a label after a jump
    cglabel(genlabel());
    return (NOTEMP);
  case A_BOUNDS:
    label = add_strlit(n->strlit, n->is_const);
    temp = add_strlit(Thisfunction->name, true);
    return (cgboundscheck(lefttemp, n->count, label, temp));
  case A_VASTART:
    cg_vastart(n);
    return (NOTEMP);
  case A_VAEND:
    cg_vaend(n);
    return (NOTEMP);
  case A_VAARG:
    return(cg_vaarg(n));
  case A_AAITERSTART:
    return(gen_aaiterstart(n));
  case A_AANEXT:
    return(gen_aanext(n));
  }

  // Error
  lfatal(n->line, "genAST() unknown op %d\n", n->op);
  return (NOTEMP);
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
static void gen_WHILE(ASTnode * n, int for_label) {
  Breaklabel *this;
  int Lstart, Lend;
  int t1;

  // Generate the start and end labels
  // and output the start label
  Lstart = genlabel();
  Lend = genlabel();
  cglabel(Lstart);

  // Push the start and end labels on the Breaklabel stack.
  // Use the for_label as the continue label if not zero
  this = (Breaklabel *) Malloc(sizeof(Breaklabel));
  if (for_label != 0)
    this->continue_label = for_label;
  else
    this->continue_label = Lstart;
  this->break_label = Lend;
  this->prev = Breakhead;
  Breakhead = this;

  // Generate the condition code but only
  // if the condition isn't a TRUE node
  if (n->op != A_NUMLIT) {
    t1 = genAST(n->left);

    // Jump if false to the end label
    cgjump_if_false(t1, Lend);
  }

  // Generate the statement block for the WHILE body
  genAST(n->mid);

  // Finally output the jump back to the condition,
  // and the end label
  cgjump(Lstart);
  cglabel(Lend);

  // And pop the Breaklabel node from the stack
  Breakhead = this->prev;
}

// Generate space for a local variable
// and assign its value
void gen_local(ASTnode * n) {
  int lefttemp;
  int size;
  bool makezero = true;
  bool isarray = false;

  // Get the variable's size
  size = n->type->size;

  // An array? Multiply by the # of elements
  if (is_array(n->sym)) {
    isarray = true;
    size = size * n->count;
  }

  // We have an initialisation
  // no need to zero the space
  if (n->left != NULL)
    makezero = false;

  // Allocate space for the variable
  cgaddlocal(n->type, n->sym, size, makezero, isarray);

  // Get the expression's value
  // on the left if there is one
  if (n->left != NULL) {
    lefttemp = genAST(n->left);

    // Store this into the local variable
    cgstorvar(lefttemp, n->type, n->sym);
  }

  // Generate any code for the other children
  genAST(n->mid);
  genAST(n->right);
}

// Given a parameter's type and inout flag,
// and an ASTnode which is the argument,
// return the node to match the parameter's type
static ASTnode *fixup_argument(Type* paramtype, bool is_inout, ASTnode * node) {
  // If we still have a parameter with a type
  if (paramtype != NULL) {

    // If this is an inout parameter
    if (is_inout) {
      // Ensure the parameter's type is a pointer
      // to the node's type
      if (paramtype != pointer_to(node->type))
	fatal("inout argument not of type %s\n",
	      get_typename(value_at(paramtype)));

      // Get the node's addess or, if not, an error.
      // This code echoes unary_expression()
      switch (node->op) {
      case A_DEREF:
	node = node->left;	// Remove an A_DEREF
	break;
      case A_IDENT:		// Change to ADDR
	node->op = A_ADDR;
	break;
      case A_ADDOFFSET:
	break;
      default:
	fatal("inout argument has no address\n");
      }
      node->type = paramtype;
    } else {
      // Widen the expression to match the parameter type
      node = widen_expression(node, paramtype);
    }
  } else {
    // No parameter, so this is a variadic argument.
    // On x64, widen ints to at least int32 and flt32s to flt64
    if (is_integer(node->type) && (node->type->kind < TY_INT32)) {
      if (node->type->is_unsigned == true)
	node = widen_type(node, ty_uint32, 0);
      else
	node = widen_type(node, ty_int32, 0);
    }
    if (is_flonum(node->type) && (node->type->kind == TY_FLT32))
      node = widen_type(node, ty_flt64, 0);
  }
  return (node);
}

// Generate the argument values for a function
// call and then perform the call itself.
// Return any value into a temporary.
//
static int gen_funccall(ASTnode * n) {
  Sym *func, *param;
  Paramtype *ptype;
  ASTnode *this, *node;
  Litval zero;
  Type **typelist = NULL;
  int *arglist = NULL;
  int i, numargs = 0;
  int excepttemp = NOTEMP;
  int return_temp;
  int zerotemp;
  bool func_throws;

  // Get the matching symbol for the function's name
  func = n->sym;
  if (func->symtype != ST_FUNCTION && func->type->kind != TY_FUNCPTR)
    lfatal(n->line, "%s is not a function\n", n->left->strlit);

  // Cache if the function throws an exception
  func_throws = (func->exceptvar != NULL);

  // If the function throws an exception, we had better
  // be in a try or catch clause
  if (func_throws && (Ehead == NULL))
    lfatal(n->line, "Must call %s() in a try or catch clause\n",
	n->left->strlit);

  // Walk the expression list to count the number of arguments
  for (this = n->right; this != NULL; this = this->right) {
    if (this->op == A_GLUE || this->op == A_ASSIGN)
      numargs++;
  }

  // For function pointers, count the number of parameters
  if (func->type->kind == TY_FUNCPTR)
    for (ptype= func->type->paramtype; ptype != NULL; ptype= ptype->next)
      func->count++;

  // Check the arg count vs. the function parameter count.
  // Allow more arguments if the function is variadic
  if ((numargs < func->count) ||
	((func->is_variadic == false) && (numargs > func->count)))
    lfatal(n->line, "Wrong number of arguments to %s(): %d vs. %d\n",
	  n->left->strlit, numargs, func->count);

  if (numargs > 0) {
    // Allocate space to hold the types and
    // temporaries for the expressions
    arglist = (int *) Malloc(numargs * sizeof(int));
    typelist = (Type **) Malloc(numargs * sizeof(Type *));

    if (arglist == NULL || typelist == NULL)
      lfatal(n->line, "Out of memory in gen_funccall()\n");

    // Do we have a function pointer?
    if (func->type->kind == TY_FUNCPTR) {
      // Walk the parameter type list
      ptype= func->type->paramtype;
      for (i = 0, this = n->right; this != NULL; this = this->right, i++) {
	if (this->op == A_GLUE)
	  node = this->left;
	else
	  node = this;

	// Make the node match the parameter
	if (ptype==NULL)
	  node= fixup_argument(NULL, false, node);
	else
	  node= fixup_argument(ptype->type, ptype->is_inout, node);

	// Put the type and the temporary into the list
	typelist[i] = node->type;
	arglist[i] = genAST(node);

	// Move up to the next parameter type
	if (ptype != NULL)
	  ptype = ptype->next;
      }
           // Do we have a named expression list?
    } else if (n->right->op == A_ASSIGN) {

      // Can't do this with a variadic function
      if (func->is_variadic == true)
	lfatal(n->line, "Cannot use named argument with a variadic function\n");

      // Walk the function's parameter list and set count zero for each.
      // We use this as a flag to tell if a param name gets used again.
      for (param = func->paramlist; param != NULL; param = param->next)
	param->count = 0;

      // Walk the function's parameter list again
      for (i = 0, param = func->paramlist; param != NULL;
	   i++, param = param->next) {
	// Find the named expression that matches the parameter name
	for (this = n->right; this != NULL; this = this->right) {
	  if (!strcmp(param->name, this->strlit)) {

	    // See if we have already used this parameter name.
	    // Mark it as being used
	    if (param->count == 1)
	      lfatal(n->line, "Parameter %s used multiple times\n", param->name);
	    param->count = 1;

	    // Make the node match the parameter
	    this->left= fixup_argument(param->type, param->is_inout, this->left);

	    // Put the type and the temporary into the list
	    typelist[i] = this->left->type;
	    arglist[i] = genAST(this->left);
	  }
	}
      }
    } else {

      // No, it's only a normal expression list.
      // Walk the expression list again.
      // Check and, if needed, widen the expression's
      // type to match the parameter's type.
      // Generate the code for each expression.
      // Cache the temporary number and the type for each one.
      param = func->paramlist;
      for (i = 0, this = n->right; this != NULL; this = this->right, i++) {
	if (this->op == A_GLUE)
	  node = this->left;
	else
	  node = this;

	// Make the node match the parameter
	if (param==NULL)
	  node= fixup_argument(NULL, false, node);
	else
	  node= fixup_argument(param->type, param->is_inout, node);

	// Put the type and the temporary into the list
	typelist[i] = node->type;
	arglist[i] = genAST(node);

	// Move up to the next parameter
	if (param != NULL)
	  param = param->next;
      }
    }
  }

  // If we have an exception variable
  // and the function throws an exception,
  // get its address into a temporary
  if (func_throws) {
    excepttemp = cgaddress(Ehead->sym);

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
  if (func_throws && (Ehead != NULL) && (Ehead->in_try == true)) {

    // Get the value of the first member in the exception variable
    excepttemp = cgderef(excepttemp, ty_int32);

    // Compare the first member against zero
    excepttemp = cgcompare(A_EQ, excepttemp, zerotemp, ty_int32);

    // Jump if false to the catch label
    cgjump_if_false(excepttemp, Ehead->Lcatch);
  }

  // Otherwise, return any value from the function call
  return (return_temp);
}

static void gen_try(ASTnode * n) {
  int Lcatch, Lend;
  Edetails *this;

  // Generate the labels for the start
  // and end of the catch clause
  Lcatch = genlabel();
  Lend = genlabel();

  // Make an Edetails node for this try statement
  // and fill it in
  this = (Edetails *) Malloc(sizeof(Edetails));
  this->sym = n->sym;
  this->Lcatch = Lcatch;
  this->in_try = true;

  // Push the node on the stack
  this->prev = Ehead;
  Ehead = this;

  // Generate the code for the try clause
  // and jump past the catch clause
  genAST(n->left);
  cgjump(Lend);

  // Output the label for the catch clause,
  // then the catch code, then the end label
  this->in_try = false;
  cglabel(Lcatch);
  genAST(n->right);
  cglabel(Lend);

  // Finally remove the Edetails node
  Ehead = Ehead->prev;
}

// Generate the code for a SWITCH statement
static void gen_SWITCH(ASTnode * n) {
  int *caselabel;
  int *codelabel;
  int i, Lend;
  int temp, t2;
  Type *ty;
  ASTnode *c;
  Switchlabel *this;

  // Build a Switchlabel node and push it on to
  // the stack of Switchlabels
  this = (Switchlabel *) Malloc(sizeof(Switchlabel));
  this->prev = Switchhead;
  Switchhead = this;

  // Create an array for the case testing labels
  // and an array for the case code labels
  caselabel = (int *) Malloc((n->litval.intval + 1) * sizeof(int));
  codelabel = (int *) Malloc((n->litval.intval + 1) * sizeof(int));

  // Because QBE doesn't yet support jump tables,
  // we simply evaluate the switch condition and
  // then do successive comparisons and jumps,
  // just like we were doing successive if/elses

  // Generate a label for the end of the switch statement.
  Lend = genlabel();

  // Generate labels for each case. Put the end label
  // in as the entry after all the cases
  for (i = 0, c = n->right; c != NULL; i++, c = c->right) {
    caselabel[i] = genlabel();
    codelabel[i] = genlabel();
  }
  caselabel[i] = codelabel[i] = Lend;

  // Output the code to calculate the switch condition.
  // Get the type so we can widen the case values.
  // If the type is a string (int8 *) then hash the
  // string value and change the type to be uint64.
  temp = genAST(n->left);
  if (n->left->type == pointer_to(ty_int8)) {
    temp= cg_strhash(temp);
    n->left->type = ty_uint64;
  }
  ty = n->left->type;

  // Walk the right-child linked list
  // to generate the code for each case
  for (i = 0, c = n->right; c != NULL; i++, c = c->right) {

    // Output the label for this case's test
    cglabel(caselabel[i]);

    // If this is not the default case
    if (c->op != A_DEFAULT) {
      // Jump to the next case test if the value doesn't match the case value
      t2 = cgloadlit(&(c->litval), ty);
      t2 = cgcompare(A_EQ, temp, t2, ty);
      cgjump_if_false(t2, caselabel[i + 1]);

      // Otherwise, jump to the code to handle this case
      cgjump(codelabel[i]);
    }

    // Output the label for this case's code
    cglabel(codelabel[i]);

    // If the case has no body, jump to the following case's body
    if (c->left == NULL) {
      cgjump(codelabel[i + 1]);
    } else {
      // Before we generate the code, update the Switchlabel
      // to have the label for the next case code, in
      // case we do a fallthrough in the body
      Switchhead->next_label = codelabel[i + 1];

      // Generate the case code
      genAST(c->left);

      // Always jump to the end of the switch (no fallthrough)
      cgjump(Lend);
    }
  }

  // Now output the end label and pull the Switchlabel from the stack
  cglabel(Lend);
  Switchhead = Switchhead->prev;
  return;
}

// Generate the code for an
// A_LOGAND or A_LOGOR operation
static int gen_logandor(ASTnode * n) {

  // Generate several labels
  int Lright = genlabel();
  int Lfalse = genlabel();
  int Ltrue = genlabel();
  int Lend = genlabel();
  int temp;

  if (n->op == A_LOGAND) {
    // Lazy AND evaluation. Do the left side, jump if false.
    // If not, do the right side, jump if false.
    // If we didn't jump, the result is true.
    temp = genAST(n->left);
    cgjump_if_false(temp, Lfalse);
    temp = genAST(n->right);
    cgjump_if_false(temp, Lfalse);
  } else {
    // Lazy OR evaluation. Do the left side. If false,
    // do the right side, else the result is true.
    // If we jumped to Lright, do the right side.
    // If false, the result is false.
    temp = genAST(n->left);
    cgjump_if_false(temp, Lright);
    cgjump(Ltrue);
    cglabel(Lright);
    temp = genAST(n->right);
    cgjump_if_false(temp, Lfalse);
    cglabel(Ltrue);
  }

  cgloadboolean(temp, 1, n->left->type);
  cgjump(Lend);
  cglabel(Lfalse);
  cgloadboolean(temp, 0, n->left->type);
  cglabel(Lend);
  return (temp);
}

static int gen_ternary(ASTnode * n) {
  int t;
  int expr, result;
  int Lfalse, Lend;

  // Generate two labels: one for the
  // false expression, and one for the
  // end of the overall expression
  Lfalse = genlabel();
  Lend = genlabel();

  // Get a temporary to hold the result of the two expressions
  result = cgalloctemp();

  // Generate the condition code
  t = genAST(n->left);

  // Jump if false to the false label
  cgjump_if_false(t, Lfalse);

  // Generate the true expression and the false label.
  expr = genAST(n->mid);
  cgmove(expr, result, n->mid->type);
  cgjump(Lend);
  cglabel(Lfalse);

  // Generate the false expression and the end label.
  expr = genAST(n->right);
  cgmove(expr, result, n->right->type);
  cglabel(Lend);

  return (result);
}

int genalign(Type * ty, int offset) {
  return (cgalign(ty, offset));
}

void gen_file_preamble(void) {
  cg_file_preamble();
}

void gen_func_preamble(Sym * func) {
  cg_func_preamble(func);
}

void gen_func_postamble(Type * type) {
  cg_func_postamble(type);
}

static int gen_cast(ASTnode * n) {
  int exprtemp = genAST(n->left);
  int functemp = add_strlit(Thisfunction->name, true);
  return(cgcast(exprtemp, n->left->type, n->type, functemp));
}

// Either get the value of a key/value pair from
// an associative array
// or set the value of the key to exprtemp
static int gen_aarray(ASTnode * n, int exprtemp, Type *ty) {
  int keytemp;
  int arytemp;

  // We're an rvalue with no type, do nothing.
  // A_ASSIGN will call us again soon!
  if ((n->rvalue == false) && (ty == NULL))
    return(NOTEMP);

  // Get the pointer to the associative
  // array structure into a temporary
  arytemp= cgloadvar(n->left->sym);

  // Get the key value into a temporary
  keytemp = genAST(n->right);

  // If the key type is ty_string, assume it's a string.
  // Call an external function to get its hash value
  if (n->left->sym->keytype == ty_string) {
    keytemp= cg_strhash(keytemp);
  } else {
    // Otherwise widen the key value to be 64 bits
    keytemp = cgcast(keytemp, n->left->sym->keytype, ty_uint64, NOTEMP);
  }

  // If we have a type, we're an lvalue
  if (ty != NULL) {
    // Save the key/value in the array
    cg_setaaval(arytemp, keytemp, exprtemp, ty);
    return(NOTEMP);
  } else {
    // Look up the value
    return(cg_getaaval(arytemp, keytemp, n->type));
  }
}

static int gen_exists(ASTnode * n) {
  int keytemp;
  int arytemp;

  // Get the pointer to the associative
  // array structure into a temporary
  arytemp= cgloadvar(n->left->left->sym);

  // Get the key value into a temporary
  keytemp = genAST(n->left->right);

  // If the key type is ty_string, assume it's a string.
  // Call an external function to get its hash value
  if (n->left->left->sym->keytype == ty_string) {
    keytemp= cg_strhash(keytemp);
  } else {
    // Otherwise widen the key value to be 64 bits
    keytemp = cgcast(keytemp, n->left->left->sym->keytype, ty_uint64, NOTEMP);
  }

  // Look up the value and return if it exists
  return(cg_existsaaval(arytemp, keytemp));
  return(0);
}

static int gen_undef(ASTnode * n) {
  int keytemp;
  int arytemp;

  // Get the pointer to the associative
  // array structure into a temporary
  arytemp= cgloadvar(n->left->left->sym);

  // Get the key value into a temporary
  keytemp = genAST(n->left->right);

  // If the key type is ty_string, assume it's a string.
  // Call an external function to get its hash value
  if (n->left->left->sym->keytype == ty_string) {
    keytemp= cg_strhash(keytemp);
  } else {
    // Otherwise widen the key value to be 64 bits
    keytemp = cgcast(keytemp, n->left->left->sym->keytype, ty_uint64, NOTEMP);
  }

  // Remove the key and value
  cg_delaaval(arytemp, keytemp);
  return(NOTEMP);
}

// Set up the iteration on an associative array
// and return a pointer to the first value
static int gen_aaiterstart(ASTnode * n) {
  int arytemp;

  // Get the pointer to the associative
  // array structure into a temporary
  arytemp= cgloadvar(n->sym);

  // Call the initialisation function to get the first value
  return(cg_aaiterstart(arytemp));
}

// Iterate and get the next value
// from an associative array
static int gen_aanext(ASTnode * n) {
  int arytemp;

  // Get the pointer to the associative
  // array structure into a temporary
  arytemp= cgloadvar(n->sym);

  // Get the next value
  return(cg_aanext(arytemp));
}

int gen_assign(int ltemp, int rtemp, ASTnode *n) {
  int functemp;

  switch (n->right->op) {
  case A_IDENT:
    // We are assigning to an identifier.

    // If the type has a range, check it
    if (has_range(n->right->type)) {
      functemp = add_strlit(Thisfunction->name, true);
      cgrangecheck(ltemp, n->right->type, functemp);
    }

    return (cgstorvar(ltemp, n->right->type, n->right->sym));
  case A_DEREF:
    // We are assigning though a pointer.
    // If the type has a range, check it
    if (has_range(n->right->type)) {
      functemp = add_strlit(Thisfunction->name, true);
      cgrangecheck(ltemp, n->right->type, functemp);
    }

    return (cgstorderef(ltemp, rtemp, n->right->type));
  case A_AARRAY:
    // We are adding/updating a key/value pair
    // in an associative array
    gen_aarray(n->right, ltemp, n->right->type);
    return(NOTEMP);
  default:
    lfatal(n->line, "Bad A_ASSIGN in genAST()\n");
  }
  return(NOTEMP);
}
