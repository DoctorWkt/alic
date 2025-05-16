// Parser for the alic compiler.
// (c) 2025 Warren Toomey, GPL3

// Note: You can grep '//-' this file to extract the grammar

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "alic.h"
#include "proto.h"

// Forward declarations
static void function_declarations(void);
static void function_declaration(void);
static ASTnode *function_prototype(void);
static ASTnode *typed_declaration_list(void);
static ASTnode *typed_declaration(void);
static Type* type(void);
static ASTnode *statement_block(void);
static ASTnode *declaration_stmts(void);
static ASTnode *procedural_stmts(void);
static ASTnode *procedural_stmt(void);
static ASTnode *assign_stmt(void);
static ASTnode *short_assign_stmt(void);
static ASTnode *if_stmt(void);
static ASTnode *while_stmt(void);
static ASTnode *for_stmt(void);
static ASTnode *function_call(void);
static ASTnode *expression_list(void);
static ASTnode *expression(void);
static ASTnode *bitwise_expression(void);
static ASTnode *relational_expression(void);
static ASTnode *shift_expression(void);
static ASTnode *additive_expression(void);
static ASTnode *multiplicative_expression(void);
static ASTnode *factor(void);
static ASTnode *variable(void);

// Parse the input file
//
//- input_file= function_declarations EOF
//-
void parse_file(void) {
  function_declarations();
}

//- function_declarations= function_declaration*
//-
static void function_declarations(void) {
  // Loop parsing functions until we hit the EOF
  while (Thistoken.token != T_EOF) {
    function_declaration();
  }
}

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

// Parse a function prototype and
// return an ASTnode with the details
//
//- function_prototype= typed_declaration LPAREN typed_declaration_list RPAREN
//-                   | typed_declaration LPAREN VOID RPAREN
//-
static ASTnode *function_prototype(void) {
  ASTnode *func;
  ASTnode *paramlist=NULL;

  // Get the function's name and type
  func= typed_declaration();
  lparen();

  // If the next token is VOID, skip it
  if (Thistoken.token == T_VOID) {
    scan(&Thistoken);
  } else {
    // Get the list of parameters
    paramlist= typed_declaration_list();
  }

  rparen();

  func->left= paramlist;
  return(func);
}

// Get a linked list of typed declarations
// as a set of ASTnodes linked by the middle child
//
//- typed_declaration_list= typed_declaration (COMMA typed_declaration_list)*
//-
static ASTnode *typed_declaration_list(void) {
  ASTnode *first, *this, *next;

  // Get the first typed_declaration
  first= this= typed_declaration();

  while (1) {
    // If no comma, stop now
    if (Thistoken.token != T_COMMA) break;

    // Skip the comma
    // Get the next declaration and link it in
    scan(&Thistoken);
    next= typed_declaration();
    this->mid= next; this= next;
  }

  return(first);
}

// Get a symbol declaration along with its type as an ASTnode
//
//- typed_declaration= type IDENT
//-
static ASTnode *typed_declaration(void) {
  ASTnode *identifier;
  Type *t;

  t= type();
  match(T_IDENT, true);

  identifier= mkastleaf(A_IDENT, NULL, false, NULL, 0);
  identifier->strlit= strdup(Text);
  identifier->type= t;
  return(identifier);
}

// Return a pointer to a Type structure
// that matches the current token
//
//- type= built-in type | user-defined type
//-
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

// A statement block has all the declarations first,
// followed by any procedural statements.
//
//- statement_block= LBRACE procedural_stmt* RBRACE
//-                | LBRACE declaration_stmt* procedural_stmt* RBRACE
//-
static ASTnode *statement_block(void) {
  ASTnode *s=NULL, *d=NULL;

  lbrace();

  // An empty statement body
  if (Thistoken.token == T_RBRACE)
    return(NULL);

  // A declaration_stmt starts with a type, so look for one.
  // XXX This will need to be fixed when we have user-defined types
  if (Thistoken.token >= T_VOID && Thistoken.token <= T_FLT64)
    d= declaration_stmts();

  // Now get any procedural statements
  s= procedural_stmts();
  if (d == NULL)
    d= s;
  else
    d->right= s;

  rbrace();
  return(d);
}

// Parse zero or more declaration statements and
// build an AST tree with them linked by the middle child
//- declaration_stmts= (typed_declaration ASSIGN expression SEMI)+
//-
static ASTnode *declaration_stmts(void) {
  ASTnode *d, *e;
  ASTnode *this;

  // Get one declaration statement
  d= typed_declaration();
  match(T_ASSIGN, true);
  e= expression();
  semi();

  // Declare that variable
  this= declaration_statement(d, e);

  // Look for a type. If so, we have another declaration statement
  if (Thistoken.token >= T_VOID && Thistoken.token <= T_FLT64) {
    this->mid= declaration_stmts();
  }

  return(this);
}

// Parse zero or more procedural statements and
// build an AST tree holding all the statements
//- procedural_stmt= ( print_stmt
//-                  | assign_stmt
//-                  | if_stmt
//-                  | while_stmt
//-                  | for_stmt
//-                  | function_call
//-                  )*
//-
//- print_stmt= PRINTF LPAREN STRLIT COMMA expression RPAREN SEMI
//-
static ASTnode *procedural_stmts(void) {
  ASTnode *left= NULL;
  ASTnode *right;

  while (1) {
    // Try to get another statement
    right= procedural_stmt();
    if (right==NULL) break;

    // Glue left and right if we have both
    // or just set left for now
    if (left==NULL) left=right;
    else left= mkastnode(A_GLUE,left,NULL,right);
  }

  return(left);
}

// Parse a single procedural statement.
// Return NULL if there is none.
static ASTnode *procedural_stmt(void) {
  ASTnode *left, *right;

  // If we have a right brace, no statement
  if (Thistoken.token == T_RBRACE)
    return(NULL);

  // See if this token is a known keyword or identifier
  switch(Thistoken.token) {
  case T_PRINTF:
    // Skip the printf and the left parenthesis
    scan(&Thistoken);
    lparen();

    // Check that we have a string literal and a comma.
    // Make an A_STRLIT node
    match(T_STRLIT, false);
    left= mkastleaf(A_STRLIT, NULL, false, NULL, 0);
    left->strlit= Thistoken.tokstr;
    scan(&Thistoken);
    comma();

    // Get the expression, the right parenthesis and the semicolon
    right= expression();
    rparen(); semi();
    return(print_statement(left, right));
  case T_IF:
    return(if_stmt());
  case T_WHILE:
    return(while_stmt());
  case T_FOR:
    return(for_stmt());
  case T_IDENT:
    // Get the next token. If it's '=' then
    // we have an assignment statement.
    // If it's a '(' then it's a function call.
    scan(&Peektoken);
    switch(Peektoken.token) {
    case T_ASSIGN:
      return(assign_stmt());
    case T_LPAREN:
      return(function_call());
    default: fatal("Unexpected token %s after identifier\n",
			get_tokenstr(Peektoken.token));
    }
  }

  return(NULL);
}

//- assign_stmt= short_assign_stmt SEMI
//-
static ASTnode *assign_stmt(void) {
  ASTnode *a= short_assign_stmt();
  semi();
  return(a);
}

//- short_assign_stmt= variable ASSIGN expression
//-
static ASTnode *short_assign_stmt(void) {
  ASTnode *v, *e;

  // Get the variable, check for an '=' then get the expression
  v= variable(); match(T_ASSIGN, true); e= expression();
  return(assignment_statement(v, e));
}

//- if_stmt= IF LPAREN relational_expression RPAREN statement_block
//-          (ELSE statement_block)?
//-
static ASTnode *if_stmt(void) {
  ASTnode *e, *t, *f=NULL;

  // Skip the IF, check for a left parenthesis.
  // Get the expression, right parenthesis
  // and the statement block
  scan(&Thistoken);
  lparen();
  e= relational_expression();
  rparen();
  t= statement_block();

  // If we now have an ELSE
  // get the following statement block
  if (Thistoken.token== T_ELSE) {
    scan(&Thistoken);
    f= statement_block();
  }

  return(mkastnode(A_IF, e, t, f));
}

//- while_stmt= WHILE LPAREN relational_expression RPAREN statement_block
//-
static ASTnode *while_stmt(void) {
  ASTnode *e, *s;

  // Skip the WHILE, check for a left parenthesis.
  // Get the expression, right parenthesis
  // and the statement block
  scan(&Thistoken);
  lparen();
  e= relational_expression();
  rparen();
  s= statement_block();

  return(mkastnode(A_WHILE, e, s, NULL));
}

//- for_stmt= FOR LPAREN assign_stmt relational_expression SEMI
//-           short_assign_stmt RPAREN statement_block
//-
static ASTnode *for_stmt(void) {
  ASTnode *i, *e, *send, *s;

  // Skip the FOR, check for a left parenthesis.
  // Get the assignment statement and relational expression.
  // Check for a semicolon. Get the short assignment statement.
  // Check for a right parenthesis and get the statement block.
  scan(&Thistoken);
  lparen();
  i= assign_stmt();
  e= relational_expression();
  semi();
  send= short_assign_stmt();
  rparen();
  s= statement_block();

  // Glue the end code after the statement block
  s= mkastnode(A_GLUE, s, NULL, send);

  // We put the initial code at the end so that
  // we can send the node to gen_WHILE() :-)
  return(mkastnode(A_FOR, e, s, i));
}

//- function_call= IDENT LPAREN expression_list? RPAREN SEMI
//-
static ASTnode *function_call(void) {
  ASTnode *s, *e=NULL;

  // Make an IDENT node from the current token
  s= mkastleaf(A_IDENT, NULL, false, NULL, 0);
  s->strlit= Thistoken.tokstr;

  // Skip the identifier and get the left parenthesis
  scan(&Thistoken);
  lparen();

  // If the next token is not a right parenthesis,
  // get the expression list
  if (Thistoken.token != T_RPAREN)
    e= expression_list();

  // Get the right parenthesis and semicolon
  rparen();
  semi();
  
  return(mkastnode(A_FUNCCALL,s,NULL,e));
}

//- expression_list= expression (COMMA expression_list)*
//-
static ASTnode *expression_list(void) {
  ASTnode *e, *l=NULL;

  // Get the expression
  e= expression();

  // If we have a comma, skip it.
  // Get the following expression list
  if (Thistoken.token == T_COMMA) {
    scan(&Thistoken);
    l= expression_list();
  }

  // Glue e and l and return them
  return(mkastnode(A_GLUE,e,NULL,l));
}

//- expression= bitwise_expression
//-
static ASTnode *expression(void) {
  return(bitwise_expression());
}

//- bitwise_expression= ( INVERT relational_expression
//-                     |        relational_expression
//-                     )
//-                     ( AND relational_expression
//-                     | OR  relational_expression
//-                     | XOR relational_expression
//-                     )*
//-
static ASTnode *bitwise_expression(void) {
  ASTnode *left, *right;
  bool invert= false;
  bool loop=true;

  // Deal with a leading '~'
  if (Thistoken.token == T_INVERT) {
    scan(&Thistoken); invert= true;
  }

  // Get the relational expression
  // and invert if required
  left= relational_expression();
  if (invert) left= unarop(left, A_INVERT);

  // See if we have more relational operations
  while (loop) {
    switch(Thistoken.token) {
    case T_AMPER:
      scan(&Thistoken);
      right= relational_expression();
      left= binop(left, right, A_AND); break;
    case T_OR:
      scan(&Thistoken);
      right= relational_expression();
      left= binop(left, right, A_OR); break;
    case T_XOR:
      scan(&Thistoken);
      right= relational_expression();
      left= binop(left, right, A_XOR); break;
    default:
      loop=false;
    }
  }

  // Nope, return what we have
  return(left);
}

//- relational_expression= ( NOT shift_expression
//-                        |     shift_expression
//-                        )
//-                        ( GE shift_expression
//-                        | GT shift_expression
//-                        | LE shift_expression
//-                        | LT shift_expression
//-                        | EQ shift_expression
//-                        | NE shift_expression
//-                        )?
//- 
static ASTnode *relational_expression(void) {
  ASTnode *left, *right;
  bool not= false;

  // Deal with a leading '!'
  if (Thistoken.token == T_LOGNOT) {
    scan(&Thistoken); not= true;
  }

  // Get the shift expression and
  // logically not if required
  left= shift_expression();
  if (not) left= unarop(left, A_NOT);

  // See if we a relational operation
  switch(Thistoken.token) {
  case T_GE:
    scan(&Thistoken); right= shift_expression();
    left= binop(left, right, A_GE); break;
  case T_GT:
    scan(&Thistoken); right= shift_expression();
    left= binop(left, right, A_GT); break;
  case T_LE:
    scan(&Thistoken); right= shift_expression();
    left= binop(left, right, A_LE); break;
  case T_LT:
    scan(&Thistoken); right= shift_expression();
    left= binop(left, right, A_LT); break;
  case T_EQ:
    scan(&Thistoken); right= shift_expression();
    left= binop(left, right, A_EQ); break;
  case T_NE:
    scan(&Thistoken); right= shift_expression();
    left= binop(left, right, A_NE); break;
  }

  // Nope, return what we have
  return(left);
}

//- shift_expression= additive_expression
//-                 ( LSHIFT additive_expression
//-                 | RSHIFT additive_expression
//-                 )*
//-
static ASTnode *shift_expression(void) {
  ASTnode *left, *right;
  bool loop=true;

  left= additive_expression();

  // See if we have more shft operations
  while (loop) {
  switch(Thistoken.token) {
    case T_LSHIFT:
      scan(&Thistoken); right= additive_expression();
      left= binop(left, right, A_LSHIFT); break;
    case T_RSHIFT:
      scan(&Thistoken); right= additive_expression();
      left= binop(left, right, A_RSHIFT); break;
    default:
      loop=false;
    }
  }

  // Nope, return what we have
  return(left);
}

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

//- multiplicative_expression= l:factor
//-                          ( STAR  factor
//-                          | SLASH factor
//-                          )*
static ASTnode *multiplicative_expression(void) {
  ASTnode *left, *right;
  bool loop=true;

  // Get the first factor
  left= factor();

  // See if we have more multiplicative operations
  while (loop) {
    switch(Thistoken.token) {
    case T_STAR:
      scan(&Thistoken); right= factor();
      left= binop(left, right, A_MULTIPLY);
      break;
    case T_SLASH:
      scan(&Thistoken); right= factor();
      left= binop(left, right, A_DIVIDE);
      break;
    default:
      loop=false;
    }
  }

  // Nope, return what we have
  return(left);
}


//- factor= NUMLIT
//-       | TRUE
//-       | FALSE
//-       | variable
//-
static ASTnode *factor(void) {
  ASTnode *f;

  switch(Thistoken.token) {
  case T_NUMLIT:
    // Build an ASTnode with the numeric value and suitable type
    f= mkastleaf(A_NUMLIT, parse_litval(&Thistoken),
		 true, NULL, Thistoken.numval.uintval);
    scan(&Thistoken);
    break;
  case T_TRUE:
    f= mkastleaf(A_NUMLIT, ty_bool, true, NULL, 1);
    scan(&Thistoken);
    break;
  case T_FALSE:
    f= mkastleaf(A_NUMLIT, ty_bool, true, NULL, 0);
    scan(&Thistoken);
    break;
  case T_IDENT:
    f= variable();
    break;
  default:
    fatal("Unknown token as a factor: %s\n",
			get_tokenstr(Thistoken.token));
  }

  return(f);
}

//- variable= IDENT
//-
static ASTnode *variable(void) {
  ASTnode *n= mkastleaf(A_IDENT, NULL, false, NULL, 0);
  n->strlit= Thistoken.tokstr;
  n= mkident(n);		// Check variable exists, get its type
  scan(&Thistoken);
  return(n);
}
