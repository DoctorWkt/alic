// QBE code generator for the alic compiler.
// (c) 2025 Warren Toomey, GPL3

#include <stdio.h>
#include <stdlib.h>
#include "alic.h"
#include "proto.h"

extern FILE *outfh;
extern Type *ty_int32;
extern Type *ty_uint32;

static int nexttemp = 1;	// Incrementing temporary number

// Allocate a QBE temporary
static int cgalloctemp(void) {
  return (++nexttemp);
}

// Generate a label
void cglabel(int l) {
  fprintf(outfh, "@L%d\n", l);
}

// Generate a string literal
void cgstrlit(int label, char *val) {
  fprintf(outfh, "data $L%d = { b \"%s\", b 0 }\n", label, val);

}

// Generate a jump to a label
void cgjump(int l) {
  fprintf(outfh, "  jmp @L%d\n", l);
}

// Table of QBE type names used
// after the '=' sign in instructions
static char *qbe_typename[TY_FLT64 + 1] = {
  "", "w", "w", "w", "w", "l", "s", "d"
};

// Table of QBE type names used
// in store instructions
static char *qbe_storetypename[TY_FLT64 + 1] = {
  "", "b", "b", "h", "w", "l", "s", "d"
};

// Table of QBE type names used when loading.
// Second half represents unsigned types
static char *qbe_loadtypename[2 * (TY_FLT64 + 1)] = {
  "", "sb", "sb", "sh", "sw", "l", "s", "d",
  "", "ub", "ub", "uh", "uw", "l", "s", "d",
};

// Table of QBE type names used when extending.
// Second half represents unsigned types
static char *qbe_exttypename[2 * (TY_FLT64 + 1)] = {
  "", "sw", "sw", "sw", "sw", "sl", "s", "d",
  "", "uw", "uw", "uw", "uw", "ul", "s", "d",
};

// Return the QBE type that
// matches the given built-in type
static char *qbetype(Type * type) {
  int kind = type->kind;

  if (type->kind > TY_FLT64) fatal("not a built-in type");
  if (type->kind == TY_VOID) fatal("no QBE void type");
  return (qbe_typename[kind]);
}

// Ditto for stores
static char *qbe_storetype(Type * type) {
  int kind = type->kind;

  if (type->kind > TY_FLT64) fatal("not a built-in type");
  if (type->kind == TY_VOID) fatal("no QBE void type");
  return (qbe_storetypename[kind]);
}

// Ditto for loads, with signed knowledge
static char *qbe_loadtype(Type * type) {
  int kind = type->kind;

  if (type->kind > TY_FLT64) fatal("not a built-in type");
  if (type->kind == TY_VOID) fatal("no QBE void type");
  if (type->is_unsigned) kind += TY_FLT64 + 1;
  return (qbe_loadtypename[kind]);
}

// Ditto for extends, with signed knowledge
static char *qbe_exttype(Type * type) {
  int kind = type->kind;

  if (type->kind > TY_FLT64) fatal("not a built-in type");
  if (type->kind == TY_VOID) fatal("no QBE void type");
  if (type->is_unsigned) kind += TY_FLT64 + 1;
  return (qbe_exttypename[kind]);
}

// Print out the file preamble
void cg_file_preamble(void) {
}

// Print out the function preamble
void cg_func_preamble(char *name) {
  fprintf(outfh, "export function $%s() {\n", name);
  fprintf(outfh, "@START\n");
}

// Print out the function postamble
void cg_func_postamble(void) {
  fprintf(outfh, "@END\n");
  fprintf(outfh, "  ret\n");
  fprintf(outfh, "}\n");
}

// Define a global symbol
void cgglobsym(Sym * s) {
  if (s == NULL) return;

  // Get the matching QBE type
  char *qtype = qbe_storetype(s->type);

  switch (s->type->kind) {
  case TY_FLT32:
  case TY_FLT64:
    fprintf(outfh, "export data $%s = { %s %s_%f, }\n",
	    s->name, qtype, qtype, s->initval.dblval);
    break;
  default:
    fprintf(outfh, "export data $%s = { %s %ld, }\n",
	    s->name, qtype, s->initval.intval);
  }
}

// Print out temporary's value using printf()
void cgprint(int label, int temp, Type *type) {
  // Get the matching QBE type
  char *qtype = qbetype(type);

  fprintf(outfh, "  call $printf(l $L%d, %s %%.t%d)\n",
		label, qtype, temp);
}

// Load an integer literal value into a temporary.
// Return the number of the temporary.
int cgloadlit(Litval value, Type * type) {
  // Get a new temporary
  int t = cgalloctemp();

  // Get the matching QBE type
  char *qtype = qbetype(type);

  switch (type->kind) {
  case TY_FLT32:
  case TY_FLT64:
    fprintf(outfh, "  %%.t%d =%s copy %s_%f\n", t, qtype, qtype,
	    value.dblval);
    break;
  default:
    fprintf(outfh, "  %%.t%d =%s copy %ld\n", t, qtype, value.intval);
  }
  return (t);
}

// Perform a binary operation on two temporaries and
// return the number of the temporary with the result
static int cgbinop(int t1, int t2, char *op, Type * type) {
  // Get the matching QBE type
  char *qtype = qbetype(type);

  fprintf(outfh, "  %%.t%d =%s %s %%.t%d, %%.t%d\n", t1, qtype, op, t1, t2);
  return (t1);
}

// Add two temporaries together and return
// the number of the temporary with the result
int cgadd(int t1, int t2, Type * type) {
  return (cgbinop(t1, t2, "add", type));
}

// Subtract the second temporary from the first and
// return the number of the temporary with the result
int cgsub(int t1, int t2, Type * type) {
  return (cgbinop(t1, t2, "sub", type));
}

// Multiply two temporaries together and return
// the number of the temporary with the result
int cgmul(int t1, int t2, Type * type) {
  return (cgbinop(t1, t2, "mul", type));
}

// Divide the first temporary by the second and
// return the number of the temporary with the result
int cgdiv(int t1, int t2, Type * type) {
  return (cgbinop(t1, t2, "div", type));
}

// Negate a temporary's value
int cgnegate(int t, Type * type) {
  fprintf(outfh, "  %%.t%d =%s sub 0, %%.t%d\n", t, qbetype(type), t);
  return (t);
}

// List of QBE comparison operations. Add 6 for unsigned
static char *qbecmp[] = {
  "eq", "ne", "slt", "sgt", "sle", "sge",
  "eq", "ne", "slt", "sgt", "sle", "sge"
};

// Compare two temporaries and return the boolean result
int cgcompare(int op, int t1, int t2, Type * type) {
  // Get the matching QBE type
  char *qtype = qbetype(type);

  // Get the QBE comparison
  int offset = type->is_unsigned ? 6 : 0;
  char *cmpstr = qbecmp[op - A_EQ + offset];

  // Get a new temporary
  int t = cgalloctemp();

  fprintf(outfh, "  %%.t%d =w c%s%s %%.t%d, %%.t%d\n",
	  t, cmpstr, qtype, t1, t2);
  return (t);
}

// Jump to the label if the value in t1 is zero
void cgjump_if_false(int t1, int label) {
  // Get a label for the next instruction
  int label2 = genlabel();

  fprintf(outfh, "  jnz %%.t%d, @L%d, @L%d\n", t1, label2, label);
  cglabel(label2);
}

// Logically NOT a temporary's value
int cgnot(int t, Type * type) {
  // Get the matching QBE type
  char *qtype = qbetype(type);

  fprintf(outfh, "  %%.t%d =%s ceq%s %%.t%d, 0\n", t, qtype, qtype, t);
  return (t);
}

// Invert a temporary's value
int cginvert(int t, Type * type) {
  fprintf(outfh, "  %%.t%d =%s xor %%.t%d, -1\n", t, qbetype(type), t);
  return (t);
}

// Bitwise AND two temporaries together and return
// the number of the temporary with the result
int cgand(int t1, int t2, Type * type) {
  return (cgbinop(t1, t2, "and", type));
}

// Bitwise OR two temporaries together and return
// the number of the temporary with the result
int cgor(int t1, int t2, Type * type) {
  return (cgbinop(t1, t2, "or", type));
}

// Bitwise XOR two temporaries together and return
// the number of the temporary with the result
int cgxor(int t1, int t2, Type * type) {
  return (cgbinop(t1, t2, "xor", type));
}

// Shift left t1 by t2 bits
int cgshl(int t1, int t2, Type * type) {
  return (cgbinop(t1, t2, "shl", type));
}

// Shift right t1 by t2 bits
int cgshr(int t1, int t2, Type * type) {
  return (cgbinop(t1, t2, "shr", type));
}

// Load a value from a variable into a temporary.
// Return the number of the temporary.
int cgloadvar(Sym * sym) {
  // Allocate a new temporary
  int t = cgalloctemp();

  // Get the matching QBE type
  char *qloadtype = qbe_loadtype(sym->type);
  char *qtype = qbetype(sym->type);

  fprintf(outfh, "  %%.t%d =%s load%s %%%s\n", t, qtype, qloadtype, sym->name);
  return (t);
}

void cgstorvar(int t, Type * exprtype, Sym * sym) {
  // Get the matching QBE type
  char *qtype = qbe_storetype(sym->type);

  fprintf(outfh, "  store%s %%.t%d, %%%s\n", qtype, t, sym->name);
}

// Cast a temporary to have a new type
int cgcast(int t1, Type * type, Type * newtype) {
  // Allocate a new temporary
  int t2 = cgalloctemp();

  // As t1 is already word-sized,
  // we can upgrade the alic type for t1
  switch (type->kind) {
  case TY_BOOL:
  case TY_INT8:
  case TY_INT16:
    if (type->is_unsigned) type = ty_uint32;
    else		   type = ty_int32;
    break;
  default:
  }

  // Get the matching QBE types
  char *oldqtype = qbe_exttype(type);
  char *newqtype = qbetype(newtype);

  // Conversion from int to flt
  if (is_integer(type) && is_flonum(newtype)) {
    fprintf(outfh, "  %%.t%d =%s %stof %%.t%d\n", t2, newqtype, oldqtype, t1);
    return (t2);
  }

  // Widening
  if (newtype->size > type->size) {
    switch (type->kind) {
    case TY_INT32:
      fprintf(outfh, "  %%.t%d =%s ext%s %%.t%d\n",
	      t2, newqtype, oldqtype, t1);
      break;
    case TY_FLT32:
      fprintf(outfh, "  %%.t%d =%s ext%s %%.t%d\n",
	      t2, newqtype, oldqtype, t1);
      break;
    default:
      fatal("Not sure how to widen from %s to %s\n",
	    get_typename(type), get_typename(newtype));
    }
    return (t2);
  }

  // Narrowing
  if (newtype->size < type->size) {
    switch (type->kind) {
    case TY_INT32:
      return (t1);
    default:
      fatal("Not sure how to narrow from %s to %s\n",
	    get_typename(type), get_typename(newtype));
    }
    return (t2);
  }

  // We didn't narrow or widen!
  return (t1);
}

// Add space for a local variable
void cgaddlocal(Type *type, Sym *sym) {
  // Get the type's size.
  // Make it at least 4 bytes in size
  // as QBE requires this for a variable
  // on the stack
  int size= (type->size < 4) ? 4 : type->size;

  fprintf(outfh, "  %%%s =l alloc%d 1\n", sym->name, size);
}
