// Type functions for the alic compiler
// (c) 2025 Warren Toomey, GPL3

#include "alic.h"
#include "proto.h"

extern FILE *outfh;
extern FILE *debugfh;

Type *ty_void = &(Type) { TY_VOID, 1, 1 };
Type *ty_bool = &(Type) { TY_BOOL, 1, 1 };

Type *ty_int8 = &(Type) { TY_INT8, 1, 1 };
Type *ty_int16 = &(Type) { TY_INT16, 2, 2 };
Type *ty_int32 = &(Type) { TY_INT32, 4, 4 };
Type *ty_int64 = &(Type) { TY_INT64, 8, 8 };

Type *ty_uint8 = &(Type) { TY_INT8, 1, 1, true };
Type *ty_uint16 = &(Type) { TY_INT16, 2, 2, true };
Type *ty_uint32 = &(Type) { TY_INT32, 4, 4, true };
Type *ty_uint64 = &(Type) { TY_INT64, 8, 8, true };

Type *ty_flt32 = &(Type) { TY_FLT32, 4, 4 };
Type *ty_flt64 = &(Type) { TY_FLT64, 8, 8 };

void add_type(ASTnode * node);

// Create a new Type struct
static Type *new_type(TypeKind kind, int size, int align) {
  Type *ty = calloc(1, sizeof(Type));
  ty->kind = kind;
  ty->size = size;
  ty->align = align;
  return (ty);
}

// Is this type an integer?
bool is_integer(Type * ty) {
  TypeKind k = ty->kind;
  return (k == TY_INT8 || k == TY_INT16 || k == TY_INT32 || k == TY_INT64);
}

// Is this type floating point?
bool is_flonum(Type * ty) {
  return (ty->kind == TY_FLT32 || ty->kind == TY_FLT64);
}

// Is this type numeric?
bool is_numeric(Type * ty) {
  return (is_integer(ty) || is_flonum(ty));
}

// List of built-in types
static char *typename[] = {
  "void", "bool", "int8", "int16",
  "int32", "int64", "flt32", "flt64",
  "unsigned int8", "unsigned int16",
  "unsigned int32", "unsigned int64"
};

// Return a string representing the type.
// For now, just the built-in types.
char *get_typename(Type * ty) {
  if (ty->is_unsigned)
    return (typename[ty->kind + 6]);	// XXX 6 is magic number
  else
    return (typename[ty->kind]);
}

// Given an ASTnode and a type, try to widen
// the node's type to match the given type.
// Return the same ASTnode if no widening is needed,
// or an ASTnode which widens the first one,
// or NULL if the types are not compatible.
ASTnode *widen_type(ASTnode *node, Type *ty) {
  ASTnode *newnode;

  // They have the same type, nothing to do
  if (node->type == ty) return(node);

  // We can't widen to a boolean
  if (ty == ty_bool) return(NULL);

  // We can't widen from a void
  if (node->type == ty_void)
    fatal("cannot widen anything of type void\n");

  // Change an int of any size to a float
  if (is_integer(node->type) && is_flonum(ty)) {
    newnode = mkastnode(A_CAST, ty, true, node, NULL, NULL, NULL, 0);
    return (newnode);
  }

  // The given type is smaller than the node's type, do nothing
  if (ty->size < node->type->size) return(node);

  // The node is a literal. We can update its
  // type without widening, but some rules apply
  if (node->op == A_NUMLIT) {
    // Check we're not trying to make a negative A_NUMLIT unsigned.
    if (ty->is_unsigned && !node->type->is_unsigned
				&& node->litval.intval < 0)
      fatal("Cannot cast negative literal value %ld to be unsigned\n",
	      node->litval.intval);

    // Deal with changing int literals to float literals
    if (is_integer(node->type) && is_flonum(ty))
      node->litval.dblval = node->litval.intval;

    node->type = ty; return(node);
  }

  // Signed and unsigned types cannot mix
  if (node->type->is_unsigned != ty->is_unsigned)
    return(NULL);

  // We are left with widening the left
  newnode = mkastnode(A_CAST, ty, true, node, NULL, NULL, NULL, 0);
  return (newnode);
}

// If an AST node has no type, determine
// its type based on the child nodes.
void add_type(ASTnode * node) {
  ASTnode *newnode;

  // Do nothing if no node, or it already has a type
  if (node == NULL || node->type != NULL) return;

  // If a relational expression, it's boolean
  if ((node->op >= A_EQ) && node->op <= A_NOT) {
    node->type = ty_bool;
    return;
  }

  // Set the child types if they have none
  add_type(node->left);
  add_type(node->right);

  // Try to widen each one to be the other's type
  newnode= widen_type(node->left,  node->right->type);
  if (newnode != NULL)
    node->left= newnode;
  newnode= widen_type(node->right, node->left->type);
  if (newnode != NULL)
    node->right= newnode;

  // Now set this node's type
  node->type = node->left->type;
}

// Given a string representing an integer or float literal, parse
// it and return a value which can be stored in an Litval union.
// Also return a suitable type for the value.
uint64_t parse_litval(char *litstr, Type ** type) {
  Litval e;

  // Is it a float?
  if (strchr(litstr, '.')) {
    e.dblval = strtod(litstr, NULL);
    *type = ty_flt32;
    return (e.uintval);
  }

  // Is it a character literal?
  if (litstr[0] == '\'') {
    e.uintval = litstr[1];	// XXX Do backslash literals later
  } else {
    // It must be an integer then
    e.uintval = strtoull(litstr, NULL, 10);
  }

  // Find the smallest suitable integer type for the value
  if (e.intval >= SCHAR_MIN && e.intval <= SCHAR_MAX)
    *type = ty_int8;
  else if (e.intval >= SHRT_MIN && e.intval <= SHRT_MAX)
    *type = ty_int16;
  else if (e.intval >= INT_MIN && e.intval <= INT_MAX)
    *type = ty_int32;
  else if (litstr[0] == '-')
    *type = ty_int64;
  else
    *type = ty_uint64;
  return (e.uintval);
}
