// This is an example program that shows off some of the features of the
// alic language. It's not supposed to be a good game of tic tac toe!
// (c) 2025 Warren Toomey, GPL3

#include <sys/types.ah>
#include <stdlib.ah>
#include <except.ah>
#include <ctype.ah>

// An opaque type. No details of what's inside but we can take a
// pointer to one. These come from <stdio.ah>, here for demonstration
type FILE;
extern FILE * stdin;
extern FILE * stdout;
extern FILE * stderr;

// Function prototypes, also in <stdio.ah>.
// Note int and flt types have bit sizes
int32 printf(char *fmt, ...);
int8 *fgets(char *ptr, int size, FILE *stream);
int32 fflush(FILE *stream);

// We use Triplets to identify winning moves
// and moves which would result in winning moves
type Triplet = struct {
  int8 a,
  int8 b,
  int8 c
};

// This is the list of winning moves. It's marked const
// so that none of the elements can be changed. Every
// array declaration must be marked with the number of
// elements, as alic does bounds checking
const Triplet Win[8] = {
  {1, 2, 3}, {4, 5, 6}, {7, 8, 9},
  {1, 4, 7}, {2, 5, 8}, {3, 6, 9},
  {1, 5, 9}, {3, 5, 7}
};

// List of winnable moves. If we have the first two
// positions and the third is free, we can win
const Triplet Winnable[24] = {
  {1, 2, 3}, {1, 3, 2}, {2, 3, 1},
  {4, 5, 6}, {4, 6, 5}, {5, 6, 4},
  {7, 8, 9}, {7, 9, 8}, {8, 9, 7},
  {1, 4, 7}, {1, 7, 4}, {7, 4, 1},
  {2, 5, 8}, {2, 8, 5}, {5, 8, 2},
  {3, 6, 9}, {3, 9, 6}, {9, 6, 3},
  {1, 5, 9}, {1, 9, 5}, {9, 5, 1},
  {3, 5, 7}, {3, 7, 5}, {7, 5, 3}
};

// The state of the game is stored in
// this array. We don't use element zero.
// Values are 'X', 'O' or a digit.
int32 Board[10] = { ' ',
  '1', '2', '3',
  '4', '5', '6',
  '7', '8', '9'
};

// Size of text buffers
#define BUFSIZE 100

// Exceptions that we can throw.
// They must not be value zero.
// enums are just to give names to numbers
enum { ENOINPUT=1, ENONUM, EBADNUM };

// Prompt the user for a move and return
// a number between 1 and 9. Throw an
// exception if the user doesn't enter
// a number between 1 and 9.
int8 get_users_move(void) throws Exception *e {
  int8 buffer[BUFSIZE];
  int8 *ptr;

  // Prompt the user
  printf("Your X move (1-9): ");
  fflush(stdout);

  // Get their input. Throw an exception if none
  ptr= fgets(buffer, BUFSIZE - 1, stdin);

  // In alic, 0 and 1 are not true/false. So we cannot
  // say if (!ptr ..., we must do a comparison instead.
  if ((ptr == NULL) || (buffer[0] == '\n')) {

    // Note that we use . not -> to follow struct pointers
    e.errno= ENOINPUT; abort;
  }

  // Not a number. Throw an exception
  if (isdigit(buffer[0]) == 0) {
    e.errno= ENONUM; abort;
  }

  // Not a single digit, or the digit
  // is zero. Throw an exception
  if ((buffer[1] != '\n') || (buffer[0] == '0')) {
    e.errno= EBADNUM; abort;
  }

  // Convert the ASCII digit into a number
  return(buffer[0] - '0');
}

// Given a player's letter, see if the board
// contains a winning combination for them.
// Return true if so, false otherwise
bool is_win(int8 ch) {
  int32 i;

  // sizeof() on an array gives the number of elements
  for (i=0; i< sizeof(Win); i++) {
    if ((Board[ Win[i].a ] == ch) && (Board[ Win[i].b ] == ch) &&
        (Board[ Win[i].c ] == ch))
      return(true);
  }

  return(false);
}

// Given a player's letter, see if there is
// a winning move for that player. Return
// the move if so, 0 otherwise
int8 winning_move(int8 ch) {
  int32 i;
  for (i=0; i< sizeof(Winnable); i++) {
    if ((Board[ Winnable[i].a ] == ch) && (Board[ Winnable[i].b ] == ch) &&
        (isdigit(Board[ Winnable[i].c ]) != 0))
      return(Winnable[i].c);
  }

  return(0);
}

// Find a random position on the board which is free
int8 random_position(void) {
  int8 move;

  // Get a random number, ensure it's in the range 0 .. 8,
  // then add 1, then cast it to be int8 [ would be int32 otherwise ].
  move= 1 + cast( rand() % 9, int8);
  return(move);
}

// Print the board out
void print_board(void) {
  printf("\n %c | %c | %c\n", Board[7], Board[8], Board[9]);
  printf("---+---+---\n");
  printf(" %c | %c | %c\n", Board[4], Board[5], Board[6]);
  printf("---+---+---\n");
  printf(" %c | %c | %c\n\n", Board[1], Board[2], Board[3]);
}


public void main(void) {
  Exception foo;
  int8 move;
  int8 movecount=0;

  // Loop until there is a winner or a draw
  while (true) {
    print_board();

    // Get a user's move, loop until it is valid.
    // bool is a built-in type, true is a keyword
    while(true) {

      // Try to run one or more functions, and
      // catch any exception that they throw
      try(foo) {
        move= get_users_move();
        break;
      } catch {
        // There are no "break"s in switch statements.
 	// You can "fallthru" to the next case if needed.
        switch(foo.errno) {
	  case ENOINPUT: printf("You gave me no input\n");
	  case ENONUM:   printf("That wasn't a number\n");
	  case EBADNUM:  printf("That isn't in the range 1-9\n");
        }
      }
    } 

    // That move is already taken.
    // Again, we cannot do if (!isdigit())
    if (isdigit(Board[move]) == 0) {
      printf("That square is already taken\n");
      continue;
    }

    Board[move]= 'X'; movecount++;
    if (movecount==9) break;

    // See if that's a winning move
    if (is_win('X')) {
      print_board();
      printf("You win! Congratulations.\n");
      exit(0);
    }

    // Now it's our turn. See if we have a winning move
    move= winning_move('O');
    if (move != 0) {
      Board[move]= 'O'; print_board();
      printf("My move is %d. I win.\n", move);
      exit(0);
    }

    // No winning move. Stop them from winning
    move= winning_move('X');
    if (move != 0) {
      Board[move]= 'O';
      printf("My move is %d\n", move);
      movecount++;
      continue;
    }

    // Still going. Choose position
    // 5 or a random position if
    // 5 is not available
    if (isdigit(Board[5]) != 0)
      move= 5;
    else
      move= random_position();
    Board[move]= 'O';
    printf("My move is %d\n", move);

    // See if that's a winning move
    if (is_win('O')) {
      print_board();
      printf("I win.\n");
      exit(0);
    }

    movecount++;
  }

  print_board();
  printf("The game is a draw\n");
  exit(0);
}
