// Type functions for the alic compiler
// (c) 2025 Warren Toomey, GPL3

#include "alic.h"
#include "proto.h"

// A list of minimums per type, signed followed by unsigned
int64_t typemin[8]= {
  INT8_MIN, INT16_MIN, INT32_MIN, INT64_MIN, 0, 0, 0, 0
};

// A list of maximumx per type, signed followed by unsigned.
// We don't use the uint64 type value, so it is zero
int64_t typemax[8]= {
  INT8_MAX,  INT16_MAX,  INT32_MAX,  INT64_MAX,
  UINT8_MAX, UINT16_MAX, UINT32_MAX, 0
};

Type *ty_void = &(Type) { TY_VOID, 1 };
Type *ty_bool = &(Type) { TY_BOOL, 1 };

Type *ty_int8 = &(Type) { TY_INT8, 1 };
Type *ty_int16 = &(Type) { TY_INT16, 2 };
Type *ty_int32 = &(Type) { TY_INT32, 4 };
Type *ty_int64 = &(Type) { TY_INT64, 8 };

Type *ty_uint8 = &(Type) { TY_INT8, 1, true };
Type *ty_uint16 = &(Type) { TY_INT16, 2, true };
Type *ty_uint32 = &(Type) { TY_INT32, 4, true };
Type *ty_uint64 = &(Type) { TY_INT64, 8, true };

Type *ty_flt32 = &(Type) { TY_FLT32, 4 };
Type *ty_flt64 = &(Type) { TY_FLT64, 8 };

Type *ty_voidptr = &(Type) { TY_VOID, 8, false, 1 };	// Used by NULL
Type *ty_int8ptr = &(Type) { TY_INT8, 8, false, 1 };	// Used by strlits

// Global variables
Type *Typehead;

// Initialise the type list with the built-in types
void init_typelist(void) {
  Typehead = ty_voidptr;
  ty_voidptr->next = ty_int8ptr;
  ty_int8ptr->next = ty_void;
  ty_void->next = ty_bool;
  ty_bool->next = ty_int8;
  ty_int8->next = ty_int16;
  ty_int16->next = ty_int32;
  ty_int32->next = ty_int64;
  ty_int64->next = ty_uint8;
  ty_uint8->next = ty_uint16;
  ty_uint16->next = ty_uint32;
  ty_uint32->next = ty_uint64;
  ty_uint64->next = ty_flt32;
  ty_flt32->next = ty_flt64;
  ty_flt64->next = NULL;
}

// Create a new Type struct and
// add it to the list of types
Type *new_type(int kind, int size, bool is_unsigned,
			int ptr_depth, char *name, Type * base) {
  Type *ty= NULL;
  Type *walktype;
  bool newnode= false;

  // See if this is an existing type.
  // If it is and it's not an opaque type, a problem
  if (name != NULL) {
    ty= find_type(name, 0, false, ptr_depth);
    if ((ty != NULL) && (ty->size > 0))
    fatal("Type %s already exists\n", name);
  }

  // It doesn't exist, make a Type node
  if (ty == NULL) {
    ty = Calloc(sizeof(Type));
    newnode= true;
  }

  // Fill in the fields
  ty->kind = kind;
  ty->size = size;
  ty->is_unsigned = is_unsigned;
  ty->ptr_depth = ptr_depth;
  ty->name = name;
  ty->basetype = base;

  // Add it to the list of types
  if (newnode == true) {
    if (Typehead != NULL)
      ty->next = Typehead;
    Typehead = ty;
  } else {
    // We've redefined an opaque type. Walk the
    // list of types. Find any type which points
    // to a type of this name and fill in the basetype
    for (walktype= Typehead; walktype != NULL; walktype= walktype->next) {
      if ((walktype->ptr_depth > 0) && (walktype->name!=NULL) &&
				!strcmp(walktype->name, ty->name)) {
	walktype->basetype= ty;
	walktype->kind= ty->kind;
      }
    }
  }

  return (ty);
}

// Given either a user-defined type name or
// (if NULL) a built-in typekind, and the
// pointer depth, return a pointer to the
// relevant Type structure, or NULL if it
// does not exist
Type *find_type(char *typename, int kind, bool is_unsigned, int ptr_depth) {
  Type *this;

  if (typename != NULL) {
    // We have a name, so search for this name
    for (this = Typehead; this != NULL; this = this->next) {
      if (this->name != NULL && this->ptr_depth == ptr_depth
	  && !strcmp(this->name, typename)) {
	// This type could be an alias.
	// If so, return the base type but not when
	// the type has a range
	if (this->basetype != NULL && this->ptr_depth == 0 && !has_range(this))
	  return (this->basetype);
	else
	  return (this);
      }
    }
    return (NULL);
  } else {
    // Otherwise, search for the type kind
    for (this = Typehead; this != NULL; this = this->next) {
      if (this->kind == kind && this->is_unsigned == is_unsigned &&
          this->ptr_depth == ptr_depth)
	return (this);
    }
  }

  return (NULL);
}

// Given a type pointer, return a type that
// represents a pointer to the argument
Type *pointer_to(Type * ty) {
  Type *this;

  // Search for a pointer to this type
  this = find_type(ty->name, ty->kind, ty->is_unsigned, ty->ptr_depth + 1);
  if (this != NULL)
    return (this);

  // We didn't find one, so make one and return it.
  return (new_type(ty->kind, PTR_SIZE, ty->is_unsigned,
		ty->ptr_depth + 1, ty->name, ty->basetype));
}

// Given a type pointer, return a type that
// represents the type that the argument points at
Type *value_at(Type * ty) {
  Type *this;

  if (ty->ptr_depth == 0)
    fatal("Can't value_at() with depth zero!\n");

  // Search for the type that we point to
  this = find_type(ty->name, ty->kind, ty->is_unsigned, ty->ptr_depth - 1);
  if (this != NULL)
    return (this);

  // We didn't find one, so make one and return it.
  return (new_type(ty->kind, PTR_SIZE, ty->is_unsigned,
		ty->ptr_depth - 1, ty->name, ty->basetype));
}

// Is this type an integer?
bool is_integer(Type * ty) {
  int k = ty->kind;
  if (ty->ptr_depth != 0)
    return (false);
  return (k == TY_INT8 || k == TY_INT16 || k == TY_INT32 || k == TY_INT64);
}

// Is this type floating point?
bool is_flonum(Type * ty) {
  if (ty->ptr_depth != 0)
    return (false);
  return (ty->kind == TY_FLT32 || ty->kind == TY_FLT64);
}

// Is this type numeric?
bool is_numeric(Type * ty) {
  return (is_integer(ty) || is_flonum(ty));
}

// Is this type a pointer?
bool is_pointer(Type * ty) {
  if (ty->kind==TY_FUNCPTR) return(true);
  return (ty->ptr_depth != 0);
}

// Is this type a struct?
bool is_struct(Type * ty) {
  return ((ty->ptr_depth == 0) && (ty->kind == TY_STRUCT));
}

// List of built-in types
static char *typename[] = {
  "int8", "int16", "int32", "int64",
  "flt32", "flt64", "void", "bool",
  "uint8", "uint16", "uint32", "uint64"
};

#define TYPELEN 255
static char typenbuf[TYPELEN];

// Return a string representing the type.
char *get_typename(Type * ty) {
  int i;

  if (ty->name != NULL)
    strcpy(typenbuf, ty->name);
  else {
    if (ty->is_unsigned)	// 8 is a magic number
      strcpy(typenbuf, typename[ty->kind + 8]);
    else
      strcpy(typenbuf, typename[ty->kind]);
  }

  if (ty->ptr_depth > 0) {
    strcat(typenbuf, " ");
    for (i = 0; i < ty->ptr_depth; i++)
      strcat(typenbuf, "*");
  }

  return (strdup(typenbuf));
}

// Given an ASTnode and a type, try to widen
// the node's type to match the given type.
// Return the same ASTnode if no widening is needed,
// or an ASTnode which widens the first one,
// or NULL if the types are not compatible.
ASTnode *widen_type(ASTnode * node, Type * ty, int op) {
  ASTnode *newnode;
  Type *at_type;
  int size;

  // They have the same type, nothing to do
  if (node->type == ty)
    return (node);

  // We can't widen to a boolean
  if (ty == ty_bool)
    return (NULL);

  // If both types are pointers
  if (is_pointer(ty) && is_pointer(node->type)) {

    // We can widen from a void pointer
    if (node->type == ty_voidptr) {
      node->type = ty;
      return (node);
    }
    // We can widen to a void pointer
    if (ty == ty_voidptr)
      return (node);

    // Otherwise the pointers are incompatible
    return (NULL);
  }

  // We can't widen a pointer to a non-pointer
  if (!is_pointer(ty) && is_pointer(node->type))
    return(NULL);

  // If the type is a pointer and the node is an integer
  if (is_pointer(ty) && is_integer(node->type)) {

    switch(op) {
      case A_ADD:
      case A_SUBTRACT:
        // When we are adding or subtracting, scale the
        // node to be size of the value at the pointer.
        // This catches `int32 *x; x= x + 1; // Should be +4

        // Widen the node to be ty_uint64
        newnode = widen_type(node, ty_uint64, 0);
	if (newnode == NULL)
	  fatal("Could not widen %s to be ty_uint64\n",
			get_typename(node->type));
	node= newnode;

        // Get the size of the type the pointer points at
        at_type = value_at(ty);
        size = at_type->size;
        if (size == 0)
          fatal("Cannot change a pointer to an opaque type\n");

        // Scale only when bigger than one
        if (size > 1) {
          node = unarop(node, A_SCALE);
          node->litval.intval = size;
          node->type = ty;
        }
        return (node);

      case A_ADDOFFSET:
	// Change the node's type to be the pointer's
        node->type = ty;
        return (node);

      default:
	fatal("Cannot mix an integer with a pointer, op %d\n", op);
    }
  }

  // We can't widen from a void
  if (node->type == ty_void)
    fatal("Cannot widen anything of type void\n");

  // We can't widen a float to an int
  if (is_flonum(node->type) && is_integer(ty))
    return(NULL);

  // Change an int of any size to a float
  if (is_integer(node->type) && is_flonum(ty)) {
    newnode = mkastnode(A_WIDEN, node, NULL, NULL);
    newnode->type = ty;
    newnode->rvalue = true;
    return (newnode);
  }

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

    node->type = ty;
    return (node);
  }

  // Signed and unsigned types cannot mix
  if (node->type->is_unsigned != ty->is_unsigned)
    return (NULL);

  // The given type is smaller than the node's type, not possible
  if (ty->size < node->type->size)
    return (NULL);

  // We are left with widening the node
  newnode = mkastnode(A_WIDEN, node, NULL, NULL);
  newnode->type = ty;
  newnode->rvalue = true;
  return (newnode);
}

// If an AST node has no type, determine
// its type based on the child nodes.
void add_type(ASTnode * node) {
  ASTnode *newnode;

  // Do nothing if no node, or it already has a type
  if (node == NULL || node->type != NULL)
    return;

  // Set the child types if they have none
  add_type(node->left);
  add_type(node->right);

  // Try to widen each one to be the other's type
  newnode = widen_type(node->left, node->right->type, node->op);
  if (newnode != NULL)
    node->left = newnode;
  newnode = widen_type(node->right, node->left->type, node->op);
  if (newnode != NULL)
    node->right = newnode;

  // If a relational expression, it's boolean
  if ((node->op >= A_EQ) && node->op <= A_NOT) {
    node->type = ty_bool;
  } else
    // Use the left child's type
    node->type = node->left->type;
}

// Given an integer Litval , return a type that is suitable for it.
Type *parse_litval(Litval * e) {

  // If it's a float, return that
  if (e->numtype == NUM_FLT)
    return (ty_flt32);

  // Find the smallest suitable integer type for the value
  if (e->intval >= INT8_MIN && e->intval <= INT8_MAX)
    return (ty_int8);
  else if (e->intval >= INT16_MIN && e->intval <= INT16_MAX)
    return (ty_int16);
  else if (e->intval >= INT32_MIN && e->intval <= INT32_MAX)
    return (ty_int32);
  else if (e->numtype == NUM_INT)
    return (ty_int64);
  else
    return (ty_uint64);
}

// Return true if a type has a limited range
bool has_range(Type *ty) {
  return(ty->lower != 0 && ty->upper != 0);
}

// Given a symbol which represents a function,
// find and return a matching function pointer type
Type *get_funcptr_type(Sym *sym) {
  Type *this;
  Sym *psym;
  Paramtype *ptype;
  bool params_match;

  // Walk the list
  for (this = Typehead; this != NULL; this = this->next) {

    // Skip things that are not function pointers
    if (this->kind != TY_FUNCPTR) continue;

    // Skip if the return types do not match
    if (sym->type != this->rettype) continue;

    // Skip if the variadic flags differ
    if (sym->is_variadic != this->is_variadic) continue;

    // Now compare the parameter types
    ptype= this->paramtype;
    psym= sym->paramlist;
    params_match= true;
    while (1) {
      // We've run out of parameters to check
      if (ptype == NULL && psym == NULL) break;

      // Mismatch between number of parameters
      if (ptype == NULL && psym != NULL) { params_match= false; break; }
      if (ptype != NULL && psym == NULL) { params_match= false; break; }

      // Parameter types don't match
      if (ptype->type != psym->type) { params_match= false; break; }

      // They do match. Go up to the next in the list
      ptype= ptype->next; psym= psym->next;
    }

    if (params_match) break;
  }

  if (this==NULL)
    fatal("Need to declare a function pointer type to suit %s()\n", sym->name);
  return(this);
}
