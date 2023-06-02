#ifndef __CLICNST__
#define __CLICNST__

#include <stddef.h>

#define MAX_COMMAND_LENGTH 256

#define DEF_CLI_HDR  "Soft-Firewall>$ "
#define CMD_NAME_SIZE   64
#define LEAF_VALUE_HOLDER_SIZE 64
#define LEAF_ID_SIZE    32
#define PARAM_HELP_STRING_SIZE 64
#define MAX_OPTION_SIZE 16
#define MAX_CMD_TREE_DEPTH 16
#define CHILDREN_START_INDEX    0
#define CHILDREN_END_INDEX      (MAX_OPTION_SIZE -1)




#define KEY_ASCII_TAB   9
#define KEY_ASCII_NEWLINE 10
#define CLI_HISTORY_LIMIT   10


#define NEGATE_CHARACTER "no"

typedef enum{
    CONFIG_DISABLE,
    CONFIG_ENABLE,
    OPERATIONAL,
    MODE_UNKNOWN
} op_mode;

typedef enum{
    INT,
    STRING,
    IPV4,
    FLOAT,
    IPV6,
    BOOLEAN,
    INVALID,
    LEAF_MAX
} leaf_type_t;

static const char *
get_str_leaf_type(leaf_type_t leaf_type)
{

    switch (leaf_type)
    {   
    case INT:
        return "INT";
    case STRING:
        return "STRING";
    case IPV4:
        return "IPV4";
    case FLOAT:
        return "FLOAT";
    case IPV6:
        return "IPV6";
    case BOOLEAN:
        return "BOOLEAN";
    case LEAF_MAX:
        return "LEAF_MAX";
    default:
        return "Unknown";
    }   
    return NULL;
}

typedef enum{
    VALIDATION_FAILED = -1, 
    VALIDATION_SUCCESS
} CLI_VAL_RC;

#endif 