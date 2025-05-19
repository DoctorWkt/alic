// Structures and definitions for the alic compiler.
// (c) 2025 Warren Toomey, GPL3

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>

#define TEXTLEN 512		// Used by several buffers

// Type kinds
typedef enum {
  TY_VOID,  TY_BOOL,  TY_INT8,  TY_INT16,
  TY_INT32, TY_INT64, TY_FLT32, TY_FLT64,
  TY_USER, TY_STRUCT
} TypeKind;

// Type structure. Built-ins are kept as
// separate variables. We keep a linked
// list of user-defined types
typedef struct Type Type;

// For structs and unions, we keep a
// linked list of members and their type
typedef struct Memb Memb;

struct Memb {
  char *name;		// Name of this member
  Type *type;		// Type of this member
  int offset;		// Offset of this member from base address
  Memb *next;		// Next member in the struct
};

struct Type {
  TypeKind kind;
  int size;		// sizeof() value
  bool is_unsigned;	// unsigned or signed
  int ptr_depth;	// Number of derefs to base type
  char *name;		// Name of user-defined type
  Type *basetype;	// Pointer to the base type if this is an alias
  Memb *memb;		// List of members for structs
  Type *next;
};

// Integer and real literal values are represented by this union
typedef union {
  int64_t  intval;
  uint64_t uintval;
  double   dblval;
} Litval;

// List of token ids
enum {
  T_EOF,						// 1

  // Binary operators
  T_AMPER, T_OR, T_XOR,					// 1
  T_EQ, T_NE, T_LT, T_GT, T_LE, T_GE,			// 4
  T_LSHIFT, T_RSHIFT,					// 10
  T_PLUS, T_MINUS, T_STAR, T_SLASH, T_MOD,		// 12

  // Other operators
  T_ASSIGN, T_INVERT, T_LOGNOT, T_LOGAND, T_LOGOR,	// 17
  T_POSTINC, T_POSTDEC,					// 22

  // Built-in type keywords
  T_VOID,  T_BOOL,					// 24
  T_INT8,  T_INT16,  T_INT32,  T_INT64,			// 26
  T_UINT8, T_UINT16, T_UINT32, T_UINT64,		// 30
  T_FLT32, T_FLT64,					// 34

  // Other keywords
  T_IF, T_ELSE, T_FALSE, T_FOR,				// 36
  T_TRUE, T_WHILE, T_RETURN, T_NULL,			// 40
  T_TYPE, T_ENUM, T_STRUCT, T_UNION,			// 44
  T_TRY, T_CATCH, T_THROWS, T_ABORT,			// 48
  T_BREAK, T_CONTINUE, T_SIZEOF,			// 52
  T_SWITCH, T_CASE, T_DEFAULT, T_FALLTHRU,		// 55

  // Structural tokens
  T_NUMLIT, T_STRLIT, T_SEMI, T_IDENT,			// 59
  T_LBRACE, T_RBRACE, T_LPAREN, T_RPAREN,		// 63
  T_COMMA, T_ELLIPSIS, T_DOT,				// 67
  T_LBRACKET, T_RBRACKET, T_COLON			// 71
};

// What type of data is in the Token's numval:
// signed or unsigned int, float, or originally
// a character literal
enum {
  NUM_INT=1, NUM_UINT, NUM_FLT, NUM_CHAR
};

// Token structure
typedef struct Token Token;
struct Token {
  int token;			// Token id from the enum list
  char *tokstr;			// For T_STRLIT, the string value
  Litval numval;		// For T_NUMLIT, the numerical value
  int numtype;			// and the type of numerical value
};

// We keep a linked list of string literals
typedef struct Strlit Strlit;
struct Strlit {
  char *val;		// The string literal
  int label;		// Label associated with the string
  Strlit *next;
};

// We keep a linked list of symbols (variables, functions etc.)
typedef struct Sym Sym;
struct Sym {
  char *name;		// Symbol's name.
  int  symtype;		// Is this a variable, function etc.
  int  visibility;	// The symbol's visibility
  bool has_addr;	// Does the symbol have an address?
  Type *type;		// Pointer to the symbol's type
			// TODO: functions and others
  Litval initval;	// Symbol's initial value. For functions: has
			// the function already been declared with
			// a statement block
  int count;		// Number of struct members or function parameters
			// For a function, -1 means it is variadic
  Sym *memb;		// List of function parameters
  Sym *exceptvar;	// Function variable that holds an exception
  Sym *next;		// Pointer to the next symbol
};

// Symbol types
enum {
  ST_VARIABLE=1, ST_FUNCTION, ST_ENUM
};

// Symbol visibility
enum {
  SV_LOCAL=1, SV_GLOBAL
};

// A scope holds a symbol table, and scopes are linked so that
// we search the most recent scope first.
typedef struct Scope Scope;
struct Scope {
  Sym *head;		// Head of the scope's symbol table
  Scope *next;		// Pointer to the next scope
};

// Abstract Syntax Tree structure
typedef struct ASTnode ASTnode;
struct ASTnode {
  int op;			// "Operation" to be performed on this tree
  Type *type;			// Pointer to the node's type
  bool rvalue;                  // True if the node is an rvalue
  ASTnode *left;        	// Left, middle and right child trees
  ASTnode *mid;
  ASTnode *right;
  Sym *sym;			// For many AST nodes, the pointer to
                                // the symbol in the symbol table
  Litval litval;		// For A_NUMLIT, the numeric literal value
  char *strlit;			// For some nodes, the string literal value
};

// AST node types
enum {
  A_ASSIGN = 1, A_CAST,						// 1
  A_ADD, A_SUBTRACT, A_MULTIPLY, A_DIVIDE, A_NEGATE,		// 3
  A_EQ, A_NE, A_LT, A_GT, A_LE, A_GE, A_NOT,			// 8
  A_AND, A_OR, A_XOR, A_INVERT,					// 15
  A_LSHIFT, A_RSHIFT,						// 19
  A_NUMLIT, A_IDENT, A_BREAK, A_GLUE, A_IF, A_WHILE, A_FOR,	// 21
  A_TYPE, A_STRLIT, A_LOCAL, A_FUNCCALL, A_RETURN, A_ADDR,	// 28
  A_DEREF, A_ABORT, A_TRY, A_CONTINUE, A_SCALE, A_ADDOFFSET,	// 34
  A_SWITCH, A_CASE, A_DEFAULT, A_FALLTHRU, A_MOD,		// 40
  A_LOGAND, A_LOGOR,						// 45
};

// The value when a code generator function
// has no temporary number to return
#define NOREG -1

// External variables and structures
extern char *Infilename;	// Name of file we are parsing
extern FILE *Infh;		// The input file handle
extern FILE *Outfh; 		// The output file handle
extern FILE *Debugfh;		// The debugging file handle
extern int Line;		// Current line number

extern Token Peektoken;		// A look-ahead token
extern Token Thistoken;		// The last token scanned
extern char Text[];		// Text of the last token scanned

extern Sym *Thisfunction;	// The function we are parsing

extern Type *ty_void;		// The built-in types
extern Type *ty_bool;
extern Type *ty_int8;
extern Type *ty_int16;
extern Type *ty_int32;
extern Type *ty_int64;
extern Type *ty_uint8;
extern Type *ty_uint16;
extern Type *ty_uint32;
extern Type *ty_uint64;
extern Type *ty_flt32;
extern Type *ty_flt64;
extern Type *ty_voidptr;
extern Type *ty_int8ptr;
extern Type *Typehead;		// Head of the type list

extern int Linestart;		// In lexer, we are at start of the line
extern int Putback;		// The token that was put back

extern bool O_dumptokens;	// Dump the input file's tokens
extern bool O_dumpsyms;		// Dump the symbol table
extern bool O_dumpast;		// Dump each function's AST tree
extern bool O_logmisc;		// Log miscellaneous things
