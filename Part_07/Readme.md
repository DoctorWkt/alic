# *alic* Part 7: A Start on Pointers, A C Pre-Processor and Semantic Errors

In part seven of my *alic* journey, I've made a start on implementing pointers.
My main reason for adding them is that I want to be able to use the C library and not have to write my own *alic* library. And for that, we need pointers!

I've also added the ability to run the input source file through the C pre-processor. And I've also added more semantic error checks to the compiler.

## NULL is Built Into *alic*

I'm seriously trying *not* to add type casts in to *alic*. So, if we are going to have pointers, how do we write `NULL` when I don't want to write `(void *)0`?

At first, I thought I would add code to the compiler to allow a comparison between any pointer type value and the integer literal zero. That ended up being complicated.

Instead, I've updated the `Type` structure:

```
struct Type {
  TypeKind kind;
  int size;             // sizeof() value
  bool is_unsigned;     // unsigned or signed
  int ptr_depth;        // Number of derefs to base type (NEW)
  Type *next;
};
```

Then, I added two new `Type` variables to the compiler:

```
Type *ty_voidptr = &(Type) { TY_VOID, 8, false, 1 };    // Used by NULL
Type *ty_int8ptr = &(Type) { TY_INT8, 8, false, 1 };    // Used by strlits

// Global variables
Type *Typehead;                        // Head of the type list
```

and in `main()`:

```
  Typehead= ty_voidptr;                 // Start the list of types
  Typehead->next= ty_int8ptr;
```

Thus, we now have two pointer types, one that means `void *` and the other that means `int8 *`.

In the lexical tokeniser, "NULL" now gets recognised as the token `T_NULL`. In the parser, the `factor` rule has been renamed and modified:

```
primary_expression= NUMLIT
                   | STRLIT
                   | TRUE
                   | FALSE
                   | NULL
                   | variable
                   | function_call

static ASTnode *primary_expression(void) {
  ASTnode *f;
  ...
  case T_NULL:
    f= mkastleaf(A_NUMLIT, ty_voidptr, true, NULL, 0);
    scan(&Thistoken);
    break;
  ...
}
```

In other words, when we see a "NULL" keyword we create a numeric literal AST node of type `ty_voidptr` and with the value zero. And that's the same as writing `(void *)0` in C!

Over in `widen_type()` in [types.c](types.c),

```
ASTnode *widen_type(ASTnode *node, Type *ty) {
  ASTnode *newnode;

  // They have the same type, nothing to do
  if (node->type == ty) return(node);
  ...
  // If the type is a pointer, we
  // can only widen from a voidptr.
  // Update the node's type
  if (is_pointer(ty)) {
    if (node->type == ty_voidptr) {
      node->type= ty;
      return(node);
    }
    return(NULL);
  }
  ...
}
```

So, two pointers of the same type are compatible, but only a `ty_voidptr` can be "widened" to be any other pointer type. Tests 39, 40 and 42 check that `NULL` works properly.

## String Literals Have A Type

Given that we now have pointer types in *alic*, I thought it was time that we stopped treating string literals as special and give them a type. I already mentioned the `ty_int8ptr` type above.

In `primary_expression()` in the parser:

```
static ASTnode *primary_expression(void) {
  ASTnode *f;
  ...
  case T_STRLIT:
    // Build an ASTnode with the string literal and ty_int8ptr type
    f= mkastleaf(A_STRLIT, ty_int8ptr, false, NULL, 0);
    f->strlit= Thistoken.tokstr;
    scan(&Thistoken);
    break;
```

This means that string literals are now expressions of type `int8 *`. That means that we now have to generate code for them when we see them in the AST tree for a statement block.

Over in `genAST()` in [genast.c](genast.c):

```
int genAST(ASTnode * n) {
  ...
  // General processing
  switch (n->op) {
  ...
  case A_STRLIT:
    label= add_strlit(n->strlit);
    return(cgloadglobstr(label));
  ...
  }
}
```

We add the string literal to the list of known string literals and then call a new function in [cgen.c](cgen.c), `cgloadglobstr()`, to load the label associated with the literal into a temporary.

## Pointer Type Declarations

At this point we have `NULL` with an implicit type and string literals with an implicit type, but we can't declare any pointer variables. We want to be able to do:

```
int8 *fred = NULL;
```

So the rule for types in the grammar now looks like this:

```
type= (builtin_type | user_defined_type) STAR*

builtin_type= 'void'  | 'bool'
            | 'int8'  | 'int16'  | 'int32'  | 'int64'
            | 'uint8' | 'uint16' | 'uint32' | 'uint64'
            | 'flt32' | 'flt64'

user_defined_type= IDENT
```

i.e. any built-in or user-defined type can be followed by zero or more '`*`' characters. The `type()` function in [parser.c](parser.c) now counts the number of trailing '`*`' characters. If there isn't any, it simply returns the type found.

If the count of '`*`'s is one or more, it calls a new function called `find_type()` in [types.c](types.c). This walks the linked list from `Typehead` looking for a type that has the correct base type and pointer depth (number of '*'s). It either finds one, or makes a new type to match, then returns this.

Given that we already have a `ty_int8ptr` type linked in to the `Typehead` list, parsing `int8 *fred` will find it. But if we declare `int32 **jim`, this will make a new pointer type and add it to the list.

## Pointer Operations

So where are we now? We can declare variables (and functions) of pointer type, and we have `NULL` and string literals with appropriate pointer types. Now we need some pointer operations!

As *alic* is influenced by C, I'm going to stick with the '*' (value at) and '&' (address of) operators. I don't want to design an object-oriented language, but it would be nice (later on) to have abstract [references](https://en.wikipedia.org/wiki/Reference_(computer_science)) instead of pointers. For now, I'll add pointer operations even if, later on, I take them out.

Assuming that we have a pointer variable, the '*' operator gets the value that it points at. It's going to be of the same base type as the pointer but with one less pointer depth. And, if we have a variable (which must have an address), the '&' gets the address of that variable. It's going to be of the same base type as the pointer but with one greater pointer depth.

To aid in the type changes, [types.c](types.c) now has two helper functions:

```
// Given a type pointer, return a type that
// represents a pointer to the argument
Type *pointer_to(Type *ty) { ... }

// Given a type pointer, return a type that
// represents the type that the argument points at
Type *value_at(Type *ty) { ... }
```

Over in the parser, the grammar now has unary operators:

```
multiplicative_expression= unary_expression
                         ( STAR  unary_expression
                         | SLASH unary_expression
                         )*

unary_expression= primary_expression
                | STAR unary_expression
                | AMPER primary_expression

primary_expression= NUMLIT ...
```

> Aside: I like to refer to this [ANSI C grammar](https://www.lysator.liu.se/c/ANSI-C-grammar-y.html) to help me when I'm designing new parts of the *alic* language.

Note that we can have zero or more '*'s at the start of a unary expression as the rule is recursive. But we can only have a single '&' at the start of a unary expression, as the next expression must be a primary expression.

I'm not going to go through the code in `unary_expression()`. There are some semantic checks: an '&' must precede an identifier, and a '*' must precede an expression of pointer type.

The '*' operator causes an `A_DEREF` AST node to be added to the tree; the '&' operator causes an `A_ADDR` AST node to be added to the tree. The code in `genAST()` deals with these by calling these QBE code generating functions:

```
// Generate code to load the address of an
// identifier. Return a new temporary
int cgaddress(Sym *sym) { ... }

// Dereference a pointer to get the value
// it points at into a new temporary
int cgderef(int t, Type *ty) { ... }
```

Nearly all the code here comes from my previous *acwj* compiler. My test to check that they work is test 42:

```
void main(void) {
  int32 fred= 5;
  int32 mary= 0;
  int32 *jim= NULL;

  jim= &fred;
  mary= *jim;
  print("fred is %d\n", fred);
  print("mary is %d\n", mary);
}
```

`jim` is pointed to `fred` which has value 5. Then `mary` gets the value that `jim` points at. Hence, both `print` statements print out the number 5.


## Variadic Functions

We've had a `print` statement built into the *alic* language since the first part of this journey. I've been able to phase it out and replace it with `printf()` from the standard I/O library. To get there, we neeedd to be able to declare a function as [variadic](https://en.wikipedia.org/wiki/Variadic_function).

I've decided to keep the C "..." syntax, but I have set the grammar up to only allow this with no other function parameters:

```
function_prototype= typed_declaration LPAREN typed_declaration_list RPAREN
                  | typed_declaration LPAREN VOID RPAREN
                  | typed_declaration LPAREN ELLIPSIS RPAREN
```

We can declare `void printf(...);` as a function prototype, but we can't declare `void printf(int8 *fmt, ...);` as a prototype. I'm happy with this for now.

The lexical tokeniser recognises "..." as the token T_ELLIPSIS. In the parser code, when we see this, we have to report that the function is varidiac. But `function_prototype()` returns an AST node (a hang over from the *leg* days). What to do? Here's what I chose:

```
  // If the next token is an ELLIPSIS, skip it
  // and mark the function as variadic
  // by using the rvalue field
  if (Thistoken.token == T_ELLIPSIS) {
    scan(&Thistoken);
    func->rvalue= true;
  }
```

It's ugly but it works! Now, in the symbol table, I'm doing this:

```
typedef struct _sym {
  char *name;           // Symbol's name.
  int  symtype;         // Is this a variable, function etc.
  int  visibility;      // The symbol's visibility
  ...
  int count;            // Number of struct members or function parameters
                        // For a function, -1 means it is variadic
  ...
  struct _sym *next;    // Pointer to the next symbol
} Sym;

// Symbol visibility
enum {
  SV_LOCAL=1, SV_GLOBAL
};
```

I could have added yet another field, `bool is_variadic`. But we have a parameter count, and so `-1` now means variadic. Also note the new `visibility` field; QBE use '$' as a prefix for global symbols and '%' for non-global symbols. When we are loading an address in `cgaddress()`, we need to know if a symbol is global or not. Hence the `visibilty` field.

OK, we can parse the ellipsis "..." and mark in the symbol table that a function is variadic. Great! Now, what do we do with this knowledge?

Well, in `gen_funccall()` in [genast.c](genast.c), it has the job of checking both the count of arguments vs. the count of function parameters and also widening arguments to match the function's parameters.

I've added code in here that does these things:

  * If the function is variadic, don't cross-check the arg count vs. the param count
  * Don't try to widen any expressions if the function is variadic.

With this in place, test 43 now works:

```
void printf(...);

void main(void) {
  printf("Trying this out %d %d %d\n", 23, 45, 666);
}
```

## A `printf()` Workaround

I had hope that this would allow me to dispose of the built-in `print` statement completely. When I tried, several of the existing tests that print floats out failed. It turns out that the `printf()` on my system requires all 32-bit float arguments to be widened to 64-bit doubles.

We used to do this manually in the `print_stmt()` code. I've had to add an ugly workaround in the `gen_funccall()` function: if the function's name is `printf` then widen `flt32`s to `flt64`s.

This allows me to completely remove everything we need for the `print` statement: tokens, AST node operations, several functions etc.

## Input from the C Pre-Processor

In part six of the *alic* journey, I said that I needed the ability to have header files and to pre-process *alic* source files with the C pre-processor. To that end, I've imported the code from the `main.c` file in my *acwj* project here. I'm not going to comment on the new [main.c](main.c) code; you can read through it yourself. It got tested quite thoroughly in *acwj* so I think it should be fine here.

## More Semantic Error Checks

I've added a bunch more semantic error checks to the parser code. In particular, we should not be able to do bitwise, shift or numeric operations on expression of `bool` type. None of these make sense:

```
bool a = true * 5;
bool b = a << 23;
bool c = a * b;
bool d = 2 * a - 3 * c;
```

We can still assign a `bool` expression to a numeric variable, in which case `true` gets turned into 1 and `false` into 0. But the only useful `bool` operations should be comparisons.

There is a new helper function in [misc.c](misc.c) called `cant_do()` which prints out a fatal error if the given expression has the given type.

## Conclusion and The Next Step

I've finally been able to get rid of the built-in `print` statement as we now have variadic functions and header files. We also have pointers, pointer types and pointer operations. `NULL` is built into the *alic* language and string literals now have a type.

That should be enough to make a start on user-defined types. Next up, I want to be able to create *opaque* types and give existing types a new name, e.g.

```
type FILE;            // The type exists, we can only have a pointer to it
type String= int *;   // String is the same type as int *
```