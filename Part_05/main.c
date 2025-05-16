// The front-end for the alic compiler.
// (c) 2025 Warren Toomey, GPL3

#include <stdio.h>
#include <stdlib.h>
#undef extern_
#include "alic.h"
#include "proto.h"

char *Infilename;		// Name of file we are parsing
FILE *Infh;			// The input file
FILE *Outfh;			// The output file
FILE *Debugfh=NULL;		// The debugging file
int  Line=1;			// Current line number
bool O_dumptokens=false;	// Dump the input file's tokens
bool O_dumpsyms=false;		// Dump the symbol table
bool O_dumpast=false;		// Dump each function's AST tree

void usage(char *name) {
  fprintf(stderr, "Usage: %s [-D debugfile] [ -L flags] [-o outfile] file\n",
								name);
  fprintf(stderr, "   flags are one or more of tok,sym,ast, comma separated\n");
  exit(1);
}

int main(int argc, char *argv[]) {
  int opt;

  Symhead= NULL;
  Outfh = stdout;

  // Get any flag values
  while ((opt = getopt(argc, argv, "D:L:o:")) != -1) {
    switch (opt) {
    case 'D':
      Debugfh = fopen(optarg, "w");
      if (Debugfh == NULL) {
	fprintf(stderr, "Unable to open debug file %s\n", optarg);
	exit(1);
      }
      break;
    case 'L':
      if (strstr(optarg, "tok")) O_dumptokens=true;
      if (strstr(optarg, "sym")) O_dumpsyms=true;
      if (strstr(optarg, "ast")) O_dumpast=true;
      break;
    case 'o':
      Outfh = fopen(optarg, "w");
      if (Outfh == NULL) {
	fprintf(stderr, "Unable to open intermediate file %s\n", optarg);
	exit(1);
      }
      break;
    default:
      usage(argv[0]);
    }
  }

  if ((argc - optind) != 1) usage(argv[0]);

  Infilename = argv[optind];
  Infh= fopen(Infilename, "r");
  if (Infh == NULL) {
    fprintf(stderr, "Unable to open %s\n", Infilename);
    exit(1);
  }

  if ((O_dumptokens || O_dumpsyms || O_dumpast) && Debugfh==NULL) {
    fprintf(stderr, "-L used with no -D debug file\n");
    exit(1);
  }

  // Dump the tokens and re-open the input file
  if (O_dumptokens) {
    dumptokens();
    fclose(Infh);
    Infh= fopen(Infilename, "r");
  }

  scan(&Thistoken);		// Get the first token from the input
  Peektoken.token = 0;		// and set there is no lookahead token
  cg_file_preamble();
  parse_file();			// Parse the input file
  gen_strlits();

  if (O_dumpsyms)
   dumpsyms();

  exit(0);
}
