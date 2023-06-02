#include <stddef.h>
#include <stdint.h>
#include <ncurses.h>
#include <stdlib.h>
#include <memory.h>
#include <assert.h>
#include "CmdTree.h"
#include "../stack/stack.h"
#include "../serializer/serialize.h"
#include "../string_util.h"
#include "cmdcodes_def.h"
#include "clistd.h"

typedef struct cmd_ctx_ {

    Stack_t *stack;
    ser_buff_t *tlv_buffer;
    param_t *cursor;
} cmd_ctx_t;

static cmd_ctx_t ctx;
static cmd_ctx_t nexted_ctx;

/*Default zero level commands hooks. */
static param_t root;
static param_t show;
static param_t debug;
static param_t config;
static param_t clearp;
static param_t run;

#define MIN(a,b)    (a < b ? a : b)


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
        GET_CMD_NAME(param)
        [CMD_NAME_SIZE - 1] = '\0';
    }   
    else if (param_type == LEAF)
    {   
        GET_PARAM_LEAF(param) = (leaf_t *)calloc(1, sizeof(leaf_t));
        param->param_type = LEAF;
        GET_PARAM_LEAF(param)->leaf_type = leaf_type;
        param->cmd_type.leaf->user_validation_cb_fn = user_validation_cb_fn;
        strncpy((char *)GET_LEAF_ID(param), leaf_id, MIN(LEAF_ID_SIZE, strlen(leaf_id)));
        GET_LEAF_ID(param)
        [LEAF_ID_SIZE - 1] = '\0';
    }   
    else if (param_type == NO_CMD)
    {   
        GET_PARAM_CMD(param) = (cmd_t *)calloc(1, sizeof(cmd_t));
        param->param_type = NO_CMD;
        memcpy(GET_CMD_NAME(param), NEGATE_CHARACTER, strlen(NEGATE_CHARACTER));
        GET_CMD_NAME(param)
        [CMD_NAME_SIZE - 1] = '\0';
    }   

    param->callback = callback;

    strncpy(GET_PARAM_HELP_STRING(param), help, MIN(PARAM_HELP_STRING_SIZE, strlen(help)));
    GET_PARAM_HELP_STRING(param)
    [PARAM_HELP_STRING_SIZE - 1] = '\0';
    param->disp_callback = NULL;

    for (; i < MAX_OPTION_SIZE; i++) {   
        param->options[i] = NULL;
    }   

    param->CMDCODE = -1;
}

void 
libcli_register_param(param_t *parent, param_t *child) {

    int i = 0;

    if (!parent) parent = libcli_get_root();

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

    param_t *root_hook = libcli_get_root();

    libcli_register_param (root_hook, libcli_get_show_hook());
    libcli_register_param (root_hook, libcli_get_config_hook());
    libcli_register_param (root_hook, libcli_get_debug_hook());
    libcli_register_param (root_hook, libcli_get_clear_hook());
    libcli_register_param (root_hook, libcli_get_run_hook());

   
    param_t *config_hook = libcli_get_config_hook();

    {
         /* config host-name <name>*/
         /* config host-name ...*/
        static param_t hostname;
        init_param (&hostname, CMD, "host-name", NULL, NULL, INVALID, NULL, "host-name");
        libcli_register_param (config_hook, &hostname);
        {
            /* config host-name <name> */
            static param_t name;
            init_param(&name, LEAF, NULL, config_device_default_handler, NULL, STRING, "host-name", "Host Name");
            libcli_register_param(&hostname, &name);
            set_param_cmd_code(&name, CONFIG_DEVICE_HOSTNAME);
        }
    }
 }


static void 
cmd_tree_init () {

    static bool initialized = false; 

    if (initialized) return;

    ctx.stack = get_new_stack ();
    init_token_array();
    init_serialized_buffer (&ctx.tlv_buffer);
    ctx.cursor = libcli_get_root();
    libcli_build_default_cmdtree();
}

/* Function to be used to get access to above hooks*/

param_t *
libcli_get_root(void)
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



static cmd_tree_parse_res_t 
cmd_tree_parse_command_internal (char **tokens, int token_count) {

    cmd_tree_parse_res_t rc = cmd_tree_parse_app_accepted;

    return rc;
}

cmd_tree_parse_res_t
cmd_tree_parse_command (unsigned char *command, int size) {

    char** tokens = NULL;
    int token_count = 0;

    cmd_tree_parse_res_t rc = cmd_tree_parse_app_accepted;
    
    cmd_tree_init () ;

    re_init_tokens (MAX_CMD_TREE_DEPTH);

    tokens = tokenizer(command, ' ', &token_count);

    if (!token_count) return cmd_tree_parse_invalid_cmd;

    rc = cmd_tree_parse_command_internal (tokens, token_count);

    return rc;
}