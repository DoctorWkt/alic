// astnodes.c
ASTnode *mkastnode(int op, Type *type, bool rvalue, ASTnode *left, ASTnode *mid, ASTnode *right, Sym *sym, uint64_t intval);
ASTnode *mkastleaf(int op, Type *type, bool rvalue, Sym *sym, uint64_t uintval);
void dumpAST(ASTnode *n, int label, int level);

// cgen.c
void cglabel(int l);
void cgjump(int l);
void cg_file_preamble(void);
void cg_func_preamble(void);
void cg_func_postamble(void);
void cgglobsym(Sym *s);
void cg_printint(int temp);
void cg_printdbl(int temp);
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

// expr.c
ASTnode *binop(ASTnode *l, ASTnode *r, uint op);
ASTnode *unarop(ASTnode *l, uint op);

// genast.c
int genlabel(void);
int genAST(ASTnode *n);

// main.c
int main(int argc, char *argv[]);

// misc.c
void fatal(const char *fmt, ...);

// parse.c
int yyparse(void);

// stmts.c
ASTnode *print_statement(ASTnode *e);
ASTnode *assignment_statement(ASTnode *v, ASTnode *e);
ASTnode *declaration_statement(char *symname, ASTnode *e, Type *ty);

// syms.c
Sym *add_symbol(char *name, bool is_static, Type *type, uint64_t initval);
Sym *find_symbol(char *name);
ASTnode *mkident(char *name);
void gen_globsyms(void);

// types.c
bool is_integer(Type *ty);
bool is_flonum(Type *ty);
bool is_numeric(Type *ty);
char *get_typename(Type *ty);
ASTnode *widen_type(ASTnode *node, Type *ty);
void add_type(ASTnode *node);
uint64_t parse_litval(char *litstr, Type **type);
