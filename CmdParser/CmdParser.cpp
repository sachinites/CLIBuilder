#include <stddef.h>
#include <stdio.h>
#include <ncurses.h>
#include "../KeyProcessor/KeyProcessor.h"
#include "CmdParser.h"

cli_t *cli = NULL;

static int
command_parser_adv (unsigned char *command, int size) {

    /* cli_screen_cursor_move_next_line () do not seems to be working here when
        screen hits at the bottom !! \n works.*/
    printw ("\nCommand Parser Recvd : %s", command);

    return 0;
}


int 
main (int argc, char **argv) {

    cli_key_processor_init (&cli, command_parser_adv);
    cli_start_shell();
    
    return 0;
}