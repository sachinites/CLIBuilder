#ifndef __CMDTREE__
#define __CMDTREE__

#include <stdbool.h>
#include "../cli_const.h"
#include "../gluethread/glthread.h"

typedef struct _param_t_ param_t;
typedef struct serialized_buffer ser_buff_t;


typedef int (*user_validation_callback)(ser_buff_t *, unsigned char *leaf_value);
typedef void (*display_possible_values_callback)(param_t *, ser_buff_t *);
typedef int (*cmd_callback)(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_diable);


typedef struct cmd{
    char cmd_name[CMD_NAME_SIZE];
} cmd_t;

typedef struct leaf{
    leaf_type_t leaf_type;
    char value_holder[LEAF_VALUE_HOLDER_SIZE];
    user_validation_callback user_validation_cb_fn;
    char leaf_id[LEAF_ID_SIZE];/*Within a single command, it should be unique*/
} leaf_t;

typedef enum{
    CMD,
    LEAF,
    NO_CMD
} param_type_t;

typedef union _param_t{
    cmd_t *cmd;
    leaf_t *leaf;
} _param_t;

struct _param_t_{
    param_type_t param_type;
    _param_t cmd_type;
    cmd_callback callback;
    char help[PARAM_HELP_STRING_SIZE];
    struct _param_t_ *options[MAX_OPTION_SIZE];
    display_possible_values_callback disp_callback;
    int CMDCODE;
    glthread_t glue;
};
GLTHREAD_TO_STRUCT (glue_to_param, param_t, glue);

typedef CLI_VAL_RC (*leaf_type_handler)(leaf_t *leaf, char *value_passed);

#define GET_PARAM_CMD(param)    (param->cmd_type.cmd)
#define GET_PARAM_LEAF(param)   (param->cmd_type.leaf)
#define IS_PARAM_NO_CMD(param)  (param->param_type == NO_CMD)
#define IS_PARAM_CMD(param)     (param->param_type == CMD)
#define IS_PARAM_LEAF(param)    (param->param_type == LEAF)
#define GET_LEAF_TYPE_STR(param)    (get_str_leaf_type(GET_PARAM_LEAF(param)->leaf_type))
#define GET_LEAF_VALUE_PTR(param)   (GET_PARAM_LEAF(param)->value_holder)
#define GET_LEAF_TYPE(param)        (GET_PARAM_LEAF(param)->leaf_type)
#define GET_CMD_NAME(param)         (GET_PARAM_CMD(param)->cmd_name)
#define GET_PARAM_HELP_STRING(param) (param->help)
#define GET_LEAF_ID(param)          (GET_PARAM_LEAF(param)->leaf_id)

void 
cmd_tree_init ();

/* Function to be used to get access to above hooks*/

param_t *
libcli_get_root_hook(void);

param_t *
libcli_get_show_hook(void);

param_t *
libcli_get_debug_hook(void);

param_t *
libcli_get_config_hook(void);

param_t *
libcli_get_clear_hook(void);

param_t *
libcli_get_run_hook(void);

bool
cmd_tree_leaf_char_save (param_t *leaf_param, unsigned char, int index);

void 
cmd_tree_collect_param_tlv (param_t *param, ser_buff_t *ser_buff);

#endif 