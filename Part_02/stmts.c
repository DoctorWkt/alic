// Statement handling for the alic compiler
// (c) 2025 Warren Toomey, GPL3

#include "alic.h"
#include "proto.h"

extern FILE *debugfh;
extern Type *ty_flt64;

ASTnode *print_statement(ASTnode * e) {
  // If we are printing a flt32, widen it to be a flt64
  if (e->type->kind == TY_FLT32) e = widen_type(e, ty_flt64);

  // Make an A_PRINT AST node
  return(mkastnode(A_PRINT, e->type, 0, e, NULL, NULL, NULL, 0));
}

ASTnode *assignment_statement(ASTnode * v, ASTnode * e) {
  ASTnode *newnode;

  // Widen the expression's type if required
  newnode= widen_type(e, v->type);
  if (newnode == NULL)
    fatal("Incompatible types %s vs %s\n",
        get_typename(e->type), get_typename(v->type));
  e = newnode;

  v->rvalue = false;
  v->op = A_ASSIGN;
  v->left = e;
  v->type = v->sym->type;
  return(v);
}

ASTnode *declaration_statement(char *symname, ASTnode * e, Type * ty) {
  Sym *s;
  ASTnode *v;
  ASTnode *newnode;

  // Widen the expression's type if required
  newnode= widen_type(e, ty);
  if (newnode == NULL)
    fatal("Incompatible types %s vs %s\n",
        get_typename(e->type), get_typename(ty));
  e = newnode;

  // If the expression is not a literal value,
  // give it an initial zero value
  if (e->op != A_NUMLIT) e->litval.uintval = 0;

  s = add_symbol(symname, false, ty, e->litval.uintval);
  if (debugfh)
    fprintf(debugfh, "declare type %s %s = %ld\n",
	    get_typename(ty), symname, e->litval.intval);
  free(symname);

  // And if the expression is not a literal value,
  // now do an assignment statement to set the real value
  if (e->op != A_NUMLIT) {
    v = mkastleaf(A_IDENT, s->type, true, s, 0);
    return(assignment_statement(v, e));
  }
  return(NULL);
}
