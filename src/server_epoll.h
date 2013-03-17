#ifndef _SERVER_EPOLL_H
#define _SERVER_EPOLL_H

#include "shared.h"

int client_read_handler (epoll_client_t *, reactor_t *);

int client_write_handler (epoll_client_t *, reactor_t *);

int listener_handler (epoll_client_t *, reactor_t *);

void server_epoll_mode (context_t *);

#endif
