# Overview of *alic* and Differences from C

This document refers to the latest version of *alic* in the *alic* journey.

*alic* is a toy language, partly inspired by C. If I don't mention a feature here, and if you can't see it in *alic*'s [grammar definition](grammar.txt), then *alic* doesn't have the feature.

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

However, note that `bool` is not an integer type: you can only assign `true` or `false` to a `bool` variable.

There is no `void` type; this keyword is only used to show a function that returns no value and/or takes no arguments.

There is a `void *` type. This is a type that can be assigned a pointer of any type, and be assigned to a pointer of any type.

`NULL` is built into the *alic* language and is a `void *` pointer with the value 0.

At present, *alic* does not have the ability to do type casting.

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

With `while` and `for` loops, the condition has to be a relational expression (i.e. a comparison) or the constant `true`. You can't say `while(1)` but you can say `while(true)`.

The three sections of the `for` loop are optional. If the middle condition is missing, it is treated as being `true`.

You can use `break` and `continue` in loops, just as you can in C.
You *can't* use `break` in a `switch` statement: see below for details.

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

A [variadic](https://en.wikipedia.org/wiki/Variadic_function) function is indicated by an ellipsis ( `...` ) as the function's parameter list, e.g.

```
  int foobar(...) { ... }
```

This differs from C where you can name several parameters and put the ellipsis after them.

## Header Files

The *alic* compiler invokes the C-preprocessor on the input files, so you can include header files in your programs. The *include* directory in each part holds a number of header files. Their suffix is `.ah` to distinguish them from C header files.

## Symbol Visibility

*alic* has two keywords which affect the visibility of a symbol outside a function: `extern` and `public`. `extern` means the same as it does in C: a symbol is defined in another file. The `public` keyword indicates that a non-local symbol (e.g. a function or variable) should be made visible to other files.

By default, functions and non-local variables are marked as *not* visible to other files: they are, thus, private to the file being compiled.

The aim here is to make it easier for a programmer to prevent "leakage" of symbol names. If you want a function or variable to be visible, you now have to mark it as `public`.

## Enums

An `enum` in *alic* is **not** a type; it's just a way to give names to integer values, e.g.

```
enum { a, b, c=25, d, e= -34, f, g };
```

`a` is the constant 0, `b` is 1, `c` and `d` as shown, `f` is -33 and `g` is -32.

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

*alic* has structured types. One difference from C is that the list of members of a struct are separated by commas, not semicolons. Another difference is that unions can only be declared inside a struct, and the union itself has no name. Here is an example:

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

In *alic*, when you declare an array, you *must* give the number of elements, e.g.

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

By default, an access into an array will be bounds checked. If the index is below zero, or greater than or equal to the number of elements, the program will print an error message and `exit(1)`. You can disable this by using the `-B` compiler command-line option.

If you use array access via a pointer, there is no bounds checking.

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

### sizeof()

`sizeof()` is fairly similar to the C version. You can get the size of a type and the size of a variable. However, if the variable is an array, then you get the number of elements in the array. For example:

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

## Switch Statements

*(see [Part 11](../Part_11/Readme.md))*

These look the same as C switch statements, but there is a **big** difference: cases do not fall through to the next case; instead, they jump to the end of the switch statement. If you want to fall through to the next case, you need to use the `fallthru` keyword. Also, the `break` statement is **not** used in a switch statement; it is only used for loops.

Here is an example *alic* program that demonstrates the switch statement:


```
void main(void) {
  int32 x;

  for (x=1; x <= 9; x++) {
    switch(x) {
      case  3: printf("case 3\n");                        // Only prints 3
      case  4:
      case  5:
      case  6: printf("case %d\n",x);                     // 4 and 5 fall through to 7
               if (x < 6) {
                 printf("fallthru to ...\n");
                 fallthru;                                // because of this line
               }
               printf("case 6 does not fall through!\n");
      case  7: printf("case 7\n");                        // Only prints 7
      default: printf("case %d, default\n", x);
    }
  }
}
```

## Example *alic* Programs

In the *tests* directory in each part there are dozens of example programs which I use to do regression testing on the compiler. Most are trivial but there are some bigger programs.
