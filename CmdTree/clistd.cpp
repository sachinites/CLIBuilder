#include "clistd.h"
#include "CmdTree.h"
#include "../KeyProcessor/KeyProcessor.h"
#include "../cmdtlv.h"
#include "../string_util.h"

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