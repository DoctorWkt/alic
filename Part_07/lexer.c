// Lexical tokeniser for the alic compiler.
// (c) 2019, 2025 Warren Toomey, GPL3

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "alic.h"
#include "proto.h"

int Linestart= 0;
int Putback= 0;
char Text[TEXTLEN + 1];		// Text of the last token scanned
Token Peektoken;		// A look-ahead token
Token Thistoken;		// The last token scanned

// Get the next character from the input file.
static int next(void) {
  int c, l;

  if (Putback) {		// Use the character put
    c = Putback;		// back if there is one
    Putback = 0;
    return(c);
  }

  c = fgetc(Infh);		// Read from input file

  while (Linestart && c == '#') {	// We've hit a pre-processor statement
    Linestart = 0;			// No longer at the start of the line
    scan(&Thistoken);			// Get the line number into l
    if (Thistoken.token != T_NUMLIT)
      fatal("Expecting pre-processor line number, got %s\n", Text);
    l = Thistoken.numval.intval;

    scan(&Thistoken);			// Get the filename in Text
    if (Thistoken.token != T_STRLIT)
      fatal("Expecting pre-processor file name, got %s\n", Text);

    if (Text[0] != '<') {		// If this is a real filename
      if (strcmp(Text, Infilename))	// and not the one we have now
	Infilename = strdup(Text);	// save it. Then update the line num
      Line = l;
    }

    while ((c = fgetc(Infh)) != '\n'); // Skip to the end of the line
    c = fgetc(Infh);			// and get the next character
    Linestart = 1;			// Now back at the start of the line
  }

  Linestart = 0;			// No longer at the start of the line
  if ('\n' == c) {
    Line++;				// Increment line count
    Linestart = 1;			// Now back at the start of the line
  }
  return(c);
}

// Put back an unwanted character
static void putback(int c) {
  Putback = c;
}

// Skip past input that we don't need to deal with, 
// i.e. whitespace, newlines. Return the first
// character we do need to deal with.
static int skip(void) {
  int c;

  c = next();
  while (isspace(c))
    c = next();
  return(c);
}

// Return the position of character c
// in string s, or -1 if c not found
static int chrpos(char *s, int c) {
  int i;
  for (i = 0; s[i] != '\0'; i++)
    if (s[i] == (char) c)
      return (i);
  return (-1);
}

// Read in a hexadecimal constant from the input
static int hexchar(void) {
  int c, h, n = 0, f = 0;

  // Loop getting characters
  while (isxdigit(c = next())) {
    // Convert from char to int value
    h = chrpos("0123456789abcdef", tolower(c));

    // Add to running hex value
    n = n * 16 + h;
    f = 1;
  }

  // We hit a non-hex character, put it back
  putback(c);

  // Flag tells us we never saw any hex characters
  if (!f)
    fatal("missing digits after '\\x'\n");
  if (n > 255)
    fatal("value out of range after '\\x'\n");

  return(n);
}

// Return the next character from a character
// or string literal
static int scanch(void) {
  int i, c, c2;

  // Get the next input character and interpret
  // metacharacters that start with a backslash
  c = next();
  if (c == '\\') {
    switch (c = next()) {
    case 'a':
      return('\a');
    case 'b':
      return('\b');
    case 'f':
      return('\f');
    case 'n':
      return('\n');
    case 'r':
      return('\r');
    case 't':
      return('\t');
    case 'v':
      return('\v');
    case '\\':
      return('\\');
    case '"':
      return('"');
    case '\'':
      return('\'');

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
      for (i = c2 = 0; isdigit(c) && c < '8'; c = next()) {
	if (++i > 3)
	  break;
	c2 = c2 * 8 + (c - '0');
      }

      putback(c);		// Put back the first non-octal char
      return(c2);
    case 'x':
      return(hexchar());
    default:
      fatal("unknown escape sequence %c\n", c);
    }
  }
  return(c);			// Just an ordinary old character!
}

// List of characters that can be found in a numeric literal
char *numchar= "0123456789ABCDEFabcdef.x";

// Scan a numeric literal value from the input file
// and store it in the given token pointer
static void scan_numlit(Token *t, int c, bool is_negative) {
  int i=0;
  bool isfloat=false;
  int radix=10;

  // Assume an unsigned int
  t->numtype= NUM_UINT;

  // Put the first character and negative sign in the buffer
  if (is_negative) {
    Text[i++]= '-';
    t->numtype= NUM_INT;
  }
  Text[i++]= c;

  // Loop while we have enough buffer space
  for (; i < TEXTLEN - 1; i++) {
    c = scanch();

    // Found a non-numeric character
    if (strchr(numchar,c)==NULL) {
      putback(c);
      break;
    }

    // Otherwise add it to the buffer
    Text[i]= c;
  }

  // NUL terminate the string
  Text[i] = '\0';

  // Determine either if it's a float
  // or any octal/hex radix
  if (strchr(Text, '.') != NULL) {
    isfloat= true;
    t->numtype = NUM_FLT;
  } else {
    if (Text[0]=='0') {
      if (Text[1]=='x')
	radix=16;
      else
	radix=8;
    }
  }

  // Do the conversion
  if (isfloat)
    t->numval.dblval = strtod(Text, NULL);
  else
    t->numval.uintval = strtoull(Text, NULL, radix);
}

// Scan in a string literal from the input file,
// and store it in buf[]. Return the length of
// the string. 
static int scanstr(char *buf) {
  int i, c;

  // Loop while we have enough buffer space
  for (i = 0; i < TEXTLEN - 1; i++) {
    // Get the next char and append to buf
    // Return when we hit the ending double quote
    if ((c = scanch()) == '"') {
      buf[i] = 0;
      return(i);
    }
    buf[i] = (char) c;
  }

  // Ran out of buf[] space
  fatal("String literal too long\n");
  return(0);
}

// Scan an identifier from the input file and
// store it in buf[]. Return the identifier's length
static int scanident(int c, char *buf, int lim) {
  int i = 0;

  // Allow digits, alpha and underscores
  while (isalpha(c) || isdigit(c) || '_' == c) {
    // Error if we hit the identifier length limit,
    // else append to buf[] and get next character
    if (lim - 1 == i) {
      fatal("Identifier too long\n");
    } else if (i < lim - 1) {
      buf[i++] = (char) c;
    }
    c = next();
  }

  // We hit a non-valid character, put it back.
  // NUL-terminate the buf[] and return the length
  putback(c);
  buf[i] = '\0';
  return(i);
}


// A structure to hold a keyword, its first letter
// and the token id associated with the keyword
struct keynode {
  char first;
  char *keyword;
  int  token;
};

// List of keywords and matching tokens
static struct keynode keylist[]= {
  { 'N', "NULL",   T_NULL   },
  { 'b', "bool",   T_BOOL   },
  { 'e', "else",   T_ELSE   },
  { 'f', "false",  T_FALSE  },
  { 'f', "flt32",  T_FLT32  },
  { 'f', "flt64",  T_FLT64  },
  { 'f', "for",    T_FOR    },
  { 'i', "if",     T_IF     },
  { 'i', "int8",   T_INT8   },
  { 'i', "int16",  T_INT16  },
  { 'i', "int32",  T_INT32  },
  { 'i', "int64",  T_INT64  },
  { 'r', "return", T_RETURN },
  { 't', "true",   T_TRUE   },
  { 'u', "uint8",  T_UINT8  },
  { 'u', "uint16", T_UINT16 },
  { 'u', "uint32", T_UINT32 },
  { 'u', "uint64", T_UINT64 },
  { 'v', "void",   T_VOID   },
  { 'w', "while",  T_WHILE  },
  { 0,   NULL,     0        }
};

// Given a word from the input, return the matching
// keyword token number or 0 if it's not a keyword.
// Switch on the first letter so that we don't have
// to waste time strcmp()ing against all the keywords.
static int keyword(char *s) {
  int i;

  for (i=0; keylist[i].first != 0; i++) {
    // Too early
    if (keylist[i].first < *s) continue;

    // A match
    if (!strcmp(s, keylist[i].keyword)) return(keylist[i].token);

    // Too late
    if (keylist[i].first > *s) return(0);
  }

  return(0);
}

// Scan and return the next token found in the input.
// Return 1 if token valid, 0 if no tokens left.
int scan(Token * t) {
  int c, tokentype;

  // If we have a lookahead token, return this token
  if (Peektoken.token != 0) {
    t->token = Peektoken.token;
    t->tokstr = Peektoken.tokstr;
    t->numval.intval = Peektoken.numval.intval;
    t->numtype = Peektoken.numtype;
    Peektoken.token = 0;
    return(1);
  }

  // Skip whitespace
  c = skip();

  // Determine the token based on
  // the input character
  switch (c) {
  case EOF:
    t->token = T_EOF;
    return(0);
  case '+':
    t->token = T_PLUS;
    break;
  case '-':
    c= next();

    if (isdigit(c)) {		// Negative numeric literal
      scan_numlit(t, c, true);
      t->token = T_NUMLIT;
    } else {
      putback(c);
      t->token = T_MINUS;
    }
    break;
  case '*':
    t->token = T_STAR;
    break;
  case '/':
    t->token = T_SLASH;
    break;
  case ';':
    t->token = T_SEMI;
    break;
  case '{':
    t->token = T_LBRACE;
    break;
  case '}':
    t->token = T_RBRACE;
    break;
  case '(':
    t->token = T_LPAREN;
    break;
  case ')':
    t->token = T_RPAREN;
    break;
  case '~':
    t->token = T_INVERT;
    break;
  case '^':
    t->token = T_XOR;
    break;
  case ',':
    t->token = T_COMMA;
    break;
  case '.':
    if ((c = next()) == '.') {
      t->token = T_ELLIPSIS;
      if ((c = next()) != '.')
	fatal("Expected '...', only got '..'\n");
      } else {
	putback(c);
        t->token = T_DOT;
      }
      break;
  case '=':
    if ((c = next()) == '=') {
      t->token = T_EQ;
    } else {
      putback(c);
      t->token = T_ASSIGN;
    }
    break;
  case '!':
    if ((c = next()) == '=') {
      t->token = T_NE;
    } else {
      putback(c);
      t->token = T_LOGNOT;
    }
    break;
  case '<':
    if ((c = next()) == '=') {
      t->token = T_LE;
    } else if (c == '<') {
      t->token = T_LSHIFT;
    } else {
      putback(c);
      t->token = T_LT;
    }
    break;
  case '>':
    if ((c = next()) == '=') {
      t->token = T_GE;
    } else if (c == '>') {
      t->token = T_RSHIFT;
    } else {
      putback(c);
      t->token = T_GT;
    }
    break;
  case '&':
    if ((c = next()) == '&') {
      t->token = T_LOGAND;
    } else {
      putback(c);
      t->token = T_AMPER;
    }
    break;
  case '|':
    if ((c = next()) == '|') {
      t->token = T_LOGOR;
    } else {
      putback(c);
      t->token = T_OR;
    }
    break;
  case '\'':
    // If it's a quote, scan in the
    // literal character value and
    // the trailing quote
    t->numval.intval = scanch();
    t->numtype= NUM_CHAR;
    t->token = T_NUMLIT;
    if (next() != '\'')
      fatal("Expected '\\'' at end of char literal\n");
    break;
  case '"':
    // Scan in a literal string
    scanstr(Text);
    t->token = T_STRLIT;
    t->tokstr= strdup(Text);
    break;
  default:
    // If it's a digit, scan the
    // literal integer value in
    if (isdigit(c)) {
      scan_numlit(t,c,false);
      t->token = T_NUMLIT;
      break;
    } else if (isalpha(c) || '_' == c) {
      // Read in a keyword or identifier
      scanident(c, Text, TEXTLEN);

      // If it's a recognised keyword, return that token
      if ((tokentype = keyword(Text)) != 0) {
	t->token = tokentype;
	break;
      }
      // Not a recognised keyword, so it must be an identifier
      t->token = T_IDENT;
      t->tokstr= strdup(Text);
      break;
    }

    // The character isn't part of any recognised token, error
    fatal("Unrecognised character: %c\n", c);
  }

  // We found a token
  return(1);
}

// List of tokens as strings
static char *tokstr[]= {
  "EOF",

  "&", "|", "^",
  "==", "!=", "<", ">", "<=", ">=",
  "<<", ">>",
  "+", "-", "*", "/", "%",

  "=", "~", "!", "&&", "||",

  "void",  "bool",
  "int8",  "int16",  "int32",  "int64",
  "uint8", "uint16", "uint32", "uint64",
  "flt32", "flt64",

  "if", "else", "false", "for",
  "true", "while", "return", "NULL",

  "numlit", "strlit", ";", "ident",
  "{", "}", "(", ")",
  ",", "...", "."
};

char *get_tokenstr(int token) {
  return(tokstr[token]);
}

// Dump the tokens in the input file
void dumptokens(void) {
  Token t;

  while (1) {
    if (scan(&t)==0) return;
    fprintf(Debugfh, "%s", tokstr[t.token]);
    switch(t.token) {
    case T_STRLIT:
      fprintf(Debugfh, " \"%s\"", Text);
      break;
    case T_NUMLIT:
      if (t.numtype==NUM_CHAR) {
        fprintf(Debugfh, " '%c'", (int)t.numval.intval);
	break;
      }
      // fallthrough
    case T_IDENT:
      fprintf(Debugfh, " %s", Text);
    }
    fprintf(Debugfh, "\n");
  }
}

// Ensure that the current token is t,
// and psossibly fetch the next token.
// Otherwise throw an error
void match(int t, bool getnext) {
  if (Thistoken.token != t)
    fatal("Expected %s, got %s\n", tokstr[t], tokstr[Thistoken.token]);

  if (getnext) scan(&Thistoken);
}

// Match a semicolon and fetch the next token
void semi(void) {
  match(T_SEMI,true);
}

// Match a left brace and fetch the next token
void lbrace(void) {
  match(T_LBRACE,true);
}

// Match a right brace and fetch the next token
void rbrace(void) {
  match(T_RBRACE,true);
}

// Match a left parenthesis and fetch the next token
void lparen(void) {
  match(T_LPAREN,true);
}

// Match a right parenthesis and fetch the next token
void rparen(void) {
  match(T_RPAREN,true);
}

// Match an identifer and fetch the next token
void ident(void) {
  match(T_IDENT,true);
}

// Match a comma and fetch the next token
void comma(void) {
  match(T_COMMA,true);
}
