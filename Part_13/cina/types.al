// Type functions for the alic compiler
// (c) 2025 Warren Toomey, GPL3

#define tyextern_
#include "alic.ah"
#include "proto.ah"

// Because we can't do & { struct initial values }
// in alic, we make Type variables and then get
// pointers to them

Type tystr_void  = { TY_VOID, 1, false, 0, NULL, NULL, NULL, NULL };
Type tystr_bool  = { TY_BOOL, 1, false, 0, NULL, NULL, NULL, NULL };

Type tystr_int8  = { TY_INT8,  1, false, 0, NULL, NULL, NULL, NULL };
Type tystr_int16 = { TY_INT16, 2, false, 0, NULL, NULL, NULL, NULL };
Type tystr_int32 = { TY_INT32, 4, false, 0, NULL, NULL, NULL, NULL };
Type tystr_int64 = { TY_INT64, 8, false, 0, NULL, NULL, NULL, NULL };

Type tystr_uint8 =  { TY_INT8,  1, true, 0, NULL, NULL, NULL, NULL };
Type tystr_uint16 = { TY_INT16, 2, true, 0, NULL, NULL, NULL, NULL };
Type tystr_uint32 = { TY_INT32, 4, true, 0, NULL, NULL, NULL, NULL };
Type tystr_uint64 = { TY_INT64, 8, true, 0, NULL, NULL, NULL, NULL };

Type tystr_flt32 = { TY_FLT32, 4, false, 0, NULL, NULL, NULL, NULL };
Type tystr_flt64 = { TY_FLT64, 8, false, 0, NULL, NULL, NULL, NULL };

// voidptr used by NULL, int8ptr used by strlits
Type tystr_voidptr = { TY_VOID, 8, false, 1, NULL, NULL, NULL, NULL };
Type tystr_int8ptr = { TY_INT8, 8, false, 1, NULL, NULL, NULL, NULL };


// Global variables
public Type *Typehead;
public Type *ty_void;
public Type *ty_bool;

public Type *ty_int8;
public Type *ty_int16;
public Type *ty_int32;
public Type *ty_int64;

public Type *ty_uint8;
public Type *ty_uint16;
public Type *ty_uint32;
public Type *ty_uint64;

public Type *ty_flt32;
public Type *ty_flt64;

public Type *ty_voidptr;
public Type *ty_int8ptr;

// Initialise the type list with the built-in types
public void init_typelist(void) {
  ty_void    = &tystr_void;
  ty_bool    = &tystr_bool;
  ty_int8    = &tystr_int8;
  ty_int16   = &tystr_int16;
  ty_int32   = &tystr_int32;
  ty_int64   = &tystr_int64;
  ty_uint8   = &tystr_uint8;
  ty_uint16  = &tystr_uint16;
  ty_uint32  = &tystr_uint32;
  ty_uint64  = &tystr_uint64;
  ty_flt32   = &tystr_flt32;
  ty_flt64   = &tystr_flt64;
  ty_voidptr = &tystr_voidptr;
  ty_int8ptr = &tystr_int8ptr;

  Typehead = ty_voidptr;
  ty_voidptr.next = ty_int8ptr;
  ty_int8ptr.next = ty_void;
  ty_void.next = ty_bool;
  ty_bool.next = ty_int8;
  ty_int8.next = ty_int16;
  ty_int16.next = ty_int32;
  ty_int32.next = ty_int64;
  ty_int64.next = ty_uint8;
  ty_uint8.next = ty_uint16;
  ty_uint16.next = ty_uint32;
  ty_uint32.next = ty_uint64;
  ty_uint64.next = ty_flt32;
  ty_flt32.next = ty_flt64;
  ty_flt64.next = NULL;
}

// Create a new Type struct and
// add it to the list of types
public Type *new_type(int kind, int size, int ptr_depth,
					char *name, Type * base) {
  Type *ty= NULL;
  Type *walktype;
  bool newnode= false;

  // See if this is an existing type.
  // If it is and it's not an opaque type, a problem
  if (name != NULL) {
    ty= find_type(name, 0, ptr_depth);
    if ((ty != NULL) && (ty.size > 0))
    fatal("type %s already exists\n", name);
  }

  // It doesn't exist, make a Type node
  if (ty == NULL) {
    ty = Calloc(sizeof(Type));
    newnode= true;
  }

  // Fill in the fields
  ty.kind = kind;
  ty.size = size;
  ty.ptr_depth = ptr_depth;
  ty.name = name;
  ty.basetype = base;

  // Add it to the list of types
  if (newnode == true) {
    if (Typehead != NULL)
      ty.next = Typehead;
    Typehead = ty;
  } else {
    // We've redefined an opaque type. Walk the
    // list of types. Find any type which points
    // to a type of this name and fill in the basetype
    for (walktype= Typehead; walktype != NULL; walktype= walktype.next) {
      if ((walktype.ptr_depth > 0) && (walktype.name!=NULL) &&
				strcmp(walktype.name, ty.name)==0) {
	walktype.basetype= ty;
	walktype.kind= ty.kind;
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
public Type *find_type(char *typename, int kind, int ptr_depth) {
  Type *this;

  if (typename != NULL) {
    // We have a name, so search for this name
    for (this = Typehead; this != NULL; this = this.next) {
      if (this.name != NULL && this.ptr_depth == ptr_depth
	  && strcmp(this.name, typename)==0) {
	// This type could be an alias.
	// If so, return the base type
	if (this.basetype != NULL && this.ptr_depth == 0)
	  return (this.basetype);
	else
	  return (this);
      }
    }
    return (NULL);
  } else {
    // Otherwise, search for the type kind
    for (this = Typehead; this != NULL; this = this.next) {
      if (this.kind == kind && this.ptr_depth == ptr_depth)
	return (this);
    }
  }

  return (NULL);
}

// Given a type pointer, return a type that
// represents a pointer to the argument
public Type *pointer_to(Type * ty) {
  Type *this;

  // Search for a pointer to this type
  this = find_type(ty.name, ty.kind, ty.ptr_depth + 1);
  if (this != NULL)
    return (this);

  // We didn't find one, so make one and return it.
  return (new_type
	  (ty.kind, PTR_SIZE, ty.ptr_depth + 1, ty.name, ty.basetype));
}

// Given a type pointer, return a type that
// represents the type that the argument points at
public Type *value_at(Type * ty) {
  Type *this;

  if (ty.ptr_depth == 0)
    fatal("Can't value_at() with depth zero!\n");

  // Search for the type that we point to
  this = find_type(ty.name, ty.kind, ty.ptr_depth - 1);
  if (this != NULL)
    return (this);

  // We didn't find one, so make one and return it.
  return (new_type
	  (ty.kind, PTR_SIZE, ty.ptr_depth - 1, ty.name, ty.basetype));
}

// Is this type an integer?
public bool is_integer(Type * ty) {
  int k = ty.kind;
  if (ty.ptr_depth != 0)
    return (false);
  return (k == TY_INT8 || k == TY_INT16 || k == TY_INT32 || k == TY_INT64);
}

// Is this type floating point?
public bool is_flonum(Type * ty) {
  if (ty.ptr_depth != 0)
    return (false);
  return (ty.kind == TY_FLT32 || ty.kind == TY_FLT64);
}

// Is this type numeric?
public bool is_numeric(Type * ty) {
  return (is_integer(ty) || is_flonum(ty));
}

// Is this type a pointer?
public bool is_pointer(Type * ty) {
  return (ty.ptr_depth != 0);
}

// Is this type a struct?
public bool is_struct(Type * ty) {
  return ((ty.ptr_depth == 0) && (ty.kind == TY_STRUCT));
}

// List of built-in types
char *typename[12] = {
  "void", "bool", "int8", "int16",
  "int32", "int64", "flt32", "flt64",
  "unsigned int8", "unsigned int16",
  "unsigned int32", "unsigned int64"
};

#define TYPELEN 255
char typenbuf[TYPELEN];

// Return a string representing the type.
public char *get_typename(Type * ty) {
  int i;

  if (ty.name != NULL)
    strcpy(typenbuf, ty.name);
  else {
    if (ty.is_unsigned)	// 6 is a magic number
      strcpy(typenbuf, typename[ty.kind + 6]);
    else
      strcpy(typenbuf, typename[ty.kind]);
  }

  if (ty.ptr_depth > 0) {
    strcat(typenbuf, " ");
    for (i = 0; i < ty.ptr_depth; i++)
      strcat(typenbuf, "*");
  }

  return (strdup(typenbuf));
}

// Given an ASTnode and a type, try to widen
// the node's type to match the given type.
// Return the same ASTnode if no widening is needed,
// or an ASTnode which widens the first one,
// or NULL if the types are not compatible.
public ASTnode *widen_type(ASTnode * node, Type * ty, int op) {
  ASTnode *newnode;
  Type *at_type;
  int size;

  // They have the same type, nothing to do
  if (node.ty == ty)
    return (node);

  // We can't widen to a boolean
  if (ty == ty_bool)
    return (NULL);

  // If both types are pointers
  if (is_pointer(ty) && is_pointer(node.ty)) {

    // We can widen from a void pointer
    if (node.ty == ty_voidptr) {
      node.ty = ty;
      return (node);
    }
    // We can widen to a void pointer
    if (ty == ty_voidptr)
      return (node);

    // Otherwise the pointers are incompatible
    return (NULL);
  }

  // We can't widen a pointer to a non-pointer
  if (!is_pointer(ty) && is_pointer(node.ty))
    return(NULL);

  // If the ty is a pointer and the node is an integer
  if (is_pointer(ty) && is_integer(node.ty)) {

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
			get_typename(node.ty));
	node= newnode;

        // Get the size of the type the pointer points at
        at_type = value_at(ty);
        size = at_type.size;
        if (size == 0)
          fatal("Cannot change a pointer to an opaque type\n");

        // Scale only when bigger than one
        if (size > 1) {
          node = unarop(node, A_SCALE);
          node.litval.intval = size;
          node.ty = ty;
        }
        return (node);

      case A_ADDOFFSET:
	// Change the node's type to be the pointer's
        node.ty = ty;
        return (node);

      default:
	fatal("Cannot mix an integer with a pointer, op %d\n", op);
    }
  }

  // We can't widen from a void
  if (node.ty == ty_void)
    fatal("cannot widen anything of type void\n");

  // Change an int of any size to a float
  if (is_integer(node.ty) && is_flonum(ty)) {
    newnode = mkastnode(A_CAST, node, NULL, NULL);
    newnode.ty = ty;
    newnode.rvalue = true;
    return (newnode);
  }

  // The given type is smaller than the node's type, do nothing
  if (ty.size < node.ty.size)
    return (node);

  // The node is a literal. We can update its
  // type without widening, but some rules apply
  if (node.op == A_NUMLIT) {
    // Check we're not trying to make a negative A_NUMLIT unsigned.
    if (ty.is_unsigned && !node.ty.is_unsigned
	&& node.litval.intval < 0)
      fatal("Cannot cast negative literal value %ld to be unsigned\n",
	    node.litval.intval);

    // Deal with changing int literals to float literals
    if (is_integer(node.ty) && is_flonum(ty))
      node.litval.dblval = node.litval.intval;

    node.ty = ty;
    return (node);
  }

  // Signed and unsigned types cannot mix
  if (node.ty.is_unsigned != ty.is_unsigned)
    return (NULL);

  // We are left with widening the node
  newnode = mkastnode(A_CAST, node, NULL, NULL);
  newnode.ty = ty;
  newnode.rvalue = true;
  return (newnode);
}

// If an AST node has no type, determine
// its type based on the child nodes.
void add_type(ASTnode * node) {
  ASTnode *newnode;

  // Do nothing if no node, or it already has a type
  if (node == NULL || node.ty != NULL)
    return;

  // Set the child types if they have none
  add_type(node.left);
  add_type(node.right);

  // Try to widen each one to be the other's type
  newnode = widen_type(node.left, node.right.ty, node.op);
  if (newnode != NULL)
    node.left = newnode;
  newnode = widen_type(node.right, node.left.ty, node.op);
  if (newnode != NULL)
    node.right = newnode;

  // If a relational expression, it's boolean
  if ((node.op >= A_EQ) && node.op <= A_NOT) {
    node.ty = ty_bool;
  } else
    // Use the left child's type
    node.ty = node.left.ty;
}

// Given an integer Litval , return a type that is suitable for it.
Type *parse_litval(Litval * e) {

  // If it's a float, return that
  if (e.numtype == NUM_FLT)
    return (ty_flt32);

  // Find the smallest suitable integer type for the value
  if (e.intval >= SCHAR_MIN && e.intval <= SCHAR_MAX)
    return (ty_int8);
  else if (e.intval >= SHRT_MIN && e.intval <= SHRT_MAX)
    return (ty_int16);
  else if (e.intval >= INT_MIN && e.intval <= INT_MAX)
    return (ty_int32);
  else if (e.numtype == NUM_INT)
    return (ty_int64);
  else
    return (ty_uint64);
}
