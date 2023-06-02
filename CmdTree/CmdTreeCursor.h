#ifndef __CMDTREE_CURSOR__
#define __CMDTREE_CURSOR__

typedef struct cmd_tree_cursor_ cmd_tree_cursor_t;
typedef struct cli_ cli_t;

typedef enum cmdt_cursor_op_res_ {

    cmdt_cursor_ok,
    cmdt_cursor_no_match_further,
    cmdt_cursor_done_auto_completion
} cmdt_cursor_op_res_t;

void 
cmd_tree_cursor_init (cmd_tree_cursor_t **cmdtc, cli_t *cli);

 cmdt_cursor_op_res_t
 cmdt_cursor_parse_next_char (cmd_tree_cursor_t *cmdtc, unsigned char c);

void 
cmd_tree_cursor_reset (cmd_tree_cursor_t *cmdtc) ;

void 
cmd_tree_default_cursor_init () ;

cmd_tree_cursor_t *
cmf_tree_get_default_cursor ();

void 
cmd_tree_cursor_move_to_next_level (cmd_tree_cursor_t *cmdtc) ;

#endif 