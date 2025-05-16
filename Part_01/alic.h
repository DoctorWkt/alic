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

// We keep a linked list of symbols (variables, functions etc.)
typedef struct _sym {
  char *name;		// Symbol's name. If NULL, marks the end of a scope
  bool is_static;	// Is it static?
  Type *type;		// Pointer to the symbol's type
			// TODO: functions and others
  Litval initval;	// Symbol's initial value
  struct _sym *next;	// Pointer to the next symbol
} Sym;

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
  Litval litval;		// For A_NUMLIT, the literal value
  int linenum;                  // Line number from where this node comes
} ASTnode;

// AST node types
enum {
  A_ASSIGN = 1, A_CAST,
  A_ADD, A_SUBTRACT, A_MULTIPLY, A_DIVIDE, A_NEGATE,
  A_EQ, A_NE, A_LT, A_GT, A_LE, A_GE, A_NOT,
  A_AND, A_OR, A_XOR, A_INVERT,
  A_LSHIFT, A_RSHIFT,
  A_NUMLIT, A_IDENT,
};


#define NOREG -1
