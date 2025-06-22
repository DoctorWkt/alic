# *alic* Part 15: Some More Loop Constructs

What I like about *alic* is that I can use it as a "playground" for trying out new language ideas. In this part of the *alic* journey I'm going to try adding some new loop constructs and see if they are actually useful.

## Modifying the FOR Loop

In C you can write a FOR loop with several statements in the first and last section, e.g.

```
  for (x= 1, y= 3, z= 5; x < 10; x++, y= y+2, z= z+3)
    printf("%d\n", x + y + z);
```

I understand why they had to use commas and not semicolons to separate the statements, but it's not really consistent, is it? Statements should be terminated by semicolons! So I'm going to try changing the *alic* FOR loop to have statement blocks as options in the first and third sections of the loop construct. The above loop in *alic* would look like this:

```
  for ({ x= 1; y= 3; z= 5;}; x < 10; {x++; y= y+2; z= z+3;})
    printf("%d\n", x + y + z);
```

If you're a C programmer, you probably think this is ugly, and yes it is. But it is consistent! Maybe it would be better to spread the code out like this:

```
  for ( { x= 1; y= 3; z= 5;};
          x < 10;
        {x++; y= y+2; z= z+3;} )
    printf("%d\n", x + y + 3);
```

Well, I'll try it out and see what I think.

## Three Flavours of FOREACH

Next up, I want to add a `foreach` loop with three syntax variations. The aim here is to add some syntactic sugar for common loop actions. Here are my proposals.

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

And the third is to walk a linked list:

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

We can do all three of these right now with the existing FOR loop in *alic*, but they add several idioms to the language that make it more obvious as to what the code is doing.

So these are my proposals. Now it's time to implement them!

## The Grammar Changes

Here are the changed/added grammar rules:

```
for_stmt= FOR LPAREN (LBRACE procedural_stmts RBRACE | short_assign_stmt)?
                      SEMI expression? SEMI
                     (LBRACE procedural_stmts RBRACE | short_assign_stmt)?
              RPAREN statement_block

foreach_stmt= FOREACH postfix_variable LPAREN
              ( postfix_variable
              | expression ELLIPSIS expression
              | postfix_variable COMMA postfix_variable
              ) RPAREN statement_block
```

The FOR statement now allows either some procedural statements surrounded by braces or just a short assignment statement, for the first and last section of the loop construct.

The FOREACH statement starts with the `foreach` keyword, a postfix variable and a left parenthesis. Then we have the three syntax variations followed by the right parenthesis and the statement block.

## Modifying the FOR Loop

This was a very simple change. In `for_stmt()` in [parser.c](parser.c) after scanning the '(' in, we see if the next token is a ';', a '{' or neither. If a '{' we have a list of procedural statements. The code changes are minimal and I won't detail them here.

## Parsing the FOREACH Variants

You can see, looking at the grammar rules for the FOREACH statement, after the left parenthesis there is either a postfix variable or an expression. So, do I call `postfix_variable()` or `expression()`? Well, a `postfix_variable()` is an `expression()`, so I'll call `expression()` and check that it gives me a postfix variable when I need it.

So far I've added the parsing code for FOREACH but with no semantics. Just so you can see what this looks like, here is the code with no semantics:

```
static ASTnode *foreach_stmt(void) {
  ASTnode *var;                 // The loop variable
  ASTnode *listvar;             // The list var if iterating a list
  ASTnode *initval;             // The first and last value when ...
  ASTnode *finalval;
  ASTnode *nextval;             // The next variable if comma
  ASTnode *s;
  
  // Skip the 'foreach' keyword
  scan(&Thistoken);

  // Get the variable and the lparen
  var= postfix_variable(NULL);
  lparen();
  
  // Get the following variable/expression
  initval= expression();

  // Look at the next token to determine what
  // flavour of 'foreach' we are doing
  switch(Thistoken.token) {
    case T_ELLIPSIS:
      scan(&Thistoken);
      finalval= expression();
      break;
    case T_COMMA:
      scan(&Thistoken);
      nextval= expression();
      break;
    case T_RPAREN:
      listvar= initval;
      break;
    default: fatal("Malformed foreach loop\n");
  }

  // Get the rparen and the statement block
  rparen();
  s = statement_block(NULL);
  return(s);
}
```

So now these three loops get parsed but no code generated yet:

```
  foreach i (fred)               printf("%d\n", i);
  foreach i (1 ... 100)          printf("%d\n", i);
  foreach this (Head, this.next) printf("%d\n", this.value);
```

Time to add some semantics! The second and third should be easy: I just have to manually build the AST tree that would be built by a FOR statement. The first one will be tricky. I need a hidden index value into the `fred` array. Do I create a "hidden" index variable or is there some other way? If a "hidden" variable, I'm going to need one for each FOREACH loop. Hmm, tricky.

## Implenting FOREACH

As I have to build the AST tree manually, what I did first was to write the three `foreach` loops as a normal `for` loop, then dump the AST tree to a text file. Then I could see exactly how I needed to construct the three trees.

The "low ... high" variant is quite easy (in `foreach_stmt()`):

```
    case T_ELLIPSIS:
      // Skip the ellipsis and get the final expression
      scan(&Thistoken);
      finalval= expression();

      // Build an assignment statement for the initial value
      initval= assignment_statement(var, initval);

      // Build the comparison of var against final value
      compare= binop(rvar, finalval, A_LE);

      // Build the implicit var++ statement
      send = mkastleaf(A_NUMLIT, ty_int8, true, NULL, 1);
      send = binop(rvar, send, A_ADD);
      send = assignment_statement(var, send);
      break;
```

The loop walking variant is also quite easy:

```
    case T_COMMA:
      scan(&Thistoken);
      nextval= postfix_variable(NULL);
      // Check that the initval is a variable
      if (is_postfixvar(initval)==false)
        fatal("Expected variable before comma in foreach\n");

      // Build an assignment statement for the initial value
      initval= assignment_statement(var, initval);

      // Build the comparison of var against NULL
      compare= mkastleaf(A_NUMLIT, ty_voidptr, true, NULL, 0);
      compare= binop(rvar, compare, A_NE);

      // Build the assignment to the next value
      send = assignment_statement(var, nextval);
      break;
```

I knew the list iterating variant was going to be a challenge. Yes, I do have to create a "hidden" index variable. There is a function in [parser.c](parser.c) called `new_idxvar()` which returns the name of a new hidden index variable.

Now we have a small problem. I need to make an AST tree that will declare the hidden variable. I also need an AST tree which represents the hidden variable as an *rvalue*, and I also need an AST tree with the hidden variable as an *lvalue* so I can assign to it. Thus the code:

```
      // Declare a hidden index variable: an A_LOCAL ASTnode
      initval= mkastleaf(A_IDENT, ty_int32, false, NULL, 0);
      initval->strlit = new_idxvar();
      initval= declaration_statement(initval, NULL);

      // Make an rvalue copy of the hidden index variable
      ridx = (ASTnode *) Calloc(sizeof(ASTnode));
      memcpy(ridx, initval, sizeof(ASTnode));
      ridx->op= A_IDENT;
      ridx->rvalue= true;

      // Make an lvalue copy of the hidden index variable
      idx = (ASTnode *) Calloc(sizeof(ASTnode));
      memcpy(idx, initval, sizeof(ASTnode));
      idx->op= A_IDENT;
      idx->rvalue= false;
```

The comparison and increment code is pretty much the same as the "low ... high" variant, but we have one more wrinkle: we need to set the loop variable to each array element before we enter the loop's statement block. For example:

```
  foreach elem (list) {
    // elem must be initialised before the printf() line
    printf("%d\n", elem);
  }
```

So we have code that does this:

```
      // Assign the array's element to var
      spre= assignment_statement(var, get_array_element(listvar, ridx));
```

and when we build the loop's statement block:

```
  // Get the loop's statement block.
  s = statement_block(NULL);

  // Glue the change statement send to s.
  // If spre is not NULL, glue that before s.
  s = mkastnode(A_GLUE, s, NULL, send);
  if (spre != NULL)
    s = mkastnode(A_GLUE, spre, NULL, s);
```

Going back to a more traditional `for` loop:

```
  for (idx= 0; idx < sizeof(list); ) {
    elem= list[idx];                     // spre code
    printf("%d\n", elem);                // s    code
    idx++;                               // send code
  }
```

## Using FOREACH In Anger

I added `foreach` to the C version of the compiler; I then added it to the *alic* version of the compiler in [cina/](cina/). Test 172 tests all three variants of the `foreach` loop.

Now that `foreach` works, I went back to the *alic* version of the compiler and replaced 30 out of 40 `for` loops with `foreach` loops. I was able to use the list walking `foreach` loop and the "low ... high" variant, but not the list walking loop. Some examples are:

```
  foreach i (0 ... TEXTLEN - 1) { ... }
  foreach this (Strhead, this.next) { ... }
  foreach thisscope (Scopehead, thisscope.next) { ... }
``` 

I actually like the list walking loop: I'd rather write the last two lines above instead of these lines:

```
  for (this = Strhead; this != NULL; this = this->next)
  for (thisscope = Scopehead; thisscope != NULL; thisscope = thisscope->next)
```

## Conclusion and The Next Step

Well, this was a short part of the *alic* journey. I got all the changes to the language and compilers done in a day. I'm also starting to run out of ideas for new things to add to *alic*!

So I'll post this incomplete part up on Github and add more here once I think of things to do.

