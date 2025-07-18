// Function handling for the alic compiler
// (c) 2025 Warren Toomey, GPL3

#include "alic.ah"
#include "proto.ah"

// Given an ASTnode representing a function's name & type
// and a second ASTnode holding a list of parameters, add
// the function to the symbol table. Die if the function
// exists and the parameter list is different or the
// existing function's type doesn't match the new one.
// Return true if there was a previous function delaration
// that had a statement block, otherwise false.
public bool add_function(const ASTnode * func,
		ASTnode * paramlist, const int visibility) {
  Sym *this;
  Sym *funcptr;
  int paramcnt = 0;

  // Try to add the function to the symbol table
  if (func.is_funcpointer)
    funcptr = add_symbol(func.strlit, ST_FUNCPOINTER, func.ty, visibility);
  else
    funcptr = add_symbol(func.strlit, ST_FUNCTION, func.ty, visibility);

  // The function already exists
  if (funcptr == NULL) {
    // Find the existing prototype
    funcptr = find_symbol(func.strlit);

    // Check the return type
    if (func.ty != funcptr.ty)
      fatal("%s() declaration has different type than previous: %s vs %s\n",
	    func.strlit, get_typename(func.ty),
	    get_typename(funcptr.ty));

    // Check that the exception handling marker for
    // the prototype and the real function are the same
    if (((funcptr.exceptvar != NULL) && (func.sym == NULL)) ||
	((funcptr.exceptvar == NULL) && (func.sym != NULL)))
      fatal("%s(): inconsistent exception handling cf. prototype\n",
	    func.strlit);

    // Walk both the paramlist and the member list 
    // in this to verify both lists are the same
    this = funcptr.paramlist;
    while (true) {
      // No remaining parameters
      if (this == NULL && paramlist == NULL)
	break;

      // Different number of parameters
      if (this == NULL || paramlist == NULL)
	fatal("%s() declaration: # params different than previous\n",
	      func.strlit);

      // Parameter names differ
      if (strcmp(this.name, paramlist.strlit) != 0)
	fatal("%s() declaration: param name mismatch %s vs %s\n",
	      func.strlit, this.name, paramlist.strlit);

      // Parameter types differ
      if (this.ty != paramlist.ty)
	fatal("%s() declaration: param type mismatch %s vs %s\n",
	      func.strlit, get_typename(this.ty),
	      get_typename(paramlist.ty));

      // Move up to the next parameter in both lists
      this = this.next;
      paramlist = paramlist.mid;
    }

    // All OK. Return if it was previously
    // declared with a statement block.
    return (funcptr.has_block);
  }

  // The function is a new one. Walk the parmlist adding
  // each name and type to the function's member list
  for (; paramlist != NULL; paramlist = paramlist.mid) {
    this = add_sym_to(&(funcptr.paramlist), paramlist.strlit,
		      ST_VARIABLE, paramlist.ty);
    if (this == NULL)
      fatal("Multiple parameters named %s in %s()\n",
	    paramlist.strlit, funcptr.name);
    this.has_block = false;
    this.visibility = SV_LOCAL;
    this.is_const= paramlist.is_const;
    paramcnt++;
  }

  // Set the number of function parameters, and
  // mark it as a variadic function if needed
  if (func.is_variadic == true)
    funcptr.is_variadic = true;
  funcptr.count = paramcnt;

  // If the function throws an exception, copy
  // the pointer to the exception variable over
  if (func.sym != NULL)
    funcptr.exceptvar = func.sym;

  // No statement block as yet
  return (false);
}

// Declare a function which has a statement block
public void declare_function(const ASTnode * f, const int visibility) {
  Sym *this;

  // Add the function declaration to the symbol table.
  // Die if a previous declaration had a statement block
  if (add_function(f, f.left, visibility) == true)
    fatal("Multiple declarations for %s()\n", f.strlit);

  // Find the function's symbol entry and mark that it
  // does have a statement block
  this = find_symbol(f.strlit);
  this.has_block = true;

  gen_func_preamble(this);
}

// Generate a function's statement block
public void gen_func_statement_block(const ASTnode * s) {
  if (O_dumpast) {
    dumpAST(s, 0);
    fflush(Debugfh);
  }

  genAST(s);
  gen_func_postamble(Thisfunction.ty);
}
