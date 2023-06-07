#ifndef __CLISTD__ 
#define __CLISTD__

#include "../cli_const.h"

typedef struct leaf leaf_t;
typedef struct _param_t_ param_t;

typedef struct serialized_buffer ser_buff_t;

extern leaf_validation_rc_t clistd_validate_leaf (param_t *param);

int
clistd_config_device_default_handler (param_t *param, ser_buff_t *b, op_mode enable_or_disable);

int
show_help_handler (param_t *param, ser_buff_t *ser_buff, op_mode enable_or_disable) ;

#endif