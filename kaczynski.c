#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE
/*includes*/
#include <stddef.h>
#include <termios.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

/*defines*/
#define CTRL_KEY(k) ((k) & 0x1f)
#define ABUF_INIT {NULL, 0}


/*global variables*/
typedef struct {
    int size;
    char *karakteri;
}erow;

typedef struct {
  char *b;
  int len;
}abuf;

typedef struct {
    struct termios orig_termios;
    int screencols, screenrows;
    int cx, cy;
    int rowoff;
    erow *row;
    int numrows;
} termm;

termm E;



enum editorKey {
  ARROW_LEFT = 1000,
  ARROW_RIGHT, 
  ARROW_UP,
  ARROW_DOWN,
  DEL_KEY,
  PAGE_UP,
  PAGE_DOWN
};

/*function prototypes*/
void ukljuciRaw(void);
void iskljuciRaw(void);
void umri(const char *s);
void abAppend(abuf *, const char*, int);
void abFree(abuf *);
void editorMoveCursor(char); 

/*terminal stuff*/
void ukljuciRaw() {
    if(tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
        umri("tcgetattr");

    atexit(iskljuciRaw);

    struct termios raw = E.orig_termios;

    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN |ISIG);

    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;


    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) umri("tcsetattr");
}

void iskljuciRaw() {
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        umri("tcsetattr");
}
void signalHandler(int sig) {
     printf("Hvatanje signala: %d\n", sig);
    iskljuciRaw();
    exit(0);
}

void umri(const char *s){
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    perror(s);
    exit(1);
}
int editorReadKey(){
    int nread;
    char c;

    while ((nread = read(STDIN_FILENO, &c, 1)) != 1){
        if((nread == -1 && errno != EAGAIN)) 
            umri("read");
    }

    if(c == '\x1b'){
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '3': return DEL_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                    }
                }
            } else {
                switch (seq[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                }
            }
        }
        return '\x1b';
    } else {
        return c;
    }

}
int getCursorPosition(int *rows, int *cols) {
    char buff[32];
    unsigned int i = 0;


    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;
  
    while (i < sizeof(buff) - 1) {
        if(read(STDIN_FILENO, &buff[i], 1) != 1) break;
        if(buff[i] == 'R') break;
        i++;
    }
    buff[i] = '\0';

    //printf("\r\n&buff[1]: '%s'\r\n", &buff[1]);

    if(buff[0] != '\x1b' || buff[1] != '[') return -1;
    if(sscanf(&buff[2], "%d;%d", rows, cols) != 2) return -1;

    return 0;
}

int getWindowSize(int *rows, int *cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
         return getCursorPosition(rows, cols);
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}
void editorAppendRow(char *s, size_t len) {
    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));

    int at = E.numrows;
    E.row[at].size = len;
    E.row[at].karakteri = malloc(len + 1);
    memcpy(E.row[at].karakteri, s, len);
    E.row[at].karakteri[len] = '\0';
    E.numrows++;
}

void edopen(char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) umri("fopen");

    char *line = NULL;
    ssize_t linelen;
    size_t linecap;

    while((linelen = getline(&line, &linecap, fp)) != -1){
        if (linelen != -1) {
            while (linelen > 0 && (line[linelen - 1] == '\n' ||
                        line[linelen - 1] == '\r'))
                linelen--;
            editorAppendRow(line,linelen);
        }
    }
    free(line);
    fclose(fp);

}



/* input */
void editorProcessKeypress() {
    int c = editorReadKey(), times;
    switch (c) {
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit (0);
            break;
        case ARROW_UP:
            if(E.cy)
                E.cy--;
            break;
        case ARROW_DOWN:
            if(E.cy != E.numrows - 1)
                E.cy++;
            break;
        case ARROW_LEFT:
            if(E.cx)
                E.cx--;
            break;
        case ARROW_RIGHT:
            if(E.cx != E.screencols - 1)
                E.cx++;
            break;
        case PAGE_DOWN:
            times = E.screenrows;
            while (times-- && E.cy < E.screenrows - 1){
                E.cy++;
            }
            break;
        case PAGE_UP:
            times = E.screenrows;
            while(times-- && E.cy){
                E.cy--;
            }
            break;
    }
}


/*** output ***/
void editorScroll() {
    if(E.cy < E.rowoff)
        E.rowoff = E.cy;
    if(E.cy > E.rowoff + E.screenrows)
        E.rowoff = E.cy - E.screenrows + 1;
}

void editorDrawRows(abuf *ab) {
    int y;

    for (y = 0; y < E.screenrows; y++) {
        int filerow = y + E.rowoff;
        if(filerow >= E.numrows){
                abAppend(ab, "~", 1);
        }else {
            int len = E.row[filerow].size;
            if (len < 0) len = 0;
            if(len > E.screencols) len = E.screencols;
            abAppend(ab, E.row[filerow].karakteri, len);
        }
        abAppend(ab, "\x1b[K", 3);
        if (y < E.screenrows - 1) {
            abAppend(ab, "\r\n", 2);
        }
    }
}

void editorRefreshScreen() {
    editorScroll();

    abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRows(&ab);
    
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, E.cx + 1);
    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}
void abAppend(abuf *ab, const char *s, int len) {
  char *new = realloc(ab->b, ab->len + len);
  if (new == NULL) return;
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}
void abFree(abuf *ab) {
  free(ab->b);
}
void inited(){
    E.cx      = 0; 
    E.cy      = 0;
    E.numrows = 0;
    E.rowoff  = 0;
    E.row     = NULL;
    if (getWindowSize(&E.screenrows, &E.screencols) == -1) umri("getWindowSize");
}
int main(int argc, char **argv){
    ukljuciRaw();
    inited();
    if(argc > 1){
        edopen(argv[1]);
    }
    while(1){

        editorRefreshScreen();
        editorProcessKeypress();
    }
    iskljuciRaw();
    return 0;
}
