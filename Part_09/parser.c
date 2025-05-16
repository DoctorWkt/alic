// Parser for the alic compiler.
// (c) 2025 Warren Toomey, GPL3

// Note: You can grep '//-' this file to extract the grammar

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "alic.h"
#include "proto.h"

// Forward declarations
static void type_declaration(void);
static void function_declaration(ASTnode *decl);
static ASTnode *function_prototype(ASTnode *func);
static ASTnode *typed_declaration_list(void);
static void enum_declaration(void);
static void struct_declaration(char *name);
static ASTnode *union_declaration(void);
void global_var_declaration(ASTnode *decl);
static ASTnode *typed_declaration(void);
static Type* type(bool checkonly);
static ASTnode *statement_block(Sym *func);
static ASTnode *declaration_stmts(void);
static ASTnode *procedural_stmts(void);
static ASTnode *procedural_stmt(void);
static ASTnode *assign_stmt(void);
static ASTnode *short_assign_stmt(void);
static ASTnode *if_stmt(void);
static ASTnode *while_stmt(void);
static ASTnode *for_stmt(void);
static ASTnode *return_stmt(void);
static ASTnode *function_call(void);
static ASTnode *expression_list(void);
static ASTnode *named_expression_list(void);
static ASTnode *expression(void);
static ASTnode *bitwise_expression(void);
static ASTnode *relational_expression(void);
static ASTnode *shift_expression(void);
static ASTnode *additive_expression(void);
static ASTnode *multiplicative_expression(void);
static ASTnode *unary_expression(void);
static ASTnode *primary_expression(void);
static ASTnode *variable(void);

// Global variables
Sym *Thisfunction;	// The function we are parsing

// Parse the input file
//
//- input_file= ( type_declaration
//-             | enum_declaration
//-             | global_var_declaration
//-             | function_declaration
//-             )* EOF
//-
void input_file(void) {
  ASTnode *decl;

  // Loop parsing functions until we hit the EOF
  while (Thistoken.token != T_EOF) {
    switch(Thistoken.token) {
    case T_TYPE:
      type_declaration(); break;
    case T_ENUM:
      enum_declaration(); break;
    default:
      // This could be a function or variable declaration.
      // Get the typed declaration.
      decl= typed_declaration();

      // If the next token is an LPAREN, 
      // it's a function declaration,
      // otherwise a global variable declaration
      if (Thistoken.token == T_LPAREN)
        function_declaration(decl);
      else
	global_var_declaration(decl);
    }
  }
}

// Parse a new type declaration
//
//- type_declaration= TYPE IDENT SEMI
//-                 | TYPE IDENT ASSIGN type SEMI
//-                 | TYPE IDENT ASSIGN struct_declaration SEMI
//-
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

  // See if this type's name already exists
  if (find_type(typename, 0, 0)!=NULL)
    fatal("type %s already exists\n", typename);

  // Skip the identifier
  scan(&Thistoken);

  // If the next token is an '='
  if (Thistoken.token == T_ASSIGN) {
    // Skip the '='
    scan(&Thistoken);

    // If the next token is STRUCT
    if (Thistoken.token == T_STRUCT) {
      // Parse the struct list
      struct_declaration(typename);
    } else {

      // Get the base type with no pointer depth
      basetype= type(true);
      if (basetype == NULL)
        fatal("Unknown base type in type declaration: %s\n",
				get_tokenstr(Thistoken.token));

      // The base type might be followed by '*'
      // so parse this as well
      ptrtype= type(false);
    
      // Add the alias type to the list.
      // Make any pointer type at the same time
      new_type(TY_USER, basetype->size, 0, typename, basetype);
      if (basetype != ptrtype)
        new_type(TY_USER, ptrtype->size, ptrtype->ptr_depth,
				typename, ptrtype);
    }
  } else {
    // No '=' sign
    // Add the opaque type to the list
    new_type(TY_USER, 0, 0, typename, NULL);
  }

  // Get the trailing semicolon
  semi();
}

// Parse an enumerated list. We must have at least
// one item in the enum list
//
//- enum_declaration= ENUM LBRACE enum_list RBRACE SEMI
//-
//- enum_list= enum_item (COMMA enum_item)*
//-
//- enum_item= IDENT
//-          | IDENT ASSIGN NUMLIT
//-
static void enum_declaration(void) {
  Litval val;
  Type *ty;
  Sym *sym;
  char *name;

  // Skip the TYPE keyword and get the left brace
  scan(&Thistoken);
  lbrace();

  // Loop getting the next enum item
  val.intval= 0;
  while (1) {
    // Make sure that we have an identifier
    match(T_IDENT, true);
    name= Thistoken.tokstr;

    // If it's followed by an '='
    if (Thistoken.token == T_ASSIGN) {
      // Skip it and get the following numeric literal token
      scan(&Thistoken);
      match(T_NUMLIT, false);

      // Check that the literal value isn't a float
      if (Thistoken.numtype == NUM_FLT)
	fatal("Cannot use a float literal as an enumerated value\n");

      // Update val to hold this literal value
      val.intval= Thistoken.numval.intval;

      // Skip the literal value
      scan(&Thistoken);
    }

    // Get a suitable type for the literal value
    ty= parse_litval(val, NUM_INT);

    // Add the enum's name and value to the global scope.
    // Then set the symbol's literal value
    if (find_symbol(name) != NULL)
      fatal("symbol %s already exists\n", name);
    sym= add_symbol(name, ST_ENUM, ty, true);
    sym->initval= val;

    // Increment the enum value
    val.intval++;

    // If we have a right brace, stop looping
    // Otherwise check for an absorb a comma
    if (Thistoken.token == T_RBRACE) break;
    match(T_COMMA, true);
  }

  // Skip past the right brace and semicolon
  rbrace();
  semi();
}

// Given a pointer to a newly-created struct type
// one or more typed declarations in an AST list,
// and the possible offset where the first member could start,
// add the declaration(s) as members to the type.
// isunion is true when we are adding a set of union members.
// Die if there are any semantic errors.
// Return the possible offset of the next member
static int add_memb_to_struct(Type *strtype, ASTnode *asthead,
				int offset, bool isunion) {
  ASTnode *astmemb;
  Memb *thismemb, *lastmemb;
  int biggest_memb=0;
  ASTnode *astbiggest;

  if (O_logmisc)
    fprintf(Debugfh, "add_memb: offset is %d\n", offset);

  // If this is a union, find the biggest member
  // as this will be the one that we align on.
  // Calculate the offset now
  if (isunion) {
    for (astmemb= asthead; astmemb != NULL; astmemb= astmemb->mid) {
      if (astmemb->type->size > biggest_memb) {
        biggest_memb= astmemb->type->size;
        astbiggest= astmemb;
      }
    }

    offset= cgalign(astbiggest->type, offset);
    if (O_logmisc)
      fprintf(Debugfh, "set biggest in union to %d offset %d\n",
				biggest_memb, offset);
  }

  // Walk the list of new members to add to the struct
  for (astmemb= asthead; astmemb != NULL; astmemb= astmemb->mid) {
    // Do not let structs or opaque types in a struct
    if ((astmemb->type->kind == TY_STRUCT && astmemb->type->ptr_depth == 0)
						|| astmemb->type->size == 0)
      fatal("member of type %s cannot be in a struct\n",
		get_typename(astmemb->type));

    // Walk the existing members to a) check that we don't have
    // duplicate member names, and b) get a pointer to the last
    // member currently in the struct
    for (lastmemb= thismemb= strtype->memb; thismemb != NULL; 
			lastmemb= thismemb, thismemb= thismemb->next) {
      if (!strcmp(thismemb->name, astmemb->strlit))
        fatal("duplicate member name %s in struct declaration\n",
						thismemb->name);
    }

    // It's safe to add the astmemb to the struct.
    // Create the Memb struct, add the name and type
    thismemb= (Memb *)Calloc(sizeof(Memb));
    thismemb->name= astmemb->strlit;
    thismemb->type= astmemb->type;

    // Is this the first member?
    if (strtype->memb == NULL) {
      thismemb->offset= 0;
      strtype->memb= thismemb;
      if (O_logmisc)
        fprintf(Debugfh, "%s member %s: offset %d size %d\n",
	  get_typename(thismemb->type),
	  thismemb->name, thismemb->offset, thismemb->type->size);
      return(thismemb->type->size);
    }

    // Not the first member. Get the aligned offset for it
    // Then append it. If a union, we already have the right offset
    if (isunion == false)
      offset= cgalign(astmemb->type, offset);
    thismemb->offset= offset;
    lastmemb->next= thismemb;

    // Update the offset if not a union
    if (isunion == false)
      offset= offset + astmemb->type->size;

    if (O_logmisc)
      fprintf(Debugfh, "%s member %s: offset %d size %d\n",
	get_typename(thismemb->type),
	thismemb->name, thismemb->offset, thismemb->type->size);
  }

  // Now return the possible offset of the next member
  if (isunion) {
    if (O_logmisc)
      fprintf(Debugfh, "isunion offset %d biggest %d\n", offset, biggest_memb);
    return(offset + biggest_memb);
  }
  else
    return(offset);
}

// Parse a struct declaration.
//
//- struct_declaration= STRUCT LBRACE struct_list RBRACE
//-
//- struct_list= struct_item (COMMA struct_item)*
//-
//- struct_item= typed_declaration
//-            | union_declaration
//-
static void struct_declaration(char *typename) {
  Type *thistype;
  ASTnode *astmemb;
  int offset=0;

  // Skip the STRUCT keyword and get the left brace
  scan(&Thistoken);
  lbrace();

  // See if this type's name already exists
  if (find_type(typename, 0, 0)!=NULL)
    fatal("type %s already exists\n", typename);

  // Build a new STRUCT type
  thistype= new_type(TY_STRUCT, 0, 0, typename, NULL);
  if (O_logmisc)
    fprintf(Debugfh, "type struct %s:\n", typename);

  // Loop getting members for the struct
  while (1) {
    // Is the next token a UNION?
    if (Thistoken.token == T_UNION) {
      // Get the union declaration
      // and add the members to the struct
      astmemb= union_declaration();
      offset= add_memb_to_struct(thistype, astmemb, offset, true);
    } else {
      // Get a typed declaration
      // and add the member to the struct
      astmemb= typed_declaration();
      offset= add_memb_to_struct(thistype, astmemb, offset, false);
    }

    // If no comma, stop now
    if (Thistoken.token != T_COMMA) break;

    // Skip the comma
    scan(&Thistoken);
  }

  // Set the struct size in bytes
  thistype->size= offset;
  if (O_logmisc)
    fprintf(Debugfh, "struct total size is %d\n", offset);

  // Get the trailing right brace
  rbrace();
}

// Parse a union declaration

//- union_declaration= UNION LBRACE typed_declaration_list RBRACE
//-
static ASTnode *union_declaration(void) {
  ASTnode *astmemb;

  // Skip the UNION keyword and get the left brace
  scan(&Thistoken);
  lbrace();

  astmemb= typed_declaration_list();

  // Get the trailing right brace
  rbrace();
  return(astmemb);
}

// Parse a global variable declaration. We receive the
// typed_declaration from function_prototype()
//
//- global_var_declaration= typed_declaration SEMI;
//-
void global_var_declaration(ASTnode *decl) {

  // See if the variable's name already exists
  if (find_symbol(decl->strlit) != NULL)
    fatal("symbol %s already exists\n", decl->strlit);

  // Add the symbol and type as a global
  add_symbol(decl->strlit, ST_VARIABLE, decl->type, true);

  // Get the trailing semicolon
  semi();
}


// Parse a single function declaration
//
//- function_declaration= function_prototype statement_block
//-                     | function_prototype SEMI
//-
static void function_declaration(ASTnode *func) {
  ASTnode *s;

  // Get the function's prototype.
  func= function_prototype(func);

  // If the next token is a semicolon
  if (Thistoken.token == T_SEMI) {
    // Add the function prototype to the symbol table
    add_function(func, func->left);

    // Skip the semicolon and return
    scan(&Thistoken); return;
  }

  // It's not a prototype, so we expect a statement block now.
  // Set Thisfunction to the function's symbol structure
  declare_function(func);
  Thisfunction= find_symbol(func->strlit);
  s= statement_block(Thisfunction);
  gen_func_statement_block(s);
}

// Parse a function prototype and
// return an ASTnode with the details
//
//- function_prototype= typed_declaration LPAREN typed_declaration_list RPAREN
//-                   | typed_declaration LPAREN VOID RPAREN
//-                   | typed_declaration LPAREN ELLIPSIS RPAREN
//-
static ASTnode *function_prototype(ASTnode *func) {
  ASTnode *paramlist=NULL;

  // We already have the typed declaration in func.
  // Check the next token
  lparen();

  // If the next token is VOID,
  // see if it is followed by a ')'
  if (Thistoken.token == T_VOID) {
    scan(&Peektoken);
    // It is, so we have no parameters
    if (Peektoken.token == T_RPAREN) {
      Peektoken.token= 0;
      scan(&Thistoken);
      func->left= NULL;
      return(func);
    }
  }

  // If the next token is an ELLIPSIS, skip it
  // and mark the function as variadic
  // by using the rvalue field
  if (Thistoken.token == T_ELLIPSIS) {
    scan(&Thistoken);
    func->rvalue= true;
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

  t= type(false);	// Get the type and skip past it
  match(T_IDENT, true);

  identifier= mkastleaf(A_IDENT, NULL, false, NULL, 0);
  identifier->strlit= strdup(Text);
  identifier->type= t;
  return(identifier);
}

// Return a pointer to a Type structure
// that matches the current token, or
// NULL if the token isn't a known type.
// If checkonly is set, recognise the token
// as a type but don't absorb it.
//
//- type= (builtin_type | user_defined_type) STAR*
//-
//- builtin_type= 'void'  | 'bool'
//-             | 'int8'  | 'int16'  | 'int32'  | 'int64'
//-             | 'uint8' | 'uint16' | 'uint32' | 'uint64'
//-             | 'flt32' | 'flt64'
//-
//- user_defined_type= IDENT
//-
static Type* type(bool checkonly) {
  Type *t=NULL;
  char *typename=NULL;

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

// A statement block has all the declarations first,
// followed by any procedural statements. The func
// argument is non-NULL when we are starting a new
// function.
//
//- statement_block= LBRACE declaration_stmts procedural_stmts RBRACE
//-
static ASTnode *statement_block(Sym *func) {
  ASTnode *s=NULL, *d=NULL;

  lbrace();

  // An empty statement body
  if (Thistoken.token == T_RBRACE)
    return(NULL);

  // Start a new scope
  new_scope(func);

  // A declaration_stmt starts with a type, so look for one.
  if (type(true)!=NULL)
    d= declaration_stmts();

  // Now get any procedural statements
  s= procedural_stmts();
  if (d == NULL)
    d= s;
  else
    d->right= s;

  rbrace();
  // Dispose of this scope
  end_scope();
  return(d);
}

// Parse zero or more declaration statements and
// build an AST tree with them linked by the middle child
//- declaration_stmts= ( typed_declaration ASSIGN expression SEMI
//-                    | typed_declaration SEMI
//-                    )*
//-
static ASTnode *declaration_stmts(void) {
  ASTnode *d, *e= NULL;
  ASTnode *this;

  // Get one declaration statement
  d= typed_declaration();

  // If there is an '=' next, we have an assignment
  if (Thistoken.token == T_ASSIGN) {
    scan(&Thistoken);
    e= expression();
  }

  semi();

  // Declare that variable
  this= declaration_statement(d, e);

  // Look for a type. If so, we have another declaration statement
  if (type(true)!=NULL) {
    this->mid= declaration_stmts();
  }

  return(this);
}

// Parse zero or more procedural statements and
// build an AST tree holding all the statements
//- procedural_stmts= ( assign_stmt
//-                   | if_stmt
//-                   | while_stmt
//-                   | for_stmt
//-                   | return_stmt
//-                   | function_call SEMI
//-                   )*
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
  ASTnode *left;

  // If we have a right brace, no statement
  if (Thistoken.token == T_RBRACE)
    return(NULL);

  // See if this token is a known keyword or identifier
  switch(Thistoken.token) {
  case T_IF:
    return(if_stmt());
  case T_WHILE:
    return(while_stmt());
  case T_FOR:
    return(for_stmt());
  case T_RETURN:
    return(return_stmt());
  case T_STAR:
    return(assign_stmt());
  case T_IDENT:
    // Get the next token.
    // If it's a '(' then it's a function call.
    scan(&Peektoken);
    if (Peektoken.token == T_LPAREN) {
      // Get the AST for the function and
      // absorb the trailing semicolon
      left= function_call();
      semi();
      return(left);
    }

    // No '(' so it's an assignment statement
    return(assign_stmt());
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
  t= statement_block(NULL);

  // If we now have an ELSE
  // get the following statement block
  if (Thistoken.token== T_ELSE) {
    scan(&Thistoken);
    f= statement_block(NULL);
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
  s= statement_block(NULL);

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
  s= statement_block(NULL);

  // Glue the end code after the statement block
  s= mkastnode(A_GLUE, s, NULL, send);

  // We put the initial code at the end so that
  // we can send the node to gen_WHILE() :-)
  return(mkastnode(A_FOR, e, s, i));
}

//- return_stmt= RETURN LPAREN expression RPAREN SEMI
//-            | RETURN SEMI
//-
static ASTnode *return_stmt(void) {
  ASTnode *this, *e=NULL;

  // Skip the 'return' token
  scan(&Thistoken);

  // If we have a left parenthesis, we are returning a value
  if (Thistoken.token == T_LPAREN) {
    // Can't return a value if the function returns void
    if (Thisfunction->type == ty_void)
      fatal("Can't return from void %s()\n", Thisfunction->name);

    // Skip the left parenthesis
    lparen();

    // Parse the following expression
    e= expression();

    // Widen the expression's type if required
    e= widen_expression(e, Thisfunction->type);

    // Get the ')'
    rparen();
  }

  // Error if no expression but the function returns a value
  if (e==NULL && Thisfunction->type != ty_void)
    fatal("No return value from non-void %s()\n", Thisfunction->name);

  // Build the A_RETURN node
  this= mkastnode(A_RETURN, e, NULL, NULL);

  // Get the ';'
  semi();
  return(this);
}

//- function_call= IDENT LPAREN expression_list? RPAREN
//-              | IDENT LPAREN named_expression_list RPAREN
//-
static ASTnode *function_call(void) {
  ASTnode *s, *e=NULL;
  Sym *sym;

  // Make an IDENT node from the current token
  s= mkastleaf(A_IDENT, NULL, false, NULL, 0);
  s->strlit= Thistoken.tokstr;

  // Get the function's Sym pointer
  sym= find_symbol(s->strlit);
  if (sym==NULL || sym->symtype != ST_FUNCTION)
    fatal("Unknown function %s()\n", s->strlit);

  // Skip the identifier and get the left parenthesis
  scan(&Thistoken);
  lparen();

  // If the next token is not a right parenthesis,
  if (Thistoken.token != T_RPAREN) {
    // See if the lookahead token is an '='.
    // If so, we have a named expression list
    scan(&Peektoken);
    if (Peektoken.token == T_ASSIGN) {
      e= named_expression_list();
    } else {
      // No, so get an expression list
      e= expression_list();
    }
  }

  // Get the right parenthesis
  rparen();
  
  // Build the function call node and set its type
  s= mkastnode(A_FUNCCALL,s,NULL,e);
  s->type= sym->type;
  return(s);
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

// A named expression is an expression which is preceded by
// an identifier and an '=' sign. It looks very much like
// the short_assign_stmt, but we can't use that as  the
// identifier's name belongs to the function we are calling,
// it's not a variable in the function we are parsing.
//
//- named_expression_list= IDENT ASSIGN expression
//-                        (COMMA named_expression_list)*
//-
static ASTnode *named_expression_list(void) {
  ASTnode *first, *expr, *this, *next;

  // Build an ASSIGN node with the identifier's
  // name, then skip the identifier
  first= this= mkastleaf(A_ASSIGN, NULL, false, NULL, 0);
  first->strlit= Thistoken.tokstr;
  scan(&Thistoken);

  // Check for the '=' token
  match(T_ASSIGN, true);

  // Get the expression and attach it as the left child
  expr= expression();
  first->left= expr;

  while (1) {
    // If no comma, stop now
    if (Thistoken.token != T_COMMA) break;

    // Skip the comma
    // Get the next named expression and link it in
    scan(&Thistoken);
    next= named_expression_list();
    this->right= next; this= next;
  }

  return(first);
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

  // Get the relational expression and invert if required.
  // We can't do bitwise operations on a boolean
  left= relational_expression();
  if (invert) {
    cant_do(left, ty_bool, "Cannot do bitwise operations on a boolean\n");
    left= unarop(left, A_INVERT);
  }

  // See if we have more relational operations
  while (loop) {
    switch(Thistoken.token) {
    case T_AMPER:
      scan(&Thistoken);
      right= relational_expression();
      cant_do(left, ty_bool, "Cannot do bitwise operations on a boolean\n");
      cant_do(right, ty_bool, "Cannot do bitwise operations on a boolean\n");
      left= binop(left, right, A_AND); break;
    case T_OR:
      scan(&Thistoken);
      right= relational_expression();
      cant_do(left, ty_bool, "Cannot do bitwise operations on a boolean\n");
      cant_do(right, ty_bool, "Cannot do bitwise operations on a boolean\n");
      left= binop(left, right, A_OR); break;
    case T_XOR:
      scan(&Thistoken);
      right= relational_expression();
      cant_do(left, ty_bool, "Cannot do bitwise operations on a boolean\n");
      cant_do(right, ty_bool, "Cannot do bitwise operations on a boolean\n");
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
      cant_do(left, ty_bool, "Cannot do shift operations on a boolean\n");
      cant_do(right, ty_bool, "Cannot do shift operations on a boolean\n");
      left= binop(left, right, A_LSHIFT); break;
    case T_RSHIFT:
      scan(&Thistoken); right= additive_expression();
      cant_do(left, ty_bool, "Cannot do shift operations on a boolean\n");
      cant_do(right, ty_bool, "Cannot do shift operations on a boolean\n");
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
  if (negate) {
    cant_do(left, ty_bool, "Cannot do additive operations on a boolean\n");
    left= unarop(left, A_NEGATE);
  }

  // See if we have more additive operations
  while (loop) {
    switch(Thistoken.token) {
    case T_PLUS:
      scan(&Thistoken); right= multiplicative_expression();
      cant_do(left, ty_bool, "Cannot do additive operations on a boolean\n");
      cant_do(right, ty_bool, "Cannot do additive operations on a boolean\n");
      left= binop(left, right, A_ADD); break;
    case T_MINUS:
      scan(&Thistoken); right= multiplicative_expression();
      cant_do(left, ty_bool, "Cannot do additive operations on a boolean\n");
      cant_do(right, ty_bool, "Cannot do additive operations on a boolean\n");
      left= binop(left, right, A_SUBTRACT); break;
    default:
      loop=false;
    }
  }

  // Nope, return what we have
  return(left);
}

//- multiplicative_expression= unary_expression
//-                          ( STAR  unary_expression
//-                          | SLASH unary_expression
//-                          )*
//-
static ASTnode *multiplicative_expression(void) {
  ASTnode *left, *right;
  bool loop=true;

  // Get the first unary_expression
  left= unary_expression();

  // See if we have more multiplicative operations
  while (loop) {
    switch(Thistoken.token) {
    case T_STAR:
      scan(&Thistoken); right= unary_expression();
      cant_do(left, ty_bool, "Cannot do multiplicative operations on a boolean\n");
      cant_do(right, ty_bool, "Cannot do multiplicative operations on a boolean\n");
      left= binop(left, right, A_MULTIPLY);
      break;
    case T_SLASH:
      scan(&Thistoken); right= unary_expression();
      cant_do(left, ty_bool, "Cannot do multiplicative operations on a boolean\n");
      cant_do(right, ty_bool, "Cannot do multiplicative operations on a boolean\n");
      left= binop(left, right, A_DIVIDE);
      break;
    default:
      loop=false;
    }
  }

  // Nope, return what we have
  return(left);
}

// Parse a unary expression and return
// a sub-tree representing it.
//- unary_expression= primary_expression
//-                 | STAR unary_expression
//-                 | AMPER primary_expression
//-
static ASTnode *unary_expression(void) {
  ASTnode *u;

  switch(Thistoken.token) {
  case T_AMPER:
    // Get the next token and parse it
    scan(&Thistoken);
    u= primary_expression();

    // Ensure that it's an identifier
    if (u->op != A_IDENT)
      fatal("& operator must be followed by an identifier\n");

    // Now change the operator to A_ADDR and the type to
    // a pointer to the original type. Mark the identifier
    // as needing a real memory address
    u->op= A_ADDR;
    u->type= pointer_to(u->type);
    u->sym->has_addr= true;
    return(u);
  case T_STAR:
    // Get the next token and parse it
    // recursively as a unary expression.
    // Make it an rvalue
    scan(&Thistoken);
    u= unary_expression();
    u->rvalue= true;

    // Ensure the tree's type is a pointer
    if (!is_pointer(u->type))
      fatal("* operator must be followed by an expression of pointer type\n");

    // Prepend an A_DEREF operation to the tree
    // and update the tree's type. Mark the child
    // as being an rvalue
    u->rvalue= true;
    u= mkastnode(A_DEREF, u, NULL, NULL);
    u->type= value_at(u->left->type);
    u->rvalue= true;
    return(u);
  }

  // default:
  return(primary_expression());
}

//- primary_expression= NUMLIT
//-                   | STRLIT
//-                   | TRUE
//-                   | FALSE
//-                   | NULL
//-                   | variable
//-                   | ENUMVAL
//-                   | function_call
//-
static ASTnode *primary_expression(void) {
  ASTnode *f;
  Sym *sym;
  Type *ty;

  switch(Thistoken.token) {
  case T_NUMLIT:
    // Build an ASTnode with the numeric value and suitable type
    ty= parse_litval(Thistoken.numval, Thistoken.numtype);
    f= mkastleaf(A_NUMLIT, ty, true, NULL, Thistoken.numval.uintval);
    scan(&Thistoken);
    break;
  case T_STRLIT:
    // Build an ASTnode with the string literal and ty_int8ptr type
    f= mkastleaf(A_STRLIT, ty_int8ptr, false, NULL, 0);
    f->strlit= Thistoken.tokstr;
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
  case T_NULL:
    f= mkastleaf(A_NUMLIT, ty_voidptr, true, NULL, 0);
    scan(&Thistoken);
    break;
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
  default:
    fatal("Unknown token as a primary_expression: %s\n",
			get_tokenstr(Thistoken.token));
  }

  return(f);
}

//- variable= IDENT
//-         | STAR IDENT
//-         | IDENT DOT IDENT
//-
static ASTnode *variable(void) {
  Memb *memb;
  Type *ty;
  ASTnode *off;
  bool is_valueat= false;

  // Do we have a leading '*' ?
  if (Thistoken.token == T_STAR) {
    // Yes, mark as such and skip it
    is_valueat= true;
    scan(&Thistoken);
  }

  // Make an IDENT leaf node with the identifier in Thistoken
  ASTnode *n= mkastleaf(A_IDENT, NULL, false, NULL, 0);
  n->strlit= Thistoken.tokstr;
  n= mkident(n);		// Check variable exists, get its type
  scan(&Thistoken);

  // No following '.'
  if (Thistoken.token != T_DOT) {
    // We don't have a "value at"
    if (is_valueat == false)
      return(n);

    // Get the identifier's type.
    // Add a DEREF node to the identifier
    // and set its type. Set the child as
    // an rvalue
    ty= n->type;
    n->rvalue= true;
    n= mkastnode(A_DEREF, n, NULL, NULL);
    n->type= value_at(ty);
    return(n);
  }

  // We have a '.'. Skip it.
  // Check that the symbol is of struct type
  scan(&Thistoken);
  
  if (n->type->kind != TY_STRUCT)
    fatal("%s is not a struct, cannot use '.'\n", n->strlit);

  // Check that the pointer depth is 0 or 1
  if (n->type->ptr_depth > 1)
    fatal("%s is not a struct or struct pointer, cannot use '.'\n", n->strlit);
  
  // If the variable is a struct (not a pointer), get its address
  if (n->type->ptr_depth == 0) {
    n->op= A_ADDR;
    n->type= pointer_to(n->type);
    n->sym->has_addr= true;
  }

  // Check that the identifier following the '.'
  // is a member of the struct
  for (memb= n->sym->type->memb; memb!= NULL; memb=memb->next)
    if (!strcmp(memb->name, Thistoken.tokstr)) break;

  if (memb==NULL)
    fatal("No member named %s in struct %s\n", Thistoken.tokstr, n->strlit);

  // Skip the identifier
  scan(&Thistoken);

  // Make a NUMLIT node with the member's offset
  off= mkastleaf(A_NUMLIT, ty_uint64, true, NULL, memb->offset);

  // Add the struct's address and the offset together
  n= binop(n, off, A_ADD);

  // Mark the address as a pointer to
  // the dereference'd type
  n->type= pointer_to(memb->type);

  // Now dereference this address
  // and mark it with the correct type
  n= mkastnode(A_DEREF, n, NULL, NULL);
  n->type= memb->type;
  n->rvalue= true;
  return(n);
}
