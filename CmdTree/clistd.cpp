#include "clistd.h"
#include "../cmdtlv.h"
#include "../KeyProcessor/KeyProcessor.h"

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
