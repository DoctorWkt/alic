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
void cgstrlit(int label, string val, bool is_const) {
  char ch;

  // Put constant string literals in the rodata section
  if (is_const)
    fprintf(Outfh, "section \".rodata\"\n");

  fprintf(Outfh, "data $L%d = { ", label);

  foreach ch (val)
    fprintf(Outfh, "b %d, ", ch);

  fprintf(Outfh,  "b 0 }\n");
}

// Generate a jump to a label
void cgjump(int l) {
  fprintf(Outfh, "  jmp @L%d\n", l);
}

// Table of QBE type names used
// after the '=' sign in instructions
string qbe_typename[8] = {
  "w", "w", "w", "l", "s", "d", "", "w"
};

// Table of QBE type names used
// in store instructions
string qbe_storetypename[8] = {
  "b", "h", "w", "l", "s", "d", "", "b"
};

// Table of QBE type names used when loading.
// Second half represents unsigned types
string qbe_loadtypename[16] = {
  "sb", "sh", "sw", "l", "s", "d", "", "sb",
  "ub", "uh", "uw", "l", "s", "d", "", "ub"
};

// Table of QBE type names used when extending.
// Second half represents unsigned types
string qbe_exttypename[16] = {
  "sw", "sw", "sw", "sl", "s", "d", "", "sw",
  "uw", "uw", "uw", "ul", "s", "d", "", "uw"
};

// Return the QBE type that
// matches the given built-in type
string qbetype(Type * ty) {
  int kind = ty.kind;

  if (is_pointer(ty))
    return ("l");
  if (ty.kind > TY_BOOL)
    fatal("%s not a built-in type\n", get_typename(ty));

  return (qbe_typename[kind]);
}

// Ditto for stores
string qbe_storetype(Type * ty) {
  int kind = ty.kind;

  if (is_pointer(ty))
    return ("l");
  if (ty.kind > TY_BOOL)
    fatal("%s not a built-in type\n", get_typename(ty));
  if (ty.kind == TY_VOID)
    fatal("No QBE void type");

  return (qbe_storetypename[kind]);
}

// Ditto for loads, with signed knowledge
string qbe_loadtype(Type * ty) {
  int kind = ty.kind;

  if (is_pointer(ty))
    return ("l");
  if (ty.kind > TY_BOOL)
    fatal("%s not a built-in type\n", get_typename(ty));
  if (ty.kind == TY_VOID)
    fatal("No QBE void type");
  if (ty.is_unsigned)
    kind = kind + TY_BOOL + 1;

  return (qbe_loadtypename[kind]);
}

// Ditto for extends, with signed knowledge
string qbe_exttype(Type * ty) {
  int kind = ty.kind;

  if (ty.kind > TY_BOOL)
    fatal("%s not a built-in type\n", get_typename(ty));
  if (ty.kind == TY_VOID)
    fatal("No QBE void type");
  if (ty.is_unsigned)
    kind = kind + TY_BOOL + 1;

  return (qbe_exttypename[kind]);
}

// Given a type and the current possible offset
// of a member of this type in a struct, return
// the correct offset for the member
// requirement in bytes
int cgalign(Type * ty, int offset) {
  int alignment = 1;

  // Pointers are 8-byte aligned
  if (is_pointer(ty))
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
  fputs("data $.casterr = { b \"cast() expression out of range in %s()\\n\", b 0 }\n\n", Outfh);
  fputs("data $.rangeerr = { b \"expression out of range for type in %s()\\n\", b 0 }\n\n", Outfh);
  fputs("data $.stridxerr = { b \"string index out of range in %s()\\n\", b 0 }\n\n", Outfh);
}

// Temporary which holds the vastart argument list
int va_ptr;

// Print out the function preamble
void cg_func_preamble(Sym * func) {
  Sym *this;
  string qtype;

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
  foreach this (func.paramlist, this.next) {
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

  size= ty.size;

  // No global associative arrays yet
  if (sym.keytype != NULL)
    fatal("No global associative arrays yet, sorry\n");

  // We can't declare it when it is opaque (zero size)
  if (sym.ty.size == 0)
    fatal("Can't declare %s as size zero\n", sym.name);

  // Put constant symbols in the rodata section
  if (sym.is_const)
    fprintf(Outfh, "section \".rodata\"\n");

  // Export the variable if public.
  // Private variables are not exported
  if (sym.visibility == SV_PUBLIC)
    fprintf(Outfh, "export ");

  // If the data is 8 bytes or more,
  // align it on an 8-byte boundary
  if (size >= 8)
    align = 8;
  else {
    // Determine the next biggest (or equal)
    // power of two given the size
    while (power < size)
      power = power * 2;
    align = power;
  }

  fprintf(Outfh, "data $%s = align %d { ", sym.name, align);

  if (make_zero == true) {
    size= get_varsize(sym);
    fprintf(Outfh, "z %d", size);
  }
}

// Add a value to a global symbol
void cgglobsymval(ASTnode * value, int offset) {
  string qtype;
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
    label= add_strlit(value.strlit, value.is_const);
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
  string qtype = qbetype(ty);
  fprintf(Outfh, "  %%.t%d =%s copy %d\n", t, qtype, val);
}

// Load an integer literal value into a temporary.
// Return the number of the temporary.
int cgloadlit(inout Litval value, Type * ty) {
  string qtype;

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
int cgbinop(int t1, int t2, string op, Type * ty) {
  // Get the matching QBE type
  string qtype = qbetype(ty);

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

// List of QBE comparison operations. Add 6 for unsigned or 12 for floats
string qbecmp[18] = {
  "eq", "ne", "slt", "sgt", "sle", "sge",
  "eq", "ne", "ult", "ugt", "ule", "uge",
  "eq", "ne", "lt",  "gt",  "le",  "ge"
};

// Compare two temporaries and return the boolean result
int cgcompare(int op, int t1, int t2, Type * ty) {
  // Get the matching QBE type
  string qtype = qbetype(ty);
  string cmpstr;
  int offset;
  int t;

  // Get the QBE comparison
  offset= 0;
  if (ty.is_unsigned) offset= 6;
  if (is_flonum(ty)) offset= 12;
  cmpstr = qbecmp[op - A_EQ + offset];

  // Get a new temporary
  t = cgalloctemp();

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
  string qtype = qbetype(ty);

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
  Type *ty= sym.ty;
  char qbeprefix = (sym.visibility == SV_LOCAL) ? '%' : '$';
  string qloadtype;
  string qtype;

  // Allocate a new temporary
  int t = cgalloctemp();

  // If it's a function, just copy it
  if (sym.symtype == ST_FUNCTION) {
    fprintf(Outfh, "  %%.t%d =l copy $%s\n", t, sym.name);
    return(t);
  }

  // If it's a function pointer, copy or load it
  if (sym.ty.kind == TY_FUNCPTR) {
    if (sym.has_addr==true)
      fprintf(Outfh, "  %%.t%d =l load %c%s\n", t, qbeprefix, sym.name);
    else
      fprintf(Outfh, "  %%.t%d =l copy %c%s\n", t, qbeprefix, sym.name);
    return(t);
  }

  // Get the matching QBE type
  qloadtype = qbe_loadtype(ty);
  qtype = qbetype(ty);

  // If it's an associative array, get the pointer
  if (sym.keytype != NULL) {
    fprintf(Outfh, "  %%.t%d =l copy %c%s\n", t, qbeprefix, sym.name);
    return(t);
  }

  // If it has an address and isn't an array
  if ((sym.has_addr) && !is_array(sym))
    fprintf(Outfh, "  %%.t%d =%s load%s %c%s\n",
	    t, qtype, qloadtype, qbeprefix, sym.name);
  else
    fprintf(Outfh, "  %%.t%d =%s copy %c%s\n",
	    t, qtype, qbeprefix, sym.name);

  return (t);
}

// Given a temporary and a type, do a run-time
// check to ensure that the temporary's value
// fits into any type range
public void cgrangecheck(int t, Type *ty, int funcname) {
  int t1 = cgalloctemp();
  int t2 = cgalloctemp();
  string qtype = qbetype(ty);
  int Lgood = genlabel();
  int Lfail = genlabel();

  // Check t's value against the minimum
  fprintf(Outfh, "  %%.t%d =%s copy %ld\n", t1, qtype, ty.lower);
  t2 = cgcompare(A_GE, t, t1, ty);
  cgjump_if_false(t2, Lfail);

  // Check t's value against the maximum
  fprintf(Outfh, "  %%.t%d =%s copy %ld\n", t1, qtype, ty.upper);
  t2 = cgcompare(A_LE, t, t1, ty);
  cgjump_if_false(t2, Lfail);
  cgjump(Lgood);

  // Output the call to .fatal() if the range checks fail
  cglabel(Lfail);
  fprintf(Outfh, "  call $.fatal(l $.rangeerr, l $L%d)\n", funcname);
  cglabel(Lgood);
}

int cgstorvar(int t, Type * exprtype, Sym * sym) {
  char qbeprefix = (sym.visibility == SV_LOCAL) ? '%' : '$';
  string qtype;
  Type *ty= sym.ty;

  // If it's a function pointer, change the type
  if (sym.ty.kind == TY_FUNCPTR)
    ty= ty_voidptr;

  // Get the matching QBE type
  qtype = qbe_storetype(ty);

  if (sym.has_addr)
    fprintf(Outfh, "  store%s %%.t%d, %c%s\n", qtype, t, qbeprefix,
	    sym.name);
  else
    fprintf(Outfh, "  %c%s =%s copy %%.t%d\n",
	    qbeprefix, sym.name, qtype, t);

  return (NOTEMP);
}

// Add space for a local variable
void cgaddlocal(Type * ty, Sym * sym, int size, bool makezero, bool isarray) {
  int align = 8;
  int temp = cgalloctemp();
  int t2 = cgalloctemp();
  string name = sym.name;

  // If it's associative array, allocate room for a pointer
  // and construct the empty associative array
  if (sym.keytype != NULL) {
    fprintf(Outfh, "  %%%s =l alloc8 8\n", name);
    fprintf(Outfh, "  %%%s =l call $al_new_aarray()\n", name);
    return;
  }

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

// Call a function or function pointer with the given symbol id.
// Return the temporary with the result
int cgcall(Sym * sym, int numargs, int excepttemp, int *arglist,
	   Type ** typelist) {
  int rettemp = NOTEMP;
  int functemp;
  int i;

  // It's a function
  if (sym.symtype == ST_FUNCTION) {
    // Call the function
    if (sym.ty == ty_void)
      fprintf(Outfh, "  call $%s(", sym.name);
    else {
      // Get a new temporary for the return result
      rettemp = cgalloctemp();

      fprintf(Outfh, "  %%.t%d =%s call $%s(",
	    rettemp, qbetype(sym.ty), sym.name);
    }
  } else {
    // It's a function pointer. Get the pointer
    // value into a temporary.
    functemp= cgloadvar(sym);

    // Call the function pointer
    if (sym.ty == ty_void)
      fprintf(Outfh, "  call %%.t%d(", functemp);
    else {
      // Get a new temporary for the return result
      rettemp = cgalloctemp();

      fprintf(Outfh, "  %%.t%d =%s call %%.t%d(",
	    rettemp, qbetype(sym.ty), functemp);
    }
  }

  // If the function has an exception variable, output it
  // Use count as the id of the temporary holding its value
  if (sym.exceptvar != NULL) {
    fprintf(Outfh, "l %%.t%d", excepttemp);
    if (numargs != 0)
      fprintf(Outfh, ", ");
  }

  // Output the list of arguments
  foreach i (0 ... numargs - 1) {
    fprintf(Outfh, "%s %%.t%d", qbetype(typelist[i]), arglist[i]);

    // If the function is variadic, QBE requires a '...'
    // after the last non-variadic argument
    if ((sym.is_variadic == true) && (i == sym.count - 1))
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
  string qtype = qbetype(ty);
  string qloadtype = qbe_loadtype(ty);

  // Get a temporary for the return result
  int ret = cgalloctemp();

  fprintf(Outfh, "  %%.t%d =%s load%s %%.t%d\n", ret, qtype, qloadtype, t);
  return (ret);
}

int cgstorderef(int t1, int t2, Type * ty) {
  // Get the matching QBE type
  string qtype = qbe_storetype(ty);

  fprintf(Outfh, "  store%s %%.t%d, %%.t%d\n", qtype, t1, t2);
  return (NOTEMP);
}

// Do a bounds check on t1's value. If below zero
// or >= count, call a function that will exit()
// the program. Otherwise return t1's value.
int cgboundscheck(int t1, int counttemp, int aryname, int funcname) {
  int comparetemp = cgalloctemp();
  int zerotemp = cgalloctemp();
  int Lgood = genlabel();
  int Lfail = genlabel();

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
  string qtype = qbetype(n.ty);

  if (va_ptr == NOTEMP)
    lfatal(n.line, "va_arg() with no preceding va_start()\n");

  fprintf(Outfh, "  %%.t%d =%s vaarg %%.t%d\n", t, qtype, va_ptr);
  return(t);
}

// These are the actions we need to perform when
// converting one integer type to another integer type.
enum {
  C_E=1,			// Use a QBE instruction to extend the size
  C_M=2,			// Do a check on the type's minimum value
  C_X=4,			// Do a check on the type's maximum value
  C_ME=3,			// Combinations of the above
  C_MX=6,
  C_NOCHECKMASK=1		// AND with this to disable checks
};

// This holds a rows of the above actions.
// Convert a specific int type to other int types.
type Cvtrow = struct {
  int mask[8]  			// signed int types followed by unsigned ints
};

Cvtrow cvt[8]= {
  { { 0,    0,    0,    C_E, C_M,  C_M,  C_M,  C_ME } },	// int8
  { { C_MX, 0,    0,    C_E, C_MX, C_M,  C_M,  C_ME } },	// int16
  { { C_MX, C_MX, 0,    C_E, C_MX, C_MX, C_M,  C_ME } },	// int32
  { { C_MX, C_MX, C_MX, 0,   C_MX, C_MX, C_MX, C_M  } },	// int64
  { { C_X,  0,    0,    C_E, 0,    0,    0,    C_E  } },	// uint8
  { { C_X,  C_X,  0,    C_E, C_X,  0,    0,    C_E  } },	// uint16
  { { C_X,  C_X,  C_X,  C_E, C_X,  C_X,  0,    C_E  } },	// uint32
  { { C_X,  C_X,  C_X,  C_X, C_X,  C_X,  C_X,  0    } }		// uint64
};

// Given an expression's value in a temporary, the type of the expression,
// change the value to the given new type ty. This could mean a loss of
// precision (e.g. float to int, flt64 to flt32) or a change of range
// (e.g. int32 to int8, or int16 to uint32). Die with a fatal error if
// the value exceeds the new type's range.
public int cgcast(int exprtemp, Type *ety, Type *ty, int funcname) {
  int t1 = cgalloctemp();
  int t2 = cgalloctemp();
  int row;
  int col;
  int mask;
  int Lgood = genlabel();
  int Lfail = genlabel();
  bool didjump= false;
  int64 min;
  int64 max;
  string qetype;
  string qtype;

// fprintf(Outfh, "# Casting %s to %s\n", get_typename(ety), get_typename(ty));

  // If the two types are the same, return the temporary
  if (ety == ty)
    return(exprtemp);

  // flt64 to flt32
  if ((ety == ty_flt64) && (ty == ty_flt32)) {
    fprintf(Outfh, "  %%.t%d =s truncd %%.t%d\n", t1, exprtemp);
    return(t1);
  }

  // flt32 to flt64
  if ((ety == ty_flt32) && (ty == ty_flt64)) {
    fprintf(Outfh, "  %%.t%d =d exts %%.t%d\n", t1, exprtemp);
    return(t1);
  }

  // Get the expression's QBE type and the new QBE type
  qetype = qbetype(ety);
  qtype = qbetype(ty);

  // int to float
  if (is_integer(ety) && is_flonum(ty)) {
    qetype= qbe_exttype(ety);
    fprintf(Outfh, "  %%.t%d =%s %stof %%.t%d\n", t1, qtype, qetype, exprtemp);
    return (t1);
  }

  // At this point we are down to flt -> int and int -> int conversions.
  // For the former, we convert the float expression value to an (u)int64
  // and then do a conversion to the destination int.
  if (is_flonum(ety)) {
    // float to (u)int64 is tricky as we can't do the bounds checks with 
    // int literals. So we do them with float literals instead.
    if (ty == ty_uint64) {
      fprintf(Outfh, "  %%.t%d =%s copy %s_0.0\n", t1, qetype, qetype);
      t2 = cgcompare(A_GE, exprtemp, t1, ety);
      cgjump_if_false(t2, Lfail);
      fprintf(Outfh, "  %%.t%d =%s copy %s_18446744073709551615.0\n",
                                        t1, qetype, qetype);
      t2 = cgcompare(A_LE, exprtemp, t1, ety);
      cgjump_if_false(t2, Lfail);
    }

    if (ty == ty_int64) {
      fprintf(Outfh, "  %%.t%d =%s copy %s_-9223372036854775808.0\n",
                                        t1, qetype, qetype);
      t2 = cgcompare(A_GE, exprtemp, t1, ety);
      cgjump_if_false(t2, Lfail);
      fprintf(Outfh, "  %%.t%d =%s copy %s_9223372036854775807.0\n",
                                        t1, qetype, qetype);
      t2 = cgcompare(A_LE, exprtemp, t1, ety);
      cgjump_if_false(t2, Lfail);
    }

    // Do the float to int conversion.
    // Get a new temp so it's QBE 'l' type
    t2 = cgalloctemp();
    if (ty.is_unsigned) {
      fprintf(Outfh, "  %%.t%d =l %stoui %%.t%d\n", t2, qetype, exprtemp);
      ety= ty_uint64;
    } else {
      fprintf(Outfh, "  %%.t%d =l %stoui %%.t%d\n", t2, qetype, exprtemp);
      ety= ty_int64;
    }

    // Set the expression's new type to be integer
    qetype= "l";
    didjump= true; exprtemp= t2;

// fprintf(Outfh, "# After flt conversion, ety is %s\n", get_typename(ety));

    // If the destination is (u)int64 then jump to Lgood now
    if ((ty == ty_int64) || (ty == ty_uint64))
      cgjump(Lgood);
  }

  // At this point we have an int expression value and an int
  // destination type. Now we do some table-driven conversions.
  // Get the bitmask of conversion operations to perform
  row= ety.kind; if (ety.is_unsigned) row= row + 4;
  if (ty.is_unsigned) {
    col= ty.kind + 4;
    min= typemin[ty.kind + 4];
    max= typemax[ty.kind + 4];
  } else {
    col= ty.kind;
    min= typemin[ty.kind];
    max= typemax[ty.kind];
  }
  mask= cvt[row].mask[col];

  // If funcname is NOTEMP, don't do any max/min checks.
  // We use this when widening values to uint64 when
  // creating associative array keys.
  if (funcname == NOTEMP)
    mask= mask & C_NOCHECKMASK;

// fprintf(Outfh, "# Int to int mask is 0x%x\n", mask);

  // Do a maximum check if needed
  if ((mask & C_X) != 0) {
    fprintf(Outfh, "  %%.t%d =%s copy %ld\n", t1, qetype, max);
    t2 = cgcompare(A_LE, exprtemp, t1, ety);
    cgjump_if_false(t2, Lfail); didjump= true;

    // Put the jump in if there is no minimum check
    if ((mask & C_M) == 0) cgjump(Lgood);
  }

  // Do a minimum check if needed
  if ((mask & C_M) != 0) {
    fprintf(Outfh, "  %%.t%d =%s copy %ld\n", t1, qetype, min);
    t2 = cgcompare(A_GE, exprtemp, t1, ety);
    cgjump_if_false(t2, Lfail);
    cgjump(Lgood); didjump= true;
  }

  // Output the call to .fatal() if the above range tests failed
  if (didjump == true) {
    cglabel(Lfail);
    fprintf(Outfh, "  call $.fatal(l $.casterr, l $L%d)\n", funcname);
    cglabel(Lgood);
  }

  // Do a QBE extend operation if needed
  if ((mask & C_E) != 0) {
    qetype= qbe_exttype(ety);
    t2 = cgalloctemp();
    fprintf(Outfh, "  %%.t%d =%s ext%s %%.t%d\n", t2, qtype, qetype, exprtemp);
    exprtemp= t2;
  }

  return(exprtemp);
}

int cg_getaaval(int arytemp, int keytemp, Type *ty) {
  int t1 = cgalloctemp();
  int t2 = cgalloctemp();
  string qtype = qbetype(ty);

  // Call the associative array lookup function
  fprintf(Outfh, "  %%.t%d =l call $al_get_aavalue(l %%.t%d, l %%.t%d)\n",
	t1, arytemp, keytemp);

  // If the type is smaller than 64 bits, narrow the result
  if (ty.size < 8) {
    fprintf(Outfh, "  %%.t%d =%s copy %%.t%d\n", t2, qtype, t1);
    return(t2);
  } else
    return(t1);
}

void cg_setaaval(int arytemp, int keytemp, int valtemp, Type *ty) {
  int t= valtemp;
  string qtype;

  // Widen the value's type to be 64 bits
  if (ty.size < 8) {
    t = cgalloctemp();
    qtype = qbe_exttype(ty);
    fprintf(Outfh, "  %%.t%d =l ext%s %%.t%d\n", t, qtype, valtemp);
  }

  // Call the associative array set function
  fprintf(Outfh, "  call $al_add_aakeyval(l %%.t%d, l %%.t%d, l %%.t%d)\n",
	arytemp, keytemp, t);
}

int cg_existsaaval(int arytemp, int keytemp) {
  int t = cgalloctemp();

  // Call the associative array lookup function
  fprintf(Outfh, "  %%.t%d =w call $al_exists_aakey(l %%.t%d, l %%.t%d)\n",
	t, arytemp, keytemp);
  return(t);
}

int cg_delaaval(int arytemp, int keytemp) {
  int t = cgalloctemp();

  // Call the associative array lookup function
  fprintf(Outfh, "  call $al_del_aakey(l %%.t%d, l %%.t%d)\n",
	arytemp, keytemp);
  return(t);
}

// Return a 64-bit hash value for the string
// that keytemp points at
int cg_strhash(int keytemp) {
  int t = cgalloctemp();

  fprintf(Outfh, "  %%.t%d =l call $aa_djb2hash(l %%.t%d)\n",
	t, keytemp);
  return(t);
}

int cg_free_aarray(Sym * sym) {
  int arytemp;

  arytemp= cgloadvar(sym);
  fprintf(Outfh, "  call $al_free_aarray(l %%.t%d)\n", arytemp);
  return(NOTEMP);
}

int cg_aaiterstart(int arytemp) {
  int t = cgalloctemp();

  fprintf(Outfh, "  %%.t%d =l call $al_aa_iterstart(l %%.t%d)\n",
			t, arytemp);
  return(t);
}

int cg_aanext(int arytemp) {
  int t = cgalloctemp();

  fprintf(Outfh, "  %%.t%d =l call $al_getnext_aavalue(l %%.t%d)\n",
			t, arytemp);
  return(t);
}

// Call a function that returns an array of
// pointers and iterate over this list.
public int cg_funciterator(ASTnode * n, Breaklabel *this) {
  int listptr;
  int elemptr;
  int elemdref;
  int zerotemp;
  int t1;
  int t2;
  int Lifend;
  int Lfortop;
  ASTnode *assign;

  // Call the function and get the list pointer.
  // Copy the value to the element pointer
  fprintf(Outfh, "# Start of a function iteration\n");
  listptr= genAST(n.mid);
  elemptr= cgalloctemp();
  fprintf(Outfh, "  %%.t%d =l copy %%.t%d\n", elemptr, listptr);
  fprintf(Outfh, "# %%.t%d is listptr, %%.t%d is elemptr\n", listptr, elemptr);

  // Compare listptr against NULL and skip if it is
  zerotemp= cgalloctemp();
  t1= cgalloctemp();
  Lifend= genlabel();
  fprintf(Outfh, "# Compare listptr against NULL and skip if it is\n");
  fprintf(Outfh, "  %%.t%d =l copy 0\n", zerotemp);
  fprintf(Outfh, "  %%.t%d =w cnel %%.t%d, %%.t%d\n", t1, listptr, zerotemp);
  cgjump_if_false(t1, Lifend);

  // Top of the foreach loop: is *element NULL?
  fprintf(Outfh, "# Top of the foreach loop: is *element NULL?\n");
  Lfortop= genlabel(); cglabel(Lfortop);
  elemdref= cgalloctemp();
  fprintf(Outfh, "# %%.t%d is elemdref\n", elemdref);
  fprintf(Outfh, "  %%.t%d =l loadl %%.t%d\n", elemdref, elemptr);
  t1= cgalloctemp();
  fprintf(Outfh, "  %%.t%d =w cnel %%.t%d, %%.t%d\n", t1, elemdref, zerotemp);
  cgjump_if_false(t1, this.break_label);

  // Dereference elemdref and store in the loop variable.
  // We do this by building a suitable ASSIGN ASTnode and call gen_assign()
  fprintf(Outfh, "# Dereference elemdref and store in the loop variable\n");
  t1= cgderef(elemdref, n.left.ty);

  // Generate any code for the loop variable
  fprintf(Outfh, "# Generate any code for the loop variable\n");
  t2= genAST(n.left);

  // Assign the deref'd elemdref to the loop variable
  assign= mkastnode(A_ASSIGN, NULL, NULL, n.left);
  fprintf(Outfh, "# Assign the deref'd elemdref to the loop variable\n");
  gen_assign(t1, t2, assign);

  // Loop body
  fprintf(Outfh, "# Loop body\n");
  genAST(n.right);

  // Free elemdref
  fprintf(Outfh, "# Free elemdref\n");
  fprintf(Outfh, "  call $free(l %%.t%d)\n", elemdref);

  // Move elemptr up by sizeof(pointer)
  fprintf(Outfh, "# Move elemptr up by sizeof(pointer)\n");
  cglabel(this.continue_label);
  t1= cgalloctemp();
  fprintf(Outfh, "  %%.t%d =l copy 8\n", t1);
  fprintf(Outfh, "  %%.t%d =l add %%.t%d, %%.t%d\n", elemptr, elemptr, t1);

  // Jump to the top of the for loop
  fprintf(Outfh, "# Jump to the top of the for loop\n");
  cgjump(Lfortop);

  // End of the for statement
  fprintf(Outfh, "# End of the for statement\n");
  cglabel(this.break_label);

  // Free the list pointer
  fprintf(Outfh, "# Free the list pointer\n");
  fprintf(Outfh, "  call $free(l %%.t%d)\n", listptr);

  // End of the if statement
  fprintf(Outfh, "# End of the if statement\n");
  cglabel(Lifend);

  return(NOTEMP);
}

// Runtime check that the offset into a string is OK
void cg_stridxcheck(int idxtemp, int basetemp, int funcname) {
  int t1;
  int zerotemp = cgalloctemp();
  int lentemp = cgalloctemp();
  int Lgood = genlabel();
  int Lfail = genlabel();

  // Check that the base address isn't NULL
  fprintf(Outfh, "  %%.t%d =l copy 0\n", zerotemp);
  t1 = cgcompare(A_NE, basetemp, zerotemp, ty_int64);
  cgjump_if_false(t1, Lfail);

  // Check that the index isn't negative
  t1 = cgcompare(A_GE, idxtemp, zerotemp, ty_int64);
  cgjump_if_false(t1, Lfail);

  // Get the string's length
  fprintf(Outfh, "  %%.t%d =l call $strlen(l %%.t%d)\n", lentemp, basetemp);

  // Check that the index is below the length
  t1 = cgcompare(A_LT, idxtemp, lentemp, ty_int64);
  cgjump_if_false(t1, Lfail);
  cgjump(Lgood);

  // Output the call to .fatal() if the range checks fail
  cglabel(Lfail);
  fprintf(Outfh, "  call $.fatal(l $.stridxerr, l $L%d)\n", funcname);
  cglabel(Lgood);

  return;
}

// Iterate over all the characters in a string
public int cg_stringiterator(ASTnode * n, Breaklabel *this) {
  int t1;
  int t2;
  int listptr;
  int zerotemp;
  int Lifend;
  int Lfortop;
  ASTnode *assign;

  // Get a copy of the base of the string
  fprintf(Outfh, "# Start of a string iteration\n");
  listptr= genAST(n.mid);

  // Check if this is NULL
  zerotemp = cgalloctemp();
  t1= cgalloctemp();
  Lifend= genlabel();
  fprintf(Outfh, "# Compare listptr against NULL and skip if it is\n");
  fprintf(Outfh, "  %%.t%d =l copy 0\n", zerotemp);
  fprintf(Outfh, "  %%.t%d =w cnel %%.t%d, %%.t%d\n", t1, listptr, zerotemp);
  cgjump_if_false(t1, Lifend);

  // Top of the foreach loop: is *listptr zero?
  fprintf(Outfh, "# Top of the foreach loop: is *listptr zero?\n");
  Lfortop= genlabel(); cglabel(Lfortop);
  fprintf(Outfh, "# Dereference the listptr\n");
  t1= cgderef(listptr, n.left.ty);
  t2= cgalloctemp();
  fprintf(Outfh, "  %%.t%d =w cnew %%.t%d, %%.t%d\n", t2, t1, zerotemp);
  cgjump_if_false(t2, this.break_label);

  // Assign the deref'd listptr to the loop variable
  fprintf(Outfh, "# Assign the deref'd listptr to the loop variable\n");
  assign= mkastnode(A_ASSIGN, NULL, NULL, n.left);
  t2= genAST(n.left);
  gen_assign(t1, t2, assign);

  // Loop body
  fprintf(Outfh, "# Loop body\n");
  genAST(n.right);

  // Move listptr up by one
  cglabel(this.continue_label);
  t1= cgalloctemp();
  fprintf(Outfh, "  %%.t%d =l copy 1\n", t1);
  fprintf(Outfh, "  %%.t%d =l add %%.t%d, %%.t%d\n", listptr, listptr, t1);

  // Jump to the top of the for loop
  fprintf(Outfh, "# Jump to the top of the for loop\n");
  cgjump(Lfortop);

  // End of the for statement
  fprintf(Outfh, "# End of the for statement\n");
  cglabel(this.break_label);

  // End of the if statement
  fprintf(Outfh, "# End of the if statement\n");
  cglabel(Lifend);
  return(NOTEMP);
}

// Iterate over all the elements in an array
int cg_arrayiterator(ASTnode * n, Breaklabel *this) {
  ASTnode *assign;
  int Lfortop;
  int aryptr;
  int idx;
  int arysize;
  int t1;
  int t2;
  int t3;

  // Get the base address of the array
  fprintf(Outfh, "# Start of an array iteration\n");
  aryptr= genAST(n.mid);

  // Set the hiddex index to zero
  idx= cgalloctemp();
  fprintf(Outfh, "  %%.t%d =l copy 0\n", idx);

  // Get the array's size
  arysize= cgalloctemp();
  fprintf(Outfh, "  %%.t%d =l copy %d\n", arysize, n.mid.count);
  
  // Top of the loop: is idx < the array's size
  fprintf(Outfh, "# Top of the loop: is idx < the array's size\n");
  Lfortop= genlabel(); cglabel(Lfortop);
  t1= cgalloctemp();
  fprintf(Outfh, "  %%.t%d =w csltl %%.t%d, %%.t%d\n", t1, idx, arysize);
  cgjump_if_false(t1, this.break_label);

  // Get the element's value from the list
  fprintf(Outfh, "# Get the element's value from the list\n");
  t2= cgderef(aryptr, n.left.ty);
  assign= mkastnode(A_ASSIGN, NULL, NULL, n.left);
  t3= genAST(n.left);
  gen_assign(t2, t3, assign);

  // Loop body
  fprintf(Outfh, "# Loop body\n");
  genAST(n.right);

  // Increment idx
  fprintf(Outfh, "# Loop increment\n");
  cglabel(this.continue_label);

  t1= cgalloctemp();
  fprintf(Outfh, "  %%.t%d =l copy 1\n", t1);
  fprintf(Outfh, "  %%.t%d =l add %%.t%d, %%.t%d\n", idx, idx, t1);
  t2= cgalloctemp();
  fprintf(Outfh, "  %%.t%d =l copy %d\n", t2, n.left.ty.size);
  fprintf(Outfh, "  %%.t%d =l add %%.t%d, %%.t%d\n", aryptr, aryptr, t2);

  // Jump to the top of the for loop
  fprintf(Outfh, "# Jump to the top of the for loop\n");
  cgjump(Lfortop);

  // End of the for statement
  fprintf(Outfh, "# End of the for statement\n");
  cglabel(this.break_label); 
  return(NOTEMP);
}
