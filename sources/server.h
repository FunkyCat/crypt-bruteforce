#ifndef _SERVER_MODE_H_
#define _SERVER_MODE_H_

#include "shared.h"

int tm_create_listener (short port, struct in_addr addr);

int tm_client_process (int, context_t *);

void * tm_client_thread (context_t *, int);

int tm_listener_cycle (int, context_t *);

void server_mode (context_t *);

#endif
