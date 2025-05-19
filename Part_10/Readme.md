# *alic* Part 10: Adding Exceptions to *alic*

In this part of the *alic* journey, I'm going to add [exceptions](https://en.wikipedia.org/wiki/Exception_handling) to the language. As always, I will start with a rationale and an outline of the changes.

## Why Exceptions?

Well over a decade or so ago, I used to teach programming. One of the languages I taught was Java. Even though I'm not an object-oriented fan, I did find Java's "try/catch" mechanism of dealing with errors to be tidier than C.

In C, you only get a return value back from a function. To indicate that an error occurred, the function might return a special "error" value which you would have to check. And if you were lucky, the function might also set the global `errno` variable with details of any error that occurred.

That's fine if you only call one or two functions. But if you call, e.g., 20 functions in a statement block, now you have to add 20 `if` statements to check the return value and/or the `errno` value. And then you have to work out how to deal with the problem. I've seen a lot of C code that looks like this:

```
  result= function1();
  if (<it was an error>) goto bad;
  result= function2();
  if (<it was an error>) goto bad;
  result= function3();
  if (<it was an error>) goto bad;
  result= function3();
  if (<it was an error>) goto bad;
  ...
  etc.
  ...
bad: <error handling code>
```

And I've seen an awful lot of C code where the error returned from a function is completely ignored!

So, there has to be a better way to return errors back from a function and to deal with them when you get them back.

## Using '.' to Access Struct Members Through Pointers

Before I expound my ideas for exception handling, I need to take a small detour. I've always wondered why C needed the `->` operator to get access to a struct member when we have a pointer to a struct, e.g. `this->next`. If the compiler was smart, we could use the dot ('.') operator for either a struct or a struct pointer: it just has to look at the pointer depth on the left-hand side of the dot.

I started doing this in the last part of the journey, but I forgot to finish it. It's now done. Test 68 is an example:

```
void main(void) {
  FOO *ptr;

  ptr   = &jim;         // Point at the struct
  ptr.a = 5;            // Access the members
  ptr.b = 23;           // through the pointer

  printf("No more ->, we have %d and %d\n", ptr.a, ptr.b);
}
```

I just had to make a few changes to `variable()` in [parser.c](parser.c) to make it work. I've added comments in the function to help explain the change.

## My Idea for Exceptions in *alic*

Here's how I'm going to try to add exceptions to *alic*. Right now, a function can return a single result. I want to change this so that it can, instead, return an *exception* when some error has occurred. The exception should hold at least a number that represents what type of error occurred; there should also be the ability to return additional information.

Let's call the function that wants to send back an exception the "thrower", as it *throws* the exception back. And let's call the caller of said function the "catcher", as it will *catch* the exception.

I want the caller and thrower to have the ability to decide how the exception data is structured. So, one rule that I'll enforce is that the exception must be a pointer to a struct, and the first member of the struct must be of type `int32`.

We also have to prevent the thrower from sending back, as the exception, a pointer to a local variable. Why? Because, once the function returns, everything that was in its stack frame could now be corrupted. Instead, the catcher will supply a pointer to an exception variable to the thrower; the thrower can then fill in the exception structure and "throw" it back to the catcher.

Let's have a look at an example. I'm going to write a wrapper around the existing C `malloc()` which either returns a valid pointer or throws an exception. Let's start with a generic `Exception` structure:

```
type Exception = struct {
  int32 errno,         // The error that caused the exception
  void *details        // A pointer to other details if needed
};
```

Now let's look at the wrapper:

```
void *Malloc(size_t size) throws Exception *e {
  void *ptr= malloc(size);         // Try to malloc() the area
  if (ptr == NULL) {               // It failed
     e.errno= ENOMEM;              // Set the error to ENOMEM
     e.details= NULL;              // with no details
     abort;                        // and throw the exception
  }
  return(ptr);                     // Otherwise return the valid pointer
}
``` 

First up, I can't think of a better keyword than `abort` for now. I don't want to use `throw` as that's a transitive verb and it needs an object. I could make the syntax `throw(e)` but that will lead to errors when the programmer doesn't use the symbol named in the function declaration.

This leads on to another rule: you can't call a function that throws an exception unless you catch it. So now let's look at my suggestion for the catching syntax:

```
  Exception foo;
  int8 *list;
  ...
  try(foo) {
    list= Malloc(23);
  }
  catch {
    fprintf(stderr, "Could not allocate memory, error %d\n", foo.errno);
    exit(1);
  }
```

The `try` statement's grammar is roughly:

```
try_statement= TRY LPAREN IDENT RPAREN statement_block CATCH statement_block
```

We name the identifier before the first statement block: this allows the compiler to check that the identifier is a struct with an `int32` first member. It also means that we can tell the statement block that we are participating in a `try` statement: when we see the `Malloc()` call, we know the name of the variable whose address needs to be sent to `Malloc()`.

In terms of calling `Malloc()`, we copy 23 into `Malloc()`'s `size` parameter, and we also copy the address of `foo` into `Malloc()`'s `e` pointer. At the same time, the catcher function will zero the first member of the exception struct; this makes the assumption that there won't be an exception.

In the thrower function the `abort` is really just a `return` except that we don't check for a return value when the function requires a return value. We rely on the thrower setting the exception's first member to a non-zero value to indicate that there was an exception.

When the thrower returns to the catcher, the compiler will check the value of the exception's first member and jump to the `catch` clause if non-zero. Otherwise it will assign the result and continue on. When the code gets to the end of the `try` clause, it will jump past the `catch` code.

Obviously, there could be many function calls in the `try` clause which could throw exceptions. As soon as one throws an exception, we jump to the `catch` clause. And, there is likely to be  function calls in the `catch` clause which also could throw exceptions. I really don't want to have nested `try/catch` clauses, so I'll get the compiler to not jump anywhere if we are in the `catch` clause. It means that the exception value could change while we are inside the `catch` clause.

## New Keywords!

We have four new keywords: `try`, `catch`, `throws` and `abort`. See [lexer.c](lexer.c) for the changes.

## Changes to the *alic* Grammar

Here they are:

```
function_prototype= (typed_declaration LPAREN typed_declaration_list RPAREN
                    | typed_declaration LPAREN VOID RPAREN
                    | typed_declaration LPAREN ELLIPSIS RPAREN
                    ) (THROWS typed_declaration )?

procedural_stmts= ( assign_stmt
                  | if_stmt
                  | while_stmt
                  | for_stmt
                  | return_stmt
                  | abort_stmt
                  | try_stmt
                  | function_call SEMI
                  )*

try_statement= TRY LPAREN IDENT RPAREN statement_block CATCH statement_block
```

Note the optional `THROWS` line in the function prototype. In `function_prototype()` in [parser.c](parser.c) we look for this token. If it's there we get the typed declaration. We check that it's a pointer to a struct, and check that the first member is of type `int32`. We build a symbol node with the name and type, and attach it to the ASTnode for the function's prototype.

The `Sym` structure now has this:

```
struct Sym {
  ...
  Sym *exceptvar;       // Function variable that holds an exception
  ...
};
```

The exception variable found by `function_prototype()` is eventually added to the function's symbol in the symbol table when we add the function there. This is how we know the function throws an exception.

That's the parsing of the "thrower"'s declaration. Now let's look at the parsing of the code in the "catcher".

`try_stmt()` in [parser.c](parser.c) parses the try statement. We check that the identifier is a known variable. We check that it's a struct type with an `int32` as its first member.

We now build an `A_TRY` ASTnode with a pointer to the variable's symbol. Then, after verifying all the syntactical sugar, we attach the `try` and `catch` statement blocks to the ASTnode and return it.

## Generating the Code for All This

OK, so the parsing wasn't too hard. But generating the QBE code for it is certainly interesting. We have several things to do:

  * Check that we are in either a `try` or `catch` block when we call a function that throws an exception.
  * When we call a function that throws an exception, we have to copy the exception pointer over to it. To do this, we will have a parameter invisible to the caller.
  * We have to generate the correct function preamble for a function that throws an exception.
  * Each time we call a function that throws an exception, and we are in a `try` block, we have to test the exception value and possibly jump to the `catch` block.
  * And I'm sure there was more :-)

OK, lots to talk about. Let's start somewhere.

## A Stack of Exception Details

The `try/catch` statement can occur in any statement block, and the `try/catch` statement itself has two statement blocks. Ergo, `try/catch` statements can be nested; we have to deal with this.

At the top of [genast.c](genast.c) we have a new structure and a pointer to the top of the stack of them:

```
typedef struct Edetails Edetails;

struct Edetails {
  Sym *sym;                     // The variable that catches the exception
  int Lcatch;                   // The label starting the catch clause
  bool in_try;                  // Are we processing the try clause?
  Edetails *prev;               // The previous node on the stack
};

static Edetails *Ehead= NULL;   // The stack of Edetail nodes
```

Each time we start generating a `try/catch` statement, we build one of these nodes and push it on the stack. So let's now look at the code that generates the `try/catch` statement: `gen_try()` in [genast.c](genast.c). I'll give the main comments:

```
static void gen_try(ASTnode *n) {
  // Generate the labels for the start
  // and end of the catch clause

  // Make an Edetails node for this try statement
  // and fill it in: symbol, catch label and
  // in_try is set to be true

  // Push the node on the stack

  // Generate the code for the try clause
  // and jump past the catch clause

  // Output the label for the catch clause,
  // then the catch code, then the end label

  // Finally remove the Edetails node from the stack
}
```

That's nice and easy. Now we need to look at function preambles, calling functions and returning from functions.

## A Function's Preamble

`cg_func_preamble()` in [cgen.c](cgen.c) has a minimal change. If the function throws an exception, we output the exception variable as the first parameter for the function. For example, if we have this function declaration:

```
void fred(int32 a) throws FOO *e { ... }
```

then the QBE output will look like:

```
export function  $fred(l %e, w %a)
```

## Calling a Function and Returning from It

The bulk of the code changes are in `gen_funccall()` in [genast.c](genast.c).

Firstly, we cache if the function throws an exception and check that we are in a `try` or `catch` block:

```
  bool func_throws;
  ...
  // Cache if the function throws an exception
  func_throws= (func->exceptvar != NULL);

  // If the function throws an exception, we had better
  // be in a try or catch clause
  if (func_throws && (Ehead == NULL))
    fatal("must call %s() in a try or catch clause\n", n->left->strlit);
```

Now the interesting bit. Previously, all we did (once we copied the arguments to the parameters) was do a `cgcall()` to generate the QBE call instruction. Now we do this (comments only):

```
  // If we have an exception variable
  // and the function throws an exception,
  // get its address into a temporary
  if (func_throws) {

    // Get a literal zero into a temporary

    // Set the exception variable's first member to zero
  }

  // Generate the QBE code for the function call
  return_temp= cgcall(func, numargs, excepttemp, arglist, typelist);

  // If we are in a try clause, test if the first
  // member of the exception variable is not zero.
  // If not, jump to the catch clause
  if (func_throws && (Ehead != NULL)  && (Ehead->in_try == true)) {

    // Get the value of the first member in the exception variable

    // Compare the first member against zero

    // Jump if false to the catch label
  }
```

The `cgcall()` function now gets the exception temporary number. If not `NOREG`, it gets copied as the first argument to the function.

## An Example

That's about it. Let's have a look at an example and see what QBE code is generated. This is test 71. Above, we saw the declaration of `fred()`, a function that throws an exception, and the resulting QBE code. Let's now call it from `main()`:

```
void main(void) {
  FOO thing;

  try(thing) { fred(5); }
  catch      { printf("fail on 5\n"); }

  printf("We got past the first try/catch\n");

  try(thing) { fred(-2); }
  catch      { printf("fail on -2\n"); }

  printf("We got past the second try/catch\n");
}
```

I'll just go through the first `try/catch` statement. Here is the QBE output:

```
  %thing =l alloc16 1                  # Allocate space for the FOO structure
  %.t10 =w copy 5                      # Copy the literal 5 into temporary %.t10
  %.t11 =l copy %thing                 # Get a pointer to the `thing` struct
  %.t12 =w copy 0                      # Set the first member to zero
  storew %.t12, %.t11
  call $fred(l %.t11, w %.t10)         # Call fred() with the pointer and the literal 5
  %.t13 =w loadw %.t11                 # Get the 32-bit value at the start of `thing`
  %.t14 =w ceqw %.t13, %.t12           # Compare the exception value against zero
  jnz %.t14, @L8, @L6                  # Jump to @L6 if there was an exception
@L8
  jmp @L7                              # Otherwise jump past the catch code
@L6                                    # Start of the catch code
  %.t15 =l copy $L9                    # Get a pointer to a string literal
  call $printf(l %.t15)                # and print it out
@L7

  ...
data $L9 = { b "fail on 5\n", b 0 }    # The string literal
```

## Conclusion and The Next Step

I've had the idea of adding `try/catch` to C for a while now. I had thought about adding it to my *acwj* compiler, but I decided not to. I'm glad that I got to add it to *alic*.

We are up to about 4,600 lines of code for the compiler and about 100 non-blank lines for *alic*'s grammar. And the compiler is definitely fragile.

I do want to add arrays to *alic*, but I'm also trying to minimise the amount of [undefined behaviour](https://en.wikipedia.org/wiki/Undefined_behavior) in *alic*.

I am going to add array-style access to pointers. But for variables that are declared arrays, I want their number of elements to be constant and add bounds checking to prevent negative indices or indices above the number of elements.

So, the next step I think will be to add array-style access to pointers with no bounds checking. After that, I'll come up with a grammar for declaring arrays and add bounds checking when we use them.
