# *alic* Part 22: My To-Do List

I've keep keeping a "to-do" list for *alic*; it's a list of little changes that I'd like to add to the language, none of which are big enough to be a single part of the journey. Now that I've run out of "big" ideas, I thought I would work my way through the "to-do" list!

## Copying Structs

We should be able to do:

```
type FOO = struct { ... } ;
...
  FOO x;
  FOO y;
  ...
  y= x;
```

and the whole struct will get copied. I've added this function to generate the QBE code to call `memcpy()` at the bottom of [cgen.c](cgen.c):

```
// Copy a struct to another struct
// given pointers to both
int cg_copystruct(int srctemp, int desttemp, int size) {
  int t= cgalloctemp();

  fprintf(Outfh, "  %%.t%d =l copy %d\n", t, size);
  fprintf(Outfh, "  call $memcpy(l %%.t%d, l %%.t%d, l %%.t%d)\n",
                        desttemp, srctemp, t);
  return(NOTEMP);
}
```

In [genast.c](genast.c) we now have a special case for assignments:

```
  // Do special case nodes before the general processing
  switch (n->op) {
  case A_ASSIGN:
    // If left and right are struct types, use
    // memcpy() to copy them
    if (is_struct(n->left->type) && is_struct(n->right->type)) {
      lefttemp= cgaddress(n->left->sym);
      righttemp= cgaddress(n->right->sym);
      return(cg_copystruct(lefttemp, righttemp, n->left->type->size));
    }
    break;
  ...
  }
```

The code in [parser.c](parser.c) already checks that both sides of an assignment have compatible types, so we only have to check that both sides are structs. And it works, see test 219.

## Relaxing the Bracketed Expression List Rules

When I was coding `cgen.al` in *alic*, I had to write this initialisation code:

```
Cvtrow cvt[8]= {
  { { 0,    0,    0,    C_E, C_M,  C_M,  C_M,  C_ME } },        // int8
  { { C_MX, 0,    0,    C_E, C_MX, C_M,  C_M,  C_ME } },        // int16
  { { C_MX, C_MX, 0,    C_E, C_MX, C_MX, C_M,  C_ME } },        // int32
  { { C_MX, C_MX, C_MX, 0,   C_MX, C_MX, C_MX, C_M  } },        // int64
  { { C_X,  0,    0,    C_E, 0,    0,    0,    C_E  } },        // uint8
  { { C_X,  C_X,  0,    C_E, C_X,  0,    0,    C_E  } },        // uint16
  { { C_X,  C_X,  C_X,  C_E, C_X,  C_X,  0,    C_E  } },        // uint32
  { { C_X,  C_X,  C_X,  C_X, C_X,  C_X,  C_X,  0    } }         // uint64
};
```

and I couldn't work out why I needed nested `{ { ... } }`.

I've decided to relax the rules for bracketed expression lists. From now on, any `{ ... }` inside the outermost `{ ... }` in a bracketed expression list are optional; you can put them in anywhere you like but the compiler will simply ignore them. It means that all the values/expressions inside the outermost `{ ... }` should form a single list headed by an `A_BEL` ASTnode.

Here's the new code for `bracketed_expression_list()` in [parser.c](parser.c):

```
// Get a bracketed expression list.
// Regardless of the number of nested '{' ... '}',
// we build a single list of expressions
// headed by an A_BEL node.
//
// For bracketed expression lists, we keep a
// count of the depth of '{' nesting.
//
static int bel_depth;

//- bracketed_expression_list= LBRACE bracketed_expression_element
//-                                   (COMMA bracketed_expression_element)*
//-                            RBRACE
//-
static ASTnode *bracketed_expression_list(void) {
  ASTnode *bel;
  ASTnode *this;
  ASTnode *last;

  // Skip the left brace
  scan(&Thistoken);

  // Make the BEL node which will hold the list
  bel = mkastnode(A_BEL, NULL, NULL, NULL);
  last= NULL;
  bel_depth=1;

  // Loop getting expressions
  while (1) {
    switch(Thistoken.token) {
      case T_COMMA:
        scan(&Thistoken);
        break;
      case T_LBRACE:
        scan(&Thistoken);
        bel_depth++;
        break;
      case T_RBRACE:
        scan(&Thistoken);
        bel_depth--;
        break;
      default:
        this= bracketed_expression_element();
        if (last== NULL) {
          bel->left= last= this;
        } else {
          last->mid= this; last= this;
        }
    }
    if (bel_depth == 0) break;
  }
  return(bel);
}
```

The rules are even more relaxed than what the grammar suggests. This can now be parsed:

```
int32 fred[5] = { 1 2 3 {} , , , {} {} {} {} , 4 {{{}} 5 } };
```

I should document in the overview that internal commas and `{ ... }` are now entirely optional and there for the programmer's convenience only.

Now that we don't create nested `A_BEL` AST trees, the code in `check_bel()` in [genast.c](genast.c) has been changed to deal with the single `A_BEL` list. It is still recursive, but each instance of `check_bel()` now returns what is left of the `A_BEL` list that still needs to be consumed.

## Pointers to Functions that Throw Exceptions

Back when I added function pointers, I didn't deal with functions that throw exceptions. It's been on my list to fix, which I've now done.

Because function pointers are variables of a function pointer type, I've had to add another member to the `Type` struct in [alic.h](alic.h):

```
struct Type {
  ...
  Type *excepttype;             // Exception type for a function pointer
  ...
};
```

Normal functions have symbols and, in the `Sym` symbol table, have a `Sym *exceptvar` member which points at the variable used to throw an exception. We need to copy this in to a function pointer so that the variable is pushed onto the stack when we call the function.

To do this, I've added another special case to the `A_ASSIGN` AST operation in `genAST()` in [genast.c](genast.c):

```
  case A_ASSIGN:
    ...
    // If the right-hand side is a function pointer,
    // copy the exception variable from the left
    if (n->right->type->kind == TY_FUNCPTR)
      n->right->sym->exceptvar= n->left->sym->exceptvar;
    break;
```

The `break` means that we do this first and then fall into the usual `A_ASSIGN` handling code further down.

Now we need to modify the parser to receive a declaration of a function pointer type that throws an exception. In [parser.c](parser.c):

```
//- funcptr_declaration= FUNCPTR type
//-                      LPAREN type_list (COMMA ELLIPSIS)? RPAREN
//-                      (THROWS type)?
//-
static void funcptr_declaration(char *typename) {
  ...
  // If we have a "throws", skip it
  // and get the exception type
  if (Thistoken.token == T_THROWS) {
    scan(&Thistoken);
    excepttype = match_type(false);

    // The type must be a pointer to a struct which
    // has an int32 as the first member
    <code ommitted>
  }

  // Add the type to the table of types
  ty= new_type(TY_FUNCPTR, ty_voidptr->size, false, 0, typename, NULL);
  ty->rettype= rettype;
  ty->paramtype= paramtype;
  ty->excepttype= excepttype;
  ...
}
```

And there is one last thing we need to do. In [types.c](types.c) we have a function to check that an existing function matches the type of a function pointer, `get_funcptr_type()`. We need to modify this to also check any exception variable's type against the function pointer's exception type:

```
Type *get_funcptr_type(Sym *sym) {
  ...

    // Skip if the exception types do not match
    if (this->excepttype != NULL) {
      if (sym->exceptvar == NULL || sym->exceptvar->type != this->excepttype)
        continue;
    }
  ...
}
```

Test 221 checks that we can point a function pointer at a function that throws an exception, call through the function pointer and receive the exception that it throws.

## Use Enum Names as well as NUMLITs

In a couple of places in the *alic* grammar we were forced to use integer numeric literals where it should be possible to use `enum` names. I've just changed the grammar to allow this. Here are the changes:

```
type_declaration= TYPE IDENT SEMI
                | TYPE IDENT ASSIGN type integer_range? SEMI
                | TYPE IDENT ASSIGN struct_declaration  SEMI
                | TYPE IDENT ASSIGN funcptr_declaration SEMI

integer_range= RANGE integer_constant ... integer_constant

integer_constant= NUMLIT | ENUMVAL

array_size= (LBRACKET integer_constant RBRACKET)+
```

We have a new function in [parser.c](parser.c) called `integer_constant()` which parses either an integer literal or an `enum` name and returns its value. This is now called in the places where, previously, the code was manually parsing and checking a `T_NUMLIT` token.

Test 222 checks that we can use `enum` names in range declarations and array sizes.

## Adding more *alic*-isms to the `cina/` Compiler

Each time I add/change code in the C version of the compiler, I take the change and add it to the *alic* version of the compiler in the [cina/](cina/) directory. I usually only make the minimal enough changes to get it to work.

I've just gone back and added several *alic*-isms to the compiler in [cina/](cina/). I've added `const` everywhere that I could, added a couple of `inout` parameters and replaced four or five `for` loops with `foreach` loops.

## Getting RISC-V to Work

QBE supports three 64-bit CPUs: AMD64, ARM and RISC-V. I've previously been able to get the AMD64 and ARM backends to work but not the RISC-V one. I just checked again: I can pass the *alic* triple tests, but several of the tests are failing.

Actually, some tests are failing on the ARM backend too. We have:

  * ARM: tests 161, 173, 174, 175.
  * RISC-V: tests 117, 118, 133, 135, 161, 163, 164, 165, 166, 185, 186, 187, 196, 203, 204, 205.  

A lot of the RISC-V failures are in `.fatal()` (the function I embed in each QBE output) not printing out the proper name of symbols.

Luckily, we have the [cproc compiler](https://sr.ht/~mcf/cproc/) which is a C compiler that generates working QBE code for all three CPUs. What I did was rewrite `.fatal()` in C and compile it with `cproc` on all three platforms.

It turns out that the QBE code to do varidiac functions differs on all three platforms. I changed the `Makefile` for the compiler to generate a CPU architecture define in `incdir.h`. Now in [cgen.c](cgen.c) we have:

```
// Print out the file preamble
void cg_file_preamble(void) {
  // Output a copy of the function that emits
  // an error message and exit()s
#ifdef CPU_aarch64
  ...
#endif
#ifdef CPU_x86_64
  ...
#endif
#ifdef CPU_riscv64
  ...
#endif
```

This fixes nearly all of the RISC-V tests except test 135 and test 161.

Tests 173-175 on the ARM platform have associative arrays with string keys. Actually, that's not true: they use `int8 *` keys. When I introduced the `string` type I changed the code to only check for the `string` type. I've now added code in [genast.c](genast.c) that looks like this instead:

```
  // If the key type is ty_string or a pointer to ty_int8,
  // assume it's a string. Call an external function
  // to get its hash value
  if (n->left->sym->keytype == ty_string ||
      n->left->sym->keytype == pointer_to(ty_int8)) {
    keytemp= cg_strhash(keytemp);
  } else {
    // Otherwise widen the key value to be 64 bits
    keytemp = cgcast(keytemp, n->left->sym->keytype, ty_uint64, NOTEMP);
  }
```

Test 161 was a really stupid bug when converting a floating-point value to a singled integer one: I was using the `dtoui` (double to unsigned int) QBE instruction not `dtosi`! The single letter bug fix in [cgen.c](cgen.c) is:

```
    if (ty->is_unsigned) {
      fprintf(Outfh, "  %%.t%d =l %stoui %%.t%d\n", t2, qetype, exprtemp);
      ety= ty_uint64;
    } else {
      //                     NOT  %stoui !!!
      fprintf(Outfh, "  %%.t%d =l %stosi %%.t%d\n", t2, qetype, exprtemp);
      ety= ty_int64;
    }
```

With this done, we are now down to:

  * All tests pass for both compilers, and the triple test, on AMD64 and ARM.
  * On RISC-V, test 135 fails with the C version of the compiler, and many tests fail with the *alic* version of the compiler. It looks like a varidiac problem again.

After a bit of work, I found that I had to add this change to `cg_vastart()`:

```
// Allocate space for the variable argument list
void cg_vastart(ASTnode *n) {
#ifdef CPU_riscv64
  int temp;
#endif
  ...
  // Allocate the storage for the list
  // and get a pointer to it
  ...

  // Also save it in the program's pointer
#ifdef CPU_riscv64
  temp= cgalloctemp();
  fprintf(Outfh, "  %%.t%d =l loadl %%.t%d\n", temp, va_ptr);
  cgstorvar(temp, n->type, n->sym);
#else
  cgstorvar(va_ptr, n->type, n->sym);
#endif
}
```

With this change, we now pass all tests with the C and *alic* compilers, and the triple test, on all three platforms: AMD64, ARM and RISC-V. Fantastic!

## A Ranged Type Bug

I noticed this while doing all of the above. This compiles when it shouldn't:

```
type FOO = int32 range 32 ... 64;

FOO x = 93;
```

We should check the range of the type when we are providing initial values. The fix for non-local variables was nice and easy. In `check_bel()` at the bottom:

```
  // We are generating a non-local value
  if (basetemp == NOTEMP) {
    // It has to be a literal value
    if ((list->op != A_NUMLIT) && (list->op != A_STRLIT))
      fatal("Initialisation value not a literal value\n");

    // Check any ranged type against the initial value
    if (has_range(ty) && list->op == A_NUMLIT) {
      if ((list->litval.intval < ty->lower) ||
          (list->litval.intval > ty->upper))
        fatal("Value %d outside range of type %s\n",
                list->litval.intval, ty->name);
    }
  ...
  }
```

Now I need to add a run-time range check for local variable initialisations.

Again, this was easy. I've added a run-time range check to `check_bel()` at the bottom:

```
    // We are dealing with a local variable.
    // Generate the expression's code and get the value
    exprtemp= genAST(list);

    // Check the expression's range if required
    if (has_range(ty)) {
      functemp = add_strlit(Thisfunction->name, true);
      cgrangecheck(exprtemp, ty, functemp);
    }

    // Assign it into the aggregate variable at the offset
    cgstore_element(basetemp, offset, exprtemp, ty);
```

And there is similar code added to `gen_local()`; all of the above is in [cgen.c](cgen.c).

## Conclusion and The Next Step

I'm starting to feel that the *alic* language and the two compilers are becoming "mature". What I found in this step is that it's important to target more than one platform. Yes, I'm still targetting QBE as the output language, but there were a few differences with the three CPU types that I needed to deal with.

To that end, and to give me something to do, I am considering porting *alic* to the PDP-11 platform. This is where the [C language started](https://www.nokia.com/bell-labs/about/dennis-m-ritchie/chist.pdf) and I think it would be fitting to bring *alic* up on this platform. I know this is going to take me just as much time as it's taken to get *alic* to its current state.

I'm going to make a Git branch for the PDP-11 work. Any changes that can be imported back to this `main` branch will appear here. I am going to have to radically restructure the compiler to get it to fit on the PDP-11, so it can't be included here in the `main` branch.

