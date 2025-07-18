# *alic* Part 5: A Hand-Written Lexer and Parser

Well, here we are in part five of my *alic* journey with an unchanged
language. I've removed the *leg* parser generator and I've hand-written
a lexer and recursive descent parser for the compiler.

Obviously, the number of lines in the *leg* grammar is fewer than the number of lines in the hand-written C version:

```
$ wc parse.leg            (Part 4)
 288 1123 7448 parse.leg

$ wc lexer.c parser.c     (Part 5)
  592  2025 13116 lexer.c
  704  2051 17807 parser.c
 1296  4076 30923 total
```

But the size of the resulting machine code is now much smaller:

```
$ size parse.o          (Part 4)
   text    data     bss     dec     hex filename
  33872       8      96   33976    84b8 parse.o

$ size lexer.o parser.o (Part 5)
   text    data     bss     dec     hex filename
   5924     912     616    7452    1d1c lexer.o
   4922       0       0    4922    133a parser.o
  10846     912     616   12374         TOTAL
```

## The *alic* Grammar

Now that we don't have a *leg* file with the grammar for *alic*, I've added the grammar
as a set of comments in the [parser.c](parser.c) file. You can do a `$ make grammar` to
print this out:

```
$ make grammar

input_file= function_declarations EOF

function_declarations= function_declaration*

function_declaration= function_prototype statement_block
                    | function_prototype SEMI

function_prototype= typed_declaration LPAREN typed_declaration_list RPAREN
                  | typed_declaration LPAREN VOID RPAREN

typed_declaration_list= typed_declaration (COMMA typed_declaration_list)*

typed_declaration= type IDENT

type= built-in type | user-defined type

statement_block= LBRACE procedural_stmt* RBRACE
               | LBRACE declaration_stmt* procedural_stmt* RBRACE

declaration_stmts= (typed_declaration ASSIGN expression SEMI)+

procedural_stmt= ( print_stmt
                 | assign_stmt
                 | if_stmt
                 | while_stmt
                 | for_stmt
                 | function_call
                 )*
(etc.)
```

Uppercase words represent tokens that are generated by the lexical tokeniser in [lexer.c](lexer.c).

## Scanning Tokens from the Input

I've borrowed the lexical tokeniser [lexer.c](lexer.c) from part 63 of my [acwj](https://github.com/DoctorWkt/acwj) compiler. I won't go into much detail about how it works here, as you can read about it in [part 1](https://github.com/DoctorWkt/acwj/tree/master/01_Scanner) of that project.

The Token  structure now looks like this (from [alic.h](alic.h)):

```
// Token structure
typedef struct _token {
  int token;                    // Token id from the enum list
  char *tokstr;                 // For T_STRLIT, the string value
  Litval numval;                // For T_NUMLIT, the numerical value
  int numtype;                  // and the type of numerical value
} Token;
```

with an associated list of token ids:

```
enum {
  T_EOF,

  // Binary operators in ascending precedence order
  T_AMPER, T_OR, T_XOR,
  T_EQ, T_NE, T_LT, T_GT, T_LE, T_GE,
  T_LSHIFT, T_RSHIFT,
  T_PLUS, T_MINUS, T_STAR, T_SLASH, T_MOD,
  ...
};
```
Because we now have floating point and signed/unsigned integer numeric tokens, I had to add the ` numtype` field to the `Token` structure, and this list:

```
enum {
  NUM_INT=1, NUM_UINT, NUM_FLT, NUM_CHAR
};
```

The main function in [lexer.c](lexer.c) is

```
// Scan and return the next token found in the input.
// Return 1 if token valid, 0 if no tokens left.
int scan(Token * t) { ... }
```

and we have two global `Token` variables `Thistoken` and `Peektoken` which hold the current token and, occasionally, the token coming up after the current token.

There are a few differences between this version of my lexical tokeniser and the one from the *acwj* project. One is the handling of numeric literals: *acwj* only had integer literals whereas *alic* also has floating-point type literals.

The `scan_numlit()` function in the lexer now uses either `strtod()` to scan in floating-point literals, or `strtoull()` to scan in integer literals. The `Token *t` variable gets the `numval` and `numtype` fields filled in, so we know if it is signed, unsigned or floating-point.

In the *acwj* lexer, I had this huge `switch` statement with a bunch of embedded `if` statements to recognise keywords. I've realised that this can be replaced by an array of keywords which we can loop through. I've kept the first letter concept as a way to make it more efficient then a linear `strcmp()` walk:

```
// A structure to hold a keyword, its first letter
// and the token id associated with the keyword
struct keynode {
  char first;
  char *keyword;
  int  token;
};

// List of keywords and matching tokens
static struct keynode keylist[]= {
  { 'b', "bool", T_BOOL },
  { 'e', "else", T_ELSE },
  { 'f', "false", T_FALSE },
  { 'f', "flt32", T_FLT32 },
  { 'f', "flt64", T_FLT64 },
  { 'f', "for", T_FOR },
  { 'i', "if", T_IF },
  { 'i', "int8", T_INT8 },
  ...
};

// Given a word from the input, return the matching
// keyword token number or 0 if it's not a keyword.
// Switch on the first letter so that we don't have
// to waste time strcmp()ing against all the keywords.
static int keyword(char *s) {
  int i;

  for (i=0; keylist[i].first != 0; i++) {
    // Too early
    if (keylist[i].first < *s) continue;

    // A match
    if (!strcmp(s, keylist[i].keyword)) return(keylist[i].token);

    // Too late
    if (keylist[i].first > *s) return(0);
  }

  return(0);
}
```

I had thought of doing something similar for all the one- and two-character operator tokens (e.g. '=', '==', '<', '<=', '<<' etc.) but I've left the original code in for now. I might go back and revisit this later.

## The Hand-Written Parser

I'm not going to go through all the 700 lines of code in the new file, [parser.c](parser.c). It's a fairly straight-forward (and boring to write!) recursive descent parser. We look at the next lexical token from the input and, based on it, decide what to do next.

There are a bunch of helper functions in [lexer.c](lexer.c) to check the existence of syntactic sugar tokens (e.g parentheses, curly brackets, semicolons etc.) along the way. The main ones are `match()` to match a given token. The other functions call `match()`, e.g. `semi()`, `lparen()`, `rparen()` etc.

Let's just take a look at a couple of examples from the parser. Here's the code to parse a function declaration:

```
// Parse a single function declaration
//
//- function_declaration= function_prototype statement_block
//-                     | function_prototype SEMI
//-
static void function_declaration(void) {
  ASTnode *func;
  ASTnode *s;

  // Get the function's prototype
  func= function_prototype();

  // If the next token is a semicolon
  if (Thistoken.token == T_SEMI) {
    // Add the function prototype to the symbol table
    add_function(func, func->left);

    // Skip the semicolon and return
    scan(&Thistoken); return;
  }

  // It's not a prototype, so we expect a statement block now
  declare_function(func);
  s= statement_block();
  gen_func_statement_block(s);
}
```

We enter `function_declaration` not knowing what the first token is, so we call `function_prototype()` which, afer a few more function calls, matches a type keyword (e.e. `int8` or `void`) and then scans in and returns the function's prototype.

Then we look at the next token. Based on the grammar rules, it could be a semicolon or not. If it's a semicolon, we only have a prototype, so we can add this to the symbol table. But if there is no semicolon, we expect to see a statement block. We call `statement_block()` to parse this and get the AST tree for the function. This gets sent to the QBE code generator via `gen_func_statement_block(s)`.

Note the call to `scan(&Thistoken);` which is the way we skip past the current token and get the next token. I could have called the helper function `semi()` here to do the same thing. I didn't because, just a few lines earlier, I already did `if (Thistoken.token == T_SEMI)` so I don't need to re-check that the token is a semicolon.

As (if) you read through the code, you will see where I call `match()` or the related helper functions to check the syntactic sugar tokens, or when I simply call `scan(&Thistoken);` when I know the token value already.

Let's now look at one of the binary expression parsers. They are all very similar, and I found it easy to write one and copy/paste it to become the others. This one is to parse '+' and '-' expressions:

```
//- additive_expression= ( PLUS? multiplicative_expression
//-                      | MINUS multiplicative_expression
//-                      )
//-                      ( PLUS  multiplicative_expression
//-                      | MINUS multiplicative_expression
//-                      )*
//-
static ASTnode *additive_expression(void) {
  ASTnode *left, *right;
  bool negate= false;
  bool loop=true;

  // Deal with a leading '+' or '-'
  switch(Thistoken.token) {
  case T_PLUS:
    scan(&Thistoken); break;
  case T_MINUS:
    scan(&Thistoken); negate= true; break;
  }

  // Get the multiplicative_expression
  // and negate it if required
  left= multiplicative_expression();
  if (negate) left= unarop(left, A_NEGATE);

  // See if we have more additive operations
  while (loop) {
    switch(Thistoken.token) {
    case T_PLUS:
      scan(&Thistoken); right= multiplicative_expression();
      left= binop(left, right, A_ADD); break;
    case T_MINUS:
      scan(&Thistoken); right= multiplicative_expression();
      left= binop(left, right, A_SUBTRACT); break;
    default:
      loop=false;
    }
  }

  // Nope, return what we have
  return(left);
}
```

First up, note that we have two `bool` variables. We check the first token to see if we have a unary '+' or '-' token. We can ignore the plus, but we set the `negate` variable for the latter.

Now we can get the multiplicative expression, and use `negate` to apply any unary negation action.

At this point, the grammar rules says that there can be zero or more '+' or '-' operators followed by a multiplicative expression. So, we loop scanning these in and use `binop()` to join them in to the AST tree that we are building. If we don't see a following '+' or '-' operator, then we know that we can leave the loop and return the AST tree that we have constructed.

Again, note the use of `scan(&Thistoken);` to skip past the current token and get the next token.

## Peeking Ahead

There is only one place in the current *alic* grammar where we need to look ahead one token. This is when we have to parse statements:

```
procedural_stmt= ( print_stmt
                 | assign_stmt
                 | if_stmt
                 | while_stmt
                 | for_stmt
                 | function_call
                 )*
```

Both the assignment statement and function call start with an identifier, e.g.

```
a = 7;
b(23);
```

Thus, we need to peek past the current token to see if there is an '=' or '(' following it.
 The code in `procedural_stmt()` uses the `Peektoken` global variable to do this:

```
static ASTnode *procedural_stmt(void) {
  ...

  // See if this token is a known keyword or identifier
  switch(Thistoken.token) {
  case T_PRINTF:
     ...
  case T_IF:
    ...
  case T_WHILE:
    ...
  case T_FOR:
    ...
  case T_IDENT:
    // Get the next token. If it's '=' then
    // we have an assignment statement.
    // If it's a '(' then it's a function call.
    scan(&Peektoken);
    switch(Peektoken.token) {
    case T_ASSIGN:
      ...
    case T_LPAREN:
      ...
    }
  }
}
```

I've written the lexer code in such a way that we don't have to undo the peeking. When we `scan()` (in [lexer.c](lexer.c)) to get the next token, we use `Peektoken` if it has a value. Otherwise we go about the work of scanning the next token. 

## Any Advantage by Dropping the *leg* Parser?

In part four, I railed against the requirement that every rule in the *leg* grammar had to return the same type of data which, in *alic*, is an ASTnode pointer. That's why I wanted to rewrite the parser. Have I taken advantage of the freedom of a hand-written parser?

Yes, I have! In the *leg* grammar, when I was parsing a type keyword, I had to build an ASTnode just to return the type:

```
# Types
#
type =    "int8"   -    { $$= mkastleaf(A_TYPE, ty_int8,  false, NULL,0);  }
        | "int16"  -    { $$= mkastleaf(A_TYPE, ty_int16,  false, NULL,0); }
        | "int32"  -    { $$= mkastleaf(A_TYPE, ty_int32,  false, NULL,0); }
        | "int64"  -    { $$= mkastleaf(A_TYPE, ty_int64,  false, NULL,0); }
        | "uint8"  -    { $$= mkastleaf(A_TYPE, ty_uint8,  false, NULL,0); }
        | "uint16" -    { $$= mkastleaf(A_TYPE, ty_uint16, false, NULL,0); }
        | "uint32" -    { $$= mkastleaf(A_TYPE, ty_uint32, false, NULL,0); }
        | "uint64" -    { $$= mkastleaf(A_TYPE, ty_uint64, false, NULL,0); }
        | "flt32"  -    { $$= mkastleaf(A_TYPE, ty_flt32,  false, NULL,0); }
        | "flt64"  -    { $$= mkastleaf(A_TYPE, ty_flt64,  false, NULL,0); }
        | "bool"   -    { $$= mkastleaf(A_TYPE, ty_bool,   false, NULL,0); }
        | VOID          { $$= mkastleaf(A_TYPE, ty_void,   false, NULL,0); }
```

In the hand-written parser, I can do this:

```
static Type* type(void) {
  Type *t;

  // See if this token is a built-in type
  switch(Thistoken.token) {
  case T_VOID:   t= ty_void;   break;
  case T_BOOL:   t= ty_bool;   break;
  case T_INT8:   t= ty_int8;   break;
  case T_INT16:  t= ty_int16;  break;
  case T_INT32:  t= ty_int32;  break;
  case T_INT64:  t= ty_int64;  break;
  case T_UINT8:  t= ty_uint8;  break;
  case T_UINT16: t= ty_uint16; break;
  case T_UINT32: t= ty_uint32; break;
  case T_UINT64: t= ty_uint64; break;
  case T_FLT32:  t= ty_flt32;  break;
  case T_FLT64:  t= ty_flt64;  break;
  }

  if (t==NULL)
    fatal("Unknown type %s\n", get_tokenstr(Thistoken.token));

  // Get the next token and return
  scan(&Thistoken);
  return(t);
}
```

Now I just return a `Type` pointer and I don't go through the pain of making an ASTnode just to discard it later.

## Error Checking

Another big advantage of a hand-written parser is that we can do better error checking. I've already mentioned the checking of the syntactic sugar tokens. You can see, above, that I now have some code to stop the parsing if we hit a token that isn't a known type token. In [parser.c](parser.c) there are a few other calls to `fatal()` when we detect a syntax error in the input. Looking at my *acwj* parsing code, I can see a whole bunch of other error checks that will eventually end up here in *alic*.

## Conclusion and The Next Step

I'm quite happy to have started with *leg* as a parser generator for *alic*, as it made it easy to start designing the language from scratch. I'm also happy to change over to a hand-written lexer and parser now that the language is becoming more complex.

I'm hoping that the next step will be the introduction of a language feature for *alic* that is not in C: named arguments to functions, e.g.

```
void fred(int32 a, int8 b, flt32 c) { ... } ;

void main(void) {
  ...
  fred(c= 100.0 * 35, b= x+y, a= -11);
```
