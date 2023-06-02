#include <stddef.h>
#include <stdio.h>
#include <ncurses.h>
#include "../CmdTree/CmdTree.h"
#include "../KeyProcessor/KeyProcessor.h"

typedef struct cli_ cli_t;

cli_t *cli = NULL;

static int
command_parser_process_cmd (unsigned char *command, int size) {

    cmd_tree_parse_res_t rc;
    /* cli_screen_cursor_move_next_line () do not seems to be working here when
        screen hits at the bottom !! \n works.*/
    printw ("\nCommand Parser Recvd : %s", command);

    rc = cmd_tree_parse_command (command, size);

    switch (rc) {

        case cmd_tree_parse_app_accepted:
        case cmd_tree_parse_app_rejected:
            return 0;
        case cmd_tree_parse_incomplete_cmd:
        case cmd_tree_parse_invalid_cmd:
            return -1;
        default:
            return -1;
    }
    return -1;
}

/* Return 0 if the command parsing is successful, return -1 if this is invalid command*/
int
cli_cmd_tree_parse_command (unsigned char *command, int size) {

    return command_parser_process_cmd (command, size);
}


int 
main (int argc, char **argv) {

    cli_key_processor_init (&cli);
    cli_start_shell();
    
    return 0;
}


