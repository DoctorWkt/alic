# *alic* Part 21: Local Struct/Array Initialisation

In this part of the *alic* journey I am going to try and add initialisation of local structs and arrays to the compiler.

Sometimes I write these notes up once I've got everything working. This time I'm going to start writing them before I've made a single change. Let's see what happens!

## The Issues

With variables that live outside a function, their memory generally lives near the code section of the program and below the heap. There is only one instance of each variable and we can put the initial values into the program's executable to be loaded at runtime.

As a function can be called multiple times, there can be several instances of a local variable. As well, their initialisation often cannot be done at compile time. Consider this example:

```
int32 fred(int32 x) {
  int32 y = x + 3;
}
```

We have to calculate `x + 3` at runtime before we can intialise `y` in an assignment statement.

This is all fine for scalar variables, but the problem becomes bigger for arrays and structs. Consider:

```
int32 fred(int32 x) {
  int32 y[5]= { x+2, x+3, y[0] - 8, x * y[1], 12 - x };
}
```

Now we have five expressions to calculate and five assignments to perform. It's the same for structs, e.g.:

```
type FOO = struct { int32 a, int16 b, flt32 c };

public void main(void) {
  FOO fred = { 1, 2, 3.14 };
}
```

And what is the semantic result of this?

```
int32 fred(int32 x) {
  int32 y[5]= { y[0] + 1, y[1] + 2, y[2] + 3, y[3] + 4, y[4] + 5 };
}
```

Should we assume that the array is zero-filled before we start the initialisation process? That would definitely reduce undefined behaviour.

## What We Have Already

We have a function called `check_bel()` which walks a bracketed expression list like `{ 1, 2, 3.14 }`, compares it against an aggregate variable like a struct or array, widens each value as required, and then calls `cgglobsymval()` to output the QBE code to put the values into the final executable. I think we can modify this to also generate the QBE code to do the assignment to a local aggregate variable.

I've moved `check_bel()` from [parser.c](parser.c) and into [genast.c](genast.c) as the code doesn't do any parsing, but it does process the AST tree of a bracketed expression list and generates code from it.

At the moment, initialisation of scalar local variables (like `int32 x = 12;`) is done by `declaration_statement()` in [stmts.c](stmts.c). Right at the top:

```
  // Can't use bracketed expressions lists in functions
  if ((e != NULL) && (e->op == A_BEL))
    fatal("Cannot use a bracketed expression list in a function\n");
```

which we need to change. The function goes on to build an `A_LOCAL` AST tree with a pointer to the variable's `Sym` node and any intialisation expression.

The `A_LOCAL` tree is then passed to the `gen_local()` function in [genast.c](genast.c) which generates the code for the single expression and then stores the result in the variable.

## What We Need to Do

We need to:

  * Change `declaration_statement()` to allow `A_BEL` expressions to be added to the `A_LOCAL` AST node.
  * Change `gen_local()` to call `check_bel()` when we have an aggregate variable and a bracketed expression list.
  * Either in `gen_local()` or `check_bel()`, modify the code to determine the address offset of a struct member or array element, generate the code for each expression and store the result into the address offset from the base of the variable.

I think that there's some scope here for an optimisation: if we get the variable's base address into a QBE temporary, each time we calculate an offset from this (which can be done at compile time) we can add this to the base address temporary. Otherwise, we would get the base address for each and every assignment.

I also think that we still need to zero-fill each aggregate variable before we start running code for their initialisation (see above). A possible optimisation: only do this if the variable that we initialising is named in any of the initialisation expressions. Yes, but perhaps that is [premature optimisation](https://www.laws-of-software.com/laws/knuth/).

So this is my proposal for the work to be done. And now we try to make it happen! ...

## Step One: Allow Local Initialisation

I've removed the check to prevent bracketed expression lists ("BEL"s) in `declaration_statement()`. In the same function, we now only try to widen the given expression if the variable being declared is scalar:

```
  // Widen the expression's type if required
  // but only for scalars (was just e!= NULL before)
  if (e != NULL && s->is_array == false && !is_struct(s->type)) {
    newnode = widen_type(e, s->type, 0);
  ...
  }
```

So, now, an `A_LOCAL` AST node can contain a pointer to an aggregate symbol and an `A_BEL` expression (i.e. a BEL tree).

Over in `gen_local()` in [genast.c](genast.c), we originally went straight into evaluating the expression and storing it in the variable. Now we have this test:

```
  // Is this an aggregate variable?
  if (is_array(n->sym) || is_struct(n->sym->type)) {
    ...
  } else {
    // No, it's a scalar variable.
    // Get the expression's value
    // on the left if there is one
    if (...) {
      // Store this into the local variable
      ...
    }
  }
```

Now, for aggregate variables, we can get the base address and call `check_bel()` with this extra information:

```
  // Is this an aggregate variable?
  if (is_array(n->sym) || is_struct(n->sym->type)) {
    // Get the base address of the variable
    basetemp= cgaddress(n->sym);

    // Now walk any bracketed element list
    // and initialise the members/elements
    if (n->left != NULL)
      check_bel(n->sym, n->left, 0, false, basetemp);
  } else { ...
  }
```

Down in `check_bel()` we now have `basetemp` as an extra parameter. At the bottom of the function:

```
  // We are generating a non-local value
  if (basetemp == NOTEMP) {
    // It has to be a literal value
    if ((list->op != A_NUMLIT) && (list->op != A_STRLIT))
      fatal("Initialisation value not a literal value\n");

    // Update the list element's type
    list->type = ty;

    // Output the value at the offset
    if (O_logmisc)
      fprintf(Debugfh, "globsymval offset %d\n", offset);
    cgglobsymval(list, offset);
  }
```

So, in a non-local context, we pass in `NOTEMP` for `basetemp`, which calls `cgglobsymval()` to put the initial values in the program's executable. And, if we get an actual `basetemp` value (in the future), we will assign the expression's value to the `basetemp` address plus the `offset`.

That's the theory, anyway. Currently, all the existing tests except test 114 still pass, and the latter is testing that we can't use BELs in a function.

## Part Two: Getting It to Work

We need to add the base of the variable's address to an offset and store an expression's value at that address. I've created this function in [cgen.c](cgen.c) to do this:

```
// Store a value at the offset from the base
// of a struct or array
int cgstore_element(int basetemp, int offset, int exprtemp, Type *ty) {
  int temp = cgalloctemp();

  // Add the base and the offset
  fprintf(Outfh, "  %%.t%d =l add %%.t%d, %d\n", temp, basetemp, offset);

  // Store the expression value at that address
  return(cgstorderef(exprtemp, temp, ty));
}
```

Back in [genast.c](genast.c) in `gen_local()`, I've added code to ensure that aggregate variables will be zero-filled even if there is a BEL for them:

```
  // We have an initialisation and it's a scalar
  // variable, no need to zero the space
  if (n->left != NULL && !is_array(n->sym) && !is_struct(n->sym->type))
    makezero = false;
```

And, down in `check_bel()` at the bottom where we need to output the QBE code:

```
  // We are generating a non-local value
  if (basetemp == NOTEMP) {
    ...
  } else {
    // We are dealing with a local variable.
    // Generate the expression's code and get the value
    exprtemp= genAST(list);

    // and assign it into the aggregate variable at the offset
    cgstore_element(basetemp, offset, exprtemp, ty);
  }
```

And, with that, it seems to work!

## A Test Program

Here's my test program with its output:

```
public void main(void) {
  int32 mary = 45;
  int32 fred[3]= {5, 2, 3};
  int32 dave[3]= { fred[2] * mary, fred[1] + mary, mary / fred[0] };

  printf("mary is %d\n", mary);
  printf("We have three fred numbers %d %d %d\n",
        fred[0], fred[1], fred[2]);
  printf("We have three dave numbers %d %d %d\n",
        dave[0], dave[1], dave[2]);
}
```

Note that the initialisation list for `dave[]` includes `mary` and values from `fred[]`! The output is:

```
mary is 45
We have three fred numbers 5 2 3
We have three dave numbers 135 47 9
```

I've added this test and rewritten some of the original global initialisation tests to be local tests. They all work, so now I'm very happy!

## Part Three: Things Not to Worry About

I thought I'd try this out and see if it behaved "properly":

```
public void main(void) {
  int32 jim[5]= { jim[0] + 2, jim[1] + 8, jim[0] + jim[1],
                  jim[1] + jim[2], jim[2] + jim[4] };

  for (i=0; i < 5; i++)
    printf("jim[%d] is %d\n", i, jim[i]);
}
```

Well, it didn't even compile. My compiler gave me this error:

```
x.al line 7: Unknown symbol jim
```

Why? The answer is this. In `declaration_stmts()` in [parser.c](parser.c):

```
static ASTnode *declaration_stmts(void) {
  ASTnode *d, *e = NULL;
  ASTnode *this;

  // Get one declaration statement
  d = array_typed_declaration();

  if (d->is_inout== true)
    fatal("Only function parameters can be declared inout\n");

  // If there is an '=' next, we have an assignment
  if (Thistoken.token == T_ASSIGN) {
    e = decl_initialisation();
  }

  semi();

  // Declare that variable
  this = declaration_statement(d, e);
  ...
}
```

We get the variable's name and the BEL for it. Then we call `declaration_statement()` to declare it. Thus, at the point where we do `e = decl_initialisation();` the variable name is *not* in the symbol table!

Yes, I could rework the code to declare the variable before we get its initialisation. But I think I will leave this as it is, and remove the necessity to zero-fill aggregate local variables when they have a BEL to initialise them.

## Conclusion and The Next Step

This wasn't a big change, but I thought I'd show you my process for making changes to the compiler.

I also don't have a clue what I want to add to the language yet, so this might be the last part of the *alic* journey for a while. I hope you've enjoyed it!

