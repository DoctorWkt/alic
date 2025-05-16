# *alic* Part 4: Function Arguments & Parameters, and Function Calls

In part four of my *alic* journey, I've got functions working. This includes:

  * Declaring functions with zero or more parameters
  * Having local variables which are visible within a function
  * Being able to call another function and pass arguments to it

It's another big update to the *alic* language. Let's take a tour of the changes.

## Changes to the *alic* Grammar

Actually, there isn't much in the way of changes to the grammar. We had functions declarations with lists of parameters in part three, but we were not dealing with them at the time.

In part four, we have added function calls as a possible statement to [parse.leg](parse.leg):

```
procedural_stmt = print_stmt
        | assign_stmt
        | if_stmt
        | while_stmt
        | for_stmt
        | function_call

# Function Calls: no parameters or a list of parameters
#
function_call =
        s:SYMBOL LPAREN e:expression_list RPAREN SEMI
        { $$ = mkastnode(A_FUNCCALL,s,NULL,e); }
        | s:SYMBOL LPAREN RPAREN SEMI
        { $$ = mkastnode(A_FUNCCALL,s,NULL,NULL); }

expression_list =
        e:expression COMMA l:expression_list
        { $$ = mkastnode(A_GLUE,e,NULL,l); }
        | e:expression
        { $$ = mkastnode(A_GLUE,e,NULL,NULL); }
```

A function call is a symbol followed by left/right parentheses and a semicolon. In between the parentheses there is either nothing or an expression list. An expression list is either one expression or an expression, a comma and an expression list.

This captures statements like:

```
   fred();
   fred(2+3);
   fred(2+3, a*b+c);
```

Note that function calls (for now) are statements not expressions. As yet, I haven't implemented return values from functions.

Looking at the C code that goes with the grammar, you should see that we build an AST tree for the function call (operation `A_FUNCCALL`) with an `A_IDENT` node for the function's symbolic name, and an `A_GLUE` list for the expressions which are the arguments to the function. Later on in `genAST()`, we will walk the tree to generate the function call in QBE code.

Another change to [parse.leg](parse.leg) isn't a grammar change but what we do when we recognise function declarations:

```
function_declaration = f:function_prototype
        {
          declare_function(f);
        }
        s:statement_block
        {
          gen_func_statement_block(s);
        }
        | f:function_prototype SEMI
        {
          // Add the function declaration to the symbol table
          add_function(f, f->left);
        }

function_prototype = func:typed_declaration
                LPAREN params:typed_declaration_list RPAREN
        {
          func->left= params;
          $$ = func;
        }

typed_declaration_list = d:typed_declaration COMMA dlist:typed_declaration_list
                                { d->mid= dlist; $$ = d; }
        | d:typed_declaration   { $$ = d; }
        | VOID                  { $$ = NULL; }

typed_declaration = t:type s:SYMBOL
        {
          // Add the type to the IDENT node
          s->type= t->type;
          $$ = s;
        }
```

The parameters in a function declaration are a comma-separated list of typed declarations, e.g. `int32 x, uint8 y, flt64 z`. We add the type to each symbol's ASTnode, and then link them all using the middle child pointer.

Then, in the `function_prototype` rule, the parameter list is added as the left child to the function's ASTnode.

Going up to the `function_declaration` rule, if the `function_prototype` is immediately followed by a semicolon, then this really is just a prototype: we can add the function and its list of parameters to the symbol table.

On the other hand, if the function prototype is followed by a statement block, then we `declare_function()` (i.e. generate the function's QBE preamble), and then `gen_func_statement_block()` (i.e. generate the QBE code for the statement block). And, in case you are worried, `declare_function()` does add the new function details to the symbol table.

## Refactoring the Symbol Table Code

Up to now, we had a linked list for the symbol table where all the symbols were visible. Now, we need to ensure that a function's parameters and local variables are only visible to that function. In other words, we need a local scope for each function as well as a global scope.

For now, I have just two scopes: local and global. We now have these pointers:

```
Sym *Symhead = NULL;    // Linked list of symbols
Sym *Globhead = NULL;   // Pointer to first global symbol
                        // when we have a local scope
Sym *Curfunc = NULL;    // Pointer to the function we are processing
```

When we want to start a local scope for a new function, we call:

```
// Start a new scope section on the symbol table.
void new_scope(Sym *func) {
  Globhead= Symhead;
  Curfunc= func;
}
```

When Globhead isn't NULL and points at something, then this is the start of the global symbol table. Each time a function is declared, it gets added to the global section of the symbol table.

Once a local scope is started, any symbol added to the table comes before `Globhead` and the function we are currently working on is pointed to by `Curfunc`, i.e.

```
                                     Globhead   Curfunc
                                        |          |
                                        V          V
Symhead -> list of local variables -> list of global symbols
```

When we finish working on a function, we simply re-point `Symhead` to the list of global symbols:

```
// Remove the latest scope section from the symbol table.
void end_scope(void) {
  Symhead= Globhead; Globhead= NULL; Curfunc= NULL;
}
```

## Adding a Function to the Symbol Table

The `Sym` structure for each node in the symbol table now has these fields:

```
typedef struct _sym {
  char *name;           // Symbol's name.
  int  symtype;         // Is this a variable, function etc.
  ...
  int count;            // Number of function parameters
  struct _sym *memb;    // List of function params, or struct members
  struct _sym *next;    // Pointer to the next symbol
} Sym;

// Symbol types
enum {
  ST_VARIABLE=1, ST_FUNCTION
};
```

We can now tell if a symbol is a variable or a function. If it's a function, we now have a sideways linked list called `memb` which holds the name and type of each parameter.

Because we now have several `Sym` linked lists (the symbol table, the parameter list per function), I've introduced a new function in [syms.c](syms.c):

```
// Given a pointer to the head of a symbol list, add
// a new symbol node to the list. If the symbol's name
// is already in the list, return NULL. Otherwise
// return a pointer to the new symbol.
Sym *add_sym_to(Sym **head, char *name, int symtype, Type * type) {
  ...
}
```

We can pass in either `Symhead` to add a symbol to the symbol table, or we can pass in a function's `memb` pointer to add a parameter to a function. The code also deals with the existence of `Globhead` to ensure that local variables stay local! The original `add_symbol()` function is modified to use this new function.

The old `find_symbol()` is also modified. We still walk the entire symbol table linked lists. But when we hit the `Curfunc` node, it's the function we are working on, so we also walk its `memb` list.

## Function Declarations: Semantic Errors

One of the important issues that a compiler needs to deal with are *semantic errors*: inputs which are not prevented by the language's grammar but which don't make sense. An example would be trying to declare the variable `x` twice in a function.

With the current *alic* language, we can declare a function prototype before we declare the same function with a statement block. When this happens, the compiler needs to check that the parameter list in both the prototype and actual function are the same.

I've added the [funcs.c](funcs.c) file to the compiler. I won't go through the actual code, but here are the function declarations from the file:

```
// Given an ASTnode representing a function's name & type
// and a second ASTnode holding a list of parameters, add
// the function to the symbol table. Die if the function
// exists and the parameter list is different or the
// existing function's type doesn't match the new one.
// Return 1 if there was a previous function delaration
// that had a statement block, otherwise 0
int add_function(ASTnode * func, ASTnode * paramlist) { ... }

// Declare a function which has a statement block.
// Die if a previous declaration had a statement block
void declare_function(ASTnode *f) { ... }
```

Here are some of the fatal messages from this file:

```
    fatal("%s() declaration has different type than previous: %s vs %s\n",
	fatal("%s() declaration: # params different than previous\n",
	fatal("%s() declaration: param name mismatch %s vs %s\n",
	fatal("%s() declaration: param type mismatch %s vs %s\n",
    fatal("multiple declarations for %s()\n", f->strlit);
```

Tests 28 to 34 check for these semantic errors as well as checking that the scope rules are enforced.

## The QBE Preamble for a Function

Back in the grammar for function declarations, there was a call to `declare_function()` once we had the function's prototype, then a call to `gen_func_statement_block()` once we had the function's statement block. Let's look at both of these from [funcs.c](funcs.c).

Here is the code for `declare_function()`:

```
// Declare a function which has a statement block
void declare_function(ASTnode *f) {
  Sym *this;

  // Add the function declaration to the symbol table.
  // Die if a previous declaration had a statement block
  if (add_function(f, f->left))
    fatal("multiple declarations for %s()\n", f->strlit);

  // Find the function's symbol entry and mark that it
  // does have a statement block
  this= find_symbol(f->strlit);
  this->initval.intval= 1;

  cg_func_preamble(this); new_scope(this);
}
```

As I promised, we do add the function to the symbol table! `add_function()` returns 1 if we already have that function with a statement block, to help us with detecting the semantic error. How can we tell this? The answer is the line of code in this function: `this->initval.intval= 1;`. We use the literal value in the function's `Sym` node as a flag to remember that it has a statement body.

After that, generate the function's preamble and start a local scope.

`cg_func_preamble()` in [cgen.c](cgen.c) now takes a pointer to the function's `Sym` node. It outputs the function's name as well as the list of parameters, with *alic* types converted to QBE types. As an example:

```
void fred(int32 x, int8 y) { ... }
```
becomes
```
export function $fred(w %x, w %y) { ... }
```

## Local Variables in QBE

QBE is quirky in terms of variables which are private to a function. If you don't need the variable to have an address (i.e you are not going to point to it), then you can declare and set it like this:

```
  %foo =w copy 0          # The % means private
```

We already saw that the function parameters are marked as private with a `%` sign.

If you need a variable which does have an address, then you use a QBE `alloc` operation to put it on the stack, and a `store` operation to write to it. And you can only allocate in sizes of 4, 8 or 16 bytes. Thus, `int8`s and `int16`s have to have 4 bytes allocated to them.

In my previous compiler, *acwj*, I added a flag to the `Sym` node which I've reused here:

```
typedef struct _sym {
  ...
  bool has_addr;        // Does the symbol have an address?
  ...
  struct _sym *next;    // Pointer to the next symbol
} Sym;
```

When we declare a local variable in `declaration_statement()` in [stmts.c](stmts.c), this is set true. And for parameters in `add_function()` in [funcs.c](funcs.c), this is set false.

The QBE instruction generating functions in [cgen.c](cgen.c), `cgloadvar()` and `cgstorvar()` now use this flag to decide how to get or set a variable's value. Thus, the translation of this *alic* program:

```
void fred(int32 x, int8 y) {
  int16 a= 3;
  int32 b= 0;
  flt32 c= 0.0;

  printf("a is %d\n", a);
}
```

is

```
export function $fred(w %x, w %y) {     # %x and %y have no addresses
@START
  %a =l alloc4 1                           # %a, %b and %c have addresses
  %.t2 =w copy 3
  storeh %.t2, %a
  %b =l alloc4 1
  %.t3 =w copy 0
  storew %.t3, %b
  %c =l alloc4 1
  %.t4 =s copy s_0.000000
  stores %.t4, %c
  %.t5 =w loadsh %a
  call $printf(l $L1, w %.t5)
@END
  ret
}
data $L1 = { b "a is %d\n", b 0 }
```

## Conclusion and The Next Step

We now have a nice, tidy small language with this version of *alic*. We have types, several expression operators, local variables, control statements (IF, FOR, WHILE), function declarations and function calls. And, apart from the type names, it all looks suspiciously like C.

For the next step, I was going to add named arguments to function calls. As an example:

```
void fred(int32 a, int8 b, flt32 c) { ... } ;

void main(void) {
  ...
  fred(c= 100.0 * 35, b= x+y, a= -11);
```

where the left-hand side of the `=` is the parameter name of the function and there is an expression on the right-hand side.

But I'm starting to find it difficult to work with the *leg* parser generator. My main issue is that there can only be one type of data that a rule returns. In my [parse.leg](parse.leg) file:

```
#define YYSTYPE ASTnode *
```

so I have to return some tree-shaped data structure from every grammar rule. Imagine, later on, I need to parse a typed symbol like `int32 ***ptr;`. It would be nice to just return a `Sym` pointer with the `type` field already pointing to the appropriate `Type` structure. But here, I'd have to return something like:

```
    A_IDENT
   /      \
  "ptr"  A_GLUE
         /   \
      A_MULT A_GLUE
             /    \
          A_MULT  A_MULT
```

That's awful! I'm already struggling with this with lists of parameters and lists of expressions.

It would be wonderful if we could declare the type of the sections of a rule in *leg*, e.g.

```
Sym *typed_declaration = Sym *t:type Type *s:SYMBOL
        {
          // Add the type to the IDENT node
          s->type= t;
          $$ = s;
        }
```

but we can't, unfortunately. So I think I'm going to take the big step of writing my own lexer and recursive descent parser for *alic*. I can modify the lexer from *acwj*, but I will have to write the *alic* parser from scratch.

The new lexer and parser, thus, will be the next step in this journey. I want to have exactly the same tests and results that we have here in part four, but with no *leg* parser. Wish me luck!
