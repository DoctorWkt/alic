// IL to QBE translator for the alic compiler.
// (c) 2025 Warren Toomey, GPL3

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "alic.h"

extern FILE *Infh;
extern FILE *Outfh;

void fatal(const char *fmt, ...) {
  va_list ptr;

  va_start(ptr, fmt);
  vfprintf(stderr, fmt, ptr);
  va_end(ptr);
  exit(1);
}

// List of AST node names
static char *astname[] = { NULL,
  "ASSIGN", "WIDEN",
  "ADD", "SUBTRACT", "MULTIPLY", "DIVIDE", "NEGATE",
  "EQ", "NE", "LT", "GT", "LE", "GE", "NOT",
  "AND", "OR", "XOR", "INVERT",
  "LSHIFT", "RSHIFT",
  "NUMLIT", "IDENT", "BREAK", "GLUE", "IF", "WHILE", "FOR",
  "TYPE", "STRLIT", "LOCAL", "FUNCCALL", "RETURN", "ADDR",
  "DEREF", "ABORT", "TRY", "CONTINUE", "SCALE", "ADDOFFSET",
  "SWITCH", "CASE", "DEFAULT", "FALLTHRU", "MOD",
  "LOGAND", "LOGOR", "BEL", "BOUNDS", "TERNARY",
  "VASTART", "VAARG", "VAEND", "CAST",
  "AARRAY", "EXISTS", "UNDEF", "AAFREE",
  "AAITERSTART", "AANEXT", "FUNCITER", "STRINGITER",
  "ARRAYITER",
  "FUNCPREAMBLE", "FUNCPOSTAMBLE",
  "LOADLIT", "LOADBOOL", "LOADVAR",
  "STORVAR", "STORDEREF", "STRLITVAL",
  "GENLABEL", "JUMP", "JUMPIFFALSE",
  "GLOBSYM", "GLOBSYMVAL", "GLOBSYMEND",
  "STORELEM", "MOVE", "RANGE", "A_COPYSTRUCT",
  "STRHASH", "INITTEMP", "INCTEMP",
  "AAGETVAL", "AASETVAL", "AAEXISTS", "AADELVAL",
  "MISC", "FITERNEXT", "STRINDEX", "SITERNEXT"
};

char *get_astname(int op) {
  return (astname[op]);
}

char Buf[TEXTLEN];		// Used to read strings from the IL file

// Read at most count-1 characters from the
// f FILE and store them in the s buffer.
// Terminate the s buffer with a NUL.
// Return NULL if unable to read or an EOF.
// Else, return the original s pointer pointer.
char *fgetstr(char *s, size_t count, FILE * f) {
  size_t i = count;
  size_t err;
  char ch;
  char *ret = s;

  while (i-- != 0) {
    err = fread(&ch, 1, 1, f);
    if (err != 1) {
      if (s == ret)
	return (NULL);
      break;
    }
    *s++ = ch;
    if (ch == 0)
      break;
  }
  *s = 0;
  return (ferror(f) ? (char *) NULL : ret);
}

// Is the IL type an integer?
bool is_integer(int ty) {
  if (ty <= TY_INT64) return (true);
  if (ty > TY_BOOL) return (true);
  return (false);
}

// Is the IL type an float?
bool is_flonum(int ty) {
  if (ty == TY_FLT32 || ty == TY_FLT64) return (true);
  return (false);
}

// Is the IL type unsigned?
bool is_unsigned(int ty) {
  if (ty > TY_BOOL) return (true);
  return (false);
}

static int nexttemp = 1;	// Incrementing temporary number

// Allocate a QBE temporary
int ilalloctemp(void) {
  return (++nexttemp);
}

// Generate and return a new QBE label number
static int labelid = 1;
int genlabel(void) {
  labelid++;
  return (labelid);
}

// Output an QBE label
void qbelabel(int l) {
  fprintf(Outfh, "@QL%d\n", l);
}

// Table of QBE type names used
// after the '=' sign in instructions.
// Second half represents unsigned types
static char *qbe_typename[] = {
  "w", "w", "w", "l", "s", "d", "", "w",
  "w", "w", "w", "l", "s", "d", "", "w"
};

// Table of QBE type names used
// in store instructions
static char *qbe_storetypename[] = {
  "b", "h", "w", "l", "s", "d", "", "b",
  "b", "h", "w", "l", "s", "d", "", "b"
};

// Table of QBE type names used when loading.
// Second half represents unsigned types
static char *qbe_loadtypename[] = {
  "sb", "sh", "sw", "l", "s", "d", "", "sb",
  "ub", "uh", "uw", "l", "s", "d", "", "ub"
};

// Table of QBE type names used when extending.
// Second half represents unsigned types
static char *qbe_exttypename[] = {
  "sw", "sw", "sw", "sl", "s", "d", "", "sw",
  "uw", "uw", "uw", "ul", "s", "d", "", "uw"
};

// Return the QBE type that
// matches the given built-in type
static char *qbetype(int dtype) {
  return (qbe_typename[dtype]);
}


// Ditto for stores
static char *qbe_storetype(int dtype) {
  if (dtype == TY_VOID)
    fatal("No QBE void type\n");
  return (qbe_storetypename[dtype]);
}

// Ditto for loads, with signed knowledge
static char *qbe_loadtype(int dtype) {
  if (dtype == TY_VOID)
    fatal("No QBE void type\n");
  return (qbe_loadtypename[dtype]);
}

// Ditto for extends, with signed knowledge
static char *qbe_exttype(int dtype) {
  if (dtype == TY_VOID)
    fatal("No QBE void type\n");
  return (qbe_exttypename[dtype]);
}

// Perform a binary operation on two temporaries
void ilbinop(ILnode * n, char *op) {
  // Get the matching QBE type
  char *qtype = qbetype(n->dtype);

  fprintf(Outfh, "  %%.t%d =%s %s %%.t%d, %%.t%d\n",
	  n->dst, qtype, op, n->dst, n->src);
}

void ilnegate(ILnode * n) {
  // Get the matching QBE type
  char *qtype = qbetype(n->dtype);

  fprintf(Outfh, "  %%.t%d =%s sub 0, %%.t%d\n", n->dst, qtype, n->src);
}

void ilnot(ILnode * n) {
  // Get the matching QBE type
  char *qtype = qbetype(n->dtype);

  fprintf(Outfh, "  %%.t%d =%s ceq%s %%.t%d, 0\n", n->dst, qtype, qtype,
	  n->src);
}

void ilinvert(ILnode * n) {
  fprintf(Outfh, "  %%.t%d =%s xor %%.t%d, -1\n", n->dst,
	  qbetype(n->dtype), n->dst);
};

// Print out the file preamble
void ilfile_preamble(void) {
  // Output a copy of the function that emits
  // an error message and exit()s
#ifdef CPU_aarch64
  fputs("type :va_list.1 = align 8 { 32 }\n", Outfh);
#endif
  fputs("function $.fatal(l %.t1, ...) {\n", Outfh);
  fputs("@L1\n", Outfh);
#ifdef CPU_x86_64
  fputs("  %.t2 =l alloc8 24\n", Outfh);
  fputs("  vastart %.t2\n", Outfh);
  fputs("  %.t3 =l loadl $stderr\n", Outfh);
  fputs("  call $vfprintf(l %.t3, l %.t1, l %.t2)\n", Outfh);
#endif
#ifdef CPU_riscv64
  fputs("  %.t2 =l alloc8 32\n", Outfh);
  fputs("  vastart %.t2\n", Outfh);
  fputs("  %.t6 =l loadl %.t2\n", Outfh);
  fputs("  %.t3 =l loadl $stderr\n", Outfh);
  fputs("  call $vfprintf(l %.t3, l %.t1, l %.t6)\n", Outfh);
#endif
#ifdef CPU_aarch64
  fputs("  %.t3 =l alloc8 32\n", Outfh);
  fputs("  vastart %.t3\n", Outfh);
  fputs("  %.t4 =l loadl $stderr\n", Outfh);
  fputs("  %.t6 =w call $vfprintf(l %.t4, l %.t1, :va_list.1 %.t3)\n", Outfh);
#endif
  fputs("  call $exit(w 1)\n", Outfh);
  fputs("  ret \n", Outfh);
  fputs("}\n\n", Outfh);

  fputs
    ("data $.bounderr = { b \"%s[%d] out of bounds in %s()\\n\", b 0 }\n\n",
     Outfh);
  fputs
    ("data $.casterr = { b \"cast() expression out of range in %s()\\n\", b 0 }\n\n",
     Outfh);
  fputs
    ("data $.rangeerr = { b \"expression out of range for type in %s()\\n\", b 0 }\n\n",
     Outfh);
  fputs
    ("data $.stridxerr = { b \"string index out of range in %s()\\n\", b 0 }\n\n",
     Outfh);
}

// Temporary which holds the vastart argument list
static int va_ptr;

// Output a function's preamble
void ilfunc_preamble(ILnode * n) {
  // Get the function's return type
  char *qtype = qbetype(n->dtype);
  int ty;
  int i;

  // No va_ptr as yet
  va_ptr = NOTEMP;

  // Get the function's name
  fgetstr(Buf, TEXTLEN, Infh);

  if (n->flags & IL_PUBLIC)
    fprintf(Outfh, "export ");
  fprintf(Outfh, "function %s $%s(", qtype, Buf);

  // If we have an exception variable
  if (n->flags & IL_EXCEPTVAR) {
    // Get its type and name
    fread(&ty, sizeof(int), 1, Infh);
    qtype = qbetype(ty);
    fgetstr(Buf, TEXTLEN, Infh);
    fprintf(Outfh, "%s %%%s", qtype, Buf);
    if (n->count != 0)
      fprintf(Outfh, ", ");
  }

  // Output the list of parameters
  for (i = 0; i < n->count; i++) {
    // Get its type and name
    fread(&ty, sizeof(int), 1, Infh);
    qtype = qbetype(ty);
    fgetstr(Buf, TEXTLEN, Infh);
    fprintf(Outfh, "%s %%%s", qtype, Buf);
    if (i < n->count - 1)
      fprintf(Outfh, ", ");
  }


  // Print ... if the function is variadic
  if (n->flags & IL_VARIDIAC)
    fprintf(Outfh, ", ...");

  fprintf(Outfh, ") {\n");
  fprintf(Outfh, "@START\n");
}

// Print out the function postamble
void ilfunc_postamble(ILnode * n) {

  fprintf(Outfh, "@END\n");

  // Return a value if the function's type isn't void
  if (n->dtype != TY_VOID)
    fprintf(Outfh, "  ret %%.ret\n");
  else
    fprintf(Outfh, "  ret\n");
  fprintf(Outfh, "}\n\n");
}

// Output a local variable
void il_localvar(ILnode * n) {
  int size;
  int align = PTR_SIZE;
  int temp = ilalloctemp();
  int t2 = ilalloctemp();

  // Get the variables's name
  fgetstr(Buf, TEXTLEN, Infh);

  // If it's associative array, allocate room for a pointer
  // and construct the empty associative array
  if ((n->flags & IL_ISAARRAY) != 0) {
    fprintf(Outfh, "  %%%s =l alloc8 8\n", Buf);
    fprintf(Outfh, "  %%%s =l call $al_new_aarray()\n", Buf);
    return;
  }

  // Get a suitable alignment and allocate stack space
  size = n->count;
  if (size < 8) align = 4;

  fprintf(Outfh, "  %%%s =l alloc%d %d\n", Buf, align, size);

  // No need to zero the space
  if ((n->flags & IL_ZERO) == 0) return;

  // Yes, zero the space
  switch (size) {
  case 1:
    fprintf(Outfh, "  %%.i%d =w copy 0\n", temp);
    fprintf(Outfh, "  storeb %%.i%d, %%%s\n", temp, Buf);
    break;
  case 2:
    fprintf(Outfh, "  %%.i%d =w copy 0\n", temp);
    fprintf(Outfh, "  storeh %%.i%d, %%%s\n", temp, Buf);
    break;
  case 4:
    fprintf(Outfh, "  %%.i%d =w copy 0\n", temp);
    fprintf(Outfh, "  storew %%.i%d, %%%s\n", temp, Buf);
    break;
  case 8:
    fprintf(Outfh, "  %%.i%d =l copy 0\n", temp);
    fprintf(Outfh, "  storel %%.i%d, %%%s\n", temp, Buf);
    break;
  default:
    fprintf(Outfh, "  %%.i%d =l copy 0\n", temp);
    fprintf(Outfh, "  %%.i%d =l copy %d\n", t2, size);
    fprintf(Outfh, "  call $memset(l %%%s, l %%.i%d, l %%.i%d)\n",
	    Buf, temp, t2);
  }
}

void ilstorvar(ILnode * n) {
  char qbeprefix = ((n->flags & IL_GLOBAL) == 0) ? '%' : '$';
  char *qtype = qbe_storetype(n->dtype);

  // Get the variables's name
  fgetstr(Buf, TEXTLEN, Infh);

  if ((n->flags & IL_HASADDR) != 0)
    fprintf(Outfh, "  store%s %%.t%d, %c%s\n", qtype, n->src, qbeprefix, Buf);
  else
    fprintf(Outfh, "  %c%s =%s copy %%.t%d\n", qbeprefix, Buf, qtype, n->src);
}

void illoadvar(ILnode * n) {
  char qbeprefix = ((n->flags & IL_GLOBAL) == 0) ? '%' : '$';
  char *qtype = qbetype(n->dtype);
  char *qloadtype = qbe_loadtype(n->dtype);

  // Get the variables's name
  fgetstr(Buf, TEXTLEN, Infh);

  // If it's a function, just copy it
  if ((n->flags & IL_ISFUNCTION) != 0) {
    fprintf(Outfh, "# is a function\n");
    fprintf(Outfh, "  %%.t%d =l copy %c%s\n", n->dst, qbeprefix, Buf);
    return;
  }

  // If it's a function pointer, copy or load it
  if ((n->flags & IL_ISFUNCPTR) != 0) {
    if ((n->flags & IL_HASADDR) != 0)
      fprintf(Outfh, "  %%.t%d =l load %c%s\n", n->dst, qbeprefix, Buf);
    else
      fprintf(Outfh, "  %%.t%d =l copy %c%s\n", n->dst, qbeprefix, Buf);
    return;
  }

  // If it has an address and isn't an array
  if (((n->flags & IL_HASADDR) != 0) && ((n->flags & IL_ISARRAY) == 0))
    fprintf(Outfh, "  %%.t%d =%s load%s %c%s\n",
	    n->dst, qtype, qloadtype, qbeprefix, Buf);
  else
    fprintf(Outfh, "  %%.t%d =%s copy %c%s\n", n->dst, qtype, qbeprefix, Buf);
}

void illoadlit(ILnode * n) {
  Litval value;
  char *qtype = qbetype(n->dtype);

  // Get the actual literal value
  fread(&value, sizeof(Litval), 1, Infh);

  switch (n->dtype) {
  case TY_FLT32:
  case TY_FLT64:
    fprintf(Outfh, "  %%.t%d =%s copy %s_%f\n", n->dst, qtype, qtype,
	    value.dblval);
    break;
  default:
    fprintf(Outfh, "  %%.t%d =%s copy %ld\n", n->dst, qtype, value.intval);
  }
}

void illoadbool(ILnode * n) {
  char *qtype = qbetype(n->dtype);
  fprintf(Outfh, "  %%.t%d =%s copy %d\n", n->dst, qtype, n->count);
}

void ilfunccall(ILnode * n) {
  char qbeprefix = ((n->flags & IL_GLOBAL) == 0) ? '%' : '$';
  int i;
  int ty;
  int temp;
  int t2;

  // Get the function's or funcptr's name
  fgetstr(Buf, TEXTLEN, Infh);

  // It's a function pointer
  if ((n->flags & IL_ISFUNCPTR) != 0) {
    t2 = ilalloctemp();

    // Either copy or load the value depending on
    // if it has an address or not
    if ((n->flags & IL_HASADDR) != 0) {
      fprintf(Outfh, "  %%.i%d =l load %c%s\n", t2, qbeprefix, Buf);
    } else {
      fprintf(Outfh, "  %%.i%d =l copy %c%s\n", t2, qbeprefix, Buf);
    }

    // Call through the function pointer
    if (n->dtype == TY_VOID)
      fprintf(Outfh, "  call %%.i%d(", t2);
    else
      fprintf(Outfh, "  %%.t%d =%s call %%.i%d(", n->dst,
					qbetype(n->dtype), t2);
  } else {
    // Call the function
    if (n->dtype == TY_VOID)
      fprintf(Outfh, "  call $%s(", Buf);
    else
      fprintf(Outfh, "  %%.t%d =%s call $%s(", n->dst, qbetype(n->dtype), Buf);
  }

  // If the function has an exception variable, output it
  if (n->src != NOTEMP) {
    fprintf(Outfh, "l %%.t%d", n->src);
    if (n->count != 0)
      fprintf(Outfh, ", ");
  }

  // Output the list of arguments
  for (i = 0; i < n->count; i++) {
    fread(&ty, sizeof(int), 1, Infh);
    fread(&temp, sizeof(int), 1, Infh);
    fprintf(Outfh, "%s %%.t%d", qbetype(ty), temp);

    // If the function is variadic, QBE requires a '...'
    // after the last non-variadic argument
    if (i == n->label)
      fprintf(Outfh, ", ... ");

    // Output any separating comma
    if (i < n->count - 1)
      fprintf(Outfh, ", ");
  }

  fprintf(Outfh, ")\n");
}

void illoadstr(ILnode * n) {
  fprintf(Outfh, "  %%.t%d =l copy $L%d\n", n->dst, n->label);
}

void ilstrlit(ILnode * n) {
  char *cptr;

  // Get the literal value
  fgetstr(Buf, TEXTLEN, Infh);

  // Put constant string literals in the rodata section
  if ((n->flags & IL_ISCONST) != 0)
    fprintf(Outfh, "section \".rodata\"\n");

  fprintf(Outfh, "data $L%d = { ", n->label);

  for (cptr = Buf; *cptr; cptr++) {
    fprintf(Outfh, "b %d, ", *cptr);
  }

  fprintf(Outfh, "b 0 }\n");
}

// Given a temporary and a type, do a run-time
// check to ensure that the temporary's value
// fits into any type range
void ilrangecheck(ILnode * n) {
  int t1 = ilalloctemp();
  char *qtype = qbetype(n->stype);
  int Lgood = genlabel();
  int Lfail = genlabel();
  int label2 = genlabel();
  int t = n->src;
  int funcname = n->count;
  int lower = n->label;
  int upper = n->flags;

  // Check t's value against the minimum
  fprintf(Outfh, "  %%.i%d =w csge%s %%.t%d, %d\n", t1, qtype, t, lower);
  fprintf(Outfh, "  jnz %%.i%d, @QL%d, @QL%d\n", t1, label2, Lfail);
  qbelabel(label2);

  // Check t's value against the maximum
  fprintf(Outfh, "  %%.i%d =w csle%s %%.t%d, %d\n", t1, qtype, t, upper);
  fprintf(Outfh, "  jnz %%.i%d, @QL%d, @QL%d\n", t1, Lgood, Lfail);

  // Output the call to .fatal() if the range checks fail
  qbelabel(Lfail);
  fprintf(Outfh, "  call $.fatal(l $.rangeerr, l $L%d)\n", funcname);
  qbelabel(Lgood);
}

// These are the actions we need to perform when
// converting one integer type to another integer type.
enum {
  C_E = 1,			// Use a QBE instruction to extend the size
  C_M = 2,			// Do a check on the type's minimum value
  C_X = 4,			// Do a check on the type's maximum value
  C_ME = 3,			// Combinations of the above
  C_MX = 6,
  C_NOCHECKMASK = 1		// AND with this to disable checks
};

// This holds a rows of the above actions.
// Convert a specific int type to other int types.
typedef struct Cvtrow Cvtrow;
struct Cvtrow {
  int mask[8];			// signed int types followed by unsigned ints
};

Cvtrow cvt[8] = {
  {0, 0, 0, C_E, C_M, C_M, C_M, C_ME},	// int8
  {C_MX, 0, 0, C_E, C_MX, C_M, C_M, C_ME},	// int16
  {C_MX, C_MX, 0, C_E, C_MX, C_MX, C_M, C_ME},	// int32
  {C_MX, C_MX, C_MX, 0, C_MX, C_MX, C_MX, C_M},	// int64
  {C_X, 0, 0, C_E, 0, 0, 0, C_E},	// uint8
  {C_X, C_X, 0, C_E, C_X, 0, 0, C_E},	// uint16
  {C_X, C_X, C_X, C_E, C_X, C_X, 0, C_E},	// uint32
  {C_X, C_X, C_X, C_X, C_X, C_X, C_X, 0}	// uint64
};

// A list of minimums per type, signed followed by unsigned
Intsize typemin[8] = {
#ifdef CPU_pdp11
  SCHAR_MIN, SHRT_MIN, INT_MIN, 0, 0, 0, 0, 0
#else
  INT8_MIN, INT16_MIN, INT32_MIN, INT64_MIN, 0, 0, 0, 0
#endif
};

// A list of maximumx per type, signed followed by unsigned.
// We don't use the uint64 type value, so it is zero
Intsize typemax[8] = {
#ifdef CPU_pdp11
  SCHAR_MAX,  SHRT_MAX,  INT_MAX,  0,
  UCHAR_MAX, USHRT_MAX, UINT_MAX, 0
#else
  INT8_MAX,  INT16_MAX,  INT32_MAX,  INT64_MAX,
  UINT8_MAX, UINT16_MAX, UINT32_MAX, 0
#endif
};

// Cast src to dst using their respective type.
void ilcast(ILnode * n) {
  int srcidx;
  int dstidx;
  int mask;
  int Lgood = genlabel();
  int Lfail = genlabel();
  int label2 = genlabel();
  char *dtype = qbetype(n->dtype);
  char *qetype;
  int t1 = ilalloctemp();
  int t2;
  bool didjump = false;
  char *schar;
  char tchar = 't';
#ifdef CPU_pdp11
  int32_t min, max;
#else
  int64_t min, max;
#endif

  fprintf(Outfh, "# Casting %d to %d\n", n->stype, n->dtype);

  // If the two types are the same, copy src to dst
  if (n->dtype == n->stype) {
    fprintf(Outfh, "  %%.t%d =%s copy %%.t%d\n", n->dst, dtype, n->src);
    return;
  }

  // Treat a bool as an int8
  if (n->stype == TY_BOOL) n->stype = TY_INT8;

  // flt64 to flt32
  if ((n->stype == TY_FLT64) && (n->dtype == TY_FLT32)) {
    fprintf(Outfh, "  %%.t%d =s truncd %%.t%d\n", n->dst, n->src);
    return;
  }

  // flt32 to flt64
  if ((n->stype == TY_FLT32) && (n->dtype == TY_FLT64)) {
    fprintf(Outfh, "  %%.t%d =d exts %%.t%d\n", n->dst, n->src);
    return;
  }

  // int to float
  if (is_integer(n->stype) && is_flonum(n->dtype)) {
    qetype = qbe_exttype(n->stype);
    fprintf(Outfh, "  %%.t%d =%s %stof %%.t%d\n",
	    n->dst, dtype, qetype, n->src);
    return;
  }

  // At this point we are down to flt -> int and int -> int conversions.
  // For the former, we convert the float expression value to an (u)int64
  // and then do a conversion to the destination int.
  if (is_flonum(n->stype)) {
    // float to (u)int64 is tricky as we can't do the bounds checks with 
    // int literals. So we do them with float literals instead.
    qetype = qbe_exttype(n->stype);
    if (n->dtype == TY_INT64 + IL_UNSIGNED) {
      fprintf(Outfh, "  %%.i%d =w cge%s %%.t%d, %s_0.0\n",
	      t1, qetype, n->src, qetype);
      fprintf(Outfh, "  jnz %%.i%d, @QL%d, @QL%d\n", t1, label2, Lfail);
      qbelabel(label2);

      label2 = genlabel();
      fprintf(Outfh, "  %%.i%d =w cle%s %%.t%d, %s_18446744073709551615.0\n",
	      t1, qetype, n->src, qetype);
      fprintf(Outfh, "  jnz %%.i%d, @QL%d, @QL%d\n", t1, label2, Lfail);
      qbelabel(label2);
    }

    if (n->dtype == TY_INT64) {
      fprintf(Outfh, "  %%.i%d =w cge%s %%.t%d, %s_-9223372036854775808.0\n",
	      t1, qetype, n->src, qetype);
      fprintf(Outfh, "  jnz %%.i%d, @QL%d, @QL%d\n", t1, label2, Lfail);
      qbelabel(label2);

      label2 = genlabel();
      fprintf(Outfh, "  %%.i%d =w cle%s %%.t%d, %s_9223372036854775807.0\n",
	      t1, qetype, n->src, qetype);
      fprintf(Outfh, "  jnz %%.i%d, @QL%d, @QL%d\n", t1, label2, Lfail);
      qbelabel(label2);
    }

    // Do the float to int conversion.
    // Get a new temp so it's QBE 'l' type
    t2 = ilalloctemp();
    if (is_unsigned(n->dtype)) {
      fprintf(Outfh, "  %%.i%d =l %stoui %%.t%d\n", t2, qetype, n->src);
      n->stype = TY_INT64 + IL_UNSIGNED;
    } else {
      fprintf(Outfh, "  %%.i%d =l %stosi %%.t%d\n", t2, qetype, n->src);
      n->stype = TY_INT64;
    }

    // Set the expression's new type to be integer
    // and mark that it's now an "i" temp not a "t" temp
    qetype = "l";
    didjump = true;
    n->src = t2;
    tchar = 'i';

    fprintf(Outfh, "# After flt conversion, n->stype is %d\n", n->stype);

    // If the destination is (u)int64 then jump to Lgood now
    if ((n->dtype == TY_INT64) || (n->dtype == TY_INT64 + IL_UNSIGNED))
      fprintf(Outfh, "  jmp @QL%d\n", Lgood);
  }

  // At this point we have an int expression value and an int
  // destination type. Now we do some table-driven conversions.
  // Get the indexes for the cvt[] array
  srcidx = n->stype;
  if (is_unsigned(srcidx))
    srcidx = srcidx - 4;
  dstidx = n->dtype;
  if (is_unsigned(dstidx))
    dstidx = dstidx - 4;


  // Get the bitmask of conversion operations to perform
  // and the range of the destination type
  mask = cvt[srcidx].mask[dstidx];
  min = typemin[dstidx];
  max = typemax[dstidx];

#if 0
  // If funcname is NOTEMP, don't do any max/min checks.
  // We use this when widening values to uint64 when
  // creating associative array keys.
  if (funcname == NOTEMP)
    mask = mask & C_NOCHECKMASK;

// fprintf(Outfh, "# Int to int mask is 0x%x\n", mask);
#endif

  // Get an "u" if the dest type is unsigned
  schar = (is_unsigned(n->stype)) ? "u" : "s";
  qetype = qbetype(n->stype);

  // Do a maximum check if needed
  if ((mask & C_X) != 0) {

    fprintf(Outfh, "  %%.i%d =w c%sle%s %%.%c%d, %ld\n",
	    t1, schar, qetype, tchar, n->src, max);
    fprintf(Outfh, "  jnz %%.i%d, @QL%d, @QL%d\n", t1, label2, Lfail);
    qbelabel(label2);
    didjump = true;

    // Put a good jump in if there is no minimum check
    if ((mask & C_M) == 0)
      fprintf(Outfh, "  jmp @QL%d\n", Lgood);
  }

  // Do a minimum check if needed
  if ((mask & C_M) != 0) {
    fprintf(Outfh, "  %%.i%d =w c%sge%s %%.%c%d, %ld\n",
	    t1, schar, qetype, tchar, n->src, min);
    fprintf(Outfh, "  jnz %%.i%d, @QL%d, @QL%d\n", t1, Lgood, Lfail);
    didjump = true;
  }

  // Output the call to .fatal() if the above range tests failed
  if (didjump == true) {
    qbelabel(Lfail);
    fprintf(Outfh, "  call $.fatal(l $.casterr, l $L%d)\n", n->count);
    qbelabel(Lgood);
  }

  // Do a QBE extend operation if needed
  if ((mask & C_E) != 0) {
    qetype = qbe_exttype(n->stype);
    fprintf(Outfh, "  %%.t%d =%s ext%s %%.%c%d\n",
	    n->dst, dtype, qetype, tchar, n->src);
  } else {
    fprintf(Outfh, "  %%.t%d =%s copy %%.%c%d\n", n->dst, dtype, tchar,
	    n->src);
  }
}

// List of QBE comparison operations. Add 6 for unsigned or 12 for floats
static char *qbecmp[] = {
  "eq", "ne", "slt", "sgt", "sle", "sge",
  "eq", "ne", "ult", "ugt", "ule", "uge",
  "eq", "ne", "lt", "gt", "le", "ge"
};

// Compare two temporaries
void ilcompare(ILnode * n) {
  // Get the matching QBE type
  char *qtype = qbetype(n->dtype);
  char *cmpstr;

  // Get the QBE comparison
  int offset = 0;
  if (is_unsigned(n->dtype))
    offset = 6;
  if (is_flonum(n->dtype))
    offset = 12;
  cmpstr = qbecmp[n->op - A_EQ + offset];

  fprintf(Outfh, "  %%.t%d =%s c%s%s %%.t%d, %%.t%d\n",
	  n->dst, qtype, cmpstr, qtype, n->dst, n->src);
}

// Output a jump to a label
void iljump(ILnode * n) {
  fprintf(Outfh, "  jmp @L%d\n", n->label);
}

// Output a label
void illabel(ILnode * n) {
  fprintf(Outfh, "@L%d\n", n->label);
}

// Jump to the label if the value in src is zero
void iljump_if_false(ILnode * n) {
  // Get a label for the next instruction
  int label2 = genlabel();

  fprintf(Outfh, "  jnz %%.t%d, @QL%d, @L%d\n", n->src, label2, n->label);
  qbelabel(label2);
}

void ilreturn(ILnode * n) {
  // Only return a value if the function is not void
  if (n->dtype != TY_VOID)
    fprintf(Outfh, "  %%.ret =%s copy %%.t%d\n", qbetype(n->dtype), n->src);

  fprintf(Outfh, "  jmp @END\n");

  // QBE needs a label after a jump
  qbelabel(genlabel());
}

// Dereference a pointer to get the value
// it points at into a new temporary
void ilderef(ILnode * n) {

  // Get the matching QBE type and load type
  char *qtype = qbetype(n->dtype);
  char *qloadtype = qbe_loadtype(n->dtype);

  fprintf(Outfh, "  %%.t%d =%s load%s %%.t%d\n",
	  n->dst, qtype, qloadtype, n->src);
}

void ilstorderef(ILnode * n) {
  // Get the matching QBE type
  char *qtype = qbe_storetype(n->dtype);

  fprintf(Outfh, "  store%s %%.t%d, %%.t%d\n", qtype, n->src, n->dst);
}

// Output code to load the address of an identifier
void iladdress(ILnode * n) {
  char qbeprefix = ((n->flags & IL_GLOBAL) == 0) ? '%' : '$';

  // Get the variables's name
  fgetstr(Buf, TEXTLEN, Infh);

  fprintf(Outfh, "  %%.t%d =l copy %c%s\n", n->dst, qbeprefix, Buf);
}

// Used when outputting storage
// for global structs
static int globoffset;

// Start a global symbol
void ilglobsym(ILnode * n) {
  int align;
  int power = 1;
  int size;

  globoffset = 0;
  size = n->count;

  // Get the symbol's name
  fgetstr(Buf, TEXTLEN, Infh);

  // Put constant symbols in the rodata section
  if ((n->flags & IL_ISCONST) != 0)
    fprintf(Outfh, "section \".rodata\"\n");

  // Export the variable if public.
  // Private variables are not exported
  if (n->flags & IL_PUBLIC)
    fprintf(Outfh, "export ");

  // If the data is PTR_SIZE bytes or more,
  // align it on an PTR_SIZE-byte boundary
  if (size >= PTR_SIZE)
    align = PTR_SIZE;
  else {
    // Determine the next biggest (or equal)
    // power of two given the size
    while (power < size)
      power = power * 2;
    align = power;
  }

  fprintf(Outfh, "data $%s = align %d { ", Buf, align);

  // No need to zero the space
  if ((n->flags & IL_ZERO) != 0)
    fprintf(Outfh, "z %d", size);
}

// Add a value to a global symbol
void ilglobsymval(ILnode * n) {
  Litval value;
  char *qtype = qbe_storetype(n->dtype);
  int offset = n->src;
  int size = n->count;
  bool make_zero = (n->flags & IL_ZERO) != 0;

  // If the offset is bigger than the current offset,
  // output some zero padding
  if (offset > globoffset) {
    fprintf(Outfh, "z %d, ", offset - globoffset);
    globoffset = offset;
  }

  // Update the globoffset to match the
  // amount of data we will output
  globoffset = globoffset + size;

  // No initial value
  if (make_zero) {
    fprintf(Outfh, "z %d, ", size);
    return;
  }

  // We have a string value
  if (n->label != NOLABEL) {
    fprintf(Outfh, "%s $L%d, ", qtype, n->label);
    return;
  }

  // We have a numeric literal value. Get it
  fread(&value, sizeof(Litval), 1, Infh);

  if (is_flonum(n->dtype))
    fprintf(Outfh, "%s s_%f, ", qtype, value.dblval);
  else
    fprintf(Outfh, "%s %ld, ", qtype, value.intval);
}

// End a global symbol
void ilglobsymend(ILnode * n) {
  fprintf(Outfh, " }\n");
}

// Abort from the function
void ilabort(ILnode * n) {

  // QBE needs a label after a jump
  fprintf(Outfh, "  jmp @END\n");
  qbelabel(genlabel());
}

void ilstore_elem(ILnode * n) {
  // Get the matching QBE type
  char *qtype = qbe_storetype(n->dtype);

  int temp = ilalloctemp();

  // Add the base and the offset
  fprintf(Outfh, "  %%.i%d =l add %%.t%d, %d\n", temp, n->src, n->count);

  // Store the expression value at that address
  fprintf(Outfh, "  store%s %%.t%d, %%.i%d\n", qtype, n->dst, temp);
}

// Do a bounds check on the src value. If below zero or
// >= count, call a function that will exit() the program.
void ilboundscheck(ILnode * n) {
  int comparetemp = ilalloctemp();
  int Lgood = genlabel();
  int Lfail = genlabel();
  int label2 = genlabel();

  // Compare the bound against the index value
  // Jump if false to the failure label
  fprintf(Outfh, "  %%.i%d =w csltl %%.t%d, %%.t%d\n", comparetemp,
	  n->dst, n->src);
  fprintf(Outfh, "  jnz %%.i%d, @QL%d, @QL%d\n", comparetemp, label2, Lfail);
  fprintf(Outfh, "@QL%d\n", label2);

  // Compare zero against the index value
  // Jump if false to the failure label
  // Otherwise jump to the good label
  fprintf(Outfh, "  %%.i%d =w csgel %%.t%d, 0\n", comparetemp, n->dst);
  fprintf(Outfh, "  jnz %%.i%d, @QL%d, @QL%d\n", comparetemp, Lgood, Lfail);

  // Call the failure function
  fprintf(Outfh, "@QL%d\n", Lfail);
  fprintf(Outfh, "  call $.fatal(l $.bounderr, l $L%d, l %%.t%d, l $L%d)\n",
	  n->label, n->dst, n->flags);
  fprintf(Outfh, "@QL%d\n", Lgood);
}

void ilmove(ILnode * n) {
  // Get the matching QBE type
  char *qtype = qbetype(n->stype);

  fprintf(Outfh, "  %%.t%d =%s copy %%.t%d\n", n->dst, qtype, n->src);
}

// Allocate space for the variable argument list
void ilvastart(ILnode * n) {
#ifdef CPU_riscv64
  int temp;
#endif
  char *qtype = qbe_storetype(n->dtype);

  // Get the variables's name
  fgetstr(Buf, TEXTLEN, Infh);

  // It's already been done
  if (va_ptr != NOTEMP)
    return;

  va_ptr = ilalloctemp();

  // Allocate the storage for the list
  // and get a pointer to it
  fprintf(Outfh, "  %%.i%d =l alloc8 24\n", va_ptr);
  fprintf(Outfh, "  vastart %%.i%d\n", va_ptr);

  // Also save it in the program's pointer
#ifdef CPU_riscv64
  temp = ilalloctemp();
  fprintf(Outfh, "  %%.i%d =l loadl %%.i%d\n", temp, va_ptr);
  fprintf(Outfh, "  store%s %%.t%d, %%%s\n", qtype, temp, Buf);
#else
  fprintf(Outfh, "  store%s %%.i%d, %%%s\n", qtype, va_ptr, Buf);
#endif
}

// End the use of the variable argument list
void ilvaend(ILnode * n) {
  return;
}

void ilvaarg(ILnode * n) {
  char *qtype = qbetype(n->dtype);

  if (va_ptr == NOTEMP)
    fatal("va_arg() with no preceding va_start()\n");

  fprintf(Outfh, "  %%.t%d =%s vaarg %%.i%d\n", n->dst, qtype, va_ptr);
}

void ilcopystruct(ILnode * n) {
  int t = ilalloctemp();

  fprintf(Outfh, "  %%.i%d =l copy %d\n", t, n->count);
  fprintf(Outfh, "  call $memcpy(l %%.t%d, l %%.t%d, l %%.i%d)\n",
	  n->dst, n->src, t);
}

// Return a 64-bit hash value for 
// the string that n->src points at
void ilstrhash(ILnode * n) {
  fprintf(Outfh, "  %%.t%d =l call $aa_djb2hash(l %%.t%d)\n", n->dst, n->src);
}

void ilinittemp(ILnode * n) {
  char *qtype = qbetype(n->dtype);

  fprintf(Outfh, "  %%.t%d =%s copy %d\n", n->dst, qtype, n->count);
}

void ilinctemp(ILnode * n) {
  char *qtype = qbetype(n->dtype);

  fprintf(Outfh, "  %%.t%d =%s add %%.t%d, %d\n",
	  n->dst, qtype, n->dst, n->count);
}

void aagetval(ILnode * n) {
  int t1 = ilalloctemp();
  char *qtype = qbetype(n->dtype);
  char qbeprefix = ((n->flags & IL_GLOBAL) == 0) ? '%' : '$';

  // Get the associative array's name
  fgetstr(Buf, TEXTLEN, Infh);

  // Call the associative array lookup function
  fprintf(Outfh, "  %%.i%d =l call $al_get_aavalue(l %c%s, l %%.t%d)\n",
        t1, qbeprefix, Buf, n->src);

  // Narrow the result if the type is smaller than 64 bits
  fprintf(Outfh, "# n->dtype is %d\n", n->dtype);
  fprintf(Outfh, "  %%.t%d =%s copy %%.i%d\n", n->dst, qtype, t1);
}

void aasetval(ILnode * n) {
  char qbeprefix = ((n->flags & IL_GLOBAL) == 0) ? '%' : '$';
  char *qtype = qbe_exttype(n->stype);
  int t1;

  // Get the associative array's name
  fgetstr(Buf, TEXTLEN, Infh);

  // Widen the value's type to be 64 bits and
  // call the associative array set function
  if ((n->stype != TY_INT64) && (n->stype != (TY_INT64 + IL_UNSIGNED))) {
    t1 = ilalloctemp();
    fprintf(Outfh, "  %%.i%d =l ext%s %%.t%d\n", t1, qtype, n->dst);
    fprintf(Outfh, "  call $al_add_aakeyval(l %c%s, l %%.t%d, l %%.i%d)\n",
	qbeprefix, Buf, n->src, t1);
  } else {
    fprintf(Outfh, "  call $al_add_aakeyval(l %c%s, l %%.t%d, l %%.t%d)\n",
	qbeprefix, Buf, n->src, n->dst);
  }
}

void aaexists(ILnode * n) {
  char qbeprefix = ((n->flags & IL_GLOBAL) == 0) ? '%' : '$';

  // Get the associative array's name
  fgetstr(Buf, TEXTLEN, Infh);

  // Call the associative array lookup function
  fprintf(Outfh, "  %%.t%d =w call $al_exists_aakey(l %c%s, l %%.t%d)\n",
        n->dst, qbeprefix, Buf, n->src);
}

void aadelval(ILnode * n) {
  char qbeprefix = ((n->flags & IL_GLOBAL) == 0) ? '%' : '$';

  // Get the associative array's name
  fgetstr(Buf, TEXTLEN, Infh);

  // Call the associative array lookup function
  fprintf(Outfh, "  call $al_del_aakey(l %c%s, l %%.t%d)\n",
        qbeprefix, Buf, n->src);
}

void aaiterstart(ILnode * n) {
  char qbeprefix = ((n->flags & IL_GLOBAL) == 0) ? '%' : '$';

  // Get the associative array's name
  fgetstr(Buf, TEXTLEN, Infh);

  fprintf(Outfh, "  %%.t%d =l call $al_aa_iterstart(l %c%s)\n",
                        n->dst, qbeprefix, Buf);
}

void aanext(ILnode * n) {
  char qbeprefix = ((n->flags & IL_GLOBAL) == 0) ? '%' : '$';

  // Get the associative array's name
  fgetstr(Buf, TEXTLEN, Infh);

  fprintf(Outfh, "  %%.t%d =l call $al_getnext_aavalue(l %c%s)\n",
                        n->dst, qbeprefix, Buf);
}

void aafree(ILnode * n) {
  char qbeprefix = ((n->flags & IL_GLOBAL) == 0) ? '%' : '$';

  // Get the associative array's name
  fgetstr(Buf, TEXTLEN, Infh);

  fprintf(Outfh, "  call $al_free_aarray(l %c%s)\n", qbeprefix, Buf);
}

// Call a function that returns an array of pointers
// and begin the iteration over this list
void ilfunciterstart(ILnode * n) {
  int elemptr= n->src;
  int elemddref= n->dst;		// An output
  int Lifend= n->flags;
  int Lbreak= n->label;
  int Lfortop= n->count;
  int label2 = genlabel();
  int t1= ilalloctemp();
  int elemdref= ilalloctemp();
  char *qtype = qbetype(n->dtype);
  char *qloadtype = qbe_loadtype(n->dtype);

  // Compare elemptr against NULL and skip if it is
  fprintf(Outfh, "# Compare elemptr against NULL and skip if it is\n");
  fprintf(Outfh, "  jnz %%.t%d, @QL%d, @L%d\n", elemptr, label2, Lifend);
  qbelabel(label2);

  // Top of the foreach loop: deref elemptr and see if it is NULL
  fprintf(Outfh, "# Top of the foreach loop: is *element NULL?\n");
  fprintf(Outfh, "@L%d\n", Lfortop);
  fprintf(Outfh, "# %%.i%d is elemdref\n", elemdref);
  fprintf(Outfh, "  %%.i%d =l loadl %%.t%d\n", elemdref, elemptr);
  fprintf(Outfh, "  %%.i%d =w cnel %%.i%d, 0\n", t1, elemdref);
  label2 = genlabel();
  fprintf(Outfh, "  jnz %%.i%d, @QL%d, @L%d\n", t1, label2, Lbreak);
  qbelabel(label2);

  // Dereference elemdref to be stored into the destination temporary
  fprintf(Outfh, "# Dereference elemdref\n");
  fprintf(Outfh, "  %%.t%d =%s load%s %%.i%d\n",
					elemddref, qtype, qloadtype, elemdref);
}

// After the iteration body, output the code to move
// up to the next element in the array of pointers
void ilfunciternext(ILnode * n, ILnode *o) {
  int elemptr= n->src;
  int listptr= n->dst;
  int elemdref= ilalloctemp();
  int Lifend= n->flags;
  int Lbreak= n->label;
  int Lfortop= n->count;
  int Lcontinue= o->dst;

  // Deref and free elemptr
  fprintf(Outfh, "# Free elemdref\n");
  fprintf(Outfh, "  %%.i%d =l loadl %%.t%d\n", elemdref, elemptr);
  fprintf(Outfh, "  call $free(l %%.i%d)\n", elemdref);

  // Move elemptr up by sizeof(pointer)
  fprintf(Outfh, "# Move elemptr up by sizeof(pointer)\n");
  fprintf(Outfh, "@L%d\n", Lcontinue);
  fprintf(Outfh, "  %%.t%d =l add %%.t%d, %d\n", elemptr, elemptr, PTR_SIZE);

  // Jump to the top of the for loop
  fprintf(Outfh, "# Jump to the top of the for loop\n");
  fprintf(Outfh, "  jmp @L%d\n", Lfortop);

  // End of the for statement
  fprintf(Outfh, "# End of the for statement\n");
  fprintf(Outfh, "@L%d\n", Lbreak);

  // Free the list pointer
  fprintf(Outfh, "# Free the list pointer\n");
  fprintf(Outfh, "  call $free(l %%.t%d)\n", listptr);

  // End of the if statement
  fprintf(Outfh, "# End of the if statement\n");
  fprintf(Outfh, "@L%d\n", Lifend);
}

// Runtime check that the offset into a string is OK
void ilstrindex(ILnode *n) {
  int indextemp= n->dst;
  int basetemp= n->src;
  int functemp= n->count;
  int lentemp = ilalloctemp();
  int t1 = ilalloctemp();
  int Lgood = genlabel();
  int Lfail = genlabel();
  int label2;

  // Check that the base address isn't NULL
  fprintf(Outfh, "  %%.i%d =w cnel %%.t%d, 0\n", t1, basetemp);
  label2 = genlabel();
  fprintf(Outfh, "  jnz %%.i%d, @QL%d, @QL%d\n", t1, label2, Lfail);
  qbelabel(label2);

  // Check that the index isn't negative
  fprintf(Outfh, "  %%.i%d =w csgel %%.t%d, 0\n", t1, indextemp);
  label2 = genlabel();
  fprintf(Outfh, "  jnz %%.i%d, @QL%d, @QL%d\n", t1, label2, Lfail);
  qbelabel(label2);

  // Get the string's length
  fprintf(Outfh, "  %%.i%d =l call $strlen(l %%.t%d)\n", lentemp, basetemp);

  // Check that the index is below the length
  fprintf(Outfh, "  %%.i%d =w csltl %%.t%d, %%.i%d\n", t1, indextemp, lentemp);
  fprintf(Outfh, "  jnz %%.i%d, @QL%d, @QL%d\n", t1, Lgood, Lfail);

  // Output the call to .fatal() if the range checks fail
  fprintf(Outfh, "@QL%d\n", Lfail);
  fprintf(Outfh, "  call $.fatal(l $.stridxerr, l $L%d)\n", functemp);
  fprintf(Outfh, "@QL%d\n", Lgood);
}

// Begin the iteration over the characters in a string
void ilstriterstart(ILnode * n) {
  int strptr= n->src;
  int chtemp= n->dst;
  int Lbreak= n->label;
  int Lfortop= n->count;
  int t1 = ilalloctemp();
  int label2;

  // Check if the string base is NULL
  fprintf(Outfh, "# Start of a string iteration\n");
  fprintf(Outfh, "  %%.i%d =w cnel %%.t%d, 0\n", t1, strptr);
  label2 = genlabel();
  fprintf(Outfh, "  jnz %%.i%d, @QL%d, @L%d\n", t1, label2, Lbreak);
  qbelabel(label2);

  // Get the character at the current position
  fprintf(Outfh, "# Top of the foreach loop: is *strptr zero?\n");
  fprintf(Outfh, "@L%d\n", Lfortop);
  fprintf(Outfh, "  %%.t%d =w loadsb %%.t%d\n", chtemp, strptr);
  fprintf(Outfh, "  %%.i%d =w cnew %%.t%d, 0\n", t1, chtemp);
  label2 = genlabel();
  fprintf(Outfh, "  jnz %%.i%d, @QL%d, @L%d\n", t1, label2, Lbreak);
  qbelabel(label2);
}

// After the iteration body, output the code to move
// up to the next character in the string
void ilstriternext(ILnode * n) {
  int strptr= n->src;
  int Lbreak= n->label;
  int Lcontinue= n->flags;
  int Lfortop= n->count;

  // Move listptr up by one
  fprintf(Outfh, "# Loop increment\n");
  fprintf(Outfh, "@L%d\n", Lcontinue);
  fprintf(Outfh, "  %%.t%d =l add %%.t%d, 1\n", strptr, strptr);

  // Jump to the top of the for loop
  fprintf(Outfh, "# Jump to the top of the for loop\n");
  fprintf(Outfh, "  jmp @L%d\n", Lfortop);

  // End of the for statement
  fprintf(Outfh, "# End of the for statement\n");
  fprintf(Outfh, "@L%d\n", Lbreak);
}

extern char *get_astname(int op);

void il2qbe(void) {
  ILnode n;
  ILnode o;

  ilfile_preamble();

  // Loop reading ILnodes
  while (1) {
    if (fread(&n, sizeof(ILnode), 1, Infh) != 1) break;

 // fprintf(stderr, "IL op %s\n", get_astname(n.op));

    switch (n.op) {
    case A_ADD:           ilbinop(&n, "add"); break;
    case A_SUBTRACT:      ilbinop(&n, "sub"); break;
    case A_MULTIPLY:      ilbinop(&n, "mul"); break;
    case A_DIVIDE:        ilbinop(&n, "div"); break;
    case A_AND:           ilbinop(&n, "and"); break;
    case A_OR:            ilbinop(&n, "or"); break;
    case A_XOR:           ilbinop(&n, "xor"); break;
    case A_MOD:           ilbinop(&n, "rem"); break;
    case A_LSHIFT:        ilbinop(&n, "shl"); break;
    case A_RSHIFT:        ilbinop(&n, "shr"); break;
    case A_NEGATE:        ilnegate(&n); break;
    case A_NOT:           ilnot(&n); break;
    case A_INVERT:        ilinvert(&n); break;
    case A_EQ:
    case A_NE:
    case A_LT:
    case A_GT:
    case A_LE:
    case A_GE:            ilcompare(&n); break;
    case A_FUNCPREAMBLE:  ilfunc_preamble(&n); break;
    case A_FUNCPOSTAMBLE: ilfunc_postamble(&n); break;
    case A_LOCAL:         il_localvar(&n); break;
    case A_STORVAR:       ilstorvar(&n); break;
    case A_LOADVAR:       illoadvar(&n); break;
    case A_LOADLIT:       illoadlit(&n); break;
    case A_LOADBOOL:      illoadbool(&n); break;
    case A_FUNCCALL:      ilfunccall(&n); break;
    case A_STRLIT:        illoadstr(&n); break;
    case A_STRLITVAL:     ilstrlit(&n); break;
    case A_CAST:          ilcast(&n); break;
    case A_JUMP:          iljump(&n); break;
    case A_JUMPIFFALSE:   iljump_if_false(&n); break;
    case A_GENLABEL:      illabel(&n); break;
    case A_RETURN:        ilreturn(&n); break;
    case A_DEREF:         ilderef(&n); break;
    case A_STORDEREF:     ilstorderef(&n); break;
    case A_STORELEM:      ilstore_elem(&n); break;
    case A_ADDR:          iladdress(&n); break;
    case A_GLOBSYM:       ilglobsym(&n); break;
    case A_GLOBSYMVAL:    ilglobsymval(&n); break;
    case A_GLOBSYMEND:    ilglobsymend(&n); break;
    case A_ABORT:         ilabort(&n); break;
    case A_BOUNDS:        ilboundscheck(&n); break;
    case A_RANGE:         ilrangecheck(&n); break;
    case A_MOVE:          ilmove(&n); break;
    case A_VASTART:       ilvastart(&n); break;
    case A_VAARG:         ilvaarg(&n); break;
    case A_VAEND:         ilvaend(&n); break;
    case A_COPYSTRUCT:    ilcopystruct(&n); break;
    case A_STRHASH:       ilstrhash(&n); break;
    case A_INITTEMP:      ilinittemp(&n); break;
    case A_INCTEMP:       ilinctemp(&n); break;
    case A_AAGETVAL:      aagetval(&n); break;
    case A_AASETVAL:      aasetval(&n); break;
    case A_AAEXISTS:      aaexists(&n); break;
    case A_AADELVAL:      aadelval(&n); break;
    case A_AAITERSTART:   aaiterstart(&n); break;
    case A_AANEXT:        aanext(&n); break;
    case A_AAFREE:        aafree(&n); break;
    case A_STRINDEX:	  ilstrindex(&n); break;
    case A_STRINGITER:    ilstriterstart(&n); break;
    case A_SITERNEXT:     ilstriternext(&n); break;
    case A_FUNCITER:      ilfunciterstart(&n); break;
    case A_FITERNEXT:      
	if (fread(&o, sizeof(ILnode), 1, Infh) != 1) break;
	ilfunciternext(&n, &o); break;

    default: fatal("Unknown IL operation %d\n", n.op);
    }
  }
}
