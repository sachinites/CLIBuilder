#include <assert.h>
#include <stdlib.h>
#include <ncurses.h>
#include "../stack/stack.h"
#include "../serializer/serialize.h"
#include "../cmdtlv.h"
#include "../string_util.h"
#include "CmdTree.h"
#include "clistd.h"
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

    /* The root of the current CLI being triggered. It could be any non-leaf
        param in the CLI history.*/
    param_t *root;
    /* DS used to store the CLI conext in execution*/
    Stack_t *stack;
    /* When entering into Mode, this chkpnt is used to mark the stack state*/
    int stack_checkpoint;
    /* Buffer used to store the values of individual keywords of CLI.  This should go hand-in-hand
        with the stack. This TLV buffer is given to the application eventually for parsing and picking up CLI values*/
    ser_buff_t *tlv_buffer;
    /* While parsing the cmd tree, this pointer points to the current param being parsed*/
    param_t *curr_param;
    /* This is the index into Param's keyword/value to track which character is being typed*/
    int icursor;
    /* This is used to track the state of cmd tree in reslation to CLI being executed*/
    cmdt_tree_cursor_state_t cmdtc_state;
    /* This is the list of multiple matching keywords*/
    glthread_t matching_params_list;
    /* This is the leaf param present in the current level in the cmd tree*/
    param_t *leaf_param;
    /* This is the cursor type, where cbc or wbw*/
    cmdtc_cursor_type_t cmdtc_type;
    /* This is boolean which indiscate that cmd is successfully submitted to appln or not */
    bool success;
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
cmdtc_tree_get_cursor (cmdtc_cursor_type_t cmdtc_type) {

    switch (cmdtc_type) {

        case cmdtc_type_wbw:
            return cmdtc_wbw;
        case cmdtc_type_cbc:
            return cmdtc_cbc;
    }
    return NULL;
}

const char *
cmdtc_get_state_str (cmd_tree_cursor_t *cmdtc) {

    switch (cmdtc->cmdtc_state) {
        case cmdt_cur_state_init:
            return (const char *)"cmdt_cur_state_init";
        case cmdt_cur_state_multiple_matches:
            return (const char *)"cmdt_cur_state_multiple_matches";
        case cmdt_cur_state_single_word_match:
             return (const char *)"cmdt_cur_state_single_word_match";
        case cmdt_cur_state_matching_leaf:
            return (const char *)"cmdt_cur_state_matching_leaf";
        case  cmdt_cur_state_no_match:
            return (const char *)"cmdt_cur_state_no_match";
        default : ;
    }
    return NULL;
}

void
cmdtc_debug_print_stats (cmd_tree_cursor_t *cmdtc) {

   tlv_struct_t *top_tlv = NULL;
   char *tlv_top_name = NULL;

    if (!cmdtc) return;

    param_t *param_top = (param_t *)StackGetTopElem (cmdtc->stack);
  
    if (!is_serialized_buffer_empty (cmdtc->tlv_buffer)) {

        char *ptr = serialize_buffer_get_current_ptr (cmdtc->tlv_buffer);
        top_tlv = (tlv_struct_t *)  (ptr - sizeof (tlv_struct_t));
    }

    if (top_tlv) {

        if (top_tlv->tlv_type == TLV_TYPE_CMD_NAME ||
                top_tlv->tlv_type == TLV_TYPE_NEGATE) {
            tlv_top_name = (char *)top_tlv->value;
        }
         else {
             tlv_top_name = (char *)top_tlv->leaf_id;
         }
    }

    /* Get the checkpointed param from stack if any*/
    param_t *param_chkp = (cmdtc->stack_checkpoint > -1) ? \
                        (param_t *)cmdtc->stack->slot[cmdtc->stack_checkpoint] : NULL;

    printw ("\nroot = %s, curr_param = %s, stack top = %s, "
                    "chkp = %s, top_index = %d, tlv_top = %s, state = %s", 
            IS_PARAM_CMD(cmdtc->root) ? 
                GET_CMD_NAME(cmdtc->root) : GET_LEAF_ID (cmdtc->root),
            IS_PARAM_CMD(cmdtc->curr_param) ? 
                GET_CMD_NAME(cmdtc->curr_param) : GET_LEAF_ID (cmdtc->curr_param),
            param_top ? (IS_PARAM_CMD(param_top) ? 
                GET_CMD_NAME(param_top) : GET_LEAF_ID (param_top)) : NULL,            
           param_chkp ? (IS_PARAM_CMD(param_chkp) ? 
                GET_CMD_NAME(param_chkp) : GET_LEAF_ID (param_chkp)) : NULL,
            cmdtc->stack->top,
            tlv_top_name,
            cmdtc_get_state_str (cmdtc));
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
cmdtc_reset_cursor (cmd_tree_cursor_t *cmdtc) {

    cmd_tree_cursor_deinit (cmdtc);
    cmdtc->root = libcli_get_root_hook();
    cmdtc->curr_param = cmdtc->root;
}

void 
cmd_tree_cursor_deinit (cmd_tree_cursor_t *cmdtc) {

    glthread_t *curr;
    reset_stack (cmdtc->stack);
    cmdtc->stack_checkpoint = -1;
    reset_serialize_buffer (cmdtc->tlv_buffer);
    cmdtc->root = NULL;
    cmdtc->curr_param = NULL;
    cmdtc->icursor = 0;
    cmdtc->cmdtc_state = cmdt_cur_state_init;
    while ((dequeue_glthread_first (&cmdtc->matching_params_list)));
    cmdtc->leaf_param = NULL;
    cmdtc->success = false;
}

/* Fn to move the cmd tree cursor one level down the tree*/
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

bool 
cmdtc_is_this_config_command (cmd_tree_cursor_t *cmdtc) {

    assert (!isStackEmpty (cmdtc->stack));
    param_t *bottom_param = (param_t *)cmdtc->stack->slot[0];
    return (bottom_param == libcli_get_config_hook());
}

bool 
cmdtc_is_cursor_at_apex_root (cmd_tree_cursor_t *cmdtc) {

    bool rc;
    rc = (cmdtc->curr_param == libcli_get_root_hook ());
    if (rc) {
        assert (isStackEmpty (cmdtc->stack));
        assert (!serialize_buffer_get_size(cmdtc->tlv_buffer));
    }
    return rc;
}

/* Fn to move the cursor one level up in the cmd tree. Note that this fn is called to implement BackSpace and Page UP
In case of BackSpace, we would not like to lower down the checkpoints and update root
In case of of Page UP, we would like to update checkpoints as well as root. Hence, pass
boolean flags to control the relevant updates.
*/
int
cmd_tree_cursor_move_one_level_up (
            cmd_tree_cursor_t *cmdtc,
            bool honor_checkpoint,
            bool update_root)  {

    int count = 0;

    assert (cmdtc->cmdtc_type == cmdtc_type_cbc);

    switch (cmdtc->cmdtc_state) {
        case cmdt_cur_state_init:
            if (cmdtc->curr_param == libcli_get_root_hook()) return 0;
            assert (cmdtc->curr_param == (param_t *)StackGetTopElem(cmdtc->stack));
            if (honor_checkpoint) {
                if (cmdtc->stack_checkpoint == cmdtc->stack->top) return 0; 
            }
            /* Lower down the checkpoint of the stack if we are at checkpoint*/
            if (cmdtc->stack_checkpoint == cmdtc->stack->top) {
                cmdtc->stack_checkpoint--;
            }
            pop(cmdtc->stack);
            count = IS_PARAM_CMD (cmdtc->curr_param) ? \
                            cmdtc->curr_param->cmd_type.cmd->len : \
                            strlen(GET_LEAF_VALUE_PTR(cmdtc->curr_param));
            count += 1; /* +1 is to accomo*/
            cmdtc->curr_param = (isStackEmpty (cmdtc->stack)) ?  \
                libcli_get_root_hook() : (param_t *)StackGetTopElem(cmdtc->stack);
            
            /* This fn is called for PAGE_UP and BACKSPACE. We need to update root
                only in case of PAGE_UP only*/
            if (update_root) cmdtc->root = cmdtc->curr_param;

           /* Lower down the checkpoint of the serialized buffer. */
            if (serialize_buffer_get_checkpoint_offset (cmdtc->tlv_buffer) == 
                    serialize_buffer_get_size (cmdtc->tlv_buffer)) {
        
                serialize_buffer_skip (cmdtc->tlv_buffer, -1 * sizeof (tlv_struct_t));
                serialize_buffer_mark_checkpoint(cmdtc->tlv_buffer);
            }
            else {
                serialize_buffer_skip (cmdtc->tlv_buffer, -1 * sizeof (tlv_struct_t));
            }
        break;
        case cmdt_cur_state_multiple_matches:
            if (cmdtc->leaf_param) {
                memset (GET_LEAF_VALUE_PTR(cmdtc->leaf_param), 0, cmdtc->icursor);
                cmdtc->leaf_param = NULL;
            }
            while (dequeue_glthread_first(&cmdtc->matching_params_list));
            count = cmdtc->icursor ;
            cmdtc->icursor = 0;
            cmdtc->cmdtc_state =  cmdt_cur_state_init;
            if (count == 0) {
            /* User has not typed a single character while he has multiple options to choose
                from. In this case, move one level up*/
                return cmd_tree_cursor_move_one_level_up (cmdtc, honor_checkpoint, update_root);
            }
            break;        
        case cmdt_cur_state_single_word_match: 
        case cmdt_cur_state_matching_leaf:
            if (cmdtc->leaf_param) {
                memset (GET_LEAF_VALUE_PTR(cmdtc->leaf_param), 0, cmdtc->icursor);
                cmdtc->leaf_param = NULL;
            }
            while (dequeue_glthread_first(&cmdtc->matching_params_list));
            count = cmdtc->icursor ;
            cmdtc->icursor = 0;
            cmdtc->cmdtc_state =  cmdt_cur_state_init;
            if (isStackEmpty (cmdtc->stack)) {
                cmdtc->curr_param = libcli_get_root_hook();
                break;
            }
            assert (cmdtc->curr_param != (param_t *)StackGetTopElem(cmdtc->stack));
            cmdtc->curr_param = (param_t *)StackGetTopElem(cmdtc->stack);           
            break;
        case cmdt_cur_state_no_match:
            assert(0);
        default: ;
    }
    return count;
}

int
cmdtc_process_pageup_event (cmd_tree_cursor_t *cmdtc) {

    int count;

    if (isStackEmpty (cmdtc->stack)) return 0;
    count = cmd_tree_cursor_move_one_level_up (cmdtc, false, true);
    return count + 2;   /* 2 is to accomodate ''>" & hyphen sign*/
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
   int param_word_len;

    count = 0;
    param = NULL;
    init_glthread (&temp_list);

    ITERATE_GLTHREAD_BEGIN (param_list, curr) {

        param = glue_to_param (curr);
        param_word_len = GET_PARAM_CMD(param)->len;
        if (param_word_len != len ) {
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

/* This fn finds how many intial characters are common in all keywords present in
     cmdtc->matching_params_list starting from index start_index. This fn is READ-only fn */
static int
cmdtc_find_common_intial_lcs_len (glthread_t *param_list, int start_index) {

    int len, j;
    int count;
    param_t *param;
    param_t *fparam;
    glthread_t *curr;
    unsigned char ch, ch2;

    if (IS_GLTHREAD_LIST_EMPTY (param_list)) return 0;

    count = 0;
    fparam = glue_to_param (glthread_get_next (param_list));
    j = start_index;

    while(true) {

        ch = (unsigned char) GET_CMD_NAME (fparam)[j];

        ITERATE_GLTHREAD_BEGIN (param_list , curr) {

            param = glue_to_param (curr);
            ch2 =  (unsigned char) GET_CMD_NAME (param)[j];
            if (ch == ch2) continue;
            return count;

        } ITERATE_GLTHREAD_END (param_list , curr)
        
        count++;
        j++;
    }

    return count;
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

    attron (COLOR_PAIR(GREEN_ON_BLACK));

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

    attroff (COLOR_PAIR(GREEN_ON_BLACK));
    cli_printsc (cli_get_default_cli(), false);
    getyx(stdscr, row, col2);
    move (row, col1);
}

static cmdt_cursor_op_res_t
cmdt_cursor_process_space (cmd_tree_cursor_t *cmdtc) {

    int len;
    int  mcount;
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
                    return cmdt_cursor_no_match_further;
                }
            }

            else if (mcount >1 ) {

                /* We are here when user pressed ' ' and there are multiple matching params, and 
                    no leaf at this level. Auto-complete to the matching point amoing all params here*/
                len = cmdtc_find_common_intial_lcs_len (&cmdtc->matching_params_list, 0);

                /* Take any param from the list*/
                param = glue_to_param (glthread_get_next (&cmdtc->matching_params_list));

                /* Perform Auto - Complete*/
                while (cmdtc->icursor != len) {
                    cli_process_key_interrupt (
                        (int)GET_CMD_NAME(param)[cmdtc->icursor]);
                }
                cmdt_cursor_display_options (cmdtc);
                cmdtc->cmdtc_state = cmdt_cur_state_multiple_matches;
                return cmdt_cursor_no_match_further;
            }

            else if (mcount == 1) {

                /* Only one matching key-word, go for auto-completion now*/
                cmdtc->cmdtc_state = cmdt_cur_state_single_word_match;
                cmdtc->curr_param = glue_to_param (glthread_get_next (&cmdtc->matching_params_list));
                
                 /* Perform Auto - Complete*/
                while (GET_CMD_NAME(cmdtc->curr_param)[cmdtc->icursor] != '\0') {
                    cli_process_key_interrupt (
                        (int)GET_CMD_NAME(cmdtc->curr_param)[cmdtc->icursor]);
                }
                 cmd_tree_cursor_move_to_next_level (cmdtc);
                 return cmdt_cursor_done_auto_completion;
            }

        break;



        case cmdt_cur_state_multiple_matches:

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

            if (param) {
                cmdtc->curr_param = param;
                cmd_tree_cursor_move_to_next_level (cmdtc);
                return cmdt_cursor_ok;
            }

            /* Do auto completion until the tie point amonst several matching key-words*/
            len = cmdtc_find_common_intial_lcs_len (&cmdtc->matching_params_list, cmdtc->icursor);
            param = glue_to_param (glthread_get_next (&cmdtc->matching_params_list));
            while (len--) {
                cli_process_key_interrupt (
                        (int)GET_CMD_NAME(param)[cmdtc->icursor]);
            }
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

            /* Standard Validation Checks on Leaf */
            if (clistd_validate_leaf (cmdtc->curr_param) != LEAF_VALIDATION_SUCCESS) {
                return cmdt_cursor_no_match_further;
            }

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

    if ((c == KEY_ASCII_SPACE) || (c == KEY_ASCII_TAB)) {

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

Stack_t *
cmdtc_get_stack (cmd_tree_cursor_t *cmdtc) {
    return cmdtc->stack;
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

    /* Allow Mode when we are either in init state Or 
        in multiple match state but with i_cursor index as 0 which
        means, user has multiple options to choose from, but not yet
        typed a single character yet*/
    if ( ! ((cmdtc->cmdtc_state == cmdt_cur_state_init) ||
            (cmdtc->cmdtc_state == cmdt_cur_state_multiple_matches &&
                cmdtc->icursor == 0))) return;

    /* No point in going into Mode if we are at bottom of the cmd tree*/
    if (cmdtc_is_cursor_at_bottom_mode_node (cmdtc)) return;

    /* Drain the complete stack temporarily, we will rebuilt it as it is*/
    while ((param = (param_t *)pop (cmdtc->stack))) {
        assert (!IS_QUEUED_UP_IN_THREAD (&param->glue));
        glthread_add_next (&temp_list, &param->glue);
    }

    /* Now prepare the new CLI hdr which is DEF HDR + 
        path from root to current level in cmd-tree*/
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

    /* Adjust the prompt characters */
    buffer[byte_count - 1] = '>';
    buffer[byte_count] = ' ';
    byte_count++;

    cli_set_hdr (cli, NULL, byte_count);

    /* Rebuild  the stack again, thanks you stack !*/
    while ((curr = dequeue_glthread_first (&temp_list))) {

         param = glue_to_param (curr);
         push (cmdtc->stack , (void *)param);
    }

    /* Save the context of where we are now in CLI tree by updating the root,
        and checkpoint the stack and TLV buffer  */
    cmdtc->root = cmdtc->curr_param;
    serialize_buffer_mark_checkpoint (cmdtc->tlv_buffer);
    cmdtc->stack_checkpoint = cmdtc->stack->top;

    /* Finally display the new  prompt to the user */
    cli_printsc (cli, true);
}

/* This fn resets the cmd tree cursor back to pavilion to be
    ready to process next command*/
void 
cmd_tree_cursor_reset_for_nxt_cmd (cmd_tree_cursor_t *cmdtc) {

    param_t *param;

    assert(cmdtc->stack->top >= cmdtc->stack_checkpoint);

    /* Restore the stack to the checkpoint */
    while (cmdtc->stack->top > cmdtc->stack_checkpoint) {

        param = (param_t *)pop(cmdtc->stack);

        if (IS_PARAM_LEAF(param)) {
            memset (GET_LEAF_VALUE_PTR(param), 0, LEAF_VALUE_HOLDER_SIZE);
        }
    }

    /* Restore the serialized buffer to the checkpoint state */
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
        attron(COLOR_PAIR(RED_ON_BLACK));
        printw("\nError : Incomplete CLI...");
        attroff(COLOR_PAIR(RED_ON_BLACK));
        return;
    }
    cmdtc->success = true;

    int tlv_buffer_original_size = serialize_buffer_get_size (cmdtc->tlv_buffer);
    int tlv_buffer_checkpoint_offset = serialize_buffer_get_checkpoint_offset (cmdtc->tlv_buffer);

    if (cmdtc_is_this_config_command (cmdtc)) {
        /* ToDo : Support Command Negation !*/
        enable_or_diable = CONFIG_ENABLE;
    }
    else {
        enable_or_diable = OPERATIONAL;
    }

    /* Handle Trigger of Operational Cmd*/
    if (enable_or_diable == OPERATIONAL) {

        param = (param_t *)StackGetTopElem(cmdtc->stack);
        if (param->callback (param, cmdtc->tlv_buffer, enable_or_diable)) {
            cmdtc->success = false;
            return;
        }
        return;
    }

    /* Handle Trigger of Config Command. Config Commands are triggered in
        patches !*/
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
            /* Standard Validation Checks on Leaf */
            if (clistd_validate_leaf (cmdtc->curr_param) != LEAF_VALIDATION_SUCCESS) {

                attron(COLOR_PAIR(RED_ON_BLACK));
                printw ("\nError : value %s do not comply with expected data type : %s", 
                    GET_LEAF_VALUE_PTR (cmdtc->curr_param),
                    GET_LEAF_TYPE_STR(cmdtc->curr_param));
                attroff(COLOR_PAIR(RED_ON_BLACK));
                memset (GET_LEAF_VALUE_PTR(cmdtc->curr_param), 0, LEAF_VALUE_HOLDER_SIZE);
                cmd_tree_cursor_reset_for_nxt_cmd (cmdtc);
                return;
            }            
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
    int str_len_param = param->cmd_type.cmd->len;

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
    scanw("%d", &choice);
    
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

    cli_sanity_check (cli);
    
    cmdtc = cmdtc_tree_get_cursor (cmdtc_type_wbw);
   
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
            attron(COLOR_PAIR(RED_ON_BLACK));
            printw ("\nCLI Error : Unrecognized Param : %s", *(tokens + i));
            attroff(COLOR_PAIR(RED_ON_BLACK));
            cmd_tree_cursor_reset_for_nxt_cmd(cmdtc);
            return false;
        }

        memset(&tlv, 0, sizeof(tlv_struct_t));

        if (IS_PARAM_LEAF(param)) {

            /* Temporarily store leaf value in param, as in line-mode we dont really store CLI
                values in CmTree params*/
            strncpy (GET_LEAF_VALUE_PTR (param), *(tokens + i), strlen (*(tokens + i)));

            if (clistd_validate_leaf (param) != LEAF_VALIDATION_SUCCESS) {

                memset (GET_LEAF_VALUE_PTR (param), 0, LEAF_VALUE_HOLDER_SIZE);
                attron(COLOR_PAIR(RED_ON_BLACK));
                printw ("\nError : value %s do not comply with expected data type : %s", 
                    *(tokens + i),
                    GET_LEAF_TYPE_STR(param));
                attroff(COLOR_PAIR(RED_ON_BLACK));
                cmd_tree_cursor_reset_for_nxt_cmd (cmdtc);
                return false;
            } 

            memset (GET_LEAF_VALUE_PTR (param), 0, LEAF_VALUE_HOLDER_SIZE);

            if (param->cmd_type.leaf->user_validation_cb_fn &&
                (param->cmd_type.leaf->user_validation_cb_fn(
                    cmdtc->tlv_buffer, (unsigned char *)*(tokens + i)) == LEAF_VALIDATION_FAILED)) {
                
                attron(COLOR_PAIR(RED_ON_BLACK));
                printw ("\nCLI Error : User Validation Failed for value : %s", *(tokens + i));
                attroff(COLOR_PAIR(RED_ON_BLACK));
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

    attron (COLOR_PAIR(GREEN_ON_BLACK));
    printw ("\nParse Success\n");
    attroff (COLOR_PAIR(GREEN_ON_BLACK));
    cli_record_copy (cli_get_default_history(), cli);
}

void
cmdtc_display_all_complete_commands (cmd_tree_cursor_t *cmdtc) {

        cmd_tree_display_all_complete_commands (cmdtc->curr_param, 0);
 }