#ifndef EMS_THREADED_H
#define EMS_THREADED_H

#include <stddef.h>
#include <pthread.h>

#include "operations.h"
#include "constants.h"

typedef struct CommandArgs {
    struct Ems *ems;
    unsigned int event_id;
    size_t num_seats;
    size_t num_rows;
    size_t num_cols;
    size_t *xs_cpy;
    size_t *ys_cpy;
    int out_fd;
} CommandArgs_t;

void *thread_ems_create(void *args);

void *thread_ems_reserve(void *args);

void *thread_ems_show(void *args);

void *thread_ems_list_events(void *args);


#endif // EMS_THREADED_H