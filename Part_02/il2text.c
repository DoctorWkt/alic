// IL to text translator for the alic compiler.
// (c) 2025 Warren Toomey, GPL3

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "alic.h"

FILE *Infh;

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

// List of IL types
static char *iltypename[]= {
  "int8",  "int16",  "int32",  "int64", "flt32", "flt64", "void", "bool",
  "uint8", "uint16", "uint32", "uint64"
};

static char *iltype(int ty) {
  return(iltypename[ty]);
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

// Perform a binary operation on two temporaries
void ilbinop(ILnode * n, char *op) {
  char *itype = iltype(n->dtype);
  printf("%s t%d = %s t%d, t%d\n", itype, n->dst, op, n->dst, n->src);
}

void ilnegate(ILnode * n) {
  char *itype = iltype(n->dtype);
  printf("%s t%d = neg t%d\n", itype, n->dst, n->dst);
}

void ilnot(ILnode * n) {
  char *itype = iltype(n->dtype);
  printf("%s t%d = not t%d\n", itype, n->dst, n->dst);
}

void ilinvert(ILnode * n) {
  char *itype = iltype(n->dtype);
  printf("%s t%d = inv t%d\n", itype, n->dst, n->dst);
};

// Print out the file preamble
void ilfile_preamble(void) {
  printf("File Preamble\n\n");
}

// Output a function's preamble
void ilfunc_preamble(ILnode * n) {
  int ty;
  int i;
  char *itype = iltype(n->dtype);

  // Get the function's name
  fgetstr(Buf, TEXTLEN, Infh);

  if (n->flags & IL_PUBLIC) printf( "public ");
  printf("%s %s(", itype, Buf);

  // Output the list of parameters
  for (i = 0; i < n->count; i++) {
    // Get its type and name
    fread(&ty, sizeof(int), 1, Infh);
    itype = iltype(ty);
    fgetstr(Buf, TEXTLEN, Infh);
    printf( "%s %s", itype, Buf);
    if (i < n->count - 1)
      printf( ", ");
  }

  // Print ... if the function is variadic
  if (n->flags & IL_VARIDIAC)
    printf( ", ...");

  printf( ") ");

  // If we have an exception variable
  if (n->flags & IL_EXCEPTVAR) {
    // Get its type and name
    fread(&ty, sizeof(int), 1, Infh);
    itype = iltype(ty);
    fgetstr(Buf, TEXTLEN, Infh);
    printf( "throws %s %s", itype, Buf);
  }

  printf("{ \n");
}

// Print out the function postamble
void ilfunc_postamble(ILnode * n) {

  printf( "}\n");
}

// Output a local variable
void il_localvar(ILnode * n) {
  int size;
  size = n->count;

  // Get the variables's name
  fgetstr(Buf, TEXTLEN, Infh);
  printf( "local %s, size %d bytes\n", Buf, size);
}

void ilstorvar(ILnode * n) {
  char *itype = iltype(n->dtype);
  fgetstr(Buf, TEXTLEN, Infh);
  printf("%s %s = t%d\n", itype, Buf, n->src);
}

void illoadvar(ILnode * n) {
  char *itype = iltype(n->dtype);
  fgetstr(Buf, TEXTLEN, Infh);
  printf( "%s t%d = %s\n", itype, n->dst, Buf);
}

void illoadlit(ILnode * n) {
  Litval value;
  char *itype = iltype(n->dtype);
  fread(&value, sizeof(Litval), 1, Infh);

  switch (n->dtype) {
  case TY_FLT32:
  case TY_FLT64:
    printf( "%s t%d = %f\n", itype, n->dst, value.dblval); break;
  default:
    printf( "%s t%d = %ld\n", itype, n->dst, value.intval);
  }
}

void illoadbool(ILnode * n) {
  char *itype = iltype(n->dtype);
  printf( "  t%d =%s copy %d\n", n->dst, itype, n->count);
}

void ilfunccall(ILnode * n) {
  int i;
  int ty;
  int temp;

  // Get the function's or funcptr's name
  fgetstr(Buf, TEXTLEN, Infh);
  printf( "%s(", Buf);

  // If the function has an exception variable, output it
  if (n->src != NOTEMP) {
    printf( "t%d", n->src);
    if (n->count != 0) printf( ", ");
  }

  // Output the list of arguments
  for (i = 0; i < n->count; i++) {
    fread(&ty, sizeof(int), 1, Infh);
    fread(&temp, sizeof(int), 1, Infh);
    printf( "%s t%d", iltype(ty), temp);

    // If the function is variadic
    if (i == n->label)
      printf( ", ... ");

    // Output any separating comma
    if (i < n->count - 1)
      printf( ", ");
  }

  printf( ")\n");
}

void illoadstr(ILnode * n) {
  printf( "t%d = $L%d\n", n->dst, n->label);
}

void ilstrlit(ILnode * n) {
  char *cptr;

  fgetstr(Buf, TEXTLEN, Infh);
  if ((n->flags & IL_ISCONST) != 0) printf( "const ");
  printf( "data $L%d = \"", n->label);
  for (cptr = Buf; *cptr; cptr++) printf( "%c", *cptr);
  printf( "\"\n");
}

// Given a temporary and a type, do a run-time
// check to ensure that the temporary's value
// fits into any type range
void ilrangecheck(ILnode * n) {
  char *itype = iltype(n->stype);
  int t = n->src;
  int lower = n->label;
  int upper = n->flags;

  printf("rangecheck %s t%d in [%d ... %d]\n", itype, t, lower, upper);
}

// Cast src to dst using their respective type.
void ilcast(ILnode * n) {
  char *stype = iltype(n->stype);
  char *dtype = iltype(n->dtype);
  printf( "%s t%d = cast(t%d, %s)\n", dtype, n->dst, n->src, stype);
}

// List of comparison operations. Add 6 for unsigned or 12 for floats
static char *qbecmp[] = {
  "eq", "ne", "slt", "sgt", "sle", "sge",
  "eq", "ne", "ult", "ugt", "ule", "uge",
  "eq", "ne", "lt", "gt", "le", "ge"
};

// Compare two temporaries
void ilcompare(ILnode * n) {
  char *itype = iltype(n->dtype);
  char *cmpstr;
  int offset = 0;

  if (is_unsigned(n->dtype)) offset = 6;
  if (is_flonum(n->dtype)) offset = 12;
  cmpstr = qbecmp[n->op - A_EQ + offset];

  printf( "%s t%d = c%s t%d, t%d\n",
	  itype, n->dst, cmpstr, n->dst, n->src);
}

// Output a jump to a label
void iljump(ILnode * n) {
  printf( "jmp L%d\n", n->label);
}

// Output a label
void illabel(ILnode * n) {
  printf( "L%d:\n", n->label);
}

// Jump to the label if the value in src is zero
void iljump_if_false(ILnode * n) {
  printf( "jnz t%d, L%d\n", n->src, n->label);
}

void ilreturn(ILnode * n) {
  // Only return a value if the function is not void
  if (n->dtype != TY_VOID)
    printf( "return %s t%d\n", iltype(n->dtype), n->src);
  else
    printf( "return\n");
}

// Dereference a pointer to get the value
// it points at into a new temporary
void ilderef(ILnode * n) {
  char *itype = iltype(n->dtype);
  printf( "%s t%d = deref t%d\n",
	  itype, n->dst, n->src);
}

void ilstorderef(ILnode * n) {
  char *itype = iltype(n->dtype);
  printf( "%s t%d = storederef t%d\n", itype, n->dst, n->src);
}

void iladdress(ILnode * n) {
  char *itype = iltype(n->dtype);
  fgetstr(Buf, TEXTLEN, Infh);
  printf( "%s t%d = &%s\n", itype, n->dst, Buf);
}

// Start a global symbol
void ilglobsym(ILnode * n) {
  int size = n->count;
  fgetstr(Buf, TEXTLEN, Infh);
  if (n->flags & IL_PUBLIC) printf( "public ");
  printf( "data $%s = { ", Buf);

  if ((n->flags & IL_ZERO) != 0)
    printf( "%d zeroes", size);
}

// Add a value to a global symbol
void ilglobsymval(ILnode * n) {
  char *itype = iltype(n->dtype);
  Litval value;
  int size = n->count;
  bool make_zero = (n->flags & IL_ZERO) != 0;

  // No initial value
  if (make_zero) {
    printf( "%d zeroes, ", size);
    return;
  }

  // We have a string value
  if (n->label != NOLABEL) {
    printf( "%s $L%d, ", itype, n->label);
    return;
  }

  // We have a numeric literal value. Get it
  fread(&value, sizeof(Litval), 1, Infh);
}

// End a global symbol
void ilglobsymend(ILnode * n) {
  printf( " }\n");
}

// Abort from the function
void ilabort(ILnode * n) {
  printf( "abort\n");
}

void ilstore_elem(ILnode * n) {
  printf( "store_elem t%d, t%d[%d]\n", n->dst, n->src, n->count);
}

// Do a bounds check on the src value. If below zero or
// >= count, call a function that will exit() the program.
void ilboundscheck(ILnode * n) {
  printf( "boundscheck t%d against t%d\n", n->dst, n->src);
}

void ilmove(ILnode * n) {
  char *itype = iltype(n->stype);

  printf( "%s t%d = copy t%d\n", itype, n->dst, n->src);
}

// Allocate space for the variable argument list
void ilvastart(ILnode * n) {
  fgetstr(Buf, TEXTLEN, Infh);
  printf( "vastart %s\n", Buf);
}

// End the use of the variable argument list
void ilvaend(ILnode * n) {
  printf( "vaend\n");
}

void ilvaarg(ILnode * n) {
  char *itype = iltype(n->dtype);

  printf( "%s t%d = vaarg\n", itype, n->dst);
}

void ilcopystruct(ILnode * n) {
  printf( "copystruct(t%d, t%d, size %d)\n",
	  n->dst, n->src, n->count);
}

// Return a 64-bit hash value for 
// the string that n->src points at
void ilstrhash(ILnode * n) {
  printf( "t%d = hash(t%d)\n", n->dst, n->src);
}

void ilinittemp(ILnode * n) {
  char *itype = iltype(n->dtype);

  printf( "%s t%d = #%d\n", itype, n->dst, n->count);
}

void ilinctemp(ILnode * n) {
  char *itype = iltype(n->dtype);
  printf( "%s t%d += #%d\n", itype, n->dst, n->count);
}

void aagetval(ILnode * n) {
  int t1 = 0;
  char *itype = iltype(n->dtype);
  char qbeprefix = ((n->flags & IL_GLOBAL) == 0) ? '%' : '$';

  // Get the associative array's name
  fgetstr(Buf, TEXTLEN, Infh);

  // Call the associative array lookup function
  printf( "  %%.i%d =l call $al_get_aavalue(l %c%s, l t%d)\n",
        t1, qbeprefix, Buf, n->src);

  // Narrow the result if the type is smaller than 64 bits
  printf( "# n->dtype is %d\n", n->dtype);
  printf( "  t%d =%s copy %%.i%d\n", n->dst, itype, t1);
}


void aasetval(ILnode * n) {
  char qbeprefix = ((n->flags & IL_GLOBAL) == 0) ? '%' : '$';
  char *itype = iltype(n->stype);
  int t1;

  // Get the associative array's name
  fgetstr(Buf, TEXTLEN, Infh);

  // Widen the value's type to be 64 bits and
  // call the associative array set function
  if ((n->stype != TY_INT64) && (n->stype != (TY_INT64 + IL_UNSIGNED))) {
    t1 = 0;
    printf( "  %%.i%d =l ext%s t%d\n", t1, itype, n->dst);
    printf( "  call $al_add_aakeyval(l %c%s, l t%d, l %%.i%d)\n",
	qbeprefix, Buf, n->src, t1);
  } else {
    printf( "  call $al_add_aakeyval(l %c%s, l t%d, l t%d)\n",
	qbeprefix, Buf, n->src, n->dst);
  }
}

void aaexists(ILnode * n) {
  char qbeprefix = ((n->flags & IL_GLOBAL) == 0) ? '%' : '$';

  // Get the associative array's name
  fgetstr(Buf, TEXTLEN, Infh);

  // Call the associative array lookup function
  printf( "  t%d =w call $al_exists_aakey(l %c%s, l t%d)\n",
        n->dst, qbeprefix, Buf, n->src);
}

void aadelval(ILnode * n) {
  char qbeprefix = ((n->flags & IL_GLOBAL) == 0) ? '%' : '$';

  // Get the associative array's name
  fgetstr(Buf, TEXTLEN, Infh);

  // Call the associative array lookup function
  printf( "  call $al_del_aakey(l %c%s, l t%d)\n",
        qbeprefix, Buf, n->src);
}

void aaiterstart(ILnode * n) {
  fgetstr(Buf, TEXTLEN, Infh);

  printf( "t%d = aaiterstart(%s)\n", n->dst, Buf);
}

void aanext(ILnode * n) {
  fgetstr(Buf, TEXTLEN, Infh);
  printf( "t%d = aanext(%s)\n", n->dst, Buf);
}

void aafree(ILnode * n) {
  fgetstr(Buf, TEXTLEN, Infh);
  printf( "aafree(%s)\n", Buf);
}

// Call a function that returns an array of pointers
// and begin the iteration over this list
void ilfunciterstart(ILnode * n) {
  int elemptr= n->src;
  int Lifend= n->flags;
  int Lfortop= n->count;

  printf( "# Compare elemptr against NULL and skip if it is\n");
  printf( "jnz t%d, L%d\n", elemptr, Lifend);
  printf( "# Top of the foreach loop: is *element NULL?\n");
  printf( "L%d\n", Lfortop);
  printf( "# Dereference elemdref\n");
}

// After the iteration body, output the code to move
// up to the next element in the array of pointers
void ilfunciternext(ILnode * n, ILnode *o) {
  int Lifend= n->flags;
  int Lbreak= n->label;
  int Lfortop= n->count;
  int Lcontinue= o->dst;

  printf( "# Free elemdref\n");
  printf( "# Move elemptr up by sizeof(pointer)\n");
  printf( "L%d\n", Lcontinue);
  printf( "# Jump to the top of the for loop\n");
  printf( "jmp L%d\n", Lfortop);
  printf( "# End of the for statement\n");
  printf( "L%d\n", Lbreak);
  printf( "# Free the list pointer\n");
  printf( "# End of the if statement\n");
  printf( "L%d\n", Lifend);
}

// Runtime check that the offset into a string is OK
void ilstrindex(ILnode *n) {
  printf("# Check that the base address isn't NULL\n");
  printf("# Check that the index isn't negative\n");
  printf("# Get the string's length\n");
  printf("# Check that the index is below the length \n");
}

// Begin the iteration over the characters in a string
void ilstriterstart(ILnode * n) {
  printf( "# Start of a string iteration\n");
  printf( "# Check if the string base is NULL\n");
  printf( "# Get the character at the current position\n");
  printf( "# Top of the foreach loop: is *strptr zero?\n");
}

// After the iteration body, output the code to move
// up to the next character in the string
void ilstriternext(ILnode * n) {
  printf( "# Loop increment\n");
  printf( "# Move listptr up by one\n");
  printf( "# Jump to the top of the for loop\n");
  printf( "# End of the for statement\n");
}

void il2text(void) {
  ILnode n;
  ILnode o;

  ilfile_preamble();

  // Loop reading ILnodes
  while (1) {
    if (fread(&n, sizeof(ILnode), 1, Infh) != 1) break;

  // printf("\t\t\t\tIL op %s\n", get_astname(n.op));

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
    case A_ADDR:          iladdress(&n); break;
    case A_STRLIT:        illoadstr(&n); break;
    case A_LOADLIT:       illoadlit(&n); break;
    case A_LOCAL:         il_localvar(&n); break;
    case A_STORVAR:       ilstorvar(&n); break;
    case A_LOADVAR:       illoadvar(&n); break;
    case A_LOADBOOL:      illoadbool(&n); break;
    case A_FUNCCALL:      ilfunccall(&n); break;
    case A_STRLITVAL:     ilstrlit(&n); break;
    case A_CAST:          ilcast(&n); break;
    case A_JUMP:          iljump(&n); break;
    case A_JUMPIFFALSE:   iljump_if_false(&n); break;
    case A_GENLABEL:      illabel(&n); break;
    case A_RETURN:        ilreturn(&n); break;
    case A_DEREF:         ilderef(&n); break;
    case A_STORDEREF:     ilstorderef(&n); break;
    case A_STORELEM:      ilstore_elem(&n); break;
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

int main(int argc, char **argv) {

  if (argc != 2) {
    fprintf(stderr, "Usage: %s file.i\n", argv[0]); exit(1);
  }

  if ((Infh = fopen(argv[1], "r")) == NULL) {
    fprintf(stderr, "Unable to open %s: %s\n", argv[1], 
            strerror(errno));
    exit(1);
  }

  il2text();
  exit(0);
}
