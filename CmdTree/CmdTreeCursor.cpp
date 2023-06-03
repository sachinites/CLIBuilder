#include <assert.h>
#include <stdlib.h>
#include <ncurses.h>
#include "../stack/stack.h"
#include "../serializer/serialize.h"
#include "../cmdtlv.h"
#include "../string_util.h"
#include "CmdTree.h"
#include "CmdTreeCursor.h"
#include "../KeyProcessor/KeyProcessor.h"

extern void
cli_process_key_interrupt(int ch);

typedef enum cmdt_cursor_state_ {

    cmdt_cur_state_init,
    cmdt_cur_state_multiple_matches,
    cmdt_cur_state_single_word_match,
    cmdt_cur_state_matching_leaf,
    cmdt_cur_state_no_match
} cmdt_tree_cursor_state_t;

typedef struct cmd_tree_cursor_ {

    param_t *root;
    Stack_t *stack;
    int stack_checkpoint;
    ser_buff_t *tlv_buffer;
    param_t *curr_param;
    int icursor;
    cmdt_tree_cursor_state_t cmdtc_state;
    glthread_t matching_params_list;
    param_t *leaf_param;
} cmd_tree_cursor_t;

static cmd_tree_cursor_t *default_cmdtc = NULL;

void 
cmd_tree_cursor_init (cmd_tree_cursor_t **cmdtc) {

    *cmdtc = (cmd_tree_cursor_t *)calloc (1, sizeof (cmd_tree_cursor_t));
    (*cmdtc)->root = libcli_get_root_hook();
    (*cmdtc)->stack = get_new_stack();
    (*cmdtc)->stack_checkpoint = -1;    
      init_serialized_buffer_of_defined_size (&((*cmdtc)->tlv_buffer), TLV_MAX_BUFFER_SIZE);
    (*cmdtc)->curr_param = libcli_get_root_hook();
    (*cmdtc)->icursor=0;
    (*cmdtc)->cmdtc_state = cmdt_cur_state_init;
    (*cmdtc)->matching_params_list = {0, 0};
    (*cmdtc)->leaf_param = NULL;
}

void 
cmd_tree_default_cursor_init () {

    cmd_tree_cursor_init (&default_cmdtc);
}

cmd_tree_cursor_t *
cmf_tree_get_default_cursor () {

    return default_cmdtc;
}


void 
cmd_tree_cursor_deinit (cmd_tree_cursor_t *cmdtc) {

    glthread_t *curr;
    reset_stack (cmdtc->stack);
    cmdtc->stack_checkpoint = -1;
    reset_serialize_buffer (cmdtc->tlv_buffer);
    cmdtc->curr_param = NULL;
    cmdtc->icursor = 0;
    cmdtc->cmdtc_state = cmdt_cur_state_init;
    while ((dequeue_glthread_first (&cmdtc->matching_params_list)));
    cmdtc->leaf_param = NULL;
}

void 
cmd_tree_cursor_move_to_next_level (cmd_tree_cursor_t *cmdtc) {

    push(cmdtc->stack, (void *)cmdtc->curr_param);
    cmd_tree_collect_param_tlv(cmdtc->curr_param, cmdtc->tlv_buffer);
    assert (cmdtc->curr_param); /* Must be already set by the caller*/
    cmdtc->icursor = 0;
    cmdtc->cmdtc_state = cmdt_cur_state_init;
    while (dequeue_glthread_first(&cmdtc->matching_params_list));
    cmdtc->leaf_param = NULL;
}

static int
cmdt_collect_all_matching_params (cmd_tree_cursor_t *cmdtc, unsigned char c, bool wildcard) {

    int i, count = 0;
    glthread_t *curr;
    param_t *child_param;

    if (IS_GLTHREAD_LIST_EMPTY (&cmdtc->matching_params_list)) {

        /* Iterarate over all firect children of cmdtc->curr_param, and append them tp list which matches c at cmdtc->icursor*/
        for (i = CHILDREN_START_INDEX; i <= CHILDREN_END_INDEX; i++) {

            child_param = cmdtc->curr_param->options[i];
            if (!child_param) continue;

            if (IS_PARAM_LEAF (child_param)) {
                assert(!cmdtc->leaf_param);
                cmdtc->leaf_param = child_param;
                continue;
            }

            if (!wildcard && 
                (GET_CMD_NAME(child_param))[cmdtc->icursor] != c) continue;
            assert (!IS_QUEUED_UP_IN_THREAD (&child_param->glue));
            glthread_add_last (&cmdtc->matching_params_list, &child_param->glue);
            count++;
        }
        return count;
    }

    count = get_glthread_list_count(&cmdtc->matching_params_list);

    /* if list is not empty, then refine the list now*/
    ITERATE_GLTHREAD_BEGIN (&cmdtc->matching_params_list, curr) {

        child_param = glue_to_param(curr);
        assert (IS_PARAM_CMD (child_param));
        if (!wildcard &&
             (GET_CMD_NAME(child_param))[cmdtc->icursor] != c) {
            count--;
            remove_glthread (&child_param->glue);
        }
        
    } ITERATE_GLTHREAD_END (&cmdtc->matching_params_list, curr) 

    return count;
}

static void 
cmdt_cursor_display_options (cmd_tree_cursor_t *cmdtc) {

    glthread_t *curr;
    param_t *param;

    int row, col1, col2;
    getyx(stdscr, row, col1);

    if (IS_GLTHREAD_LIST_EMPTY (&cmdtc->matching_params_list) &&
            !cmdtc->leaf_param) {
        /* Nothing to display */
        return;
    }

    cli_screen_cursor_move_next_line ();

    ITERATE_GLTHREAD_BEGIN (&cmdtc->matching_params_list, curr) {

        param = glue_to_param (curr);
        printw ("nxt cmd  -> %-31s   |   %s\n", 
            GET_CMD_NAME(param), 
            GET_PARAM_HELP_STRING(param));
      
    } ITERATE_GLTHREAD_END (&cmdtc->matching_params_list, curr);

    if (cmdtc->leaf_param) {
        
        printw ("nxt cmd  -> %-32s   |   %s\n", 
            GET_LEAF_TYPE_STR(cmdtc->leaf_param), 
            GET_PARAM_HELP_STRING(cmdtc->leaf_param));        
    }

    cli_printsc (cli_get_default_cli(), false);
    getyx(stdscr, row, col2);
    move (row, col1);
}


static  cmdt_cursor_op_res_t
cmdt_cursor_process_space (cmd_tree_cursor_t *cmdtc) ;

cmdt_cursor_op_res_t
cmdt_cursor_process_space (cmd_tree_cursor_t *cmdtc) {

    int i, mcount;
    tlv_struct_t tlv;
    glthread_t *curr;
    param_t *param;

    switch (cmdtc->cmdtc_state) {

        case cmdt_cur_state_init:
            /* User is typing ' ' even without typing any character for the next word
            In this case, if there is only one alternate option, then only go for
            auto-completion. In rest of the cases, block user and display alternatives*/
            mcount = cmdt_collect_all_matching_params (cmdtc, 'X', true);

            if (mcount == 0) {

                if (cmdtc->leaf_param) {
                    /* Leaf is available at this level, user just cant type ' '. Undo
                        the state change and block user cursor*/
                    cmdt_cursor_display_options (cmdtc);
                    cmdtc->leaf_param = NULL;
                    while (dequeue_glthread_first(&cmdtc->matching_params_list)) ;
                    return cmdt_cursor_no_match_further;
                }
            }

            else if (mcount >1 ) {

                cmdt_cursor_display_options (cmdtc);
                cmdtc->leaf_param = NULL;
                while (dequeue_glthread_first(&cmdtc->matching_params_list)) ;
                return cmdt_cursor_no_match_further;
            }

            else if (mcount == 1) {

                if (cmdtc->leaf_param) {
                    
                    cmdt_cursor_display_options (cmdtc);
                    /* Leaf is available at this level, user just cant type ' '. Undo
                        the state change and block user cursor*/
                    cmdtc->leaf_param = NULL;
                    while (dequeue_glthread_first(&cmdtc->matching_params_list));
                    return cmdt_cursor_no_match_further;
                }
                /* Only one matching key-word, go for auto-completion now*/
                cmdtc->cmdtc_state = cmdt_cur_state_single_word_match;
                cmdtc->curr_param = glue_to_param (glthread_get_next (&cmdtc->matching_params_list));
                
                while (GET_CMD_NAME(cmdtc->curr_param)[cmdtc->icursor] != '\0') {
                    cli_process_key_interrupt (
                        (int)GET_CMD_NAME(cmdtc->curr_param)[cmdtc->icursor]);
                }
                 cmd_tree_cursor_move_to_next_level (cmdtc);
                 return cmdt_cursor_done_auto_completion;
            }

        break;

        case cmdt_cur_state_multiple_matches:
            /* User must not type space if there are more multiple matches*/
            cmdt_cursor_display_options (cmdtc);
            return cmdt_cursor_no_match_further;

        case cmdt_cur_state_single_word_match:
            /* only one option is matching and that is fixed word , do auto-completion*/
            while (GET_CMD_NAME(cmdtc->curr_param)[cmdtc->icursor] != '\0') {
                cli_process_key_interrupt (
                        (int)GET_CMD_NAME(cmdtc->curr_param)[cmdtc->icursor]);
            }
            /* update cmdtc to point to next level*/
            cmd_tree_cursor_move_to_next_level (cmdtc);
            return cmdt_cursor_done_auto_completion;

        case cmdt_cur_state_matching_leaf:
            /* User has typed ' ' while inputting the value of leaf. Go to next level*/
            cmd_tree_cursor_move_to_next_level (cmdtc);
            return cmdt_cursor_ok;
        case cmdt_cur_state_no_match:
            /* We cant reach here*/
            assert(0);
        default:;
        return cmdt_cursor_no_match_further;
    }
    return cmdt_cursor_no_match_further;
}

 cmdt_cursor_op_res_t
 cmdt_cursor_parse_next_char (cmd_tree_cursor_t *cmdtc, unsigned char c) {

    int mcount;
    param_t *param;
    cmdt_cursor_op_res_t rc = cmdt_cursor_ok;

    if (c == ' ') {

        return cmdt_cursor_process_space (cmdtc);
    }

    /* Starting from the beginning */
    switch (cmdtc->cmdtc_state) {

        case cmdt_cur_state_init:
            
            assert ( cmdtc->icursor == 0);

            mcount = cmdt_collect_all_matching_params (cmdtc, c, false);

            /* Case 1 : When exactly 1 single param (keyword) matches . it cannot be leaf param*/
            if (mcount == 1) {

                cmdtc->cmdtc_state = cmdt_cur_state_single_word_match;
                cmdtc->curr_param = glue_to_param (glthread_get_next (&cmdtc->matching_params_list));

                if (cmdtc->curr_param) {

                    /* No Action*/
                }
                else if (cmdtc->leaf_param){
                    cmd_tree_leaf_char_save (cmdtc->curr_param, c, cmdtc->icursor);
                }
                cmdtc->icursor++;
                assert(cmdtc->icursor == 1);
                return cmdt_cursor_ok;
            }

            else if (mcount > 1) {

                /* if more than 1 child params are matching*/
                cmdtc->cmdtc_state = cmdt_cur_state_multiple_matches;
                if (cmdtc->leaf_param) {
                     cmd_tree_leaf_char_save (cmdtc->leaf_param, c, cmdtc->icursor);
                }
                cmdtc->icursor++;
                assert(cmdtc->icursor == 1);
                return cmdt_cursor_ok;
            }

            else {

                /* If none of the key word is matching, if leaf param is there, then accept the char */
                if (cmdtc->leaf_param) {
                    cmdtc->curr_param = cmdtc->leaf_param;
                    cmdtc->cmdtc_state =  cmdt_cur_state_matching_leaf;
                    cmd_tree_leaf_char_save (cmdtc->leaf_param, c, cmdtc->icursor);
                    cmdtc->icursor++;
                    return cmdt_cursor_ok;
                }
                /* no matching word, and no leaf to accept value, block the user*/
                return   cmdt_cursor_no_match_further;
            }
        break;



    case cmdt_cur_state_multiple_matches:

            mcount = cmdt_collect_all_matching_params (cmdtc, c, false);

            /* Case 1 : When exactly 1 single param patches . it could be leaf param*/
            if (mcount == 1) {

                cmdtc->cmdtc_state = cmdt_cur_state_single_word_match;
                cmdtc->curr_param = glue_to_param (glthread_get_next (&cmdtc->matching_params_list));

                if (cmdtc->curr_param) {

                    /* No Action*/
                }
                else if (cmdtc->leaf_param){

                    cmd_tree_leaf_char_save (cmdtc->curr_param, c, cmdtc->icursor);
                }
                cmdtc->icursor++;
                return cmdt_cursor_ok;
            }

            else if (mcount > 1) {

                /* if more than 1 child params are matching*/
                if (cmdtc->leaf_param) {
                     cmd_tree_leaf_char_save (cmdtc->leaf_param, c, cmdtc->icursor);
                }
                cmdtc->icursor++;
                return cmdt_cursor_ok;
            }

            else {

                /* If none of the key word is matching, if leaf param is there, then accept the char */
                if (cmdtc->leaf_param) {
                    cmdtc->curr_param = cmdtc->leaf_param;
                    cmdtc->cmdtc_state =  cmdt_cur_state_matching_leaf;
                    cmd_tree_leaf_char_save (cmdtc->leaf_param, c, cmdtc->icursor);
                    cmdtc->icursor++;
                    return cmdt_cursor_ok;
                }
                /* no matching word, and no leaf to accept value, block the user*/
                return   cmdt_cursor_no_match_further;
            }
        break;



    case cmdt_cur_state_single_word_match:
         mcount = cmdt_collect_all_matching_params (cmdtc, c, false);

          /* Case 1 : When the same param continues to be matched, and this is cmd param only*/
            if (mcount == 1) {

                if (cmdtc->leaf_param) {
                     cmd_tree_leaf_char_save (cmdtc->leaf_param, c, cmdtc->icursor);
                }

                cmdtc->icursor++;
                return cmdt_cursor_ok;
            }

            else if (mcount > 1) {
                assert(0);
            }

            else {

                /* If none of the key word is matching, if leaf param is there, then accept the char */
                if (cmdtc->leaf_param) {
                    cmdtc->curr_param = cmdtc->leaf_param;
                    cmdtc->cmdtc_state =  cmdt_cur_state_matching_leaf;
                    cmd_tree_leaf_char_save (cmdtc->leaf_param, c, cmdtc->icursor);
                    cmdtc->icursor++;
                    return cmdt_cursor_ok;
                }
                /* no matching word, and no leaf to accept value, block the user*/
                return   cmdt_cursor_no_match_further;
            } 
        break;


    case cmdt_cur_state_matching_leaf:
        assert(cmdtc->leaf_param &&
                  cmdtc->curr_param && 
                  (cmdtc->leaf_param == cmdtc->curr_param));
        cmd_tree_leaf_char_save (cmdtc->leaf_param, c, cmdtc->icursor);
        cmdtc->icursor++;
        return cmdt_cursor_ok;
        break;


    case cmdt_cur_state_no_match:
        /* User is already blocked, and furter parsing should not be invoked*/
        assert(0);
        break;

    default: 
        assert(0);
    }


    assert(0);
    return cmdt_cursor_no_match_further;
 }

bool 
cmdtc_is_cursor_at_bottom_mode_node (cmd_tree_cursor_t *cmdtc) {

    param_t *param = cmdtc->curr_param;

    return (param->options[0] == NULL);
}

/* Cmd Tree Cursor based functions */
void 
cmdtc_process_question_mark (cmd_tree_cursor_t *cmdtc) {

    cli_t *cli;
    int mcount;

    cli = cli_get_default_cli();

    if (!cli_cursor_is_at_end_of_line (cli)) return;
    if (cmdtc_is_cursor_at_bottom_mode_node (cmdtc)) return;

    /* If we have already computed next set of alternatives, display them. We would
        compute them if user has began typing  new word in a CLI*/
    if (!IS_GLTHREAD_LIST_EMPTY (&cmdtc->matching_params_list) ||
            cmdtc->leaf_param) {
        
        cmdt_cursor_display_options (cmdtc);
        return;
    }

    /* If user has not typed beginning a new word in a cli, then compute the next
        set pf alternatives*/

    mcount = cmdt_collect_all_matching_params (cmdtc, 'X', true);
    cmdt_cursor_display_options (cmdtc);
    cmdtc->leaf_param = NULL;
    while (dequeue_glthread_first(&cmdtc->matching_params_list)) ;
}

void
cmdtc_show_complete_clis (cmd_tree_cursor_t *cmdtc) {

}

void 
cmd_tree_enter_mode (cmd_tree_cursor_t *cmdtc) {

    cli_t *cli;
    glthread_t *curr;
    param_t *param;
    glthread_t temp_list;
    
    int byte_count = 0;

    init_glthread (&temp_list);

    cli = cli_get_default_cli();

    if (cmdtc->root == cmdtc->curr_param) return;
    if (!cli_cursor_is_at_end_of_line (cli)) return;
    if (cli_cursor_is_at_begin_of_line (cli)) return;
    if (cmdtc->cmdtc_state != cmdt_cur_state_init) return;
    if (cmdtc_is_cursor_at_bottom_mode_node (cmdtc)) return;

    while ((param = (param_t *)pop (cmdtc->stack))) {
        assert (!IS_QUEUED_UP_IN_THREAD (&param->glue));
        glthread_add_next (&temp_list, &param->glue);
    }

    cli_complete_reset (cli);
    unsigned char *buffer = cli_get_cli_buffer (cli);
    
    byte_count += snprintf ((char *)buffer + byte_count, 
                            MAX_COMMAND_LENGTH, "%s", DEF_CLI_HDR);

    ITERATE_GLTHREAD_BEGIN(&temp_list, curr) {

        param = glue_to_param (curr);

        if (IS_PARAM_CMD (param)) {

            byte_count += snprintf ((char *)buffer + byte_count, 
                            MAX_COMMAND_LENGTH - byte_count, "%s-", GET_CMD_NAME(param));
        }
        else {
            
            byte_count += snprintf ((char *)buffer + byte_count, 
                            MAX_COMMAND_LENGTH - byte_count, "%s-", GET_LEAF_VALUE_PTR(param));

        }
    }ITERATE_GLTHREAD_END(&temp_list, curr) 

    buffer[byte_count - 1] = '>';
    buffer[byte_count] = ' ';
    byte_count++;

    cli_set_hdr (cli, NULL, byte_count);

    /* Prepare the stack again*/
    while ((curr = dequeue_glthread_first (&temp_list))) {

         param = glue_to_param (curr);
         push (cmdtc->stack , (void *)param);
    }

    /* Save the contect of where we are now in CLI tree*/
    cmdtc->root = cmdtc->curr_param;
    serialize_buffer_mark_checkpoint (cmdtc->tlv_buffer);
    cmdtc->stack_checkpoint = cmdtc->stack->top;

    cli_printsc (cli, true);
}

static void 
cmd_tree_trigger_cli (cmd_tree_cursor_t *cmdtc) {

    int i;
    int count = 1;
    param_t *param;
    op_mode enable_or_diable;

    int tlv_buffer_original_size = tlv_buffer_get_size (cmdtc->tlv_buffer);
    int tlv_buffer_checkpoint_offset = serialize_buffer_get_checkpoint_offset (cmdtc->tlv_buffer);

    param = (param_t *)cmdtc->stack->slot[0];

    if (param == libcli_get_config_hook ()) {
        /* ToDo : Support Command Negation !*/
        enable_or_diable = CONFIG_ENABLE;
    }
    else {
        enable_or_diable = OPERATIONAL;
    }

    for (i = cmdtc->stack_checkpoint + 1 ; i <= cmdtc->stack->top; i++) {

        param = (param_t *)cmdtc->stack->slot[i];

        if (!param->callback)  {
            count++;
            continue;
        }
        /* Temporarily over-write the size of TLV buffer */
        cmdtc->tlv_buffer->next = tlv_buffer_checkpoint_offset + (sizeof (tlv_struct_t) * count);
        count++;
        param->callback (param, cmdtc->tlv_buffer, enable_or_diable);
    }

    cmdtc->tlv_buffer->next = tlv_buffer_original_size;
}

void 
cmd_tree_process_carriage_return_key (cmd_tree_cursor_t *cmdtc) {

    cli_t *cli = cli_get_default_cli();

    /* User has simply pressed the entry key wihout typing anything.
        Nothing to do by cmdtree cursor, Keyprocessor will simply shift
        the cursor to next line*/
    if (cli_is_buffer_empty (cli)) return;

    switch (cmdtc->cmdtc_state) {
        
        case cmdt_cur_state_init:
            /* Fire the CLI*/
            cmd_tree_trigger_cli (cmdtc);
            return;
        case cmdt_cur_state_multiple_matches:
            return;
        case cmdt_cur_state_single_word_match:
            /* Auto complete the word , push into the stack and fire the CLI*/
            return;
        case cmdt_cur_state_matching_leaf:
            /* Push it into the stack and Fire the CLI*/
            return;
        case cmdt_cur_state_no_match:
            return;
        default: ;
    }
 }