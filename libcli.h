#ifndef __LIBCLI__
#define __LIBCLI__

#include <ncurses.h>
#include "stack/stack.h"
#include "KeyProcessor/KeyProcessor.h"
#include "CmdTree/CmdTree.h"
#include "cmdtlv.h"

void
libcli_init ();

/*See the definition of this fn to know about arguments*/
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
libcli_register_param (param_t *parent, param_t *child);

void 
libcli_set_param_cmd_code (param_t *param, int cmd_code) ;

void
libcli_support_cmd_negation (param_t *param);

void 
libcli_init_done ();

void
cli_start_shell();

int cprintf (const char* format, ...) ;

#define printf cprintf

#endif
