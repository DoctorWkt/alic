// Generate code from an AST tree for the alic compiler.
// (c) 2025 Warren Toomey, GPL3

#include "alic.h"
#include "proto.h"

static void gen_IF(ASTnode * n);
static void gen_WHILE(ASTnode * n, int for_label);
static void gen_local(ASTnode * n);
static ASTnode *fixup_argument(Type * paramtype, bool is_inout,
			       ASTnode * node);
static void gen_call(Sym * func, int numargs, int return_temp, int excepttemp,
		     int varidiac_posn, int *arglist, Type ** typelist);
static int gen_funccall(ASTnode * n);
static void gen_try(ASTnode * n);
static void gen_SWITCH(ASTnode * n);
static int gen_logandor(ASTnode * n);
static int gen_ternary(ASTnode * n);
static int gen_cast(ASTnode * n);
static int gen_aarray(ASTnode * n, int exprtemp, Type * ty);
static int gen_exists(ASTnode * n);
static int gen_undef(ASTnode * n);
static int gen_aaiterstart(ASTnode * n);
static int gen_aanext(ASTnode * n);
static void genglobsymval(ASTnode * value, int offset);
static void gen_arrayiter(ASTnode * n, Breaklabel * this);
static void gen_funciterator(ASTnode * n, Breaklabel * this);
static void gen_stringiterator(ASTnode * n, Breaklabel * this);

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

static int nexttemp = 1;	// Incrementing temporary number

// Allocate a QBE temporary
static int ilalloctemp(void) {
  return (++nexttemp);
}

// Generate and return a new label number
static int labelid = 1;
int genlabel(void) {
  labelid++;
  return (labelid);
}

// Given an alic type, return a suitable IL type.
// Write it to Interfh if output is true.
// If the pointer is NULL, use TY_VOID.
static int iltype(Type * ty, bool output) {
  int ilty;

  if (ty == NULL) ty = ty_void;

  if (is_pointer(ty)) {
    ilty = PTR_TYPE->kind + IL_UNSIGNED;
  } else {
    if (ty->kind > TY_BOOL)
      fatal("%s not an IL type\n", get_typename(ty));
    ilty = ty->kind;
    if (ty->is_unsigned)
      ilty = ilty + IL_UNSIGNED;
  }

  if (output == true) fwrite(&ilty, sizeof(int), 1, Interfh);
  return (ilty);
}

// Output an intermediate language node
static void outIL(int8_t op, Type * dty, Type * sty, int dst, int src,
		  int label, int flags, int count) {
  ILnode n;

  n.op = op;
  n.dtype = iltype(dty, false);
  n.stype = iltype(sty, false);
  n.dst = dst;
  n.src = src;
  n.label = label;
  n.flags = flags;
  n.count = count;
  fwrite(&n, sizeof(ILnode), 1, Interfh);
}

// Output a string to the IL file
static void outILstr(char *str) {
  fputs(str, Interfh);
  fputc(0, Interfh);
}

// Output a string literal value
void genstrlit(Strlit * s) {
  int flags = 0;

  if (s->is_const) flags = flags | IL_ISCONST;
  outIL(A_STRLITVAL, ty_string, NULL, 0, 0, s->label, flags, 0);
  outILstr(s->val);
}

static int iladdress(Sym * sym) {
  int temp = ilalloctemp();
  int flags = 0;

  if (sym->visibility != SV_LOCAL) flags = IL_GLOBAL;
  outIL(A_ADDR, NULL, NULL, temp, 0, NOLABEL, flags, 0);
  outILstr(sym->name);
  return (temp);
}

// Output the IL for a function preamble
void gen_func_preamble(Sym * func) {
  Sym *this;
  int flags = 0;

  // Set the function's IL flags
  if (func->visibility == SV_PUBLIC) flags = IL_PUBLIC;
  if (func->exceptvar != NULL) flags = flags | IL_EXCEPTVAR;
  if (func->is_variadic == true) flags = flags | IL_VARIDIAC;

  // Write the IL node followed by the function's name
  outIL(A_FUNCPREAMBLE, func->type, NULL, 0, 0, 0, flags, func->count);
  outILstr(func->name);

  // If we have an exception variable, output it
  if (func->exceptvar != NULL) {
    iltype(func->exceptvar->type, true);
    outILstr(func->exceptvar->name);
  }

  // Output the list of parameters
  for (this = func->paramlist; this != NULL; this = this->next) {
    iltype(this->type, true);
    outILstr(this->name);
  }
}

static void ilcast(int src, Type * sty, int dst, Type * dty, int functemp) {
  outIL(A_CAST, dty, sty, dst, src, dty->lower, dty->upper, functemp);
}

static void ilcompare(int op, int dst, int src, Type * dty) {
  outIL(op, dty, NULL, dst, src, 0, 0, 0);
}

// Given an AST, generate assembly code recursively.
// Return the temporary id with the tree's final value.
int genAST(ASTnode * n) {
  int lefttemp, righttemp;
  int functemp;
  int temp;
  int label;
  int flags;
  Breaklabel *this;

  // Empty tree, do nothing
  if (n == NULL)
    return (NOTEMP);

  // Do special case nodes before the general processing
  switch (n->op) {
  case A_ASSIGN:
    // If left and right are struct types, use
    // memcpy() to copy them
    if (is_struct(n->left->type) && is_struct(n->right->type)) {
      lefttemp = iladdress(n->left->sym);
      righttemp = iladdress(n->right->sym);
      outIL(A_COPYSTRUCT, NULL, NULL,
	    righttemp, lefttemp, NOLABEL, 0, n->left->type->size);
      return (NOTEMP);
    }

    // If the right-hand side is a function pointer,
    // copy the exception variable from the left
    if (n->right->type->kind == TY_FUNCPTR)
      n->right->sym->exceptvar = n->left->sym->exceptvar;
    break;
  case A_LOCAL:
    gen_local(n); return (NOTEMP);
  case A_FUNCCALL:
    return (gen_funccall(n));
  case A_TRY:
    gen_try(n); return (NOTEMP);
  case A_IF:
    gen_IF(n); return (NOTEMP);
  case A_WHILE:
    gen_WHILE(n, 0); return (NOTEMP);
  case A_SWITCH:
    gen_SWITCH(n); return (NOTEMP);
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
    flags = 0;
    if (n->sym->visibility != SV_LOCAL) flags = flags | IL_GLOBAL;
    outIL(A_AAFREE, NULL, NULL, NOTEMP, NOTEMP, NOLABEL, flags, 0);
    outILstr(n->sym->name);
    return (NOTEMP);
  case A_FUNCITER:
    // Add a Breaklabel node
    this = (Breaklabel *) Malloc(sizeof(Breaklabel));
    this->continue_label = genlabel();
    this->break_label = genlabel();
    this->prev = Breakhead;
    Breakhead = this;
    gen_funciterator(n, this);

    // Remove the Breaklabel node
    Breakhead = this->prev;
    return (NOTEMP);
  case A_STRINGITER:
    // Add a Breaklabel node
    this = (Breaklabel *) Malloc(sizeof(Breaklabel));
    this->continue_label = genlabel();
    this->break_label = genlabel();
    this->prev = Breakhead;
    Breakhead = this;
    gen_stringiterator(n, this);

    // Remove the Breaklabel node
    Breakhead = this->prev;
    return (NOTEMP);
  case A_ARRAYITER:
    // Add a Breaklabel node
    this = (Breaklabel *) Malloc(sizeof(Breaklabel));
    this->continue_label = genlabel();
    this->break_label = genlabel();
    this->prev = Breakhead;
    Breakhead = this;
    gen_arrayiter(n, this);

    // Remove the Breaklabel node
    Breakhead = this->prev;
    return (NOTEMP);
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
    outIL(A_GENLABEL, NULL, NULL, 0, 0, Breakhead->continue_label, 0, 0);
  }

  if (n->right)
    righttemp = genAST(n->right);

  // General processing
  switch (n->op) {
  case A_NUMLIT:
    temp = ilalloctemp();
    outIL(A_LOADLIT, n->type, NULL, temp, 0, NOLABEL, 0, 0);
    fwrite(&(n->litval), sizeof(Litval), 1, Interfh);
    return (temp);
  case A_ADD:
  case A_SUBTRACT:
  case A_MULTIPLY:
  case A_MOD:
  case A_DIVIDE:
  case A_AND:
  case A_OR:
  case A_XOR:
  case A_LSHIFT:
  case A_RSHIFT:
    outIL(n->op, n->type, NULL, lefttemp, righttemp, NOLABEL, 0, 0);
    return (lefttemp);
  case A_ADDOFFSET:
    // Do a runtime check on a string's length
    if (n->type == ty_string) {
      functemp = add_strlit(Thisfunction->name, true);
      outIL(A_STRINDEX, NULL, NULL, lefttemp, righttemp, NOLABEL, 0,
	    functemp);
    }
    outIL(A_ADD, n->type, NULL, lefttemp, righttemp, NOLABEL, 0, 0);
    return (lefttemp);
  case A_NEGATE:
    outIL(A_NEGATE, n->type, NULL, lefttemp, lefttemp, NOLABEL, 0, 0);
    return (lefttemp);
  case A_IDENT:
    // Load our value if we are an rvalue
    if (n->rvalue == true) {
      temp = ilalloctemp();
      flags = 0;
      if (n->sym->visibility != SV_LOCAL)   flags = flags | IL_GLOBAL;
      if (n->sym->has_addr)                 flags = flags | IL_HASADDR;
      if (is_array(n->sym))                 flags = flags | IL_ISARRAY;
      if (n->sym->type->kind == TY_FUNCPTR) flags = flags | IL_ISFUNCPTR;
      if (n->sym->symtype == ST_FUNCTION)   flags = flags | IL_ISFUNCTION;
      outIL(A_LOADVAR, n->sym->type, NULL, temp, 0, 0, flags, 0);
      outILstr(n->sym->name);
      return (temp);
    }
    return (NOTEMP);
  case A_ASSIGN:
    return (gen_assign(lefttemp, righttemp, n));
  case A_WIDEN:
    functemp = add_strlit(Thisfunction->name, true);
    temp = ilalloctemp();
    ilcast(lefttemp, n->left->type, temp, n->type, functemp);
    return (temp);
  case A_EQ:
  case A_NE:
  case A_LT:
  case A_GT:
  case A_LE:
  case A_GE:
    ilcompare(n->op, lefttemp, righttemp, n->left->type);
    return (lefttemp);
  case A_INVERT:
    outIL(A_INVERT, n->type, NULL, lefttemp, lefttemp, NOLABEL, 0, 0);
    return (lefttemp);
  case A_NOT:
    outIL(n->op, n->type, NULL, lefttemp, lefttemp, NOLABEL, 0, 0);
    return (lefttemp);
  case A_GLUE:
    return (NOTEMP);
  case A_RETURN:
    // If the return type has a range, check the value
    if (has_range(Thisfunction->type)) {
      functemp = add_strlit(Thisfunction->name, true);
      outIL(A_RANGE, NULL, Thisfunction->type, Thisfunction->type->lower,
	    lefttemp, NOLABEL, Thisfunction->type->upper, functemp);
    }
    outIL(A_RETURN, Thisfunction->type, NULL, 0, lefttemp, NOLABEL, 0, 0);
    return (NOTEMP);
  case A_ABORT:
    outIL(A_ABORT, NULL, NULL, 0, 0, NOLABEL, 0, 0);
    return (NOTEMP);
  case A_STRLIT:
    label = add_strlit(n->strlit, n->is_const);
    temp = ilalloctemp();
    outIL(A_STRLIT, NULL, NULL, temp, 0, label, 0, 0);
    return (temp);
  case A_ADDR:
    return (iladdress(n->sym));
  case A_DEREF:
    // If we are an rvalue, dereference to get the value we point at,
    // otherwise leave it for A_ASSIGN to store through the pointer
    if (n->rvalue == true) {
      temp = ilalloctemp();
      outIL(A_DEREF, value_at(n->left->type), NULL, temp, lefttemp, NOLABEL,
	    0, 0);
      return (temp);
    } else
      return (lefttemp);
  case A_BREAK:
    // Make sure we have a label to jump to
    if (Breakhead == NULL)
      lfatal(n->line, "Can only break within a loop\n");
    outIL(A_JUMP, NULL, NULL, 0, 0, Breakhead->break_label, 0, 0);
    return (NOTEMP);
  case A_CONTINUE:
    // Make sure we have a label to jump to
    if (Breakhead == NULL)
      lfatal(n->line, "Can only continue within a loop\n");
    outIL(A_JUMP, NULL, NULL, 0, 0, Breakhead->continue_label, 0, 0);
    return (NOTEMP);
  case A_SCALE:
    // At some point, add an optimisation
    // to use shifts instead of multiply when
    // the scale size is 2, 4 or 8.
    //
    // Get a temp with the size to scale
    temp = ilalloctemp();
    outIL(A_LOADLIT, PTR_TYPE, NULL, temp, 0, NOLABEL, 0, 0);
    fwrite(&(n->litval), sizeof(Litval), 1, Interfh);
    outIL(A_MULTIPLY, n->type, NULL, lefttemp, temp, NOLABEL, 0, 0);
    return (lefttemp);
  case A_FALLTHRU:
    if (Switchhead == NULL)
      lfatal(n->line, "Cannot fallthru when not in a switch statement\n");
    outIL(A_JUMP, NULL, NULL, 0, 0, Switchhead->next_label, 0, 0);
#ifndef CPU_pdp11
    // QBE needs a label after a jump
    outIL(A_GENLABEL, NULL, NULL, 0, 0, genlabel(), 0, 0);
#endif
    return (NOTEMP);
  case A_BOUNDS:
    label = add_strlit(n->strlit, n->is_const);
    temp = add_strlit(Thisfunction->name, true);
    outIL(A_BOUNDS, NULL, NULL, lefttemp, righttemp, label, temp, 0);
    return (lefttemp);
  case A_VASTART:
    outIL(A_VASTART, n->sym->type, NULL, 0, 0, NOLABEL, 0, 0);
    outILstr(n->sym->name);
    return (NOTEMP);
  case A_VAEND:
    outIL(A_VAEND, NULL, NULL, 0, 0, NOLABEL, 0, 0);
    return (NOTEMP);
  case A_VAARG:
    temp = ilalloctemp();
    outIL(A_VAARG, n->type, NULL, temp, 0, NOLABEL, 0, 0);
    return (temp);
  case A_AAITERSTART:
    return (gen_aaiterstart(n));
  case A_AANEXT:
    return (gen_aanext(n));
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
  outIL(A_JUMPIFFALSE, NULL, NULL, 0, t1, Lfalse, 0, 0);

  // Generate the true statement block
  genAST(n->mid);

  // If there is an optional ELSE clause,
  // generate the jump to skip to the end
  if (n->right) {
    outIL(A_JUMP, NULL, NULL, 0, 0, Lend, 0, 0);
  }

  // Now the false label
  outIL(A_GENLABEL, NULL, NULL, 0, 0, Lfalse, 0, 0);

  // Optional ELSE clause: generate the false
  // statement block and the end label
  if (n->right) {
    genAST(n->right);
    outIL(A_GENLABEL, NULL, NULL, 0, 0, Lend, 0, 0);
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
  outIL(A_GENLABEL, NULL, NULL, 0, 0, Lstart, 0, 0);

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
    outIL(A_JUMPIFFALSE, NULL, NULL, 0, t1, Lend, 0, 0);
  }

  // Generate the statement block for the WHILE body
  genAST(n->mid);

  // Finally output the jump back to the condition,
  // and the end label
  outIL(A_JUMP, NULL, NULL, 0, 0, Lstart, 0, 0);
  outIL(A_GENLABEL, NULL, NULL, 0, 0, Lend, 0, 0);

  // And pop the Breaklabel node from the stack
  Breakhead = this->prev;
}

// Generate space for a local variable
// and assign its value
static void gen_local(ASTnode * n) {
  int lefttemp;
  int basetemp;
  int functemp;
  int flags = IL_ZERO;
  int size;

  // Get the variable's size
  size = get_varsize(n->sym);

  // We have an initialisation,
  // no need to zero the space
  if (n->left != NULL)
    flags = 0;

  // We have an associative array
  if (n->sym->keytype != NULL)
    flags = flags | IL_ISAARRAY;

  // Create the local variable
  outIL(A_LOCAL, NULL, NULL, 0, 0, 0, flags, size);
  outILstr(n->sym->name);

  // Is this an aggregate variable?
  if (is_array(n->sym) || is_struct(n->sym->type)) {

    // Get the base address of the variable
    basetemp = iladdress(n->sym);

    // Now walk any bracketed element list
    // and initialise the members/elements
    if (n->left != NULL)
      check_bel(n->sym, n->left, 0, false, basetemp);
  } else {
    // No, it's a scalar variable

    // Get the expression's value
    // on the left if there is one
    if (n->left != NULL) {
      lefttemp = genAST(n->left);
      // Check the expression's range if required
      if (has_range(n->type)) {
	functemp = add_strlit(Thisfunction->name, true);
	outIL(A_RANGE, NULL, n->type, n->type->lower,
	      lefttemp, NOLABEL, n->type->upper, functemp);
      }
    }

    // If we didn't zero the variable, assign the initial value
    if (flags == 0) {
      if (n->sym->visibility != SV_LOCAL)
	flags = flags | IL_GLOBAL;
      if (n->sym->has_addr)
	flags = flags | IL_HASADDR;
      outIL(A_STORVAR, n->type, NULL, 0, lefttemp, 0, flags, 0);
      outILstr(n->sym->name);
    }
  }

  // Generate any code for the other children
  genAST(n->mid);
  genAST(n->right);
}

// Given a parameter's type and inout flag,
// and an ASTnode which is the argument,
// return the node to match the parameter's type
static ASTnode *fixup_argument(Type * paramtype, bool is_inout,
			       ASTnode * node) {
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

// Generate the IL code for a function call
static void gen_call(Sym * func, int numargs,
		     int return_temp, int excepttemp, int varidiac_posn,
		     int *arglist, Type ** typelist) {
  int i;
  int flags = 0;

  if (func->type->kind == TY_FUNCPTR) flags = IL_ISFUNCPTR;
  if (func->visibility != SV_LOCAL)   flags = flags | IL_GLOBAL;
  if (func->has_addr)                 flags = flags | IL_HASADDR;

  // Output the IL node
  outIL(A_FUNCCALL, func->type, NULL, return_temp, excepttemp,
	varidiac_posn, flags, numargs);
  outILstr(func->name);

  // Now output the types and temporaries for all the arguments
  for (i = 0; i < numargs; i++) {
    iltype(typelist[i], true);
    fwrite(arglist, sizeof(int), 1, Interfh);
    arglist++;
  }
}

// Check and generate the argument values for a function call.
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
  int temp;
  int excepttemp = NOTEMP;
  int return_temp = NOTEMP;
  int varidiac_posn = -1;
  int zerotemp;
  bool func_throws;

  // Get the matching symbol for the function's name
  func = n->sym;
  if (func->symtype != ST_FUNCTION && func->type->kind != TY_FUNCPTR)
    lfatal(n->line, "%s is not a function\n", n->left->strlit);

  // If the function returns a value, get a temporary for it
  if (func->type != ty_void) {
    return_temp = ilalloctemp();
  }

  // Cache if the function throws an exception
  func_throws = (func->exceptvar != NULL);

  // Set the position of the varidiac argument as required
  if (func->is_variadic == true)
    varidiac_posn = func->count - 1;

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
    for (ptype = func->type->paramtype; ptype != NULL; ptype = ptype->next)
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
      ptype = func->type->paramtype;
      for (i = 0, this = n->right; this != NULL; this = this->right, i++) {
	if (this->op == A_GLUE)
	  node = this->left;
	else
	  node = this;

	// Make the node match the parameter
	if (ptype == NULL)
	  node = fixup_argument(NULL, false, node);
	else
	  node = fixup_argument(ptype->type, ptype->is_inout, node);

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
	lfatal(n->line,
	       "Cannot use named argument with a variadic function\n");

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
	      lfatal(n->line, "Parameter %s used multiple times\n",
		     param->name);
	    param->count = 1;

	    // Make the node match the parameter
	    this->left =
	      fixup_argument(param->type, param->is_inout, this->left);

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
	if (param == NULL)
	  node = fixup_argument(NULL, false, node);
	else
	  node = fixup_argument(param->type, param->is_inout, node);

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
    excepttemp = iladdress(Ehead->sym);

    // Get a literal zero into a temporary
    zero.intval = 0;
    zerotemp = ilalloctemp();
    outIL(A_LOADLIT, ty_int32, NULL, zerotemp, 0, NOLABEL, 0, 0);
    fwrite(&zero, sizeof(Litval), 1, Interfh);

    // Set the exception variable's first member to zero
    outIL(A_STORDEREF, ty_int32, NULL, excepttemp, zerotemp, NOLABEL, 0, 0);
  }

  // Generate the QBE code for the function call
  gen_call(func, numargs, return_temp, excepttemp,
	   varidiac_posn, arglist, typelist);

  // If we are in a try clause, test if the first
  // member of the exception variable is not zero.
  // If not, jump to the catch clause
  if (func_throws && (Ehead != NULL) && (Ehead->in_try == true)) {

    // Get the value of the first member in the exception variable
    temp = ilalloctemp();
    outIL(A_DEREF, ty_int32, NULL, temp, excepttemp, NOLABEL, 0, 0);

    // Compare the first member against zero
    ilcompare(A_EQ, temp, zerotemp, ty_int32);

    // Jump if false to the catch label
    outIL(A_JUMPIFFALSE, NULL, NULL, 0, temp, Ehead->Lcatch, 0, 0);
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
  outIL(A_JUMP, NULL, NULL, 0, 0, Lend, 0, 0);

  // Output the label for the catch clause,
  // then the catch code, then the end label
  this->in_try = false;
  outIL(A_GENLABEL, NULL, NULL, 0, 0, Lcatch, 0, 0);
  genAST(n->right);
  outIL(A_GENLABEL, NULL, NULL, 0, 0, Lend, 0, 0);

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
  if (n->left->type == pointer_to(ty_int8) || n->left->type == ty_string) {
    outIL(A_STRHASH, NULL, NULL, temp, temp, NOLABEL, 0, 0);
    n->left->type = ty_uint64;
  }

  ty = n->left->type;

  // Walk the right-child linked list
  // to generate the code for each case
  for (i = 0, c = n->right; c != NULL; i++, c = c->right) {

    // Output the label for this case's test
    outIL(A_GENLABEL, NULL, NULL, 0, 0, caselabel[i], 0, 0);

    // If this is not the default case
    if (c->op != A_DEFAULT) {

      // Jump to the next case test if the value doesn't match the case value
      t2 = ilalloctemp();
      outIL(A_LOADLIT, ty, NULL, t2, 0, NOLABEL, 0, 0);
      fwrite(&(c->litval), sizeof(Litval), 1, Interfh);
      ilcompare(A_EQ, t2, temp, ty);
      outIL(A_JUMPIFFALSE, NULL, NULL, 0, t2, caselabel[i + 1], 0, 0);

      // Otherwise, jump to the code to handle this case
      outIL(A_JUMP, NULL, NULL, 0, 0, codelabel[i], 0, 0);
    }

    // Output the label for this case's code
    outIL(A_GENLABEL, NULL, NULL, 0, 0, codelabel[i], 0, 0);

    // If the case has no body, jump to the following case's body
    if (c->left == NULL) {
      outIL(A_JUMP, NULL, NULL, 0, 0, codelabel[i + 1], 0, 0);
    } else {
      // Before we generate the code, update the Switchlabel
      // to have the label for the next case code, in
      // case we do a fallthrough in the body
      Switchhead->next_label = codelabel[i + 1];

      // Generate the case code
      genAST(c->left);

      // Always jump to the end of the switch (no fallthrough)
      outIL(A_JUMP, NULL, NULL, 0, 0, Lend, 0, 0);
    }
  }

  // Now output the end label and pull the Switchlabel from the stack
  outIL(A_GENLABEL, NULL, NULL, 0, 0, Lend, 0, 0);
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
    outIL(A_JUMPIFFALSE, NULL, NULL, 0, temp, Lfalse, 0, 0);
    temp = genAST(n->right);
    outIL(A_JUMPIFFALSE, NULL, NULL, 0, temp, Lfalse, 0, 0);
  } else {
    // Lazy OR evaluation. Do the left side. If false,
    // do the right side, else the result is true.
    // If we jumped to Lright, do the right side.
    // If false, the result is false.
    temp = genAST(n->left);
    outIL(A_JUMPIFFALSE, NULL, NULL, 0, temp, Lright, 0, 0);
    outIL(A_JUMP, NULL, NULL, 0, 0, Ltrue, 0, 0);
    outIL(A_GENLABEL, NULL, NULL, 0, 0, Lright, 0, 0);
    temp = genAST(n->right);
    outIL(A_JUMPIFFALSE, NULL, NULL, 0, temp, Lfalse, 0, 0);
    outIL(A_GENLABEL, NULL, NULL, 0, 0, Ltrue, 0, 0);
  }

  outIL(A_LOADBOOL, n->left->type, NULL, temp, 0, NOLABEL, 0, 1);
  outIL(A_JUMP, NULL, NULL, 0, 0, Lend, 0, 0);
  outIL(A_GENLABEL, NULL, NULL, 0, 0, Lfalse, 0, 0);
  outIL(A_LOADBOOL, n->left->type, NULL, temp, 0, NOLABEL, 0, 0);
  outIL(A_GENLABEL, NULL, NULL, 0, 0, Lend, 0, 0);
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
  result = ilalloctemp();

  // Generate the condition code
  t = genAST(n->left);

  // Jump if false to the false label
  outIL(A_JUMPIFFALSE, NULL, NULL, 0, t, Lfalse, 0, 0);

  // Generate the true expression and the false label.
  expr = genAST(n->mid);
  outIL(A_MOVE, NULL, n->mid->type, result, expr, NOLABEL, 0, 0);
  outIL(A_JUMP, NULL, NULL, 0, 0, Lend, 0, 0);
  outIL(A_GENLABEL, NULL, NULL, 0, 0, Lfalse, 0, 0);

  // Generate the false expression and the end label.
  expr = genAST(n->right);
  outIL(A_MOVE, NULL, n->right->type, result, expr, NOLABEL, 0, 0);
  outIL(A_GENLABEL, NULL, NULL, 0, 0, Lend, 0, 0);

  return (result);
}

// Given a type and the current possible offset
// of a member of this type in a struct, return
// the correct offset for the member
// requirement in bytes
int genalign(Type * ty, int offset) {
  int alignment = 1;

  // Pointers are PTR_SIZE-byte aligned
  if (is_pointer(ty))
    alignment = PTR_SIZE;
  else {
    // Structs: use the type of the first member
    if (ty->kind == TY_STRUCT)
      ty = ty->memb->type;

    switch (ty->kind) {
    case TY_BOOL:
    case TY_INT8: return (offset);
    case TY_INT16: alignment = 2; break;
    case TY_INT32:
    case TY_FLT32:
      alignment = 4; break; case TY_INT64:
    case TY_FLT64:
      alignment = 8; break;
    case TY_VOID:
    case TY_USER:
    case TY_STRUCT:
      fatal("No QBE for type %s\n", get_typename(ty));
    }
  }

  // Calculate the new offset
  offset = (offset + (alignment - 1)) & ~(alignment - 1);
  return (offset);
}

void gen_func_postamble(Type * type) {
  outIL(A_FUNCPOSTAMBLE, type, NULL, 0, 0, 0, 0, 0);
}

static int gen_cast(ASTnode * n) {
  int exprtemp = genAST(n->left);
  int functemp = add_strlit(Thisfunction->name, true);
  int temp = ilalloctemp();
  ilcast(exprtemp, n->left->type, temp, n->type, functemp);
  return (temp);
}

// Either get the value of a key/value pair from
// an associative array
// or set the value of the key to exprtemp
static int gen_aarray(ASTnode * n, int exprtemp, Type * ty) {
  int keytemp;
  int functemp = add_strlit(Thisfunction->name, true);
  int t2 = ilalloctemp();
  int flags;

  // We're an rvalue with no type, do nothing.
  // A_ASSIGN will call us again soon!
  if ((n->rvalue == false) && (ty == NULL))
    return (NOTEMP);

  flags = 0;
  if (n->left->sym->visibility != SV_LOCAL) flags = flags | IL_GLOBAL;
  if (n->left->sym->has_addr)               flags = flags | IL_HASADDR;

  // Get the key value into a temporary
  keytemp = genAST(n->right);

  // If the key type is ty_string or a pointer to ty_int8,
  // assume it's a string. Call an external function
  // to get its hash value
  if (n->left->sym->keytype == ty_string ||
      n->left->sym->keytype == pointer_to(ty_int8)) {
    outIL(A_STRHASH, NULL, NULL, keytemp, keytemp, NOLABEL, 0, 0);
  } else {
    // Otherwise widen the key value to be 64 bits
    ilcast(keytemp, n->left->sym->keytype, t2, ty_uint64, functemp);
    keytemp = t2;
  }

  // If we have a type, we're an lvalue
  if (ty != NULL) {
    // Save the key/value in the array
    outIL(A_AASETVAL, NULL, ty, exprtemp, keytemp, NOLABEL, 0, 0);
    outILstr(n->left->sym->name);
    return (NOTEMP);
  } else {
    // Look up the value
    outIL(A_AAGETVAL, n->type, NULL, t2, keytemp, NOLABEL, 0, 0);
    outILstr(n->left->sym->name);
    return (t2);
  }
}

static int gen_exists(ASTnode * n) {
  int keytemp;
  int functemp = add_strlit(Thisfunction->name, true);
  int t2 = ilalloctemp();
  int flags;

  flags = 0;
  if (n->left->left->sym->visibility != SV_LOCAL) flags = flags | IL_GLOBAL;
  if (n->left->left->sym->has_addr)               flags = flags | IL_HASADDR;

  // Get the key value into a temporary
  keytemp = genAST(n->left->right);

  // If the key type is ty_string or a pointer to ty_int8,
  // assume it's a string. Call an external function
  // to get its hash value
  if (n->left->left->sym->keytype == ty_string ||
      n->left->left->sym->keytype == pointer_to(ty_int8)) {
    outIL(A_STRHASH, NULL, NULL, keytemp, keytemp, NOLABEL, 0, 0);
  } else {
    // Otherwise widen the key value to be 64 bits
    ilcast(keytemp, n->left->sym->keytype, t2, ty_uint64, functemp);
    keytemp = t2;
  }

  // Look up the value and return if it exists
  outIL(A_AAEXISTS, NULL, NULL, t2, keytemp, NOLABEL, 0, 0);
  outILstr(n->left->left->sym->name);
  return (t2);
  return (0);
}

static int gen_undef(ASTnode * n) {
  int keytemp;
  int functemp = add_strlit(Thisfunction->name, true);
  int t2 = ilalloctemp();
  int flags;

  flags = 0;
  if (n->left->left->sym->visibility != SV_LOCAL) flags = flags | IL_GLOBAL;
  if (n->left->left->sym->has_addr)               flags = flags | IL_HASADDR;

  // Get the key value into a temporary
  keytemp = genAST(n->left->right);

  // If the key type is ty_string or a pointer to ty_int8,
  // assume it's a string. Call an external function
  // to get its hash value
  if (n->left->left->sym->keytype == ty_string ||
      n->left->left->sym->keytype == pointer_to(ty_int8)) {
    outIL(A_STRHASH, NULL, NULL, keytemp, keytemp, NOLABEL, 0, 0);
  } else {
    // Otherwise widen the key value to be 64 bits
    ilcast(keytemp, n->left->sym->keytype, t2, ty_uint64, functemp);
    keytemp = t2;
  }

  // Remove the key and value
  outIL(A_AADELVAL, NULL, NULL, NOTEMP, keytemp, NOLABEL, 0, 0);
  outILstr(n->left->left->sym->name);
  return (NOTEMP);
}

// Set up the iteration on an associative array
// and return a pointer to the first value
static int gen_aaiterstart(ASTnode * n) {
  int arytemp = ilalloctemp();
  int valtemp = ilalloctemp();
  int flags;

  flags = 0;
  if (n->sym->visibility != SV_LOCAL) flags = flags | IL_GLOBAL;
  if (n->sym->has_addr)               flags = flags | IL_HASADDR;

  outIL(A_AAITERSTART, NULL, NULL, valtemp, arytemp, NOLABEL, 0, NOTEMP);
  outILstr(n->sym->name);
  return (valtemp);
}

// Iterate and get the next value
// from an associative array
static int gen_aanext(ASTnode * n) {
  int arytemp = ilalloctemp();
  int valtemp = ilalloctemp();
  int flags;

  flags = 0;
  if (n->sym->visibility != SV_LOCAL) flags = flags | IL_GLOBAL;
  if (n->sym->has_addr)               flags = flags | IL_HASADDR;

  // Get the next value
  outIL(A_AANEXT, NULL, NULL, valtemp, arytemp, NOLABEL, 0, NOTEMP);
  outILstr(n->sym->name);
  return (valtemp);
}

int gen_assign(int ltemp, int rtemp, ASTnode * n) {
  int functemp;
  int flags = 0;

  switch (n->right->op) {
  case A_IDENT:
    // We are assigning to an identifier.

    // If the type has a range, check it
    if (has_range(n->right->type)) {
      functemp = add_strlit(Thisfunction->name, true);
      outIL(A_RANGE, NULL, n->right->type, n->right->type->lower,
	    ltemp, NOLABEL, n->right->type->upper, functemp);
    }
    // Output the IL operations
    if (n->right->sym->visibility != SV_LOCAL) flags = flags | IL_GLOBAL;
    if (n->right->sym->has_addr)               flags = flags | IL_HASADDR;
    outIL(A_STORVAR, n->right->type, NULL, 0, ltemp, 0, flags, 0);
    outILstr(n->right->sym->name);

    return (NOTEMP);
  case A_DEREF:
    // We are assigning though a pointer.
    // If the type has a range, check it
    if (has_range(n->right->type)) {
      functemp = add_strlit(Thisfunction->name, true);
      outIL(A_RANGE, NULL, n->right->type, n->right->type->lower,
	    ltemp, NOLABEL, n->right->type->upper, functemp);
    }

    outIL(A_STORDEREF, n->right->type, NULL, rtemp, ltemp, NOLABEL, 0, 0);
    return (NOTEMP);
  case A_AARRAY:
    // We are adding/updating a key/value pair
    // in an associative array
    gen_aarray(n->right, ltemp, n->right->type);
    return (NOTEMP);
  default:
    lfatal(n->line, "Bad A_ASSIGN in genAST()\n");
  }
  return (NOTEMP);
}

// Add a value to a global symbol
static void genglobsymval(ASTnode * value, int offset) {
  int label = NOLABEL;

  // Value is a string literal, get its label
  if (value->op == A_STRLIT)
    label = add_strlit(value->strlit, value->is_const);

  outIL(A_GLOBSYMVAL, value->type, NULL, 0,
	offset, label, 0, value->type->size);

  // Output any numeric value
  if (value->op != A_STRLIT)
    fwrite(&(value->litval), sizeof(Litval), 1, Interfh);
}

// Given a symbol and a bracketed expression list,
// check that the list is suitable for the symbol.
// Also output the values to the assembly file.
// offset holds the byte offset of the value from the
// base of any struct/array. If is_element is true,
// we are processing elements in the sym array.
// If basetemp is not NOTEMP, we are initialising
// a local variable
ASTnode *check_bel(Sym * sym, ASTnode * list,
		   int offset, bool is_element, int basetemp) {
  ASTnode *wide;
  Sym *memb;
  int i;
  int exprtemp;
  int functemp;
  Type *ty;

  // No list, we ran out of values in the list
  if (list == NULL)
    fatal("Not enough values in the expression list\n");

  // Get the type of the symbol or its elements
  ty = sym->type;
  if (is_element == true)
    ty = value_at(sym->type);

  // If we are at the start of a bracketed expression
  // list and we have an array or struct, remove
  // the A_BEL node that starts it
  if (list->op == A_BEL && is_element == false &&
      (is_struct(ty) || is_array(sym)))
    list = list->left;

  // The symbol is an array. Get the elements' type.
  // Use the count of elements and walk the list
  if (is_element == false && is_array(sym)) {
    ty = value_at(sym->type);
    for (i = 0; i < get_numelements(sym, 0); i++) {
      list = check_bel(sym, list, offset + i * ty->size, true, basetemp);
    }
    return (list);
  }

  // This is a struct.
  // Walk the list of struct members and
  // check each against the list value
  if (is_struct(ty)) {
    for (memb = ty->memb; memb != NULL; memb = memb->next) {
      list = check_bel(memb, list, offset + memb->offset, false, basetemp);
    }
    return (list);
  }

  // Not an array or struct, we must have a scalar
  if (list->op == A_BEL)
    fatal("%s is scalar, cannot use an initialisation list\n", sym->name);

  // Make sure the expression matches the symbol's type.
  wide = widen_type(list, ty, 0);
  if (wide == NULL)
    fatal("Initialisation value for %s not of type %s, but type %s\n",
	  sym->name, get_typename(ty), get_typename(list->type));

  // We are generating a non-local value
  if (basetemp == NOTEMP) {
    // It has to be a literal value
    if ((list->op != A_NUMLIT) && (list->op != A_STRLIT))
      fatal("Initialisation value not a literal value\n");

    // Check any ranged type against the initial value
    if (has_range(ty) && list->op == A_NUMLIT) {
      if ((list->litval.intval < ty->lower) ||
	  (list->litval.intval > ty->upper))
	fatal("Value %d outside range of type %s\n",
	      list->litval.intval, ty->name);
    }

    // Update the list element's type
    list->type = ty;

    // Output the value at the offset
    if (O_logmisc)
      fprintf(Debugfh, "globsymval offset %d\n", offset);
    genglobsymval(list, offset);
  } else {
    // We are dealing with a local variable.
    // Generate the expression's code and get the value
    exprtemp = genAST(list);

    // Check the expression's range if required
    if (has_range(ty)) {
      functemp = add_strlit(Thisfunction->name, true);
      outIL(A_RANGE, NULL, ty, ty->lower,
	    exprtemp, NOLABEL, ty->upper, functemp);
    }

    // Assign it into the aggregate variable at the offset
    outIL(A_STORELEM, ty, NULL, exprtemp, basetemp, NOLABEL, 0, offset);
  }

  return (list->mid);
}

void genglobsym(Sym * sym, bool make_zero) {
  int flags = 0;

  // No global associative arrays yet
  if (sym->keytype != NULL)
    fatal("No global associative arrays yet, sorry\n");

  // We can't declare it when it is opaque (zero size)
  if (sym->type->size == 0)
    fatal("Can't declare %s as size zero\n", sym->name);

  // Is the symbol local or global
  if (sym->visibility != SV_LOCAL)  flags = flags | IL_GLOBAL;
  if (sym->visibility == SV_PUBLIC) flags = flags | IL_PUBLIC;
  if (make_zero) flags =            flags | IL_ZERO;
  if (sym->is_const)                flags = flags | IL_ISCONST;

  outIL(A_GLOBSYM, NULL, NULL, 0, 0, NOLABEL, flags, sym->type->size);
  outILstr(sym->name);
}

void genglobsymend(Sym * sym) {
  outIL(A_GLOBSYMEND, NULL, NULL, 0, 0, NOLABEL, 0, 0);
}

static void gen_arrayiter(ASTnode * n, Breaklabel * this) {
  ASTnode *assign;
  int Lfortop;
  int aryptr;
  int idx;
  int arysize;
  int t1;
  int t2;
  int t3;

  // Get the base address of the array
  aryptr = genAST(n->mid);

  // Set the hiddex index to zero
  idx = ilalloctemp();
  outIL(A_INITTEMP, ty_int32, NULL, idx, 0, NOLABEL, 0, 0);

  // Get the array's size
  arysize = ilalloctemp();
  outIL(A_INITTEMP, ty_int32, NULL, arysize, 0, NOLABEL, 0, n->mid->count);

  // Top of the loop: is idx < the array's size
  Lfortop = genlabel();
  t1 = ilalloctemp();
  outIL(A_GENLABEL, NULL, NULL, 0, 0, Lfortop, 0, 0);
  // MOVE as the compare will overwrite the destination
  outIL(A_MOVE, NULL, ty_int32, t1, idx, NOLABEL, 0, 0);
  ilcompare(A_LT, t1, arysize, ty_int32);
  outIL(A_JUMPIFFALSE, NULL, NULL, 0, t1, this->break_label, 0, 0);

  // Get the element's value from the list
  t2 = ilalloctemp();
  outIL(A_DEREF, n->left->type, NULL, t2, aryptr, NOLABEL, 0, 0);
  assign = mkastnode(A_ASSIGN, NULL, NULL, n->left);
  t3 = genAST(n->left);
  gen_assign(t2, t3, assign);

  // Loop body
  genAST(n->right);

  // Increment idx
  outIL(A_GENLABEL, NULL, NULL, 0, 0, this->continue_label, 0, 0);
  outIL(A_INCTEMP, ty_int32, NULL, idx, 0, NOLABEL, 0, 1);

  // Increment the array pointer
  outIL(A_INCTEMP, PTR_TYPE, NULL, aryptr,
	0, NOLABEL, 0, n->left->type->size);

  // Jump to the top of the for loop
  outIL(A_JUMP, NULL, NULL, 0, 0, Lfortop, 0, 0);

  // End of the for statement
  outIL(A_GENLABEL, NULL, NULL, 0, 0, this->break_label, 0, 0);
}

// Call a function that returns an array of
// pointers and iterate over this list
static void gen_funciterator(ASTnode * n, Breaklabel * this) {
  int listptr = ilalloctemp();
  int elemptr = ilalloctemp();
  int elemddref = ilalloctemp();
  int Lifend = genlabel();
  int Lfortop = genlabel();
  int loopvar;
  ASTnode *assign;

  // Get the pointer to the loop variable into a temporary
  loopvar = genAST(n->left);

  // Call the function, copy the list pointer into elemptr
  listptr = genAST(n->mid);
  outIL(A_MOVE, NULL, PTR_TYPE, elemptr, listptr, NOLABEL, 0, 0);

  // Start the interation, get back elemddref with the next value
  // of the iteration
  outIL(A_FUNCITER, n->left->type, NULL, elemddref, elemptr,
	this->break_label, Lifend, Lfortop);

  // Store the value into the loop variable
  assign = mkastnode(A_ASSIGN, NULL, NULL, n->left);
  gen_assign(elemddref, loopvar, assign);

  // Generate the loop body
  genAST(n->right);

  // Generate the code after the loop body: we need 2 ILnodes
  outIL(A_FITERNEXT, NULL, NULL, listptr, elemptr,
	this->break_label, Lifend, Lfortop);
  outIL(A_MISC, NULL, NULL, this->continue_label, 0, 0, 0, 0);
}

// Iterate over all the characters in a string
static void gen_stringiterator(ASTnode * n, Breaklabel * this) {
  int loopvar;
  int strptr;
  int chtemp = ilalloctemp();
  int Lfortop = genlabel();
  ASTnode *assign;

  // Get the pointer to the loop variable into a temporary
  loopvar = genAST(n->left);

  // Get a copy of the base of the string
  strptr = genAST(n->mid);

  // Start the iteration, get back the next character
  outIL(A_STRINGITER, NULL, NULL, chtemp, strptr,
	this->break_label, 0, Lfortop);

  // Store the character into the loop variable
  assign = mkastnode(A_ASSIGN, NULL, NULL, n->left);
  gen_assign(chtemp, loopvar, assign);

  // Generate the loop body
  genAST(n->right);

  // Generate the code after the loop body
  outIL(A_SITERNEXT, NULL, NULL, 0, strptr,
	this->break_label, this->continue_label, Lfortop);
}
