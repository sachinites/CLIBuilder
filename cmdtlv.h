/*
 * =====================================================================================
 *
 *       Filename:  cmdtlv.h
 *
 *    Description:  TLV implementation on top of serialized library
 *
 *        Version:  1.0
 *        Created:  Friday 04 August 2017 03:59:45  IST
 *       Revision:  1.0
 *       Compiler:  gcc
 *
 *         Author:  Er. Abhishek Sagar, Networking Developer (AS), sachinites@gmail.com
 *        Company:  Brocade Communications(Jul 2012- Mar 2016), Current : Juniper Networks(Apr 2017 - Present)
 *
 * =====================================================================================
 */


#ifndef __CMDTLV__H
#define __CMDTLV__H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <ncurses.h>
#include "serializer/serialize.h"
#include "cli_const.h"
#include "string_util.h"

#pragma pack (push,1)
typedef struct tlv_struct{
    uint8_t tlv_type;
    leaf_type_t leaf_type;
    unsigned char leaf_id[LEAF_ID_SIZE];
    unsigned char value[LEAF_VALUE_HOLDER_SIZE];
} tlv_struct_t;
#pragma pack(pop)

#define TLV_LOOP_BEGIN(ser_buff, tlvptr)                                                \
{                                                                                       \
    assert(ser_buff);                                                                   \
    tlvptr = (tlv_struct_t *)(ser_buff->b);                                             \
    unsigned int _i = 0, k = serialize_buffer_get_size(ser_buff)/sizeof(tlv_struct_t);   \
    for(; _i < k; _i++, tlvptr++)

#define TLV_LOOP_END    }

#define tlv_copy_leaf_id(tlvptr, dst)                          \
    strncpy((char *)dst, tlvptr->leaf_id, strlen(tlvptr->leaf_id));    \
    dst[strlen(tlvptr->leaf_id)] = '\0';


#define tlv_copy_leaf_value(tlvptr, dst)                         \
    strncpy((char *)dst, tlvptr->value, strlen(tlvptr->value));          \
    dst[strlen(tlvptr->value)] = '\0';

#define collect_tlv(ser_buff, tlvptr)           \
    serialize_string(ser_buff, (char *)tlvptr, sizeof(tlv_struct_t))

#define prepare_tlv_from_leaf(leaf, tlvptr)    \
    tlvptr->tlv_type = TLV_TYPE_NORMAL; \
    tlvptr->leaf_type = leaf->leaf_type;       \
    strncpy((char *)tlvptr->leaf_id, leaf->leaf_id, MIN(LEAF_ID_SIZE, strlen(leaf->leaf_id)));

#define put_value_in_tlv(tlvptr, _val)         \
	{										   \
		const char *temp = _val;			   \
		strncpy((char *)tlvptr->value, temp, LEAF_VALUE_HOLDER_SIZE);	\
	}

static inline void 
print_tlv_content(tlv_struct_t *tlv){

    if(!tlv)
        return;

    printf ("tlv->tlv_type = %d\n", tlv->tlv_type);
    printf("tlv->leaf_type = %s\n", get_str_leaf_type(tlv->leaf_type));
    printf("tlv->leaf_id   = %s\n", tlv->leaf_id);
    printf("tlv->value     = %s\n", tlv->value);
}

static inline bool
parser_match_leaf_id (unsigned char *tlv_leaf_id, const char *leaf_id_manual) {

    size_t len;
    if ((len = strlen((const char *)tlv_leaf_id)) != strlen(leaf_id_manual)) return false;
    return (strncmp((const char *)tlv_leaf_id, leaf_id_manual, len) == 0);
}

static inline void
dump_tlv_serialized_buffer(ser_buff_t *tlv_ser_buff){

    tlv_struct_t *tlv = NULL;

    printf ("\n");
    //printf("cmd code = %d\n", EXTRACT_CMD_CODE(tlv_ser_buff));
    TLV_LOOP_BEGIN(tlv_ser_buff, tlv){
        print_tlv_content(tlv);
        printf("\n");
    } TLV_LOOP_END;
}

static inline void
swap_tlv_units(tlv_struct_t *tlv1, tlv_struct_t *tlv2){
    
    tlv_struct_t tlv;
    tlv = *tlv1;
    *tlv1 = *tlv2;
    *tlv2 = tlv;
}

#endif /* __CMDTLV__H */
