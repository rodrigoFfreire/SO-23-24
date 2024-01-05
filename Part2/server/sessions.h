#ifndef CONNECTIONS_H
#define CONNECTIONS_H

#include "queue.h"
#include "common/constants.h"

#define CLIENT_SUCCESS 0
#define CLIENT_FAILED 1 
#define CLIENT_UNRESPONSIVE 2

typedef struct Session {
    ConnectionQueue_t *queue;
    unsigned int session_id;
} Session_t;

/// Main function that runs on the worker threads. Accepts a connection with a client
/// and executes its commands.
/// @param args Thread argument. A `Session_t` struct is passed as argument.
/// @return `NULL`
void *connect_clients(void *args);


#endif
