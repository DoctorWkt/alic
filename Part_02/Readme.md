# alic Part 2: An Intermediate Language

I've had some progress with things since I sketched my plan of attack in part 1 of this "alic on PDP-11" journey. I've set up:

  * a complete [2.11BSD system](https://github.com/chasecovello/211bsd-pidp11) running on [Open SimH](https://opensimh.org/) with a C compiler that understands ANSI function prototypes, and
  * a cut-down PDP-11 environment using my own [Apout emulator](https://github.com/DoctorWkt/unix-jun72/tree/master/tools/apout) with the same C compiler as above.

Both of these allow me to set up the *alic* `Makefile` with suitable defines for the PDP-11 platform and try to compile the C version of the *alic* compiler. The Open SimH system faithfully simulates the 2.11BSD operating system, but it's fiddly to get files into and out of the system and the speed is slower than Apout. On the other hand, the Apout emulator is fast and it can access the existing C files with no file transfers; however, it doesn't have any floating point emulation at present.

So I'm using the Apout environment for now until I hit a floating-point issue.

## What I've Learned So Far

First up, the C compiler on 2.11BSD is a bit pernickety. For example, you have to declare all function variables before you can use them (just like *alic*!). It took me a little while to move lines of code around in order to get all the C source files to compile with the 2.11BSD C compiler. But they now all compile.

When it came to linking the compiler into a single executable, I ran out of code space in the 64K of available code memory. That wasn't surprising. What was pleasantly surprising was that I actually came close to fitting! If I replace all the functions in [cgen.c](cgen.c) with code that simply returns a value matching each function's type, I get object files with these sizes in bytes:

```
$ size *.o
text    data    bss     dec     hex
1226    638     0       1864    748     astnodes.o
632     2       0       634     27a     cgen.o
450     30      0       480     1e0     expr.o
902     478     0       1380    564     funcs.o
6904    886     0       7790    1e6e    genast.o
2838    1634    0       4472    1178    lexer.o
1736    1090    2       2828    b0c     main.o
346     58      0       404     194     misc.o
14294   4866    4       19164   4adc    parser.o
420     138     0       558     22e     stmts.o
196     2       0       198     c6      strlits.o
1310    166     0       1476    5c4     syms.o
2538    978     256     3772    ebc     types.o

$ size alic
text    data    bss     dec     hex
42112   11720   1012    54844   d63c
```

So that is promising! My next step was to bring back the full [cgen.c](cgen.c) code; then, separate the lexer into its own program to remove the 2.8K of lexer code from the compiler executable. Unfortunately, the code minus the lexer was still too big to fit into 64K. Sigh.

> *Aside: I also threw away the code to serialise the token stream; I wish I hadn't now.*

Given that the [cgen.c](cgen.c) code is much bigger than the lexer code, I've decided to split it out into a separate program and serialise the function calls between [genast.c](genast.c) and [cgen.c](cgen.c).

## An Intermediate Language is Born

What I've effectively done is to create an [*intermediate language*](https://en.wikipedia.org/wiki/Intermediate_representation) for *alic*. You've probably already come across examples of intermediate representation before, e.g. LLVM.

I've taken all the `cg_XXX()` function calls in [genast.c](genast.c) and replaced them with a call to a function called `outIL()` which fills in and writes out this new structure (in [alic.h](alic.h)):

```
// Nodes for the intermediate language
struct ILnode {
  int8_t op;            // Operation
  int8_t dtype;         // Destination type
  int8_t stype;         // Source type
  int dst;              // Destination temporary
  int src;              // Source temporary
  int label;            // Jump label
  int flags;            // Bitfield of flags
  int count;            // Any required count
};
```

Thus, [genast.c](genast.c) now produces a serialised stream of `ILnode`s. A separate program, called `il2asm` reads each `ILnode` and sends it to a matching function in the file [il2qbe.c](il2qbe.c). If you look at the code at the bottom of this file, you will see:

```
void il2qbe(void) {
  ILnode n;

  ilfile_preamble();

  // Loop reading ILnodes
  while (1) {
    if (fread(&n, sizeof(ILnode), 1, Infh) != 1) break;

    switch (n.op) {
    case A_ADD:           ilbinop(&n, "add"); break;
    case A_SUBTRACT:      ilbinop(&n, "sub"); break;
    case A_MULTIPLY:      ilbinop(&n, "mul"); break;
    case A_DIVIDE:        ilbinop(&n, "div"); break;
    ...
    default: fatal("Unknown IL operation %d\n", n.op);
    }
  }
}

```

## Intermediate Language Operations

The intermediate language is designed not to be dependent on any platform or backend. It also should not be too *alic*-specific. For example, the only types in the intermediate language are the four signed integer types, the four unsigned integer types, the two floating-point types and the boolean type.

I've ended up with sixty four intermediate language operations. As many of them are the same as the AST operations, I've kept the existing AST operations and added more "A_" operations which are solely used in the intermediate language. Here is the complete list of IL operations:

A_AADELVAL, A_AAEXISTS, A_AAFREE, A_AAGETVAL, A_AAITERSTART, A_AANEXT,
A_AASETVAL, A_ABORT, A_ADD, A_ADDR, A_AND, A_BOUNDS, A_CAST, A_COPYSTRUCT,
A_DEREF, A_DIVIDE, A_EQ, A_FITERNEXT, A_FUNCCALL, A_FUNCITER, A_FUNCPOSTAMBLE,
A_FUNCPREAMBLE, A_GE, A_GENLABEL, A_GLOBSYM, A_GLOBSYMEND, A_GLOBSYMVAL,
A_GT, A_INCTEMP, A_INITTEMP, A_INVERT, A_JUMP, A_JUMPIFFALSE, A_LE,
A_LOADBOOL, A_LOADLIT, A_LOADVAR, A_LOCAL, A_LSHIFT, A_LT, A_MOD, A_MOVE,
A_MULTIPLY, A_NE, A_NEGATE, A_NOT, A_OR, A_RANGE, A_RETURN, A_RSHIFT,
A_SITERNEXT, A_STORDEREF, A_STORELEM, A_STORVAR, A_STRHASH, A_STRINDEX,
A_STRINGITER, A_STRLIT, A_STRLITVAL, A_SUBTRACT, A_VAARG, A_VAEND, A_VASTART,
A_XOR.

Many of the operations need only the single `ILnode`. Operations that refer to symbols, e.g. A_LOADVAR, are serialised with a following NUL-terminated string that has the symbol's name. Operations that need a numeric literal, e.g. A_LOADVAR, are serialised with a following `Litval` node. There is a single IL operation, `A_FITERNEXT` where I couldn't fit all the operations information into a single `ILnode`, so it gets followed by a second `ILnode` with the rest of the information required.

As an example, here is the [il2qbe.c](il2qbe.c) function to do addition:

```
// Perform a binary operation on two temporaries
void ilbinop(ILnode * n, char *op) {
  // Get the matching QBE type
  char *qtype = qbetype(n->dtype);

  fprintf(Outfh, "  %%.t%d =%s %s %%.t%d, %%.t%d\n",
          n->dst, qtype, op, n->dst, n->src);
}
...
    // Down in the main loop
    case A_ADD:           ilbinop(&n, "add"); break;

```

## The ILnode Fields

Here is the `ILnode` struct again:

```
// Nodes for the intermediate language
struct ILnode {
  int8_t op;            // Operation
  int8_t dtype;         // Destination type
  int8_t stype;         // Source type
  int dst;              // Destination temporary
  int src;              // Source temporary
  int label;            // Jump label
  int flags;            // Bitfield of flags
  int count;            // Any required count
};
```

The `dtype` and `stype` fields hold the same values as the existing type kinds up to `TY_BOOL`:

```
enum {
  TY_INT8, TY_INT16, TY_INT32, TY_INT64, TY_FLT32, TY_FLT64,
  TY_VOID, TY_BOOL, <the rest are unused, however ...>
};

```

To represent the unsigned types:

```
// If an ILnode type is unsigned, we add this
// to the optype to get it past TY_BOOL
#define IL_UNSIGNED 8
```

So `TY_INT16` is enum 1 and `TY_UINT16` is enum 9.

The `dst` and `src` temporaries are just like the temporaries that we had before, but with a wrinkle. Previously these were generated in [cgen.c](cgen.c) and returned by the functions therein. Now there is a unidirectional flow of `ILnode` structs that leave [genast.c](genast.c); this means that generation of the temporary numbers is now done by [genast.c](genast.c).

The `label` field is used by the `A_JUMP` and `A_JUMPIFFALSE` operations. Again, the label numbers have to be generated by [genast.c](genast.c).

The `count` field is used when we have a count of things, e.g. the number of parameters in a function declaration or the number of arguments to a function call. In these cases, the `ILnode` with the operation can be followed by `count` number of other records. For example in [il2qbe.c](il2qbe.c):

```
// Output a function's preamble
void ilfunc_preamble(ILnode * n) {
  ...
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
}
```

The `fgetstr()` function reads in a NUL-terminated string from the IL file.

## Exceptions to the Rule

Of course, the fields in the `ILnode` *should* be used in accordance with their names, but things are never so simple. For example, when we are doing function iterations we need to know about several temporaries and several loop labels. Instead of adding a lot more fields to the `ILnode` struct, I simply fill them in where I need information, e.g.

```
void ilfunciterstart(ILnode * n) {
  int elemptr= n->src;
  int elemddref= n->dst;                // An output
  int Lifend= n->flags;
  int Lbreak= n->label;
  int Lfortop= n->count;
  ...
  // Dereference elemdref to be stored into the destination temporary
  fprintf(Outfh, "# Dereference elemdref\n");
  fprintf(Outfh, "  %%.t%d =%s load%s %%.i%d\n",
                                        elemddref, qtype, qloadtype, elemdref);
}
```

Note `elemddref`. Previously, we could allocate a temporary, put a value in it and return it from [cgen.c](cgen.c). Now we need [genast.c](genast.c) to allocate the temporary so that we can put a value into it.

## The ILnode Flags

Back when we had [cgen.c](cgen.c), we could pass `Sym` and `Type` nodes to the functions in it. Now we can't! So I extract the information needed from these nodes and pass them as bit flags to the functions now in [il2qbe.c](il2qbe.c):

```
// ILnode flags
#define IL_EXCEPTVAR 0x1        // Function has an exception variable
#define IL_VARIDIAC  0x2        // Function is varidiac
#define IL_PUBLIC    0x4        // Symbol is public
#define IL_GLOBAL    0x8        // Symbol is non-local
#define IL_ZERO      0x10       // Zero-fill the symbol
#define IL_HASADDR   0x20       // Symbol has an address
#define IL_ISCONST   0x40       // Symbol is constant
#define IL_ISARRAY   0x80       // Symbol is an array
#define IL_ISFUNCPTR 0x100      // Symbol is a function pointer
#define IL_ISAARRAY  0x200      // Symbol is an associative array
#define IL_ISFUNCTION 0x400     // Symbol is a function
```

In [genast.c](genast.c) you will see code that looks like this in places:

```
      flags = 0;
      if (n->sym->visibility != SV_LOCAL)   flags = flags | IL_GLOBAL;
      if (n->sym->has_addr)                 flags = flags | IL_HASADDR;
      if (is_array(n->sym))                 flags = flags | IL_ISARRAY;
      if (n->sym->type->kind == TY_FUNCPTR) flags = flags | IL_ISFUNCPTR;
      if (n->sym->symtype == ST_FUNCTION)   flags = flags | IL_ISFUNCTION;
      outIL(A_LOADVAR, n->sym->type, NULL, temp, 0, 0, flags, 0);
```

## Getting Around Calling `genAST()`

Another problem I had to overcome is that a few of the functions in the old [cgen.c](cgen.c) were actually calling `genAST()`, so there was a circular dependency between the functions in [cgen.c](cgen.c) and in [genast.c](genast.c). That's now impossible as the two files live in separate executables. How do we solve this problem?

Well, first up, why did we need to do this in the first place? It happens mostly when we are doing iteration, e.g. with `foreach` loops. Here's an example where we are iterating across the characters in a string (from the old [cgen.c](cgen.c) code):

```
// Iterate over all the characters in a string
int cg_stringiterator(ASTnode * n, Breaklabel *this) {
  ...

  // Get a copy of the base of the string
  listptr= genAST(n->mid);
  ...

  // Top of the loop, get the next character
  t1= cgderef(listptr, n->left->type);

  // Assign the deref'd listptr to the loop variable
  assign= mkastnode(A_ASSIGN, NULL, NULL, n->left);
  t2= genAST(n->left);
  gen_assign(t1, t2, assign);

  // Loop body
  fprintf(Outfh, "# Loop body\n");
  genAST(n->right);
  ...
}
```

We have to move all three `genAST()` calls back into [genast.c](genast.c). At the same time, we have to get [genast.c](genast.c) to give us an already-allocated temporary so that we can do the `cgderef()` and put the character into a temporary that [genast.c](genast.c) already knows.

So, to solve this problem, I've broken all the iteration operations into two parts: the part that comes before the loop body (initialisation) and the part that comes after the loop body (incrementing). As an example, here is the string iteration code now in [genast.c](genast.c):

```
// Iterate over all the characters in a string
static void gen_stringiterator(ASTnode * n, Breaklabel * this) {
  int loopvar;
  int strptr;
  int chtemp = ilalloctemp();   // Holds the next character
  int Lfortop = genlabel();
  ASTnode *assign;

  // Get the pointer to the loop variable into a temporary
  loopvar = genAST(n->left);

  // Get a copy of the base of the string
  strptr = genAST(n->mid);

  // Start the iteration, get back the next character
  outIL(A_STRINGITER, NULL, NULL, chtemp, strptr,
        this->break_label, 0, Lfortop);

  // Store the character into the loop variable
  assign = mkastnode(A_ASSIGN, NULL, NULL, n->left);
  gen_assign(chtemp, loopvar, assign);

  // Generate the loop body
  genAST(n->right);

  // Generate the code after the loop body
  outIL(A_SITERNEXT, NULL, NULL, 0, strptr,
        this->break_label, this->continue_label, Lfortop);
}
```

We have two IL operations: `A_STRINGITER` and `A_SITERNEXT`. Both get the temporary (`chtemp`) that points at the next character in the string. The first operation fills in the `chtemp` temporary with the actual next character so that we can assign it through the `loopvar` temporary. And now all three children of the string iterator `ASTnode` get sent through `genAST()`.

## Trying the Intermediate Language Out

I've written a program called `il2text` which takes the serialised stream of `ILnode`s and converts them into a textual format. It's very ad-hoc and it isn't formalised like a "proper" intermediate language. To try it out, e.g.

```
$ make install
  ...
  mkdir -p /tmp/alic/bin
  cp alic il2asm il2text /tmp/alic/bin

$ ./alic -S tests/test073.al

$ ls -lrt tests/
  ...
-rw-r--r-- 1 wkt wkt 4956 Aug 12 20:32 test073.q
-rw-r--r-- 1 wkt wkt 5099 Aug 12 20:32 test073.i
-rw-r--r-- 1 wkt wkt 4538 Aug 12 20:32 test073.s

$ ./il2text tests/test073.i
File Preamble

int32 gcf(uint64 e, int32 num1) throws int32 num2{ 
local temp, size 4 bytes
int32 t2 = num1
int32 t3 = 0
int32 t2 = csle t2, t3
jnz t2, L2
int32 t4 = 33
uint64 t5 = e
uint64 t6 = 0
uint64 t5 = add t5, t6
int32 t5 = storederef t4
abort
L2:
int32 t7 = num2
...
}
```

## Conclusion and The Next Step

Right now I have all 224 tests working with the C version of the *alic* compiler that has the intermediate language. There is still only the QBE backend in [il2qbe.c](il2qbe.c). I have not even tried to merge all of this into the *alic* version of the compiler that normally lives in the [cina/](cina/) directory: there are going to be more major changes to the compiler. I will wait until things settle down and then re-translate it all again, sigh!

The existing compiler also compiles and runs on the PDP-11 and can translate several of the test files into the intermediate language, and print them out using `il2text`. But it fails on any substantially large input.

Thus, I think my next goals are to a) start to write the PDP-11 backend `il2pdp11.c` and produce PDP-11 assembly code and b) begin the process of putting the compiler on a "data diet".

