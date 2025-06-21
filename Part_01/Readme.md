# *alic* Part 1: Built-in Types and Simple Expressions

In part one of my *alic* journey, I want to have a language with some
built-in types, some essential operations on values of these types, simple
variables and some statements to declare, assign to and print variables
(and print expressions).

## The Tokeniser and Parser

The C language has a well-defined syntax; so, for the [acwj](https://github.com/DoctorWkt/acwj) compiler, I wrote my own recursive descent parser. With *alic* the language will evolve over time, so I've decided to start with a parser generator instead. It will allow me to more easily modify the *alic* grammar as I go.

I've chosen to use the [peg](https://github.com/gpakosz/peg) recursive descent parser generator for this journey instead of tools like *yacc* and *antlr*. (Actually, I'm going to use the *leg* extended version of *peg*). The *peg* author, Ian Piumarta, describes these advantages of *peg*:

> Unlike *lex* and *yacc*, *peg* and *leg* support unlimited backtracking, provide ordered choice as a means for disambiguation, and can combine scanning (lexical analysis) and parsing (syntactic analysis) into a single activity.

So far I've found *leg* to be nice if not a bit quirky. I'll talk about it as we go through the *alic* grammar below.

> **NOTE: In part five of the journey I decide to stop using *leg* and write my own recursive descent parser. So, you will only need to install *leg* for the first four parts if you actually want to compile my code.**

## Part 1: Types

In this first part of the journey I'm going to focus on the built-in types for *alic*. They are:

  * signed integers `int8`, `int16`, `int32` and `int64`,
  * unsigned integers `uint8`, `uint16`, `uint32` and `uint64`,
  * floating point types `flt32` and `flt64`,
  * a boolean type, `bool`, and
  * a `void` type which we won't use at present.

In terms of implicit widening:

 * smaller signed integers can be widened to larger signed integers,
 * smaller unsigned integers can be widened to larger unsigned integers,
 * both signed and unsigned integers can be widened to either floating point type, and
 * `bool` can be widened to any integer or floating point type: `false` is 0 and `true` is 1.

Note that I want to keep `bool` separate from the integer types; thus, 0 does not mean `false`, nor does 1 mean `true`.

## Statements

For this first part of the *alic* journey, there are only three statements. Here are the relevant grammar rules from the [parse.leg](parse.leg) file with the implementation code removed:

```
statements = statement+ EOF

statement = print_stmt
          | declaration_stmt
          | assign_stmt
          | .

print_stmt = PRINT expression SEMI

declaration_stmt = type SYMBOL ASSIGN expression SEMI

assign_stmt = variable ASSIGN expression SEMI
```

As you can see, *leg* uses a very BNF-like syntax to describe the language. One thing that I like is that it will try alternatives in order. For example, it will look at a token to see if a `statement` is a `print_stmt` first. If that fails, it sees if the token matches a `declaration_stmt`. If that fails, it tries an `assign_stmt`. And if all else fails, it will match on a single character.

Why match on a single character? Because I can then print out a fatal "syntax error" message when the final rule is matched.

Let's rewrite the above grammar in English. An input file consists of `statements`, which are one or more `statement`s followed by the end of file. A `statement` is either a `print_stmt`, a `declaration_stmt`, an `assign_stmt` or a syntax error.

A `print_stmt` is the token PRINT followed by an expression and a semi-colon, e.g. `print x + 2;` . A `declaration_stmt` is a named SYMBOL preceded by a type, then an ASSIGN token, an expression and a semi-colon, e.g. `int32 x = 3;` . And an `assign_stmt` is a named variable, then an ASSIGN token, an expression and a semi-colon, e.g. `x = x + 45;`.

## *leg* and Tokens

*leg* acts as both a parser generator and also a token generator. Here are some more rules from the [parse.leg](parse.leg) file:

```
EOF = !.
CR  = ( '\n' | '\r' | '\r\n' )
-   = ( CR   | ' '  | '\t'   )*
```

The EOF (end of file) token is "not a character": when there is no character left to read, we are at EOF. A CR token is one of the ASCII characters LF, CR or the CRLF combination.

The `-` token means whitespace; it is zero or more of a CR token, a space or a tab character. The [peg manual](https://www.piumarta.com/software/peg/peg.1.html) suggests this and I've followed it as it reads much more easily than a token name like WHITESPACE.

Now, some more token rules:

```
# Operators
#
SLASH  = '/' -
STAR   = '*' -
MINUS  = '-' -
PLUS   = '+' -
SEMI   = ';' -
ASSIGN = '=' -
GT     = '>'  -
GE     = '>=' -
LT     = '<'  -
LE     = '<=' -
EQ     = '==' -
NE     = '!=' -
INVERT = '~'  -
NOT    = '!'  -
AND    = '&'  -
OR     = '|'  -
XOR    = '^'  -
LSHIFT = '<<' -
RSHIFT = '>>' -

# Keywords
#
PRINT  = "print" -
TRUE   = 'true'  -
FALSE  = 'false' -

type =    "int8"   -
        | "int16"  -
        | "int32"  -
        | "int64"  -
        | "uint8"  -
        | "uint16" -
        | "uint32" -
        | "uint64" -
        | "flt32"  -
        | "flt64"  -
        | "bool"   -
        | SYMBOL   -
```

Note that these rules essentially recognise a keyword or operator followed by any amount of whitespace. I've chosen to not make `type` uppercase because it has some **semantic** properties: later on I will search through a list of user-defined type names.

Finally, here are the rest of the token rules:

```
# Variables, Symbols, Literals
#
variable = SYMBOL

SYMBOL = [A-Za-z_]+ [A-Za-z0-9_]* -

FLTLIT = '-'? DIGIT+ '.' DIGIT+ -
INTLIT = '-'? DIGIT+ -
DIGIT = [0-9]
```

A DIGIT is a character in the list `'0'` ... `'9'`. An INTLIT may start with a minus sign followed by any number of DIGITs then whitespace. A FLTLIT is similar but it must contain a period character. A SYMBOL can only start with an alphabetic character or an underscore, but it can then be followed by any number of alphabetic or numeric characters or underscores.

I've made the `variable` rule match a single SYMBOL. Why not just use the word SYMBOL? Because later a variable might be an array element, e.g. `list[5]` or a struct member, e.g. `user.name`.

## Implementing Types

Here is the code from [alic.h](alic.h) that defines the Type structure:

```
// Built-in type ids.
typedef enum {
  TY_VOID,  TY_BOOL,  TY_INT8,  TY_INT16,
  TY_INT32, TY_INT64, TY_FLT32, TY_FLT64
} TypeKind;

// Type structure. Built-ins are kept as
// separate variables. We keep a linked
// list of user-defined types
typedef struct Type Type;

struct Type {
  TypeKind kind;
  int size;           // sizeof() value
  int align;          // alignment
  bool is_unsigned;   // unsigned or signed
  Type *next;
};
```

I'm not using the linked list as yet, and I don't think I'm using the `align`
or `size` members either. They will get used in future parts of the journey.

For the built-in types we create a bunch of individual variables to hold
their details. This is at the top of [types.h](types.h):

```
Type *ty_void = &(Type) { TY_VOID, 1, 1 };
Type *ty_bool = &(Type) { TY_BOOL, 1, 1 };

Type *ty_int8 = &(Type)  { TY_INT8,  1, 1 };
Type *ty_int16 = &(Type) { TY_INT16, 2, 2 };
Type *ty_int32 = &(Type) { TY_INT32, 4, 4 };
Type *ty_int64 = &(Type) { TY_INT64, 8, 8 };

Type *ty_uint8 = &(Type)  { TY_INT8,  1, 1, true };
Type *ty_uint16 = &(Type) { TY_INT16, 2, 2, true };
Type *ty_uint32 = &(Type) { TY_INT32, 4, 4, true };
Type *ty_uint64 = &(Type) { TY_INT64, 8, 8, true };

Type *ty_flt32 = &(Type) { TY_FLT32, 4, 4 };
Type *ty_flt64 = &(Type) { TY_FLT64, 8, 8 };
```

## Expressions

The purpose of this first part of the *alic* journey is to sort out the built-in types, and so we need to deal with expressions involving literals and variables of these types. We need to work out the order of precedence for the available operators. We also need to think about the implicit types for literal values.

When I wrote the [acwj](https://github.com/DoctorWkt/acwj) compiler, I used a [Pratt parser](https://journal.stuffwithstuff.com/2011/03/19/pratt-parsers-expression-parsing-made-easy/) to deal with operator precedence. This time I'm using the *leg* parser generator, so I've decided just to write the grammar rules to prescribe the operator order precedence.

As you look through the *leg* grammar rules for *alic*'s expressions, you might also compare them to the [C grammar rules in Yacc](https://www.lysator.liu.se/c/ANSI-C-grammar-y.html).

```
# Expressions
#
expression = bitwise_expression

bitwise_expression =
	( INVERT relational_expression
	| relational_expression
	)
	( AND relational_expression
	| OR  relational_expression
	| XOR relational_expression
	)* -

relational_expression =
	( NOT shift_expression
	| shift_expression
	)
	( GE shift_expression
	| GT shift_expression
	| LE shift_expression
	| LT shift_expression
	| EQ shift_expression
	| NE shift_expression
	)* -

shift_expression = additive_expression
	( LSHIFT additive_expression
	| RSHIFT additive_expression
	)* -

additive_expression =
	( PLUS? multiplicative_expression
    | MINUS multiplicative_expression
	)
    ( PLUS  multiplicative_expression
    | MINUS multiplicative_expression
    )* -

multiplicative_expression = factor
	( STAR  factor
	| SLASH factor
	)* -

factor =  FLTLIT
	| INTLIT
	| TRUE
	| FALSE
	| variable
```

The grammar order is essentially from lowest to highest precedence. Let's start at the bottom. A `factor` is either an int or float literal, `true`, `false` or a variable name.

A `multiplicative_expression` is at least a factor, or a factor followed by zero or more binary `*` or `/` operators and another factor. This parses `2`, `2 * a`, `2 * a / b` etc.

The rule for an `additive_expression` is more interesting. Note the `PLUS?` which matches zero or more unary `+` signs. The first half parses stuff like `3`, `+3` and `-3`. The second half matches zero or more binary `+` or `-` operators.

Similarly, the `relational_expression` rule parses a unary `!` operator as well as the six binary comparison operators. And the `bitwise_expression` rule matches the unary `~` operator and three binary bitwise operators.

Thus, we have this operator precedence list from highest to lowest:

  * `*` and `/`
  * `+` and `-`, both binary and unary
  * `<<` and `>>`
  * `!` (unary), `>=`, `>`, `<=`, `<`, `==` and `!=` (binary)
  * `~` (unary), `&`, `|` and `^` (binary)

## Building the AST Tree

To the right of most of the above expression rules are C fragments that build the AST tree ([abstract syntax tree](https://medium.com/basecs/leveling-up-ones-parsing-game-with-asts-d7a6fc2400ff)) for an overall expression. Have a look at [parse.leg](parse.leg) for the details. Here are some examples:

```
    INVERT l:relational_expression        { l  = unarop(l, A_INVERT); }
    ...
    AND r:relational_expression           { l  = binop(l,r,A_AND); }
    ...
    TRUE                                  { $$ = mkastleaf(A_NUMLIT,
                                            ty_bool, true, NULL, 1); }
```

The first builds an AST node with the A_INVERT unary operation and `l` as the left child. The second builds an AST node with the A_AND binary operation and children `l` and `r`. The TRUE code builds an A_NUMLIT AST leaf node with the literal value 1.

Also note that most rules end with the code `{ $$ = l; }`. This passes the subtree of the expression at this point up to a higher level. For example, if we were parsing `2 + 3 * 4`,
we would follow the grammar down and parse `3 * 4` and return the sub-tree

```
    A_MUL
    /   \
   3     4
```

This would go up and be added to another sub-tree:

```
    A_ADD
    /    \
   2    A_MUL
        /   \
       3     4
```

The functions `binop()` and `unarop()` are in [expr.c](expr.c). They call `mkastnode()` from [astnodes.c](astnodes.c) to build and link the AST nodes into a tree.

## Dealing with Numeric Literals

Before we look at the code that implements the type semantics for *alic*, let's first cover an interesting issue: numeric literals. These cause us issues as their type is fluid: is 3 of type `int8` or `int32`, or even `uint8`?

If we assume `int8`, then we can't do:

```
uint16 fred = 3;
```

because `fred` is unsigned and 3 is signed, and the compiler thinks we might lose a negative value when assigning to an unsigned variable.

For this reason, there is some literal-specific code in our implementation.

## Implementing Type Semantics

Going back to the `binop()` function in [expr.c](expr.c), you will see a call to the `add_type()` function immediately after an AST node is created with no type. The `add_type()` function in [types.c](types.c) looks at an AST node's children and determines if there is a suitable type that works for both of them, e.g.  `int8` and `int32` children would be covered by the `int32` type.

I won't go through the `add_type()` code in detail. Here are some relevant comments and actions:

  * Do nothing if the node already has a type.
  * If the node's operation is relational, set the node's type to `bool`.
  * If the children have no type, run `add_type()` on them too.
  * Try to widen each child node's type to match the other child node's type, using `widen_type()`.
  * Once this is done, set the current node's type to match.

Let's now look at `widen_type()`, also in [types.c](types.c). It's mission is:

```
// Given an ASTnode and a type, try to widen
// the node's type to match the given type.
// Return the same ASTnode if no widening is needed,
// or an ASTnode which widens the first one,
// or NULL if the types are not compatible.
```

Again, here are the relevant comments and actions:

  * If the node already has the type, return the node.
  * Return NULL if the type is `bool`, as we can't widen to a `bool`.
  * If the node is of `void` type, it's a fatal error; we can't do that!
  * We can change an integer type of any size to be a floating point type.
  * If the new type size is smaller than the node's type size, return the node. That would be a narrowing :-)
  * For numeric literals, we have some special handling. Negative values cannot be converted to an unsigned type. Otherwise we can simply change the literal's type and avoid a CAST operation.
  * If the node and type have different signedness, then return NULL as they are incompatible.
  * If we get here, then we can do the widening. We use an A_CAST AST node for this operation.

Yes, the code is a bit fiddly; it's about 40 lines (with comments) to get the type semantics the way we want them.

## Other Type Functions

That's most of the type semantics for *alic* dealt with. There are a few other miscellaneous functions in [types.c](types.c):

  * `is_integer()`, `is_flonum()` and `is_numeric()` test for integer, floating point and numeric types.
  * `parse_litval()` converts a numeric literal into the smallest integer or floating point type that it fits into.

## The Rest of the Compiler

The other files and functions in the current compiler essentially follow the form that I used for [acwj](https://github.com/DoctorWkt/acwj).

[astnodes.c](astnodes.c) has functions that build and link AST nodes, and a function to print out an AST tree.

[syms.c](syms.c) builds a linked list of known symbols and their types, and can find a named symbol and return a pointer to its node. This will get heavily modified later on when I add user-defined types to the language.

[genast.c](genast.c) has a single function `genAST()` which walks an AST tree and calls the code generator to output intermediate code to perform the node's action. Again, this will get modified later on. Note that, for now, `genAST()` is run each time a statement gets parsed: I don't build a single AST tree for the whole input file.

As with [acwj](https://github.com/DoctorWkt/acwj) I'm targetting [QBE](https://c9x.me/compile/) as my intermediate code. This means that I don't have to deal with register allocation nor code optimisation; QBE does it for free.

The functions in [cgen.c](cgen.c) essentially follow the form of those in *acwj*. This time, however, I have unsigned and floating point types. QBE doesn't treat bytes and 16-bit halfwords as first class types; there are some annoying issues when using these and converting to/from them.

You'll see at the top of [cgen.c](cgen.c) there are a bunch of arrays which hold the QBE type suffix depending on:

  * are we naming the type of the destination,
  * are we storing into a memory location, or loading into a temporary,
  * are we widening one type to be a different type.

For now, when we compile an input file, we output QBE code for a `printint()` function, a `printdbl()` function and the start of the `main()` function.

There are functions for all the unary and binary operations, for widening types, for defining a variable, storing to it and loading from it. Obviously, this file and [genast.c](genast.c) are both going to grow as the language develops.

## Building and Testing the Compiler

I'm developing the compiler on a Devuan Linux box, but if you have a Linux box with a C compiler then you should be fine. You will need to download, compile and install:

  * [peg](https://github.com/gpakosz/peg) (for the first four parts) and
  * [QBE](https://c9x.me/compile/)

Then you should be able to do a `$ make` at the top level to build the executable called `parser`.

There are a bunch of example test programs in the `tests/` directory. At the top level, do a `$ make test` to go into this directory and run the `runtests` script. This checks the output of each file to ensure it runs correctly, or checks that the compiler dies with the correct fatal error.

If you want to just run a single test, in the `tests/` directory you can do e.g.

```
$ ./runone test009.al
17
5.500000
23.000000
60.000000
```

And if you want to see the AST tree dump and the resulting QBE intermediate file, you can do:

```
$ ../parse -D tree.txt -o test009.q test009.al
$ ls -l tree.txt test009.q
-rw-r--r-- 1 wkt wkt 1327 Apr 14 13:14 test009.q
-rw-r--r-- 1 wkt wkt  567 Apr 14 13:14 tree.txt
```

`-D` sends debug output to the named file, `-o` sends the intermediate code to the named file.

## Conclusion and The Next Step

That's about it for part 1 of the *alic* journey. We now have a language with three statement formats, a bunch of types, and a hierarchy of expressions to join variables and literals with all of the types. So far that's taken about 1,250 lines of code!

If you were itching to write your own language, then this is a pretty good example of how you would start and what you might initially consider implementing.

In the next part of the *alic* journey, I think I will start adding the usual control flow statements: IF, WHILE and FOR.
