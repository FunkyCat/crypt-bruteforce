#ifndef _CLIENT_MODE_H_
#define _CLIENT_MODE_H_

#include "shared.h"

int cm_create_socket (short port, struct in_addr addr);

int cm_socket_cycle (int socket, context_t * context);

void client_mode (context_t * context);

#endif
