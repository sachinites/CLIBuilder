#include <string.h>
#include "clistd.h"
#include "CmdTree.h"
#include "../KeyProcessor/KeyProcessor.h"
#include "../cmdtlv.h"
#include "../string_util.h"

/* Standard Validations */

typedef CLI_VAL_RC (*leaf_type_handler)(param_t *param,  char *value_passed);

static CLI_VAL_RC
ipv4_validation_handler (param_t *leaf, char *value_passed){

    char *token, *rest;
    int num, dotCount = 0;

    // Make a copy of the original string
    char *ipCopy = strdup(value_passed);

    // Tokenize the IP address string using the dot delimiter
    token = strtok_r(ipCopy, ".", &rest);

    while (token != NULL) {

        dotCount++;

        // Check if the token is a valid number
        if (sscanf(token, "%d", &num) != 1 || num < 0 || num > 255) {
            free(ipCopy);
            return VALIDATION_FAILED;
        }

        token = strtok_r(NULL, ".", &rest);
    }

    free(ipCopy);

    // Check if there are exactly 4 parts separated by dots
    if (dotCount != 4)
        return VALIDATION_FAILED;

    return VALIDATION_SUCCESS;
}

static CLI_VAL_RC
ipv6_validation_handler(param_t *leaf, char *value_passed){

     return VALIDATION_SUCCESS;
}

static CLI_VAL_RC
int_validation_handler(param_t *leaf, char *value_passed){

     return VALIDATION_SUCCESS;
}

static CLI_VAL_RC
string_validation_handler(param_t *leaf, char *value_passed){

     return VALIDATION_SUCCESS;
}

static CLI_VAL_RC
float_validation_handler(param_t *leaf, char *value_passed){

     return VALIDATION_SUCCESS;
}

static CLI_VAL_RC 
boolean_validation_handler(param_t *leaf, char *value_passed){

     return VALIDATION_SUCCESS;
}

static leaf_type_handler leaf_handler_array[LEAF_TYPE_MAX] = {

    int_validation_handler,
    string_validation_handler,
    ipv4_validation_handler,
    float_validation_handler,
    ipv6_validation_handler,
    boolean_validation_handler,
    NULL
};

extern CLI_VAL_RC
clistd_validate_leaf (param_t *param) {

    return leaf_handler_array[GET_LEAF_TYPE(param)](param, GET_LEAF_VALUE_PTR (param));
}


/* Standard Validations */

int
config_device_default_handler (param_t *param, ser_buff_t *ser_buff, op_mode enable_or_disable) {

    tlv_struct_t *tlv = NULL;
    
    TLV_LOOP_BEGIN(ser_buff, tlv){

        if(enable_or_disable == CONFIG_ENABLE) {
            cli_set_hdr (cli_get_default_cli(), tlv->value, strlen ((const char *)tlv->value));
        }
        else{
            cli_set_hdr (cli_get_default_cli(),  (unsigned char *)DEF_CLI_HDR, strlen ((const char *)DEF_CLI_HDR));
        }

    }TLV_LOOP_END;

    return 0;
}

static void
_dump_one_cmd(param_t *param, unsigned short tabs)
{

    int i = 0;

    PRINT_TABS(tabs);

    if (IS_PARAM_CMD(param) || IS_PARAM_NO_CMD(param))
        printw("-->%s(%d)", GET_CMD_NAME(param), tabs);
    else
        printw("-->%s(%d)", GET_LEAF_TYPE_STR(param), tabs);

    for (; i < MAX_OPTION_SIZE; i++)
    {
        if (param->options[i])
        {
            printw("\n");
            _dump_one_cmd(param->options[i], ++tabs);
            --tabs;
        }
        else
            break;
    }
}

static void dump_cmd_tree()
{
    _dump_one_cmd(libcli_get_root_hook(), 0);
}

int
show_cmd_tree(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){

        dump_cmd_tree();
        return 0;
}