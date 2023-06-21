#ifndef __TRACER_T__
#define __TRACER_T__

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

typedef struct tracer_ tracer_t;

tracer_t *
tracer_init (const char *tr_str_id, const char *file_name, int out_fd, uint64_t logging_bits) ;

void 
trace (tracer_t *tracer, uint64_t bit, const char *format, ...);

void 
enable_file_logging (tracer_t *tracer, bool enable);

void 
enable_console_logging (tracer_t *tracer, bool enable);

#endif 