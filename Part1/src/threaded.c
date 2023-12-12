#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include "operations.h"
#include "threaded.h"


void *thread_ems_create(void *args) {
    CommandArgs_t *cmd_args = (CommandArgs_t *) args;
    if (ems_create(cmd_args->ems, cmd_args->event_id, cmd_args->num_rows, cmd_args->num_cols))
        fprintf(stderr, "Failed to create event\n");


    free(args);

    return NULL;
}

void *thread_ems_reserve(void *args) {
    CommandArgs_t *cmd_args = (CommandArgs_t *) args;
    if (ems_reserve(cmd_args->ems, cmd_args->event_id, cmd_args->num_seats, cmd_args->xs_cpy, cmd_args->ys_cpy))
        fprintf(stderr, "Failed to reserve seats\n");

    free(cmd_args->xs_cpy);
    free(cmd_args->ys_cpy);
    free(args);

    return NULL;
}

void *thread_ems_show(void *args) {
    CommandArgs_t *cmd_args = (CommandArgs_t *) args;
    if (ems_show(cmd_args->ems, cmd_args->event_id, cmd_args->out_fd))
        fprintf(stderr, "Failed to show event\n");
    
    free(args); 

    return NULL;
}

void *thread_ems_list_events(void *args) {
    CommandArgs_t *cmd_args = (CommandArgs_t *) args;
    if (ems_list_events(cmd_args->ems, cmd_args->out_fd))
        fprintf(stderr, "Failed to create event\n");

    free(args);
    
    return NULL;
}