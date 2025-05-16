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

// Built-in type ids.
typedef enum {
  TY_VOID,  TY_BOOL,  TY_INT8,  TY_INT16,
  TY_INT32, TY_INT64, TY_FLT32, TY_FLT64
} TypeKind;

// Type structure. Built-ins are kept as
// separate variables. We keep a linked
// list of user-defined types
typedef struct Type Type;

struct Type {
  TypeKind kind;
  int size;           // sizeof() value
  int align;          // alignment
  bool is_unsigned;   // unsigned or signed
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
  T_EOF,

  // Binary operators in ascending precedence order
  T_AMPER, T_OR, T_XOR,
  T_EQ, T_NE, T_LT, T_GT, T_LE, T_GE,
  T_LSHIFT, T_RSHIFT,
  T_PLUS, T_MINUS, T_STAR, T_SLASH, T_MOD,

  // Other operators
  T_ASSIGN, T_INVERT, T_LOGNOT, T_LOGAND, T_LOGOR,

  // Built-in type keywords
  T_VOID,  T_BOOL,
  T_INT8,  T_INT16,  T_INT32,  T_INT64,
  T_UINT8, T_UINT16, T_UINT32, T_UINT64,
  T_FLT32, T_FLT64,

  // Other keywords
  T_IF, T_ELSE, T_FALSE, T_FOR, T_PRINTF,
  T_TRUE, T_WHILE,

  // Structural tokens
  T_NUMLIT, T_STRLIT, T_SEMI, T_IDENT,
  T_LBRACE, T_RBRACE, T_LPAREN, T_RPAREN,
  T_COMMA
};

// What type of data is in the Token's numval:
// signed or unsigned int, float, or originally
// a character literal
enum {
  NUM_INT=1, NUM_UINT, NUM_FLT, NUM_CHAR
};

// Token structure
typedef struct _token {
  int token;			// Token id from the enum list
  char *tokstr;			// For T_STRLIT, the string value
  Litval numval;		// For T_NUMLIT, the numerical value
  int numtype;			// and the type of numerical value
} Token;

// We keep a linked list of string literals
typedef struct _strlit {
  char *val;		// The string literal
  int label;		// Label associated with the string
  struct _strlit *next;
} Strlit;

// We keep a linked list of symbols (variables, functions etc.)
typedef struct _sym {
  char *name;		// Symbol's name.
  int  symtype;		// Is this a variable, function etc.
  bool has_addr;	// Does the symbol have an address?
  Type *type;		// Pointer to the symbol's type
			// TODO: functions and others
  Litval initval;	// Symbol's initial value. For functions: has
			// the function already been declared with
			// a statement block
  int count;		// Number of struct members or function parameters
  struct _sym *memb;	// List of function params, or struct members
  struct _sym *next;	// Pointer to the next symbol
} Sym;

// Symbol types
enum {
  ST_VARIABLE=1, ST_FUNCTION
};

// Abstract Syntax Tree structure
typedef struct _astnode {
  int op;                   // "Operation" to be performed on this tree
  Type *type;			// Pointer to the node's type
  bool rvalue;                  // True if the node is an rvalue
  struct _astnode *left;        // Left, middle and right child trees
  struct _astnode *mid;
  struct _astnode *right;
  Sym *sym;			// For many AST nodes, the pointer to
                                // the symbol in the symbol table
  Litval litval;		// For A_NUMLIT, the numeric literal value
  char *strlit;			// For some nodes, the string literal value
} ASTnode;

// AST node types
enum {
  A_ASSIGN = 1, A_CAST,						// 1
  A_ADD, A_SUBTRACT, A_MULTIPLY, A_DIVIDE, A_NEGATE,		// 3
  A_EQ, A_NE, A_LT, A_GT, A_LE, A_GE, A_NOT,			// 8
  A_AND, A_OR, A_XOR, A_INVERT,					// 15
  A_LSHIFT, A_RSHIFT,						// 19
  A_NUMLIT, A_IDENT, A_PRINT, A_GLUE, A_IF, A_WHILE, A_FOR,	// 21
  A_TYPE, A_STRLIT, A_LOCAL, A_FUNCCALL				// 28
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

extern Sym *Symhead;		// Head of the symbol table

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

extern bool O_dumptokens;	// Dump the input file's tokens
extern bool O_dumpsyms;		// Dump the symbol table
extern bool O_dumpast;		// Dump each function's AST tree
