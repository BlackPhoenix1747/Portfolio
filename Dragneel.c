/* INCLUDES/LIBRARIES */
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>

/* DEFINES */
#define DRAG_VERSION "0.0.1" 

/* DATA */
struct editorConfig {
  int screenrows;
  int screencols;
  struct termios orig_termios;
};

struct editorConfig E; 

/* Error Handling Funtion */
void die(const char *s) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  perror(s);
  exit(1);
}

/* Disable Raw Mode Funtion */
void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) die("tcsetattr");
}

/* Enable Raw Mode Function */
void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
  atexit(disableRawMode);
  
  struct termios raw = E.orig_termios;
  raw.c_iflag &= ~(ICRNL | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw)== -1) die("tcsetattr");
}

/* Keypresses */
char editorReadKey() {
  int nread;
  char c;
  while((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno !=EAGAIN) die("read");
  }
  return c;
}

/* Get Cursor Position Function */
int getCursorPosition(int *rows, int *cols) {
  char buf[32];
  unsigned int i = 0;

  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1 ) break;
    if (buf[i] == 'R') break;
    i++;
  }
  buf[i] = '\0';
  if (buf[0] != '\x1b' || buf[1] != '[') return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

  return 0;
}

/* Get Window Size Function */
int getWindowSize(int *rows, int *cols) {
  struct winsize ws;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
    editorReadKey();
    return getCursorPosition(rows, cols);
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/*** append buffer ***/

struct abuf {
  char *b;
  int len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len) {
  char *new = realloc(ab->b, ab->len + len);
  if (new == NULL) return;
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}
void abFree(struct abuf *ab) {
  free(ab->b);
}

/* Input */
void editorProcessKeyboard() {
  char c = editorReadKey();

  switch (c) {
    case 27:
      write(STDOUT_FILENO, "\x1b [2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
      break;
  }
}

/* Output */
void editorDrawRows(struct abuf *ab) {
  int y;
  for (y = 0; y < E.screenrows; y++) {
    if (y == E.screencols / 3) {
    char welcome[80];
    int welcomelen = snprintf(welcome, sizeof(welcome), "Dragneel Editor -- version %s", DRAG_VERSION);
    if (welcomelen > E.screencols) welcomelen = E.screencols;
    int padding = (E.screencols - welcomelen) / 2;
    if (padding) {
      abAppend(ab, "~", 1);
      padding--;
    }
    while (padding--) abAppend(ab, " ", 1);
    abAppend(ab, welcome, welcomelen);
  } else {
    abAppend(ab, "~", 1);
  }
    abAppend(ab, "\x1b[K", 3);
    if (y < E.screenrows -1) {
      abAppend(ab, "\r\n", 2);
    }
  }
}

void editorRefreshScreen() {
  struct abuf ab = ABUF_INIT;

  abAppend(&ab, "\x1b[?25l", 6);
  abAppend(&ab, "\x1b[J", 3);

  editorDrawRows(&ab);
    
  abAppend(&ab, "\x1b[H", 3);
  abAppend(&ab, "\x1b[??25h", 6);

  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}


/* Main Program */

void mainEditor() {
  if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
}

int main() {
  enableRawMode();
  mainEditor();
  
  while(1) {
    editorRefreshScreen();
  editorProcessKeyboard();
  }
  
  return 0;
}