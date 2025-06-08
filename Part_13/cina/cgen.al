// QBE code generator for the alic compiler.
// (c) 2025 Warren Toomey, GPL3

#include "alic.ah"
#include "proto.ah"

int nexttemp = 1;	// Incrementing temporary number

// Allocate a QBE temporary
int cgalloctemp(void) {
  nexttemp++;
  return (nexttemp);
}

// Generate a label
void cglabel(int l) {
  fprintf(Outfh, "@L%d\n", l);
}

// Generate a string literal
void cgstrlit(int label, char *val) {
  char *cptr;
  fprintf(Outfh, "data $L%d = { ", label);

  for (cptr = val; *cptr != 0; cptr++) {
    fprintf(Outfh, "b %d, ", *cptr);
  }

  fprintf(Outfh,  "b 0 }\n");
}

// Generate a jump to a label
void cgjump(int l) {
  fprintf(Outfh, "  jmp @L%d\n", l);
}

// Table of QBE type names used
// after the '=' sign in instructions
char *qbe_typename[8] = {
  "", "w", "w", "w", "w", "l", "s", "d"
};

// Table of QBE type names used
// in store instructions
char *qbe_storetypename[8] = {
  "", "b", "b", "h", "w", "l", "s", "d"
};

// Table of QBE type names used when loading.
// Second half represents unsigned types
char *qbe_loadtypename[16] = {
  "", "sb", "sb", "sh", "sw", "l", "s", "d",
  "", "ub", "ub", "uh", "uw", "l", "s", "d"
};

// Table of QBE type names used when extending.
// Second half represents unsigned types
char *qbe_exttypename[16] = {
  "", "sw", "sw", "sw", "sw", "sl", "s", "d",
  "", "uw", "uw", "uw", "uw", "ul", "s", "d"
};

// Return the QBE type that
// matches the given built-in type
char *qbetype(Type * ty) {
  int kind = ty.kind;

  if (is_pointer(ty))
    return ("l");
  if (ty.kind > TY_FLT64)
    fatal("%s not a built-in type\n", get_typename(ty));

  return (qbe_typename[kind]);
}

// Ditto for stores
char *qbe_storetype(Type * ty) {
  int kind = ty.kind;

  if (ty.ptr_depth > 0)
    return ("l");
  if (ty.kind > TY_FLT64)
    fatal("%s not a built-in type\n", get_typename(ty));
  if (ty.kind == TY_VOID)
    fatal("no QBE void type");

  return (qbe_storetypename[kind]);
}

// Ditto for loads, with signed knowledge
char *qbe_loadtype(Type * ty) {
  int kind = ty.kind;

  if (ty.ptr_depth > 0)
    return ("l");
  if (ty.kind > TY_FLT64)
    fatal("%s not a built-in type\n", get_typename(ty));
  if (ty.kind == TY_VOID)
    fatal("no QBE void type");
  if (ty.is_unsigned)
    kind = kind + TY_FLT64 + 1;

  return (qbe_loadtypename[kind]);
}

// Ditto for extends, with signed knowledge
char *qbe_exttype(Type * ty) {
  int kind = ty.kind;

  if (ty.kind > TY_FLT64)
    fatal("%s not a built-in type\n", get_typename(ty));
  if (ty.kind == TY_VOID)
    fatal("no QBE void type");
  if (ty.is_unsigned)
    kind = kind + TY_FLT64 + 1;

  return (qbe_exttypename[kind]);
}

// Given a type and the current possible offset
// of a member of this type in a struct, return
// the correct offset for the member
// requirement in bytes
int cgalign(Type * ty, int offset) {
  int alignment = 1;

  // Pointers are 8-byte aligned
  if (ty.ptr_depth > 0)
    alignment = 8;
  else {
    // Structs: use the type of the first member
    if (ty.kind == TY_STRUCT)
      ty = ty.memb.ty;

    switch (ty.kind) {
    case TY_BOOL:
    case TY_INT8:
      return (offset);
    case TY_INT16:
      alignment = 2;
    case TY_INT32:
    case TY_FLT32:
      alignment = 4;
    case TY_INT64:
    case TY_FLT64:
      alignment = 8;
    case TY_VOID:
    case TY_USER:
    case TY_STRUCT:
      fatal("No QBE size for type kind %d\n", ty.kind);
    }
  }

  // Calculate the new offset
  offset = (offset + (alignment - 1)) & (~(alignment - 1));
  return (offset);
}

// Print out the file preamble
void cg_file_preamble(void) {
  // Output a copy of the function that emits
  // an error message and exit()s
fputs("function $.fatal(l %.t1, ...) {\n", Outfh);
  fputs("@L1\n", Outfh);
  fputs("  %.t2 =l alloc8 24\n", Outfh);
  fputs("  vastart %.t2\n", Outfh);
  fputs("  %.t3 =l loadl $stderr\n", Outfh);
  fputs("  call $vfprintf(l %.t3, l %.t1, l %.t2)\n", Outfh);
  fputs("  call $exit(w 1)\n", Outfh);
  fputs("  ret \n", Outfh);
  fputs("}\n\n", Outfh);


  fputs("data $.bounderr = { b \"%s[%d] out of bounds in %s()\\n\", b 0 }\n\n", Outfh);
  fputs("data $.unsignerr = { b \"can't make %d unsigned in %s()\\n\", b 0 }\n\n", Outfh);
}

// Temporary which holds the vastart argument list
int va_ptr;

// Print out the function preamble
void cg_func_preamble(Sym * func) {
  Sym *this;
  char *qtype;

  // No va_ptr as yet
  va_ptr= NOTEMP;

  // Get the function's return type
  qtype = qbetype(func.ty);

  if (func.visibility == SV_PUBLIC)
    fprintf(Outfh, "export ");
  fprintf(Outfh, "function %s $%s(", qtype, func.name);

  // If we have an exception variable, output it
  if (func.exceptvar != NULL) {
    fprintf(Outfh, "l %%%s", func.exceptvar.name);
    if (func.paramlist != NULL)
      fprintf(Outfh, ", ");
  }

  // Output the list of parameters
  for (this = func.paramlist; this != NULL; this = this.next) {
    // Get the parameter's type
    qtype = qbetype(this.ty);
    fprintf(Outfh, "%s %%%s", qtype, this.name);

    // Print out any comma separator
    if (this.next != NULL)
      fprintf(Outfh, ", ");
  }

  // Print ... if the function is variadic
  if (func.is_variadic == true)
    fprintf(Outfh, ", ...");

  fprintf(Outfh, ") {\n");
  fprintf(Outfh, "@START\n");
}

// Print out the function postamble
void cg_func_postamble(Type * ty) {
  fprintf(Outfh, "@END\n");

  // Return a value if the function's type isn't void
  if (ty != ty_void)
    fprintf(Outfh, "  ret %%.ret\n");
  else
    fprintf(Outfh, "  ret\n");
  fprintf(Outfh, "}\n\n");
}

// Used when outputting storage
// for global structs
int globoffset;

// Start a global symbol.
void cgglobsym(Sym * sym, bool make_zero) {
  int align;
  int size;
  int power = 1;
  Type *ty = sym.ty;

  globoffset = 0;

  // We can't declare it when it is opaque (zero size)
  if (sym.ty.size == 0)
    fatal("Can't declare %s as size zero\n", sym.name);

  // Export the variable if public.
  // Private variables are not exported
  if (sym.visibility == SV_PUBLIC)
    fprintf(Outfh, "export ");

  // If the data is 8 bytes or more,
  // align it on an 8-byte boundary
  if (ty.size >= 8)
    align = 8;
  else {
    // Determine the next biggest (or equal)
    // power of two given the size
    while (power < ty.size)
      power = power * 2;
    align = power;
  }

  fprintf(Outfh, "data $%s = align %d { ", sym.name, align);

  if (make_zero == true) {
    // Arrays. Get the type of each element and
    // multiply the type size by the number of elements
    if (is_array(sym)) {
      ty = value_at(ty);
      size = sym.count * ty.size;
    } else {
      size = ty.size;
    }
    fprintf(Outfh, "z %d", size);
  }
}

// Add a value to a global symbol
void cgglobsymval(ASTnode * value, int offset) {
  char *qtype;
  int label;

  // If the offset is bigger than the current offset,
  // output some zero padding
  if (offset > globoffset) {
    fprintf(Outfh, "z %d, ", offset - globoffset);
    globoffset = offset;
  }

  qtype = qbe_storetype(value.ty);

  // Update the globoffset to match the
  // amount of data we will output
  globoffset = globoffset + value.ty.size;

  // No initial value, use 0
  if (value == NULL) {
    if (value.ty.kind == TY_STRUCT)
      fprintf(Outfh, "z %d, ", value.ty.size);
    else if (is_flonum(value.ty))
      fprintf(Outfh, "%s s_0.0, ", qtype);
    else
      fprintf(Outfh, "%s 0, ", qtype);

    return;
  }

  // We have a value
  if (value.op == A_STRLIT) {
    label= add_strlit(value.strlit);
    fprintf(Outfh, "%s $L%d, ", qtype, label);
  } else if (is_flonum(value.ty))
    fprintf(Outfh, "%s s_%f, ", qtype, value.litval.dblval);
  else
    fprintf(Outfh, "%s %ld, ", qtype, value.litval.intval);
}

// End a global symbol
void cgglobsymend(Sym * sym) {
  fprintf(Outfh, " }\n");
}

// Load a boolean value (only 0 or 1)
// into the given temporary
void cgloadboolean(int t, int val, Type * ty) {
  char *qtype = qbetype(ty);
  fprintf(Outfh, "  %%.t%d =%s copy %d\n", t, qtype, val);
}

// Load an integer literal value into a temporary.
// Return the number of the temporary.
int cgloadlit(Litval * value, Type * ty) {
  char *qtype;

  // Get a new temporary
  int t = cgalloctemp();

  // Deal with pointers
  if (is_pointer(ty)) {
    fprintf(Outfh, "  %%.t%d =l copy %ld\n", t, value.intval);
    return (t);
  }

  // Get the matching QBE type
  qtype = qbetype(ty);

  switch (ty.kind) {
  case TY_FLT32:
  case TY_FLT64:
    fprintf(Outfh, "  %%.t%d =%s copy %s_%f\n", t, qtype, qtype,
	    value.dblval);
  default:
    fprintf(Outfh, "  %%.t%d =%s copy %ld\n", t, qtype, value.intval);
  }

  return (t);
}

// Perform a binary operation on two temporaries and
// return the number of the temporary with the result
int cgbinop(int t1, int t2, char *op, Type * ty) {
  // Get the matching QBE type
  char *qtype = qbetype(ty);

  fprintf(Outfh, "  %%.t%d =%s %s %%.t%d, %%.t%d\n", t1, qtype, op, t1, t2);
  return (t1);
}

// Add two temporaries together and return
// the number of the temporary with the result
int cgadd(int t1, int t2, Type * ty) {
  return (cgbinop(t1, t2, "add", ty));
}

// Subtract the second temporary from the first and
// return the number of the temporary with the result
int cgsub(int t1, int t2, Type * ty) {
  return (cgbinop(t1, t2, "sub", ty));
}

// Multiply two temporaries together and return
// the number of the temporary with the result
int cgmul(int t1, int t2, Type * ty) {
  return (cgbinop(t1, t2, "mul", ty));
}

// Divide the first temporary by the second and
// return the number of the temporary with the result
int cgdiv(int t1, int t2, Type * ty) {
  return (cgbinop(t1, t2, "div", ty));
}

// Get the modulo of the first temporary by the second and
// return the number of the temporary with the result
int cgmod(int t1, int t2, Type * ty) {
  return (cgbinop(t1, t2, "rem", ty));
}

// Negate a temporary's value
int cgnegate(int t, Type * ty) {
  fprintf(Outfh, "  %%.t%d =%s sub 0, %%.t%d\n", t, qbetype(ty), t);
  return (t);
}

// List of QBE comparison operations. Add 6 for unsigned
char *qbecmp[12] = {
  "eq", "ne", "slt", "sgt", "sle", "sge",
  "eq", "ne", "slt", "sgt", "sle", "sge"
};

// Compare two temporaries and return the boolean result
int cgcompare(int op, int t1, int t2, Type * ty) {
  // Get the matching QBE type
  char *qtype = qbetype(ty);

  // Get the QBE comparison
  int offset = (ty.is_unsigned) ? 6 : 0;
  char *cmpstr = qbecmp[op - A_EQ + offset];

  // Get a new temporary
  int t = cgalloctemp();

  fprintf(Outfh, "  %%.t%d =w c%s%s %%.t%d, %%.t%d\n",
	  t, cmpstr, qtype, t1, t2);
  return (t);
}

// Jump to the label if the value in t1 is zero
void cgjump_if_false(int t1, int label) {
  // Get a label for the next instruction
  int label2 = genlabel();

  fprintf(Outfh, "  jnz %%.t%d, @L%d, @L%d\n", t1, label2, label);
  cglabel(label2);
}

// Logically NOT a temporary's value
int cgnot(int t, Type * ty) {
  // Get the matching QBE type
  char *qtype = qbetype(ty);

  fprintf(Outfh, "  %%.t%d =%s ceq%s %%.t%d, 0\n", t, qtype, qtype, t);
  return (t);
}

// Invert a temporary's value
int cginvert(int t, Type * ty) {
  fprintf(Outfh, "  %%.t%d =%s xor %%.t%d, -1\n", t, qbetype(ty), t);
  return (t);
}

// Bitwise AND two temporaries together and return
// the number of the temporary with the result
int cgand(int t1, int t2, Type * ty) {
  return (cgbinop(t1, t2, "and", ty));
}

// Bitwise OR two temporaries together and return
// the number of the temporary with the result
int cgor(int t1, int t2, Type * ty) {
  return (cgbinop(t1, t2, "or", ty));
}

// Bitwise XOR two temporaries together and return
// the number of the temporary with the result
int cgxor(int t1, int t2, Type * ty) {
  return (cgbinop(t1, t2, "xor", ty));
}

// Shift left t1 by t2 bits
int cgshl(int t1, int t2, Type * ty) {
  return (cgbinop(t1, t2, "shl", ty));
}

// Shift right t1 by t2 bits
int cgshr(int t1, int t2, Type * ty) {
  return (cgbinop(t1, t2, "shr", ty));
}

// Load a value from a variable into a temporary.
// Return the number of the temporary.
int cgloadvar(Sym * sym) {
  char qbeprefix = (sym.visibility == SV_LOCAL) ? '%' : '$';

  // Allocate a new temporary
  int t = cgalloctemp();

  // Get the matching QBE type
  char *qloadtype = qbe_loadtype(sym.ty);
  char *qtype = qbetype(sym.ty);

  // If it has an address and isn't an array
  if ((sym.has_addr) && !is_array(sym))
    fprintf(Outfh, "  %%.t%d =%s load%s %c%s\n",
	    t, qtype, qloadtype, qbeprefix, sym.name);
  else
    fprintf(Outfh, "  %%.t%d =%s copy %c%s\n",
	    t, qtype, qbeprefix, sym.name);

  return (t);
}

int cgstorvar(int t, Type * exprtype, Sym * sym) {
  char qbeprefix = (sym.visibility == SV_LOCAL) ? '%' : '$';

  // Get the matching QBE type
  char *qtype = qbe_storetype(sym.ty);

  if (sym.has_addr)
    fprintf(Outfh, "  store%s %%.t%d, %c%s\n", qtype, t, qbeprefix,
	    sym.name);
  else
    fprintf(Outfh, "  %c%s =%s copy %%.t%d\n",
	    qbeprefix, sym.name, qtype, t);

  return (NOTEMP);
}

// Cast a temporary to have a new type
int cgcast(int t1, Type * ty, Type * newtype) {
  char *oldqtype;
  char *newqtype;

  // Allocate a new temporary
  int t2 = cgalloctemp();

  // As t1 is already word-sized,
  // we can upgrade the alic type for t1
  switch (ty.kind) {
  case TY_BOOL:
  case TY_INT8:
  case TY_INT16:
    if (ty.is_unsigned)
      ty = ty_uint32;
    else
      ty = ty_int32;
  default:
  }

  // Get the matching QBE types
  oldqtype = qbe_exttype(ty);
  newqtype = qbetype(newtype);

  // Conversion from int to flt
  if (is_integer(ty) && is_flonum(newtype)) {
    fprintf(Outfh, "  %%.t%d =%s %stof %%.t%d\n", t2, newqtype, oldqtype, t1);
    return (t2);
  }

  // Widening
  if (newtype.size > ty.size) {
    switch (ty.kind) {
    case TY_INT32:
      fprintf(Outfh, "  %%.t%d =%s ext%s %%.t%d\n",
	      t2, newqtype, oldqtype, t1);
    case TY_FLT32:
      fprintf(Outfh, "  %%.t%d =%s ext%s %%.t%d\n",
	      t2, newqtype, oldqtype, t1);
    default:
      fatal("Not sure how to widen from %s to %s\n",
	    get_typename(ty), get_typename(newtype));
    }
    return (t2);
  }

  // Narrowing
  if (newtype.size < ty.size) {
    switch (ty.kind) {
    case TY_INT32:
      return (t1);
    default:
      fatal("Not sure how to narrow from %s to %s\n",
	    get_typename(ty), get_typename(newtype));
    }
    return (t2);
  }

  // We didn't narrow or widen!
  return (t1);
}

// Add space for a local variable
void cgaddlocal(Type * ty, Sym * sym, int size, bool makezero, bool isarray) {
  int align = 8;
  int temp = cgalloctemp();
  int t2 = cgalloctemp();
  char *name = sym.name;

  // Get a suitable alignment and allocate stack space
  if (size < 8)
    align = 4;

  fprintf(Outfh, "  %%%s =l alloc%d %d\n", sym.name, align, size);

  // No need to zero the space
  if (makezero == false)
    return;

  // Yes, zero the space
  switch (size) {
  case 1:
    fprintf(Outfh, "  %%.t%d =w copy 0\n", temp);
    fprintf(Outfh, "  storeb %%.t%d, %%%s\n", temp, name);
  case 2:
    fprintf(Outfh, "  %%.t%d =w copy 0\n", temp);
    fprintf(Outfh, "  storeh %%.t%d, %%%s\n", temp, name);
  case 4:
    fprintf(Outfh, "  %%.t%d =w copy 0\n", temp);
    fprintf(Outfh, "  storew %%.t%d, %%%s\n", temp, name);
  case 8:
    fprintf(Outfh, "  %%.t%d =l copy 0\n", temp);
    fprintf(Outfh, "  storel %%.t%d, %%%s\n", temp, name);
  default:
    fprintf(Outfh, "  %%.t%d =l copy 0\n", temp);
    fprintf(Outfh, "  %%.t%d =l copy %d\n", t2, size);
    fprintf(Outfh, "  call $memset(l %%%s, l %%.t%d, l %%.t%d)\n",
	    name, temp, t2);
  }
}

// Call a function with the given symbol id.
// Return the temporary with the result
int cgcall(Sym * sym, int numargs, int excepttemp, int *arglist,
	   Type ** typelist) {
  int rettemp = NOTEMP;
  int i;

  // Call the function
  if (sym.ty == ty_void)
    fprintf(Outfh, "  call $%s(", sym.name);
  else {
    // Get a new temporary for the return result
    rettemp = cgalloctemp();

    fprintf(Outfh, "  %%.t%d =%s call $%s(",
	    rettemp, qbetype(sym.ty), sym.name);
  }

  // If the function has an exception variable, output it
  // Use count as the id of the temporary holding its value
  if (sym.exceptvar != NULL) {
    fprintf(Outfh, "l %%.t%d", excepttemp);
    if (numargs != 0)
      fprintf(Outfh, ", ");
  }

  // Output the list of arguments
  for (i = 0; i < numargs; i++) {
    fprintf(Outfh, "%s %%.t%d", qbetype(typelist[i]), arglist[i]);

    // If the function is variadic, QBE requires a '...'
    // after the last non-variadic argument
    if (i == sym.count - 1)
      fprintf(Outfh, ", ... ");

    // Output any separating comma
    if (i < numargs - 1)
      fprintf(Outfh, ", ");
  }

  fprintf(Outfh, ")\n");
  return (rettemp);
}

// Generate code to return a value from a function
void cgreturn(int temp, Type * ty) {

  // Only return a value if the function is not void
  if (ty != ty_void)
    fprintf(Outfh, "  %%.ret =%s copy %%.t%d\n", qbetype(ty), temp);

  fprintf(Outfh, "  jmp @END\n");

  // QBE needs a label after a jump
  cglabel(genlabel());
}

// Abort from the function
void cgabort(void) {

  // QBE needs a label after a jump
  fprintf(Outfh, "  jmp @END\n");
  cglabel(genlabel());
}

// Given the label number of a global string,
// load its address into a new temporary
int cgloadglobstr(int label) {
  // Get a new temporary
  int t = cgalloctemp();
  fprintf(Outfh, "  %%.t%d =l copy $L%d\n", t, label);
  return (t);
}

// Generate code to load the address of an
// identifier. Return a new temporary
int cgaddress(Sym * sym) {
  int r = cgalloctemp();
  char qbeprefix = (sym.visibility == SV_LOCAL) ? '%' : '$';

  fprintf(Outfh, "  %%.t%d =l copy %c%s\n", r, qbeprefix, sym.name);
  return (r);
}

// Dereference a pointer to get the value
// it points at into a new temporary
int cgderef(int t, Type * ty) {

  // Get the matching QBE type and load type
  char *qtype = qbetype(ty);
  char *qloadtype = qbe_loadtype(ty);

  // Get a temporary for the return result
  int ret = cgalloctemp();

  fprintf(Outfh, "  %%.t%d =%s load%s %%.t%d\n", ret, qtype, qloadtype, t);
  return (ret);
}

int cgstorderef(int t1, int t2, Type * ty) {
  // Get the matching QBE type
  char *qtype = qbe_storetype(ty);

  fprintf(Outfh, "  store%s %%.t%d, %%.t%d\n", qtype, t1, t2);
  return (NOTEMP);
}

// Do a bounds check on t1's value. If below zero
// or >= count, call a function that will exit()
// the program. Otherwise return t1's value.
int cgboundscheck(int t1, int count, int aryname, int funcname) {
  int counttemp = cgalloctemp();
  int comparetemp = cgalloctemp();
  int zerotemp = cgalloctemp();
  int Lgood = genlabel();
  int Lfail = genlabel();

  // Get the count into a temporary
  fprintf(Outfh, "  %%.t%d =l copy %d\n", counttemp, count);

  // Compare against the index value
  // Jump if false to the failure label
  comparetemp = cgcompare(A_LT, t1, counttemp, ty_int64);
  cgjump_if_false(comparetemp, Lfail);

  // Get zero into a temporary
  fprintf(Outfh, "  %%.t%d =l copy 0\n", zerotemp);

  // Compare against the index value
  // Jump if false to the failure label
  // Otherwise jump to the good label
  comparetemp = cgcompare(A_GE, t1, zerotemp, ty_int64);
  cgjump_if_false(comparetemp, Lfail);
  cgjump(Lgood);

  // Call the failure function
  cglabel(Lfail);
  fprintf(Outfh, "  call $.fatal(l $.bounderr, l $L%d, l %%.t%d, l $L%d)\n",
	  aryname, t1, funcname);
  cglabel(Lgood);

  return (t1);
}

void cgmove(int t1, int t2, Type * ty) {
  fprintf(Outfh, "  %%.t%d =%s copy %%.t%d\n", t2, qbetype(ty), t1);
}

// Allocate space for the variable argument list
void cg_vastart(ASTnode *n) {

  // It's already been done
  if (va_ptr != NOTEMP)
    return;

  va_ptr= cgalloctemp();

  // Allocate the storage for the list
  // and get a pointer to it
  fprintf(Outfh, "  %%.t%d =l alloc8 24\n", va_ptr);
  fprintf(Outfh, "  vastart %%.t%d\n", va_ptr);

  // Also save it in the program's pointer
  cgstorvar(va_ptr, n.ty, n.sym);
}

// End the use of the variable argument list
void cg_vaend(ASTnode *n) {
  return;
}

int cg_vaarg(ASTnode *n) {
  int t = cgalloctemp();
  char *qtype = qbetype(n.ty);

  if (va_ptr == NOTEMP)
    fatal("va_arg() with no preceding va_start()\n");

  fprintf(Outfh, "  %%.t%d =%s vaarg %%.t%d\n", t, qtype, va_ptr);
  return(t);
}

// Check that we can use the value in
// lefttemp as an unsigned value. Very
// similar to the bounds check code.
int cg_unsign(int t, Type *ty, int funcname) {
  int comparetemp = cgalloctemp();
  int zerotemp = cgalloctemp();
  int Lgood = genlabel();
  int Lfail = genlabel();
  char *qtype = qbetype(ty);

  // Get zero into a temporary
  fprintf(Outfh, "  %%.t%d =%s copy 0\n", zerotemp, qtype);

  // Compare against the value in t
  // Jump if false to the failure label
  // Otherwise jump to the good label
  comparetemp = cgcompare(A_GE, t, zerotemp, ty);
  cgjump_if_false(comparetemp, Lfail);
  cgjump(Lgood);

  // Call the failure function
  cglabel(Lfail);
  fprintf(Outfh, "  call $.fatal(l $.unsignerr, %s %%.t%d, l $L%d)\n",
          qtype, t, funcname);
  cglabel(Lgood);

  return (t);
}
