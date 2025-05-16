# *alic* Part 8: Opaque Types, Type Aliases, Enumerated Values

I've started work on user-defined types in this part of the *alic* journey. Before I describe what I've done, I want to outline my rationale first.

## C Annoyances

First up, I really don't like that enums in C can have names, e.g.

```
enum fred { a, b, c };                  // a is 0, b is 1, c is 2
enum foo  { d=2, e=6, f };              // d is 2, e is 6, f is 7
```

I think that enums are just a way of defining constant integer literals. I want to aim for only allowing this in *alic*:

```
enum  { a, b, c };                  // a is 0, b is 1, c is 2
enum  { d=2, e=6, f };              // d is 2, e is 6, f is 7
```

Next up, I really find it annoying that the new type name in a C `typedef` comes last. It means we have to do this sort of thing:

```
typedef struct _intlist {
  int value;
  struct _intlist *next;       // Can't use Intlist as we've not seen it yet
} Intlist;
```

I'd much rather something like:

```
type Intlist = struct {        // Now the type name exists, we can use it
  int32 value;
  Intlist *next;
};
```

So, in *alic*, I'm going to declare user-defined types using the grammar as shown in the last example, with a `type` keyword starting the declaration.

## Opaque Types

I mentioned the idea of an opaque type in part six of the *alic* journey. An example would be:

```
type FILE;
```

This has the meaning: `FILE` is a user-defined type. We don't have any details about its internals, but it does exist. We cannot see inside it, but we can get a pointer to a `FILE` and we can pass this pointer around.

The idea is that we can write library functions which hide the details of the structures they use. Nobody really needs to see the internals of the `FILE` struct; we just need to be able to use `fopen()`, `fclose()`, fread()`, fwrite()`, `fprintf()` etc.

To do this, I've modified the *alic* grammar again so that we can declare types globally, i.e. outside a function declaration:

```
input_file= ( type_declaration
            | function_declaration
            )* EOF

type_declaration = TYPE IDENT SEMI
                 | TYPE IDENT ASSIGN type SEMI

function_declaration= function_prototype statement_block
                    | function_prototype SEMI
```

We can now have any number of global type or function declarations.

A type declaration can either be the keyword `type` followed by a new type name (i.e. an opaque type), or we can *alias* an existing type to be known by a new type name. I'll get on to type aliases soon.

We now have a new function in [parser.c](parser.c):

```
static void type_declaration(void) {
  char *typename;
  Type *basetype;
  Type *ptrtype;

  // Skip the TYPE keyword
  scan(&Thistoken);

  if (Thistoken.token != T_IDENT)
    fatal("Expecting a name after \"type\"\n");

  // Get the type's name
  typename= Thistoken.tokstr;

  // Skip the identifier
  scan(&Thistoken);

  // If the next token is an '='
  if (Thistoken.token == T_ASSIGN) {
     // I will discuss this soon!
     ...
  } else {

    // Add the opaque type to the list
    new_type(TY_USER, 0, 0, typename, NULL);
  }

  // Get the trailing semicolon
  semi();
}
```

There is a new kind of type, `TY_USER`, and we have modified the `Type` structure to have a name and a base type:

```
struct Type {
  TypeKind kind;
  int size;             // sizeof() value
  bool is_unsigned;     // unsigned or signed
  int ptr_depth;        // Number of derefs to base type
  char *name;           // Name of user-defined type
  Type *basetype;       // Pointer to the base type if this is an alias
  Type *next;
};
```

The `new_type(TY_USER, 0, 0, typename, NULL);` code adds a new Type node to the list of known types, with size zero, zero pointer depth, the given name and no base type).

Because an opaque type isn't built-in and has no size, we can't actually use it by itself. For example:

```
type FRED;

void main(void) {
  FRED x=0;
}
```

causes the error message "FRED not a built-in type". But because NULL is a built-in value of type `void *`, we can do this instead:

```
type FRED;

void main(void) {
  FRED* x=NULL;
```

We can now write an *alic* program that looks like this:

```
type FILE;

void printf(...);
FILE *fopen(int8 *fmt, int8 *mode);

void main(void) {
  FILE *infh= fopen("file", "w");
}
```

## Changes to the Type Code in *alic*

Now that types can have user-defined names, we need to be able to search for them. We already have a list of types with `Typehead` pointing to the start of the linked list.

I've modified `find_type()` to also search for a type's name if it is provided. When we are searching for a name, we walk the type list comparing names, and return when we have a name match and a pointer depth match (so we can find `FILE *` which has pointer depth 1).

When we added pointers and the '*' and '&' operators in the last part of the journey, I introduced `pointer_to()` and `value_at()` in [types.c](types.c) to get the Type pointer for a pointer to a type or the type that a pointer points at. These have been changed to use the rewritten `find_type()` function.

Now, in the parser, we now have to be able to parse user-defined types when we come across them in the input file. The `type()` function in [parser.c](parser.c) now looks like this:

```
// Return a pointer to a Type structure
// that matches the current token, or
// NULL if the token isn't a known type.
// If checkonly is set, recognise the token
// as a type but don't absorb it.
static Type* type(bool checkonly) {
  Type *t=NULL;
  char *typename=NULL;

  // See if this token is a built-in type
  switch(Thistoken.token) {
  case T_VOID:   t= ty_void;   break;
  ...
  case T_IDENT:  typename= strdup(Thistoken.tokstr);
                 t= find_type(typename, TY_USER, 0);
  }

  // Stop now if we are only checking for a type's existence
  if (checkonly)
    return(t);

  // We don't recognise it as a type
  if (t==NULL)
    fatal("Unknown type %s\n", get_tokenstr(Thistoken.token));

  // Get the next token
  scan(&Thistoken);

  // Loop counting the number of STAR tokens
  // and getting a a pointer to the previous type
  while (Thistoken.token== T_STAR) {
    scan(&Thistoken); t= pointer_to(t);
  }

  return(t);
}
```

If we have an identifier as a type name (as opposed to a built-in type keyword), we use `find_type()` to search for it. And when we are counting the trailing '*' characters, we use `pointer_to()` to return the matching Type with an increasing pointer depth.

This has the side effect of possibly adding extra Type nodes to the type list with all intermediate depth levels.

For example, if we declare:

```
  int8 ***foo = NULL;
```

then we will add three extra Type nodes to the type list. All will be of TY_INT8 type and they will have depths 1, 2 and 3.

Now, why do we need the new `checkonly` argument to the `type()` function? It's because we need to see if an identifier is a user-defined type without skipping past it.

Previously, `statement_block()` started like this:

```
  // A declaration_stmt starts with a type, so look for one.
  // XXX This will need to be fixed when we have user-defined types
  if (Thistoken.token >= T_VOID && Thistoken.token <= T_FLT64)
    d= declaration_stmts();
```

Note my `XXX` warning that I need to fix this. Well, that's what I've done:

```
  // A declaration_stmt starts with a type, so look for one.
  if (type(true)!=NULL)
    d= declaration_stmts();
```

Finally, in order to find type aliases I've had to add all the built-in Type nodes to the type list. We now have this function in [types.c](types.c) that is called when the compiler starts:

```
// Initialise the type list with the built-in types
void init_typelist(void) {
  Typehead =        ty_voidptr;
  ty_voidptr->next= ty_int8ptr;
  ty_int8ptr->next= ty_void;
  ...
  ty_flt32->next=   ty_flt64;
  ty_flt64->next=   NULL;
}
```

Yes it's ugly but it works!!

## Type Aliases

The other grammar change I've added is to allow type aliases, e.g.

```
type char = int8;
type String = char *;
```

I've touched on the new `basetype` field in the Type nodes. Let's now look at the code in [parser.c](parser.c) that I haven't covered yet:

```
static void type_declaration(void) {
  char *typename;
  Type *basetype;
  Type *ptrtype;

  // Skip the TYPE keyword
  scan(&Thistoken);

  // Get the type's name
  typename= Thistoken.tokstr;

  // Skip the identifier
  scan(&Thistoken);

  // If the next token is an '=' we have a type alias
  if (Thistoken.token == T_ASSIGN) {
    // Skip the '='
    scan(&Thistoken);

    // Get the base type with no pointer depth
    basetype= type(true);
    if (basetype == NULL)
      fatal("Unknown base type in type declaration: %s\n",
                                get_tokenstr(Thistoken.token));

    // The base type might be followed by '*', so
    // parse this as well
    ptrtype= type(false);

    // Add the alias type to the list.
    // Make any pointer type at the same time
    new_type(TY_USER, basetype->size, 0, typename, basetype);
    if (basetype != ptrtype)
      new_type(TY_USER, ptrtype->size, ptrtype->ptr_depth,
                                typename, ptrtype);
  } else {
  ...
  }
  ...
}
```

The call to `type(true)` checks that the original type name exists without skipping it. The call to `type(false)` gets the type with any pointer depth. We add at least the base alias to the type list, and a second node with a non-zero pointer depth if required.

## New Include Files

To make use of this, I've added several new include files to the compiler. When you do a `$ make install`, these will get installed to the `/tmp/alic/include` directory. Some example lines in them are:

```
// In sys/types.h
type char = int8;
type int  = int64;

// In stddef.h
type size_t = int64;

// In stdio.h
type FILE;

void printf(...);
void fprintf(...);
FILE *fopen(char *fmt, char *mode);
size_t fwrite(char *ptr, size_t size, size_t nmemb, FILE *stream);
size_t  fread(char *ptr, size_t size, size_t nmemb, FILE *stream);
char *fgets(char *ptr, int size, FILE *stream);
int fclose(FILE *stream);

// In unistd.h
int unlink(char *pathname);
```

And, with all of these changes, we can now compile and run this program (test 55):

```
#include <stdio.h>
#include <unistd.h>

void main(void) {
  FILE *outfh= NULL;
  FILE *infh= NULL;
  char *buf= "                       ";

  outfh= fopen("fred", "w");
  if (outfh == NULL) {
    printf("Unable to open fred\n");
    return;
  }

  printf("We opened fred!\n");
  fwrite("Does this work?\n", 16, 1, outfh);    // It does!
  fclose(outfh);

  infh= fopen("fred", "r");
  if (infh == NULL) {
    printf("Unable to read fred\n");
    return;
  }

  fgets(buf, 20, infh);
  printf(buf);
  fclose(infh);
  unlink("fred");
}
```

## Rewriting the Scope Code

I've subconciously known for a while that my symbol scope code wasn't right. So I've rewritten it! We now have a Scope type in [alic.h](alic.h):

```
// A scope holds a symbol table, and scopes are linked so that
// we search the most recent scope first.
typedef struct _scope {
  Sym *head;            // Head of the scope's symbol table
  struct _scope *next;  // Pointer to the next scope
} Scope;
```

and in [syms.c](syms.c):

```
static Scope *Scopehead = NULL; // Pointer to the most recent scope
static Scope *Globhead = NULL;  // Pointer to the global symbol table

// Initialise the symbol table
void init_symtable(void) {
  Scopehead= (Scope *)calloc(1, sizeof(Scope));
  Globhead= Scopehead;
}
```

When we start `init_symtable()` sets up both `Scopehead` and `Globhead`. The scope creating and removal functions now look like:

```
// Start a new scope section on the symbol table.
void new_scope(Sym *func) {
  Scope *thisscope;

  thisscope= (Scope *)calloc(1, sizeof(Scope));
  thisscope->next= Scopehead;
  Scopehead= thisscope;
}

// Remove the latest scope section from the symbol table.
void end_scope(void) {
  Scopehead= Scopehead->next;
  if (Scopehead == NULL)
    fatal("somehow we have lost the global scope!\n");
}
```

When we are adding a symbol, we need to know if it is global or in a local scope:

```
// Add a new symbol to the current or the global scope.
// Return a pointer to the symbol
Sym *add_symbol(char *name, int symtype, Type *type, bool isglobal) {
  Sym *this;

  if (isglobal)
    this= add_sym_to(&(Globhead->head), name, symtype, type);
  else
    this= add_sym_to(&(Scopehead->head), name, symtype, type);
  return (this);
}
```

The code to find a symbol now walks the Scope list and walks the Symbol list inside each Scope node. It's actually simpler than the previous code; I wish I'd written this first.

The new scope code also means that we can have nested local scopes inside a function, e.g. test 57:

```
void main(void) {
  int32 a= 4;
  int32 b= 3;

  if (a > b) {
    // We now have nested local scopes
    int32 c = 100;
    printf("We have a %d b %d c %d\n", a, b, c);
  }
}
```

And test 58 checks that a scope disappears once we end a statement block.

## Enumerated Values

I've also implemented enumerated in *alic*. These are not types *per se*; instead, they just give integer literal values globally visible names, e.g.

```
enum { a, b, c=25, d, e= -34, f, g };
```

sets `a` as a symbol name to 0, `b` to 1 etc.

The *alic* grammar now looks like this:

```
input_file= ( type_declaration
            | function_declaration
            | enum_declaration
            )* EOF

enum_declaration= ENUM LBRACE enum_list RBRACE SEMI

enum_list= enum_item (COMMA enum_item)*

enum_item= IDENT
         | IDENT ASSIGN NUMLIT
```

Note that the `enum_list` has to have at least one item in it; we can't write `enum { };`.

The code is in the `enum_declaration()` function in [parser.c](parser.c). It is pretty straight-forward. One thing to notice is that I call `parse_litval()` to get a type for the literal value, so as to find the smallest integer type for each one. With

```
enum { fred=127, jim };
```

`fred` will be of type `int8` but `jim` won't fit into that type: it will be `int16`.

The grammar is also changed to expect enum values:

```
primary_expression= NUMLIT
                  | STRLIT
                  | TRUE
                  | FALSE
                  | NULL
                  | variable
                  | ENUMVAL
                  | function_call
```

with the code in `primary_expression()` in [parser.c](parser.c) changed to be:

```
  case T_IDENT:
    // Find out what sort of symbol this is
    sym= find_symbol(Thistoken.tokstr);
    if (sym == NULL)
      fatal("unknown symbol %s\n", Thistoken.tokstr);
    switch(sym->symtype) {
    case ST_FUNCTION: f= function_call(); break;
    case ST_VARIABLE: f= variable(); break;
    case ST_ENUM: f= mkastleaf(A_NUMLIT, ty_bool,
                        true, NULL, sym->initval.intval);
                scan(&Thistoken);
                break;
    default:
      fatal("unknown symbol type for %s\n", Thistoken.tokstr);
    }
    break;
```

## Conclusion and The Next Step

This was a fairly big part of the *alic* journey. We have enumerated values, opaque types and type aliases. We now have header files and we can use some of the C library. And I improved the scope functionality and cleaned the code up at the same time.

In the next step I want to add structural types: structs and unions. They will differ slightly from the C version.
