#ifndef CONNECTIONS_H
#define CONNECTIONS_H

#include "queue.h"


typedef struct Connection{
    char req_pipe_path[MAX_PIPE_NAME_SIZE];
    char resp_pipe_path[MAX_PIPE_NAME_SIZE];
    struct Connection* next; 
} Connection_t;

typedef struct Session {
    ConnectionQueue_t *queue;
    unsigned int session_id;
} Session_t;

void *connect_clients(void *args);


#endif