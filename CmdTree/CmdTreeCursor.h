#ifndef __CMDTREE_CURSOR__
#define __CMDTREE_CURSOR__

typedef struct cmd_tree_cursor_ cmd_tree_cursor_t;
typedef struct cli_ cli_t;
typedef struct stack Stack_t;

void 
cmd_tree_cursor_init (cmd_tree_cursor_t **cmdtc);

 cmdt_cursor_op_res_t
 cmdt_cursor_parse_next_char (cmd_tree_cursor_t *cmdtc, unsigned char c);

void 
cmd_tree_cursor_deinit (cmd_tree_cursor_t *cmdtc) ;

void 
cmd_tree_init_cursors () ;

bool
cmdtc_get_cmd_trigger_status (cmd_tree_cursor_t *cmdtc);

cmd_tree_cursor_t *
cmdtc_tree_get_cursor (cmdtc_cursor_type_t cmdtc_type);

cmdtc_cursor_type_t
cmdtc_get_type (cmd_tree_cursor_t *cmdtc);

void 
cmd_tree_cursor_move_to_next_level (cmd_tree_cursor_t *cmdtc) ;

int
cmd_tree_cursor_move_one_level_up (cmd_tree_cursor_t *cmdtc, bool honor_checkpoint) ;

void 
cmdtc_process_question_mark (cmd_tree_cursor_t *cmdtc);

void 
cmd_tree_enter_mode (cmd_tree_cursor_t *cmdtc);

void
cmdtc_show_complete_clis (cmd_tree_cursor_t *cmdtc);

void 
cmd_tree_process_carriage_return_key (cmd_tree_cursor_t *cmdtc) ;

bool 
cmdtc_is_cursor_at_bottom_mode_node (cmd_tree_cursor_t *cmdtc);

bool
cmdtc_parse_full_command (cli_t *cli);

void 
cmd_tree_cursor_reset_for_nxt_cmd (cmd_tree_cursor_t *cmdtc) ;

void 
cmd_tree_post_cli_trigger (cli_t *cli);

void 
cmdtc_reset_cursor (cmd_tree_cursor_t *cmdtc);

bool 
cmdtc_is_this_config_command (cmd_tree_cursor_t *cmdtc);

Stack_t *
cmdtc_get_stack (cmd_tree_cursor_t *cmdtc);

void
cmdtc_display_all_complete_commands (cmd_tree_cursor_t *cmdtc);

#endif 