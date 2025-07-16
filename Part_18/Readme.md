# *alic* Part 18: Adding Some ADA-isms to *alic*

In this part of the *alic* journey I'm going to add some language features which resemble those from the [ADA programming language](https://en.wikipedia.org/wiki/Ada_(programming_language)): `inout` parameters and types with ranges.

## `inout` Function Parameters

One of C's limitations is that you can only return one value from a function. There is a way around this: you pass a pointer to a value as a function argument. The function can then dereference this pointer to access and change the underlying value.

For example, `strsep()` needs to modify and return a string's contents as well as returning a value to its caller. Its prototype has a pointer to a string:

```
char *strsep(char ** stringp, char * delim);
```

When we use `strsep()` we need to pass the address of a string as the first argument, e.g. `&line` below:

```
int main() {
  char *line= "fred:23:hello:-5000:23 Bloggs street:4280";
  char *delim= ":";
  char *field;

  // Split the line up into delim separated fields
  while ((field=strsep(&line, delim))!=NULL)
    printf("%s\n", field);
  return(0);
}
```

I've modified *alic* so that we can specify a function parameter as `inout`. This means that the function can modify the parameter and these modifications will affect the function caller's argument.

In *alic* we can now rewrite the `strsep()` prototype:

```
char *strsep(inout char * string, char * delim);
```

Both the function caller and the `strsep()` function see `string` as an ordinary string. In reality, it is the address of the argument that is passed to the function.

Here is another example, test 183:

```
int32 fred(inout int a, int b) {
  a++;
  return(a + b);
}

public void main(void) {
  int x=5;
  int y=6;
  int z=0;

  printf("x %d y %d z %d\n", x, y, z);
  z= fred(x, y);
  printf("x %d y %d z %d\n", x, y, z);
}
```

In `fred()`, parameter `a` is declared `inout`. So, when we do `a++` in `fred()`, this is going to also affect the argument `x` in `main()`. The first `printf()` will print out `x 5 y 6 z 0`. Once `a++` is performed, both `a` and `x` have the value 6. Thus, the second `printf()` will print out `x 6 y 6 z 12`.

> For those ADA programmers reading this, yes this is more like an "access" parameter than an "in out" parameter. I chose to use the keyword `inout` here instead of `access`.

## Implementing `inout`

I stumbled with this concept, gave up and had to have a second go at implementing it.

On my first attempt, I just put an `is_inout` flag in the `Sym` struct but kept the symbol's type as declared. Then I tried to add enough jiggery pokery in places to convert it to a pointer to that type and to dereference that type when I needed to. This didn't work and it was just ugly.

On my second attempt, I set the parameter's type to be a **pointer** to the given type as well as setting a boolean `is_inout` flag in the `Sym` structure. This ended up being much simpler. But let's start, as always, at the lexical scanning.

We have a new keyword, `inout`. As usual, this means adding a `T_INOUT` token to [alic.h](alic.h) and modifying the code in [lexer.c](lexer.c) to scan it and make the token. The changes as small and I'll skip them.

On to the grammar change:

```
typed_declaration= CONST? INOUT? type IDENT
```

Preceding the type of an identifier, we can now specify the `inout` keyword. Unfortunately, this rule gets used everywhere, and we only want to allow function parameters to be declared `inout`. If you look through [parser.c](parser.c), you will see several of these:

```
  if (this thing was marked as being inout)
    fatal("Only function parameters can be declared inout\n");
```

And, while the grammar technically allows it (in [funcs.c](func.c)):

```
    // If the parameter is marked inout,
    // change its type to be a pointer to
    // the type. Also check it's not
    // marked as const
    if (this->is_inout) {
      if (this->is_const)
        fatal("An inout function parameter cannot be also const\n");
      this->type= pointer_to(this->type);
    }
```

There is no point in saying `const inout int32 x` as a parameter. Now you can't change it!

## Getting `inout` Addresses and Dereferencing Them

Both the `Sym` and `ASTnode` structs in [alic.h](alic.h) now have a `bool is_inout` member. When we parse a typed declaration and see the keyword `inout`, the returned ASTnode has the `is_inout` flag set true. This value then gets copied into the function parameter's `Sym` node in `add_function()` in in [funcs.c](func.c).

If we declare an `inout int32 x` function parameter, the `x` Symbol now has the `int32 *` type and is marked as `is_inout`. Now we need to:

  * pass the address of the corresponding argument to the function, and
  * dereference the address to access the parameter's value

because neither the function's caller nor the function itself know about our "pointer" subterfuge.

Let's start with passing the argument's address. In `gen_funccall()` in [genast.c](genast.c), when we see an `inout` parameter:

```
          // If this is an inout parameter
          if (param->is_inout) {
            // Ensure the parameter's type is a pointer
            // to the node's type
            if (param->type != pointer_to(node->type))
              fatal("inout argument not of type %s\n",
                        get_typename(value_at(param->type)));

            // Get the node's addess or, if not, an error.
            // This code echoes unary_expression()
            switch(node->op) {
              case A_DEREF:
                node= node->left;       // Remove an A_DEREF
                break;
              case A_IDENT:             // Change to ADDR
                node->op = A_ADDR;
                break;
              case A_ADDOFFSET:
                break;
              default:
                fatal("inout argument has no address\n");
            }
            node->type= param->type;
```

Normally, when we copy a argument's value to a parameter, we can widen it, e.g. an `int8` value like 'X' can be widened to `int32`. We can't do this here as we are passing the argument's address: the argument type and parameter type must match *exactly*. Once we are sure of this, we can get the argument's address using the `switch` statement shown. Finally, we update the AST node's type to be the parameter's type, which is a pointer to the original type.

Eventually, when we run `cg_call()` to generate the QBE code, this argument is now a pointer and will be copied as such.

Now, onto dereferencing the parameter and treating it like an ordinary variable. For normal variables, e.g. `inout int32 x`:

```
  // When we write this
  x++;  x= x + 5; y= x + 7;
  // We need this to become
  *x++; *x= *x + 5; y= *x + 7;
```

And if the parameter is a struct, e.g. `inout FOO a`, we need to treat `a` as a pointer to the struct. I will use the C syntax here to show you the difference:

```
  // When we write
  a.field= a.field + 17;
  result= a.field - 3;
  // We need this to become
  a->field= a->field + 17;
  result= a->field - 3;
```

In *alic* we deal with variables and struct member access in `postfix_variable()` in [parser.c](parser.c). When we see an `inout` symbol, we need to add some dereferencing.

Before I go through the code, we have a serendipitous situation in that the code that deals with struct members already deals with both "no pointer" and "yes pointer" situations:

```
  case T_DOT:
    ...
    // If the variable is a struct (not a pointer), get its address
    // (so we can add on the member's offset below)
    if (ty->ptr_depth == 0) {
      if (n->sym != NULL) {
        n->op = A_ADDR;
      }
      n->type = pointer_to(ty);
    } else {
      // It is a pointer, so set ty to the base type
      // (We already have the base address of the struct)
      ty = value_at(ty);
    }
```

With that out of the road, let's look at the new `inout` code in `postfix_variable()`:

```
  // Deal with whatever token we currently have
  switch (Thistoken.token) {
  case T_IDENT:
    ...
    // An identifier. Make an IDENT leaf node
    // with the identifier in Thistoken
    n = mkastleaf(A_IDENT, NULL, false, NULL, 0);
    ...
    // If the variable is marked inout
    if (n->sym->is_inout) {
      // Add a DEREF with a suitable type
      // when it isn't a pointer to a struct.
      // This relies on case T_DOT below which
      // can deal with struct pointers as-is.
      if ((n->sym->type->kind != TY_STRUCT) ||
          (n->sym->type->ptr_depth != 1)) {
        n= mkastnode(A_DEREF, n, NULL, NULL);
        n->type= value_at(n->left->type);
        n->rvalue= true;
      }
    }
```

We only have to dereference an `inout` symbol when it's not a struct pointer because we already have code to deal with struct pointers. That's very nice!

And that's all the changes we had to make to *alic* to support `inout` function parameters. So much nicer than my initial ugly version with three times the amount of new code.

## Integer Types with Ranges

Given that we are already doing runtime checks on array indexes and on type conversions with `cast()`, we might as well add another runtime feature: integer types with ranges. The idea is that we state what inclusive range a new type has; then we ensure that no assignment to a variable of that type has a value outside of the range.

Here's an example, test 185:

```
type FOO = int8 range 23 ... 45;

public void main(void) {
  FOO x;
  FOO y;

  x= 40;  printf("x is %d\n", x);  // Will succeed
  y= 200; printf("y is %d\n", y);  // Will fail
}
```

When I compile and run this program, the output is:

```
x is 40
expression out of range for type in main()
```
and the program exits with exit value 1.

We have the usual extra keyword "range" added to [alic.h](alic.h) and [lexer.c](lexer.c). The grammar change is:

```
type_declaration= TYPE IDENT SEMI
                | TYPE IDENT ASSIGN type integer_range? SEMI
                | TYPE IDENT ASSIGN struct_declaration SEMI

integer_range= RANGE NUMLIT ... NUMLIT
```

to allow us to express a low/high numeric range for a renamed type.

I've added these fields to the `Sym` struct:

```
struct Type {
  ...
  int64_t lower;                // For user-defined integer types, the range of
  int64_t upper;                // the type. If lower==upper==0, no range
  ...
};
```

and there is a helper function in [types.c](types.c):

```
// Return true if a type has a limited range
bool has_range(Type *ty) {
  return(ty->lower != 0 && ty->upper != 0);
}
```

To parse a type with a range, in `type_declaration()` in [parser.c](parser.c) the relevant comments are:

```
      // Do we have a RANGE token? If so, parse
      // and get the lower and upper range
      if (Thistoken.token == T_RANGE) {
        < do the parsing >
        // Ensure we are only applying
        // a range to an integer type
        ...
        // Ensure the range is compatible with the type
        ...
      }

      // Add the alias type to the list
      ...
      // Update the type with any given range
      ...
```

At this point we can now declare a new integer type with a given range of values. Now we
need to enforce the range. This means that we need to emit QBE code to do the checking. In
[cgen.c](cgen.c) we have a new function to do this:

```
// Given a temporary and a type, do a run-time
// check to ensure that the temporary's value
// fits into any type range
void cgrangecheck(int t, Type *ty, int funcname) {
  int t1 = cgalloctemp();
  int t2 = cgalloctemp();
  char *qtype = qbetype(ty);
  int Lgood = genlabel();
  int Lfail = genlabel();

  // Check t's value against the minimum
  fprintf(Outfh, "  %%.t%d =%s copy %ld\n", t1, qtype, ty->lower);
  t2 = cgcompare(A_GE, t, t1, ty);
  cgjump_if_false(t2, Lfail);

  // Check t's value against the maximum
  fprintf(Outfh, "  %%.t%d =%s copy %ld\n", t1, qtype, ty->upper);
  t2 = cgcompare(A_LE, t, t1, ty);
  cgjump_if_false(t2, Lfail);
  cgjump(Lgood);

  // Output the call to .fatal() if the range checks fail
  cglabel(Lfail);
  fprintf(Outfh, "  call $.fatal(l $.rangeerr, l $L%d)\n", funcname);
  cglabel(Lgood);
}
```

That's the runtime range checking done. Now we need to ensure that it is generated when we are doing any assignment. This is nice and simple: for both `A_IDENT` and `A_DEREF` AST nodes in `gen_assign()` in [genast.c](genast.c):

```
    // If the type has a range, check it
    if (has_range(n->right->type)) {
      cgrangecheck(ltemp, n->right->type, functemp);
    }
```

And that's it! We now have runtime range checking in assignment statements for integer types with ranges.

Tests 185 to 188 check that the runtime range check works for several integer types.

## I Got Function Pointers Wrong

I was trying to pass a function pointer to `signal()` and I realised that there was no way that I could write the prototype for `signal()` in *alic*. So I've removed all the function pointer code and I will rewrite it with function pointer types.

> *A few days later ...*

It's done! We now have this functionality in *alic*:

  * Define a function pointer type,
  * Declare variables and function parameters of these types,
  * Assign to function pointer variables using function names and other function pointer variables,
  * Call a function pointer with arguments and get any return value.

One thing I haven't done yet is add any exception handling ability to function pointers. I thought I'd get the above done first and add exception handling in later.

## Function Pointer Types: Grammar

The grammar changes to define a function pointer type are these:

```
type_declaration= TYPE IDENT SEMI
                ...
                | TYPE IDENT ASSIGN funcptr_declaration SEMI

funcptr_declaration= FUNCPTR type
                     LPAREN type_list (COMMA ELLIPSIS)? RPAREN

type_list= CONST? INOUT? type (COMMA CONST? INOUT? type)*
```

We have a new keyword "funcptr", so we can write type declarations like:

```
type sighandler_t = funcptr void(int32);
```

which is a pointer to a function that receives an `int32` argument and returns `void`.

Once we can add this type to the list of types, we can then write:

```
sighandler_t signal(int32 signum, sighandler_t handler);  // Function prototype
sighandler_t foo;                                         // Non-local function pointer

```

## Data Structure Changes

We need to augment the `Type` struct in [alic.h](alic.h) to have function pointer types and to hold the return and parameter types. There is a new kind of type:

```
// Type kinds
enum {
  TY_INT8, TY_INT16, TY_INT32, TY_INT64, TY_FLT32, TY_FLT64,
  TY_VOID, TY_BOOL, TY_USER, TY_STRUCT, TY_FUNCPTR
};
```

In the `Type` struct:

```
struct Type {
  ...
  Type *rettype;                // Return type for a function pointer
  Paramtype *paramtype;         // List of parameter types for function pointers
  Type *next;
};

// When we define a function pointer type, the
// type of each parameter is stored in this list
struct Paramtype {
  Type *type;                   // Pointer to the parameter's type
  bool is_const;                // Is the parameter constant
  bool is_inout;                // Is the parameter an "inout"
  Paramtype *next;
};
```

The latter new struct holds the types and features of all the parameters of a function pointer type.

## The Lexical and Parser Changes

We have a new keyword, "funcptr", so we have the usual changes to the list of tokens in [alic.h](alic.h) and the data structures in [lexer.c](lexer.c).

In [parser.c](parser.c), we have functions `type_list()` and `funcptr_declaration()` as per the new grammar rules.

I won't go through `type_list()` as it is pretty straight-forward: keep reading in `T_CONST` and `T_INOUT` tokens followed by a type, add it to a new `Paramtype` linked list, and stop looping when we dont' see a following `T_COMMA`.

`funcptr_declaration()` is similarly straight-forward: skip the "funcptr" keyword, get the type's name and the return type, skip the '(' token, call `type_list()` to get the parameters, skip the ')', and build a new type with all of the above information. We set the size of the type to be the same size as `ty_voidptr`, i.e. the size of a pointer.

## Adding Semantics

We can now declare a function pointer type, so we can now declare variables and parameters of this type. Now we need to be able to use them!

One action we need is to call a function using a function pointer. This is relatively easy with this small change in `function_call()` in [parser.c](parser.c):

```
  // Build the function call node and set its type
  s = mkastnode(A_FUNCCALL, s, NULL, e);
  s->sym= sym;
  // Set the type depending in a function or function ptr
  if (sym->type->kind == TY_FUNCPTR)
    s->type = sym->type->rettype;
  else
    s->type = sym->type;
```

We need to be able to get a function's start address or copy a function pointer's value, i.e. treat both as a variable and not a function; this depends on if the identifier's name is or isn't followed by a '(' token. Down in `primary_expression()`:

```
    case ST_FUNCTION:
      // This could be a function call or we
      // are assigning a function's name to
      // a function pointer. If the latter,
      // the next token isn't a '('.
      ...
      if (Peektoken.token != T_LPAREN) {
        // It's not a function call.
        // Do the work to get the function's address
        break;
      }
      // No, it must be a function call
      f = function_call();
      break;
    case ST_VARIABLE:
      // If this is a function pointer, look at the next token
      if (Peektoken.token == T_LPAREN) {
	    f= function_call();
      } else {
        f = postfix_variable(NULL);
        f->is_const= sym->is_const;
      }
```

We can now build a suitable AST tree where we can call through function names and function pointer names, and get the start address/value of functions/function pointers. Now we need to turn it into QBE intermediate code. We move to [genast.c](genast.c).

In `gen_funccall()` we now have three types of function calls: calls with arguments in the order that they are given, named arguments which can be out of order, and calls through a function pointer (whose parameter types are in a `Paramtype` list and not a `Sym` list).

I've finally refactored the code and we now have a function called `fixup_argument()` which does the work of matching an argument's type against a function parameter. This gets called by all three of the above ways we can generate a function call.

In `gen_funccall()`, we now have this basic structure:

```
  // Walk the expression list to count the number of arguments to the function.
  // For function pointers, count the number of parameters.
  // Check the arg count vs. the function parameter count.
  // Allow more arguments if the function is variadic.
  // Do we have a function pointer?
  if (func->type->kind == TY_FUNCPTR) {
     // Fix up all the arguments to match the function pointer parameter list  
     // Do we have a named expression list?
  } else if (n->right->op == A_ASSIGN) {
     // Fix up all the named arguments
  } else {
     // No, it's only a normal expression list.
     // Fix up each argument in turn against the function's parameter
  }
  ...
```

Otherwise the code is the same. There are a few things left to change in [cgen.c](cgen.c) which does the low-level QBE code generation. To start with I've changed a lot of

```
  if (type->ptr_depth > 0)  // old code, to
  if (is_pointer(type))     // new code
```

because any `TY_FUNCPTR` type is automatically a pointer, viz:

```
// Is this type a pointer?
bool is_pointer(Type * ty) {
  if (ty->kind==TY_FUNCPTR) return(true);
  return (ty->ptr_depth != 0);
}
```

The main change is in `cgloadvar()` which loads the value of a variable into a temporary location:

```
  // If it's a function pointer, copy or load it
  if (sym->type->kind == TY_FUNCPTR) {
    if (sym->has_addr==true)
      fprintf(Outfh, "  %%.t%d =l load %c%s\n", t, qbeprefix, sym->name);
    else
      fprintf(Outfh, "  %%.t%d =l copy %c%s\n", t, qbeprefix, sym->name);
    return(t);
  }
```

and there is a similar change in `cgstorvar()` to ensure a function pointer variable is treated as a pointer.

Tests 177 to 179 and test 191 check that all the function pointer functionality now works.

## Conclusion and The Next Step

That was a good step in the *alic* journey. I've already added `inout` in several places in the  [cina/](cina/) compiler which is written in *alic*. I haven't used integers with ranges just yet, but I know they will come in handy. And I can finally use `signal()` in an *alic* program.

Next up, I need to fix some bugs and then I'll try to add a `string` type to *alic*.

