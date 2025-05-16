# *alic* Part 3: A Start on Functions

In part three of my *alic* journey, I've made a start on functions. These are the three main changes:

 - I've changed the "print" statement to look more like a function call
   (even though it actually isn't);
 - I've changed *alic*'s grammar to parse function prototypes and
   function declarations with a statement body; and
 - I've started work on local variables and variables' scopes.

This is quite a big change from part two. I've had to break a few things,
rebuild them and change *alic*'s grammar substantially.

One of the nice things about having a parser generator (*leg*)
and an existing backend code generator (QBE) is that I can try out
new grammar rules, see how they work and back them out if they don't.
That said, both *leg* and QBE have quirks that we need to deal with.

## A New Print Statement

As part of the process to adding functions, I wanted to alter the grammar
of the "print" statement. Here are the new grammar rules from the
(parse.leg)[parse.leg] file:

```
print_stmt = PRINTF LPAREN s:STRLIT COMMA e:expression RPAREN SEMI
        {
          $$ = print_statement(s, e);
        }

STRLIT = '"' < [^\"]* > '"' -
        {
          ASTnode *n= mkastleaf(A_STRLIT, NULL, false, NULL, 0);
          n->strlit= strdup(yytext);
          $$ = n;
        }

PRINTF = "printf" -
COMMA  = ','      -
```

A "print" statement is now the keyword `printf` followed by left/right parentheses and a semicolon. Inside the parentheses are a string literal, a comma and a single expression.

I really don't want to introduce strings into the language yet, but I do want to be able to print out messages along with evaluated expressions. So there's just enough grammar here to make it look like a `printf()` function call, but without dealing with string types just yet.

The ASTnode structure now has a `char *strlit` member to point at the string literal, and we have an ASTnode operator `A_STRLIT` to identify a node with a string literal in it.

You should be able to read the grammar rule `STRLIT = '"' < [^\"]* > '"' -` as: a string literal is a double quote followed by zero of more non-double quotes followed by a double quote and then any amount of whitespace.

The code for `print_statement()` hasn't changed from part two, it's just that *alic*'s grammar definition has changed.

## A Temporary Grammar for Function Definitions

I have some ideas for adding named function arguments to *alic*, but these will happen down the track. For now, I just want to define a function as a typed function name followed by a list of parameters and possibly a block of statements.

So here are the grammar rules from [parse.leg](parse.leg):

```
function_declaration = f:function_prototype s:statement_block
        | function_prototype SEMI

function_prototype = d:typed_declaration LPAREN typed_declaration_list RPAREN

typed_declaration = t:type s:SYMBOL

typed_declaration_list = typed_declaration COMMA typed_declaration_list
        | typed_declaration
        | VOID

VOID   = 'void'   -
```

We declare a function by having either a function prototype followed by a statement block, or just a function prototype followed by a semicolon. Think of these two examples:

```
int32 fred(void) { printf("%d\n", 32); }  // An actual function
int64 mary(int32 x) ;                     // A function prototype
```

I'm trying to re-use grammar rules where I can, so that's why the grammar rule for `function_prototype` doesn't end in a semicolon; I can use it for a full function declaration.

A `typed_declaration` is a symbol preceded by a type, e.g. `int32 fred`, `flt64 george`. We can use this grammar rule for both function declarations and variable declarations.

Note that the `function_prototype` rule shows that it starts with a `typed_declaration` and a 
`typed_declaration_list` within parentheses. I'm not actually using the list at present, but it's there for later on.

A `typed_declaration_list` is either a single `typed_declaration` followed by a comma and then another `typed_declaration_list`, or just one `typed_declaration` or just the keyword `void`. Examples of some typed declaration lists include:

```
void
int32 x, uint8 y, flt32 z
int8 ch
```

## Statement Blocks

The grammar for statement blocks has changed slightly from part two. It used to be this:

```
statement_block = LCURLY declaration_stmt* s:procedural_stmts RCURLY
        { $$ = s; }
```

and it's now:

```
statement_block = LCURLY s:procedural_stmts RCURLY
        { $$ = s; }
        | LCURLY d:declaration_stmts s:procedural_stmts RCURLY
        { d->right= s; $$ = d; }

declaration_stmts = d:declaration_stmt dlist:declaration_stmts
        { d->mid= dlist; $$ = d; }
        | d:declaration_stmt
        { $$ = d; }

declaration_stmt = s:typed_declaration ASSIGN e:expression SEMI
        {
          $$= declaration_statement(s, e);
        }
```

In terms of what the parser will recognise, they are identical. But I needed to make the change so that I could capture each individual declaration statement, turn it into an ASTnode, and build an AST tree of local variable declarations.

The C code in the `statement_block` rule now either returns the AST tree of statements, or returns the AST tree of declarations with the statements attached as the right child. When we process the AST tree, this forces us to do the declarations first followed by the statements.

The overall input to the parser is now defined as this:

```
input_file = function_declaration_list EOF

function_declaration_list = function_declaration function_declaration_list
        | function_declaration
```

i.e. at least one function declaration, or more than one function declaration. Right now, I haven't implemented function calls. It means that, at present, we could write this:

```
void main(void) {
  int32 i = 0;

  printf("i has the value %d\n", i);
}

int32 fred(int64 x) {
  printf("This function doesn't get called yet\n", 5);
}
```

and only the code in `main()` will get executed.

That's about it for the grammar changes to *alic*. Let's now look at the C code that implements the semantics of the language.

## A Start on Local Variables: Scopes

Up to now all variables are globally visible, because we didn't have functions *per se*. But as that is what we are working on, let's make a start at implementing them.

First up is implementing the idea of a [scope](https://en.wikipedia.org/wiki/Scope_(computer_science)): the ability to make variables or symbols visible to a certain section of code, but make them invisible to other sections of code.

We want to have variables that are local to each function: visible only to that function but not visible to other functions.

We already have a symbol table that is implemented as a linked list. Imagine that we have three globally visible variables `x`, `y` and `z`. Our symbol table might look like:

```
Symhead -----+
             |
             V
             x -> y -> z -> NULL
```

We need a way to separate scopes but still have a linked list with all currently visible symbols. To do this, I've decided that a symbol table node which has no name will act as a *scope separator*. Imagine that we now declare this function:

```
int32 main(void) { int32 a=0; int8 b=0; int16 c=3; a=b+c; }
```

We would add the three local variables to the symbol table, but there would be a *scope separator* between the local and global variables, e.g.

```
Symhead -----+
             |
             V
             a -> b -> c -> SEPARATOR -> x -> y -> z -> NULL
```

When we finish processing the function, we will destroy its scope of variables by moving the `Symhead` pointer to the symbol after the scope separator.

The code to do this is in [syms.c](syms.c):

```
// Start a new scope section on the symbol table.
// It is represented by a symbol with no name.
void new_scope(void) {
  // A NULL name represents the start of a scope.
  // The first NULL is the "no-name"
  add_symbol(NULL, false, NULL, 0);
}

// Remove the latest scope section from the symbol table.
void end_scope(void) {
  Sym *this;

  // Search for a symbol with no name
  for (this = Symhead; this != NULL; this = this->next)
    if (this->name == NULL) {
      Symhead= this->next;
      return;
    }
}
```

## Declaration Statements

In part two of, when we declared a (at the time, global) variable, the code in `declaration_statement()` did this:

```
  ...
  // Add the symbol to the symbol table
  s = add_symbol(symname, false, ty, e->litval.uintval);

  // If the expression is not a literal value,
  // now do an assignment statement to set the real value
  if (e->op != A_NUMLIT) {
    v = mkastleaf(A_IDENT, s->type, true, s, 0);
    return(assignment_statement(v, e));
  }
```

and in the `main()` function:

```
  ...
  cg_file_preamble();         // Generate the file's preamble
  cg_func_preamble();         // Generate a main() preamble
  if (yyparse()==0)           // Generate the QBE code from the AST tree
    fatal("syntax error\n");
  cg_func_postamble();        // Output the postamble
  gen_globsyms();             // Generate storage for all the globals
```

In part three, we can't generate the symbol storage after we process the AST tree, as all the variables are now local to their respective functions. Instead, we have to add them to the AST tree and ensure that they are allocated and initialised before the first statement in each function.

The `declaration_statement()` code in [stmts.c](stmts.c) is called from the parser thus:

```
declaration_stmt = s:typed_declaration ASSIGN e:expression SEMI
        {
          $$= declaration_statement(s, e);
        }

declaration_stmts = d:declaration_stmt dlist:declaration_stmts
        { d->mid= dlist; $$ = d; }      # Note the use of the middle child here
        | d:declaration_stmt
        { $$ = d; }
```

Here's a slightly abstracted version of the `declaration_statement()` code:

```
// Given an A_IDENT ASTnode s which represents a typed symbol
// and an ASTnode e which holds an expression, add the symbol
// to the symbol table and also to the ASTnode.
// Change the ASTnode to be an A_LOCAL. Then add the
// expression as the left child. Return the s node.
ASTnode *declaration_statement(ASTnode *s, ASTnode * e) {
  Sym *sym;
  ASTnode *newnode;

  // Widen the expression's type if required
  newnode= widen_type(e, s->type);
  ...
  e = newnode;

  // Add the symbol to the symbol table
  sym = add_symbol(s->strlit, false, s->type, 0);

  // Add the symbol pointer and the expresson to the s node.
  // Update the node's operation.
  s->sym= sym;
  s->left= e;
  s->op= A_LOCAL;
  return(s);
}
```

As an example, if we have these declarations at the start of a function:

```
int32 x = 7;
int64 y = 23;
flt32 z = 100.5;
<statements>
```

we get the AST tree:

```
     A_LOCAL
      int32 x
      /      \
     7     A_LOCAL
           int64 y
            /    \
           23   A_LOCAL
                flt32 z
                /     \
             100.5; <statements>
```

## Converting the AST to QBE Code

For each declared function, we now have a single AST tree that holds all the local variable declarations plus all the statements and expressions in the function. The symbol table is now used solely to a) check a variable exists and b) get its type.

The parser's `main()` code now simply does:

```
  cg_file_preamble();        // Generate any QBE preamble
  if (yyparse()==0)          // Parse, build AST, generate QBE code for it
    fatal("syntax error\n");
  gen_strlits();             // Output string literals used by the printf()s
```

And this means that the `genAST()` code in [genast.c](genast.c) must now deal with variable declarations as well as statements and expressions. Oh well!

In the tree handling, we treat an A_LOCAL node as special (like IF, WHILE etc.):

```
int genAST(ASTnode * n) {
  ...
  // Do special case nodes before the general processing
  switch (n->op) {
  ...
  case A_LOCAL:
    gen_local(n); return(NOREG);
```

The `gen_local()` code is pretty straight forward:

```
// Generate space for a local variable
// and assign its value
void gen_local(ASTnode *n) {
  int lefttemp;

  // Allocate space for the variable
  cgaddlocal(n->type, n->sym);

  // Get the expression's value on the left
  lefttemp  = genAST(n->left);

  // Store this into the local variable
  cgstorvar(lefttemp, n->type, n->sym);

  // and generate any code for the other children
  genAST(n->mid);   // e.g. another local declaration
  genAST(n->right); // or the function's statements
}
```

One quirk of QBE is that items on the stack (e.g. a local variable) must be 4 bytes long or more. So the code to add a local looks like this:

```
// Add space for a local variable
void cgaddlocal(Type *type, Sym *sym) {
  // Get the type's size.
  // Make it at least 4 bytes in size
  // as QBE requires this for a variable
  // on the stack
  int size= (type->size < 4) ? 4 : type->size;

  fprintf(outfh, "  %%%s =l alloc%d 1\n", sym->name, size);
}
```

The `%%` in the `fprintf()` statements means that local variables in QBE start with a `%` character; global symbols start with a `$` instead (see part two).

## An Example of An Input File and Its QBE Output

To get a feel for what the input and output now looks like, let's compile [tests/test005.al](tests/test005.al). The input file is:

```
void main(void) {
  int8 fred= 23;
  int8 jim= -7;
  jim= jim + fred;
  printf("%d\n", jim);
}
```

Note the (unused) function's type and parameter list!

This generates the AST tree:

```
int8 LOCAL fred
int8 NUMLIT 23
int8 LOCAL jim
int8 NUMLIT -7
GLUE
  int8 ASSIGN jim
    int8 ADD
      int8 IDENT jim
      int8 IDENT fred
  PRINT "%d\n"
    int8 IDENT jim
```

and the QBE code:

```
export function $main() {
@START
  %fred =l alloc4 1       # fred is 4 bytes on the stack
  %.t2 =w copy 23
  storeb %.t2, %fred      # and is initialised to 23.
  %jim =l alloc4 1        # Ditto jim is 4 bytes
  %.t3 =w copy -7
  storeb %.t3, %jim       # and is initialised to -7
  %.t4 =w loadsb %jim
  %.t5 =w loadsb %fred
  %.t4 =w add %.t4, %.t5  # Add fred and jim's values
  storeb %.t4, %jim       # Put them back into jim
  %.t6 =w loadsb %jim
  call $printf(l $L1, w %.t6) # And print jim out
@END
  ret
}
```

## String Literals

I've had to add enough code to the compiler to build a list of string literals, give each literal a label number (so we can load a pointer to the start of each literal), and to generate the QBE code for each literal.

Have a quick look at the code in [strlits.c](strlits.c). There is this linked list:

```
static Strlit *Strhead = NULL;  // Linked list of literals
```

I have a function to add a literal and get its label number back:

```
// Add a new string literal to the list
// and return its label number
int add_strlit(char *name) ...
```

One thing to note is that the code deduplicates string literals: if the string literal `"%d\n"` is used multiple times in the input file, it will only be output in the QBE code once.

There's a function to generate all the string literals:

```
// Generate code for all string literals
void gen_strlits(void) {
  Strlit *this;

  for (this = Strhead; this != NULL; this = this->next)
    cgstrlit(this->label, this->val);
}
```

which, in turn, calls a QBE-specific function in [cgen.c](cgen.c):

```
// Generate a string literal
void cgstrlit(int label, char *val) {
  fprintf(outfh, "data $L%d = { b \"%s\", b 0 }\n", label, val);

}
```

## Conclusion and The Next Step

We had several big changes in this part. The grammar is now very different to part two: we have things that look like functions and the "print" statement now looks like a function call. We've had to merge variables declarations and program statements into the one AST tree and generate code from the single tree. We've moved variables from being global to being function-specific, and we've introduced the concept of scope to the *alic* language.

Personally, I'm finding the *leg* grammar a bit overwhelming. I can't see all of it at once any more! I'm trying to keep it broken into sections so I can wrap my head around one section at a time.

At this point we have 1,600 lines of code, up from 1,440 in part two.

For the next step, I want to bring real functions and function calls to the *alic* language. This means things like:

  * Adding function prototypes to the symbol table
  * Declaring function parameters as local variables, and hence part of the local scope for each function
  * Checking that the number and type of arguments to a function match its prototype
  * Copying arguments to a function so that the local parameters get values
  * Returning a value from a function back, i.e. a function call is an expression.

It's going to be another big step!