#include <stddef.h>
#include <stdint.h>
#include <ncurses.h>
#include <stdlib.h>
#include <memory.h>
#include <assert.h>
#include "CmdTree.h"
#include "CmdTreeCursor.h"
#include "../string_util.h"
#include "cmdcodes_def.h"
#include "clistd.h"
#include "../cmdtlv.h"

/*Default zero level commands hooks. */
static param_t root;
static param_t show;
static param_t debug;
static param_t config;
static param_t clearp;
static param_t run;

static param_t *universal_params[] = {&show, &config};

void
init_param(param_t *param,    
           param_type_t param_type,    
           const char *cmd_name,    
           cmd_callback callback,
           user_validation_callback user_validation_cb_fn,
           leaf_type_t leaf_type,
           const char *leaf_id,
           const char *help);

void 
libcli_register_param(param_t *parent, param_t *child);

void 
set_param_cmd_code(param_t *param, int cmd_code) ;

void
init_param(param_t *param,    
           param_type_t param_type,    
           const char *cmd_name,    
           cmd_callback callback,
           user_validation_callback user_validation_cb_fn,
           leaf_type_t leaf_type,
           const char *leaf_id,
           const char *help) {

    int i = 0;
    if (param_type == CMD)
    {   
        GET_PARAM_CMD(param) = (cmd_t *)calloc(1, sizeof(cmd_t));
        param->param_type = CMD;
        strncpy((char *)GET_CMD_NAME(param), cmd_name, MIN(CMD_NAME_SIZE, strlen(cmd_name)));
        GET_CMD_NAME(param)[CMD_NAME_SIZE - 1] = '\0';
        GET_PARAM_CMD(param)->len = strlen (GET_CMD_NAME(param));
    }
    else if (param_type == LEAF)
    {
        GET_PARAM_LEAF(param) = (leaf_t *)calloc(1, sizeof(leaf_t));
        param->param_type = LEAF;
        GET_PARAM_LEAF(param)->leaf_type = leaf_type;
        param->cmd_type.leaf->user_validation_cb_fn = user_validation_cb_fn;
        strncpy((char *)GET_LEAF_ID(param), leaf_id, MIN(LEAF_ID_SIZE, strlen(leaf_id)));
        GET_LEAF_ID(param)[LEAF_ID_SIZE - 1] = '\0';
    }
    else if (param_type == NO_CMD)
    {   
        GET_PARAM_CMD(param) = (cmd_t *)calloc(1, sizeof(cmd_t));
        param->param_type = NO_CMD;
        memcpy(GET_CMD_NAME(param), NEGATE_CHARACTER, strlen(NEGATE_CHARACTER));
        GET_CMD_NAME(param)[CMD_NAME_SIZE - 1] = '\0';
    }

    param->callback = callback;

    strncpy(GET_PARAM_HELP_STRING(param), help, MIN(PARAM_HELP_STRING_SIZE, strlen(help)));
    GET_PARAM_HELP_STRING(param)[PARAM_HELP_STRING_SIZE - 1] = '\0';
    param->disp_callback = NULL;

    for (; i < MAX_OPTION_SIZE; i++) {   
        param->options[i] = NULL;
    }

    param->CMDCODE = -1;
    init_glthread (&param->glue);
}


void 
libcli_register_param(param_t *parent, param_t *child) {

    int i = 0;

    if (!parent) parent = libcli_get_root_hook();

    for (i = CHILDREN_START_INDEX; i <= CHILDREN_END_INDEX; i++) {
        if (parent->options[i])
            continue;
        parent->options[i] = child;
        return;
    }   
    assert(0);
}

void 
set_param_cmd_code(param_t *param, int cmd_code) {

    if (param->callback == NULL)
        assert(0);
    param->CMDCODE = cmd_code;
}

static void 
 libcli_build_default_cmdtree() {

    param_t *root_hook = libcli_get_root_hook();
    init_param(root_hook, CMD, "ROOT", 0, 0, INVALID, 0, "ROOT");

    param_t *chook = libcli_get_show_hook();
    init_param(chook, CMD, "show", 0, 0, INVALID, 0, "show cmds");
    libcli_register_param (root_hook, chook);

    chook = libcli_get_config_hook();
    init_param(chook, CMD, "config", NULL, 0, INVALID, 0, "config cmds");
    libcli_register_param (root_hook, chook);

    chook = libcli_get_debug_hook();
    init_param(chook, CMD, "debug", 0, 0, INVALID, 0, "debug cmds");
    libcli_register_param (root_hook, chook);

    chook = libcli_get_clear_hook();
    init_param(chook, CMD, "clear", 0, 0, INVALID, 0, "clear cmds");
    libcli_register_param (root_hook, chook);

    chook = libcli_get_run_hook();
    init_param(chook, CMD, "run", 0, 0, INVALID, 0, "run cmds");
    libcli_register_param (root_hook, chook);

    param_t *hook = libcli_get_config_hook();

    {
         /* config host-name <name>*/
         /* config host-name ...*/
        static param_t hostname;
        init_param (&hostname, CMD, "host-name", NULL, NULL, INVALID, NULL, "host-name");
        libcli_register_param (hook, &hostname);
        {
            /* config host-name <name> */
            static param_t name;
            init_param(&name, LEAF, NULL, clistd_config_device_default_handler, NULL, STRING, "host-name", "Host Name");
            libcli_register_param(&hostname, &name);
            set_param_cmd_code(&name, CONFIG_DEVICE_HOSTNAME);
        }
    }

    hook = libcli_get_show_hook();

    {
        /*show help*/
        static param_t help;
        init_param (&help, CMD, "help", show_help_handler, NULL, INVALID, NULL, "KYC (Know Your CLI)");
        libcli_register_param(hook, &help);
        set_param_cmd_code(&help, SHOW_CLI_HELP);
    }
    {
        /*show history*/
        static param_t history;
        init_param (&history, CMD, "history", show_history_handler, NULL, INVALID, NULL, "CLI history");
        libcli_register_param(hook, &history);
        set_param_cmd_code(&history, SHOW_CLI_HISTORY);        
    }
 }

void 
cmd_tree_init () {

    init_token_array();
    libcli_build_default_cmdtree();
    cmd_tree_init_cursors () ;
}

/* Function to be used to get access to above hooks*/

param_t *
libcli_get_root_hook(void)
{
    return &root;
}

param_t *
libcli_get_show_hook(void)
{
    return &show;
}

param_t *
libcli_get_debug_hook(void)
{
    return &debug;
}

param_t *
libcli_get_config_hook(void)
{
    return &config;
}

param_t *
libcli_get_clear_hook(void)
{
    return &clearp;
}

param_t *
libcli_get_run_hook(void)
{
    return &run;
}

bool
cmd_tree_leaf_char_save (param_t *leaf_param, unsigned char c, int index) {

    assert (IS_PARAM_LEAF (leaf_param));
    if (index == LEAF_VALUE_HOLDER_SIZE) return false;
    GET_LEAF_VALUE_PTR(leaf_param)[index] = c;
    return true;
}


void 
cmd_tree_collect_param_tlv (param_t *param, ser_buff_t *ser_buff) {

     tlv_struct_t tlv;

     memset(&tlv, 0, sizeof(tlv_struct_t));

    if (IS_PARAM_CMD (param)) {
        
        tlv.tlv_type = TLV_TYPE_CMD_NAME;
        tlv.leaf_type = STRING;
        put_value_in_tlv((&tlv), GET_CMD_NAME(param));
        collect_tlv(ser_buff, &tlv);
    }
    else {

        prepare_tlv_from_leaf(GET_PARAM_LEAF(param), (&tlv));
        put_value_in_tlv((&tlv), GET_LEAF_VALUE_PTR(param));
        collect_tlv(ser_buff, &tlv);
    }
}

static unsigned char temp[ LEAF_ID_SIZE + 2];
void
cmd_tree_display_all_complete_commands(
                param_t *root, unsigned int index, bool is_nested_mode) {

        if (!root)
            return;

        if (0 && param_is_hook (root) && is_nested_mode) return;

        if (IS_PARAM_CMD(root)){
            untokenize(index);
            tokenize(GET_CMD_NAME(root), GET_PARAM_CMD(root)->len, index);
        }

        else if (IS_PARAM_LEAF(root)){
            untokenize(index);
            memset(temp, 0, sizeof(temp));
            sprintf((char *)temp, "<%s>", GET_LEAF_ID(root));
            tokenize((char *)temp, strlen(GET_LEAF_ID(root)) + 2, index);
        }   

        unsigned int i = CHILDREN_START_INDEX;

        for ( ; i <= CHILDREN_END_INDEX; i++)
            cmd_tree_display_all_complete_commands(
                    root->options[i], index+1, is_nested_mode);
    
        if (root->callback){
            print_tokens(index + 1); 
            printw("\n");
        }   
}

void 
cmd_tree_install_universal_params (param_t *param, param_t *branch_hook) {

    int i = 0, j = 0;
    int k = sizeof (universal_params) / sizeof(universal_params[0]);
    
    while (true) {

        if (i > CHILDREN_END_INDEX) return;

        if (param->options[i]) {
            i++;
            continue;
        }

        if (universal_params[j] == branch_hook) j++;
        if (j == k) return;
        param->options[i++] = universal_params[j++]; 
        if (j == k) return;
    }
}

void 
cmd_tree_uninstall_universal_params (param_t *param) {

    int i, j;
    int k = sizeof (universal_params) / sizeof(universal_params[0]);

    for (i = CHILDREN_START_INDEX; i <= CHILDREN_END_INDEX; i++) {
        if (!param->options[i]) continue;
        for ( j = 0; j < k; j++) {
            if (param->options[i] == universal_params[j]) {
                param->options[i] = NULL;
                break;
            }
        }
    }
}

bool 
param_is_hook (param_t *param) {

if (    param == libcli_get_config_hook () ||
         param == libcli_get_clear_hook () ||
         param == libcli_get_show_hook () ||
         param == libcli_get_run_hook () ||
         param == libcli_get_debug_hook () ) {
    
    return true;
}

    return false;
}

bool 
cmd_tree_is_token_a_hook (char *token) {

    param_t *root = libcli_get_root_hook ();

    return (cmd_tree_find_matching_param (&root->options[0], token) != NULL); 

}

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
cmd_tree_find_matching_param (param_t **options, const char *cmd_name){
    
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