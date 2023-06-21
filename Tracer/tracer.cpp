#include <stdlib.h>
#include <assert.h>
#include <cstdarg>
#include <memory.h>
#include <unistd.h>
#include <pthread.h>
#include "tracer.h"

#define LOG_BUFFER_SIZE 256
#define CLI_INTG

typedef enum log_flags_ {

    ENABLE_FILE_LOG = 1,
    ENABLE_CONSOLE_LOG = 2
}log_flags_t;

typedef struct tracer_ {

    unsigned char tr_name[12];
    unsigned char Logbuffer[LOG_BUFFER_SIZE];
    int log_msg_len;
    FILE *log_file;
    int out_fd;
    uint64_t bits;
    struct tracer_ *left;
    struct tracer_ *right;
    uint8_t ctrl_flags;
    pthread_spinlock_t spin_lock;
} tracer_t;

static tracer_t *list_head = NULL;

static void 
tracer_save (tracer_t *tracer) {

    if (!list_head) list_head = tracer;
    tracer->right = list_head;
    list_head->left = tracer;
    list_head = tracer;
}

tracer_t *
tracer_init (const char *tr_str_id, const char *file_name, int out_fd, uint64_t logging_bits) {

    assert (tr_str_id);

    tracer_t *tr = (tracer_t *)calloc(1, sizeof (tracer_t));
    
    if (file_name) {
        tr->log_file = fopen (file_name, "w+");
    }

    tr->out_fd = out_fd;
    tr->bits = logging_bits;
    pthread_spin_init (&tr->spin_lock, PTHREAD_PROCESS_PRIVATE);
    tracer_save (tr);
    return tr;
}

#ifdef CLI_INTG
extern int cprintf (const char* format, ...) ;
#endif 

void 
trace (tracer_t *tracer, uint64_t bit, const char *format, ...) {

    va_list args;

    pthread_spin_lock (&tracer->spin_lock);

    if (!(tracer->bits & bit)) {
        pthread_spin_unlock (&tracer->spin_lock);
        return;
    }

    if (!tracer->ctrl_flags) {
        pthread_spin_unlock (&tracer->spin_lock);
        return;
    }

    va_start(args, format);

    memset (tracer->Logbuffer, 0, tracer->log_msg_len);
    tracer->log_msg_len = vsnprintf((char *)tracer->Logbuffer, LOG_BUFFER_SIZE, format, args);
    tracer->log_msg_len++;   // count \0 character
    va_end(args);

    if (tracer->log_file && (tracer->ctrl_flags & ENABLE_FILE_LOG)) {

        fwrite (tracer->Logbuffer, 1 , tracer->log_msg_len, tracer->log_file);
        fflush (tracer->log_file);
    }

    if (tracer->ctrl_flags & ENABLE_CONSOLE_LOG) {
        #ifndef CLI_INTG
        write (tracer->out_fd, tracer->Logbuffer, tracer->log_msg_len);
        #else 
        cprintf ("%s",  tracer->Logbuffer);
        #endif
    }

    pthread_spin_unlock (&tracer->spin_lock);
 }

void 
enable_file_logging (tracer_t *tracer, bool enable) {

    pthread_spin_lock (&tracer->spin_lock);

    if (enable) {
        tracer->ctrl_flags |= ENABLE_FILE_LOG;
    }
    else {
        tracer->ctrl_flags &= ~ENABLE_FILE_LOG;
    }

    pthread_spin_unlock (&tracer->spin_lock);
}

void 
enable_console_logging (tracer_t *tracer, bool enable) {

    pthread_spin_lock (&tracer->spin_lock);

    if (enable) {
        tracer->ctrl_flags |= ENABLE_CONSOLE_LOG;
    }
    else {
        tracer->ctrl_flags &= ~ENABLE_CONSOLE_LOG;
    }

    pthread_spin_unlock (&tracer->spin_lock);
}