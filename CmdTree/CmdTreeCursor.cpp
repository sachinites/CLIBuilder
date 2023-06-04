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
    cmdtc_cursor_type_t cmdtc_type;
    bool success;  /* Was command successfully triggered*/
} cmd_tree_cursor_t;

/* This cursor will be used when we are working in line-by-line mode*/
static cmd_tree_cursor_t *cmdtc_wbw = NULL;
/* This cursor will be used to prse the CLIs when Operating in Char-by-char Mode*/
static cmd_tree_cursor_t *cmdtc_cbc = NULL;

void 
cmd_tree_cursor_init (cmd_tree_cursor_t **cmdtc, cmdtc_cursor_type_t cmdtc_type) {

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
    (*cmdtc)->cmdtc_type = cmdtc_type;
    (*cmdtc)->success = false;
}

void 
cmd_tree_init_cursors () {

    cmd_tree_cursor_init (&cmdtc_wbw, cmdtc_type_wbw);
    cmd_tree_cursor_init (&cmdtc_cbc, cmdtc_type_cbc);
}

cmd_tree_cursor_t *
cmd_tree_get_cursor (cmdtc_cursor_type_t cmdtc_type) {

    switch (cmdtc_type) {

        case cmdtc_type_wbw:
            return cmdtc_wbw;
        case cmdtc_type_cbc:
            return cmdtc_cbc;
    }
    return NULL;
}

bool
cmdtc_get_cmd_trigger_status (cmd_tree_cursor_t *cmdtc) {

    return cmdtc->success;
}

cmdtc_cursor_type_t
cmdtc_get_type (cmd_tree_cursor_t *cmdtc) {

    return cmdtc->cmdtc_type;
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
    cmdtc->success = false;
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

/* This fn removes all the params from the params list whose len is not
        equal to 'len' provided that after removal there should be exactly one
        param left in this list. Return this param.
        This fn must not change anything in the list if after removal of said words,
        either list is empty Or more than one params of strlen = len stays in the
        params_list.
*/
static param_t *
cmdtc_filter_word_by_word_size (glthread_t *param_list, int len) {

    int count;
    glthread_t *curr;
    param_t *param;
    glthread_t temp_list;
    char *param_word;

    count = 0;
    param = NULL;
    init_glthread (&temp_list);

    ITERATE_GLTHREAD_BEGIN (param_list, curr) {

        param = glue_to_param (curr);
        param_word = GET_CMD_NAME(param);
        if (strlen (param_word) != len ) {
            remove_glthread (curr);
            glthread_add_next (&temp_list, curr);
        }
        else {
            count++;
        }
    } ITERATE_GLTHREAD_END (param_list, curr) ;

    if (count == 1) {
         while (dequeue_glthread_first (&temp_list));
         return  glue_to_param(glthread_get_next (param_list));
    }

    while ((curr = dequeue_glthread_first (&temp_list))) {
        glthread_add_next (param_list, curr);
    }

    return NULL;
}


static int
cmdtc_collect_all_matching_params (cmd_tree_cursor_t *cmdtc, unsigned char c, bool wildcard) {

    int i, count = 0;
    glthread_t *curr;
    param_t *child_param;
    glthread_t temp_list;

    assert (cmdtc->cmdtc_type ==cmdtc_type_cbc);

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

    if (wildcard) return count;

    init_glthread(&temp_list);

    /* if list is not empty, then refine the list now*/
    ITERATE_GLTHREAD_BEGIN (&cmdtc->matching_params_list, curr) {

        child_param = glue_to_param(curr);
        assert (IS_PARAM_CMD (child_param));
        if ((GET_CMD_NAME(child_param))[cmdtc->icursor] != c) {
            count--;
            remove_glthread (&child_param->glue);
            glthread_add_next (&temp_list, &child_param->glue);
        }
        
    } ITERATE_GLTHREAD_END (&cmdtc->matching_params_list, curr) 

    if (!count) {
        /* None of the optiona matched, restore the list*/
        #if 0
        cmdtc->matching_params_list = temp_list;
        #else
        while ((curr = dequeue_glthread_first (&temp_list))) {
            glthread_add_next (&cmdtc->matching_params_list, curr);
        }
        #endif
    }
    else {
        while (dequeue_glthread_first (&temp_list));
    }

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
            mcount = cmdtc_collect_all_matching_params (cmdtc, 'X', true);

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

            /* Could be possible that user has fully typed-out the word and now pressed
                 the space. But there are other words which satisfies the match criteria. For example,
                 see below. User has typed a word 'loop' completely and now pressed ' '. In this case
                 We should accept the word 'loop' and allow the user to type further despite that the
                 other word 'loopback' also satisfies the match (multiple match case)
                    Soft-Firewall>$ show node H1 loop
                    nxt cmd  -> loopback                          |   Help : node
                    nxt cmd  -> loop                                 |   Help : node
            */
            param = cmdtc_filter_word_by_word_size 
                            (&cmdtc->matching_params_list, cmdtc->icursor);
            if (!param) {
                cmdt_cursor_display_options (cmdtc);
                return cmdt_cursor_no_match_further;
            }
            cmdtc->curr_param = param;
            cmd_tree_cursor_move_to_next_level (cmdtc);
            return cmdt_cursor_ok;

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

    if (!cli_is_char_mode_on()) {
        return cmdt_cursor_ok;
    }

    if (c == ' ' || c == KEY_ASCII_TAB) {

        return cmdt_cursor_process_space (cmdtc);
    }

    /* Starting from the beginning */
    switch (cmdtc->cmdtc_state) {

        case cmdt_cur_state_init:
            
            assert ( cmdtc->icursor == 0);

            mcount = cmdtc_collect_all_matching_params (cmdtc, c, false);

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

            mcount = cmdtc_collect_all_matching_params (cmdtc, c, false);

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
         mcount = cmdtc_collect_all_matching_params (cmdtc, c, false);

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

    if (!cli_is_char_mode_on ()) return;

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

    mcount = cmdtc_collect_all_matching_params (cmdtc, 'X', true);
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

    if (!cli_is_char_mode_on ()) return;

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
    unsigned char *buffer = cli_get_cli_buffer (cli, NULL);
    
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

/* This fn resets the cmd tree cursor back to pavilion to be
    ready to process next command*/
void 
cmd_tree_cursor_reset_for_nxt_cmd (cmd_tree_cursor_t *cmdtc) {

    param_t *param;

    assert(cmdtc->stack->top >= cmdtc->stack_checkpoint);

    while (cmdtc->stack->top > cmdtc->stack_checkpoint) {

        param = (param_t *)pop(cmdtc->stack);

        if (IS_PARAM_LEAF(param)) {
            memset (GET_LEAF_VALUE_PTR(param), 0, LEAF_VALUE_HOLDER_SIZE);
        }
    }

    /* It is optional to reset the serialized TLV buffer for the same reason. But let me choose to cleanit up as a good practice.*/
    serialize_buffer_restore_checkpoint(cmdtc->tlv_buffer);

    /* Set back the curr_param to start of the root of the tree. Root of the
        tree could be actual 'root' param, or some other param in tree if
        user is operating in mode */
    cmdtc->curr_param = cmdtc->root;

    cmdtc->icursor = 0;
    cmdtc->success = false;
    cmdtc->cmdtc_state= cmdt_cur_state_init;

    while ((dequeue_glthread_first (&cmdtc->matching_params_list)));
    cmdtc->leaf_param = NULL;
}

static void 
cmd_tree_trigger_cli (cmd_tree_cursor_t *cmdtc) {

    int i;
    int count = 1;
    param_t *param;
    op_mode enable_or_diable;

    /* Do not trigger the CLI if the user has not typed CLI to the
        completion*/
    if (!cmdtc->curr_param->callback) {
        printw("\nError : Incomplete CLI...");
        return;
    }
    cmdtc->success = true;

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
        if (param->callback (param, cmdtc->tlv_buffer, enable_or_diable)) {
            cmdtc->success = false;
            break;
        }
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
    if (!cli_is_char_mode_on ()) return;
    
    switch (cmdtc->cmdtc_state) {
        
        case cmdt_cur_state_init:
            /*User has typed the complete current word, fir the CLI if last word
                has appln callback, thenFire the CLI*/
            cmd_tree_trigger_cli (cmdtc);
            if (cmdtc->success) {  cmd_tree_post_cli_trigger (cli);   }
            cmd_tree_cursor_reset_for_nxt_cmd (cmdtc);
            return;
        case cmdt_cur_state_multiple_matches:
            cmd_tree_cursor_reset_for_nxt_cmd (cmdtc);
            return;
        case cmdt_cur_state_single_word_match:
            /* Auto complete the word , push into the stack and TLV buffer and fire the CLI*/
            /* only one option is matching and that is fixed word , do auto-completion*/
            while (GET_CMD_NAME(cmdtc->curr_param)[cmdtc->icursor] != '\0') {
                cli_process_key_interrupt (
                        (int)GET_CMD_NAME(cmdtc->curr_param)[cmdtc->icursor]);
            }
            cmd_tree_cursor_move_to_next_level (cmdtc);
            cmd_tree_trigger_cli (cmdtc);
            if (cmdtc->success) {  cmd_tree_post_cli_trigger (cli);   }
            cmd_tree_cursor_reset_for_nxt_cmd (cmdtc);
            return;
        case cmdt_cur_state_matching_leaf:
            /* Push it into the stack and Fire the CLI*/
            cmd_tree_cursor_move_to_next_level (cmdtc);
            cmd_tree_trigger_cli (cmdtc);
            if (cmdtc->success) {  cmd_tree_post_cli_trigger (cli);   }
            cmd_tree_cursor_reset_for_nxt_cmd (cmdtc);
            return;
        case cmdt_cur_state_no_match:
            cmd_tree_cursor_reset_for_nxt_cmd (cmdtc);
            return;
        default: ;
    }
 }

/* Below Fns are for prsing the command word by word*/
static unsigned char command[MAX_COMMAND_LENGTH];

static param_t*
array_of_possibilities[POSSIBILITY_ARRAY_SIZE];

static inline int
is_cmd_string_match(param_t *param, const char *str, bool *ex_match){
    
    *ex_match = false;
    int str_len = strlen(str);
    int str_len_param = strlen(param->cmd_type.cmd->cmd_name);

    int rc =  (strncmp(param->cmd_type.cmd->cmd_name, 
                   str, str_len));

    if ( !rc && (str_len == str_len_param )) {
        *ex_match = true;
    }
    return rc;
}

param_t*
find_matching_param (param_t **options, const char *cmd_name){
    
    int i = 0,
         j = 0,
        choice = -1,
        leaf_index = -1;
         
    bool ex_match = false;
    
    memset(array_of_possibilities, 0, POSSIBILITY_ARRAY_SIZE * sizeof(param_t *));

    for (; options[i] && i <= CHILDREN_END_INDEX; i++) {

        if (IS_PARAM_LEAF(options[i])) {
            leaf_index = i;
            continue;
        }

        if (is_cmd_string_match(options[i], cmd_name, &ex_match) == 0) {

            if (ex_match) {
                 array_of_possibilities[ 0 ] = options[i];
                 j = 1;
                break;
            }
            array_of_possibilities[ j++ ] = options[i];
            assert (j < POSSIBILITY_ARRAY_SIZE);
            continue;
        }
    }

    if(leaf_index >= 0 && j == 0)
        return options[leaf_index];

    if( j == 0)
        return NULL;

    if(j == 1)
        return array_of_possibilities[0];

    /* More than one param matched*/
    printw("%d possibilities :\n", j);
    for(i = 0; i < j; i++)
        printw("%-2d. %s\n", i, GET_CMD_NAME(array_of_possibilities[i]));

    printw("Choice [0-%d] : ? ", j-1);
    scanf("%d", &choice);
    
    if(choice < 0 || choice > (j-1)){
        printw("\nInvalid Choice");
        return NULL;
    }
    return array_of_possibilities[choice];   
}

/* This fn parse the full command*/
bool
cmdtc_parse_full_command (cli_t *cli) {

    int i;
    int cmd_size;
    int token_cnt;
    tlv_struct_t tlv;
    param_t *param;
    char** tokens = NULL;
    cmd_tree_cursor_t *cmdtc;

    cmdtc = cli_get_cmdtc (cli);
   
    re_init_tokens(MAX_CMD_TREE_DEPTH);

    unsigned char *cmd = cli_get_user_command(cli, &cmd_size);
    
    memset (command, 0, MAX_COMMAND_LENGTH);
    memcpy (command, cmd, cmd_size);

    tokens = tokenizer(command, ' ', &token_cnt);
    
    if (!token_cnt) {

        cmd_tree_cursor_reset_for_nxt_cmd(cmdtc);
        return false;
    }

    param = cmdtc->root;

    for (i= 0; i < token_cnt; i++) {

        param = find_matching_param(&param->options[0], *(tokens +i));

        if (!param){
            printw ("\nCLI Error : Unrecognized Param : %s", *(tokens + i));
            cmd_tree_cursor_reset_for_nxt_cmd(cmdtc);
            return false;
        }

        memset(&tlv, 0, sizeof(tlv_struct_t));

        if (IS_PARAM_LEAF(param)) {

            if (param->cmd_type.leaf->user_validation_cb_fn &&

                (param->cmd_type.leaf->user_validation_cb_fn(
                    cmdtc->tlv_buffer, (unsigned char *)*(tokens + i)) == VALIDATION_FAILED)) {

                printw ("\nCLI Error : User Validation Failed for Param : %s", *(tokens + i));
                cmd_tree_cursor_reset_for_nxt_cmd(cmdtc);
                return false;
            }

            /* Note : We are not saving values within a leaf->value*/
            prepare_tlv_from_leaf(GET_PARAM_LEAF(param), (&tlv));
            put_value_in_tlv((&tlv), *(tokens +i));
            collect_tlv(cmdtc->tlv_buffer, &tlv);
        }
        else {
            cmd_tree_collect_param_tlv(param, cmdtc->tlv_buffer);
        }
        push(cmdtc->stack, (void *)param);
    }

    /* Set the last param which holds the callback*/
    cmdtc->curr_param = param; 
    cmd_tree_trigger_cli(cmdtc);
    if (cmdtc->success) {  cmd_tree_post_cli_trigger (cli);   }
    cmd_tree_cursor_reset_for_nxt_cmd(cmdtc);
    return true;
}

void 
cmd_tree_post_cli_trigger (cli_t *cli) {

    printw ("\nParse Success");
}