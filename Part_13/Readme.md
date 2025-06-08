# *alic* Part 13: Rewriting the *alic* Compiler in *alic*

In this part of the *alic* journey, I rewrote the existing *alic* compiler from C into *alic* itself. Actually, as I type this, I passed the "triple test" about ten minutes ago, so I'm still ecstatic! Let's look at what the triple test is, and what changes to the compiler I made to get here.

## The Triple Test

Up to now, I've been using a lot of very small test programs to test the compiler for *alic*. Obviously, these don't represent real-world code bases. We really need to throw something big and complicated at the compiler and see how it deals with it.

To that end, I've rewritten the C version of the *alic* compiler (henceforth known as the level 1 compiler) into the *alic* language itself. The new codebase is in the [cina/](cina/) directory with all the source files ending in `.al` and `.ah`. These files, when compiled by the level 1 compiler, produce a binary called `alica` which is the compiler in the *alic* language (the level 2 compiler).

If we can build `alica` using `alic`, then this shows that the C version of the compiler is rugged enough to consume about 6,500 lines of *alic* code without crashing. But it doesn't guarantee that the compiler produces correct code.

So now we perform the *triple test*. Using the *alica* version of the compiler (level 2), we compile the compiler's source in the *alic* language itself! If the compiler produces correct code, then the resulting binary (*alicia*, the level 3 compiler) will have the same checksum value as the level 2 compiler. And it does:

```
$ make triple
cc -c astnodes.c               # Use the host C compiler to compile the C version
...
cc -o alic ....                # Build the level 1 compiler written in C
../alic -S astnodes.al         # Use alic to compile the compiler in alic itself
...
cc -o alica  astnodes.o ...    # Use cc to link the level 2 compiler in alic
...
./alica -S astnodes.al         # Use alica to compile the compiler in alic itself
...
cc -o alicia  astnodes.o ...   # Use cc to link the level 3 compiler in alic

md5sum alica alicia
1259f787146966fba0576cfb152f8eb9  alica
1259f787146966fba0576cfb152f8eb9  alicia
```

As I mentioned in a previous part of the *alic* journey, I would have to do a fair bit of "mopping up" to get to this point. There was not one major change to the compiler but, instead, a whole heap of minor changes and bug fixes. So I'm not going to go through everything I did. I'll concentrate on the big things and the ones that caused me the most grief!

## The Ternary Operator

As I use the ternary operator in the C version of the compiler, I needed to add this operator to the *alic* language. The grammar is:

```
expression= ternary_expression

ternary_expression= bitwise_expression
                  | LPAREN relational_expression RPAREN
                    QUESTION ternary_expression COLON ternary_expression
```

which makes it the operator with the lowest precedence. As you can see from the rules, a ternary expression can contain more ternary expressions. We had to add the '?' token and the A_TERNARY AST operation to the compiler. The parsing in `ternary_expression()` in [parser.c](parser.c) is straight-forward:

```
    // Get the relational expression
    e = relational_expression();

    // Skip the ')' and '?'
    // Get the true expression
    t = ternary_expression();

    // Skip the colon
    // Get the false expression
    f = ternary_expression();

    // Build the ASTnode
    n = mkastnode(A_TERNARY, e, t, f);
    n->type = t->type;
```

We have two types to choose from to put in the final ASTnode `n`: either the type of the true expression or the false. For now I chose to use the first one. What I really should do is ensure they have types that can be mixed.

The code for `gen_ternary()` in [genast.c](genast.c) is also
straight-forward:

```
  // Generate two labels: one for the
  // false expression, and one for the
  // end of the overall expression

  // Get a temporary to hold the result of the two expressions
  result = cgalloctemp();

  // Generate the condition code
  // Jump if false to the false label

  // Generate the true expression and the false label.
  // Copy the temporary here into the result temporary.

  // Generate the false expression and the end label.
  // Copy the temporary here into the result temporary.

  return (result);
```

## Variadic Functions, Revisited

In the `fatal()` function in [misc.c](misc.c), I'm using `va_start()` and friends from `stdargs.h`. That means that I need to add some language features to *alic* to support the access of variable argument lists.

At the same time, we have to be able to output QBE code. I read through the [QBE documentation](https://c9x.me/compile/doc/il.html#Variadic) and it left me scratching my head. So I wrote some C code that uses variable argument lists, and compiled this with the [cproc compiler](https://sr.ht/~mcf/cproc/), as this outputs QBE code. That way, I could see how to write the QBE output myself.

The first thing I needed to do was to change the grammar for function declarations to have the `...` ellipsis as the last parameter following one or more real parameters. The grammar now looks like this:

```
function_prototype= typed_declaration LPAREN
                    ( typed_declaration_list (COMMA ELLIPSIS)?
                    | VOID
                    ) RPAREN (THROWS typed_declaration )?
```

The `COMMA ELLIPSIS` is optional but it must come after a typed declaration list. This means that we now have to do, for example:

```
int printf(char *fmt, ...);
```

Now, to access variadic arguments from inside a variadic function (like `printf()`), *alic* now has two statements `va_start()` and `va_end()` and an expression `va_arg()`. Here are the grammar rules:

```
procedural_stmts= ( assign_stmt
                      ...
                  | va_start_stmt
                  | va_end_stmt

va_start_stmt= VA_START LPAREN IDENT RPAREN SEMI

va_end_stmt= VA_END LPAREN IDENT RPAREN SEMI

primary_expression= NUMLIT
                      ...
                  | va_arg_expression

va_arg_expression= VA_ARG LPAREN IDENT COMMA type RPAREN
```

All three are "pseudo" functions like `sizeof()`, and all three take an identifier as their "argument". This has to be a `void *` pointer; it holds the state needed to track which variadic argument comes next. The only expression, `va_arg()`, takes a type as its second "argument", and returns a value of that type which is the next variadic argument.

Here is an example of their use, test 129:

```
#include <stdio.ah>

void fred(int8 *fmt, ...) {
  int32 x;
  int32 y;
  flt64 z;
  void *va_ptr;             // This points to the state of the variadic args

  va_start(va_ptr);         // Initialise va_ptr
  x= va_arg(va_ptr, int32); // Get the first variadic argument of type int32
  y= va_arg(va_ptr, int32); // Get the next one, also of type int32
  z= va_arg(va_ptr, flt64); // Get the next one, of type flt64
  va_end(va_ptr);           // We don't need to track the variadic state now

  printf("fred has %d %d %f %s\n", x, y, z, fmt);
}

void main(void) {
  int32 a= 2;
  int32 b= 33;
  flt64 d= 100.3;

  fred("foo", a, b, d);
}
```

In [cgen.c](cgen.c) there are functions `cg_vastart()`, `cg_vaarg()` and `cg_vaend()` that output the QBE code. All three are quite simple, so there is no need to go through them.

One issue with variadic argument is that, based on the platform's ABI, some types need to be widened. On the x64 platform, 8-bit and 16-bit integers will be widened to at least 32-bits, and 32-bit floats need will be widened to 64-bits. There is code in `va_arg_expression()` in [parser.c](parser.c) to ensure that we can only access variadic arguments with suitable types.

And in `gen_funccall()` in [genast.c](genast.c), we also widen 8-bit and 16-bit integer variadic arguments and 32-bit floats.

## Sometimes You Have to RTFM!

I got all of this working, and then I hit a bug where I was sending a `flt64` variadic argument to a function, and it was receiving it as zero when it shouldn't be. This caused me to go around in circles for most of the day. Eventually I reached out to the QBE mailing list for help. One of the members helpfully pointed out a sentence in the QBE documentation which I'd overlooked:

> When the called function is variadic, there must be a `...` marker separating the named and variadic arguments.

Argh!. I'd been outputting `function $fred(l %fmt)` instead of `function $fred(l %fmt, ...)`! Oh well, these things happen.

## Removing Overloaded Struct Members

With the ternary operator and access to variadic arguments working, I was in a position to start translating the compiler's C code into *alic*.

Before I started, I decided it was time to do some refactoring on the C codebase. The first thing was to stop using the members of structs for different purposes. For example, in the `ASTnode` struct, I was using the `rvalue` member a) to indicate if an expression was an rvalue and b) if a variable declaration represented an array.

Why overload? Back in the 1980s it was because we needed to save every byte of memory we could. These days we have ample RAM, so there isn't a need to do this.

So, in [alic.h](alic.h), you will see that the `Sym` and `ASTnode` structs now have lots more members!

## Litval Revisited

One problem when you write code that evolves is that you don't have the final picture in your mind, and you can write code which is a bit of a hodge-podge. Up to now, we had a `Litval` struct to hold numeric literal values:

```
typedef union {
  int64_t  intval;
  uint64_t uintval;
  double   dblval;
} Litval;
```
but the information about exactly what type of data was in a `Litval` was kept elsewhere. In the new version of the compiler, we have:

```
// What type of numeric data is in a Litval
enum {
  NUM_INT = 1, NUM_UINT, NUM_FLT, NUM_CHAR
};

// Integer and float literal values are represented by this struct
struct Litval {
  union {
    int64_t intval;             // Signed integer
    uint64_t uintval;           // Unsigned integer
    double dblval;              // Floating point
  };
  int numtype;                  // The type of numerical value
};
```

which makes much more sense. I also made changes to the compiler so that we pass around pointers to a `Litval` struct; the *alic* language doesn't allow passing of structs as function arguments, so we need to pass pointers to structs instead.

## Redefining Opaque Types

The `Type` struct, which holds types, has a list of `Sym` pointers that represent the members of a struct. The `Sym` struct, which holds details of variables and functions, has a `Type` pointer member which is the type of the variable or function.

Thus, `Sym` and `Type` refer to each other. OK, so in *alic* we should be able to do:

```
type Sym;            // Set up an opaque type

type Type = struct {
  ...
  Sym *memb;         // List of members for structs
  ...
};

type Sym = struct {
  ...
  Type *type;        // Pointer to the symbol's type
};
```

But the compiler didn't allow for a type to be redefined. So I had to make changes to allow this. I've added about a dozen new lines of code in `new_type()` in [types.c](types.c) to do this.

## A Need for `unsigned()`

Up to now I've not added any ability in *alic* to cast between types, to try and reduce undefined behaviour in the language. But, when I started to translate [main.c](main.c) to *alic*, I had this code which appends the names of `.o` object files to a link command:

```
// Given a list of object files and an output filename,
// link all of the object filenames together.
void do_link(char *outfilename, char **objlist) {
  uint cnt;
  int size = TEXTLEN;
  char cmd[TEXTLEN];
  char *cptr;
  int err;

  // Start with the linker command and the output file
  cptr = cmd;
  cnt = snprintf(cptr, size, "%s %s ", LDCMD, outfilename);
  cptr = cptr + cnt;
  size = size - cnt;

  // Now append each object file
  while (*objlist != NULL) {
    cnt = snprintf(cptr, size, "%s ", *objlist);
    cptr = cptr + cnt;
    size = size - cnt;
    objlist++;
  }
  ...
}
```

Now, `cnt` has to be unsigned because we are incrementing the `cptr` pointer by `cnt` and, in *alic* pointers are treated as unsigned values. After all, what's at memory address -3?

So what's the problem? The problem is that `snprintf()` has this declaration:

```
int snprintf(char *str, size_t size, char *fmt, ...);
```

It returns a *signed* value, so I can't do `cnt = snprintf(...)`; that's mixing signed and unsigned values together. We prevent this because it leads to undefined behaviour.

At this point, I racked my brain for a solution. I didn't want to change `snprintf()`'s declaration. I couldn't change `cnt` to be signed, either.

My solution was to add another pseudo-function to *alic*: `unsigned()`. It takes an expression and, at run-time, checks that the value is a non-negative integer value. If it is, the value is passed through. If not, we stop the program like we do for a failed array bounds check.

The grammar change is this:

```
primary_expression= NUMLIT
                    ...
                  | unsigned_expression

unsigned_expression= UNSIGNED LPAREN expression RPAREN
```

In fact, as this is a comparison of the expression's value against zero, it is nearly identical to an array bounds check. In part 12 of the *alic* journey, we had a private `.boundserr()` function inserted at the top of every QBE output. I've made this more generic and renamed it as `.fatal()`. It is now a function which receives a `printf()`-style format string and some variadic arguments. These get passed to `vfprintf()` to be printed out, and then we `exit(1)`.

Thus, I can call `.fatal()` for both an array bounds failure and an negative expression value in `unsigned()`.

The implementation of `unsigned()` lives in `cg_unsign()` in [cgen.c](cgen.c). It's very similar to the array bounds checking code, but different enough that I couldn't merge them.

## Improving the & Operator

At this point I'd translated most of the small C files into *alic* and I could compile them. Then I started to tackle `parser.c`. I hit a line of code where I needed the address of a struct member:

```
  add_sym_to(&(func.sym) ...);
```

The existing compiler complained that `func.sym` wasn't an identifier. But the grammar clearly allows this:

```
unary_expression= primary_expression
                | STAR unary_expression
                | AMPER primary_expression

primary_expression= NUMLIT
                    ...
                  | postfix_variable

postfix_variable= IDENT
                | postfix_variable DOT IDENT
                | postfix_variable LBRACKET expression RBRACKET
```

So it's a deficiency that I needed to fix. In `unary_expression()` in [parser.c](parser.c) I was only expecting to see an expression which was an A_IDENT (a scalar variable) or an A_DEREF (e.g. an array element). The code should also allow for an A_ADDOFFSET, the offset from the base of a struct. The code now deals with this, and I refactored the code to be a `switch` statement instead of an `if ... else`.

## Improving `check_bel()`

The next bug to fix was to stop the compiler from crashing on some of the more complicated global variable initialisation lists in the compiler.

I think I refactored `check_bel()`, the code that walks initialisation lists in [parser.c](parser.c), two or three times before I finally got it right. And it is now simple and elegant. I'm very pleased.

## Oops, C ints are 32 Bits!

The next bug took me a while to spot. In the lexer, when we see EOF from `fgetc()` we set the token T_EOF. Except that this wasn't happening in the *alic* version of the compiler. In my `<sys/types.ah>` header file I'd written:

```
// Some simpler type names for the x64 platform
type char = int8;
type int  = int64;
```

because, after all, on a 64-bit platform C `int`s should be 64-bits. Well ... no! It turns out that they are 32-bits. So I was comparing `0xffffffffffffffff` to `0x0000000ffffffff` and the comparison was failing. Once I changed the last type line to `type int  = int32;` the problem went away.

## String Literals are Painful, Sigh

In the old `cgstrlit()` in [cgen.c](cgen.c), which outputs string literals to the QBE output file, I had a big switch statement to turn special control characters into properly escaped characters inside double quotes. Then I had to add a case for double quotes. Argh! No matter
what, I couldn't get it to work. For now I just output the byte values in decimal for all the
characters in a string literal. I remember fighting with this in *acwj* and just outputting decimal bytes.

## Refactoring `sizeof_expression()`

The existing code for `sizeof_expression()` in [parser.c](parser.c) works just fine. However, it has a `break` inside an `if` statement inside a `switch` statement. As *alic* doesn't have `break`s inside a `switch` statement, this would be problematic to translate. So I rewrote the C version of the code to avoid the `switch` statement entirely; then I translated the C code to *alic*.

## Conclusion and The Next Step

So here we are after thirteen steps of the *alic* journey, with an *alic* compiler itself wrtten in *alic*. I must admit, it's been fun designing a language from scratch, although I've borrowed heavily from C's language features.

It's also been an easier journey this time, compared to my *acwj* journey when I hadn't written a significantly-sized compiler before. I was able to re-use code and ideas from *acwj* which sped development up. And, because of my *acwj* experience, I was able to avoid some pitfalls this time.

If you are considering designing your own language and/or writing a compiler, I'll offer up a few tips:

  * Have a way of describing your language's syntax so you can see what is legal and not legal. There were a few times when I looked at *alic*'s grammar and thought: I should be able to do this, why isn't the compiler letting me do it?
  * Write lots and lots of tests, especially after you fix a compiler bug or introduce a new syntax or semantic feature. Then you can run your compiler over the existing tests and see what you've broken.
  * This time I found writing the parser code to be the easiest bit. I struggled with dealing with types (and how to mix them), scope and the symbol table. Following on from this ...
  * Get up and walk away from the keyboard! Go somewhere for a few hours and just think about things. So many times I couldn't see a way forward, but the inspiration came when I was doing chores (for me, it's usually the morning horse chores). And I got some really great language ideas doing chores, too!

What's next? Well, I've got some more language ideas to try out in *alic*. Some are new features, and some are ways to reduce undefined behaviour even more. They are all still bubbling around in my mind, not fully formed yet.

But I think this will be my next step: QBE supports 64-bit Intel/AMD CPUs, but it also supports 64-bit ARM and 64-bit RISC-V CPUs. And I have several Raspberry Pis and one RISC-V SBC, so I think I'll try to build the *alic* compilers on my ARM and RISC-V platforms and see what changes I need to make to support them.


