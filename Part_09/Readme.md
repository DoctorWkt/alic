# *alic* Part 9: Adding Structs and Unions to *alic*

In this part of the *alic* journey, I'll add structs and unions to
*alic*. As with the last part of the journey, I want to outline my
design rationale first.

## Rationale for Structs and Unions

I'm enjoying coming up with language ideas to add to *alic*. I have a
vague idea as to what I want. Then I see what grammar I can create to
express it. And then, I have to think of all the semantic issues that
go with the idea.

For structs and unions, I want the type name to come first (as we have done
with opaque types and type aliases). I don't want to have unions as types
by themselves: they should only be available inside a struct. And I don't
want to have unions to have a name, I just want their members to be visible.

As an example, here is a possible C structure:

```
typedef struct _foo {   // We have to come up with an intermediate
  int x;                // type name for the pointer below
  union {
    char  a;
    int   b;
    float c;
  } y;                  // y is a union type, this is annoying
  long z;
  struct _foo *next;    // We use the intermediate type name, not Foo
} Foo;
```

So why is `y` being of union type annoying? It's because we have to write:

```
  Foo this;
  this.y.c = 3.14;
```

later on. Luckily C allows us to write:

```
typedef struct _foo {
  union {
    char  a;
    int   b;
    float c;
  };
} Foo;
...
  Foo this;
  this.c = 3.14;
```

which gets around the problem. But C also allows us to write:

```
typedef union {
  char  a;
  int   b;
  float c;
} Bar;
...
  Bar that;
  that.c = 3.14;
```

So a) we don't need named unions and b) we don't need unions as types
because we can achieve the same thing with unions inside structs.

## The *alic* Grammar for Structs

Here is what I came up with to express the above ideas:

```
type_declaration= TYPE IDENT SEMI
                | TYPE IDENT ASSIGN type SEMI
                | TYPE IDENT ASSIGN struct_declaration SEMI
```

The first line parses opaque types, the second line type aliases and the third parses structs.

```
struct_declaration= STRUCT LBRACE struct_list RBRACE

struct_list= struct_item (COMMA struct_item)*

struct_item= typed_declaration
           | union_declaration

union_declaration= UNION LBRACE typed_declaration_list RBRACE
```

A struct declaration has the syntax `struct { ... }`, but then note that the struct items are separated by commas: in C, the separator is a semicolon.

A struct item can be either a normal typed declaration which we already have for functions, e.g. `int32 x`, or it can be a union declaration. This prevents union declarations being actual types.

A union declaration has the syntax `union { ... }`. It can only contain a list of typed declarations: this prevents us having unions inside unions.

I chose commas as separators so that I could re-use the existing parsing for typed declarations and typed declaration lists.

At this point of my work, I've added the parsing of structs and unions to *alic* but there's no semantic code for them yet. But the compiler does parse this example input:

```
type fred = struct {
  int32 a,
  flt32 b,
  union { flt64 x, int16 y, bool z },
  bool c
};
```

## Representing Structs and Unions

Each member in a struct or union has a name and a type, and we need a list of them. So we now have this structure in [alic.h](alic.h):

```
// For structs and unions, we keep a
// linked list of members and their type
typedef struct Memb Memb;

struct Memb {
  char *name;           // Name of this member
  Type *type;           // Type of this member
  int offset;           // Offset of this member from base address
  Memb *next;           // Next member in the struct
};
```

The `Type` struct now has an extra field: `Memb *memb;` to point at this list when the type is `TY_STRUCT`.

## Dealing with Member Alignment

In terms of member offsets, we have to ensure that they are aligned according to the requirements of the CPU that we are compiling down to. Generally, memory allocations of size X should be aligned at a memory address which is a multiple of X, for performance reasons. Example: an `int64` should start at a memory address which is a multiple of 8 bytes (64 bits is eight bytes).

This means that if we have:

```
type foo = struct {
  int8 x,
  flt64 y
};
```

then `x` will be at offset 0 and size 1, but `y` needs to start at offset 8 as it is size 8; we should not start it at offset 1.

There's a new function in [cgen.c](cgen.c):

```
// Given a type and the current possible offset
// of a member of this type in a struct, return
// the correct offset for the member
// requirement in bytes
int cgalign(Type *ty, int offset) { ... }
```

I'm not going through the code for this function. It's mostly borrowed from the *acwj* compiler, and you can read [my description of how it works](https://github.com/DoctorWkt/acwj/tree/master/34_Enums_and_Typedefs#symbol-table-lists-for-enums-and-typedefs) there.

We should also align all the members of a union to start at the same offset. To do this, we need to find the biggest member when we are adding the union and use it to decide the correct alignment.

In [parser.c](parser.c) there is now this function:

```
// Given a pointer to a newly-created struct type
// one or more typed declarations in an AST list,
// and the possible offset where the first member could start,
// add the declaration(s) as members to the type.
// isunion is true when we are adding a set of union members.
// Die if there are any semantic errors.
// Return the possible offset of the next member
static int add_memb_to_struct(Type *strtype, ASTnode *asthead,
                                int offset, bool isunion) { ... }
```

This is the code that deals with the creation of the `Memb` linked list and the alignments of all the members in the list. It's annoyingly ugly but there isn't much choice when you have to deal with all the above issues.

This function also deals with some semantic errors: we can't allow structs to be members of structs, nor can we allow opaque types to be struct members.

The code in `struct_declaration()` and `union_declaration(void)` do the parsing side of the work, and call `add_memb_to_struct()` each time a new member (or list of members) need to be added to a new struct.

## An Example Struct

Here's a struct with a bunch of members with very different sizes.

```
type fred = struct {
  int8  a,
  int16 b,
  bool  b2,
  int8 *ptr,
  bool  b3,
  union { flt64 x, int16 y, bool z },
  int32 c,
  int64 d,
  bool  b4
};
```

I've added debug lines to the compiler, and it prints out:

```
int8   member a:   offset  0 size 1
int16  member b:   offset  2 size 2
bool   member b2:  offset  4 size 1
int8 * member ptr: offset  8 size 8
bool   member b3:  offset 16 size 1
flt64  member x:   offset 24 size 8
int16  member y:   offset 24 size 2
bool   member z:   offset 24 size 1
int32  member c:   offset 32 size 4
int64  member d:   offset 40 size 8
bool   member b4:  offset 48 size 1
struct total size is 49
```

## A Slight Step Sideways: Global Variables

I'm taking a slight sideways step away from structs to add declarations of global variables to *alic*. At the moment, we can have local variables and they must be assigned when they are declared:

```
statement_block= LBRACE declaration_stmts procedural_stmts RBRACE

declaration_stmts= (typed_declaration ASSIGN expression SEMI)*
```

For now, I think I'm going to have to make the assignment in local declarations optional, because when we want to declare a local struct, we will need some syntax to fill in the fields. I don't feel like dealing with that right now!

So let's add both global and local variables which don't need initial values:

```
input_file= ( type_declaration
            | enum_declaration
            | global_var_declaration
            | function_declaration
            )* EOF
...
global_var_declaration= typed_declaration SEMI;
...
statement_block= LBRACE declaration_stmts procedural_stmts RBRACE

declaration_stmts= ( typed_declaration ASSIGN expression SEMI
                   | typed_declaration SEMI
                   )*
```

It could be that, later on, both local and global variable declarations are identical. If that happens, I'll change the grammar to use just one rule for both. But, right now, I can't assume that, so it makes sense to have different rules for local and global variable declarations.

The change for locals with no assignment is reasonably easy. Have a look at `declaration_stmts()` in [parser.c](parser.c) which starts with the expression pointer as NULL until there is an actual expression. I then modified `declaration_statement()` in [stmts.c](stmts.c) and `gen_local()` in [genast.c](genast.c) to deal with the possibly NULL expression.

## Parsing Global Variables

Both global variables and functions start with a typed declaration, e.g.

```
int x;
int fred(void) { ... }
```

I changed the main loop of `input_file()` in [parser.c](parser.c) to get the typed declaration. It then checks the next token. If it's a left parenthesis, we call `function_declaration()` with the variable holding the typed declaration. Otherwise, we call `global_var_declaration()` with the variable.

 `global_var_declaration()`  itself is trivial. Check that the symbol doesn't already exist; if not, just call `add_symbol()` with `isglobal` set to `true`.

## Implementing Global Variables

This ended up being reasonably simple. In `add_symbol()` in [syms.c](syms.c), if the `isglobal` argument is `true` we mark the symbol's visibility as `SV_GLOBAL` and also note that it has an address. Otherwise, it gets mark with `SV_LOCAL` visibility.

In `cgloadvar()` and `cgstorvar()` in [cgen.c](cgen.c), if a symbol is global we prefix it with the QBE '$' sigil, otherwise we use the QBE '%' sigil as the prefix.

## Accessing Struct Members

We are now up to modifying the grammar and the compiler to be able to access members in structs.
That means changing the definition of a variable:

```
variable= IDENT
        | IDENT DOT IDENT
```

Remember, we can't have structs in structs, so there can't be two or more dots between identifiers.

Accessing a struct member requires that we do this:

  * Get the base address of the struct
  * Add on the member's offset to the base
  * Use this as a pointer, and dereference it to get the member's value

We added pointer dereferencing in a previous part of the *alic* journey, and we can reuse the DEREF AST operation here. For example, if we have a variable `foo` of the struct type `fred` given above, with the `flt64` member `x` at offset 24, we would build this AST tree to get the value of `foo.x`:

```
     DEREF
      /
    ADD
   /   \
  ADDR  NUMLIT 24
  /
IDENT foo
```

I also altered the grammar for `variable` to also allow for dereferencing pointers:

```
variable= IDENT
        | STAR IDENT
        | IDENT DOT IDENT
```

The code for `variable()` in [parser.c](parser.c) now handles normal variables, dereferencing pointers and member access. For the latter, you will see that the code does this:

```
  // Make an IDENT leaf node with the identifier in Thistoken
  ASTnode *n= mkastleaf(A_IDENT, NULL, false, NULL, 0);
  n= mkident(n);                // Check variable exists, get its type
  ...
  // If the variable is a struct (not a pointer), get its address
  if (n->type->ptr_depth == 0) {
    n->op= A_ADDR;
    ...
  }
  ...
  // Make a NUMLIT node with the member's offset
  off= mkastleaf(A_NUMLIT, ty_uint64, true, NULL, memb->offset);

  // Add the struct's address and the offset together
  n= binop(n, off, A_ADD);
  ...
  // Now dereference this address
  n= mkastnode(A_DEREF, n, NULL, NULL);
  n->rvalue= true;
```

## Assigning to Struct Members

Note the last piece of code above: `n->rvalue= true;`. We assume that the variable is going to be part of an expression, so we mark that it is an ["rvalue"](https://en.wikipedia.org/wiki/Value_(computer_science)#lrvalue): a temporary value that does not persist beyond the expression that uses it. We definitely want to do the dereferencing to get the member's value in an expression.

However, when a variable is on the left-hand side of an assignment, e.g. `foo.x = 23.5;`, we don't want to do the dereferencing; instead, we need the pointer to the member so that we can write a value to this address.

How do we ensure that this happens?

In `assignment_statement()` in [stmts.c](stmts.c), we do:

```
// Given an ASTnode v which represents a variable and
// an ASTnode e which holds an expression, return
// an A_ASSIGN ASTnode with both of them
ASTnode *assignment_statement(ASTnode * v, ASTnode * e) {
  ...
  // Put the variable on the right so that it
  // is done after we get the expression value
  this = mkastnode(A_ASSIGN, e, NULL, v);

  // Ensure that the variable is not an rvalue
  v->rvalue= false;
  ...
}
```

Now the variable isn't an rvalue. And in the `genAST()` code in [genast.c](genast.c):

```
  case A_DEREF:
    // If we are an rvalue, dereference to get the value we point at,
    // otherwise leave it for A_ASSIGN to store through the pointer
    if (n->rvalue == true)
      return(cgderef(lefttemp, value_at(n->left->type)));
    else
      return(lefttemp);
  }
```

## Tests for the Above and for `malloc()`


Test 60 to 67 in the [tests/](tests/) directory test all of the new functionality in this part of the *alic* journey. Test 65 checks that we can assign to struct members and then retrieve the values from the members.

Now that I've added dereferenced pointers as variables in assignment statements, test 67 checks that we can `malloc()` an area of memory and assign a value to the base of this memory area.

I did have a problem with parsing this function prototype:

```
void free(void *ptr);
```

because the old parsing code expected to see a ')' immediately after the `void` keyword. I've changed `function_prototype()` in [parser.c](parser.c) to deal with `void *`.

I also need to relax the type compatibility code in `widen_type()` in [types.c](types.c) as it wouldn't allow:

```
  int8 *ptr;
  ...
  free(ptr);    // Previously, cannot 'widen' int8 * to be void *
```

## A Comment on "Mopping Up"

I have no idea if anybody is reading this bit; if you are, congratulations on your perseverance!

I just wanted to make a comment about "mopping up". Right now, I'm in the process of adding new functionality in each part of the journey. This is all good and fine. But, at some point, I will need to go back and "mop up".

There are definitely semantic actions that I need to either allow or prevent that I'm not currently doing. For example, we can have pointers to an opaque type:

```
type FILE;
FILE *filehandle;
...
  // I don't know if this is currently allowed or not
  *filehandle = 0;
```

We should not allow dereferencing pointers to an opaque type, but I'm sure I haven't added this check yet. So, for the moment, the compiler works but is definitely fragile.

Back in the [acwj journey](https://github.com/DoctorWkt/acwj), I did a lot of mopping up when I was trying to get the compiler to compile itself. I'm sure I will do the same here.

## Conclusion and The Next Step

This was a pretty big step in the *alic* journey and I'm sure that I touched nearly every file in the compiler. There were a few times when I had a crisis of confidence: I couldn't see the way forward to get something done. That will happen again, I'm sure!

The next step (which I've just completed) is to add a form of
exception handling to *alic*, another non-C idiom for the language!
