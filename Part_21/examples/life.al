#include <stdio.ah>
#include <stdlib.ah>
#include <unistd.ah>
#include <curses.ah>

// The Game of Life with ncurses code borrowed from
// https://github.com/cpressey/ncurses_programs/blob/master/JustForFun/life.c
// (c) 2025 Warren Toomey, GPL3

// TIME_OUT is in milliseconds
enum { DEAD=0, ALIVE=1, TIME_OUT=100 };

// We keep two 2D arrays, one for the current state of the game
// and one for the next state of the game. We calloc() them later
// as we don't know the terminals dimensions at compile-time.
int **currstate;
int **nextstate;

// Display the current state of the game
void display(WINDOW * win, int **state) {
  int x;
  int y;

  wclear(win);
  foreach y (0 ... LINES - 2)
    foreach x (0 ... COLS - 2)
      if (state[x][y] == ALIVE)
	mvwaddch(win, y, x, '@');
  wrefresh(win);
}

public int main(void) {
  int **board1;
  int **board2;
  int **temp;
  int i;
  int x;
  int y;
  int neighbours;

  // Set up the terminal
  initscr();
  cbreak();
  timeout(TIME_OUT);
  keypad(stdscr, 1);
  curs_set(0);

  // Use the dimensions of the terminal to make the two 2D arrays
  board1 = calloc(COLS, sizeof(int *));
  for (i = 0; i < COLS; i++) board1[i] = calloc(LINES, sizeof(int));
  board2 = calloc(COLS, sizeof(int *));
  for (i = 0; i < COLS; i++) board2[i] = calloc(LINES, sizeof(int));

  // Set up the current and next board states
  currstate = board1;
  nextstate = board2;

  // Put an r-pentonimo on the current board
  x = COLS / 2;
  y = LINES / 2;
  currstate[x][y]         = ALIVE;
  currstate[x - 1][y]     = ALIVE;
  currstate[x][y - 1]     = ALIVE;
  currstate[x][y + 1]     = ALIVE;
  currstate[x + 1][y + 1] = ALIVE;

  // Loop forever with a built-in TIME_OUT delay
  while (getch() != KEY_F(1)) {
    // Display the current state
    display(stdscr, currstate);

    // Update the nextstate board using the Game of Life
    // rules applied to the current board
    foreach x (1 ... COLS - 2) {
      foreach y (1 ... LINES - 2) {

	// Calculate the neighbours of the x/y cell
	neighbours = currstate[x - 1][y - 1] + currstate[x][y - 1]
		   + currstate[x + 1][y - 1] + currstate[x - 1][y]
		   + currstate[x + 1][y]     + currstate[x - 1][y + 1]
		   + currstate[x][y + 1]     + currstate[x + 1][y + 1];

	// Apply the rules for live cells
	if (currstate[x][y] == ALIVE) {
	  if (neighbours < 2 || neighbours > 3)
	    nextstate[x][y] = DEAD;
	  else
	    nextstate[x][y] = ALIVE;
	} else {
	  // and the rule for a dead cell
	  if (neighbours != 3)
	    nextstate[x][y] = DEAD;
	  else
	    nextstate[x][y] = ALIVE;
	}
      }
    }

    // Swap the current and next states
    temp = nextstate;
    nextstate = currstate;
    currstate = temp;
  }

  return (0);
}
