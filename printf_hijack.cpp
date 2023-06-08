#include <stdio.h>
#include <ncurses.h>
#include <stdarg.h>

int 
printf ( const char *format, ...) {

    int rc;
    va_list args;
    va_start(args, format);
    rc = printw (format, args);
    va_end(args);
    return rc;
}