#include "server_epoll.h"

int setnonblocking(int sock)
{
  int opts;    
  opts = fcntl(sock, F_GETFL);
  if (opts < 0) {
    fprintf (stderr, "error: fcntl(F_GETFL)\n");
    return -1;
  }
  opts = (opts | O_NONBLOCK);
  if (fcntl(sock, F_SETFL, opts) < 0) {
    fprintf (stderr, "error: fcntl(F_SETFL)\n");
    return -1;
  }
  return 0;
}


void client_state_init (epoll_state_t * state)
{
  state->buffer = NULL;
  state->status = ESS_HEAD;
  state->bytes = 0;
  state->total = 0;
}

int add_to_clients_pool (epoll_client_t * client, epoll_clients_pool_t * pool)
{
  if (pool->free_ptr < 0)
    {
      return -1;
    }
  pool->clients[pool->free[pool->free_ptr]] = client;
  return pool->free[pool->free_ptr--];
}

int del_from_clients_pool (int client_id, epoll_clients_pool_t * pool)
{
  pool->free[++pool->free_ptr] = client_id;
  return 0;
}

int client_process_message (epoll_client_t * client, reactor_t * reactor)
{
  fprintf (stdout, "client_process_message() fd = %d buffer = %s", client->fd, client->read_state.buffer);
  free (client->read_state.buffer);
  return 0;
}

int client_read_head (epoll_client_t * client, reactor_t * reactor)
{
  int size = read (client->fd, &((char *)&client->read_state.total)[client->read_state.bytes], sizeof (client->read_state.total) - client->read_state.bytes);
  if (size <= 0)
    {
      fprintf (stderr, "error: read()\n");
      return -1;
    }
  client->read_state.bytes += size;
  if (client->read_state.bytes == sizeof(client->read_state.total))
    {
      client->read_state.status = ESS_BODY;
      client->read_state.buffer = malloc (client->read_state.total);
      if (client->read_state.buffer == NULL)
	{
	  fprintf (stderr, "error: malloc()\n");
	  return -1;
	}
      client->read_state.bytes = 0;
      return size;
    }
  else
    return 0;
}

int client_read_body (epoll_client_t * client, reactor_t * reactor)
{
  int size = read (client->fd, &client->read_state.buffer[client->read_state.bytes], client->read_state.total - client->read_state.bytes);
  if (size <= 0)
    {
      fprintf (stderr, "error: read()\n");
      return -1;
    }
  client->read_state.bytes += size;
  if (client->read_state.bytes == client->read_state.total)
    {
      client->read_state.status = ESS_HEAD;
      client->read_state.bytes = 0;
      client_process_message (client, reactor);
      return size;
    }
  else
    return 0;
}


int client_read_handler (epoll_client_t * client, reactor_t * reactor, struct epoll_event * ev)
{
  int size;
  for (;;)
    switch (client->read_state.status)
      {
      case ESS_HEAD:
	size = client_read_head (client, reactor);
	if (size < 0)
	  return (size);
	break;
	
      case ESS_BODY:
	size = client_read_body (client, reactor);
	if (size < 0)
	  return (size);
	break;
      }

  return 0;
}

int client_write_head (epoll_client_t * client, reactor_t * reactor)
{
  int size = write (client->fd, &((char *)client->write_state.total)[client->write_state.bytes], sizeof (client->write_state.total) - client->write_state.bytes);
  if (size <= 0)
    {
      fprintf (stderr, "error: write()");
      return -1;
    }
  client->write_state.bytes += size;
  if (client->write_state.bytes == sizeof (client->write_state.total))
    {
      client->write_state.status = ESS_BODY;
      client->write_state.bytes = 0;
      return size;
    }
  return 0;
}

int client_write_body (epoll_client_t * client, reactor_t * reactor)
{
  int size = write (client->fd, &(client->write_state.buffer[client->write_state.bytes]), client->write_state.total - client->write_state.bytes);
  if (size <= 0)
    {
      fprintf (stderr, "error: write()");
      return -1;
    }
  client->write_state.bytes += size;
  if (client->write_state.bytes == client->write_state.total)
    {
      client->write_state.status = ESS_HEAD;
      client->write_state.bytes = 0;
      free (client->write_state.buffer);
      return size;
    }
  return 0;
}

int client_write_handler (epoll_client_t * client, reactor_t * reactor, struct epoll_event * ev)
{
  int size;
  for (;;)
    switch (client->write_state.status)
      {
      case ESS_HEAD:
	size = client_write_head (client, reactor);
	if (size < 0)
	  return (size);
	break;

      case ESS_BODY:
	size = client_write_body (client, reactor);
	if (size < 0)
	  return (size);
	break;
      }

  return 0;
}

void close_client (int client_id, reactor_t * reactor)
{
  epoll_client_t * client = reactor->clients_pool.clients[client_id];
  close (client->fd);
  del_from_clients_pool (client_id, &reactor->clients_pool);
  free (client);
}

int listener_handler (epoll_client_t * client, reactor_t * reactor, struct epoll_event * ev)
{
  struct epoll_event client_ev;
  int client_fd = accept (client->fd, NULL, NULL);
  if (client_fd == -1)
    {
      fprintf (stderr, "error: accept()\n");
      return -1;
    }
  printf ("new client fd = %d, accepting... ", client_fd);
  if (setnonblocking (client_fd) == -1)
    {
      fprintf (stderr, "error: setnonblocking()\n");
      close (client_fd);
      return -1;
    }
  epoll_client_t * client_info = malloc (sizeof (*client_info));
  if (client_info == NULL)
    {
      fprintf (stderr, "error: malloc()\n");
      close (client_fd);
      return -1;
    }
  client_info->fd = client_fd;
  client_info->read = client_read_handler;
  client_info->write = client_write_handler;
  client_state_init (&client_info->read_state);
  client_state_init (&client_info->write_state);
  client_ev.data.ptr = (void *)add_to_clients_pool(client, &reactor->clients_pool);
  client_ev.events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLERR;
  if (epoll_ctl (reactor->epollfd, EPOLL_CTL_ADD, client_fd, &client_ev) == -1)
    {
      fprintf (stderr, "error: epoll_ctl()\n");
      close_client ((int)client_ev.data.ptr, reactor);
      return -1;
    }
  fprintf (stdout, "OK, client_id = %d \n", (int)client_ev.data.ptr);
  return 0;
}

int client_read (int client_id, reactor_t * reactor, struct epoll_event * ev)
{
  epoll_client_t * client = reactor->clients_pool.clients[client_id];
  fprintf (stdout, "client_read() client_id = %d, client_fd = %d\n", client_id, client->fd);
  if (NULL == client)
    return (-1);
  if (NULL == client->read)
    return (-1);
  return (client->read (client, reactor, ev));
}

int client_write (int client_id, reactor_t * reactor, struct epoll_event * ev)
{
  epoll_client_t * client = reactor->clients_pool.clients[client_id];
  fprintf (stdout, "client_write() client_id = %d, client_fd = %d\n", client_id, client->fd);
  if (NULL == client)
    return (-1);
  if (NULL == client->write)
    return (-1);
  return (client->write (client, reactor, ev));
}

void * tem_epoll (void * args)
{
  reactor_t reactor = {
    .context = args
  };

  int i, j;
  for (i = EPOLL_CLIENTS_POOL_SIZE - 1, j = 0; i >= 0; i--, j++)
    {
      reactor.clients_pool.free[i] = j;
    }
  reactor.clients_pool.free_ptr = EPOLL_CLIENTS_POOL_SIZE - 1;

  struct epoll_event ev, events[EPOLL_MAX_EVENTS];
  int nfds;
  epoll_client_t listener = {
    .read = listener_handler,
    .write = NULL
  };


  listener.fd = srv_create_listener (reactor.context->port, reactor.context->addr);
  if (listener.fd == -1)
    {
      fprintf (stderr, "error: srv_create_listener()\n");
      return NULL;
    }
  client_state_init (&listener.read_state);
  
  reactor.listener_id = add_to_clients_pool (&listener, &reactor.clients_pool);

  reactor.epollfd = epoll_create1 (0);
  if (reactor.epollfd == -1)
    {
      close (listener.fd);
      fprintf (stderr, "error: epoll_create() errno = %d\n", errno);
      perror(strerror(errno));
      return NULL;
    }

  ev.events = EPOLLIN;
  ev.data.ptr = (void *)reactor.listener_id;
  if (epoll_ctl (reactor.epollfd, EPOLL_CTL_ADD, listener.fd, &ev) == -1)
    {
      close (listener.fd);
      close (reactor.epollfd);
      printf ("error: epoll_ctl (listener)\n");
      return NULL;
    }
  
  listen (listener.fd, 10);
  fprintf (stdout, "Listener create. Waiting for clients...\n");
  for (;;)
    {
      nfds = epoll_wait (reactor.epollfd, events, EPOLL_MAX_EVENTS, -1);
      if (nfds == -1)
	{
	  fprintf (stderr, "error: epoll_wait()\n");
	  return NULL;
	}
      printf ("Event occurred. nfds = %d\n", nfds);
      int i;
      for (i = 0; i < nfds; i++)
	{
	  int client_id = (int)events[i].data.ptr;
	  fprintf (stdout, "client_id = %d, listener_id = %d, mask = ", client_id, reactor.listener_id);
	  if (events[i].events & EPOLLIN)
	    {
	      fprintf (stdout, "IN ");
	      client_read (client_id, &reactor, &events[i]);
	    }
	  if (events[i].events & EPOLLOUT)
	    {
	      fprintf (stdout, "OUT ");
	      client_write (client_id, &reactor, &events[i]);
	    }
	  if (events[i].events & EPOLLERR)
	    {
	      fprintf (stdout, "ERR "); 
	      //Some stuff here
	    }
	  if (events[i].events & EPOLLHUP)
	    {
	      fprintf (stdout, "HUP ");

	    }
	  fprintf (stdout, "\n");
	}
    }

}

void server_epoll_mode (context_t * context)
{
  tem_epoll (context);
}
