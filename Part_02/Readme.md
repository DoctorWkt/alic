# *alic* Part 2: Simple Control Statements

In part two of my *alic* journey, I want to add some basic
control flow statements. I've chosen to implement IF & WHILE statements and
and a simple version of the FOR statement.

## Simple Control Statements

I was expecting that these would take a few days to add to the language, but in fact I was able to add them in an afternoon. There are two reasons for this: one is that I'm using a parser generator and the other is that I could borrow much of the implementation code from my
[acwj](https://github.com/DoctorWkt/acwj) compiler.

## Changes to the *alic* Grammar

Let's start with the changes I've made to *alic*'s grammar in [parse.leg](parse.leg). Firstly, I now produce a single AST tree for the whole input file. We call `genAST()` on the tree when we hit the end of the file:

```
input_file = s:statement_block EOF
        {
          if (debugfh != NULL) dumpAST(s, 0, 0);
          genAST(s);
        }
```

Instead of individual statements, we now have a statement block which resembles the body of a C function:

```
## Statements: a statement block has all the declarations first,
#  followed by at least one procedural statement.
#
statement_block = LCURLY declaration_stmt* procedural_stmts RCURLY
```

Note that the block starts and ends with `{` ... `}`. Also note that I've changed the grammar so that all the declaration statements have to come first; they can't be intermixed with other statements in the block. This means that all variables must be declared before they can be used.

The `*` in the `statement_block` rule means that there can be zero or more `declaration_stmt`s.

The defintion of the `procedural_stmts` rule caused me some grief. I have an AST operation called `A_GLUE` which glues AST sub-trees together. If we have three consecutive statements, then the final AST tree will look like:

```
         A_GLUE
        /      \
     stmt1     A_GLUE
    subtree   /      \
            stmt2   stmt3
          subtree   subtree
```

In order for the compiler to run the code to join sub-trees together, we need the *leg* grammar to have names for the sections in each rule on the right-hand side of the `=` sign. I originally tried this which didn't work:

```
procedural_stmts = l:procedural_stmt r:procedural_stmts*
        {
          if (r != NULL) l = binop(l,r,A_GLUE);
          $$ = l;
        }
```

Let's convert this into English. `procedural_stmts` consist of one `procedural_stmt` called `l` and zero or more `procedural_stmts` called `r`. Because of the `*`, there can be none, and so `r` should have no value. Hence the C code: glue `l` and `r` sub-trees together if `r` exists, otherwise just return the `l` sub-tree when there is no `r` sub-tree.

This grammar rule is recursive: `procedural_stmts` occurs on both sides of the `=` sign. And it will bottom out when we get to the end of a list of statements and there is only one statement left.

While I thought this was going to work, it failed. Somehow, *leg* was actually returning subtrees containing expressions into `r` instead of either a top-level statement or NULL. Argh!

The solution that does work (and what I'll use from now on for lists) is this:

```
procedural_stmts = l:procedural_stmt r:procedural_stmts
        | l:procedural_stmt
```

Remember that *leg* tries the options in order. So it first looks for a single `procedural_stmt` which is followed by more `procedural_stmts`. If this fails,
then it tries to find just a single `procedural_stmt`. It's still recursive, but it guarantees that we get `l` and `r` or just `l` by itself.

The full grammar rule with the associated C code is thus:

```
procedural_stmts = l:procedural_stmt r:procedural_stmts
        {
          // Glue left and right together if there are both
          $$ = binop(l,r,A_GLUE);
        }
        | l:procedural_stmt
        {
          // Otherwise just return left if there is no right
          $$ = l;
        }
```

## New Procedural Statements: WHILE

The set of procedural (not declaration) statements is now:

```
procedural_stmt = print_stmt
        | assign_stmt
        | if_stmt
        | while_stmt
        | for_stmt
```

with the last three the new statements. Let's start with the `while_stmt`.

```
while_stmt = WHILE LPAREN e:relational_expression RPAREN s:statement_block
        {
          $$= mkastnode(A_WHILE, NULL, false, e, s, NULL, NULL, 0);
        }
```

LPAREN and RPAREN are `(` and `)` respectively, and WHILE is the keyword `while`. You should be able to see that this would match the code `while ( x < 25 ) { x = x + 1; }`

`mkastnode()` builds an A_WHILE AST node with the expression `e` as the left child, the statement block `s` as the middle child, and it's marked as not being an r-value.

Running the example code above through the compiler, we get this AST tree (indenting shows the tree's levels sideways):

```
WHILE
  bool LT
    int32 IDENT rval x
    int8 NUMLIT 25
  int32 ASSIGN x
    int32 ADD
      int32 IDENT rval x
      int32 NUMLIT 1
```

Good, we have a sensible AST tree. Now we need to generate QBE intermediate code from the tree. We are going to need a label at the top of the statement block to jump back to, and we need a label after the statement block to jump to when the expression becomes false.

The code in `genAST()` in [genast.c](genast.c) now starts with this because we have to deal with AST operations that have labels and jumps:

```
// Given an AST, generate assembly code recursively.
// Return the temporary id with the tree's final value.
int genAST(ASTnode * n) {
  int lefttemp, righttemp;

  // Empty tree, do nothing
  if (n == NULL) return (NOREG);

  // Do special case nodes before the general processing
  switch (n->op) {
  case A_IF:
    gen_IF(n); return(NOREG);
  case A_WHILE:
    gen_WHILE(n); return(NOREG);
    ...
  }
  ...
}
```

And, so, onto the `gen_WHILE()` code in the same file:

```
// Generate the code for a WHILE statement
static void gen_WHILE(ASTnode * n) {
  int Lstart, Lend;
  int t1;

  // Generate the start and end labels
  // and output the start label
  Lstart = genlabel();
  Lend = genlabel();
  cglabel(Lstart);

  // Generate the condition code
  t1 = genAST(n->left);

  // Jump if false to the end label
  cgjump_if_false(t1, Lend);

  // Generate the statement block for the WHILE body
  genAST(n->mid);

  // Finally output the jump back to the condition,
  // and the end label
  cgjump(Lstart);
  cglabel(Lend);
}
```

Note that the labels `Lstart` and `Lend` get placed before the condition test and after the statement block, respectively. After `Lstart`, we execute the condition and place the result in temporary `t1` (1 is true, 0 is false). We then jump to `Lend` when the condition was zero (false). Finally, we generate the code for the statement block and a jump back to `Lstart`.

One quirk with QBE is that it doesn't have a set of "jump if X" instructions (e.g. jump if equal, jump if larger, jump if smaller than or equal to). It has a single "jump if not zero" instruction: `jnz`. My code for `cgjump_if_false()` runs the `jnz` instruction with an internally created label as well as the `Lend` label.

OK, this doesn't make sense. Let's look at the resulting QBE code for the `while ( x < 25 ) { x = x + 1; }` code:

```
@L1                               # The Lstart label
  %.t4 =w loadsw $x               # Load x's value
  %.t5 =w copy 25                 # and the number 25
  %.t6 =w csltw %.t4, %.t5        # Store the comparison x<25 in %.t6
  jnz %.t6, @L3, @L2              # If the comparison was 1 (true), jump to @L3 else to @L2
@L3                               # The label generated by cgjump_if_false()
  %.t9 =w loadsw $x               # The code for the statement block
  %.t10 =w copy 1
  %.t9 =w add %.t9, %.t10
  storew %.t9, $x
  jmp @L1                         # Jump back to the top of the loop
@L2                               # The Lend label
```


## New Procedural Statements: IF

The IF statement is tricky because there can be an optional ELSE clause. Let's look at the grammar for it:

```
if_stmt = IF LPAREN e:relational_expression RPAREN t:statement_block
                                            ELSE   f:statement_block
        {
          $$= mkastnode(A_IF, NULL, false, e, t, f, NULL, 0);
        }
        | IF LPAREN e:relational_expression RPAREN t:statement_block
        {
          $$= mkastnode(A_IF, NULL, false, e, t, NULL, NULL, 0);
        }
```
We use *leg*s ability to try the option with the ELSE clause out first before it tries the second option without the ELSE clause. The A_IF thus has two or three children: the relational expression, the statement block to run if the expression is true and (optionally) the statement block to run if the expression is false.

When we get to the `gen_IF()` code in [genast.c](genast.c) that generates the QBE code, we again need two labels. This time, they represent the start of the "expression is false" code and the end of the while IF statement. I won't go through the code because it's very similar to the `gen_WHILE()` code. But here's an example IF statement and the resulting QBE code:

```
if ( x < 25 ) { print 3; } else { print 5; }


  %.t4 =w loadsw $x
  %.t5 =w copy 25
  %.t6 =w csltw %.t4, %.t5     # if ( x < 25)
  jnz %.t6, @L3, @L1           # Jump to L3 if true, or L1 i.e Lfalse
@L3
  %.t9 =w copy 3               # Print 3 if true
  call $printint(w %.t9)
@L4
  jmp @L2                      # Then jump to L2 (Lend)
@L1                            # L1 is the Lfalse label
  %.t10 =w copy 5
  call $printint(w %.t10)      # Print 5 if false
@L2                            # L2 is the Lend label
```

## New Procedural Statements: FOR

Before I start on FOR, let's look at it in terms of quirks. It's really the same as a WHILE statement, e.g.

```
for (x=0; x < 10; x = x + 1) { print x; }
           is the same as
x= 0; while (x < 10) { print x; x = x + 1; }
```

In other words, we can convert it into a WHILE statement. However, there's a syntax issue: the third section in the parentheses does *not* end with a semicolon!

For this reason, I've altered the *alic* grammar for assignment statements:

```
for_assign_stmt = v:variable ASSIGN e:expression        # Note no ending semi-colon

assign_stmt = for_assign_stmt SEMI                      # But this one does have one!

for_stmt = FOR LPAREN i:assign_stmt e:relational_expression SEMI
                send:for_assign_stmt RPAREN s:statement_block
```

Note inside the LPAREN/RPAREN we have an `assign_stmt` (which ends with a semi-colon), a `relational_expression` followed by a semi-colon, and a `for_assign_stmt` (which doesn't end with a semi-colon).

Because the FOR statement can be converted into a WHILE statement, I've done exactly this in the associated C code:

```
          // Glue the end code after the statement block.
          // 'send' stands for "statement to put at end of statement block"
          s = binop(s,send,A_GLUE);
          // We put the initial code at the end so that
          // we can send the node to gen_WHILE() :-)
          $$= mkastnode(A_FOR, NULL, false, e, s, i, NULL, 0);
```

OK, it's kind of weird that I'm putting the initial assignment statement in as the right-hand child when it should come first. The reason for that is how we generate the QBE code in [genast.c](genast.c):

```
  // Do special case nodes before the general processing
  switch (n->op) {
  case A_IF:
    ...
  case A_WHILE:
    ...
  case A_FOR:
    // Generate the initial code
    genAST(n->right);

    // Now call gen_WHILE() using the left and mid children
    gen_WHILE(n); return(NOREG);
  }
```

It saves me having to write a `gen_FOR()` function which would be nearly identical to `gen_WHILE()`.

## A FOR Example

Now that we have FOR, we can write [tests/test025.al](tests/test025.al) which checks that our signed and unsigned types do overflow correctly:

```
{
  int8   a = 0;
  uint8  b = 0;
  int16  c = 0;
  uint16 d = 0;

  print a;
  for (a= 126;   a != -126;   a = a + 1) { print a; }
  for (b= 254;   b != 2;      b = b + 1) { print b; }
  for (c= 32766; c != -32766; c = c + 1) { print c; }
  for (d= 65532; d != 2;      d = d + 1) { print d; }
}

gives

0
126
127
-128
-127
254
255
0
1
32766
32767
-32768
-32767
65532
65533
65534
65535
0
1
```

## Conclusion and The Next Step

This ended up being a fairly short step, even though I said that
we were going to have fewer, bigger steps than *acwj*. Oh well.

Next up I want to change the language so that it has function declarations.
I've done some of the work already and it definitely will be a large step.
