#include <stddef.h>
#include <ncurses.h>
#include "libcli.h"

typedef struct cli_ cli_t;

/* CLI instance ownsed by the application */
cli_t *cli = NULL;

int 
main (int argc, char **argv) {

    cli_key_processor_init (&cli);
    cli_start_shell();
    return 0;
}
