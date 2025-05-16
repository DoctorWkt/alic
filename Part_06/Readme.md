# *alic* Part 6: More Work on Functions

In part six of my *alic* journey, I've made functions more useful. These are the three main changes:

 - I've added *named expressions* to the language, something that C doesn't have; 
 - Functions can now return values; and
 - Functions can be used as expressions

Let's have a look at each in turn.

## Named Expressions

I've just added the first language feature to *alic* which is not in the C language: named expressions in function calls. I've wanted this in C for ages. Here's an example (test 35 in the `tests/` directory):

```
void fred(int32 a, int32 b, flt32 c) {
  printf("fred has a value %d\n", a);
  printf("fred has b value %d\n", b);
  printf("fred has c value %f\n", c);
}

void main(void) {
  fred(10, 20, 30.0);
  fred(c= 30.5, a= 11, b= 19);
}
```

Note the last line where we name the three `fred()` parameters `c`, `a`, and `b`.

I always forget the order of parameters in things like the C stdio library. For example, the file handle comes first in `fprintf()` but last in `fwrite()`. With named expressions, I could  put the file handle first in an `fwrite()` call.

## Adding Named Expressions to the *alic* Language

Here are the relevant old and new grammar rules:

```
function_call= IDENT LPAREN expression_list? RPAREN SEMI
             | IDENT LPAREN named_expression_list RPAREN SEMI

expression_list= expression (COMMA expression_list)*

named_expression_list= IDENT ASSIGN expression
                       (COMMA named_expression_list)*
```

In a function call, we can have either no expression list, one expression list or one named expression list. We've seen expression lists before. A named expression list has an identifier, an '=' sign and an expression, followed by zero or more expression lists all separated by commas.

I made the conscious decision in *alic* to **not** allow assignments to be expressions. This means that we can't do `a= b= c= 100;` in *alic*. This frees me up to use the '=' sign in named expressions. In C, this function call already has a semantic meaning:

```
   fred(a=100, b=200);    // Set a to 100, b to 200, then call fred(100, 200);
```

so we'd have to choose a different grammar to add named expressions to C.

## Parsing Named Expression Lists

With normal expression lists in *alic*, we build an AST tree with GLUE nodes. For example:

```
  a+3, b-500, c*10
```

would become:

```
       A_GLUE
      /      \
    a+3     A_GLUE
           /      \
       b-500     c*10
```

I want to send an AST tree with a different structure to the function call generator, so it can tell named from unnamed expression lists. I've arbitrarily chosen the A_ASSIGN operator. As an example, the named expression list `c=30.5, a=11, b=19` is represented as:

```
       A_ASSIGN strlit c
       /       \
     30.5     A_ASSIGN strlit a
             /       \
            11      A_ASSIGN strlit b
                   /
                  19
```

The code to parse this and create the AST tree is in `named_expression_list()` in [parser.c](parser.c). It follows pretty much the same parsing pattern as the `expression_list()` code.

One small wrinkle is that, when parsing a function call, we need to look ahead a token to see if there is an '=' sign. Here's the code:

```
static ASTnode *function_call(void) {
  ...

  // Skip the identifier and get the left parenthesis
  scan(&Thistoken);
  lparen();

  // If the next token is not a right parenthesis,
  if (Thistoken.token != T_RPAREN) {
    // See if the lookahead token is an '='.
    // If so, we have a named expression list
    scan(&Peektoken);
    if (Peektoken.token == T_ASSIGN) {
      e= named_expression_list();
    } else {
      // No, so get an expression list
      e= expression_list();
    }
  }
  ...
}
```

## Generating QBE Code from the Named AST Tree

We now need to modify `gen_funccall()` in [genast.c](genast.c) to tell the difference between named and unnamed expression lists, and generate the QBE code accordingly. The code still checks that the identifier is a symbol and checks that we have the correct number of parameters.

After that, we look at the AST operation at the top of the tree:

```
    // Do we have a named expression list?
    if (n->right->op == A_ASSIGN) {
      ...
    } else {

      // No, it's only a normal expression list.
      ...
    }
```

There's a lot of nearly identical code in both `...` sections, and I do need to refactor it some time. The 'XXX' comment is a 1980s BSD-style note/warning about the code, nothing else :-) We widen the expression's type to match the parameter, then call `genAST()` to generate the QBE code for the expression.

One semantic error check we do in the new code is to see if the same parameter name gets used multiple times, e,g `fred(a= 12, b= 13, a= 25+2);`

## Functions Returning Values

Next up is the change to allow functions to return values. I've pretty much borrowed the code for this from my *acwj* project.

The grammar change is this:

```
procedural_stmts= ( print_stmt
                  | assign_stmt
                  | if_stmt
                  | while_stmt
                  | for_stmt
                  | return_stmt
                  | function_call SEMI
                  )*

return_stmt= RETURN LPAREN expression RPAREN SEMI
           | RETURN SEMI

function_call= IDENT LPAREN expression_list? RPAREN
             | IDENT LPAREN named_expression_list RPAREN
```

The first version of the return statement is when we have a value to return; the second is when there is no value to return (i.e. the function's type is `void`).

As an aside, also notice that I've removed the trailing semicolon from the `function_call` rule and added it as part of the `procedural_stmts` ruleset. That's because function calls will soon become expressions, e.g. `a= fred(5) + 26;`.

The `return_stmt()` code in [parser.c](parser.c) does the work of parsing returns. It ensures that a `void` function doesn't return a value, and we can't use `return;` in a non-void function. Otherwise it's very plain, boring code. We build an `A_RETURN` AST node with the expression (or `NULL` if none).

In `genAST()` in [genast.c](genast.c), when we are generating QBE code and we hit an `A_RETURN` operation, we call `cgreturn()`. This takes the number of the temporary which holds any return value, and the return type of the function.

To do the latter, we now have a global variable `Sym *Thisfunction` which points at the symbol structure of the function we are currently parsing. This makes it easy to get the current function's type.

The `cgreturn()` code in [cgen.c](cgen.c) is small and simple. If the function is not `void`, we copy the temporary into the QBE `%.ret` variable. And we immediately jump to the function's `@END` label.

The `cg_func_postamble()` function is also modified to either return the `%.ret` value or just simply return with no value.

## Functions as Expressions

We now need to change the compiler to allow functions to be used as expressions, and to get their return value back. Here is the grammar change:

```
factor= NUMLIT
      | TRUE
      | FALSE
      | variable
      | function_call
```

which is performed by `factor()` in [parser.c](parser.c). The main change is:

```
static ASTnode *factor(void) {
  ASTnode *f;
  Sym *sym;

  switch(Thistoken.token) {
  ...
  case T_IDENT:
    // Is this a function?
    sym= find_symbol(Thistoken.tokstr);
    if (sym != NULL && sym->symtype == ST_FUNCTION)
      f= function_call();
    else
      f= variable();
    break;
    ...
  }

  return(f);
}
```

And there are a few changes to `function_call()` over and above the ones we saw earlier:

```
static ASTnode *function_call(void) {
  ASTnode *s, *e=NULL;
  Sym *sym;
  ...
  // Get the function's Sym pointer
  sym= find_symbol(s->strlit);
  if (sym==NULL || sym->symtype != ST_FUNCTION)
    fatal("Unknown function %s()\n", s->strlit);
  ...
  // Build the function call node and set its type
  s= mkastnode(A_FUNCCALL,s,NULL,e);
  s->type= sym->type;
  return(s);
}
```

There's a small change to `gen_funccall()` in [genast.c](genast.c) to return the temporary id from `cgcall()`; previously it returned `NOREG` as a function call was a statement.

And there's some small changes to `cgcall()` in [cgen.c](cgen.c):

```
int cgcall(Sym *sym, int numargs, int *arglist, Type **typelist) {
  int rettemp= NOREG;
  int i;

  // Call the function
  if (sym->type == ty_void)
    fprintf(Outfh, "  call $%s(", sym->name);
  else {
    // Get a new temporary for the return result
    rettemp = cgalloctemp();

    fprintf(Outfh, "  %%.t%d =%s call $%s(",
        rettemp, qbetype(sym->type), sym->name);
  }

  // Output the list of arguments
  for (i = 0; i < numargs; i++) {
    fprintf(Outfh, "%s %%.t%d", qbetype(typelist[i]), arglist[i]);
    if (i < numargs-1) fprintf(Outfh, ", ");
  }

  fprintf(Outfh, ")\n");
  return (rettemp);
}
```

If the function doesn't return `void`, we get a new temporary id, call the function (and declare its return type), and assign the result to the temporary. Here's an example:

```
  call $fred(w %.t10)           # fred() is a void function
  ...
  %.t12 =w call $mary(w %.t11)  # mary() returns int32
```

Have a look at test 37 in the `tests/` directory for an example that generates the above calling code.

## Conclusion and The Next Step(s)

What I really want to achieve is about three major steps away. This is to have **opaque** user-defined types. An example would be:

```
type FILE;
```

This has the meaning: `FILE` is a user-defined type. We don't have any details about its internals, but it does exist. We cannot see inside it, but we can get a pointer to a `FILE` and we can pass this pointer around.

The idea is that we can write library functions which hide the details of the structures they use. Nobody really needs to see the internals of the `FILE` struct; we just need to be able to use `fopen()`, `fclose()`, fread()`, fwrite()`, `fprintf()` etc.

We should be able to have a header file like `stdio.h` that looks something like this:

```
type FILE;

FILE *fopen(uint8 *pathname, uint8 *mode);
int64 fclose(FILE *stream);
size_t fread(uint8 *ptr, size_t size, size_t nmemb, FILE *stream); 
size_t fwrite(uint8 *ptr, size_t size, size_t nmemb, FILE *stream);
```

with `size_t` defined as a type in another header file.

That's what I want to achieve. To get there I need:

  * the ability to call most of the usual C library functions;
  * which means that I need pointers; and
  * the ability to accept input from the C pre-processor and include my own header files.

So I think my next step is to introduce pointers to *alic*. If that ends up being easy, I'll bring in the code from *acwj* which calls the C-pre-processor.

