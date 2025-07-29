// Associative array support functions for the alic language.
// (c) 2025, Warren Toomey. GPL3

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

#undef DEBUG

typedef struct AL_AA3 AL_AA3;
typedef struct AL_AA2 AL_AA2;
typedef struct AL_AA1 AL_AA1;

// We have three data structure levels.
// Level three is a linked list of
// key/value pairs.
struct AL_AA3 {
  uint64_t key;			// The key
  uint64_t value;		// The value
  AL_AA3 *next;			// The next key/value pair in the list
};

// Level two is an array of level three pointers
#define AL_AABITSIZE 10
#define AL_AASIZE (2 << AL_AABITSIZE)	// Array size, must be power of two
#define AL_AAMASK (AL_AASIZE - 1)	// Mask to get index value

struct AL_AA2 {
  AL_AA3 *next[AL_AASIZE];
};

// Level one is an array of level two pointers
// plus state to hold any iteration progress
struct AL_AA1 {
  AL_AA2 *next[AL_AASIZE];

  uint16_t idx1;
  uint16_t idx2;
  AL_AA3 *inext;
};

// The djb2 hash function comes from
// http://www.cse.yorku.ca/~oz/hash.html
// No copyright is given for it.
//
// Given a pointer to a string, or NULL,
// return a 64-bit hash value for it.
uint64_t aa_djb2hash(uint8_t * str) {
  uint64_t hash = 5381;
  uint8_t c;

  if (str == NULL)
    return (0);

  while ((c = *str++) != 0)
    hash = ((hash << 5) + hash) + c;

  return (hash);
}

// Create a new associative array and return a pointer to it.
// Return NULL if it cannot be made.
AL_AA1 *al_new_aarray(void) {
  AL_AA1 *ary;

  ary = (AL_AA1 *) calloc(1, sizeof(AL_AA1));
#ifdef DEBUG
  printf("New aarray %p\n", ary);
#endif
  return (ary);
}

// Given an associative array pointer, a key and a value,
// add/relace this key/value in the array.
// Return false if the key/value could not be added/replaced.
bool al_add_aakeyval(AL_AA1 * ary, uint64_t key, int64_t value) {
  uint16_t idx1;
  uint16_t idx2;
  AL_AA2 *level2;
  AL_AA3 *this;

#ifdef DEBUG
  printf("Adding to aarray %p key %ld value %ld\n", ary, key, value);
#endif

  // We need an array!
  if (ary == NULL)
    return (false);

  // Get the level 1 and level 2 indices
  idx1 = key & AL_AAMASK;
  idx2 = (key >> AL_AABITSIZE) & AL_AAMASK;

  // If we don't have a level 2 yet, make it
  if (ary->next[idx1] == NULL) {
    ary->next[idx1] = (AL_AA2 *) calloc(1, sizeof(AL_AA2));
    if (ary->next[idx1] == NULL)
      return (false);
  }

  // Move down to level 2
  level2 = ary->next[idx1];

  // Walk the linked list at level 2 to find
  // if the key is already there
  for (this = level2->next[idx2]; this != NULL; this = this->next)
    if (this->key == key) break;

  // We already have this key, replace the value
  if (this != NULL) {
    this->value = value;
  } else {
    // That key doesn't exist. Create it, add the
    // value and link it into the list
    this = (AL_AA3 *) calloc(1, sizeof(AL_AA3));
    if (this == NULL)
      return (false);
    this->key = key;
    this->value = value;
    this->next = level2->next[idx2];
    level2->next[idx2] = this;
  }

  return (true);
}

// Given an associative array pointer and a key, return
// the associated value or 0 if the key is missing.
int64_t al_get_aavalue(AL_AA1 * ary, uint64_t key) {
  uint16_t idx1;
  uint16_t idx2;
  AL_AA2 *level2;
  AL_AA3 *this;

#ifdef DEBUG
  printf("Getting value from aarray %p key %ld\n", ary, key);
#endif

  // We need an array!
  if (ary == NULL)
    return (0);

  // Get the level 1 and level 2 indices
  idx1 = key & AL_AAMASK;
  idx2 = (key >> AL_AABITSIZE) & AL_AAMASK;

  // If we don't have a level 2 yet, return 0
  if (ary->next[idx1] == NULL)
    return (0);

  // Move down to level 2
  level2 = ary->next[idx1];

  // Walk the linked list at level 2 to find
  // the key and return the value
  for (this = level2->next[idx2]; this != NULL; this = this->next)
    if (this->key == key) {
#ifdef DEBUG
      printf("Returning value %ld\n", this->value);
#endif
      return (this->value);
    }

  // No key
  return (0);
}

// Given an associative array pointer and a key, return
// true if key exists or false if the key is missing.
bool al_exists_aakey(AL_AA1 * ary, uint64_t key) {
  uint16_t idx1;
  uint16_t idx2;
  AL_AA2 *level2;
  AL_AA3 *this;

#ifdef DEBUG
  printf("Exists in aarray %p key %ld\n", ary, key);
#endif

  // We need an array!
  if (ary == NULL)
    return (false);

  // Get the level 1 and level 2 indices
  idx1 = key & AL_AAMASK;
  idx2 = (key >> AL_AABITSIZE) & AL_AAMASK;

  // If we don't have a level 2 yet, return false
  if (ary->next[idx1] == NULL)
    return (false);

  // Move down to level 2
  level2 = ary->next[idx1];

  // Walk the linked list at level 2 to find
  // the key and return the value
  for (this = level2->next[idx2]; this != NULL; this = this->next)
    if (this->key == key) {
#ifdef DEBUG
      printf("Returning true\n");
#endif
      return (true);
    }

  // No key
  return (false);
}

// Given an associative array pointer and a key, delete the key entry
bool al_del_aakey(AL_AA1 * ary, uint64_t key) {
  uint16_t idx1;
  uint16_t idx2;
  AL_AA2 *level2;
  AL_AA3 *this;
  AL_AA3 *last;

#ifdef DEBUG
  printf("Deleting in aarray %p key %ld\n", ary, key);
#endif

  // We need an array!
  if (ary == NULL)
    return (false);

  // Get the level 1 and level 2 indices
  idx1 = key & AL_AAMASK;
  idx2 = (key >> AL_AABITSIZE) & AL_AAMASK;

  // If we don't have a level 2 yet, return false
  if (ary->next[idx1] == NULL)
    return (false);

  // Move down to level 2
  level2 = ary->next[idx1];

  // Walk the linked list at level 2 to find the key
  for (last = this = level2->next[idx2]; this != NULL;
       last = this, this = this->next) {
    if (this->key == key) {
      // Deal with the key at the head of the list
      if (level2->next[idx2] == last) {
	level2->next[idx2] = this->next;
      }

      last->next = this->next;
      free(this);
      return (true);
    }
  }

  // No key
  return (false);
}

// Set the associative array up
// so that we can iterate over it.
// Return a pointer to the first value or NULL.
uint64_t * al_aa_iterstart(AL_AA1 * ary) {
  uint16_t idx1;
  uint16_t idx2;
  AL_AA2 *level2;
  AL_AA3 *this;

  // We need an array!
  if (ary == NULL)
    return(NULL);

  // Find the first entry in the array
  for (idx1 = 0; idx1 < AL_AASIZE; idx1++) {
    if (ary->next[idx1] != NULL) {
      level2 = ary->next[idx1];
      for (idx2 = 0; idx2 < AL_AASIZE; idx2++) {
	if (level2->next[idx2] != NULL) {
	  ary->idx1 = idx1;
	  ary->idx2 = idx2;
          this = level2->next[idx2];
          ary->inext = this->next;
          return (&(this->value));
	}
      }
    }
  }

  // The array is empty!
  ary->idx1 = AL_AAMASK;
  ary->idx2 = AL_AAMASK;
  ary->inext = NULL;
  return(NULL);
}

// Given an associative array pointer,
// return a pointer to the next value in the array.
// Return NULL when there are no entries left.
uint64_t *al_getnext_aavalue(AL_AA1 * ary) {
  uint16_t idx1;
  uint16_t idx2;
  AL_AA2 *level2;
  AL_AA3 *this;

  // We need an array!
  if (ary == NULL)
    return (NULL);

  // No entries left
  if ((ary->idx1 == 255) && (ary->idx2 == 255) && (ary->inext == NULL))
    return (NULL);

  // inext points at the next entry
  if (ary->inext != NULL) {
    this = ary->inext;
    ary->inext = ary->inext->next;
    return (&(this->value));
  }

  // We now need to search for the next entry
  idx1 = ary->idx1;
  idx2 = ary->idx2;
  level2 = ary->next[idx1];
  while (1) {
    // Move up to the next level 2 position
    idx2++;
    if (idx2 == AL_AASIZE) {
      idx2 = 0;
      idx1++;
      if (idx1 == AL_AASIZE) break;
      level2 = ary->next[idx1];
    }

    // We have an entry
    if ((level2 != NULL) && (level2->next[idx2] != NULL)) {
      ary->idx1 = idx1;
      ary->idx2 = idx2;
      this = level2->next[idx2];
      ary->inext = this->next;
      return (&(this->value));
    }
  }

  return (NULL);
}

// Given an associative array pointer,
// free all memory associated with it
void al_free_aarray(AL_AA1 * ary) {
  uint16_t idx1;
  uint16_t idx2;
  AL_AA2 *level2;
  AL_AA3 *this;
  AL_AA3 *next;

  // We need an array!
  if (ary == NULL)
    return;

  // Walk the array, one level at a time
  for (idx1 = 0; idx1 < AL_AASIZE; idx1++) {
    if (ary->next[idx1] == NULL) continue;
    level2 = ary->next[idx1];
    for (idx2 = 0; idx2 < AL_AASIZE; idx2++) {
      if (level2->next[idx2] != NULL) continue;
      for (this = level2->next[idx2]; this != NULL; this = next) {
	next = this->next;
	free(this);
      }
      free(level2->next[idx2]);
    }
    free(ary->next[idx1]);
  }
  free(ary);
}
