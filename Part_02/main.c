// The front-end for the alic compiler.
// (c) 2025 Warren Toomey, GPL3

#include <stdio.h>
#include <stdlib.h>
#include "alic.h"
#include "proto.h"

extern char *Infilename;
FILE *debugfh = NULL;
FILE *outfh = NULL;

void usage(char *name) {
  fprintf(stderr, "Usage: %s [-D debugfile] [-o outfile] file\n", name);
  exit(1);
}

int main(int argc, char *argv[]) {

  int opt;

  outfh = stdout;

  // Get any flag values
  while ((opt = getopt(argc, argv, "D:o:")) != -1) {
    switch (opt) {
    case 'D':
      debugfh = fopen(optarg, "w");
      if (debugfh == NULL) {
	fprintf(stderr, "Unable to open debug file %s\n", optarg);
	exit(1);
      }
      break;
    case 'o':
      outfh = fopen(optarg, "w");
      if (outfh == NULL) {
	fprintf(stderr, "Unable to open intermediate file %s\n", optarg);
	exit(1);
      }
      break;
    default:
      usage(argv[0]);
    }
  }

  if ((argc - optind) != 1) usage(argv[0]);

  if (freopen(argv[optind], "r", stdin) == NULL) {
    fprintf(stderr, "Unable to open %s\n", argv[optind]);
    exit(1);
  }

  Infilename = argv[optind];

  cg_file_preamble();
  cg_func_preamble();
  if (yyparse()==0) 
    fatal("syntax error\n");
  cg_func_postamble();
  gen_globsyms();

  exit(0);
}
