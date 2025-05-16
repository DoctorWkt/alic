// astnodes.c
ASTnode *mkastnode(int op, ASTnode *left, ASTnode *mid, ASTnode *right);
ASTnode *mkastleaf(int op, Type *type, bool rvalue, Sym *sym, uint64_t uintval);
void freeAST(ASTnode *n);
void dumpAST(ASTnode *n, int level);

// cgen.c
void cglabel(int l);
void cgjump(int l);
void cgstrlit(int label, char *val);
void cg_file_preamble(void);
void cg_func_preamble(Sym *func);
void cg_func_postamble(Type *type);
void cgglobsym(Sym *s);
int cgloadlit(Litval value, Type *type);
int cgadd(int t1, int t2, Type * type);
int cgsub(int t1, int t2, Type * type);
int cgmul(int t1, int t2, Type * type);
int cgdiv(int t1, int t2, Type * type);
int cgnegate(int t, Type * type);
int cgcompare(int op, int t1, int t2, Type *type);
void cgjump_if_false(int t1, int label);
int cgnot(int t, Type *type);
int cginvert(int t, Type *type);
int cgand(int t1, int t2, Type *type);
int cgor(int t1, int t2, Type *type);
int cgxor(int t1, int t2, Type *type);
int cgshl(int t1, int t2, Type *type);
int cgshr(int t1, int t2, Type *type);
int cgloadvar(Sym *sym);
void cgstorvar(int t, Type *exprtype, Sym *sym);
int cgcast(int t, Type * type, Type * newtype);
void cgaddlocal(Type *type, Sym *sym);
int cgcall(Sym *sym, int numargs, int *arglist, Type **typelist);
void cgreturn(int temp, Type *type);
int cgloadglobstr(int label);
int cgaddress(Sym *sym);
int cgderef(int t, Type *ty);

// expr.c
ASTnode *binop(ASTnode *l, ASTnode *r, uint op);
ASTnode *unarop(ASTnode *l, uint op);
ASTnode *widen_expression(ASTnode *e, Type *type);

// funcs.c
int add_function(ASTnode *func, ASTnode *paramlist);
void declare_function(ASTnode *f);
void gen_func_statement_block(ASTnode *s);

// genast.c
int genlabel(void);
int genAST(ASTnode *n);

// lexer.c
int scan(Token *t);
char *get_tokenstr(int token);
void dumptokens(void);
void match(int t, bool getnext);
void semi(void);
void lbrace(void);
void rbrace(void);
void lparen(void);
void rparen(void);
void ident(void);
void comma(void);

// main.c
int main(int argc, char *argv[]);

// misc.c
void fatal(const char *fmt, ...);
void cant_do(ASTnode *n, Type *t, char *msg);

// parser.c
void input_file(void);

// strlits.c
int add_strlit(char *name);
void gen_strlits(void);

// stmts.c
ASTnode *assignment_statement(ASTnode *v, ASTnode *e);
ASTnode *declaration_statement(ASTnode *sym, ASTnode * e);

// syms.c
void init_symtable(void);
Sym *add_sym_to(Sym **head, char *name, int symtype, Type * type);
Sym *add_symbol(char *name, int symtype, Type *type, bool isglobal);
Sym *find_symbol(char *name);
void new_scope(Sym *func);
void end_scope(void);
ASTnode *mkident(ASTnode *n);
void gen_globsyms(void);
void dumpsyms(void);

// types.c
void init_typelist(void);
Type *new_type(TypeKind kind, int size, int ptr_depth, char *name, Type *base);
Type *find_type(char *typename, TypeKind kind, int ptr_depth);
Type *pointer_to(Type *ty);
Type *value_at(Type *ty);
bool is_integer(Type *ty);
bool is_flonum(Type *ty);
bool is_numeric(Type *ty);
bool is_pointer(Type * ty);
char *get_typename(Type *ty);
ASTnode *widen_type(ASTnode *node, Type *ty);
void add_type(ASTnode *node);
Type *parse_litval(Litval e, int numtype);
