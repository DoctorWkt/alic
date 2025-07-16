# The alic Language

Welcome to my *alic* journey. In my previous [acwj
journey](https://github.com/DoctorWkt/acwj), I developed a self-compiling
compiler for a subset of the C language. In this project, I want to
design a simple procedural language and build a compiler for it.

*alic* stands for "a language inspired by C". It's going to look quite
a lot like C. I also want to be able to use the existing C library,
so *alic* has to be ABI compatible with C. But I want to try out some
new language ideas and see if they work or not.

For details of what *alic* looks like, see the [docs/](docs/) directory,
especially the [overview of *alic*](docs/overview.md). Then look at the example
programs in the *tests* and *examples* directories in the most recent part.

In this journey I'm going to have fewer, bigger chunks of development
than I did with [acwj](https://github.com/DoctorWkt/acwj).
Here are the parts of the *alic* journey so far:

  * [Part 1](Part_01/Readme.md): Built-in Types and Simple Expressions
  * [Part 2](Part_02/Readme.md): Simple Control Statements
  * [Part 3](Part_03/Readme.md): A Start on Functions
  * [Part 4](Part_04/Readme.md): Function Arguments & Parameters, and Function Calls
  * [Part 5](Part_05/Readme.md): A Hand-Written Lexer and Parser
  * [Part 6](Part_06/Readme.md): More Work on Functions
  * [Part 7](Part_07/Readme.md): A Start on Pointers, A C Pre-Processor and Semantic Errors
  * [Part 8](Part_08/Readme.md): Opaque Types, Type Aliases, Enumerated Values
  * [Part 9](Part_09/Readme.md): Adding Structs and Unions
  * [Part 10](Part_10/Readme.md): Adding Exceptions
  * [Part 11](Part_11/Readme.md): More C Features
  * [Part 12](Part_12/Readme.md): Arrays, Finally
  * [Part 13](Part_13/Readme.md): Rewriting the *alic* Compiler in *alic*
  * [Part 14](Part_14/Readme.md): Adding `const` and `cast()` to *alic*
  * [Part 15](Part_15/Readme.md): Some More Loop Constructs
  * [Part 16](Part_16/Readme.md): Associative Arrays
  * [Part 17](Part_17/Readme.md): Strings in Switch, Function Pointers, Iterator Functions and Regular Expressions
  * [Part 18](Part_18/Readme.md): Adding Some ADA-isms to *alic*
  * [Part 19](Part_19/Readme.md): A `string` Type and Some Bug Fixes

If you just want to see the language features in *alic* which make it
different than C, then skips parts 1-5 and 13 and read the other parts!
Also read the [overview of *alic*](docs/overview.md) which covers the
differences between C and *alic*.

## Building the Compiler

I'm developing the compiler on a Devuan Linux box, but if you have a Linux box with a C compiler then you should be fine. You will need to download, compile and install:

  * [QBE](https://c9x.me/compile/)

Then you should be able to do a `$ make` in each part to build the executable called `alic`.

There are a bunch of example test programs in the `tests/` directory in each part. At the top level of each part, do a `$ make test` to go into this directory and run the `runtests` script. This checks the output of each file to ensure it runs correctly, or checks that the compiler dies with the correct fatal error.

In the `cina/` directory in Parts 13 and up you will find the *alic* compiler written in the *alic* language itself. To build this, at the top level of Part 13 (for example) do a `$ make triple`. This will:

  * Build the compiler using the C source code, producing the *alic* executable
  * Build the compiler using the *alic* source code, producing the *cina/alica* executable
  * Build the compiler using the *cina/alica* executable and the *alic* source code, producing the *cina/alicia* executable.
