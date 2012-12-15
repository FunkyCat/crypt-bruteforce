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

int client_read_head (epoll_client_t * client, reactor_t * reactor)
{
  int size = read (&((char*)&client->read_state.total)[client->read_state.bytes], sizeof (client->read_state.total) - client->read_state.bytes);
  if (size <= 0)
    {
      fprintf (stderr, "error: read()\n");
      return -1;
    }
  client->read_state.bytes += size;
  if (client->read_state.bytes == sizeof(client->read_state.total))
    {
      client->read_state.status = ESS_BODY;
      client->read_state.buf = malloc (client->read_state.total);
      if (client->read_state.buf == NULL)
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

int client_read_head (epoll_client_t * client, reactor_t * reactor)
{
  int size = read (&client->read_state.buf[client->read_state.bytes], client->read_state.total - client->read_state.bytes);
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


int client_read_handler (epoll_client_t * client, reactor_t * reactor)
{
  int size;
  for (;;)
    switch (client->read_state.status)
      {
      case ESS_HEAD:
	size = client_read_head (client, reactor);
	if (size <= 0)
	  return (size);
	break;
	
      case ESS_BODY:
	size = client_read_body (client, reactor);
	if (size <= 0)
	  return (size);
	break;
      }

  return 0;
}

int client_write_handler (epoll_client_t * client, reactor_t * reactor, struct epoll_event * ev)
{
  

  return 0;
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
  client_ev.data.ptr = client_info;
  client_ev.events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLERR;
  if (epoll_ctl (reactor->epollfd, EPOLL_CTL_ADD, client_fd, &client_ev) == -1)
    {
      fprintf (stderr, "error: epoll_ctl()\n");
      close (client_fd);
      return -1;
    }
  return 0;
}

int client_read (epoll_client_t * client, reactor_t * reactor, struct epoll_event * ev)
{
  if (NULL == client)
    return (-1);
  if (NULL == client->read)
    return (-1);
  return (client->read (client, reactor, ev));
}

int client_write (epoll_client_t * client, reactor_t * reactor, struct epoll_event * ev)
{
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
  struct epoll_event ev, events[EPOLL_MAX_EVENTS];
  int nfds;
  epoll_client_t listener = {
    .read = listener_handler,
    .write = NULL
  };
  epoll_client_t * client;

  listener.fd = srv_create_listener (reactor.context->port, reactor.context->addr);
  if (listener.fd== -1)
    {
      fprintf (stderr, "error: srv_create_listener()\n");
      return NULL;
    }
  client_state_init (&listener.read_state);

  reactor.epollfd = epoll_create (0);
  if (reactor.epollfd == -1)
    {
      close (listener.fd);
      fprintf (stderr, "error: epoll_create()\n");
      return NULL;
    }

  ev.events = EPOLLIN;
  ev.data.ptr = &listener;
  if (epoll_ctl (reactor.epollfd, EPOLL_CTL_ADD, listener.fd, &ev) == -1)
    {
      close (listener.fd);
      printf ("error: epoll_ctl (listener)\n");
      return NULL;
    }

  for (;;)
    {
      nfds = epoll_wait (reactor.epollfd, events, EPOLL_MAX_EVENTS, -1);
      if (nfds == -1)
	{
	  fprintf (stderr, "error: epoll_wait()\n");
	  return NULL;
	}
      int i;
      for (i = 0; i < nfds; i++)
	{
	  client = events[i].data.ptr;
	  if (events[i].events & EPOLLIN)
	    {
	      client_read (client, &reactor, &events[i]);
	    }
	  if (events[i].events & EPOLLOUT)
	    {
	      client_write (client, &reactor, &events[i]);
	    }
	  if (events[i].events & EPOLLERR)
	    {
	      //Some stuff here
	    }
	}
    }

}

void server_epoll_mode (context_t * context)
{
  
}
