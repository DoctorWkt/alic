// Parser for the alic compiler.
// (c) 2025 Warren Toomey, GPL3

// Note: You can grep '//-' this file to extract the grammar

#include "alic.ah"
#include "proto.ah"

// Forward declarations
void type_declaration(void);
void function_declaration(ASTnode * func, int visibility);
ASTnode *function_prototype(ASTnode * func);
ASTnode *typed_declaration_list(void);
void enum_declaration(void);
void struct_declaration(char *name);
ASTnode *union_declaration(void);
int get_visibility(void);
void global_var_declaration(ASTnode * decl, int visibility);
ASTnode *decl_initialisation(void);
ASTnode *array_typed_declaration(void);
ASTnode *typed_declaration(void);
int64 array_size(void);
Type *match_type(bool checkonly);
ASTnode *statement_block(Sym * func);
ASTnode *declaration_stmts(void);
ASTnode *procedural_stmts(void);
ASTnode *procedural_stmt(void);
ASTnode *assign_stmt(void);
ASTnode *short_assign_stmt(void);
ASTnode *if_stmt(void);
ASTnode *while_stmt(void);
ASTnode *for_stmt(void);
ASTnode *return_stmt(void);
ASTnode *abort_stmt(void);
ASTnode *break_stmt(void);
ASTnode *continue_stmt(void);
ASTnode *try_stmt(void);
ASTnode *switch_stmt(void);
ASTnode *fallthru_stmt(void);
ASTnode *function_call(void);
ASTnode *va_start_end_stmt(void);
ASTnode *bracketed_expression_list(void);
ASTnode *bracketed_expression_element(void);
ASTnode *expression_list(void);
ASTnode *named_expression_list(void);
ASTnode *expression(void);
ASTnode *ternary_expression(void);
ASTnode *bitwise_expression(void);
ASTnode *boolean_expression(void);
ASTnode *logical_and_expression(void);
ASTnode *logical_or_expression(void);
ASTnode *relational_expression(void);
ASTnode *shift_expression(void);
ASTnode *additive_expression(void);
ASTnode *multiplicative_expression(void);
ASTnode *unary_expression(void);
ASTnode *primary_expression(void);
ASTnode *sizeof_expression(void);
ASTnode *va_arg_expression(void);
ASTnode *unsigned_expression(void);
ASTnode *postfix_variable(ASTnode * n);

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
    switch (Thistoken.token) {
    case T_TYPE:
      type_declaration();
    case T_ENUM:
      enum_declaration();
    case T_PUBLIC:
    case T_EXTERN:
    default:
      // This could be a function or variable declaration.
      // Get any optional visibility
      visibility = get_visibility();

      // Get the typed declaration
      decl = array_typed_declaration();

      // If the next token is an LPAREN, 
      // it's a function declaration,
      // otherwise a global variable declaration
      if (Thistoken.token == T_LPAREN) {
	// Functions cannot return arrays
	if (decl.is_array == true)
	  fatal("can't declare %s() to be an array\n", decl.strlit);
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
void type_declaration(void) {
  char *typename;
  Type *basetype;
  Type *ptrtype;

  // Skip the TYPE keyword
  scan(&Thistoken);

  if (Thistoken.token != T_IDENT)
    fatal("Expecting a name after \"type\"\n");

  // Get the type's name
  typename = Thistoken.tokstr;

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
      basetype = match_type(true);
      if (basetype == NULL)
	fatal("Unknown base type in type declaration: %s\n",
	      get_tokenstr(Thistoken.token));

      // The base type might be followed by '*'
      // so parse this as well
      ptrtype = match_type(false);

      // Add the alias type to the list.
      // Make any pointer type at the same time
      new_type(TY_USER, basetype.size, 0, typename, basetype);
      if (basetype != ptrtype)
	new_type(TY_USER, ptrtype.size, ptrtype.ptr_depth,
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
void enum_declaration(void) {
  Litval val;
  Type *ty;
  Sym *sym;
  char *name;

  // Skip the ENUM keyword and get the left brace
  scan(&Thistoken);
  lbrace();

  // Loop getting the next enum item
  val.intval = 0;
  val.numtype = NUM_INT;
  while (true) {
    // Make sure that we have an identifier
    match(T_IDENT, true);
    name = Thistoken.tokstr;

    // If it's followed by an '='
    if (Thistoken.token == T_ASSIGN) {
      // Skip it and get the following numeric literal token
      scan(&Thistoken);
      match(T_NUMLIT, false);

      // Check that the literal value isn't a float
      if (Thistoken.litval.numtype == NUM_FLT)
	fatal("Cannot use a float literal as an enumerated value\n");

      // Update val to hold this literal value
      val.intval = Thistoken.litval.intval;

      // Skip the literal value
      scan(&Thistoken);
    }

    // Get a suitable type for the literal value
    ty = parse_litval(&val);

    // Add the enum's name and value to the global scope.
    // Then set the symbol's literal value
    if (find_symbol(name) != NULL)
      fatal("symbol %s already exists\n", name);
    sym = add_symbol(name, ST_ENUM, ty, SV_PRIVATE);
    sym.count = val.intval;

    // Increment the enum value
    val.intval++;

    // If we have a right brace, stop looping.
    // Otherwise check for and absorb a comma
    if (Thistoken.token == T_RBRACE)
      break;
    match(T_COMMA, true);
  }

  // Skip past the right brace and semicolon
  rbrace();
  semi();
}

// Given a pointer to a newly-created struct type,
// a single typed declaration or (if isunion is true)
// a list of union members, and the possible offset
// where the (first) member could start, add
// the declaration(s) as members to the type.
// Die if there are any semantic errors.
// Return the possible offset of the next member
int add_memb_to_struct(Type * strtype, ASTnode * asthead,
			      int offset, bool isunion) {
  ASTnode *astmemb;
  Sym *thismemb;
  Sym *lastmemb;
  int biggest_memb = 0;
  ASTnode *astbiggest;
  int size;
  Type *ty;

  if (O_logmisc)
    fprintf(Debugfh, "add_memb: offset is %d\n", offset);

  // If this is a union, find the biggest member
  // as this will be the one that we align on.
  if (isunion) {
    for (astmemb = asthead; astmemb != NULL; astmemb = astmemb.mid) {
      if (astmemb.ty.size > biggest_memb) {
	biggest_memb = astmemb.ty.size;
	astbiggest = astmemb;
      }
    }

    // Calculate the offset of the biggest union member
    offset = genalign(astbiggest.ty, offset);
    if (O_logmisc)
      fprintf(Debugfh, "set biggest in union to %d offset %d\n",
	      biggest_memb, offset);
  }

  // Walk the list of new members to add to the struct
  for (astmemb = asthead; astmemb != NULL; astmemb = astmemb.mid) {
    // Do not let opaque types in a struct
    if (astmemb.ty.size == 0)
      fatal("member of type %s cannot be in a struct\n",
	    get_typename(astmemb.ty));

    // Walk the existing members to a) check that we don't have
    // duplicate member names, and b) get a pointer to the last
    // member currently in the struct
    lastmemb = strtype.memb;
    thismemb = strtype.memb;
    for ( ; thismemb != NULL; ) {
      if (strcmp(thismemb.name, astmemb.strlit)==0)
	fatal("duplicate member name %s in struct declaration\n",
	      thismemb.name);
      lastmemb = thismemb;
      thismemb = thismemb.next;
    }

    // It's safe to add the astmemb to the struct.
    // Determine the size of the member. Deal with
    // array members: multiply by the number of elements.
    size = astmemb.ty.size;
    if (astmemb.is_array == true) {
      // Get the type that the array points at
      ty = value_at(astmemb.ty);
      size = ty.size;

      if (O_logmisc)
	fprintf(Debugfh, "multiplying type %s size %d by count %d\n",
		get_typename(astmemb.ty), size, astmemb.count);

      size = size * astmemb.count;
    }

    // Create the Sym struct, add the name and type
    thismemb = Calloc(sizeof(Sym));
    thismemb.name = astmemb.strlit;
    thismemb.ty = astmemb.ty;

    // Mark it as an array if needed
    if (astmemb.is_array == true) {
      thismemb.count = astmemb.count;
      thismemb.symtype = ST_VARIABLE;
    }

    // Is this the first member?
    if (strtype.memb == NULL) {
      thismemb.offset = 0;
      strtype.memb = thismemb;
      if (O_logmisc)
	fprintf(Debugfh, "%s member %s: offset %d size %d\n",
		get_typename(thismemb.ty),
		thismemb.name, thismemb.offset, size);

      // Update the offset if not a union
      if (isunion == false)
        offset = offset + size;
    } else {

      // Not the first member. Get the aligned offset for it
      // Then append it. If a union, we already have the right offset
      if (isunion == false)
        offset = genalign(astmemb.ty, offset);
      thismemb.offset = offset;
      lastmemb.next = thismemb;

      // Update the offset if not a union
      if (isunion == false)
        offset = offset + size;

      if (O_logmisc)
        fprintf(Debugfh, "%s member %s: offset %d size %d\n",
	      get_typename(thismemb.ty),
	      thismemb.name, thismemb.offset, size);
    }
  }

  // Now return the possible offset of the next member
  if (isunion) {
    if (O_logmisc)
      fprintf(Debugfh, "addmemb isunion returning offset %d\n\n",
				offset + biggest_memb);
    return (offset + biggest_memb);
  } else {
    if (O_logmisc)
      fprintf(Debugfh, "addmemb not union returning offset %d\n\n", offset);
    return (offset);
  }
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
void struct_declaration(char *name) {
  Type *thistype;
  ASTnode *astmemb;
  int offset = 0;

  // Skip the STRUCT keyword and get the left brace
  scan(&Thistoken);
  lbrace();

  // Build a new STRUCT type
  thistype = new_type(TY_STRUCT, 0, 0, name, NULL);
  if (O_logmisc)
    fprintf(Debugfh, "type struct %s:\n", name);

  // Loop getting members for the struct
  while (true) {
    // Is the next token a UNION?
    if (Thistoken.token == T_UNION) {
      // Get the union declaration
      // and add the members to the struct
      astmemb = union_declaration();
      offset = add_memb_to_struct(thistype, astmemb, offset, true);
    } else {
      // Get a single array typed declaration
      // and add the member to the struct
      astmemb = array_typed_declaration();
      offset = add_memb_to_struct(thistype, astmemb, offset, false);
    }

    // If no comma, stop now
    if (Thistoken.token != T_COMMA)
      break;

    // Skip the comma
    scan(&Thistoken);
  }

  // Set the struct size in bytes
  thistype.size = offset;
  if (O_logmisc)
    fprintf(Debugfh, "struct total size is %d\n", offset);

  // Get the trailing right brace
  rbrace();
}

// Parse a union declaration

//- union_declaration= UNION LBRACE typed_declaration_list RBRACE
//-
ASTnode *union_declaration(void) {
  ASTnode *astmemb;

  // Skip the UNION keyword and get the left brace
  scan(&Thistoken);
  lbrace();

  astmemb = typed_declaration_list();

  // Get the trailing right brace
  rbrace();
  return (astmemb);
}

// Global variables and functions have a default private
// visibility which can be changed by these keywords.
//
//- visibility= ( PUBLIC | EXTERN )?
//-
int get_visibility(void) {
  int visibility = SV_PRIVATE;
  switch (Thistoken.token) {
  case T_PUBLIC:
    visibility = SV_PUBLIC;
    scan(&Thistoken);
  case T_EXTERN:
    visibility = SV_EXTERN;
    scan(&Thistoken);
  }
  return (visibility);
}

// Given a symbol and a bracketed expression list,
// check that the list is suitable for the symbol.
// Also output the values to the assembly file.
// offset holds the byte offset of the value from the
// base of any struct/array. If is_element is true,
// we are processing elements in the sym array.
void check_bel(Sym * sym, ASTnode * list, int offset, bool is_element) {
  ASTnode *wide;
  Sym *memb;
  int i;
  Type *ty;

  // No list, we ran out of values in the list
  if (list == NULL)
    fatal("not enough values in the expression list\n");

  // No symbol, too many values
  if (sym == NULL)
    fatal("too many values in the expression list\n");

  // Get the type of the symbol or its elements
  ty= sym.ty;
  if (is_element==true)
    ty= value_at(sym.ty);

  // We need a BEL for lists and arrays.
  // Remove the BEL.
  if ((is_element==false && is_array(sym)) || is_struct(ty)) {
    if (list.op != A_BEL)
      fatal("%s needs an initialisation list\n", sym.name);

    list= list.left;
  }

  // The symbol is an array. Get the elements' type.
  // Use the count of elements and walk the list
  if (is_element==false && is_array(sym)) {
    ty= value_at(sym.ty);
    for (i = 0; i < sym.count; i++) {
      check_bel(sym, list, offset + i * ty.size, true);
      list = list.mid;
    }

    return;
  }

  // This is a struct.
  // Walk the list of struct members and
  // check each against the list value
  if (is_struct(ty)) {
    for (memb = ty.memb; memb != NULL && list != NULL;
	 memb = memb.next) {
      check_bel(memb, list, offset + memb.offset, false);
      list = list.mid;
    }

    return;
  }

  // The symbol is a scalar
  if (list.op == A_BEL)
    fatal("%s is scalar, cannot use an initialisation list\n", sym.name);

  // Make sure the expression matches the symbol's type.
  wide = widen_type(list, ty, 0);
  if (wide == NULL)
    fatal("initialisation value not of type %s\n", get_typename(ty));

  // It also has to be a literal value
  if ((list.op != A_NUMLIT) && (list.op != A_STRLIT))
    fatal("initialisation value not a literal value\n");

  // Update the list element's type
  list.ty = ty;

  // Output the value at the offset
  if (O_logmisc)
    fprintf(Debugfh, "globsymval offset %d\n", offset);
  cgglobsymval(list, offset);
  return;
}

// Parse a global variable declaration.
// We receive the typed_declaration in decl
//
//- global_var_declaration= visibility array_typed_declaration decl_initialisation SEMI
//-                       | visibility array_typed_declaration SEMI
//-
void global_var_declaration(ASTnode * decl, int visibility) {
  ASTnode *init = NULL;
  Sym *sym;

  // See if the variable's name already exists
  if (find_symbol(decl.strlit) != NULL)
    fatal("symbol %s already exists\n", decl.strlit);

  // Add the symbol and type as a global
  sym = add_symbol(decl.strlit, ST_VARIABLE, decl.ty, visibility);

  // If the declaration was marked as an array,
  // update the symbol
  if (decl.is_array == true) {
    sym.count = decl.count;
  }

  // If we have an '=', we have an initialisation
  if (Thistoken.token == T_ASSIGN) {
    init = decl_initialisation();
    if (O_logmisc) {
      fprintf(Debugfh, "%s initialisation:\n", decl.strlit);
      dumpAST(init, 0);
    }

    // Yes, an initialisation list
    if (sym.visibility == SV_EXTERN)
      fatal("cannot intiialise an external variable\n");

    // Start the output of the variable.
    // Don't zero it
    cgglobsym(sym, false);

    // Check the initialisation (list) against the symbol.
    // Also output the values in the list
    check_bel(sym, init, 0, false);

    // End the output of the variable
    cgglobsymend(sym);
  } else {

    // No initialisation list.
    // Output zeroes instead
    if (sym.visibility != SV_EXTERN) {
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
ASTnode *decl_initialisation(void) {

  // Skip the '='
  scan(&Thistoken);

  // Get either an expression or a bracketed_expression_list
  if (Thistoken.token == T_LBRACE)
    return (bracketed_expression_list());
  else
    return (expression());
}

// Parse a single function declaration
//
//- function_declaration= visibility function_prototype statement_block
//-                     | visibility function_prototype SEMI
//-
void function_declaration(ASTnode * func, int visibility) {
  ASTnode *s;

  // Get the function's prototype.
  func = function_prototype(func);

  // If the next token is a semicolon
  if (Thistoken.token == T_SEMI) {
    // Add the function prototype to the symbol table
    add_function(func, func.left, visibility);

    // Skip the semicolon and return
    scan(&Thistoken);
    return;
  }

  // It's not a prototype, so we expect a statement block now.
  // Set Thisfunction to the function's symbol structure
  if (visibility == SV_EXTERN)
    fatal("cannot declare an extern function with a body\n");
  declare_function(func, visibility);
  Thisfunction = find_symbol(func.strlit);
  s = statement_block(Thisfunction);
  gen_func_statement_block(s);
}

// Parse a function prototype and
// return an ASTnode with the details
//
//- function_prototype= typed_declaration LPAREN
//-                     ( typed_declaration_list (COMMA ELLIPSIS)?
//-                     | VOID
//-                     ) RPAREN (THROWS typed_declaration )?
//-
ASTnode *function_prototype(ASTnode * func) {
  ASTnode *paramlist = NULL;
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
      func.left= NULL;
      return(func);
    }
  }

  // Get the list of parameters
  paramlist = typed_declaration_list();

  // If the next token is an ELLIPSIS,
  // mark the function as variadic
  if (Thistoken.token == T_ELLIPSIS) {
    scan(&Thistoken);
    func.is_variadic = true;
  }

  // Get the ')' and add the params to the function
  rparen();
  func.left = paramlist;

  // If we have a THROWS
  if (Thistoken.token == T_THROWS) {
    scan(&Thistoken);

    // Get the name and base type of the exception variable
    astexcept = typed_declaration();
    basetype = value_at(astexcept.ty);

    // The type must be a pointer to a struct which
    // has an int32 as the first member
    if ((astexcept.ty.kind != TY_STRUCT) ||
	(astexcept.ty.ptr_depth != 1) ||
	(basetype.memb == NULL) || (basetype.memb.ty != ty_int32))
      fatal("variable %s not suitable to hold an exception\n",
	    astexcept.strlit);

    // Build a Sym node with the variable's name
    // and type, and add it to the ASTnode
    add_sym_to(&(func.sym), astexcept.strlit, ST_VARIABLE, astexcept.ty);
    func.sym.visibility = SV_LOCAL;
  }

  return (func);
}

// Get a linked list of typed declarations
// as a set of ASTnodes linked by the middle child
//
//- typed_declaration_list= typed_declaration (COMMA typed_declaration_list)*
//-
ASTnode *typed_declaration_list(void) {
  ASTnode *first;
  ASTnode *this;
  ASTnode *next;

  // Get the first typed_declaration
  this = typed_declaration();
  first = this;

  while (true) {
    // If no comma, stop now
    if (Thistoken.token != T_COMMA)
      break;

    // Skip the comma
    scan(&Thistoken);

    // Stop if we hit an ELLIPSIS
    if (Thistoken.token == T_ELLIPSIS)
      break;

    // Get the next declaration and link it in
    next = typed_declaration();
    this.mid = next;
    this = next;
  }

  return (first);
}

// Get a typed declaration with an optional trailing array size
// and return it as an ASTnode with the type updated and the
// count set as the array's size.
//
//- array_typed_declaration= typed_declaration (array_size)?
//- 
ASTnode *array_typed_declaration(void) {
  ASTnode *this;
  int64 size;

  // Get the typed declaration
  this = typed_declaration();

  // If next token is an '['
  if (Thistoken.token == T_LBRACKET) {
    // Get the array's size and change the type.
    // Mark it as an array
    size = array_size();
    this.count = size;
    this.is_array = true;
    this.ty = pointer_to(this.ty);
  }

  return (this);
}

// Get the size of an array
//
//- array_size= LBRACKET NUMLIT RBRACKET
//-
int64 array_size(void) {
  int64 size;

  // Skip the left bracket
  scan(&Thistoken);

  // We must see a NUMLIT
  if (Thistoken.token != T_NUMLIT)
    fatal("array size missing\n");

  // We need an integer NUMLIT which isn't negative
  if ((Thistoken.litval.numtype == NUM_FLT) || (Thistoken.litval.intval < 0))
    fatal("array size must be a positive integer literal\n");

  // Get the size, skip the NUMLIT
  size = Thistoken.litval.intval;
  scan(&Thistoken);

  // Skip the ']'
  match(T_RBRACKET, true);

  return (size);
}

// Get a symbol declaration along with its type as an ASTnode.
//
//- typed_declaration= type IDENT
//-
ASTnode *typed_declaration(void) {
  ASTnode *identifier;
  Type *t;

  // Get the type and skip past it.
  t = match_type(false);

  // Get the identifier, set its type
  match(T_IDENT, true);
  identifier = mkastleaf(A_IDENT, NULL, false, NULL, 0);
  identifier.strlit = strdup(Text);
  identifier.ty = t;

  return (identifier);
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
Type *match_type(bool checkonly) {
  Type *t = NULL;
  char *typename = NULL;

  // See if this token is a built-in type
  switch (Thistoken.token) {
  case T_VOID:
    t = ty_void;
  case T_BOOL:
    t = ty_bool;
  case T_INT8:
    t = ty_int8;
  case T_INT16:
    t = ty_int16;
  case T_INT32:
    t = ty_int32;
  case T_INT64:
    t = ty_int64;
  case T_UINT8:
    t = ty_uint8;
  case T_UINT16:
    t = ty_uint16;
  case T_UINT32:
    t = ty_uint32;
  case T_UINT64:
    t = ty_uint64;
  case T_FLT32:
    t = ty_flt32;
  case T_FLT64:
    t = ty_flt64;
  case T_IDENT:
    typename = strdup(Thistoken.tokstr);
    t = find_type(typename, TY_USER, 0);
  }

  // Stop now if we are only checking for a type's existence
  if (checkonly)
    return (t);

  // We don't recognise it as a type
  if (t == NULL)
    fatal("Unknown type %s\n", Text);

  // Get the next token
  scan(&Thistoken);

  // Loop counting the number of STAR tokens
  // and getting a a pointer to the previous type
  while (Thistoken.token == T_STAR) {
    scan(&Thistoken);
    t = pointer_to(t);
  }

  return (t);
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
ASTnode *statement_block(Sym * func) {
  ASTnode *s = NULL;
  ASTnode *d = NULL;

  // See if we have a single procedural statement
  s = procedural_stmt();
  if (s != NULL)
    return (s);

  // No, so parse a block surrounded by '{' ... '}'
  lbrace();

  // An empty statement body
  if (Thistoken.token == T_RBRACE)
    return (NULL);

  // Start a new scope
  new_scope(func);

  // A declaration_stmt starts with a type, so look for one.
  if (match_type(true) != NULL)
    d = declaration_stmts();

  // Now get any procedural statements
  s = procedural_stmts();
  if (d == NULL)
    d = s;
  else
    d.right = s;

  rbrace();
  // Dispose of this scope
  end_scope();
  return (d);
}

// Parse zero or more declaration statements and
// build an AST tree with them linked by the middle child
//- declaration_stmts= ( array_typed_declaration decl_initialisation
//-                    | array_typed_declaration SEMI
//-                    )*
//-
ASTnode *declaration_stmts(void) {
  ASTnode *d;
  ASTnode *e = NULL;
  ASTnode *this;

  // Get one declaration statement
  d = array_typed_declaration();

  // If there is an '=' next, we have an assignment
  if (Thistoken.token == T_ASSIGN) {
    e = decl_initialisation();
  }

  semi();

  // Declare that variable
  this = declaration_statement(d, e);

  // Look for a type. If so, we have another declaration statement
  if (match_type(true) != NULL) {
    this.mid = declaration_stmts();
  }

  return (this);
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
//-                   | va_start_stmt
//-                   | va_end_stmt
//-                   | function_call SEMI
//-                   )*
//-
ASTnode *procedural_stmts(void) {
  ASTnode *left = NULL;
  ASTnode *right;

  while (true) {
    // Try to get another statement
    right = procedural_stmt();
    if (right == NULL)
      break;

    // Glue left and right if we have both
    // or just set left for now
    if (left == NULL)
      left = right;
    else
      left = mkastnode(A_GLUE, left, NULL, right);
  }

  return (left);
}

// Parse a single procedural statement.
// Return NULL if there is none.
ASTnode *procedural_stmt(void) {
  ASTnode *left;

  // If we have a right brace, no statement
  if (Thistoken.token == T_RBRACE)
    return (NULL);

  // See if this token is a known keyword or identifier
  switch (Thistoken.token) {
  case T_IF:
    return (if_stmt());
  case T_WHILE:
    return (while_stmt());
  case T_FOR:
    return (for_stmt());
  case T_RETURN:
    return (return_stmt());
  case T_ABORT:
    return (abort_stmt());
  case T_BREAK:
    return (break_stmt());
  case T_CONTINUE:
    return (continue_stmt());
  case T_TRY:
    return (try_stmt());
  case T_SWITCH:
    return (switch_stmt());
  case T_FALLTHRU:
    return (fallthru_stmt());
  case T_STAR:
    return (assign_stmt());
  case T_VASTART:
  case T_VAEND:
    return (va_start_end_stmt());
  case T_IDENT:
    // Get the next token.
    scan(&Peektoken);

    // If it's a '(' then it's a function call.
    if (Peektoken.token == T_LPAREN) {
      // Get the AST for the function and
      // absorb the trailing semicolon
      left = function_call();
      semi();
      return (left);
    }
    // No '(' so it's an assignment statement
    return (assign_stmt());
  }

  return (NULL);
}

//- assign_stmt= short_assign_stmt SEMI
//-
ASTnode *assign_stmt(void) {
  ASTnode *a = short_assign_stmt();
  semi();
  return (a);
}

//- short_assign_stmt= postfix_variable ASSIGN expression
//-                  | postfix_variable POSTINC
//-                  | postfix_variable POSTDEC
//-
//-
ASTnode *short_assign_stmt(void) {
  ASTnode *v;
  ASTnode *e;
  ASTnode *diff;
  int i;
  int starcount = 0;

  // Count the number of leading '*'s
  while (Thistoken.token == T_STAR) {
    starcount++;
    scan(&Thistoken);
  }

  // Get the variable
  v = postfix_variable(NULL);

  // Do we have a '++' or '--' following?
  // If so, build an assignment statement
  // with either an ADD or a SUBTRACT
  if (Thistoken.token == T_POSTINC) {
    // Get the variable as an rvalue
    e = Calloc(sizeof(ASTnode));
    memcpy(e, v, sizeof(ASTnode));
    e.rvalue = true;
    scan(&Thistoken);

    // Build a NUMLIT node with 1 in it
    // and add it from the rval variable
    diff = mkastleaf(A_NUMLIT, ty_int8, true, NULL, 1);
    e = binop(e, diff, A_ADD);

    // Now do the assignment
    return (assignment_statement(v, e));
  }

  if (Thistoken.token == T_POSTDEC) {
    // Get the variable as an rvalue
    e = Calloc(sizeof(ASTnode));
    memcpy(e, v, sizeof(ASTnode));
    e.rvalue = true;
    scan(&Thistoken);

    // Build a NUMLIT node with 1 in it,
    // and subtract it from the rval variable
    diff = mkastleaf(A_NUMLIT, ty_int8, true, NULL, 1);
    e = binop(e, diff, A_SUBTRACT);

    // Now do the assignment
    return (assignment_statement(v, e));
  }

  // Check for an '=' then get the expression
  match(T_ASSIGN, true);
  e = expression();

  // Check that the variable's pointer depth is the same or
  // greater than our star count
  if (v.ty.ptr_depth < starcount)
    fatal("Too many dereferences for type %s\n", get_typename(v.ty));

  // Add starcount DEREF nodes to the variable
  for (i = 0; i < starcount; i++) {
    v = mkastnode(A_DEREF, v, NULL, NULL);
    v.ty = value_at(v.left.ty);
  }

  return (assignment_statement(v, e));
}

//- if_stmt= IF LPAREN boolean_expression RPAREN statement_block
//-          (ELSE statement_block)?
//-
ASTnode *if_stmt(void) {
  ASTnode *e;
  ASTnode *t;
  ASTnode *f = NULL;

  // Skip the IF, check for a left parenthesis.
  // Get the expression, right parenthesis
  // and the statement block
  scan(&Thistoken);
  lparen();
  e = boolean_expression();
  rparen();
  t = statement_block(NULL);

  // If we now have an ELSE
  // get the following statement block
  if (Thistoken.token == T_ELSE) {
    scan(&Thistoken);
    f = statement_block(NULL);
  }

  return (mkastnode(A_IF, e, t, f));
}

//- while_stmt= WHILE LPAREN boolean_expression RPAREN statement_block
//-           | WHILE LPAREN TRUE RPAREN statement_block
//-
ASTnode *while_stmt(void) {
  ASTnode *e;
  ASTnode *s;

  // Skip the WHILE, check for a left parenthesis.
  scan(&Thistoken);
  lparen();

  // If we have a TRUE token, build an ASTnode for it
  if (Thistoken.token == T_TRUE) {
    e = mkastleaf(A_NUMLIT, ty_bool, true, NULL, 1);
    scan(&Thistoken);
  } else {
    // Otherwise, get the expression
    e = boolean_expression();
  }

  // Get the trailing right parenthesis
  // and the statement block
  rparen();
  s = statement_block(NULL);

  return (mkastnode(A_WHILE, e, s, NULL));
}

//- for_stmt= FOR LPAREN short_assign_stmt? SEMI
//-                      boolean_expression? SEMI
//-                      short_assign_stmt? RPAREN statement_block
//-
ASTnode *for_stmt(void) {
  ASTnode *i = NULL;
  ASTnode *e;
  ASTnode *send = NULL;
  ASTnode *s;

  // Skip the FOR, check for a left parenthesis.
  scan(&Thistoken);
  lparen();

  // If we don't have a semicolon, get the initial statement.
  // Then get the semicolon
  if (Thistoken.token != T_SEMI)
    i = short_assign_stmt();
  semi();

  // If we don't have a semicolon, get the condition expression.
  // Otherwise, make a TRUE node instead
  if (Thistoken.token != T_SEMI) {
    e = boolean_expression();
  } else {
    e = mkastleaf(A_NUMLIT, ty_bool, true, NULL, 1);
  }
  semi();

  // If we don't have a right parentheses, get the change statement
  if (Thistoken.token != T_RPAREN)
    send = short_assign_stmt();

  // Get the right parenthesis and the statement block for the loop
  rparen();
  s = statement_block(NULL);

  // Glue the end code after the statement block.
  // Set the rvalue true to indicate that the
  // right child is the end code of a FOR loop.
  // We need this to make 'continue' work in a FOR loop.
  s = mkastnode(A_GLUE, s, NULL, send);
  s.is_short_assign = true;

  // We put the initial code at the end so that
  // we can send the node to gen_WHILE() :-)
  return (mkastnode(A_FOR, e, s, i));
}

//- return_stmt= RETURN LPAREN expression RPAREN SEMI
//-            | RETURN SEMI
//-
ASTnode *return_stmt(void) {
  ASTnode *this;
  ASTnode *e = NULL;

  // Skip the 'return' token
  scan(&Thistoken);

  // If we have a left parenthesis, we are returning a value
  if (Thistoken.token == T_LPAREN) {
    // Can't return a value if the function returns void
    if (Thisfunction.ty == ty_void)
      fatal("Can't return from void %s()\n", Thisfunction.name);

    // Skip the left parenthesis
    lparen();

    // Parse the following expression
    e = expression();

    // Widen the expression's type if required
    e = widen_expression(e, Thisfunction.ty);

    // Get the ')'
    rparen();
  }

  // Error if no expression but the function returns a value
  if (e == NULL && Thisfunction.ty != ty_void)
    fatal("No return value from non-void %s()\n", Thisfunction.name);

  // Build the A_RETURN node
  this = mkastnode(A_RETURN, e, NULL, NULL);

  // Get the ';'
  semi();
  return (this);
}

//- abort_stmt= ABORT SEMI
//-
ASTnode *abort_stmt(void) {
  ASTnode *this;

  // Skip the 'abort' token
  scan(&Thistoken);

  // Build the A_ABORT node
  this = mkastnode(A_ABORT, NULL, NULL, NULL);

  // Get the ';'
  semi();
  return (this);
}

//- break_stmt= BREAK SEMI
//-
ASTnode *break_stmt(void) {
  ASTnode *this;

  // Skip the 'break' token
  scan(&Thistoken);

  // Build the A_BREAK node
  this = mkastnode(A_BREAK, NULL, NULL, NULL);

  // Get the ';'
  semi();
  return (this);
}

//- continue_stmt= CONTINUE SEMI
//-
ASTnode *continue_stmt(void) {
  ASTnode *this;

  // Skip the 'continue' token
  scan(&Thistoken);

  // Build the A_CONTINUE node
  this = mkastnode(A_CONTINUE, NULL, NULL, NULL);

  // Get the ';'
  semi();
  return (this);
}


//- try_statement= TRY LPAREN IDENT RPAREN statement_block CATCH statement_block
//-
ASTnode *try_stmt(void) {
  Sym *sym;
  ASTnode *n;

  // Skip the 'try' and get the left parenthesis
  scan(&Thistoken);
  lparen();

  // Ensure we have an identifier and get its symbol
  match(T_IDENT, false);
  sym = find_symbol(Thistoken.tokstr);
  if (sym == NULL)
    fatal("unknown symbol %s\n", Thistoken.tokstr);

  // Check that the symbol's type is a struct with
  // an int32 as the first member
  if (!is_struct(sym.ty) ||
      (sym.ty.memb == NULL) || (sym.ty.memb.ty != ty_int32))
    fatal("variable %s not suitable to hold an exception\n",
	  Thistoken.tokstr);

  // Make an A_TRY leaf node with the given symbol
  n = mkastleaf(A_TRY, NULL, false, sym, 0);
  n.strlit = Thistoken.tokstr;
  n = mkident(n);

  // Skip the identifier and right parenthesis
  scan(&Thistoken);
  rparen();

  // Get the try statement block
  n.left = statement_block(NULL);

  // Get the 'catch'
  match(T_CATCH, true);

  // Get the catch statement block
  n.right = statement_block(NULL);
  return (n);
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
ASTnode *switch_stmt(void) {
  ASTnode *left;
  ASTnode *body;
  ASTnode *n;
  ASTnode *c;
  ASTnode *casetree = NULL;
  ASTnode *casetail = NULL;
  bool inloop = true;
  bool seendefault = false;
  int ASTop;
  int casecount = 0;
  int64 caseval = 0;

  // Skip the 'switch' and '('
  scan(&Thistoken);
  lparen();

  // Get the switch expression, the ')' and the '{'
  left = expression();
  rparen();
  lbrace();

  // Ensure that this is an int literal
  if (!is_integer(left.ty))
    fatal("switch expression not of integer type\n");

  // Build the AST node with the expression
  n = mkastnode(A_SWITCH, left, NULL, NULL);

  // Now parse the cases
  while (inloop) {
    switch (Thistoken.token) {
      // Leave the loop when we hit a '}'
    case T_RBRACE:
      if (casecount == 0)
	fatal("No cases in switch\n");
      inloop = false;
    case T_CASE:
    case T_DEFAULT:
      // Ensure this isn't after a previous 'default'
      if (seendefault)
	fatal("case or default after existing default\n");

      if (Thistoken.token == T_DEFAULT) {
	ASTop = A_DEFAULT;
	scan(&Thistoken);
	seendefault = true;
      } else {
	// Scan the case value if required
	ASTop = A_CASE;
	scan(&Thistoken);

	// Get the case expression
	left = expression();

	// Ensure the case value is an integer literal
	if ((left.op != A_NUMLIT) || !is_integer(left.ty))
	  fatal("Expecting integer literal for case value\n");
	caseval = left.litval.intval;

	// Walk the list of existing case values to ensure
	// that there isn't a duplicate case value
	for (c = casetree; c != NULL; c = c.right)
	  if (caseval == c.litval.intval)
	    fatal("Duplicate case value\n");
      }

      // Scan the ':' and increment the casecount
      match(T_COLON, true);
      casecount++;

      // If the next token is a T_CASE, the existing case will fall
      // into the next case. Otherwise, parse the case body.
      if (Thistoken.token == T_CASE)
	body = NULL;
      else
	body = procedural_stmts();

      // Build a sub-tree with any statement block as the left child
      // and link it in to the growing A_CASE tree
      if (casetree == NULL) {
	casetree = mkastnode(ASTop, body, NULL, NULL);
	casetail = casetree;
      } else {
	casetail.right = mkastnode(ASTop, body, NULL, NULL);
	casetail = casetail.right;
      }

      // Copy the case value into the new node
      // Yes, we copy into the DEFAULT node, doesn't matter!
      casetail.litval.intval = caseval;
    default:
      fatal("Unexpected token in switch: %s\n",
	    get_tokenstr(Thistoken.token));
    }
  }

  // We have a AST tree with the cases and any default. Put the
  // case count into the A_SWITCH node and attach the case tree.
  n.litval.intval = casecount;
  n.right = casetree;
  rbrace();

  return (n);
}

//- fallthru_stmt= FALLTHRU SEMI
//-
ASTnode *fallthru_stmt(void) {

  // Skip the 'fallthru'
  scan(&Thistoken);
  semi();
  return (mkastnode(A_FALLTHRU, NULL, NULL, NULL));
}

//- function_call= IDENT LPAREN expression_list? RPAREN
//-              | IDENT LPAREN named_expression_list RPAREN
//-
ASTnode *function_call(void) {
  ASTnode *s;
  ASTnode *e = NULL;
  Sym *sym;

  // Make an IDENT node from the current token
  s = mkastleaf(A_IDENT, NULL, false, NULL, 0);
  s.strlit = Thistoken.tokstr;

  // Get the function's Sym pointer
  sym = find_symbol(s.strlit);
  if (sym == NULL || sym.symtype != ST_FUNCTION)
    fatal("Unknown function %s()\n", s.strlit);

  // Skip the identifier and get the left parenthesis
  scan(&Thistoken);
  lparen();

  // If the next token is not a right parenthesis,
  if (Thistoken.token != T_RPAREN) {
    // See if the lookahead token is an '='.
    // If so, we have a named expression list
    scan(&Peektoken);
    if (Peektoken.token == T_ASSIGN) {
      e = named_expression_list();
    } else {
      // No, so get an expression list
      e = expression_list();
    }
  }

  // Get the right parenthesis
  rparen();

  // Build the function call node and set its type
  s = mkastnode(A_FUNCCALL, s, NULL, e);
  s.ty = sym.ty;
  return (s);
}

//- va_start_stmt= VA_START LPAREN IDENT RPAREN SEMI
//-
//- va_end_stmt= VA_END LPAREN IDENT RPAREN SEMI
//-
ASTnode *va_start_end_stmt(void) {
  int token= Thistoken.token;
  int astop;
  ASTnode *v;
  Sym *sym;

  // Skip the keyword and '('
  scan(&Thistoken);
  lparen();

  // Ensure that we have an identifier
  match(T_IDENT, false);

  // Try to find the symbol
  sym = find_symbol(Thistoken.tokstr);
  if ((sym == NULL) || (sym.symtype != ST_VARIABLE))
    fatal("can only do va_start(variable) and va_end(variable)\n");

  // Make sure the variable has void * as it's type
  if (sym.ty != ty_voidptr)
    fatal("va_start(variable) and va_end(variable) must be void * type\n");

  // Skip the identifier ')' and ';'
  scan(&Thistoken);
  rparen();
  semi();
  astop= (token== T_VASTART) ? A_VASTART : A_VAEND;
  v= mkastnode(astop, NULL, NULL, NULL);
  v.sym= sym;
  return(v);
}

//- bracketed_expression_list= LBRACE bracketed_expression_element
//-                                   (COMMA bracketed_expression_element)*
//-                            RBRACE
//-
ASTnode *bracketed_expression_list(void) {
  ASTnode *bel;
  ASTnode *this;

  // Skip the left brace
  scan(&Thistoken);

  // Make the BEL node which will hold the list
  bel = mkastnode(A_BEL, NULL, NULL, NULL);

  // Start with one bracketed expression element
  this = bracketed_expression_element();
  bel.left = this;

  // Loop trying to get more of them
  while (true) {
    // No more, so stop
    if (Thistoken.token != T_COMMA)
      break;

    // Get the next element and append it
    scan(&Thistoken);
    this.mid = bracketed_expression_element();
    this = this.mid;
  }

  // Skip the right brace and return the list
  scan(&Thistoken);
  return (bel);
}

//- bracketed_expression_element= expression
//-                             | bracketed_expression_list
//-
ASTnode *bracketed_expression_element(void) {
  ASTnode *elem;

  // Parse one element and return it
  switch (Thistoken.token) {
  case T_LBRACE:
    elem = bracketed_expression_list();
  default:
    elem = expression();
  }

  return (elem);
}


//- expression_list= expression (COMMA expression_list)*
//-
ASTnode *expression_list(void) {
  ASTnode *e;
  ASTnode *l = NULL;

  // Get the expression
  e = expression();

  // If we have a comma, skip it.
  // Get the following expression list
  if (Thistoken.token == T_COMMA) {
    scan(&Thistoken);
    l = expression_list();
  }

  // Glue e and l and return them
  return (mkastnode(A_GLUE, e, NULL, l));
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
ASTnode *named_expression_list(void) {
  ASTnode *first;
  ASTnode *expr;
  ASTnode *this;
  ASTnode *next;

  // Build an ASSIGN node with the identifier's
  // name, then skip the identifier
  this = mkastleaf(A_ASSIGN, NULL, false, NULL, 0);
  first = this;
  first.strlit = Thistoken.tokstr;
  scan(&Thistoken);

  // Check for the '=' token
  match(T_ASSIGN, true);

  // Get the expression and attach it as the left child
  expr = expression();
  first.left = expr;

  while (true) {
    // If no comma, stop now
    if (Thistoken.token != T_COMMA)
      break;

    // Skip the comma
    // Get the next named expression and link it in
    scan(&Thistoken);
    next = named_expression_list();
    this.right = next;
    this = next;
  }

  return (first);
}

// Try to optimise the AST tree that holds an expression
//
//- expression= ternary_expression
//-
ASTnode *expression(void) {
  return (optAST(ternary_expression()));
}

//- ternary_expression= bitwise_expression
//-                   | LPAREN relational_expression RPAREN
//-                     QUESTION ternary_expression COLON ternary_expression
//-
ASTnode *ternary_expression(void) {
  ASTnode *n;
  ASTnode *e;
  ASTnode *t;
  ASTnode *f;

  // The problem here is that LPAREN relational_expression RPAREN by itself
  // is fine, e.g. bool fred= (2 > 1); So we need to get the expression,
  // see if it is followed by a '?' before deciding it's a ternary

  // Do we have an '('?
  if (Thistoken.token == T_LPAREN) {

    // Get the expression, absorbing '(' and ')'
    n = bitwise_expression();

    // If this is followed by a '?' and
    // the expression's type is boolean
    if ((Thistoken.token == T_QUESTION) && (n.ty == ty_bool)) {
      // Skip the '?'
      scan(&Thistoken);
      e= n;

      // Get the true expression
      t = ternary_expression();

      // Skip the colon
      match(T_COLON, true);

      // Get the false expression
      f = ternary_expression();
      // Build the ASTnode
      n = mkastnode(A_TERNARY, e, t, f);
      n.ty = t.ty;
    }
  } else {
    // No leading '(', must be a bitwise expression
    n = bitwise_expression();
  }

  n.rvalue = true;
  return (n);
}

//- bitwise_expression= ( INVERT boolean_expression
//-                     |        boolean_expression
//-                     )
//-                     ( AND boolean_expression
//-                     | OR  boolean_expression
//-                     | XOR boolean_expression
//-                     )*
//-
ASTnode *bitwise_expression(void) {
  ASTnode *left;
  ASTnode *right;
  bool invert = false;
  bool loop = true;

  // Deal with a leading '~'
  if (Thistoken.token == T_INVERT) {
    scan(&Thistoken);
    invert = true;
  }

  // Get the expression and invert if required.
  // We can't do bitwise operations on a boolean
  left = boolean_expression();
  if (invert) {
    cant_do(left, ty_bool, "Cannot do bitwise operations on a boolean\n");
    left = unarop(left, A_INVERT);
  }

  // See if we have more bitwise operations
  while (loop) {
    switch (Thistoken.token) {
    case T_AMPER:
      scan(&Thistoken);
      right = boolean_expression();
      cant_do(left, ty_bool, "Cannot do bitwise operations on a boolean\n");
      cant_do(right, ty_bool, "Cannot do bitwise operations on a boolean\n");
      left = binop(left, right, A_AND);
    case T_OR:
      scan(&Thistoken);
      right = boolean_expression();
      cant_do(left, ty_bool, "Cannot do bitwise operations on a boolean\n");
      cant_do(right, ty_bool, "Cannot do bitwise operations on a boolean\n");
      left = binop(left, right, A_OR);
    case T_XOR:
      scan(&Thistoken);
      right = boolean_expression();
      cant_do(left, ty_bool, "Cannot do bitwise operations on a boolean\n");
      cant_do(right, ty_bool, "Cannot do bitwise operations on a boolean\n");
      left = binop(left, right, A_XOR);
    default:
      loop = false;
    }
  }

  // Nope, return what we have
  return (left);
}

//- boolean_expression= logical_and_expression
//-
ASTnode *boolean_expression(void) {
  return (logical_and_expression());
}


//- logical_and_expression= logical_or_expression
//-                       | logical_or_expression LOGAND logical_or_expression
//-
ASTnode *logical_and_expression(void) {
  ASTnode *left;
  ASTnode *right;

  // Get the logical OR expression
  left = logical_or_expression();

  // See if we have more logical AND operations
  while (true) {
    if (Thistoken.token != T_LOGAND)
      break;
    scan(&Thistoken);
    right = relational_expression();
    if ((left.ty != ty_bool) || (right.ty != ty_bool))
      fatal("Can only do logical AND on boolean types\n");
    left = binop(left, right, A_LOGAND);
    left.ty = ty_bool;
  }

  return (left);
}

//- logical_or_expression= relational_expression
//-                      | relational_expression LOGOR relational_expression
//-
ASTnode *logical_or_expression(void) {
  ASTnode *left;
  ASTnode *right;

  // Get the relational expression
  left = relational_expression();

  // See if we have more logical OR operations
  while (true) {
    if (Thistoken.token != T_LOGOR)
      break;
    scan(&Thistoken);
    right = relational_expression();
    if ((left.ty != ty_bool) || (right.ty != ty_bool))
      fatal("Can only do logical OR on boolean types\n");
    left = binop(left, right, A_LOGOR);
    left.ty = ty_bool;
  }

  return (left);
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
ASTnode *relational_expression(void) {
  ASTnode *left;
  ASTnode *right;
  bool not = false;

  // Deal with a leading '!'
  if (Thistoken.token == T_LOGNOT) {
    scan(&Thistoken);
    not = true;
  }

  // Get the shift expression and
  // logically not if required
  left = shift_expression();
  if (not) {
    if (left.ty != ty_bool)
      fatal("Can only do logical NOT on boolean types\n");
    left = unarop(left, A_NOT);
  }

  // See if we have a shift operation
  switch (Thistoken.token) {
  case T_GE:
    scan(&Thistoken);
    right = shift_expression();
    left = binop(left, right, A_GE);
  case T_GT:
    scan(&Thistoken);
    right = shift_expression();
    left = binop(left, right, A_GT);
  case T_LE:
    scan(&Thistoken);
    right = shift_expression();
    left = binop(left, right, A_LE);
  case T_LT:
    scan(&Thistoken);
    right = shift_expression();
    left = binop(left, right, A_LT);
  case T_EQ:
    scan(&Thistoken);
    right = shift_expression();
    left = binop(left, right, A_EQ);
  case T_NE:
    scan(&Thistoken);
    right = shift_expression();
    left = binop(left, right, A_NE);
  }

  // Nope, return what we have
  return (left);
}

//- shift_expression= additive_expression
//-                 ( LSHIFT additive_expression
//-                 | RSHIFT additive_expression
//-                 )*
//-
ASTnode *shift_expression(void) {
  ASTnode *left;
  ASTnode *right;
  bool loop = true;

  left = additive_expression();

  // See if we have more shft operations
  while (loop) {
    switch (Thistoken.token) {
    case T_LSHIFT:
      scan(&Thistoken);
      right = additive_expression();
      cant_do(left, ty_bool, "Cannot do shift operations on a boolean\n");
      cant_do(right, ty_bool, "Cannot do shift operations on a boolean\n");
      left = binop(left, right, A_LSHIFT);
    case T_RSHIFT:
      scan(&Thistoken);
      right = additive_expression();
      cant_do(left, ty_bool, "Cannot do shift operations on a boolean\n");
      cant_do(right, ty_bool, "Cannot do shift operations on a boolean\n");
      left = binop(left, right, A_RSHIFT);
    default:
      loop = false;
    }
  }

  // Nope, return what we have
  return (left);
}

//- additive_expression= ( PLUS? multiplicative_expression
//-                      | MINUS multiplicative_expression
//-                      )
//-                      ( PLUS  multiplicative_expression
//-                      | MINUS multiplicative_expression
//-                      )*
//-
ASTnode *additive_expression(void) {
  ASTnode *left;
  ASTnode *right;
  bool negate = false;
  bool loop = true;

  // Deal with a leading '+' or '-'
  switch (Thistoken.token) {
  case T_PLUS:
    scan(&Thistoken);
  case T_MINUS:
    scan(&Thistoken);
    negate = true;
  }

  // Get the multiplicative_expression
  // and negate it if required
  left = multiplicative_expression();
  if (negate) {
    cant_do(left, ty_bool, "Cannot do additive operations on a boolean\n");
    left = unarop(left, A_NEGATE);
  }

  // See if we have more additive operations
  while (loop) {
    switch (Thistoken.token) {
    case T_PLUS:
      scan(&Thistoken);
      right = multiplicative_expression();
      cant_do(left, ty_bool, "Cannot do additive operations on a boolean\n");
      cant_do(right, ty_bool, "Cannot do additive operations on a boolean\n");
      left = binop(left, right, A_ADD);
    case T_MINUS:
      scan(&Thistoken);
      right = multiplicative_expression();
      cant_do(left, ty_bool, "Cannot do additive operations on a boolean\n");
      cant_do(right, ty_bool, "Cannot do additive operations on a boolean\n");
      left = binop(left, right, A_SUBTRACT);
    default:
      loop = false;
    }
  }

  // Nope, return what we have
  return (left);
}

//- multiplicative_expression= unary_expression
//-                          ( STAR  unary_expression
//-                          | SLASH unary_expression
//-                          | MOD unary_expression
//-                          )*
//-
ASTnode *multiplicative_expression(void) {
  ASTnode *left;
  ASTnode *right;
  bool loop = true;

  // Get the first unary_expression
  left = unary_expression();

  // See if we have more multiplicative operations
  while (loop) {
    switch (Thistoken.token) {
    case T_STAR:
      scan(&Thistoken);
      right = unary_expression();
      cant_do(left, ty_bool,
	      "Cannot do multiplicative operations on a boolean\n");
      cant_do(right, ty_bool,
	      "Cannot do multiplicative operations on a boolean\n");
      left = binop(left, right, A_MULTIPLY);
    case T_SLASH:
      scan(&Thistoken);
      right = unary_expression();
      cant_do(left, ty_bool,
	      "Cannot do multiplicative operations on a boolean\n");
      cant_do(right, ty_bool,
	      "Cannot do multiplicative operations on a boolean\n");
      left = binop(left, right, A_DIVIDE);
    case T_MOD:
      scan(&Thistoken);
      right = unary_expression();
      cant_do(left, ty_bool,
	      "Cannot do multiplicative operations on a boolean\n");
      cant_do(right, ty_bool,
	      "Cannot do multiplicative operations on a boolean\n");
      left = binop(left, right, A_MOD);
    default:
      loop = false;
    }
  }

  // Nope, return what we have
  return (left);
}

// Parse a unary expression and return
// a sub-tree representing it.
//- unary_expression= primary_expression
//-                 | STAR unary_expression
//-                 | AMPER primary_expression
//-
ASTnode *unary_expression(void) {
  ASTnode *u;

  switch (Thistoken.token) {
  case T_AMPER:
    // Get the next token and parse it
    scan(&Thistoken);
    u = primary_expression();

    // Get an address based on the AST operation
    switch(u.op) {
      case A_DEREF:
        // It's a DEREF. We can just remove it to get the address.
	u= u.left;
      case A_IDENT:
	// It's an identifier. Change the operator to
	// A_ADDR and the type to a pointer to the original type.
	// Mark the identifier as needing a real memory address
	u.op = A_ADDR;
	u.ty = pointer_to(u.ty);
	u.sym.has_addr = true;
      case A_ADDOFFSET:
	// It already is an address. Change the type.
	u.ty = pointer_to(u.ty);
      default:
        fatal("& operator must be followed by an identifier\n");
    }

    return (u);

    return (u);
  case T_STAR:
    // Get the next token and parse it
    // recursively as a unary expression.
    // Make it an rvalue
    scan(&Thistoken);
    u = unary_expression();
    u.rvalue = true;

    // Ensure the tree's type is a pointer
    if (!is_pointer(u.ty))
      fatal("* operator must be followed by an expression of pointer type\n");

    // Prepend an A_DEREF operation to the tree
    // and update the tree's type. Mark the child
    // as being an rvalue
    u.rvalue = true;
    u = mkastnode(A_DEREF, u, NULL, NULL);
    u.ty = value_at(u.left.ty);
    u.rvalue = true;
    return (u);
  }

  // default:
  return (primary_expression());
}

//- primary_expression= NUMLIT
//-                   | STRLIT
//-                   | TRUE
//-                   | FALSE
//-                   | NULL
//-                   | ENUMVAL
//-                   | sizeof_expression
//-                   | va_arg_expression
//-                   | unsigned_expression
//-                   | postfix_variable
//-                   | function_call
//-                   | LPAREN expression RPAREN
//-
ASTnode *primary_expression(void) {
  ASTnode *f;
  Sym *sym;
  Type *ty;

  switch (Thistoken.token) {
  case T_LPAREN:
    // Skip the left parentheses, get the expression,
    // skip the right parentheses and return
    scan(&Thistoken);
    f = expression();
    rparen();
    return (f);
  case T_NUMLIT:
    // Build an ASTnode with the numeric value and suitable type
    ty = parse_litval(&Thistoken.litval);
    f = mkastleaf(A_NUMLIT, ty, true, NULL, Thistoken.litval.intval);
    scan(&Thistoken);
  case T_STRLIT:
    // Build an ASTnode with the string literal and ty_int8ptr type
    f = mkastleaf(A_STRLIT, ty_int8ptr, false, NULL, 0);
    f.strlit = Thistoken.tokstr;
    scan(&Thistoken);
  case T_TRUE:
    f = mkastleaf(A_NUMLIT, ty_bool, true, NULL, 1);
    scan(&Thistoken);
  case T_FALSE:
    f = mkastleaf(A_NUMLIT, ty_bool, true, NULL, 0);
    scan(&Thistoken);
  case T_NULL:
    f = mkastleaf(A_NUMLIT, ty_voidptr, true, NULL, 0);
    scan(&Thistoken);
  case T_SIZEOF:
    f = sizeof_expression();
  case T_VAARG:
    f = va_arg_expression();
  case T_UNSIGNED:
    f = unsigned_expression();
  case T_IDENT:
    // Find out what sort of symbol this is
    sym = find_symbol(Thistoken.tokstr);
    if (sym == NULL)
      fatal("unknown symbol %s\n", Thistoken.tokstr);
    switch (sym.symtype) {
    case ST_FUNCTION:
      f = function_call();
    case ST_VARIABLE:
      f = postfix_variable(NULL);
    case ST_ENUM:
      f = mkastleaf(A_NUMLIT, ty_int64, true, NULL, sym.count);
      scan(&Thistoken);
    default:
      fatal("unknown symbol type for %s\n", Thistoken.tokstr);
    }
  default:
    fatal("Unknown token as a primary_expression: %s\n",
	  get_tokenstr(Thistoken.token));
  }

  return (f);
}

//- sizeof_expression= SIZEOF LPAREN type RPAREN
//-                  | SIZEOF LPAREN IDENT RPAREN
//-
ASTnode *sizeof_expression(void) {
  ASTnode *e;
  Type *ty;
  Sym *sym;

  // Skip the keyword, get the '('
  scan(&Thistoken);
  lparen();

  if (Thistoken.token == T_IDENT) {
    // It's an identifier
    // Try to find the variable
    sym = find_symbol(Thistoken.tokstr);
    if (sym != NULL) {
      if (sym.symtype != ST_VARIABLE)
	fatal("can only do sizeof(variable)\n");

      // If this is an array, return the number of elements
      if (is_array(sym)) {
	e = mkastleaf(A_NUMLIT, ty_uint64, true, NULL, sym.count);
	scan(&Thistoken);
	rparen();
	return (e);
      }

      // Otherwise set up ty to be the symbol's type
      ty = sym.ty;
      scan(&Thistoken);
    }
  }

  // If ty is NULL, it wasn't a variable
  // so it must be a type.
  if (ty == NULL) {
    // Get the type in the parentheses
    ty = match_type(false);
  }

  // Can't get the size of an opaque type
  if (ty.size == 0)
    fatal("Can't get the size of opaque type %s\n", ty.name);

  // Make a NUMLIT node with the size of the type
  e = mkastleaf(A_NUMLIT, ty_uint64, true, NULL, ty.size);

  // Get the ')'
  rparen();
  return (e);
}

//- va_arg_expression= VA_ARG LPAREN IDENT COMMA type RPAREN
//-
ASTnode *va_arg_expression(void) {
  ASTnode *e;
  Sym *sym;
  Type *ty;

  // Skip the keyword, get the '('
  scan(&Thistoken);
  lparen();

  // Ensure that we have an identifier
  match(T_IDENT, false);

  // Try to find the symbol
  sym = find_symbol(Thistoken.tokstr);
  if ((sym == NULL) || (sym.symtype != ST_VARIABLE))
    fatal("need va_arg(variable, type)\n");

  // Make sure the variable has void * as it's type
  if (sym.ty != ty_voidptr)
    fatal("va_arg(variable,...) variable must be void * type\n");

  // Skip the identifier ')' and ','
  scan(&Thistoken);
  match(T_COMMA, true);

  // Get the type in the parentheses
  ty = match_type(false);

  // Check the type, must be scalar, a pointer, ty_flt64 or >=4 is integer
  if (!is_pointer(ty)) {
    if (is_struct(ty))
      fatal("Cannot use a struct type with va_arg(...,type)\n");

    if (is_flonum(ty) && (ty.kind == TY_FLT32))
      fatal("Cannot use flt32 with va_arg(...,type), use flt64 instead\n");

    if (is_integer(ty) && (ty.size < 4))
      fatal("Cannot use [u]int[8|16] with va_arg(...,type), use [u]int32 instead\n");
  }

  // Get the ')'
  rparen();
  e= mkastleaf(A_VAARG, ty, true, NULL, 0);
  e.sym= sym;
  return(e);
}

//- unsigned_expression= UNSIGNED LPAREN expression RPAREN
//-
ASTnode *unsigned_expression(void) {
  ASTnode *e;
  Type *ty;

  // Skip the keyword, get the '('
  scan(&Thistoken);
  lparen();

  // Get the expression in the parentheses
  e= expression();
  ty= e.ty;

  // Check the type, must be a signed scalar
  if (!is_integer(ty) || (ty.is_unsigned == true))
    fatal("Cannot unsign() a %s\n", get_typename(ty));

  // Get the ')'
  rparen();

  // Add an UNSIGNED node and change the type to be unsigned
  e= mkastnode(A_UNSIGNED, e, NULL, NULL);
  if (ty == ty_int8) e.ty= ty_uint8;
  if (ty == ty_int16) e.ty= ty_uint16;
  if (ty == ty_int32) e.ty= ty_uint32;
  if (ty == ty_int64) e.ty= ty_uint64;
  return(e);
}

// Recursively parse a variable with postfix elements
//
//- postfix_variable= IDENT
//-                 | postfix_variable DOT IDENT
//-                 | postfix_variable LBRACKET expression RBRACKET
//-
ASTnode *postfix_variable(ASTnode * n) {
  Sym *sym;
  Sym *memb;
  Type *ty;
  ASTnode *off;
  ASTnode *e;

  // Deal with whatever token we currently have
  switch (Thistoken.token) {
  case T_IDENT:
    if (n != NULL)
      fatal("cannot use identifier %s here\n", Thistoken.tokstr);

    // An identifier. Make an IDENT leaf node
    // with the identifier in Thistoken
    n = mkastleaf(A_IDENT, NULL, false, NULL, 0);
    n.strlit = Thistoken.tokstr;
    n = mkident(n);		// Check variable exists, get its type
    scan(&Thistoken);
    return (postfix_variable(n));

  case T_LBRACKET:
    // An array access. Skip the token
    scan(&Thistoken);

    // Get the expression. Widen it to be int64
    e = expression();
    e = widen_type(e, ty_int64, 0);

    // Check that n is a pointer
    ty = n.ty;
    if (ty.ptr_depth == 0)
      fatal("Cannot do array access on a scalar\n");

    // Do bounds checking if needed
    if (O_boundscheck == true) {
      // Is this an array not a pointer?
      if (n.op == A_IDENT) {
	sym = find_symbol(n.strlit);
	if (is_array(sym)) {
	  // Add the BOUND node with the array's name and size
	  e = mkastnode(A_BOUNDS, e, NULL, NULL);
	  e.count = sym.count;
	  e.strlit = sym.name;
	  e.ty = ty_int64;
	}
      }
    }

    // Get the "value at" type
    ty = value_at(ty);

    // Make a NUMLIT node with the size of the base type
    off = mkastleaf(A_NUMLIT, ty_uint64, true, NULL, ty.size);

    // Multiply this by the expression's value
    e = binop(e, off, A_MULTIPLY);

    // Add on the array's base.
    // Mark this as a pointer
    e = binop(e, n, A_ADDOFFSET);
    e.ty = n.ty;

    // If this isn't a struct,
    // dereference this address
    // and mark it with the correct type
    if (!is_struct(ty)) {
      e = mkastnode(A_DEREF, e, NULL, NULL);
      e.ty = ty;
      e.rvalue = true;
    }

    // Get the trailing right bracket
    match(T_RBRACKET, true);
    return (postfix_variable(e));

  case T_DOT:
    // A member access. Skip the '.'
    scan(&Thistoken);

    // Check that n has struct type with any pointer depth (for now)
    ty = n.ty;
    if (ty.kind != TY_STRUCT)
      fatal("%s is not a struct, cannot use '.'\n", n.strlit);

    // Check that the pointer depth is 0 or 1
    if (ty.ptr_depth > 1)
      fatal("%s is not a struct or struct pointer, cannot use '.'\n",
	    n.strlit);

    // If the variable is a struct (not a pointer), get its address
    if (ty.ptr_depth == 0) {
      if (n.sym != NULL)
	n.op = A_ADDR;
      n.ty = pointer_to(ty);
    } else {
      // It is a pointer, so set ty to the base type
      ty = value_at(ty);
    }

    // Check that the identifier following the '.'
    // is a member of the struct
    for (memb = ty.memb; memb != NULL; memb = memb.next)
      if (strcmp(memb.name, Thistoken.tokstr)==0)
	break;

    if (memb == NULL)
      fatal("No member named %s in struct %s\n", Thistoken.tokstr, n.strlit);

    // Skip the identifier
    scan(&Thistoken);

    // Make a NUMLIT node with the member's offset
    off = mkastleaf(A_NUMLIT, ty_uint64, true, NULL, memb.offset);

    // Add the struct's address and the offset together
    n = binop(n, off, A_ADDOFFSET);

    // If the member is an array or struct, don't
    // dereference it, just set the node's type
    if (is_array(memb) || (is_struct(memb.ty))) {
      n.ty = memb.ty;
    } else {
      // The member isn't an array.
      // Mark the address as a pointer to
      // the dereference'd type
      n.ty = pointer_to(memb.ty);
      n = mkastnode(A_DEREF, n, NULL, NULL);
      n.ty = memb.ty;
    }
    n.rvalue = true;

    return (postfix_variable(n));

  default:
    // Nothing to do
    return (n);
  }
}
