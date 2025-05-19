# *alic* Part 11: More C Features

I was going to start work on adding arrays to *alic*. Instead I decided to catalogue what functionality I would need to take the *alic* existing compiler code (in C) and translate it into *alic*. Having done this, I decided to add much of this functionality to *alic* now.

Here is what I've added to the *alic* language:

  * Parentheses now work in expressions.
  * A statement block can be a single procedural statement without '{' .. '}'.
  * Loops can now use `break` and `continue`. A `continue` in a `for` loop runs the increment code in the third section.
  * All three sections in a FOR loop are now optional. If the condition is missing, it is treated as being the value `true`.
  * I've added `while(true)`.
  * We can now follow a pointer chain, e.g. `x.next.next.next.val`.
  * When incrementing pointers, they increment by the size of the value they point at.
  * Using pointers, we have one level of array access.
  * I've added the logical AND and OR operators '`&&`' and '`||`'
  * I've added post-increment and post-decrement statements, but *not* expressions.
  * I've added the `sizeof()` expression, but only for types not variables.
  * I've added `switch` statements, but with a non-C twist; see below!

## Changes to the *alic* Grammar

As you can imagine, there are a lot of changes to the grammar. Let's take a look at them from top to bottom.

The first change is to the statement block:

```
statement_block= LBRACE declaration_stmts procedural_stmts RBRACE
               | procedural_stmt
```

We can have a single procedural statement without the curly brackets. This was a nice, simple change to the code:

```
static ASTnode *statement_block(Sym *func) {
  ASTnode *s=NULL, *d=NULL;

  // See if we have a single procedural statement
  s= procedural_stmt();
  if (s != NULL)
    return(s);

  // No, so parse a block surrounded by '{' ... '}'
  < the code from before >
  ...
}
```

Next up are the new statements:

```
procedural_stmts= ( assign_stmt
                  | if_stmt
                  | while_stmt
                  | for_stmt
                  | return_stmt
                  | abort_stmt
                  | break_stmt           # new
                  | continue_stmt        # new
                  | try_stmt
                  | switch_stmt          # new
                  | fallthru_stmt        # new
                  | function_call SEMI
                  )*

break_stmt= BREAK SEMI

continue_stmt= CONTINUE SEMI

fallthru_stmt= FALLTHRU SEMI
```

Three of the new statements are trivial as is the change to the parsing code. The `switch` statement is more complex.

```
switch_stmt= SWITCH LPAREN expression RPAREN switch_stmt_block

switch_stmt_block= (case_stmt
                   |default_stmt
                   )+

case_stmt= CASE expression COLON procedural_stmts?

default_stmt= DEFAULT COLON procedural_stmts
```

After the `switch` keyword we have an expression in parentheses followed by a switch statement block. This is one or more `case` or `default` statements. Both `case` and `default` start with their keyword. The `case` has the expression (an integer literal) as the case value, a colon, and zero or more procedural statements. The `default` has a colon and one or more procedural statements.

I'll cover the implementation of `switch` below.

As mentioned, the `for` loop sections are now optional:

```
for_stmt= FOR LPAREN short_assign_stmt? SEMI
                     boolean_expression? SEMI
                     short_assign_stmt? RPAREN statement_block
```

The changes were not too difficult. I set the ASTnode pointers for the first and third sections to NULL. If we don't see them, there are no AST sub-trees for them. The expression was slightly more interesting: here's the code:

```
  // If we don't have a semicolon, get the condition expression.
  // Otherwise, make a TRUE node instead
  if (Thistoken.token != T_SEMI) {
    e= boolean_expression();
  } else {
    e= mkastleaf(A_NUMLIT, ty_bool, true, NULL, 1);
  }
```

This ensures the loop condition is `true` if it isn't specified. The same sort of code is also in the parser for `while` loops:

```
while_stmt= WHILE LPAREN boolean_expression RPAREN statement_block
          | WHILE LPAREN TRUE RPAREN statement_block
```

Next up are the logical AND and OR expressions:

```
boolean_expression= logical_and_expression

logical_and_expression= logical_or_expression
                      | logical_or_expression LOGAND logical_or_expression

logical_or_expression= relational_expression
                     | relational_expression LOGOR relational_expression
```

I've introduced the `boolean_expression` as an alias for the highest precedence expression which generates a boolean value. Several of the other grammar rules (e.g. IF, FOR, WHILE) now use `boolean_expression` which, I think, makes the grammar easier to read.

Note that, according to the rules, the logical AND operator has higher precedence than the logical OR operator. I'm following the precedence rules from C here.

At the end of the grammar, we now have:

```
primary_expression= NUMLIT
                  | STRLIT
                  | TRUE
                  | FALSE
                  | NULL
                  | ENUMVAL
                  | sizeof_expression          # new
                  | postfix_variable           # was variable before
                  | function_call
                  | LPAREN expression RPAREN   # new

sizeof_expression= SIZEOF LPAREN type RPAREN

postfix_variable= IDENT
                | postfix_variable DOT IDENT
                | postfix_variable LBRACKET expression RBRACKET

variable= IDENT
```

The `sizeof()` expression is simple, and to parse it we can just read the `size` value out of the associated `Type` structure with one caveat: we can't get the size of an opaque type.

The `postfix_variable` definition allows us to use the '.' operator to follow pointer chains, and it also allows for one level of array access.

I always find it interesting that parentheses, i.e. '(' and ')', get essentially the lowest precedence in a grammar, yet they seem to have the highest precedence when they are evaluated. It's because the expression within gets its own AST sub-tree. As we recursively evaluate an AST tree from the leaf nodes up, it means that the expression in the parentheses gets evaluated before anything above it.

## Array Access

In the last part of the *alic* journey I said I wanted to work on array access. My attempt to do this caused me to add all of this C-like functionality!

For now, we can only use a pointer to access array elements. In [parser.c](parser.c) we now have `postfix_variable()` to do the parsing of array accesses. Here are the comments for array access from the function:

```
    // If this is an array access
    // Skip the left bracket
    // Get the expression in the brackets
    // Check that the IDENT variable is a pointer
    // Get the "value at" type from the IDENT variable
    // Make a NUMLIT node with the size of the base type
    // Multiply this by the expression's value
    // Add on the array's base
    // Mark this as a pointer
    // Now dereference this address
    // and mark it with the correct type
    // Get the trailing right bracket
```

Consider a pointer to a number of `int32`s, e.g.

```
  int32 *fred;
  fred= malloc(100 * sizeof(int32));

  fred[7] = 23;
```

Because `int32`s are four bytes in size, we have to get the address that `fred` points at, and add `4 * 7` to find the address of element 7 in the array. So, array accesses are a bit complicated.

## Pointer Issues

One thing I needed to do, but hadn't, was to get pointer increments and decrements right. As above, `fred` points to `int32` values. If I want to move `fred` up to the next `int32`, I should be able to do `fred++`. This supposedly adds 1 to `fred`. In reality, we have to add the size of `int32`s to `fred`.

We now have code in `widen_type()` in [types.c](types.c) to do this:

```
  // If the type is a pointer and the node is an integer
  // and we are adding or subtracting, scale the node
  // to be size of the value at the pointer.
  // This catches `int32 *x; x= x + 1; // Should be +4
  if (is_pointer(ty) && is_integer(node->type) &&
     ((op == A_ADD) || (op == A_SUBTRACT))) {

    // Widen the node to be ty_uint64
    node= widen_type(node, ty_uint64, 0);

    // Get the size of the type the pointer points at
    at_type= value_at(ty); size= at_type->size;
    if (size == 0)
      fatal("Cannot change a pointer to an opaque type\n");

    // Scale only when bigger than one
    if (size > 1) {
      node= unarop(node, A_SCALE);
      node->litval.intval= size;
      node->type= ty;
    }
    return(node);
  }
```

## Post-Increment and Post-Decrement Statements

I made a conscious decision with *alic* to not let assignments be expressions; they can only be statements. One reason is that I found it extremely difficult to deal with pre/post increment and decrement expressions in the *acwj* compiler.

In C, the order of evaluation of expressions in function calls is not specified by the C standard. Consider this function call:

```
  int x = 5;

  foo(x++, x++, x++, x++);
```

If the compiler evaluates arguments from left to right, we call `foo(5, 6, 7, 8)`. But, if the evaluation is right to left, we get `foo(8, 7, 6, 5)`. That really irks me.

So, as with assignment statements, I've decided to only have post increments and decrements, and that they should only be statements. There is no need for pre-increment/decrements:

```
  x++;
  ++x;   // Same as x++
```

In terms of implementation, I had a choice:

  * Add A_POSTINC and A_POSTDEC AST node operations, and get `genAST()` to deal with their code generation, or
  * In the parser, build a small AST tree with ASSIGN, ADD/SUBTRACT and NUMLIT 1 nodes which is then passed to `genAST()`.

I chose the latter. Either choice would have been fine!

## Logical AND and OR Operators

I wanted to keep in *alic* the [lazy evaluation](https://en.wikipedia.org/wiki/Lazy_evaluation) strategy that C has for these operators. We don't evaluate the right-hand expression if we already know the operation's result from the left-hand value.

Consider:

```
  x= 23;

  if (x >10 || x == 50) ...
```

The left-hand side is true, so there is no need to evaluate the right-hand side. This lazy evaluation makes it easier to work with pointers, e.g.

```
  if (ptr != NULL && ptr.member == 10)
    printf("pointer isn't NULL, pointer's member is 10\n");
```

If we evaluated both sides, the code would crash with a SEGFAULT when we give it a NULL pointer.

The code to generate the lazy evaluation is in `gen_logandor()` in [genast.c](genast.c). It's a bit ugly. We generate labels for QBE code that loads either a `true` or `false` value. Then we evaluate the left side and jump to one of the above labels as required before we try to evaluate the right side.

## Implementing `break` and `continue`

In the last part of the *alic* journey we had to introduce a stack of exception details, as `try()` statements could be nested. As loops in *alic* can be nested, we also need a stack of labels for `break` and `continue` to jump to. This is at the top of [genast.c](genast.c):

```
// We keep a stack of jump labels
// for break and continue statements.
typedef struct Breaklabel Breaklabel;
struct Breaklabel {
  int break_label;
  int continue_label;
  Breaklabel *prev;
};

static Breaklabel *Breakhead= NULL;     // The stack of Breaklabel nodes
```

Each time we start a new loop, we build and push a `Breaklabel` node onto the stack. Look for changes in `gen_WHILE()` that do this work.

The code for generating `break` and `continue` is thus:

```
  case A_BREAK:
    // Make sure we have a label to jump to
    if (Breakhead == NULL)
      fatal("can only break within a loop\n");
    cgjump(Breakhead->break_label);
    return(NOREG);
  case A_CONTINUE:
    // Make sure we have a label to jump to
    if (Breakhead == NULL)
      fatal("can only continue within a loop\n");
    cgjump(Breakhead->continue_label);
    return(NOREG);
```

There is one small but important issue with `continue` in `for` loops. For example:

```
  for (x= 1; x <= 10; x++) {
    if (x<5) continue;
    printf("x is %d\n", x);
  }
```

Intuitively, this should print `x` values 6, 7, 8, 9 and 10. But, remember, we append the `x++` code *after* the `printf()` code. So, if the `continue` takes us back to the top of the loop, we won't increment `x` and `x` will always be the initial value 1.

The problem is, in the *alic* compiler, I GLUE the `x++` statement code to the main loop body code, so there is no easy way to tell where the `x++` code is in the AST tree for the main body.

Or is there? So, I cheat! Over in the parsing code in `for_stmt()`:

```
  // Glue the end code after the statement block.
  // Set the rvalue true to indicate that the
  // right child is the end code of a FOR loop.
  // We need this to make 'continue' work in a FOR loop.
  s= mkastnode(A_GLUE, s, NULL, send);
  s->rvalue= true;
```

GLUE nodes are not expressions, so I can set the `rvalue` flag in a GLUE node to indicate which part of the tree is the third section in a `for` loop. We look for this flag when dealing with `for` loops in [genast.c](genast.c) and make sure a `continue` jumps to the right piece of code.

Yes it's slightly ugly but it works.

## The *alic* Switch Statement

Switch statements in *alic* look the same as C switch statements, but there is a **big** difference: cases do not fall through to the next case; instead, they jump to the end of the switch statement. If you want to fall through to the next case, you need to use the `fallthru` keyword. Also, the `break` statement is **not** used in a switch statement; it is only used for loops.

Here is an example *alic* program (test 98) that demonstrates the switch statement:


```
void main(void) {
  int32 x;

  for (x=1; x <= 9; x= x + 1) {
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

Its output is this:

```
case 1, default
case 2, default
case 3
case 4
fallthru to ...
case 7
case 5
fallthru to ...
case 7
case 6
case 6 does not fall through!
case 7
case 8, default
case 9, default
```

## Implementation of Switch

As with loops, `switch` statements can nest. Again, we need a stack to keep track of the details (top of [genast.c](genast.c)):

```
// We keep a stack of "next case"
// labels for switch statements
typedef struct Switchlabel Switchlabel;
struct Switchlabel {
  int next_label;
  Switchlabel *prev;
};
```

Any `fallthru` has to jump to the code that performs the next `case` in the `switch` statement. The `Switchlabel` node tracks this information.

The `gen_SWITCH()` function in [genast.c](genast.c) does the QBE code generation for `switch` statements. I'll just give the comments from the function:

```
  // Build a Switchlabel node and push it on to
  // the stack of Switchlabels
  // Create an array for the case testing labels
  // and an array for the case code labels
  // Because QBE doesn't yet support jump tables,
  // we simply evaluate the switch condition and
  // then do successive comparisons and jumps,
  // just like we were doing successive if/elses
  // Generate a label for the end of the switch statement.
  // Generate labels for each case. Put the end label
  // in as the entry after all the cases
  // Output the code to calculate the switch condition.
  // Get the type so we can widen the case values
  // Walk the right-child linked list
  // to generate the code for each case
  for (<each case statement>) {
    // Output the label for this case's test
    // If this is not the default case {
      // Jump to the next case test if the value doesn't match the case value
      // Otherwise, jump to the code to handle this case
    }
    // Output the label for this case's code
    // If the case has no body {
       // Jump to the following case's body
    } else {
      // Before we generate the code, update the Switchlabel
      // to have the label for the next case code, in
      // case we do a fallthrough in the body
      // Generate the case code
      // Always jump to the end of the switch (no fallthrough)
    }
  }
  // Now output the end label and pull the Switchlabel from the stack
```

It was somewhat fiddly to get right. I banged my head for a while before I got it right!

## Conclusion and The Next Step

Well, I wanted to add array access with pointers in this step. Then I realised I would probably have to `malloc()` an area to hold array values, so I needed `sizeof()`. And then I just decided I'd keep going and add more C-like functionality to *alic* as well as the array access.

So, while this was a big step in terms of changes, I haven't added much that is different to C: just `++` as statements and `fallthru` instead of `break`.

We are now up to 5,300 lines of code in the compiler, and I now feel that *alic* is nearly a useful language! I definitely want to try to rewrite the compiler in *alic* so that I can make a self-compiling compiler. This was some of the impetus for adding all the C-like functionality.

No promises, but I will *try* to add array declarations, initialisations and operations to the next part of the *alic* journey.
