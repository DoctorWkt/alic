# Overview of *alic* and Differences from C

This document refers to the latest version of *alic* in the *alic* journey.

*alic* is a toy language, partly inspired by C. My aim is to reduce the amount of undefined behaviour in *alic* (compared to C) and to add features that I wish C had.

If I don't mention a feature here, and if you can't see it in *alic*'s [grammar definition](grammar.txt), then *alic* doesn't have the feature.

## Built-in Types

*(see [Part 1](../Part_01/Readme.md))*

*alic* has these built-in types, where the numeric suffix indicates the size in bits:

  * Signed Integer: `int8`, `int16`, `int32` and `int64`
  * Unsigned Integer: `uint8`, `uint16`, `uint32` and `uint64`
  * Floating Point: `flt32` and `flt64`
  * Boolean: `bool`

In terms of implicit widening:

 * smaller signed integers can be widened to larger signed integers,
 * smaller unsigned integers can be widened to larger unsigned integers,
 * both signed and unsigned integers can be widened to either floating point type, and
 * `bool` can be widened to any integer or floating point type: `false` is 0 and `true` is 1.

There is a pseudo-function `cast()` to cast numeric types where the destination range or precision is smaller than the original type; see below.

Note that `bool` is not an integer type: you can only assign `true` or `false` to a `bool` variable.

There is no `void` type; this keyword is only used to show a function that returns no value and/or takes no arguments.

There is a `void *` type. This is a type that can be assigned a pointer of any type, and be assigned to a pointer of any type; see below.

`NULL` is built into the *alic* language and is a `void *` pointer with the value 0.


## Operators and Precedence

Here is the list of operators in *alic* and their precedence.


| Operator                      | Description                                                  |
|-------------------------------|--------------------------------------------------------------|
| `.` `[]`                      | Struct member access (also via pointer), array element access|
|  `()`                         | Parentheses, function call                                   |
|  `&` `*`                      | Address of, value at                                         |
|`*` `/` `%`                    | Multiply, divide, modulo                                     |
|  `+` `-`                      | Plus, minus                                                  |
| `<<` `>>`                     | Left shift, right shift                                      |
|`!` `==` `!=` `>` `>=` `<` `<=`| Logical not, comparison operators                            |
|       &#124;&#124;            | Logical OR                                                   |
|        `&&`                   | Logical AND                                                  |
|   `~` `&` &#124; `^`          | Bitwise NOT, AND, OR, XOR                                    |
|            `?:`               | Ternary operator                                             |


## Assignment Statements

Assignment statements are much like C: `variable = expression;`

However, assignment statements are **not** expressions; you cannot do `a= b= c= 3;` in *alic*.

Similarly, *alic* has post-increment and post-decrement **statements** (not expressions):

```
   fred++;
   jim--;
```


## Control Statements

*(see [Part 2](../Part_02/Readme.md))*

*alic* has four control statements: `if`, `while`, `for` and `switch`. They are much like the C equivalents.

With `while` and `for` loops, the condition has to be a boolean expression (e.g. a comparison) or the constant `true`. You can't say `while(1)` but you can say `while(true)`.

The first and last sections of the `for` loop are either single statements or a statement block. The latter is a list of statements surrounded by braces. Thus, these loops are equivalent:

```
  for (i=0, x=3; i < 10; i++)      // C version
  for ({i=0; x=3;} ; i < 10; i++)  // alic version
```

The three sections of the `for` loop are optional. If the middle condition is missing, it is treated as being `true`.

You can use `break` and `continue` in loops, just as you can in C.
You *can't* use `break` in a `switch` statement: see below for details.

## Foreach Loops

*(see [Part 15](../Part_15/Readme.md))*

There are four flavours of `foreach` loops which are essentially syntactic sugar versions of `for` loops.

The first is to iterate over array elements:

```
  int32 list[5]= { 1, 2, 3, 4, 7 };
  int32 elem;

  // Print out all the elements in the list
  foreach elem (list)
    printf("%d\n", elem);
```

The second is to iterate across an *inclusive* range of values:

```
  int32 i;

  // Print out the numbers from 1 to 100
  foreach i (1 ... 100)
    printf("%d\n", i);
```

The third is to walk a linked list:

```
type FOO = struct {
  int32 value,
  FOO *next
};

FOO *Head;
...
  FOO *this;

  // Walk the list from Head down
  // and print out all the values
  foreach this (Head, this.next)
    printf("%d\n", this.value);
```

## Iterator Functions

The fourth flavour of `foreach` is to call an *iterator* function and walk the list of values that it returns, e.g.

```
  uint32 x;

  foreach x (factors(60))
    printf("%2d is a factor of 60\n", x);
```

This requires that the iterator function return a data structure like this (for `factors(6))`:

![iterator data structure](figs/iterlist.png)


The top row is a contiguous list of pointers to `uint32` values with the last pointer set to NULL. This list was `malloc()`'d by the iterator function. Below that are the `uint32` values that hold the actual factors of 6; these also have been `malloc()`'d by the function.

If there are no elements to return from an iterator function, it can return NULL.

The `foreach` loop will free the top list of pointers and every `malloc()`'d node on the bottom row.

## Switch Statements

*(see [Part 11](../Part_11/Readme.md))*

These look the same as C switch statements, but there is a **big** difference: cases do not fall through to the next case; instead, they jump to the end of the switch statement. If you want to fall through to the next case, you need to use the `fallthru` keyword. Also, the `break` statement is **not** used in a switch statement; it is only used for loops.

Here is an example *alic* program that demonstrates the switch statement and its output:

```
#include <stdio.ah>

public void main(void) {
  int32 x;

  for (x=1; x <= 9; x= x + 1) {
    switch(x) {
      case  3: printf("case 3\n");
      case  5:
      case  6: printf("case %d, fallthru to ...\n",x);
	       fallthru;
      case  7: printf("case 7\n");
      default: printf("case %d, default\n", x);
    }
  }
}

case 1, default
case 2, default
case 3
case 4, default
case 5, fallthru to ...
case 7
case 6, fallthru to ...
case 7
case 7
case 8, default
case 9, default
```

You can also use string (i.e. pointer to `int8`) expressions in a switch statement and string literals as case values. These will be hashed to `uint64` integer values with the [djb2 hash function](http://www.cse.yorku.ca/~oz/hash.html), so there is a small but non-zero chance that there is a collision between your expression and a different string literal case value.

## Functions and Function Calling

*alic*'s functions resemble C functions. A function can have zero or more arguments (use `void` when there are zero arguments), and it can return zero or one value. All arguments and return values have to be scalar, i.e. not structures.

Function arguments can be expressions, so you can write:

```
  x= fred(a+2, b-3, c*d+a);
```

Argument values are evaluated from left to right.

## Named Function Arguments

*(see [Part 6](../Part_06/Readme.md))*

*alic* differs from C in that you can name arguments to a function. For example, if a function is declared as:

```
void fred(int32 a, int32 b, flt32 c) { ... }
```

then you can call it like this:

```
  fred(c= 30.5, a= 11, b= 19);
```

If you choose to name arguments, you must name all of them.

## Variadic Functions

A [variadic](https://en.wikipedia.org/wiki/Variadic_function) function is indicated by an ellipsis ( `...` ) at the end of the function's parameter list, e.g.

```
int16 foobar(int8 *fmt, ...) { <code> }
```

In order to access variadic arguments from inside a variadic function, you first declare a `void *` variable, and then use the pseudo-function `va_start()` to point it at a hidden structure which holds the state of the variadic arguments. And when you are finished with the variadic arguments, you use `va_end()` to indicate this. For example:

```
int16 foobar(int8 *fmt, ...) {
  void *ptr;

  va_start(ptr);
  <code>
  va_end(ptr);
}
```

Once you have done `va_start()`, you can use `va_arg()` to access the next variadic argument. This pseudo-function takes two arguments: the state pointer and the type of the variadic argument. So, to continue the above example:

```
int16 foobar(int8 *fmt, ...) {
  void *ptr;
  int32 x;
  flt64 y;
  uint32 z;

  va_start(ptr);           // Initialise the state pointer
  x= va_arg(ptr,int32);    // Get an int32 argument value
  y= va_arg(ptr,flt64);    // Ditto for a flt64 value
  z= va_arg(ptr,uint32);   // Ditto for an uin32 value
  va_end(ptr);
}
```

Depending on your platform's ABI, variadic arguments will be widened to meet minimum sizes. On the 64-bit Intel/AMD platform, integers are widened to be at least 32 bits and floats are widened to be 64 bits. You cannot use integer/floating types smaller than these with `va_arg()`.

## Function Pointers

*alic* provides function pointers, but the declaration syntax is different to C. Essentially, a function pointer declaration looks like a function prototype except that there is a '*' immediately after the function pointer's name. For example:

```
int32 foo(int8 *name, bool is_irregular);    // A function prototype
int32 fred* (int8 *name, bool is_irregular); // A function pointer called fred
```

Function pointers can be declared outside of a function (with no initialisation), or as a local variable with an initialisation. Here is an example of the latter:

```
int32 foo(int8 *name, bool is_iregular) { return(1); }

public void main(void) {

  // Function pointer and assignment
  int32 fred *(int8 *name, bool is_iregular) = foo;
```

Once you have a function pointer, you point it at a function with an assignment statement that names the function, e.g.

```
  fred= foo;
```

The number and type of function parameters must match.

To call via a function pointer, treat the function pointer's name as a function name. e.g

```
  x= fred("hello", true);
```

## Header Files

The *alic* compiler invokes the C-preprocessor on the input files, so you can include header files in your programs. The *include* directory in each part holds a number of header files. Their suffix is `.ah` to distinguish them from C header files.

## Symbol Visibility

*alic* has two keywords which affect the visibility of a symbol outside a function: `extern` and `public`. `extern` means the same as it does in C: a symbol is defined in another file. The `public` keyword indicates that a non-local symbol (e.g. a function or variable) should be made visible to other files.

By default, functions and non-local variables are marked as *not* visible to other files: they are, thus, private to the file being compiled.

The aim here is to make it easier for a programmer to prevent "leakage" of symbol names. If you want a function or variable to be visible, you now have to mark it as `public`. You must also add `public` to function prototypes if they represent functions which must be visible across many source files.

This also means that you **must** declare `main()` to be `public`!

## Enums

An `enum` in *alic* is **not** a type; it's just a way to give names to integer values, e.g.

```
enum { a, b, c=25, d, e= -34, f, g };
```

`a` is the constant 0, `b` is 1, `c` and `e` as shown, `d` is 26, `f` is -33 and `g` is -32.

## User-defined Types

*(see [Part 8](../Part_08/Readme.md))*

You can define new types in *alic* by using the `type` keyword. You can define opaque types, type aliases and structured types.

## Opaque Types

An opaque type has a name but no details about its size or structure, e.g.

```
type FILE;
```

The idea here is that a library that has its own type (e.g. the standard I/O library) can keep the details of the type hidden: only the existence of the type is given in a header file.

While you cannot declare a variable of opaque type in an *alic* program, you can declare a pointer to the type, e.g.

```
  FILE *input_filehandle;
```

Thus, you can receive a pointer to a `FILE` from a library function, and send a pointer to a library function, but never see the internal details of the type.

## Type Aliases

*alic* allows type aliases, e.g.

```
type char = int8;
type String = char *;
```

## void *

There is a built-in type which is `void *`. You can declare variables of this type and you can
declare functions that return this type.

You can assign a `void *` value to any pointer type, and you can assign any pointer type value to
a `void *` variable. This is useful to do things like this:

```
void *malloc(size_t size);

void main(void) {
  int32 *fred;

  fred= malloc(100 * sizeof(int32));
}
```

without the need for casting.

## Structured Types

*(see [Part 9](../Part_09/Readme.md))*

*alic* has structured types. One difference from C is that the list of members in a struct are separated by commas, not semicolons. Another difference is that unions can only be declared inside a struct, and the union itself has no name. Here is an example:

```
type FOO = struct {
  int32 a,
  flt32 b,
  union { flt64 x, int16 y, bool z },
  bool c
};
```

If you now declare a variable, then you can do this:

```
  FOO var;

  var.a = 5;
  var.x = 3.2;
  var.c = true;
```

## Pointers

*alic* has pointers which are declared using the normal C syntax. The `&` operator gets the address of a variable, and the `*` operator dereferences a pointer to get the value that it points at.

The C syntax for accessing a struct's member through a pointer is the '`->`' operator. This does **not** exist in *alic*. You can use the '.' operator instead.

Consider the `FOO var` variable above. Let's take a pointer to it:

```
  FOO var;
  FOO *ptr;

  ptr= &var;      // Get a pointer to var

  var.a= 5;       // Set one of the var member values

                  // Access the same member through the pointer
  printf("We can print out %d\n", ptr.a);
```

## Array Access with Pointers

You can use a pointer as the base of an array:

```
  int32 *ptr= malloc(100 * sizeof(int32));
  ptr[5]= 23;
  printf("%d\n", ptr[5]);
```

## Arrays

In *alic*, arrays are fixed in size. When you declare an array, you *must* give the number of elements, e.g.

```
int32 fred[5];
int16 jim[12]= { <list of values> };
extern flt32 list[10];
```

You must give the number of elements even for `extern` array declarations.

*alic* allows you to have arrays of structs, structs with array members and structs with struct members.

You can't define a type as being an array, i.e. this is not permitted:

```
type FOO = int32 fred[5];
```

## Array Bounds Checking

By default, an access into an array will be bounds checked. If the index is below zero or greater than or equal to the number of elements, the program will print an error message and `exit(1)`. You can disable this by using the `-B` compiler command-line option.

If you use array access via a pointer, there is no bounds checking.

## Associative Arrays

*(see [Part 16](../Part_16/Readme.md))*

*alic* provides [associative arrays](https://en.wikipedia.org/wiki/Associative_array): in-memory dynamic key/value stores with the following operations:

  * declare an associative array,
  * add (or update) a key/value pair to the array,
  * search the array with a key and get the associated value,
  * delete a key from the array,
  * test if a key/value pair exists in the array, and
  * iterate over the values in an array.

The value type can be any scalar value that fits into 64 bits: integers, floats, `bool` and pointers. The key type can be any integer type, `bool` and `int 8 *` (i.e. strings).

To declare an associative array, you give the type of the value and key and name the array, e.g.

```
  int32 age[int8 *];        // Array of ages keyed by person's name
  int8 *address[uint64];    // Array of address strings keyed by unsigned account number
```

To add a new key/value pair, or to update an existing key/value pair, use normal array syntax, e.g.

```
   age["Fred"]= 23;
   address[40325]= "101 Blah st, Foosville";
```

To get a value given a key, again use normal array syntax, e.g.

```
   if (age["Fred"] > 19) printf("Fred is no longer a teenager\n");
   printf("Account %ld, address %s\n", acct, address[acct]);
```

Note that if you try to get a value when the key doesn't exist, you will receive the value 0 (or `NULL` if the value type is a pointer). To properly tell if the key exists in the array, use the `exists()` pseudo-function which returns `bool`, e.g.

```
   if (exists(address[acct]))
      printf("Account %ld, address %s\n", acct, address[acct]);
   else
      printf("Account %ld has no address\n");
```

To delete a key in an associative array, use the `undef()` statement, e.g.

```
   undef(address[11112]);
```

You are permitted to delete keys that don't exist in the array.

To iterate over all the values in an associate array, use `foreach()`, e.g.

```
   int32 a;

   foreach a (age) printf("age %d in the list\n", a);
```

### Associative Array Notes

If you use pointer keys or values, remember that only the pointer values are stored in the associative array, not what they point at.

If you use a string as the key for an associative array, *alic* uses the [djb2 hash function](http://www.cse.yorku.ca/~oz/hash.html) to create a 64-bit key value to represent the string. There is a very small, but non-zero, chance that two strings will generate the same key value.

When iterating over the values in an associative array, there is no guarantee of any specific ordering of the values.

## Initialising Variables

For non-local variables, including arrays and structs, you can provide either a single value known at compile time, or a `{` ... `}` list of values separated by commas. If you have nested data structures, you can nest `{` ... `}`. For example:

```
type FOO= struct {
  int32 a,
  bool  b,
  flt32 c
};

FOO dave= { 13, true, 23.5 };

FOO fred[3]= {
        { 1, true,  1.2 },
        { 2, false, 4.5 },
        { 3, true,  6.7 }
};
```

For local variables, *alic* only lets you initialise scalar variables, i.e. not structs and not arrays. You can use expressions that will be evaluated at run-time. For example:

```
void main(void) {
  int32 x= 3;
  int32 y= x * 4;
  FOO   fred;      // Cannot initialise this
}
```

To reduce any undefined behaviour, any variable declaration (local or non-local) without an initialisation expression will be filled with zero bits.

## The `const` Keyword

*(see [Part 14](../Part_14/Readme.md))*

The `const` keyword can be applied to:

  * non-local variable declarations (if marked `extern` or `public`, after this),
  * parameter and local variable declarations,
  * before any string literals,
  * struct members in the type definition, and
  * assignment statements.

The `const` keyword has the following meanings.

For scalar variables, once the variable is declared with any initialisation its value cannot be changed. Example:

```
  const int32 x = y + 7;
  x= 5;                     // Not permitted
```

For pointer variables, after the initialisation the pointer cannot be pointed at anything else. However, the value that it points to can be changed. Example:

```
  char *name= "Fred Bloggs";
  const char *ptr = name;        // ptr points at name
  char *addr= "23 Blah st";

  ptr= addr;                     // Not permitted
  *ptr= "G";                     // Permitted: 'F' is changed to 'G'
```

For array variables, after the initialisation no value in the array can be changed. Example:

```
const int32 x[5]= { 2, 4, 6, 8, 10 };

public void main(void) {
  x[3]= 100;                     // Not permitted
}
```

For struct variables, after the initialisation no member value in the struct can be changed. Example:

```
type FOO = struct {
  int32 x,
  int8 y,
  flt32 z
};

const FOO fred = { 100, -3, 3.14 };

public void main(void) {
  fred.y = 45;                      // Not permitted
}
```

For struct members, after the initialisation the member's value in the struct cannot be changed. Example:

```
type FOO = struct {
  int32 x,
  const int8 y,
  flt32 z
};

FOO fred = { 100, -3, 3.14 };

public void main(void) {
  fred.x = 300;                     // Permitted
  fred.y = 45;                      // Not permitted
}
```

The `const` keyword can also precede string literals. In this situation, no characters in the string literal can be changed. Example:

```
public void main(void) {
  char *name= const "Fred Bloggs";

  *name = 'G';                      // Not permitted
  name[2] = 'x';                    // Not permitted
  name = NULL;                      // Permitted as name isn't const
}
```

In an assignment statement the word `const` following the `=` sign tells the compiler to mark the variable as being `const`. Any future assignments to the variable, as seen at compile time only, will be treated as an error. Example:

```
public void main(void) {
  int32 result;

  result = 7;       // Result set to 7
  result = const;   // Now it cannot be changed
  result= 100;      // Not permitted
```

You can do this to single variable names: scalars, array names, struct names.
You can't do this, using the `.` or `[]` operators, to make individual array elements
or struct members `const`.

A `const` violation via an assignment is detected at compile time and will produce an error message like this: `input.al line 6: Cannot change a const variable`.

Global `const` variables and `const` string literals will be stored in the read-only data section in the final executable. Thus, if you try to circumvent their `const` attribute (e.g. by taking a pointer to them and then assigning to a value at that pointer), your program will stop with a segmentation fault.

### sizeof()

The pseudo-function `sizeof()` is fairly similar to the C version. You can get the size of a type and the size of a variable. However, if the variable is an array, then you get the number of elements in the array. For example:

```
int32 fred[5]= { 3, 1, 4, 1, 5 };
  ...
  for (i=0; i < sizeof(fred); i++)
    printf("fred[%d] is %d\n", i, fred[i]);
```

## Exceptions and Exception Handling

*(see [Part 10](../Part_10/Readme.md))*

In *alic*, functions can throw [exceptions](https://en.wikipedia.org/wiki/Exception_handling), and there is a syntax to catch an exception and deal with it.

A function is declared to throw an exception using the `throws` extension to the declaration, e.g.

```
void *Malloc(size_t size) throws Exception *e { ... }
```

`e` is a pointer to the variable which will be sent back to the caller; in the above example it is of type `Exception` (see the `except.ah` header file). You don't have to use the `Exception` type, but there is one requirement for the type that can be used: it must be a struct with an `int32` as the first member of the type.

A function which throws an exception receives a pointer to a suitable exception variable from the caller, as shown above. The `int32` first member is zeroed when the function is called. When the function wants to throw an exception, it must set the first member to be non-zero and then use the `abort` keyword to end the function and return to the caller, e.g.

```
void *Malloc(size_t size) throws Exception *e {
  void *ptr= malloc(size);         // Try to malloc() the area
  if (ptr == NULL) {               // It failed
     e.errno= ENOMEM;              // Set the int32 error to ENOMEM
     abort;                        // and throw the exception
  }
  return(ptr);                     // Otherwise return the valid pointer
}
```

You cannot call a function that throws an exception unless you catch it. The syntax to call a function and catch any exception is:

```
   try(exception variable) { block of code which calls the function }
   catch { block of code which is invoked if an exception occurs }
```

For example:

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

The two blocks of code are normal statement blocks, so you can have dozens of statements (including `if`, `while`, `for`s) and many function calls in the blocks.

If any function in the `try` block throws an exception, the exception variable has its first member set (by the called function) non-zero and execution jumps immediately to the `catch` block. This is done *before* any assignment of the function's return value. Above, the `list` variable won't be touched if the `Malloc()` returns an exception.

You can call functions that throw exceptions in the `catch` block as well. However, nothing will happen to the flow of execution in the `catch` block. All that will happen is that your exception variable will be altered by the function that threw the exception.

## The `cast()` Pseudo-function

*(see [Part 14](../Part_14/Readme.md))*

*alic* provides a way to cast types in a way that reduces undefined behaviour. There is a built-in pseudo-function called `cast()`. It takes an expression of numeric type and a destination numeric type. The expression's value is checked at run-time to ensure that it fits into the range of the destination type. Here is an example:

```
  int8 num;

  num= cast( rand() & 0xF, int8);      // Get a random number between 0 and 15
```

If the expression's value at run-time is outside the range of the destination type, an error message will be printed and the program will crash. Here is an example:

```
  int8 num;

  num= cast( rand() & 0xFF, int8);
```

The expression `rand() & 0xFF` can create random numbers in the range 0 .. 255, but `int8` has the range -1 .. 127. Any value over 127 will cause a run-time error.

`cast()` can convert (with run-time checking) all ten numeric types to all ten numeric types.

## Regular Expressions

*alic* provides two functions that make it reasonably easy to use regular expressions. These are in `<regex.ah>`:

```
int8 *** grep(int8 *src, int8 *search);
int8 *   sed(int8 *src, int8 *search, int8 *replace);
```

The first one is an *iterator* function and the second returns either the string with the replacement or NULL. Here is an example:

```
public void main(void) {
  int8 *str;

  int8 *src=     "This is a string with a date: 12/25/2019";
  int8 *regex=   "([0-9]+)/([0-9]+)/([0-9]+)";
  int8 *replace= "$2/$1/$3";

  foreach str (grep(src, regex))
    printf("%s\n", str);

  str = sed(src, regex, replace);
  if (str != NULL)
    printf("Replaced with %s\n", str);
  else
    printf("No replacement\n");
}
```

which prints out:

```
12
25
2019
Replaced with This is a string with a date: 25/12/2019
```

## Example *alic* Programs

In the *tests* directory in each part there are dozens of example programs which I use to do regression testing on the compiler. Most are trivial but there are some bigger programs.

In the *examples* directory  in each part you will find some non-trivial example programs.

In the *cina* directory in Parts 13 and up you will find a compiler for *alic* written in the *alic* language. It's about 7,000 lines of real-world code.
