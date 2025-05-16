// Function handling for the alic compiler
// (c) 2025 Warren Toomey, GPL3

#include "alic.h"
#include "proto.h"

// Given an ASTnode representing a function's name & type
// and a second ASTnode holding a list of parameters, add
// the function to the symbol table. Die if the function
// exists and the parameter list is different or the
// existing function's type doesn't match the new one.
// Return 1 if there was a previous function delaration
// that had a statement block, otherwise 0
int add_function(ASTnode * func, ASTnode * paramlist) {
  Sym *this, *funcptr;
  int paramcnt=0;

  // Try to add the function to the symbol table
  funcptr = add_symbol(func->strlit, ST_FUNCTION, func->type, true);

  // The function already exists
  if (funcptr == NULL) {
    // Find the existing prototype
    funcptr = find_symbol(func->strlit);

    // Check the return type
    if (func->type != funcptr->type)
      fatal("%s() declaration has different type than previous: %s vs %s\n",
	    func->strlit, get_typename(func->type),
	    get_typename(funcptr->type));

    // Walk both the paramlist and the member list 
    // in this to verify both lists are the same
    this = funcptr->memb;
    while (1) {
      // No remaining parameters
      if (this == NULL && paramlist == NULL)
	break;

      // Different number of parameters
      if (this == NULL || paramlist == NULL)
	fatal("%s() declaration: # params different than previous\n",
	      func->strlit);

      // Parameter names differ
      if (strcmp(this->name, paramlist->strlit))
	fatal("%s() declaration: param name mismatch %s vs %s\n",
	      func->strlit, this->name, paramlist->strlit);

      // Parameter types differ
      if (this->type != paramlist->type)
	fatal("%s() declaration: param type mismatch %s vs %s\n",
	      func->strlit, get_typename(this->type),
	      get_typename(paramlist->type));

      // Move up to the next parameter in both lists
      this = this->next;
      paramlist = paramlist->mid;
    }

    // All OK. Return the function's intval: 1 means
    // that it was previously declared with a statement block
    return(funcptr->initval.intval);
  }

  // The function is a new one. Walk the parmlist adding
  // each name and type to the function's member list
  for (; paramlist != NULL; paramlist = paramlist->mid) {
    this= add_sym_to(&(funcptr->memb), paramlist->strlit,
		ST_VARIABLE, paramlist->type);
    this->has_addr= false;
    this->visibility= SV_LOCAL;
    paramcnt++;
  }

  // Set the number of function parameters,
  // or mark it as a variadic function
  if (func->rvalue == true)
    funcptr->count= -1;
  else
    funcptr->count= paramcnt;
  return(0);
}

// Declare a function which has a statement block
void declare_function(ASTnode *f) {
  Sym *this;

  // Add the function declaration to the symbol table.
  // Die if a previous declaration had a statement block
  if (add_function(f, f->left))
    fatal("multiple declarations for %s()\n", f->strlit);

  // Find the function's symbol entry and mark that it
  // does have a statement block
  this= find_symbol(f->strlit);
  this->initval.intval= 1;

  cg_func_preamble(this);
}

// Generate a function's statement block
void gen_func_statement_block(ASTnode *s) {
  if (O_dumpast) {
    dumpAST(s, 0);
    fflush(Debugfh);
  }
  genAST(s); freeAST(s);
  cg_func_postamble(Thisfunction->type);
}
