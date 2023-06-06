#include <string.h>
#include <arpa/inet.h>
#include <ctype.h>
#include "clistd.h"
#include "CmdTree.h"
#include "../KeyProcessor/KeyProcessor.h"
#include "../cmdtlv.h"
#include "../string_util.h"

/* Standard Validations Begin*/

typedef CLI_VAL_RC (*leaf_type_handler)(param_t *param,  char *value_passed);

static int
ip_version(const char *src) {
    char buf[16];
    if (inet_pton(AF_INET, src, buf)) {
        return 4;
    } else if (inet_pton(AF_INET6, src, buf)) {
        return 6;
    }
    return -1;
}

static CLI_VAL_RC
ipv4_validation_handler (param_t *leaf, char *value_passed){

    if (ip_version ((const char *)value_passed) == 4) 
        return VALIDATION_SUCCESS;
    return VALIDATION_FAILED;
}

static CLI_VAL_RC
ipv6_validation_handler(param_t *leaf, char *value_passed){

    if (ip_version ((const char *)value_passed) == 6) 
        return VALIDATION_SUCCESS;
    return VALIDATION_FAILED;
}

static CLI_VAL_RC
int_validation_handler(param_t *leaf, char *value_passed){

    if (value_passed == NULL || *value_passed == '\0')
        return VALIDATION_FAILED; 

    // Check if the first character is a valid sign (+ or -)
    if (*value_passed == '+' || *value_passed == '-')
        value_passed++; // Skip the sign

    // Check each remaining character
    while (*value_passed != '\0') {

        if (!isdigit(*value_passed))
            return VALIDATION_FAILED;

        value_passed++;
    }

    return VALIDATION_SUCCESS;
}

static CLI_VAL_RC
string_validation_handler(param_t *leaf, char *value_passed){

     return VALIDATION_SUCCESS;
}

static int 
isFloat(const char *input) {
    // Check if the input is empty
    if (input == NULL || *input == '\0')
        return 0; // Not a float

    // Check if the first character is a valid sign (+ or -)
    if (*input == '+' || *input == '-')
        input++; // Skip the sign

    int dotCount = 0;

    // Check each remaining character
    while (*input != '\0') {
        if (*input == '.') {
            dotCount++;

            // Check if there is more than one dot
            if (dotCount > 1)
                return 0; // Not a float
        }
        else if (!isdigit(*input))
            return 0; // Not a float

        input++;
    }

    // Check if the float ends with a dot
    if (*(input - 1) == '.')
        return 0; // Not a float

    return 1; // Input is a float
}

static CLI_VAL_RC
float_validation_handler(param_t *leaf, char *value_passed){

     if (isFloat ((const char *)value_passed) == 1) {
        return VALIDATION_SUCCESS;
     }
     return VALIDATION_FAILED;
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


/* Standard Validations End */

int
clistd_config_device_default_handler (param_t *param, ser_buff_t *ser_buff, op_mode enable_or_disable) {

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

int
show_help_handler(param_t *param, ser_buff_t *b, op_mode enable_or_disable){

    printw("\nWelcome to Help Wizard\n");
    printw("========================\n");
    printw("1. Use %c Character after the command to enter command mode\n", MODE_CHARACTER);
    printw("2. Use %c Character after the command to see possible follow up suboptions\n", SUBOPTIONS_CHARACTER);
    printw("3. Use %c Character after the command to see possible complete command completions\n", CMD_EXPANSION_CHARACTER);
    printw("4. [ ctrl + l ] - clear screen\n");
    printw("5. [ ctrl + t ] - jump to top of cmd tree\n");
    printw("6. [ BackSpace ] - jump one level up of command tree\n");
    printw("7. config [ %s ] console name <console name> - set/unset new console name\n", NEGATE_CHARACTER);
    printw("8. [UP DOWN Arrow] - show the command history\n");
    attron(COLOR_PAIR(PLAYER_PAIR));
    printw( "          Author : Abhishek Sagar\n");
    printw( "          Visit : www.csepracticals.com for more courses and projects\n");
    attroff(COLOR_PAIR(PLAYER_PAIR));
    return 0;
}