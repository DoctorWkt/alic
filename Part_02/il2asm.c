// Convert IL code into assembly code.
// (c) 2019, 2025 Warren Toomey, GPL3

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "alic.h"

void il2qbe(void);

#define TEXTLEN 512

// Commands
#define QBECMD "qbe -o "

FILE *Infh;			// The input file
FILE *Outfh;			// The output file

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
  sprintf(cmd, "%s %s %s", QBECMD, outfilename, filename);
  err = system(cmd);

  if (err != 0) {
    fprintf(stderr, "QBE translation of %s failed\n", filename);
    exit(1);
  }

  return (outfilename);
}

int main(int argc, char **argv) {
  char *outfilename;

  if (argc != 2) {
    fprintf(stderr, "Usage: %s file.i\n", argv[0]); exit(1);
  }

  if ((Infh = fopen(argv[1], "r")) == NULL) {
    fprintf(stderr, "Unable to open %s: %s\n", argv[1], 
            strerror(errno));
    exit(1);
  }

  outfilename = alter_suffix(argv[1], 'q');
  if (outfilename == NULL) {
    fprintf(stderr, "Error: %s has no suffix, try .i on the end\n", argv[1]);
    exit(1);
  }

  if ((Outfh = fopen(outfilename, "w")) == NULL) {
    fprintf(stderr, "Unable to create %s: %s\n", outfilename, 
            strerror(errno));
    exit(1);
  }

  il2qbe();
  fclose(Outfh);
  do_qbe(outfilename);
  // unlink(outfilename);

  exit(0);

}
