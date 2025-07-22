// animals - guessing game	Authors: Terrence W. Holm & Edwin L. Froese
// This comes from Minix 1.5 and is (c) Prentice-Hall, BSD license.
// Translated into alic by Warren Toomey, 2025.

#include <stddef.ah>
#include <sys/types.ah>
#include <sys/stat.ah>
#include <stdio.ah>
#include <stdlib.ah>
#include <unistd.ah>
#include <ctype.ah>
#include <string.ah>

#define  ANIMALS	"/tmp/animals"
#define  DEFAULT_ANIMAL	"beaver"
#define  MAX_NODES	999	// Enough for 500 animals 
#define  MAX_LINE	90

public int main(int argc, char **argv);
void Read_Animals(char *animal_file);
void Write_Animals(char *animal_file);
bool Ask(char *question);
char *Get_Animal(void);
char *Get_Question(void);
char *A_or_An(char *word);
char *Alloc(int size);
void Abort(int dummy);
void Error(char *message);

type AnimalNode = struct {
  bool question,
  char *text,
  int yes,
  int no
};

AnimalNode animals[MAX_NODES];

int count = 0;

public int main(int argc, char **argv) {
  char *animal_file = ANIMALS;

  if (argc > 2) {
    fprintf(stderr, "Usage:  %s  [ data_base ]\n", argv[0]);
    exit(1);
  }

  if (argc == 2)
    animal_file = argv[1];

  if (access(animal_file, R_OK) == 0)
    Read_Animals(animal_file);
  else {
    animals[0].question = false;
    animals[0].text = DEFAULT_ANIMAL;
    count = 1;
  }

  while (Ask("\nAre you thinking of an animal?")) {
    int i = 0;

    while (true) {
      if (animals[i].question) {
	if (Ask(animals[i].text))
	  i = animals[i].yes;
	else
	  i = animals[i].no;
      } else {
	printf("Were you thinking of %s %s",
	       A_or_An(animals[i].text), animals[i].text);

	if (Ask("?"))
	  printf("I knew it!\n");

	else {
	  // Insert a new question and animal name 

	  if (count + 2 > MAX_NODES)
	    Error("Too many animal names");

	  animals[count].question = false;
	  animals[count].text = animals[i].text;
	  count++;

	  animals[count].question = false;
	  printf("What animal were you thinking of? ");
	  animals[count].text = Get_Animal();
	  count++;

	  animals[i].question = true;
	  printf("What question would distinguish %s %s from\n%s %s? ",
		 A_or_An(animals[count - 2].text), animals[count - 2].text,
		 A_or_An(animals[count - 1].text), animals[count - 1].text);

	  animals[i].text = Get_Question();

	  printf("For %s %s, the answer would be",
		 A_or_An(animals[count - 1].text), animals[count - 1].text);

	  if (Ask("?")) {
	    animals[i].yes = count - 1;
	    animals[i].no = count - 2;
	  } else {
	    animals[i].yes = count - 2;
	    animals[i].no = count - 1;
	  }
	}
	break;
      }
    }				// End while ( 1 ) 
  }

  printf("\nThank you for playing \"animals\".\n");
  printf("The animal data base is now being updated.\n");
  Write_Animals(animal_file);
  printf("\nBye.\n");
  return (0);
}

//  Reading and writing the animal data base
void Read_Animals(char *animal_file) {
  FILE *f;
  char buffer[MAX_LINE];

  f = fopen(animal_file, "r");
  if (f == NULL)
    Error("Can not open animal data base");

  while (fgets(buffer, MAX_LINE, f) != NULL) {
    int string_length;
    char *str;

    buffer[strlen(buffer) - 1] = '\0';

    if (buffer[0] == 'q') {
      char *end = strchr(buffer, '?');
      string_length = cast(end - buffer, int32);
      animals[count].question = true;
      sscanf(end + 1, "%d:%d", &animals[count].yes, &animals[count].no);
    } else {
      animals[count].question = false;
      string_length = cast(strlen(buffer) - 1, int32);
    }

    str = Alloc(string_length + 1);
    str[0] = '\0';
    strncat(str, buffer + 1, string_length);
    animals[count].text = str;
    count++;
  }
  fclose(f);
}


void Write_Animals(char *animal_file) {
  FILE *f;
  int i;
  char buffer[MAX_LINE];

  f = fopen(animal_file, "w");
  if (f == NULL)
    Error("Can not write animal data base");

  for (i = 0; i < count; i++) {
    if (animals[i].question)
      sprintf(buffer, "q%s%d:%d", animals[i].text,
	      animals[i].yes, animals[i].no);
    else
      sprintf(buffer, "a%s", animals[i].text);

    fprintf(f, "%s\n", buffer);
  }
  fclose(f);
  chmod(animal_file, 0666);
}


//  Reading data from the user
bool Ask(char *question) {
  char buf[MAX_LINE];
  int response;

  printf("%s ", question);

  while (true) {
    if (fgets(buf, MAX_LINE, stdin) == NULL)
      Abort(1);
    response = buf[0];
    if (response == '\r' || response == '\n')
      continue;
    if (response == 'y' || response == 'n')
      break;
    printf("\n%s [yn]?", question);
  }

  if (response == 'y')
    return (true);
  else
    return (false);
}


char *Get_Animal(void) {
  char s[MAX_LINE];
  char *text;
  int text_length;

  fgets(s, MAX_LINE, stdin);
  text_length = cast(strlen(s), int32);
  text = Alloc(text_length);
  text[0] = '\0';
  strncat(text, s, text_length - 1);
  return (text);
}


char *Get_Question(void) {
  char s[MAX_LINE];
  char *end;
  char *text;

  fgets(s, MAX_LINE, stdin);

  // Capitalize the first letter 
  if (islower(s[0]) != 0)
    s[0] = cast(toupper(s[0]), int8);

  // Make sure the question ends with a '?' 
  end= strchr(s, '?');
  if (end == NULL)
    s[strlen(s) - 1] = '?';
  else
    end[1] = '\0';

  text = Alloc(cast(strlen(s) + 1, int32));
  strcpy(text, s);
  return (text);
}

//  Utility routines
char *A_or_An(char *word) {
  if (strchr("aeiouAEIOU", word[0]) == NULL)
    return ("a");
  else
    return ("an");
}

char *Alloc(int size) {
  char *memory;

  memory = malloc(size);
  if (memory == NULL)
    Error("No room in memory for all the animals");
  return (memory);
}

void Abort(int dummy) {
  printf("\nThank you for playing \"animals\".\n");
  printf("Since you aborted, the animal data base will not be updated.\n");
  sleep(1);
  printf("\nBye.\n");
  exit(1);
}

void Error(char *message) {
  fprintf(stderr, "Error: %s\n", message);
  exit(1);
}
