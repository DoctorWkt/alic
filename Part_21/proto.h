// astnodes.c
ASTnode *mkastnode(int op, ASTnode * left, ASTnode * mid, ASTnode * right);
ASTnode *mkastleaf(int op, Type * type, bool rvalue, Sym * sym,
		   uint64_t uintval);
// void freeAST(ASTnode *n);
void dumpAST(ASTnode * n, int level);
ASTnode *optAST(ASTnode * n);

// cgen.c
int cgalloctemp(void);
void cglabel(int l);
void cgjump(int l);
int cgalign(Type * ty, int offset);
void cgstrlit(int label, char *val, bool is_const);
void cg_file_preamble(void);
void cg_func_preamble(Sym * func);
void cg_func_postamble(Type * type);
void cgglobsym(Sym * sym, bool make_zero);
void cgglobsymval(ASTnode * value, int offset);
void cgglobsymend(Sym * sym);
void cgloadboolean(int t, int val, Type * type);
int cgloadlit(Litval * value, Type * type);
int cgadd(int t1, int t2, Type * type);
int cgsub(int t1, int t2, Type * type);
int cgmul(int t1, int t2, Type * type);
int cgdiv(int t1, int t2, Type * type);
int cgmod(int t1, int t2, Type * type);
int cgnegate(int t, Type * type);
int cgcompare(int op, int t1, int t2, Type * type);
void cgjump_if_false(int t1, int label);
int cgnot(int t, Type * type);
int cginvert(int t, Type * type);
int cgand(int t1, int t2, Type * type);
int cgor(int t1, int t2, Type * type);
int cgxor(int t1, int t2, Type * type);
int cgshl(int t1, int t2, Type * type);
int cgshr(int t1, int t2, Type * type);
int cgloadvar(Sym * sym);
void cgrangecheck(int t, Type *ty, int funcname);
int cgstorvar(int t, Type * exprtype, Sym * sym);
int cgstore_element(int basetemp, int offset, int exprtemp, Type *ty);
void cgaddlocal(Type * type, Sym * sym, int size, bool makezero,
		bool isarray);
int cgcall(Sym * sym, int numargs, int excepttemp, int *arglist,
	   Type ** typelist);
void cgreturn(int temp, Type * type);
void cgabort(void);
int cgloadglobstr(int label);
int cgaddress(Sym * sym);
int cgderef(int t, Type * ty);
int cgstorderef(int t1, int t2, Type * ty);
int cgboundscheck(int t1, int count, int aryname, int funcname);
void cgmove(int t1, int t2, Type * ty);
void cg_vastart(ASTnode *n);
void cg_vaend(ASTnode *n);
int cg_vaarg(ASTnode *n);
int cgcast(int exprtemp, Type *ety, Type *ty, int funcname);
int cg_getaaval(int arytemp, int keytemp, Type *ty);
void cg_setaaval(int arytemp, int keytemp, int valtemp, Type *ty);
int cg_existsaaval(int arytemp, int keytemp);
int cg_delaaval(int arytemp, int keytemp);
int cg_strhash(int keytemp);
int cg_free_aarray(Sym * sym);
int cg_aaiterstart(int arytemp);
int cg_aanext(int arytemp);
int cg_funciterator(ASTnode * n, Breaklabel *this);
void cg_stridxcheck(int idxtemp, int basetemp, int funcname);
int cg_stringiterator(ASTnode * n, Breaklabel *this);
int cg_arrayiterator(ASTnode * n, Breaklabel *this);

// expr.c
ASTnode *binop(ASTnode * l, ASTnode * r, int op);
ASTnode *unarop(ASTnode * l, int op);
ASTnode *widen_expression(ASTnode * e, Type * type);
ASTnode *get_ary_offset(Sym *sym, ASTnode *e, ASTnode *previdx, int level);

// funcs.c
bool add_function(ASTnode * func, ASTnode * paramlist, int visibility);
void declare_function(ASTnode * f, int visibility);
void gen_func_statement_block(ASTnode * s);

// genast.c
int genlabel(void);
int genAST(ASTnode * n);
int genalign(Type * ty, int offset);
void gen_file_preamble(void);
void gen_func_preamble(Sym * func);
void gen_func_postamble(Type * type);
int gen_assign(int ltemp, int rtemp, ASTnode *n);
void check_bel(Sym * sym, ASTnode * list, int offset, bool is_element, int basetemp);

// lexer.c
int scan(Token * t);
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
void lfatal(int line, const char *fmt, ...);
void cant_do(ASTnode * n, Type * t, char *msg);
void *Malloc(size_t size);
void *Calloc(size_t size);
uint64_t djb2hash(uint8_t * str);

// parser.c
void input_file(void);

// strlits.c
int add_strlit(char *name, bool is_const);
void gen_strlits(void);

// stmts.c
ASTnode *assignment_statement(ASTnode * v, ASTnode * e);
ASTnode *declaration_statement(ASTnode * sym, ASTnode * e);

// syms.c
void init_symtable(void);
Sym *add_sym_to(Sym ** head, char *name, int symtype, Type * type);
Sym *add_symbol(char *name, int symtype, Type * type, int visibility);
Sym *find_symbol(char *name);
void new_scope(Sym * func);
ASTnode *end_scope(void);
ASTnode *mkident(ASTnode * n);
bool is_array(Sym * sym);
int get_numelements(Sym *sym, int depth);
int get_varsize(Sym *sym);
void dumpsyms(void);

// types.c
void init_typelist(void);
Type *new_type(int kind, int size, bool is_unsigned, int ptr_depth, char *name, Type * base);
Type *find_type(char *typename, int kind, bool is_unsigned, int ptr_depth);
Type *pointer_to(Type * ty);
Type *value_at(Type * ty);
bool is_integer(Type * ty);
bool is_flonum(Type * ty);
bool is_numeric(Type * ty);
bool is_pointer(Type * ty);
bool is_struct(Type * ty);
char *get_typename(Type * ty);
ASTnode *widen_type(ASTnode * node, Type * ty, int op);
void add_type(ASTnode * node);
Type *parse_litval(Litval * e);
bool has_range(Type *ty);
Type *get_funcptr_type(Sym *sym);
