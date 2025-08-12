// regex library
// (c) 2019 Mariano Filippa, no copyright license provided
// See https://github.com/mfilippa/regex_lib

#ifndef _REGEX_LIB_H
#define _REGEX_LIB_H
  
#include <regex.h>
  
// max string size
#define REGEX_MAX_STR_SIZE 4096
  
// max of parenthesized substrings (groups) 
// e.g. '(\d+?)_(\d+?)_(\d+?)' has 3
#define REGEX_MAX_GROUPS  10
  
// regex compile flags, OR all needed
// REG_EXTENDED: use extended >> use ? + { | } ( )
// REG_ICASE: ignore case
// REG_NOSUB: report only success/fail in search
// REG_NEWLINE: newline \n divides string into multiple lines
#define REGEX_COMPILE_FLAGS (REG_EXTENDED|REG_ICASE|REG_NEWLINE)
  
// errors
typedef enum regex_err_enum { REGEX_ERR_NONE =
    0, REGEX_ERR_NOT_FOUND, REGEX_ERR_BAD_REGEX, REGEX_ERR_MAX_STR_LEN,
    REGEX_ERR_MAX_GROUP, REGEX_ERR_BAD_ARG, REGEX_ERR_GET_ERR,
    REGEX_ERR_COUNT, 
} regex_err_t;
 
// search regular expression
// - source[REGEX_MAX_STR_SIZE]: source string to perform search on
// - regex_search[REGEX_MAX_STR_SIZE]: regex string to search
// - start[REGEX_MAX_GROUPS+1]: returns start index of each subgroup
// - len[REGEX_MAX_GROUPS+1]: returns length of each subgroup
// - nsub: returns nr of subgroups found in search
// index 0 stores full string match
// index 1+ stores all substrings found
// returns: error (see regex_err_t)
  regex_err_t regex_search(const char *source, const char *regex_search,
			   unsigned int *start, unsigned int *len,
			   unsigned int *nsub);
 
// extract substring
// - dest[REGEX_MAX_STR_SIZE]: destination string to extract to
// - source[REGEX_MAX_STR_SIZE]: source string to extract substring
// - start: start index from source
// - len: length of chars to extract
// adds a NULL termination to string
// returns: error (see regex_err_t)
  regex_err_t regex_extract(char *dest, const char *source,
			    unsigned int start, unsigned int len);
 
// replace text
// - dest[REGEX_MAX_STR_SIZE]: destination to store replaced string
// - source[REGEX_MAX_STR_SIZE]: source string to perform replacement
// - regex_search[REGEX_MAX_STR_SIZE]: regex string to search
// - rpl_str[REGEX_MAX_STR_SIZE]: regex string to replace
// returns: error (see regex_err_t)
  regex_err_t regex_replace(const char *dest, const char *source,
			    const char *srch_regex, const char *rplc_regex);
 
// get error message
// - err_code: error code (see regex_err_t)
// returns: error string [REGEX_MAX_STR_SIZE]
char *regex_error_msg(regex_err_t err_code);
 
#endif // _REGEX_LIB_H
