#ifndef CONNECTIONS_H
#define CONNECTIONS_H

#include "queue.h"


typedef struct Session {
    ConnectionQueue_t *queue;
    unsigned int session_id;
} Session_t;

void *connect_clients(void *args);


#endif