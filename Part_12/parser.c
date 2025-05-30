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
static void function_declaration(ASTnode *decl, int visibility);
static ASTnode *function_prototype(ASTnode *func);
static ASTnode *typed_declaration_list(void);
static void enum_declaration(void);
static void struct_declaration(char *name);
static ASTnode *union_declaration(void);
static int get_visibility(void);
static void global_var_declaration(ASTnode *decl, int visibility);
static ASTnode *decl_initialisation(void);
static ASTnode *array_typed_declaration(void);
static ASTnode *typed_declaration(void);
static int64_t array_size();
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
static ASTnode *abort_stmt(void);
static ASTnode *break_stmt(void);
static ASTnode *continue_stmt(void);
static ASTnode *try_stmt(void);
static ASTnode *switch_stmt(void);
static ASTnode *fallthru_stmt(void);
static ASTnode *function_call(void);
static ASTnode *bracketed_expression_list(void);
static ASTnode *bracketed_expression_element(void);
static ASTnode *expression_list(void);
static ASTnode *named_expression_list(void);
static ASTnode *expression(void);
static ASTnode *bitwise_expression(void);
static ASTnode *boolean_expression(void);
static ASTnode *logical_and_expression(void);
static ASTnode *logical_or_expression(void);
static ASTnode *relational_expression(void);
static ASTnode *shift_expression(void);
static ASTnode *additive_expression(void);
static ASTnode *multiplicative_expression(void);
static ASTnode *unary_expression(void);
static ASTnode *primary_expression(void);
static ASTnode *sizeof_expression(void);
static ASTnode *postfix_variable(ASTnode *this);

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
  int visibility;

  // Loop parsing global declarations until we hit the EOF
  while (Thistoken.token != T_EOF) {
    switch(Thistoken.token) {
    case T_TYPE:
      type_declaration(); break;
    case T_ENUM:
      enum_declaration(); break;
    case T_PUBLIC:
    case T_EXTERN:
    default:
      // This could be a function or variable declaration.
      // Get any optional visibility
      visibility= get_visibility();

      // Get the typed declaration
      decl= array_typed_declaration();

      // If the next token is an LPAREN, 
      // it's a function declaration,
      // otherwise a global variable declaration
      if (Thistoken.token == T_LPAREN) {
	// Functions cannot return arrays
	if (decl->rvalue == true)
	  fatal("can't declare %s() to be an array\n", decl->strlit);
        function_declaration(decl, visibility);
      } else
	global_var_declaration(decl, visibility);
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

  // Skip the ENUM keyword and get the left brace
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
    sym= add_symbol(name, ST_ENUM, ty, SV_PRIVATE);
    sym->count= val.intval;

    // Increment the enum value
    val.intval++;

    // If we have a right brace, stop looping.
    // Otherwise check for and absorb a comma
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
  Sym *thismemb, *lastmemb;
  int biggest_memb=0;
  ASTnode *astbiggest;
  int size;
  Type *type;

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

    offset= genalign(astbiggest->type, offset);
    if (O_logmisc)
      fprintf(Debugfh, "set biggest in union to %d offset %d\n",
				biggest_memb, offset);
  }

  // Walk the list of new members to add to the struct
  for (astmemb= asthead; astmemb != NULL; astmemb= astmemb->mid) {
    // Do not let opaque types in a struct
    if (astmemb->type->size == 0)
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
    // Determine the size of the member. Deal with
    // array members: multiply by the number of elements.
    size= astmemb->type->size;
    if (astmemb->rvalue == true) {
      // Get the type that the array points at
      type= value_at(astmemb->type);
      size= type->size;

      if (O_logmisc)
        fprintf(Debugfh, "multiplying type %s size %d by count %d\n",
		get_typename(astmemb->type), size, astmemb->count);

      size= size * astmemb->count;
    }

    if (O_logmisc)
      fprintf(Debugfh, "Building a struct, %s has size %d\n", astmemb->strlit, size);

    // Create the Sym struct, add the name and type
    thismemb= (Sym *)Calloc(sizeof(Sym));
    thismemb->name= astmemb->strlit;
    thismemb->type= astmemb->type;

    // Mark it as an array if needed
    if (astmemb->rvalue == true) {
      thismemb->count= astmemb->count;
      thismemb->symtype= ST_VARIABLE;
    }

    // Is this the first member?
    if (strtype->memb == NULL) {
      thismemb->offset= 0;
      strtype->memb= thismemb;
      if (O_logmisc)
        fprintf(Debugfh, "%s member %s: offset %d size %d\n",
	  get_typename(thismemb->type),
	  thismemb->name, thismemb->offset, size);
      return(size);
    }

    // Not the first member. Get the aligned offset for it
    // Then append it. If a union, we already have the right offset
    if (isunion == false)
      offset= genalign(astmemb->type, offset);
    thismemb->offset= offset;
    lastmemb->next= thismemb;

    // Update the offset if not a union
    if (isunion == false)
      offset= offset + size;

    if (O_logmisc)
      fprintf(Debugfh, "%s member %s: offset %d size %d\n",
	get_typename(thismemb->type),
	thismemb->name, thismemb->offset, size);
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
//- struct_item= array_typed_declaration
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
      // Get an array typed declaration
      // and add the member to the struct
      astmemb= array_typed_declaration();
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

// Global variables and functions have a default private
// visibility which can be changed by these keywords.
//
//- visibility= ( PUBLIC | EXTERN )?
//-
static int get_visibility(void) {
  int visibility= SV_PRIVATE;
  switch(Thistoken.token) {
    case T_PUBLIC:
      visibility= SV_PUBLIC;
      scan(&Thistoken);
      break;
    case T_EXTERN:
      visibility= SV_EXTERN;
      scan(&Thistoken);
      break;
  }
  return(visibility);
}

// Given a symbol and a bracketed expression list,
// check that the list is suitable for the symbol.
// Also output the values to the assembly file.
// offset holds the byte offset of the value from the
// base of any struct/array. is_element is true when
// sym is an array and we are outputting elements
void check_bel(Sym *sym, ASTnode *list, int offset, bool is_element) {
  ASTnode *wide;
  Sym *memb;
  int i;
  Type *type= sym->type;

  // No list, we ran out of values in the list
  if (list == NULL)
    fatal("not enough values in the expression list\n");

  // No symbol, too many values
  if (sym == NULL)
    fatal("too many values in the expression list\n");

  // This is an element of an array,
  // get the type of each element
  if (is_element == true)
    type= value_at(type);

  // The list doesn't start with an A_BEL, so
  // it is a scalar expression
  if (list->op != A_BEL) {
    // Error if the symbol is a struct, or an array
    // and this isn't an element of the array
    if (((sym->count>0) && (is_element == false)) || (sym->type->kind == TY_STRUCT))
      fatal("%s needs an initialisation list\n", sym->name);

    // Make sure the expression matches the symbol's type
    wide= widen_type(list, type, 0);
    if (wide == NULL)
      fatal("initialisation value not of type %s\n",
			get_typename(type));

    // It also has to be a literal value
    if ((list->op != A_NUMLIT) && (list->op != A_STRLIT))
      fatal("initialisation value not a literal value\n");

    // Update the list element's type
    list->type= type;

    // Output the value at the offset
    if (O_logmisc)
      fprintf(Debugfh, "globsymval offset %d\n", offset);
    cgglobsymval(list, offset);
    return;
  }

  // The list starts with an A_BEL. Skip it
  list= list->left;

  // We need the symbol to be a struct or array
  if ((sym->count==0) && (sym->type->kind != TY_STRUCT)) {
    fatal("%s cannot have an initialisation list\n", sym->name);
  }

  // The symbol is an array. Update the type.
  // Use the count of elements and walk the list
  if (sym->count > 0) {
    type= value_at(type);
    for (i= 0; i < sym->count; i++, list=list->mid) {
      check_bel(sym, list, offset + i * type->size, true);
    }

    return;
  }

  // If this is a struct
  if (sym->type->kind == TY_STRUCT) {
    // Walk the list of struct members and
    // check each against the list value
    for (memb= sym->type->memb; memb != NULL && list != NULL;
				memb=memb->next, list=list->mid) {
       check_bel(memb, list, offset + memb->offset, false);
    }

    return;
  }

  fatal("can't get here in check_bel()\n");
}

// Parse a global variable declaration.
// We receive the typed_declaration in decl
//
//- global_var_declaration= visibility array_typed_declaration decl_initialisation SEMI
//-                       | visibility array_typed_declaration SEMI
//-
void global_var_declaration(ASTnode *decl, int visibility) {
  ASTnode *init=NULL;
  Sym *sym;

  // See if the variable's name already exists
  if (find_symbol(decl->strlit) != NULL)
    fatal("symbol %s already exists\n", decl->strlit);

  // Add the symbol and type as a global
  sym= add_symbol(decl->strlit, ST_VARIABLE, decl->type, visibility);

  // If the declaration was marked as an array,
  // update the symbol
  if (decl->rvalue == true) {
    sym->count= decl->count;
  }

  // If we have an '=', we have an initialisation
  if (Thistoken.token == T_ASSIGN) {
    init= decl_initialisation();
    if (O_logmisc) {
      fprintf(Debugfh, "%s initialisation:\n", decl->strlit);
      dumpAST(init, 0);
    }

    // Yes, an initialisation list
    if (sym->visibility == SV_EXTERN)
      fatal("cannot intiialise an external variable\n");

    // Start the output of the variable.
    // Don't zero it
    cgglobsym(sym, false);

    // Check the initialisation (list) against the symbol.
    // Also output the values in the list
    check_bel(sym, init, 0, false);

    // End the output of the variable
    cgglobsymend(sym);

    // It's OK, so save it in the symbol
    sym->initlist= init;
  } else {

    // No initialisation list.
    // Output zeroes instead
    if (sym->visibility != SV_EXTERN) {
      cgglobsym(sym, true);
      cgglobsymend(sym);
    }
  }

  // Get the trailing semicolon
  semi();
}

// Parse a declaration initialisation
//
//- decl_initialisation= ASSIGN expression
//-                    | ASSIGN bracketed_expression_list
//-
static ASTnode *decl_initialisation(void) {

  // Skip the '='
  scan(&Thistoken);

  // Get either an expression or a bracketed_expression_list
  if (Thistoken.token == T_LBRACE)
    return(bracketed_expression_list());
  else
    return(expression());
}

// Parse a single function declaration
//
//- function_declaration= visibility function_prototype statement_block
//-                     | visibility function_prototype SEMI
//-
static void function_declaration(ASTnode *func, int visibility) {
  ASTnode *s;

  // Get the function's prototype.
  func= function_prototype(func);

  // If the next token is a semicolon
  if (Thistoken.token == T_SEMI) {
    // Add the function prototype to the symbol table
    add_function(func, func->left, visibility);

    // Skip the semicolon and return
    scan(&Thistoken); return;
  }

  // It's not a prototype, so we expect a statement block now.
  // Set Thisfunction to the function's symbol structure
  if (visibility == SV_EXTERN)
    fatal("cannot declare an extern function with a body\n");
  declare_function(func, visibility);
  Thisfunction= find_symbol(func->strlit);
  s= statement_block(Thisfunction);
  gen_func_statement_block(s);
}

// Parse a function prototype and
// return an ASTnode with the details
//
//- function_prototype= ( typed_declaration LPAREN typed_declaration_list RPAREN
//-                     | typed_declaration LPAREN VOID RPAREN
//-                     | typed_declaration LPAREN ELLIPSIS RPAREN
//-                     ) (THROWS typed_declaration )?
//-
static ASTnode *function_prototype(ASTnode *func) {
  ASTnode *paramlist=NULL;
  ASTnode *astexcept;
  Type *basetype;

  // We already have the typed declaration in func.
  // Check the next token.
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

  // Get the ')' and add the params to the function
  rparen();
  func->left= paramlist;

  // If we have a THROWS
  if (Thistoken.token == T_THROWS) {
    scan(&Thistoken);

    // Get the name and base type of the exception variable
    astexcept= typed_declaration();
    basetype= value_at(astexcept->type);

    // The type must be a pointer to a struct which
    // has an int32 as the first member
    if ((astexcept->type->kind != TY_STRUCT) ||
        (astexcept->type->ptr_depth != 1)    ||
        (basetype->memb == NULL)             ||
        (basetype->memb->type != ty_int32))
      fatal("variable %s not suitable to hold an exception\n",
				astexcept->strlit);

    // Build a Sym node with the variable's name
    // and type, and add it to the ASTnode
    add_sym_to(&(func->sym), astexcept->strlit, ST_VARIABLE, astexcept->type);
    func->sym->visibility= SV_LOCAL;
  }

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

// Get a typed declaration with an optional trailing array size
// and return it as an ASTnode with the type updated and the
// count set as the array's size.
//
//- array_typed_declaration= typed_declaration (array_size)?
//- 
static ASTnode *array_typed_declaration(void) {
  ASTnode *this;
  int64_t size;

  // Get the typed declaration
  this= typed_declaration();

  // If next token is an '['
  if (Thistoken.token == T_LBRACKET) {
    // Get the array's size
    // and change the type
    // Set rvalue to mark it as an array
    size= array_size();
    this->count= size;
    this->rvalue= true;
    this->type= pointer_to(this->type);
  }

  return(this);
}

// Get the size of an array
//
//- array_size= LBRACKET NUMLIT RBRACKET
//-
static int64_t array_size() {
  int64_t size;

  // Skip the left bracket
  scan(&Thistoken);

  // We must see a NUMLIT
  if (Thistoken.token != T_NUMLIT)
    fatal("array size missing\n");

  // We need an integer NUMLIT which isn't negative
  if ((Thistoken.numtype == NUM_FLT) || (Thistoken.numval.intval < 0))
    fatal("array size must be a positive integer literal\n");

  // Get the size, skip the NUMLIT
  size= Thistoken.numval.intval;
  scan(&Thistoken);

  // Skip the ']'
  match(T_RBRACKET, true);
  
  return(size);
}

// Get a symbol declaration along with its type as an ASTnode.
//
//- typed_declaration= type IDENT
//-
static ASTnode *typed_declaration(void) {
  ASTnode *identifier;
  Type *t;

  // Get the type and skip past it.
  t= type(false);

  // Get the identifier, set its type
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
    fatal("Unknown type %s\n", Text);

  // Get the next token
  scan(&Thistoken);

  // Loop counting the number of STAR tokens
  // and getting a a pointer to the previous type
  while (Thistoken.token== T_STAR) {
    scan(&Thistoken); t= pointer_to(t);
  }

  return(t);
}

// A statement block is either a single procedural statement,
// or '{' ... '}' with zero or more declarations first,
// followed by any procedural statements. The func
// argument is non-NULL when we are starting a new
// function.
//
//- statement_block= LBRACE declaration_stmts procedural_stmts RBRACE
//-                | procedural_stmt
//-
static ASTnode *statement_block(Sym *func) {
  ASTnode *s=NULL, *d=NULL;

  // See if we have a single procedural statement
  s= procedural_stmt();
  if (s != NULL)
    return(s);

  // No, so parse a block surrounded by '{' ... '}'
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
//- declaration_stmts= ( array_typed_declaration decl_initialisation
//-                    | array_typed_declaration SEMI
//-                    )*
//-
static ASTnode *declaration_stmts(void) {
  ASTnode *d, *e= NULL;
  ASTnode *this;

  // Get one declaration statement
  d= array_typed_declaration();

  // If there is an '=' next, we have an assignment
  if (Thistoken.token == T_ASSIGN) {
    e= decl_initialisation();
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
//-                   | abort_stmt
//-                   | break_stmt
//-                   | continue_stmt
//-                   | try_stmt
//-                   | switch_stmt
//-                   | fallthru_stmt
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
  case T_ABORT:
    return(abort_stmt());
  case T_BREAK:
    return(break_stmt());
  case T_CONTINUE:
    return(continue_stmt());
  case T_TRY:
    return(try_stmt());
  case T_SWITCH:
    return(switch_stmt());
  case T_FALLTHRU:
    return(fallthru_stmt());
  case T_STAR:
    return(assign_stmt());
  case T_IDENT:
    // Get the next token.
    scan(&Peektoken);

    // If it's a '(' then it's a function call.
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

//- short_assign_stmt= postfix_variable ASSIGN expression
//-                  | postfix_variable POSTINC
//-                  | postfix_variable POSTDEC
//-
//-
static ASTnode *short_assign_stmt(void) {
  ASTnode *v, *e, *diff;
  int i, starcount=0;

  // Count the number of leading '*'s
  while (Thistoken.token == T_STAR) {
    starcount++;
    scan(&Thistoken);
  }

  // Get the variable
  v= postfix_variable(NULL);

  // Do we have a '++' or '--' following?
  // If so, build an assignment statement
  // with either an ADD or a SUBTRACT
  if (Thistoken.token == T_POSTINC) {
    // Get the variable as an rvalue
    e= (ASTnode *) Calloc(sizeof(ASTnode));
    memcpy(e, v, sizeof(ASTnode));
    e->rvalue= true;
    scan(&Thistoken);

    // Build a NUMLIT node with 1 in it
    // and add it from the rval variable
    diff= mkastleaf(A_NUMLIT, ty_int8, true, NULL, 1);
    e= binop(e, diff, A_ADD);

    // Now do the assignment
    return(assignment_statement(v, e));
  }

  if (Thistoken.token == T_POSTDEC) {
    // Get the variable as an rvalue
    e= (ASTnode *) Calloc(sizeof(ASTnode));
    memcpy(e, v, sizeof(ASTnode));
    e->rvalue= true;
    scan(&Thistoken);

    // Build a NUMLIT node with 1 in it,
    // and subtract it from the rval variable
    diff= mkastleaf(A_NUMLIT, ty_int8, true, NULL, 1);
    e= binop(e, diff, A_SUBTRACT);

    // Now do the assignment
    return(assignment_statement(v, e));
  }

  // Check for an '=' then get the expression
  match(T_ASSIGN, true); e= expression();

  // Check that the variable's pointer depth is the same or
  // greater than our star count
  if (v->type->ptr_depth < starcount)
    fatal("Too many dereferences for type %s\n",
			get_typename(v->type));

  // Add starcount DEREF nodes to the variable
  for (i=0; i< starcount; i++) {
    v= mkastnode(A_DEREF, v, NULL, NULL);
    v->type= value_at(v->left->type);
  }

  return(assignment_statement(v, e));
}

//- if_stmt= IF LPAREN boolean_expression RPAREN statement_block
//-          (ELSE statement_block)?
//-
static ASTnode *if_stmt(void) {
  ASTnode *e, *t, *f=NULL;

  // Skip the IF, check for a left parenthesis.
  // Get the expression, right parenthesis
  // and the statement block
  scan(&Thistoken);
  lparen();
  e= boolean_expression();
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

//- while_stmt= WHILE LPAREN boolean_expression RPAREN statement_block
//-           | WHILE LPAREN TRUE RPAREN statement_block
//-
static ASTnode *while_stmt(void) {
  ASTnode *e, *s;

  // Skip the WHILE, check for a left parenthesis.
  scan(&Thistoken);
  lparen();

  // If we have a TRUE token, build an ASTnode for it
  if (Thistoken.token == T_TRUE) {
    e= mkastleaf(A_NUMLIT, ty_bool, true, NULL, 1);
    scan(&Thistoken);
  } else {
    // Otherwise, get the expression
    e= boolean_expression();
  }

  // Get the trailing right parenthesis
  // and the statement block
  rparen();
  s= statement_block(NULL);

  return(mkastnode(A_WHILE, e, s, NULL));
}

//- for_stmt= FOR LPAREN short_assign_stmt? SEMI
//-                      boolean_expression? SEMI
//-                      short_assign_stmt? RPAREN statement_block
//-
static ASTnode *for_stmt(void) {
  ASTnode *i=NULL, *e, *send=NULL, *s;

  // Skip the FOR, check for a left parenthesis.
  scan(&Thistoken);
  lparen();

  // If we don't have a semicolon, get the initial statement.
  // Then get the semicolon
  if (Thistoken.token != T_SEMI)
    i= short_assign_stmt();
  semi();

  // If we don't have a semicolon, get the condition expression.
  // Otherwise, make a TRUE node instead
  if (Thistoken.token != T_SEMI) {
    e= boolean_expression();
  } else {
    e= mkastleaf(A_NUMLIT, ty_bool, true, NULL, 1);
  }
  semi();

  // If we don't have a right parentheses, get the change statement
  if (Thistoken.token != T_RPAREN)
    send= short_assign_stmt();

  // Get the right parenthesis and the statement block for the loop
  rparen();
  s= statement_block(NULL);

  // Glue the end code after the statement block.
  // Set the rvalue true to indicate that the
  // right child is the end code of a FOR loop.
  // We need this to make 'continue' work in a FOR loop.
  s= mkastnode(A_GLUE, s, NULL, send);
  s->rvalue= true;

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

//- abort_stmt= ABORT SEMI
//-
static ASTnode *abort_stmt(void) {
  ASTnode *this;
  
  // Skip the 'abort' token
  scan(&Thistoken);

  // Build the A_ABORT node
  this= mkastnode(A_ABORT, NULL, NULL, NULL);

  // Get the ';'
  semi();
  return(this);
}

//- break_stmt= BREAK SEMI
//-
static ASTnode *break_stmt(void) {
  ASTnode *this;
  
  // Skip the 'break' token
  scan(&Thistoken);

  // Build the A_BREAK node
  this= mkastnode(A_BREAK, NULL, NULL, NULL);

  // Get the ';'
  semi();
  return(this);
}

//- continue_stmt= CONTINUE SEMI
//-
static ASTnode *continue_stmt(void) {
  ASTnode *this;
  
  // Skip the 'continue' token
  scan(&Thistoken);

  // Build the A_CONTINUE node
  this= mkastnode(A_CONTINUE, NULL, NULL, NULL);

  // Get the ';'
  semi();
  return(this);
}


//- try_statement= TRY LPAREN IDENT RPAREN statement_block CATCH statement_block
//-
static ASTnode *try_stmt(void) {
  Sym *sym;

  // Skip the 'try' and get the left parenthesis
  scan(&Thistoken);
  lparen();

  // Ensure we have an identifier and get its symbol
  match(T_IDENT, false);
  sym= find_symbol(Thistoken.tokstr);
  if (sym == NULL)
    fatal("unknown symbol %s\n", Thistoken.tokstr);

  // Check that the symbol's type is a struct with
  // an int32 as the first member
  if ((sym->type->kind != TY_STRUCT) ||
      (sym->type->ptr_depth != 0)    ||
      (sym->type->memb == NULL)      ||
      (sym->type->memb->type != ty_int32))
    fatal("variable %s not suitable to hold an exception\n",
				Thistoken.tokstr);

  // Make an A_TRY leaf node with the given symbol
  ASTnode *n= mkastleaf(A_TRY, NULL, false, sym, 0);
  n->strlit= Thistoken.tokstr;
  n= mkident(n);

  // Skip the identifier and right parenthesis
  scan(&Thistoken);
  rparen();

  // Get the try statement block
  n->left= statement_block(NULL);

  // Get the 'catch'
  match(T_CATCH, true);

  // Get the catch statement block
  n->right= statement_block(NULL);
  return(n);
}

// Parse a switch statement, including the case statements and
// the default statement.
//
//- switch_stmt= SWITCH LPAREN expression RPAREN switch_stmt_block
//-
//- switch_stmt_block= ( case_stmt
//-                    | default_stmt
//-                    )+
//-
//- case_stmt= CASE expression COLON procedural_stmts?
//-
//- default_stmt= DEFAULT COLON procedural_stmts
//-
static ASTnode *switch_stmt(void) {
  ASTnode *left, *body, *n, *c;
  ASTnode *casetree = NULL, *casetail = NULL;
  bool inloop= true;
  bool seendefault= false;
  int ASTop, casecount=0;
  int64_t caseval=0;

  // Skip the 'switch' and '('
  scan(&Thistoken);
  lparen();

  // Get the switch expression, the ')' and the '{'
  left= expression();
  rparen();
  lbrace();

  // Ensure that this is an int literal
  if (!is_integer(left->type))
    fatal("switch expression not of integer type\n");

  // Build the AST node with the expression
  n= mkastnode(A_SWITCH, left, NULL, NULL);

  // Now parse the cases
  while (inloop) {
    switch (Thistoken.token) {
        // Leave the loop when we hit a '}'
      case T_RBRACE:
        if (casecount == 0)
          fatal("No cases in switch\n");
        inloop = false;
        break;
      case T_CASE:
      case T_DEFAULT:
        // Ensure this isn't after a previous 'default'
        if (seendefault)
          fatal("case or default after existing default\n");


        if (Thistoken.token == T_DEFAULT) {
          ASTop= A_DEFAULT;
          scan(&Thistoken);
          seendefault= true;
        } else {
          // Scan the case value if required
          ASTop= A_CASE;
          scan(&Thistoken);

	  // Get the case expression
	  left = expression();

	  // Ensure the case value is an integer literal
          if ((left->op != A_NUMLIT) || !is_integer(left->type))
            fatal("Expecting integer literal for case value\n");
	  caseval= left->litval.intval;

	  // Walk the list of existing case values to ensure
          // that there isn't a duplicate case value
          for (c = casetree; c != NULL; c = c->right)
            if (caseval == c->litval.intval)
              fatal("Duplicate case value\n");
        }

	// Scan the ':' and increment the casecount
        match(T_COLON, true);
        casecount++;

	// If the next token is a T_CASE, the existing case will fall
        // into the next case. Otherwise, parse the case body.
        if (Thistoken.token == T_CASE)
          body= NULL;
        else
          body= procedural_stmts();

	// Build a sub-tree with any statement block as the left child
        // and link it in to the growing A_CASE tree
        if (casetree == NULL) {
          casetree= casetail= mkastnode(ASTop, body, NULL, NULL);
        } else {
          casetail->right= mkastnode(ASTop, body, NULL, NULL);
          casetail = casetail->right;
        }

	// Copy the case value into the new node
	// Yes, we copy into the DEFAULT node, doesn't matter!
        casetail->litval.intval= caseval;
        break;
    default:
        fatal("Unexpected token in switch: %s\n",
			get_tokenstr(Thistoken.token));
    }
  }

  // We have a AST tree with the cases and any default. Put the
  // case count into the A_SWITCH node and attach the case tree.
  n->litval.intval= casecount;
  n->right= casetree;
  rbrace();

  return(n);
}

//- fallthru_stmt= FALLTHRU SEMI
//-
static ASTnode *fallthru_stmt(void) {

  // Skip the 'fallthru'
  scan(&Thistoken);
  semi();
  return(mkastnode(A_FALLTHRU, NULL, NULL, NULL));
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

//- bracketed_expression_list= LBRACE bracketed_expression_element
//-                                   (COMMA bracketed_expression_element)*
//-                            RBRACE
//-
static ASTnode *bracketed_expression_list(void) {
  ASTnode *bel, *this;

  // Skip the left brace
  scan(&Thistoken);

  // Make the BEL node which will hold the list
  bel= mkastnode(A_BEL, NULL, NULL, NULL);

  // Start with one bracketed expression element
  bel->left= this= bracketed_expression_element();

  // Loop trying to get more of them
  while (1) {
    // No more, so stop
    if (Thistoken.token != T_COMMA) break;

    // Get the next element and append it
    scan(&Thistoken);
    this->mid= bracketed_expression_element();
    this= this->mid;
  }

  // Skip the right brace and return the list
  scan(&Thistoken);
  return(bel);
}

//- bracketed_expression_element= expression
//-                             | bracketed_expression_list
//-
static ASTnode *bracketed_expression_element(void) {
  ASTnode *elem;

  // Parse one element and return it
  switch (Thistoken.token) {
  case T_LBRACE:
    elem= bracketed_expression_list();
    break;
  default:
    elem= expression();
  }

  return(elem);
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

// Try to optimise the AST tree that holds an expression
//
//- expression= bitwise_expression
//-
static ASTnode *expression(void) {
  return(optAST(bitwise_expression()));
}

//- bitwise_expression= ( INVERT boolean_expression
//-                     |        boolean_expression
//-                     )
//-                     ( AND boolean_expression
//-                     | OR  boolean_expression
//-                     | XOR boolean_expression
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

  // Get the expression and invert if required.
  // We can't do bitwise operations on a boolean
  left= boolean_expression();
  if (invert) {
    cant_do(left, ty_bool, "Cannot do bitwise operations on a boolean\n");
    left= unarop(left, A_INVERT);
  }

  // See if we have more bitwise operations
  while (loop) {
    switch(Thistoken.token) {
    case T_AMPER:
      scan(&Thistoken);
      right= boolean_expression();
      cant_do(left, ty_bool, "Cannot do bitwise operations on a boolean\n");
      cant_do(right, ty_bool, "Cannot do bitwise operations on a boolean\n");
      left= binop(left, right, A_AND); break;
    case T_OR:
      scan(&Thistoken);
      right= boolean_expression();
      cant_do(left, ty_bool, "Cannot do bitwise operations on a boolean\n");
      cant_do(right, ty_bool, "Cannot do bitwise operations on a boolean\n");
      left= binop(left, right, A_OR); break;
    case T_XOR:
      scan(&Thistoken);
      right= boolean_expression();
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

//- boolean_expression= logical_and_expression
//-
static ASTnode *boolean_expression(void) {
  return(logical_and_expression());
}


//- logical_and_expression= logical_or_expression
//-                       | logical_or_expression LOGAND logical_or_expression
//-
static ASTnode *logical_and_expression(void) {
  ASTnode *left, *right;

  // Get the logical OR expression
  left= logical_or_expression();

  // See if we have more logical AND operations
  while (1) {
    if (Thistoken.token != T_LOGAND) break;
    scan(&Thistoken); right= relational_expression();
    if ((left->type != ty_bool) || (right->type != ty_bool))
      fatal("Can only do logical AND on boolean types\n");
    left= binop(left, right, A_LOGAND);
    left->type= ty_bool;
  }

  return(left);
}

//- logical_or_expression= relational_expression
//-                      | relational_expression LOGOR relational_expression
//-
static ASTnode *logical_or_expression(void) {
  ASTnode *left, *right;

  // Get the relational expression
  left= relational_expression();

  // See if we have more logical OR operations
  while (1) {
    if (Thistoken.token != T_LOGOR) break;
    scan(&Thistoken); right= relational_expression();
    if ((left->type != ty_bool) || (right->type != ty_bool))
      fatal("Can only do logical OR on boolean types\n");
    left= binop(left, right, A_LOGOR);
    left->type= ty_bool;
  }

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

  // See if we have a shift operation
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
//-                          | MOD unary_expression
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
    case T_MOD:
      scan(&Thistoken); right= unary_expression();
      cant_do(left, ty_bool, "Cannot do multiplicative operations on a boolean\n");
      cant_do(right, ty_bool, "Cannot do multiplicative operations on a boolean\n");
      left= binop(left, right, A_MOD);
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
//-                   | ENUMVAL
//-                   | sizeof_expression
//-                   | postfix_variable
//-                   | function_call
//-                   | LPAREN expression RPAREN
//-
static ASTnode *primary_expression(void) {
  ASTnode *f;
  Sym *sym;
  Type *ty;

  switch(Thistoken.token) {
  case T_LPAREN:
    // Skip the left parentheses, get the expression,
    // skip the right parentheses and return
    scan(&Thistoken);
    f= expression();
    rparen();
    return(f);
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
  case T_SIZEOF:
    f= sizeof_expression();
    break;
  case T_IDENT:
    // Find out what sort of symbol this is
    sym= find_symbol(Thistoken.tokstr);
    if (sym == NULL)
      fatal("unknown symbol %s\n", Thistoken.tokstr);
    switch(sym->symtype) {
    case ST_FUNCTION: f= function_call(); break;
    case ST_VARIABLE: f= postfix_variable(NULL); break;
    case ST_ENUM: f= mkastleaf(A_NUMLIT, ty_bool,
			true, NULL, sym->count);
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

//- sizeof_expression= SIZEOF LPAREN type RPAREN
//-                  | SIZEOF LPAREN IDENT RPAREN
//-
static ASTnode *sizeof_expression(void) {
  ASTnode *e;
  Type *ty;
  Sym *sym;

  // Skip the keyword, get the '('
  scan(&Thistoken);
  lparen();

  switch(Thistoken.token) {
    case T_IDENT:
      // It's an identifier
      // Try to find the variable
      sym= find_symbol(Thistoken.tokstr);
      if (sym != NULL) {
        if (sym->symtype != ST_VARIABLE)
	  fatal("can only do sizeof(variable)\n");

        // If this is an array, return the number of elements
        if (sym->count > 0) {
	  e= mkastleaf(A_NUMLIT, ty_uint64, true, NULL, sym->count);
          scan(&Thistoken);
  	  rparen();
	  return(e);
        }

        // Otherwise set up ty to be the symbol's type
        ty= sym->type;
        scan(&Thistoken);
	break;
      }

      // We have an identifier, so it could be a user-defined type
    default:
      // It must be a type
      // Get the type in the parentheses
     ty= type(false);
  }

  // Can't get the size of an opaque type
  if (ty->size == 0)
    fatal("Can't get the size of opaque type %s\n", ty->name);

  // Make a NUMLIT node with the size of the type
  e= mkastleaf(A_NUMLIT, ty_uint64, true, NULL, ty->size);

  // Get the ')'
  rparen();
  return(e);
}

// Recursively parse a variable with postfix elements
//
//- postfix_variable= IDENT
//-                 | postfix_variable DOT IDENT
//-                 | postfix_variable LBRACKET expression RBRACKET
//-
static ASTnode *postfix_variable(ASTnode *n) {
  Sym *sym, *memb;
  Type *ty;
  ASTnode *off;
  ASTnode *e;

  // Deal with whatever token we currently have
  switch(Thistoken.token) {
  case T_IDENT:
    if (n != NULL)
      fatal("cannot use identifier %s here\n", Thistoken.tokstr);

    // An identifier. Make an IDENT leaf node
    // with the identifier in Thistoken
    n= mkastleaf(A_IDENT, NULL, false, NULL, 0);
    n->strlit= Thistoken.tokstr;
    n= mkident(n);		// Check variable exists, get its type
    scan(&Thistoken);
    return(postfix_variable(n));

  case T_LBRACKET:
    // An array access. Skip the token
    scan(&Thistoken);

    // Get the expression. Widen it to be int64
    e= expression();
    e= widen_type(e, ty_int64, 0);

    // Check that n is a pointer
    ty= n->type;
    if (ty->ptr_depth == 0)
      fatal("Cannot do array access on a scalar\n");

    // Do bounds checking if needed
    if (O_boundscheck == true) {
      // Is this an array not a pointer?
      if (n->op == A_IDENT) {
	sym= find_symbol(n->strlit);
	if (is_array(sym)) {
	  // Add the BOUND node with the array's name and size
	  e= mkastnode(A_BOUNDS, e, NULL, NULL);
	  e->count= sym->count;
	  e->strlit= sym->name;
	  e->type= ty_int64;
	}
      }
    }

    // Get the "value at" type
    ty= value_at(ty);

    // Make a NUMLIT node with the size of the base type
    off= mkastleaf(A_NUMLIT, ty_uint64, true, NULL, ty->size);

    // Multiply this by the expression's value
    e= binop(e, off, A_MULTIPLY);

    // Add on the array's base.
    // Mark this as a pointer
    e= binop(e, n, A_ADDOFFSET);
    e->type= n->type;

    // If this isn't a struct,
    // dereference this address
    // and mark it with the correct type
    if (ty->kind != TY_STRUCT) {
      e= mkastnode(A_DEREF, e, NULL, NULL);
      e->type= ty;
      e->rvalue= true;
    }

    // Get the trailing right bracket
    match(T_RBRACKET, true);
    return(postfix_variable(e));

  case T_DOT:
    // A member access. Skip the '.'
    scan(&Thistoken);

    // Check that n has struct type
    ty= n->type;
    if (ty->kind != TY_STRUCT)
    fatal("%s is not a struct, cannot use '.'\n", n->strlit);

    // Check that the pointer depth is 0 or 1
    if (ty->ptr_depth > 1)
    fatal("%s is not a struct or struct pointer, cannot use '.'\n", n->strlit);

    // If the variable is a struct (not a pointer), get its address
    if (ty->ptr_depth == 0) {
      if (n->sym != NULL)
	n->op= A_ADDR;
      n->type= pointer_to(ty);
    } else {
      // It is a pointer, so set ty to the base type
      ty= value_at(ty);
    }

    // Check that the identifier following the '.'
    // is a member of the struct
    for (memb= ty->memb; memb!= NULL; memb=memb->next)
      if (!strcmp(memb->name, Thistoken.tokstr)) break;

    if (memb==NULL)
      fatal("No member named %s in struct %s\n", Thistoken.tokstr, n->strlit);

    // Skip the identifier
    scan(&Thistoken);

    // Make a NUMLIT node with the member's offset
    off= mkastleaf(A_NUMLIT, ty_uint64, true, NULL, memb->offset);

    // Add the struct's address and the offset together
    n= binop(n, off, A_ADDOFFSET);

    // If the member is an array or struct, don't
    // dereference it, just set the node's type
    if (is_array(memb) || (memb->type->kind == TY_STRUCT)) {
      n->type= memb->type;
    } else {
      // The member isn't an array.
      // Mark the address as a pointer to
      // the dereference'd type
      n->type= pointer_to(memb->type);
      n= mkastnode(A_DEREF, n, NULL, NULL);
      n->type= memb->type;
    }
    n->rvalue= true;

    return(postfix_variable(n));

  default:
    // Nothing to do
    return(n);
  }
}
