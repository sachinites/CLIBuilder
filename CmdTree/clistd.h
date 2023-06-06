#ifndef __CLISTD__ 
#define __CLISTD__

#include "../cli_const.h"

typedef struct leaf leaf_t;
typedef struct _param_t_ param_t;

typedef struct serialized_buffer ser_buff_t;

int
config_device_default_handler (param_t *param, ser_buff_t *b, op_mode enable_or_disable);

extern CLI_VAL_RC clistd_validate_leaf (param_t *param);

#endif