// Lexical tokeniser for the alic compiler.
// (c) 2019, 2025 Warren Toomey, GPL3

#include "alic.ah"
#include "proto.ah"

// Get the next character from the input file.
int next(void) {
  int c;
  int l;

  if (Putback != 0) {		// Use the character put
    c = Putback;		// back if there is one
    Putback = 0;
    return (c);
  }

  c = fgetc(Infh);			// Read from input file

  while (Linestart && c == '#') {	// We've hit a pre-processor statement
    Linestart = false;			// No longer at the start of the line
    scan(Thistoken);			// Get the line number into l
    if (Thistoken.token != T_NUMLIT)
      fatal("Expecting pre-processor line number, got %s\n", Text);
    l = cast(Thistoken.litval.intval, int);

    scan(Thistoken);			// Get the filename in Text
    if (Thistoken.token != T_STRLIT)
      fatal("Expecting pre-processor file name, got %s\n", Text);

    if (Text[0] != '<') {		// If this is a real filename
      if (strcmp(Text, Infilename)!=0)	// and not the one we have now
	Infilename = strdup(Text);	// save it. Then update the line num
      Line = l;
    }

    while (true) {
      c = fgetc(Infh);
      if (c == '\n') break;		// Skip to the end of the line
    }
    c = fgetc(Infh);			// Get the next character
    Linestart = true;			// Now back at the start of the line
  }

  Linestart = false;			// No longer at the start of the line
  if ('\n' == c) {
    Line++;				// Increment line count
    Linestart = true;			// Now back at the start of the line
  }

  return (c);
}

// Put back an unwanted character
void putback(int c) {
  Putback = c;
}

// Skip past input that we don't need to deal with, 
// i.e. whitespace, newlines. Return the first
// character we do need to deal with.
int skip(void) {
  int c;

  c = next();
  while (isspace(c) != 0)
    c = next();
  return (c);
}

// Return the position of character c
// in string s, or -1 if c not found
int chrpos(char *s, int c) {
  int i;
  for (i = 0; s[i] != '\0'; i++)
    if (s[i] == c)
      return (i);
  return (-1);
}

// Read in a hexadecimal constant from the input
int hexchar(void) {
  int c;
  int h;
  int n;
  bool f = false;

  // Loop getting characters
  while (true) {
    c= next();
    if (isxdigit(c)==false) break;
    // Convert from char to int value
    h = chrpos("0123456789abcdef", tolower(c));

    // Add to running hex value
    n = n * 16 + h;
    f = true;
  }

  // We hit a non-hex character, put it back
  putback(c);

  // Flag tells us we never saw any hex characters
  if (f == false)
    fatal("Missing digits after '\\x'\n");
  if (n > 255)
    fatal("Value out of range after '\\x'\n");

  return (n);
}

// Return the next character from a character
// or string literal. Also return if the character
// was escaped
int scanch(bool *was_escaped) {
  int i;
  int c;
  int c2;

  // Get the next input character and interpret
  // metacharacters that start with a backslash
  c = next();
  if (was_escaped != NULL)
    *was_escaped= false;
  if (c == '\\') {
    if (was_escaped != NULL)
    *was_escaped= true;
    c = next();
    switch (c) {
    case 'a':
      return ('\a');
    case 'b':
      return ('\b');
    case 'f':
      return ('\f');
    case 'n':
      return ('\n');
    case 'r':
      return ('\r');
    case 't':
      return ('\t');
    case 'v':
      return ('\v');
    case '\\':
      return ('\\');
    case '"':
      return ('"');
    case '\'':
      return ('\'');

      // Deal with octal constants by reading in
      // characters until we hit a non-octal digit.
      // Build up the octal value in c2 and count
      // # digits in i. Permit only 3 octal digits.
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
      c2= 0;
      for (i = 0; (isdigit(c)!=0) && (c < '8'); c = next()) {
	i++;
	if (i > 3)
	  break;
	c2 = c2 * 8 + (c - '0');
      }

      putback(c);		// Put back the first non-octal char
      return (c2);
    case 'x':
      return (hexchar());
    default:
      fatal("Unknown escape sequence %c\n", c);
    }
  }

  return (c);			// Just an ordinary old character!
}

// List of characters that can be found in a numeric literal
char *numchar = "0123456789ABCDEFabcdef.x";

// Scan a numeric literal value from the input file
// and store it in the given token pointer
void scan_numlit(Token * t, int c, bool is_negative) {
  int i = 0;
  bool isfloat = false;
  int radix = 10;

  // Assume an unsigned int
  t.litval.numtype = NUM_UINT;

  // Put the first character and negative sign in the buffer
  if (is_negative) {
    Text[i] = '-'; i++;
    t.litval.numtype = NUM_INT;
  }

  Text[i] = cast(c, int8); i++;

  // Loop while we have enough buffer space
  for (; i < TEXTLEN - 1; i++) {
    c = scanch(NULL);

    // Found a non-numeric character
    if (chrpos(numchar, c) == -1) {
      putback(c);
      break;
    }

    // Otherwise add it to the buffer
    Text[i] = cast(c, int8);
  }

  // NUL terminate the string
  Text[i] = '\0';

  // Determine either if it's a float
  // or any octal/hex radix
  if (strchr(Text, '.') != NULL) {
    isfloat = true;
    t.litval.numtype = NUM_FLT;
  } else {
    if (Text[0] == '0') {
      if (Text[1] == 'x')
	radix = 16;
      else
	radix = 8;
    }
  }

  // Do the conversion
  if (isfloat)
    t.litval.dblval = strtod(Text, NULL);
  else
    t.litval.uintval = strtoull(Text, NULL, radix);
}

// Scan in a string literal from the input file,
// and store it in buf[]. Return the length of
// the string. 
int scanstr(char *buf) {
  int i;
  int c;
  bool was_escaped;

  // Loop while we have enough buffer space
  foreach i (0 ... TEXTLEN - 1) {
    // Get the next char and append to buf
    // Return when we hit the ending double quote
    c = scanch(&was_escaped);
    if ((c == '"') && (was_escaped == false)) {
      buf[i] = 0;
      return (i);
    }
    buf[i] = cast(c, char);
  }

  // Ran out of buf[] space
  fatal("String literal too long\n");
  return (0);
}

// Scan an identifier from the input file and
// store it in buf[]. Return the identifier's length
int scanident(int c, char *buf, int lim) {
  int i = 0;

  // Allow digits, alpha and underscores
  while (isalpha(c)!=0 || isdigit(c)!=0 || '_' == c) {
    // Error if we hit the identifier length limit,
    // else append to buf[] and get next character
    if (lim - 1 == i) {
      fatal("Identifier too long\n");
    } else if (i < lim - 1) {
      buf[i] = cast(c, char); i++;
    }
    c = next();
  }

  // We hit a non-valid character, put it back.
  // NUL-terminate the buf[] and return the length
  putback(c);
  buf[i] = '\0';
  return (i);
}


// A structure to hold a keyword, its first letter
// and the token id associated with the keyword
type Keynode= struct {
  char first,
  char *keyword,
  int token
};

// List of keywords and matching tokens
Keynode keylist[49] = {
  {'N', "NULL", T_NULL},
  {'a', "abort", T_ABORT},
  {'b', "bool", T_BOOL},
  {'b', "break", T_BREAK},
  {'c', "case", T_CASE},
  {'c', "cast", T_CAST},
  {'c', "catch", T_CATCH},
  {'c', "const", T_CONST},
  {'c', "continue", T_CONTINUE},
  {'d', "default", T_DEFAULT},
  {'e', "else", T_ELSE},
  {'e', "exists", T_EXISTS},
  {'e', "enum", T_ENUM},
  {'e', "extern", T_EXTERN},
  {'f', "fallthru", T_FALLTHRU},
  {'f', "false", T_FALSE},
  {'f', "flt32", T_FLT32},
  {'f', "flt64", T_FLT64},
  {'f', "for", T_FOR},
  {'f', "foreach", T_FOREACH},
  {'f', "funcptr", T_FUNCPTR},
  {'i', "if", T_IF},
  {'i', "inout", T_INOUT},
  {'i', "int8", T_INT8},
  {'i', "int16", T_INT16},
  {'i', "int32", T_INT32},
  {'i', "int64", T_INT64},
  {'p', "public", T_PUBLIC},
  {'r', "range", T_RANGE},
  {'r', "return", T_RETURN},
  {'s', "sizeof", T_SIZEOF},
  {'s', "struct", T_STRUCT},
  {'s', "switch", T_SWITCH},
  {'t', "throws", T_THROWS},
  {'t', "true", T_TRUE},
  {'t', "try", T_TRY},
  {'t', "type", T_TYPE},
  {'u', "uint8", T_UINT8},
  {'u', "uint16", T_UINT16},
  {'u', "uint32", T_UINT32},
  {'u', "uint64", T_UINT64},
  {'u', "undef", T_UNDEF},
  {'u', "union", T_UNION},
  {'v', "va_arg", T_VAARG},
  {'v', "va_start", T_VASTART},
  {'v', "va_end", T_VAEND},
  {'v', "void", T_VOID},
  {'w', "while", T_WHILE},
  {0, NULL, 0}
};

// Given a word from the input, return the matching
// keyword token number or 0 if it's not a keyword.
// Switch on the first letter so that we don't have
// to waste time strcmp()ing against all the keywords.
int keyword(char *s) {
  int i;

  for (i = 0; keylist[i].first != 0; i++) {
    // Too early
    if (keylist[i].first < *s)
      continue;

    // A match
    if (strcmp(s, keylist[i].keyword)==0)
      return (keylist[i].token);

    // Too late
    if (keylist[i].first > *s)
      return (0);
  }

  return (0);
}

// Scan and return the next token found in the input.
// Return true if token valid, false if no tokens left.
public bool scan(inout Token t) {
  int c;
  int tokentype;

  // If we have a lookahead token, return this token
  if (Peektoken.token != 0) {
    t.token = Peektoken.token;
    t.tokstr = Peektoken.tokstr;
    t.litval.intval = Peektoken.litval.intval;
    t.litval.numtype = Peektoken.litval.numtype;
    Peektoken.token = 0;
    return (true);
  }

  // Skip whitespace
  c = skip();

  // Determine the token based on
  // the input character
  switch (c) {
  case EOF:
    t.token = T_EOF;
    return (false);
  case '+':
    c = next();
    if (c == '+') {
      t.token = T_POSTINC;
    } else {
      putback(c);
      t.token = T_PLUS;
    }
  case '-':
    c = next();
    if (c == '-') {
      t.token = T_POSTDEC;
    } else if (isdigit(c) != 0) {	// Negative numeric literal
      scan_numlit(t, c, true);
      t.token = T_NUMLIT;
    } else {
      putback(c);
      t.token = T_MINUS;
    }
  case '*':
    t.token = T_STAR;
  case '%':
    t.token = T_MOD;
  case '/':
    t.token = T_SLASH;
  case ';':
    t.token = T_SEMI;
  case '{':
    t.token = T_LBRACE;
  case '}':
    t.token = T_RBRACE;
  case '(':
    t.token = T_LPAREN;
  case ')':
    t.token = T_RPAREN;
  case '~':
    t.token = T_INVERT;
  case '^':
    t.token = T_XOR;
  case '[':
    t.token = T_LBRACKET;
  case ']':
    t.token = T_RBRACKET;
  case ':':
    t.token = T_COLON;
  case ',':
    t.token = T_COMMA;
  case '?':
    t.token = T_QUESTION;
  case '.':
    c = next();
    if (c == '.') {
      t.token = T_ELLIPSIS;
      c = next();
      if (c != '.')
	fatal("Expected '...', only got '..'\n");
    } else {
      putback(c);
      t.token = T_DOT;
    }
  case '=':
    c = next();
    if (c == '=') {
      t.token = T_EQ;
    } else {
      putback(c);
      t.token = T_ASSIGN;
    }
  case '!':
    c = next();
    if (c == '=') {
      t.token = T_NE;
    } else {
      putback(c);
      t.token = T_LOGNOT;
    }
  case '<':
    c = next();
    if (c == '=') {
      t.token = T_LE;
    } else if (c == '<') {
      t.token = T_LSHIFT;
    } else {
      putback(c);
      t.token = T_LT;
    }
  case '>':
    c = next();
    if (c == '=') {
      t.token = T_GE;
    } else if (c == '>') {
      t.token = T_RSHIFT;
    } else {
      putback(c);
      t.token = T_GT;
    }
  case '&':
    c = next();
    if (c == '&') {
      t.token = T_LOGAND;
    } else {
      putback(c);
      t.token = T_AMPER;
    }
  case '|':
    c = next();
    if (c == '|') {
      t.token = T_LOGOR;
    } else {
      putback(c);
      t.token = T_OR;
    }
  case '\'':
    // If it's a quote, scan in the
    // literal character value and
    // the trailing quote
    t.litval.intval = scanch(NULL);
    t.litval.numtype = NUM_CHAR;
    t.token = T_NUMLIT;
    if (next() != '\'')
      fatal("Expected '\\'' at end of char literal\n");
  case '"':
    // Scan in a literal string
    scanstr(Text);
    t.token = T_STRLIT;
    t.tokstr = strdup(Text);
  default:
    // If it's a digit, scan the
    // literal integer value in
    if (isdigit(c) != 0) {
      scan_numlit(t, c, false);
      t.token = T_NUMLIT;
      return (true);
    } else if (isalpha(c)!=0 || '_' == c) {
      // Read in a keyword or identifier
      scanident(c, Text, TEXTLEN);

      // If it's a recognised keyword, return that token
      tokentype = keyword(Text);
      if (tokentype != 0) {
	t.token = tokentype;
	return (true);
      } else {
        // Not a recognised keyword, so it must be an identifier
        t.token = T_IDENT;
        t.tokstr = strdup(Text);
	return (true);
      }
    }

    // The character isn't part of any recognised token, error
    fatal("Unrecognised character: %c\n", c);
  }

  return (true);
}

// List of tokens as strings
char *tokstr[87] = {
  "EOF",

  "&", "|", "^",
  "==", "!=", "<", ">", "<=", ">=",
  "<<", ">>",
  "+", "-", "*", "/", "%",

  "=", "~", "!", "&&", "||", "++", "--", "?",

  "void", "bool",
  "int8", "int16", "int32", "int64",
  "uint8", "uint16", "uint32", "uint64",
  "flt32", "flt64",

  "if", "else", "false", "for",
  "true", "while", "return", "NULL",
  "type", "enum", "struct", "union",
  "try", "catch", "throws", "abort",
  "break", "continue", "sizeof",
  "switch", "case", "default", "fallthru",
  "public", "extern",
  "va_start", "va_arg", "va_end",
  "cast", "const", "foreach",
  "exists", "undef", "inout", "range",
  "funcptr",

  "numlit", "strlit", ";", "ident",
  "{", "}", "(", ")",
  ",", "...", ".", "[", "]", ":"
};

char *get_tokenstr(int token) {
  return (tokstr[token]);
}

// Dump the tokens in the input file
void dumptokens(void) {
  Token t;

  while (true) {
    if (scan(t) == 0)
      return;
    fprintf(Debugfh, "%s", tokstr[t.token]);
    switch (t.token) {
    case T_STRLIT:
      fprintf(Debugfh, " \"%s\"", Text);
    case T_NUMLIT:
      if (t.litval.numtype == NUM_CHAR) {
	fprintf(Debugfh, " '%c'", t.litval.intval);
      } else
        fallthru;
    case T_IDENT:
      fprintf(Debugfh, " %s", Text);
    }
    fprintf(Debugfh, "\n");
  }
}

// Ensure that the current token is t,
// and psossibly fetch the next token.
// Otherwise throw an error
public void match(const int t, const bool getnext) {
  if (Thistoken.token != t)
    fatal("Expected %s, got %s\n", tokstr[t], tokstr[Thistoken.token]);

  if (getnext)
    scan(Thistoken);
}

// Match a semicolon and fetch the next token
void semi(void) {
  match(T_SEMI, true);
}

// Match a left brace and fetch the next token
void lbrace(void) {
  match(T_LBRACE, true);
}

// Match a right brace and fetch the next token
void rbrace(void) {
  match(T_RBRACE, true);
}

// Match a left parenthesis and fetch the next token
void lparen(void) {
  match(T_LPAREN, true);
}

// Match a right parenthesis and fetch the next token
void rparen(void) {
  match(T_RPAREN, true);
}

// Match an identifer and fetch the next token
void ident(void) {
  match(T_IDENT, true);
}

// Match a comma and fetch the next token
void comma(void) {
  match(T_COMMA, true);
}
