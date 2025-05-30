# *alic* Part 12: Arrays, Finally

In this part of the *alic* journey, I'll be adding arrays at last. I've got a good idea of what this will entail, so there will be other issues to deal with like symbol visibility, initialisation lists etc. Let's start with my ideas for arrays in *alic*.

## Arrays &mdash; Slightly Different to C

I am trying to minimise the amount of undefined behaviour in *alic*. To this end, here are my ideas for arrays.

An array is declared with a set of initial values. Once declared, the array's size cannot change. This allows us to do [bounds checking](https://en.wikipedia.org/wiki/Bounds_checking) on the indexes into an array.

Yes, bounds checking is going to slow down the performance of programs that use arrays. Of course, a programmer is free to use pointers and `malloc()` to get array accesses with no bounds checking.

I also want to be able to have:

  * arrays of structs
  * structs with array members
  * structs with structs in them

This means that we should be able to do this in *alic*:

```
printf("fred.x.y[25].z is %f\n", fred.x.y[25].z);
```

## Initialising Arrays and the Syntax for Array Declarations

It makes sense to allow programmers to initialise the element values of arrays as well as the member values for structs. At the same time, I want to minimise the undefined behaviour in *alic*; to that end, if variables are not intialised, their contents need to be set to all-zero bits.

While I was working on this part of the *alic* journey, I thought I could just enforce that every array declaration must have an initialisation list, e.g.

```
int32 fred[] = { 1, 2, 3, 4, 5 };     // Explicitly 5 elements
```

Then I realised that this doesn't work, e.g. for external symbols:

```
extern int32 fred[] = ???             // Not allowed to initialise someone else's array!
```

I'd even made `[]` a token so I could parse it. That was quite a dead-end I had to undo. Sigh.

So, I've gone with the idea that the number of elements must be provided, but the array declaration can then be followed by an optional list of element values. The number of elements in the list must match the given number of array elements.

Here's some examples:

```
int32 fred[5];                   // Five zero-filled int32s
int32 mary[3] = { 1, 2, 7 };     // Three int32s
int16 dave[5] = { 11, 33 };      // Not valid, only two initialisation values
```

## Grammar Changes for Array and Struct Declarations

If we can declare arrays and structs with an initialisation list, then we need to change *alic*'s grammar to support this. Here we go:

```
global_var_declaration= visibility array_typed_declaration decl_initialisation SEMI
                      | visibility array_typed_declaration SEMI

decl_initialisation= ASSIGN expression
                   | ASSIGN bracketed_expression_list

array_typed_declaration= typed_declaration (array_size)?

array_size= LBRACKET NUMLIT RBRACKET

typed_declaration= type IDENT

visibility= ( PUBLIC | EXTERN )?
```

I'll cover `visibility` a bit further down. An `array_typed_declaration` is a typed declaration followed by an optional `[` ... `]` with the number of array elements inside the braces.

A global variable declaration doesn't have to have a declaration initialisation. If it does, it starts with an `=` followed by either a single expression or a *bracketed expression list*.

A bracketed expression list: this is interesting. We have to be able to support structs with array members, arrays of structs, and structs with struct members. This means that, if we surround values with `{` ... `}`, then we have to expect these to be nested. An example:

```
type FOO = struct {
  int32 x,
  bool  y
};

FOO fred[3]= {
  { 10, true  },
  { 20, false },
  { 30, true  }
};
```

So here are the grammar rules:

```
bracketed_expression_list= LBRACE bracketed_expression_element
                                  (COMMA bracketed_expression_element)*
                           RBRACE

bracketed_expression_element= expression
                            | bracketed_expression_list
```

A bracketed expression list starts and ends with `{` ... `}`. Inside, there is at least one bracketed expression element or, if more than one, the elements are separated by commas.

And a bracketed expression element is either a normal expression or a bracketed expression list. This last possibility allows us to nest lists inside lists.

## Why Is It So Complicated?!

I'm writing this after having implemented everything in this part of the *alic* journey. It's been the hardest step so far. Not only are there changes to nearly every part of the compiler, I spent days pulling my hair out to get things to work.

One reason is that we now have nested structured data: arrays of structs, structs with array/struct members. That really makes things complicated.

Another reason is the slight (but important) difference between using array access with a pointer versus a true array. Consider these two local variable declarations:

```
  int32 fred[5];                           // Five int32s in an array
  int32 *mary= malloc(5 * sizeof(int32));  // mary points at five int32s
```

`fred` consists of twenty (5 * 4) consecutive bytes starting at a location in memory. To access `fred[3]`, we start with the base address and add `3 * 4` to get the location of the element.

On the other hand, `mary` is **eight** consecutive bytes, because `mary` holds a *pointer* to twenty (5 * 4) consecutive bytes of memory. To access `mary[3]`, we get the value that `mary` holds, then add `3 * 4` to this value to get the location of the element: we *don't* start with `mary`s base address.

Thus, we have to track if we are dealing with a pointer or a true array when we do array element accesses.

## Symbol Visibility

I've added two new keywords to *alic*, `extern` and `public`. `extern` means the same as it does in C: a symbol is defined in another file. The keyword `public` indicates that a non-local symbol (e.g. a function or variable) be made visible to other files.

From now on, functions and non-local variables are **not** marked as visible to other files: they are, thus, *private* to the file being compiled.

I've made this change to make it easier for a programmer to prevent "leakage" of symbol names. If you want a function or variable to be visible, you now have to mark it as `public`.

We now have four symbol visibility values in [alic.h](alic.h):

```
// Symbol visibility
enum {
  SV_LOCAL=1, SV_PRIVATE, SV_PUBLIC, SV_EXTERN
};
```

I've left the word "global" in the compiler comments. This now means "not local", i.e. outside the scope of the current function, if any.

The grammar is changed to have this:

```
visibility= ( PUBLIC | EXTERN )?

global_var_declaration= visibility typed_declaration SEMI;

function_declaration= visibility function_prototype statement_block
                    | visibility function_prototype SEMI
```

Thus, the visibility keywords only apply to functions and global variables. You can use either `public` or `extern` or neither.

In terms of changes to the compiler, there is a scattering of small changes. For example, these functions now take a `visibility` argument:

```
int add_function(ASTnode *func, ASTnode *paramlist, int visibility);
void declare_function(ASTnode *f, int visibility);
Sym *add_symbol(char *name, int symtype, Type *type, int visibility);
```

In [parser.c](parser.c), the top-level `input_file()` function now does this:

```
// Loop parsing functions until we hit the EOF
  while (Thistoken.token != T_EOF) {
    switch(Thistoken.token) {
    ...
    case T_PUBLIC:
    case T_EXTERN:
    default:
      // This could be a function or variable declaration.
      // Get any optional visibility
      visibility= get_visibility();

      // Get the typed declaration
      decl= typed_declaration();

      // If the next token is an LPAREN,
      // it's a function declaration,
      // otherwise a global variable declaration
      if (Thistoken.token == T_LPAREN)
        function_declaration(decl, visibility);
      else
        global_var_declaration(decl, visibility);
    }
  }
```

The `get_visibility()` function is nice and simple:

```
static int get_visibility(void) {
  int visibility= SV_PRIVATE;   // Assume private to start
  switch(Thistoken.token) {
    case T_PUBLIC:
      visibility= SV_PUBLIC;
      scan(&Thistoken);
      break;
    case T_EXTERN:
      visibility= SV_EXTERN;
      scan(&Thistoken);
      break;
  }
  return(visibility);
}
```

And, in several functions in [cgen.c](cgen.c), we set the QBE prefix to be '%' for local symbols, otherwise '$':

```
  char qbeprefix = (sym->visibility == SV_LOCAL) ? '%' : '$';
```

## Implementing the Grammar: Getting the Array Size

In [parser.c](parser.c):

```
static int64_t array_size() { ... }
```

simply parses `[` ... `]` and returns the NUMLIT integer value that is inside the brackets.

```
static ASTnode *array_typed_declaration(void) { ... }
```

returns either a typed declaration or a typed declaration with a count if it was followed by an array size. I've modified the `ASTnode` struct in [alic.h](alic.h) to have a `count` member that holds this value. Because this `ASTnode` is *not* an expression, the `rvalue` member is unused. I'm using it as follows: if `true` this is an array declaration, `false` if not.

> Aside: I should refactor this because I can probably just use a zero/non-zero count to do the same thing.

## Changes to the Sym Structure

The `Sym` structure in [alic.h](alic.h) has changed significantly since Part 11. We used to have a single numeric `initval`: this is now a pointer to an `ASTnode` which allows us to build a (possibly nested) list of expressions for our initial values.

For a user-defined *alic* type which is a struct, we build a linked list of symbols which are the members of the struct. The new `offset` value in each symbol holds the member's offset from the start of the struct.

And the `count` value in the symbol, for variables, holds the number of elements when the variable is an array. In fact, I now have this helper function in [syms.c](syms.c):

```
// Return is a symbol is an array
bool is_array(Sym *sym) {
  return(sym->symtype == ST_VARIABLE && sym->count >0);
}
```

## Global Variable Declarations

`global_var_declaration()` in [parser.c](parser.c) is now much more complicated than before, as it has to deal with both the declaration of a variable and its initialisation.

We still check if the variable hasn't already been declared, then add it to the symbol table.

After that, if the `ASTnode` holding the declaration has a count (i.e. it's an array), we copy this count into the new symbol.

After this, if we see a `=` token, we must have an intial value or set of values. We call `decl_initialisation()` to get an `ASTnode` tree that holds this. Now it gets interesting. We have to walk this tree to a) output these values to the QBE file and b) check that we have the correct count of them and that their type matches the array/struct member they belong to.

Here are the essential bits of code:

```
  // If we have an '=', we have an initialisation
  if (Thistoken.token == T_ASSIGN) {
    init= decl_initialisation();

    if (sym->visibility == SV_EXTERN)
      fatal("cannot intiialise an external variable\n");

    // Start the output of the variable.
    cgglobsym(sym, false);

    // Check the initialisation (list) against the symbol.
    // Also output the values in the list
    check_bel(sym, init, 0, false);

    // End the output of the variable
    cgglobsymend(sym);
  }
```

Previously, we just called `cgglobsym()` to declare a global variable and initialise it. Now we call `cgglobsym()` to start the QBE declaration. There's a `cgglobsymval()` to output values inside the declaration, and we have `cgglobsymend()` to end the declaration.

## Representing a Bracketed Expression List

We now have a new type of `ASTnode`, `A_BEL`. It represents the situation when we saw a `{` in a variable's initialisation. Let's look at an example:

Consider a struct with a struct as a member:

```
type FOO = struct {
  int32 x,
  bool  y,
  flt32 z
};

type BAR = struct {
  bool  a,
  FOO   b,
  int16 c
};
```

We should be able to declare:

```
BAR tim= {
  true,
  { 12, true, 3.14 },
  -32768
};
```

We will have a bunch of `ASTnode`s holding expressions: below, I will only show you the values, not the trees that hold them. We use the middle child pointer to link things together. Each time we have a nested list, we insert an `A_BEL` node with its left pointer holding the start of the list. Thus, the above looks like:

```
   true -> A_BEL -> -32768
           /
         12 -> true -> 3.14
```

## `check_bel()`: Recursive and Tricky

`check_bel()` does the hard work of checking a list of initial values against the array or struct it belongs to, and outputting the values to QBE. It has to be recursive as we can have nesting.



This function gave me the most headaches and I suspect I still haven't got it completely correct  yet. I won't go through all the code, but just give you the comments:

```
// Given a symbol and a bracketed expression list,
// check that the list is suitable for the symbol.
// Also output the values to the assembly file.
// offset holds the byte offset of the value from the
// base of any struct/array. is_element is true when
// sym is an array and we are outputting elements
void check_bel(Sym *sym, ASTnode *list, int offset, bool is_element) {
  // No list, we ran out of values in the list
  // No symbol, too many values

  // The list doesn't start with an A_BEL, so
  // it is a scalar expression
  if (list->op != A_BEL) {
    // Error if the symbol is a struct, or an array
    // and this isn't an element of the array

    // Make sure the expression matches the symbol's type

    // It also has to be a literal value

    // Update the list element's type
    // Output the value at the offset
    cgglobsymval(list, offset);
    return;
  }

  // The list starts with an A_BEL. Skip it

  // We need the symbol to be a struct or array

  // The symbol is an array. Update the type.
  // Use the count of elements and walk the list
  if (<array>) {
    type= value_at(type);
    for (i= 0; i < sym->count; i++, list=list->mid) {
      check_bel(sym, list, offset + i * type->size, true);
    }
    return;
  }

  // If this is a struct
  if (sym->type->kind == TY_STRUCT) {
    // Walk the list of struct members and
    // check each against the list value
    for (memb= sym->type->memb; memb != NULL && list != NULL;
                                memb=memb->next, list=list->mid) {
       check_bel(memb, list, offset + memb->offset, false);
    }

    return;
  }
}
```

Note that `check_bel()` calls itself each time we hit an array or struct. The only time it doesn't is when it has a scalar initial value to deal with: this is base case for the recursion.

Also note that we calculate and pass in the `offset` of the array element/struct member each time we call `check_bel()` recursively.

I've chosen to make the type of an array be a "pointer to" the values in the array. This allows me to do:

```
int32 fred[5];
int32 *mary= fred;
```

This is why we do `type= value_at(type);` when we are processing the values in an array initialisation.

`cgglobsymval()` has the job of outputting the initial value to the QBE output file. It deals with the proper alignment of the value: consider an `int64` coming directly after an `int8`. It's annoyingly tricky but I won't go into the details.

## Optimising AST Expressions

There are some issues to deal with initialising variables. Consider this code:

```
int32 x= 5;
int32 y= 3 * 10;
int32 z= 2 * foo();

void fred(void) {
  int32 a= x + y;
  int32 b= a * 7;
}
```

The variables `x`, `y` and `z` are declared outside of a function, so we are not executing any code when we declare them. On the other hand, variables `a` and `b` are declared inside a function, so we can run code when declaring them.

Why is this important? We have the expression `3 * 10` for the `y` initialisation, but we are not inside a function and so we can't get the CPU to perform the multiplication. Similarly, we can't run any code to do the `2 * foo()` expression.

For `a` and `b` we are lucky: we are inside the `fred()` function and we can definitely run code to initialise them.

But wait! We can do something about the `y` initialisation; we can do [constant folding](https://en.wikipedia.org/wiki/Constant_folding). The expression consists of two numeric literals, so the compiler can detect this and convert the expression to the numeric literal value 30.

I've borrowed the AST optimisation code from *acwj* which does constant folding but only for integer literals. The code is now in [astnodes.c](astnodes.c). The `fold2()` function takes an ASTnode with two integer literal children and optimises them when the operation is an add, subtract, multiply or divide. The `fold1()` function deals with unary operations invert and not on a single integer literal children. And the `fold()` function walks an expression tree looking for places to apply `fold2()` and `fold1()`.

There is an `optAST()` function which just calls `fold()`. I've done this in case I want to add other AST optimisations at a later point in time.

And, in `expression()` in [parser.c](parser.c):

```
// Try to optimise the AST tree that holds an expression
//
//- expression= bitwise_expression
//-
static ASTnode *expression(void) {
  return(optAST(bitwise_expression()));
}
```

Here's an example of the expression optimisation at work. The input is test 1:

```
void main(void) {
  printf("%d\n", 32 + 2 * 3 - 5);
  printf("%d\n", 10 + 5);
  printf("%d\n", 23 - 5 * 6 + 9);
}
```

and here is the new QBE output:

```
  %.t3 =w copy 33
  call $printf(l %.t2, w %.t3)
  ...
  %.t5 =w copy 15
  call $printf(l %.t4, w %.t5)
  ...
  %.t7 =w copy 2
  call $printf(l %.t6, w %.t7)
```

## Parsing Postfix Expressions

When using structs and arrays, the grammar hasn't changed:

```
postfix_variable= IDENT
                | postfix_variable DOT IDENT
                | postfix_variable LBRACKET expression RBRACKET
```

What has changed is that I now have to deal with arrays of structs, structs with array members, and structs with struct members. And that essentially means that the code to do this now has to be recursive.

```
// Recursively parse a variable with postfix elements
// (in parser.c)
static ASTnode *postfix_variable(ASTnode *n) {
  // Deal with whatever token we currently have
  switch(Thistoken.token) {
  case T_IDENT:
    n= mkastleaf(A_IDENT, NULL, false, NULL, 0);
    n->strlit= Thistoken.tokstr;
    n= mkident(n);              // Check variable exists, get its type
    return(postfix_variable(n));

  case T_LBRACKET:
    e= <new ASTnode combining n and the array offset>;
    return(postfix_variable(e));

  case T_DOT:
    n= <new ASTnode combining n and the member's offset>;
    return(postfix_variable(n));

  default:
    // Nothing to do
    return(n);
  }
}
```

This function caused me to pull my hair out over several days before I finally sorted it out. I've put a bunch of comments in the code, but I'm still traumatised so I'm not going to try to explain it (yet!).

## Array Bounds Checking

Right at the start of this journey, I said I was going to add bounds checking to arrays. I implemented this in `postfix_variable()`. We have a new `ASTnode` operation, `A_BOUNDS`. This node comes in between the expression holding the index value and its actual use. As an example:

```
int32 fred[5];

void main(void) {
  fred[3] = 12;
}
```

Without bounds checking, the AST tree for the `main()` function looks like:

```
int32 ASSIGN
  int32 NUMLIT 12 rval
  int32 DEREF
    int32 * A_ADDOFFSET  rval
      unsigned int64 MULTIPLY  rval
        unsigned int64 NUMLIT 3 rval
        unsigned int64 NUMLIT 4 rval
      int32 * IDENT fred rval
```

i.e. take `fred`'s base address, multiply `3*4`, add it to `fred`'s address, dereference this and use it to store the number 12.

With bounds checking:

```
int32 ASSIGN 
  int32 NUMLIT 12 rval
  int32 DEREF 
    int32 * A_ADDOFFSET  rval
      int32 * CAST  rval
        int64 MULTIPLY  rval
          int64 BOUNDS  count 5
            int64 NUMLIT 3 rval
          int64 NUMLIT 4 rval
      int32 * IDENT fred rval
```

This time we take the index value 3, and do a BOUNDS check against 5 (the number of elements in `fred`) *before* we multiply it by 4.

The bounds checking is done by `cgboundscheck()` in [cgen.c](cgen.c):

```
// Do a bounds check on t1's value. If below zero
// or >= count, call a function that will exit()
// the program. Otherwise return t1's value.
int cgboundscheck(int t1, int count, int aryname, int funcname) {
  int counttemp= cgalloctemp();
  int comparetemp= cgalloctemp();
  int zerotemp= cgalloctemp();
  int Lgood= genlabel();
  int Lfail= genlabel();

  // Get the count into a temporary
  ...
  // Compare against the index value, less than
  // Jump if false to the failure label
  ...
  // Get zero into a temporary
  ...
  // Compare against the index value, greater than or equal to
  // Jump if false to the failure label
  // Otherwise jump to the good label
  ...
  // Call the failure function
  cglabel(Lfail);
  ...
  cglabel(Lgood);
  return(t1);
}
```

Each time we start processing a new *alic* source file, we start its QBE output with a non-public function called `.boundserr()`. You will see it as QBE code in `cg_file_preamble()`. I actually wrote it in *alic* and used the compiler to produce the QBE code which I then copied into the preamble function! Originally, the function looks like:

```
void boundserr(int8 *aryname, int64 value, int8 *funcname) {
  fprintf(stderr, "%s[%d] out of bounds in %s()\n", aryname, value, funcname);
  exit(1);
}
```

As an example, test 117 looks like:

```
void main(void) {
  int32 fred[5];
  int32 i;
  ...
  for (i=0; i < 10; i++)
    printf("%d\n", fred[i]);
}
```

When we compile and run it, we get: `fred[5] out of bounds in main()`


## Conclusion and The Next Step

I'm sure I still haven't covered everything that I had to do to get arrays and initialisation done in this part of the *alic* journey. We are now at 6,000 lines of code in the compiler: up from 5,300 lines in the previous part.

Apart from adding the `?:` [ternary operator](https://en.wikipedia.org/wiki/Ternary_conditional_operator), the *alic* language is now "complete" enough to allow me to start translating the compiler's C code into *alic*. In other words, I should be able to write an *alic* compiler in *alic* itself!

It's a bit of a chicken and egg situation. I need to keep the C version of the compiler so I can compile the *alic* version. But once I have the *alic* version of the compiler, I should be able to compile the *alic* version of the compiler with itself.

Anyway, I think that's where I will be heading in the future parts of the *alic* journey.

