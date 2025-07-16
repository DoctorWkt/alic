# *alic* Part 19: A `string` Type and Some Bug Fixes

Way back when I first starting thinking about writing an "improved C" language, I scribbled down some ideas. One of them was "What about strings?".

A good question! Back then I didn't really have an idea what I wanted to do. I certainly didn't want an entirely new type like Java's `String` class with its own set of functions.

Having got this far into *alic*, I now have a clear idea what I want to do. It's going to be a programmer's aid, like `const`. If a programmer chooses to use it, it should help reduce undefined behaviour.

Here's the outline.

## The `string` Type

In *alic*, the `string` type is identical to the `int8 *` type with some limitations:

  * You can't modify the contents of a string with any form of dereferencing;
  * You can't increment or decrement an existing string value; and
  * You can't go past either end of a string using array dereferencing.

That's it. Let's look at some examples:

```
  string str = "Hello, world!\n";           // Allowed
  str = "Another string\n";                 // Allowed
  printf("%s %c %c\n", str, *str, str[2]);  // Allowed

  *str= 'G';                                // All of these are
  str[3]= 'H';                              // not allowed
  str++;
  *str++;
  str= str + 5;
  printf("%c\n", str[-1]);                  // These two will cause a
  printf("%c\n", str[1000]);                // runtime check and crash
```

Of course, if I'd chosen to use `int8 *` instead of `string`, then all the operations above would be permitted.

I will also extend `foreach()` so it will iterate across the characters in a `string`.

That's the plan for the *string* type in *alic*. Before we get there, I have a few accumulated bugs to fix.

## Bug #1: User-defined Pointer Types Don't Work

Example:

```
type FOO = int32 *;

public void main(void) {
  int32 x= 23;
  FOO y;
  y= &x;
}
```

Then:

```
$ ./alic -S x.al 
x.al line 6: Incompatible types int32 * vs int32
```

I tracked this down to the code in `type_declaration()` in [parser.c](parser.c). For some reason, I was parsing the existing type (after the '=' sign) and building a new type with the given new type name, e.g. above, make a `FOO` type the same as `int32`. Then the code was building another `FOO` type with the pointer depth of 1. We were ending up with two `FOO` types! Bizarre. I obviously had a glass too many of red wine that night. I've rewritten the code.

## Bug #2: No Return Value in non-`void` Functions

I wasn't checking that a function with a non-`void` return value actually did return a value. In [parser.c](parser.c) look for a `bool` variable called `value_returned`. It is set to `false` in `function_declaration()` and, if still false at the end of this function, will cause a fatal error. The `return_stmt()` code sets the flag `true` if there is a `return` with a value.

## Bug #3: Need Identifier Check after '.'

I didn't have a check for an identifier after a '.' in `postfix_variable()` in [parser.c](parser.c). This was crashing the compiler when I had some other token like a number after the '.' token. Now fixed.

## Bug #4: Bad Test for Not Enough Struct Initialisers

If a struct has four members and we initialise a variable of that type, we need four values in the list. I actually have a test for this in `check_bel()` in [parser.c](parser.c):

```
  // No list, we ran out of values in the list
  if (list == NULL)
    fatal("Not enough values in the expression list\n");
```

But, stupidly, I did this later on:

```
  // This is a struct.
  // Walk the list of struct members and
  // check each against the list value
  if (is_struct(ty)) {
    for (memb = ty->memb; memb != NULL && list != NULL;
         memb = memb->next, list = list->mid) {
      check_bel(memb, list, offset + memb->offset, false);
    }
```

I was stopping the loop when the `list` of values was NULL, so the above `fatal()` call was never being triggered! I've taken out the `list != NULL` test and it now works.

## Bug #5: A Missing Range Test

I'd forgotten about `return()` statements and I was letting this through:

```
type FOO = int32 range 0 ... 100;

FOO fred(void) {
  return(1000);
}
```

So, in `genAST()` in [genast.c](genast.c) I added:

```
   case A_RETURN:
     // If the return type has a range, check the value
     if (has_range(Thisfunction->type)) {
       functemp = add_strlit(Thisfunction->name, true);
       cgrangecheck(lefttemp, Thisfunction->type, functemp);
     }
    cgreturn(lefttemp, Thisfunction->type);
```

Except that didn't do the range check! Another bug. In my code to test if a type has a range:

```
// Return true if a type has a limited range
bool has_range(Type *ty) {
  return(ty->lower != 0 || ty->upper != 0);
}
```

I'd written `&&` not &#124;&#124; and so the test was failing if one range value was zero. Sigh!

## Bug #6: `continue` Not Working in `foreach` Loops

Yes, I had noticed this when I was adding `foreach` loops to the [cina/](cina/) compiler. One of them had a `continue` inside a `foreach` loop and QBE didn't like the resulting code.

When I wrote the `foreach` code, I had correctly glued the "increment/next" code to the loop's statement block in `foreach_stmt()` in [parser.c](parser.c). What I'd forgotten to do was to mark this as the place where `continue` had to jump to. The code now has this:

```
  // Glue the change statement to s.
  // Set is_short_assign true to indicate that the
  // right child is the end code of a FOR loop.
  s = mkastnode(A_GLUE, s, NULL, send);
  s->is_short_assign = true;               // The missing line
```

And that's all the bugs I currently know of. We are up to 8,000 lines of code, so using the usual "1 bug per 100 LOC" that means only another 74 bugs to fix!

On to adding a `string` type.

## Making a `string` Type

The first thing to do is to add a new built-in type in [alic.h](alic.h):

```
// Type kinds
enum {
  TY_INT8, TY_INT16, TY_INT32, TY_INT64, TY_FLT32, TY_FLT64,
  TY_VOID, TY_BOOL, TY_STRING, TY_USER, TY_STRUCT, TY_FUNCPTR
};
```

and create a Type node which holds the information about this type. When I did this, I noticed that in [types.c](types.c) we already had an unnamed `int8 *` type for use with string literals:

```
Type *ty_int8ptr = &(Type) { TY_INT8, 8, false, 1 };	// Used by strlits
```

I was worried that I needed to keep this and add a second node for `string`s. After running all the tests, I'm relieved to say that I can replace the above with:

```
Type *ty_string  = &(Type) { TY_STRING, 8, false, 1 };
```

There are a few more changes to [types.c](types.c). One is that a dereferenced `string` is of `int8` type:

```
// Given a type pointer, return a type that
// represents the type that the argument points at
Type *value_at(Type * ty) {
  ...
  // *string becomes int8
  if (ty == ty_string) return(ty_int8);
  ...
}
```

And we should be able to interchange `int8 *` and `string` types with some care:

```
// Converting an AST node's type to ty
ASTnode *widen_type(ASTnode * node, Type * ty, int op) {
  ...
  // string and int8 * are compatible.
  // But we always return string to ensure string's limitations
  if (node->type == pointer_to(ty_int8) && ty == ty_string) {
    node->type= ty_string;
    return(node);
  }

  if (node->type == ty_string && ty == pointer_to(ty_int8))
    return(node);
  ...
}
```

We now need to add a "string" keyword to the lexer, which we've done before so I won't go through the changes in [lexer.c](lexer.c).

## Parsing a `string` Type

We have a grammar change to use this new keyword:

```
builtin_type=  'void'  | 'bool'   | 'string'
             | 'int8'  | 'int16'  | 'int32'  | 'int64'
             | 'uint8' | 'uint16' | 'uint32' | 'uint64'
             | 'flt32' | 'flt64'
```

and in `builtin_type()` in [parser.c](parser.c):

```
  case T_STRING:
    t = ty_string;
    break;
```

That's it for the syntax changes. Now we need to put `string` to work.

## Enforcing the `string` Semantics

As expected, most of the semantic rules live in [parser.c](parser.c) and this is the file with the most modifications. We have to ensure we cannot modify the contents of a `string` nor move what a `string` points to up or down.

In `short_assign_stmt()` which does the majority of the assignment work:

```
  // Cannot modify a string. This catches *str = 'c'; and str[3]= 'c';
  if (v->op == A_DEREF && v->left->type == ty_string)
        fatal("Cannot modify a string or its contents\n");
  ...
  if (Thistoken.token == T_POSTINC) {
    // Cannot increment or decrement a string. This catches str++;
    // There is the same code in the T_POSTDEC if statement.
    if (v->type == ty_string)
      fatal("Cannot modify a string or its contents\n");
  }
```

In `additive_expression()` which deals with addition and subtraction:

```
  case T_PLUS:
      scan(&Thistoken);
      right = multiplicative_expression();
      ...
      if (left->type == ty_string || right->type == ty_string)
        fatal("Cannot modify a string or its contents\n");
```

The same test is in the `T_MINUS` code.

We need to modify the code when we are building a string literal AST node in `primary_expression()`:

```
  case T_STRLIT:
    // Build an ASTnode with the string literal and ty_string type
    f = mkastleaf(A_STRLIT, ty_string, false, NULL, 0);
    ...
```

That's it for the compile-time checks. Now we need to add some run-time checks to ensure that we don't try to access characters off either end of a `string`.

## Run-time Checks on `string`s

We are doing an array element access for any type when we are processing the `A_ADDOFFSET` AST node. In `genAST()` in [genast.c](genast.c) we now have:

```
  case A_ADDOFFSET:
    // Do a runtime check on a string's length
    if (n->type == ty_string) {
      functemp = add_strlit(Thisfunction->name, true);
      cg_stridxcheck(lefttemp, righttemp, functemp);
    }
    return (cgadd(lefttemp, righttemp, n->type));
```

`cg_stridxcheck()` is a new function in [cgen.c](cgen.c) which outputs the QBE code to do the check. I'll just go through the comments below:

```
// Runtime check that the offset into a string is OK
void cg_stridxcheck(int idxtemp, int basetemp, int funcname) {

  // Check that the base address isn't NULL

  // Check that the index isn't negative

  // Get the string's length

  // Check that the index is below the length

  // Output the call to .fatal() if the range checks fail

  return;
}
```

## Doing `foreach` on a `string`

As with the other `foreach` loops, I've got to decide if I will hand-build the equivalent AST tree or hand-generate the QBE code. So I wrote this code down:

```
string x = "Hello there";
  ...
  int8 ch;
  foreach ch (x) { printf("%c\n", ch); }
```

and then translated it into existing *alic* code:

```
  int8 ch;
  int8 *hidptr;

  if (x != NULL) {
    for (hidptr= x; *hidptr != 0; hidptr++) {
      ch= *hidptr;
      printf("%c\n", ch);
    }
  }
```

which results in well over a dozen AST nodes, probably closer to twenty. So I have chosen to write a function in [cgen.c](cgen.c) which will create the equivalent QBE code for the loop. It's called `cg_stringiterator()`. It's quite similar to the function iterator code in `cg_funciterator()`; the comments in both should be enough to explain what they are doing.

## Another Bug: `continue` Doesn't Work in Some `foreach` Loops

After adding the `foreach` loop as a string iterator, I realised that it didn't cope with the use of `break` and `continue` inside the loop body. The same is true with function iterators; the reason is that I'm hand-writing the QBE code, and I wasn't outputting a label just before the loop increment which is where `continue` could jump to.

I'd already fixed some of the `foreach` loops (see bug #6 above). Now I needed to fix the hand-written QBE loops. What I've done with these iterator loops is, in their `genAST()` cases:

```
  case A_FUNCITER:
    // Add a Breaklabel node
    this = (Breaklabel *) Malloc(sizeof(Breaklabel));
    this->continue_label= genlabel();
    this->break_label= genlabel();
    this->prev = Breakhead;
    Breakhead = this;

    cg_funciterator(n, this);

    // Remove the Breaklabel node
    Breakhead = this->prev;
    return(NOTEMP);
  case A_STRINGITER:
    // Ditto but call cg_stringiterator()
```

and I pass the `Breaklabel` node `this` to the the two iterator functions in [cgen.c](cgen.c). In these functions, I output the labels in `this` where `break` and `continue` will jump to.

Then, in any loop body, `break` and `continue` will look at the labels held in the global `Breakhead` variable which hold the labels of the most recent loop.

## Is The `string` Type Useful?

I am going to say: definitely yes. I found all the places in the *alic* version of the compiler which had `char *` (i.e. same as `int8 *`) and replaced all of them with `string`. 

When I tried to compile the compiler with the changes, it quickly pointed out all the places where I was modifying the contents of a string or incrementing a string pointer. The two main places were changing the suffix of a filename in [main.c](main.c), and scanning in string & numeric literals in [lexer.c](lexer.c) where I was adding characters to a buffer.

All the places where I successfully changed over to `string` are now protected: if I add code that inadvertently modifies the string, the compiler will let me know!

There wasn't a good place to use the string iterator `foreach` loop in the *alic* version of the compiler though.

## Conclusion and The Next Step

I'm quite happy that I finally found a sensible way of adding a `string` type to the *alic* language. Along the way I knocked half a dozen bugs out of the compiler.

Tests 198 to 207 check that the new `string` type works as intended.

Next up, I want to try extending *alic* to support N-dimensional arrays. Right now the grammar and compiler can only support 1-dimensional arrays. I've got an outline on paper for the sort of changes I'll need to make, but it's going to be a big step forward. I also have to worry about `sizeof()` and the array-walking `foreach` constructs along the way. No promises as to when this will drop!


