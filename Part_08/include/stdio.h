#ifndef _STDIO_H_
# define _STDIO_H_

#include <sys/types.h>
#include <stddef.h>

type FILE;

void printf(...);
void fprintf(...);
FILE *fopen(char *fmt, char *mode);
size_t fwrite(char *ptr, size_t size, size_t nmemb, FILE *stream);
size_t  fread(char *ptr, size_t size, size_t nmemb, FILE *stream);
char *fgets(char *ptr, int size, FILE *stream);
int fclose(FILE *stream);

#endif
