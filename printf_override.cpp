#include <ncurses.h>
#include <cstdarg>
#include <cstring>
#include <stdint.h>
#include "string_util.h"

#define printf cprintf 

static char filterStr[128] = {0};
static char Obuffer[256] = {0};

#define FLAG_STDSCR_DISPLAY 1
#define MATCH_PATTERN   2

static uint8_t printfFlags = (FLAG_STDSCR_DISPLAY | MATCH_PATTERN);

static void 
cprintf_set_filter (const char *filterstring) {

    memset (filterStr, 0, sizeof (filterStr));
    strncpy (filterStr, filterstring, strlen (filterstring));
}

/* Return 0 if pattern matching is enabled and pattern is matched.
    Return -1 is pattern matching is enabled, and pattern is not matched
    Return 0 of mattcern matching is disabled 
*/
int cprintf (const char* format, ...)
{
    int ret = -1;
    va_list args;
    va_start(args, format);

    memset (Obuffer, 0, sizeof (Obuffer));
    vsnprintf(Obuffer, sizeof(Obuffer), format, args);

    va_end(args);

    if (printfFlags & MATCH_PATTERN) {

        if (pattern_match(Obuffer, strlen (Obuffer), (const char *)filterStr)) {
            ret = 0;
        }
    }
    else {
        ret = 0;
    }

    if (printfFlags & FLAG_STDSCR_DISPLAY) 
    {
        printw("%s", Obuffer);
    }

    return ret;
}

void initNcurses()
{
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    refresh();
}

void cleanupNcurses()
{
    endwin();
}

int main()
{
    initNcurses();

    cprintf_set_filter ("ain");
    printf ("Hello, World!\n");
    printf ("The answer is %d.\n", 42);
    printf ("This line does not contain the keyword.\n");
    printf ("This line contains the number %d.\n", 123);

    getch();

    cleanupNcurses();

    return 0;
}
