// The front-end for the alic compiler.
// (c) 2019, 2025 Warren Toomey, GPL3

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "alic.h"
#include "incdir.h"
#include "proto.h"

// Commands and default filenames
#define AOUT "a.out"
#define ASCMD "as -g -o "
#define QBECMD "qbe -o "
#define LDCMD "cc -g -no-pie -o "
#define CPPCMD "cpp -nostdinc -isystem "

// Global variables
char *Infilename;		// Name of file we are parsing
FILE *Infh;			// The input file
FILE *Outfh;			// The output file
FILE *Debugfh = NULL;		// The debugging file
int Line = 1;			// Current line number
bool O_dumptokens = false;	// Dump the input file's tokens
bool O_dumpsyms = false;	// Dump the symbol table
bool O_dumpast = false;		// Dump each function's AST tree
bool O_logmisc = false;		// Log miscellaneous things
bool O_boundscheck = true;	// Do array bounds checking
bool O_verbose = false;		// Describe the compiler's steps
bool O_dolink = true;		// Link to produce an executable
bool O_keepasm = false;		// Keep the intermediate QBE & asm code
bool O_assemble = false;	// Assemble the assembly code to .o

// Local variables
static char *Outfilename;	// Name of our output file

// Given a string with a '.' and at least a 1-character suffix
// after the '.', change the suffix to be the given character.
// Return the new string or NULL if the original string could
// not be modified
char *alter_suffix(char *str, char suffix) {
  char *posn;
  char *newstr;

  // Clone the string
  if ((newstr = strdup(str)) == NULL)
    return (NULL);

  // Find the '.'
  if ((posn = strrchr(newstr, '.')) == NULL)
    return (NULL);

  // Ensure there is a suffix
  posn++;
  if (*posn == '\0')
    return (NULL);

  // Change the suffix and NUL-terminate the string
  *posn = suffix;
  posn++;
  *posn = '\0';
  return (newstr);
}

// Given an input filename, compile that file
// down to assembly code. Return the new file's name
static char *do_compile(char *filename) {
  char cmd[TEXTLEN];

  // Change the input file's suffix to .q
  Outfilename = alter_suffix(filename, 'q');
  if (Outfilename == NULL) {
    fprintf(stderr, "Error: %s has no suffix, try .al on the end\n", filename);
    exit(1);
  }

  // Generate the pre-processor command
  snprintf(cmd, TEXTLEN, "%s %s %s", CPPCMD, INCDIR, filename);

  // Open up the pre-processor pipe
  if ((Infh = popen(cmd, "r")) == NULL) {
    fprintf(stderr, "Unable to open %s: %s\n", filename, strerror(errno));
    exit(1);
  }

  Infilename = filename;

  // Create the output file
  if ((Outfh = fopen(Outfilename, "w")) == NULL) {
    fprintf(stderr, "Unable to create %s: %s\n", Outfilename,
	    strerror(errno));
    exit(1);
  }

  // Reset the symbol table and the list of types
  init_symtable();
  init_typelist();

  Line = 1;			// Reset the scanner
  Linestart = 1;
  Putback = 0;

  Peektoken.token = 0;		// Set there is no lookahead token
  scan(&Thistoken);		// Get the first token from the input

  // Dump the tokens and re-open the input file
  if (O_dumptokens) {
    dumptokens();
    fclose(Infh);
    Infh = popen(cmd, "r");
    Line = 1;
    Linestart = 1;
    Putback = 0;
    Peektoken.token = 0;
    scan(&Thistoken);
  }

  if (O_verbose)
    fprintf(stderr, "compiling %s\n", filename);

  gen_file_preamble();		// Generate the output file preamble
  input_file();			// Parse the input file
  gen_strlits();		// Output any string literals
  fclose(Outfh);		// Close the output file

  if (O_dumpsyms)
    dumpsyms();

  return (Outfilename);
}

// Given an input filename, run QBE on the file and
// produce an assembly file. Return the object filename
char *do_qbe(char *filename) {
  char cmd[TEXTLEN];
  int err;

  char *outfilename = alter_suffix(filename, 's');
  if (outfilename == NULL) {
    fprintf(stderr, "Error: %s has no suffix, try .q on the end\n", filename);
    exit(1);
  }

  // Build the QBE command and run it
  snprintf(cmd, TEXTLEN, "%s %s %s", QBECMD, outfilename, filename);
  if (O_verbose)
    fprintf(stderr, "%s\n", cmd);
  err = system(cmd);

  if (err != 0) {
    fprintf(stderr, "QBE translation of %s failed\n", filename);
    exit(1);
  }

  return (outfilename);
}

// Given an input filename, assemble that file
// down to object code. Return the object filename
char *do_assemble(char *filename) {
  char cmd[TEXTLEN];
  int err;

  char *outfilename = alter_suffix(filename, 'o');
  if (outfilename == NULL) {
    fprintf(stderr, "Error: %s has no suffix, try .s on the end\n", filename);
    exit(1);
  }

  // Build the assembly command and run it
  snprintf(cmd, TEXTLEN, "%s %s %s", ASCMD, outfilename, filename);
  if (O_verbose)
    fprintf(stderr, "%s\n", cmd);

  err = system(cmd);
  if (err != 0) {
    fprintf(stderr, "Assembly of %s failed\n", filename);
    exit(1);
  }

  return (outfilename);
}

// Given a list of object files and an output filename,
// link all of the object filenames together.
void do_link(char *outfilename, char **objlist) {
  int cnt, size = TEXTLEN;
  char cmd[TEXTLEN], *cptr;
  int err;

  // Start with the linker command and the output file
  cptr = cmd;
  cnt = snprintf(cptr, size, "%s %s ", LDCMD, outfilename);
  cptr += cnt;
  size -= cnt;

  // Now append each object file
  while (*objlist != NULL) {
    cnt = snprintf(cptr, size, "%s ", *objlist);
    cptr += cnt;
    size -= cnt;
    objlist++;
  }

  if (O_verbose)
    fprintf(stderr, "%s\n", cmd);
  err = system(cmd);
  if (err != 0) {
    fprintf(stderr, "Linking failed\n");
    exit(1);
  }
}

// Print out a usage if started incorrectly
static void usage(char *prog) {
  fprintf(stderr, "Usage: %s [-vcSB] [-o outfile] ", prog);
  fprintf(stderr, "[-D debugfile] [-L logflags] file [file ...]\n");
  fprintf(stderr,
	  "       -v give verbose output of the compilation stages\n");
  fprintf(stderr, "       -c generate object files but don't link them\n");
  fprintf(stderr, "       -S generate assembly files but don't link them\n");
  fprintf(stderr, "       -B disable array bounds checking\n");
  fprintf(stderr, "       -o outfile, produce the outfile executable file\n");
  fprintf(stderr, "       -D debugfile, write debug info to this file\n");
  fprintf(stderr, "       -L logflags, set the log flags for debugging:\n");
  fprintf(stderr, "          one or more of tok,sym,ast,misc\n");
  fprintf(stderr, "          comma separated\n");
  exit(1);
}

// Main program: check arguments and print a usage
// if we don't have an argument.
// Then do the compilation actions.
#define MAXOBJ 100

int main(int argc, char *argv[]) {
  char *outfilename = AOUT;
  char *qbefile, *asmfile, *objfile;
  char *objlist[MAXOBJ];
  int i, objcnt = 0;
  int opt;

  // Get any flag values
  while ((opt = getopt(argc, argv, "vcSBD:L:o:")) != -1) {
    switch (opt) {
    case 'c':
      O_assemble = true;
      O_keepasm = false;
      O_dolink = false;
      break;
    case 'D':
      Debugfh = fopen(optarg, "w");
      if (Debugfh == NULL) {
	fprintf(stderr, "Unable to open debug file %s\n", optarg);
	exit(1);
      }
      break;
    case 'L':
      if (strstr(optarg, "tok"))
	O_dumptokens = true;
      if (strstr(optarg, "sym"))
	O_dumpsyms = true;
      if (strstr(optarg, "ast"))
	O_dumpast = true;
      if (strstr(optarg, "misc"))
	O_logmisc = true;
      break;
    case 'S':
      O_keepasm = true;
      O_assemble = false;
      O_dolink = false;
      break;
    case 'B':
      O_boundscheck = false;
      break;
    case 'o':
      outfilename = strdup(optarg);	// Get the output filename
      break;
    case 'v':
      O_verbose = true;
      break;
    default:
      usage(argv[0]);
    }
  }

  if ((O_dumptokens || O_dumpsyms || O_dumpast) && Debugfh == NULL) {
    fprintf(stderr, "-L used with no -D debug file\n");
    exit(1);
  }

  // Ensure we have at least one input file argument
  if ((argc - optind) != 1)
    usage(argv[0]);

  // Work on each input file in turn
  while (optind < argc) {
    qbefile = do_compile(argv[optind]);	// Compile the source file
    asmfile = do_qbe(qbefile);	// Create the assembly file

    if (O_dolink || O_assemble) {
      objfile = do_assemble(asmfile);	// Assemble it to object form
      if (objcnt == (MAXOBJ - 2)) {
	fprintf(stderr, "Too many object files for the compiler\n");
	exit(1);
      }

      objlist[objcnt++] = objfile;	// Add the object file's name
      objlist[objcnt] = NULL;	// to the list of object files
    }

    if (!O_keepasm) {		// Remove the QBE and assembly files
      unlink(qbefile);		// if we don't need to keep them
      unlink(asmfile);
    }
    optind++;
  }

  // Now link all the object files together
  if (O_dolink) {
    do_link(outfilename, objlist);

    // If we don't need to keep the object
    // files, then remove them
    if (!O_assemble) {
      for (i = 0; objlist[i] != NULL; i++)
	unlink(objlist[i]);
    }
  }

  exit(0);
}
