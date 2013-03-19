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

void event_queue_init (epoll_event_queue_t * queue)
{
  pthread_mutex_init (&queue->head_mutex, NULL);
  pthread_mutex_init (&queue->tail_mutex, NULL);
  sem_init (&queue->full, 0, 0);
  sem_init (&queue->empty, 0, EPOLL_EVENT_QUEUE_SIZE);
  queue->head = 0;
  queue->tail = 0;
}

void event_queue_push (uint64_t client_id, epoll_event_queue_t * queue)
{
  //fprintf (stdout, "push id = %llu\n", client_id);
  sem_wait (&queue->empty);
  pthread_mutex_lock (&queue->head_mutex);
  queue->queue[queue->head] = client_id;
  if (++queue->head == sizeof (queue->queue) / sizeof (queue->queue[0]))
    {
      queue->head = 0;
    }
  pthread_mutex_unlock (&queue->head_mutex);
  sem_post (&queue->full);
}

uint64_t event_queue_pop (epoll_event_queue_t * queue)
{
  sem_wait (&queue->full);
  pthread_mutex_lock (&queue->tail_mutex);
  uint64_t ret = queue->queue[queue->tail];
  //fprintf (stdout, "pop id = %llu\n", ret);
  if (++queue->tail == sizeof (queue->queue) / sizeof (queue->queue[0]))
    {
      queue->tail = 0;
    }
  pthread_mutex_unlock (&queue->tail_mutex);
  sem_post (&queue->empty);
  return ret;
}

void client_state_init (epoll_state_t * state)
{
  state->buffer = NULL;
  state->status = ESS_HEAD;
  state->bytes = 0;
  state->total = 0;
  state->actual = 0;
}

uint32_t add_to_clients_pool (epoll_client_t * client, epoll_clients_pool_t * pool)
{
  if (pool->free_ptr < 0)
    {
      fprintf (stderr, "error: add_to_clients_pool() poll is full");
      return -1;
    }
  int index = pool->free[pool->free_ptr--];
  pool->clients[index] = client;
  //fprintf (stdout, "pool_add [%d] counter = %d\n", index, client->counter);
  return index;
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
	if (size < 0)
	  return (size);
	break;
	
      case ESS_BODY:
	size = client_read_body (client, reactor);
	if (size < 0)
	  {
	    return (size);
	  }
	else if (size > 0)
	  {
	    client_process_message (client, reactor);
	    return 1;
	  }
	break;
      }

  return 0;
}

int client_write_head (epoll_client_t * client, reactor_t * reactor)
{
  char * buff = &(((char *)&client->write_state.total)[client->write_state.bytes]);
  size_t len = sizeof (client->write_state.total) - client->write_state.bytes;
  int size = write (client->fd, buff, len);
  if (size < 0)
    {
      fprintf (stderr, "error: write() errno = %d mess = %s\n", errno, strerror (errno));
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
  char * buff = &(client->write_state.buffer[client->write_state.bytes]);
  size_t len = ntohl(client->write_state.total) - client->write_state.bytes;
  int size = write (client->fd, buff, len);
  if (size < 0)
    {
      fprintf (stderr, "error: write() errno = %d mess = %s\n", errno, strerror (errno));
      return -1;
    }
  client->write_state.bytes += size;
  if (client->write_state.bytes == client->write_state.total)
    {
      client->write_state.status = ESS_HEAD;
      client->write_state.bytes = 0;
      client->write_state.actual = 0;
      free (client->write_state.buffer);
      return size;
    }
  return 0;
}

int client_write_handler (epoll_client_t * client, reactor_t * reactor)
{
  fprintf (stdout, "Writer for client fd=%d counter=%u", client->fd, client->counter);
  int size;
  
  switch (client->write_state.status)
    {
    case ESS_HEAD:
      fprintf (stdout, "Write HEAD\n");
      size = client_write_head (client, reactor);
      if (size < 0)
	{
	  return (-1);
	}
      else if (size > 0)
	{
	  return (1);
	}
      break;

    case ESS_BODY:
      fprintf (stdout, "Write BODY\n");
      size = client_write_body (client, reactor);
      if (size < 0)
	{
	  return (-1);
	}
      else if (size > 0)
	{
	  return (1);
	}
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

int listener_handler (epoll_client_t * listener, reactor_t * reactor)
{
  struct epoll_event client_ev;
  int client_fd = accept (listener->fd, NULL, NULL);
  if (client_fd < 0)
    {
      fprintf (stderr, "error: accept()\n");
      return -1;
    }
  fprintf (stdout, "new client fd = %d\n", client_fd);
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
  pthread_mutex_init (&client_info->mutex, NULL);
  client_info->fd = client_fd;
  client_info->read = client_read_handler;
  client_info->write = client_write_handler;
  client_info->counter = reactor->client_counter++;
  client_state_init (&client_info->read_state);
  client_state_init (&client_info->write_state);

  uint32_t index = add_to_clients_pool(client_info, &reactor->clients_pool);
  client_ev.data.u64 = (uint64_t)index << 32 | client_info->counter;
  client_ev.events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLERR;
  if (epoll_ctl (reactor->epollfd, EPOLL_CTL_ADD, client_fd, &client_ev) == -1)
    {
      fprintf (stderr, "error: epoll_ctl()\n");
      close_client ((int)client_ev.data.ptr, reactor);
      return -1;
    }
  fprintf (stdout, "new client counter = %u index = %u id = %llu\n", client_info->counter, index, client_ev.data.u64);
  return 0;
}

void * read_worker (void * args)
{
  reactor_t * reactor = args;

  fprintf (stdout, "%u\n", reactor->clients_pool.clients[0]->counter);

  uint64_t client_id;
  epoll_client_t * client;
  uint32_t index, counter;
  for (;;)
    {
      client_id = event_queue_pop (&reactor->read_queue);
      index = (uint32_t)(client_id >> 32);
      counter = client_id & (((uint64_t)1 << 32) - 1);
      client = reactor->clients_pool.clients[index];
      if (NULL == client)
	{
	  continue;
	}
      pthread_mutex_lock (&client->mutex);
      if (client->counter != counter)
	{
	  pthread_mutex_unlock (&client->mutex);
	  continue;
	}
      fprintf (stdout, "read_worker() : client popped, index = %d, counter = %d\n", index, counter);
      if (client->read (client, reactor) == 1)
	{
	  event_queue_push (client_id, &reactor->read_queue);
	}
      pthread_mutex_unlock (&client->mutex);
    }
}

int fill_write_buffer (epoll_client_t * client, context_t * context)
{
  task_t task;
  cs_message_t message;
  char * messxml;

  fprintf (stdout, "Fill buffer for client fd=%d counter=%u\n", client->fd, client->counter);

  queue_pop (&context->queue, &task);
  if (task.final)
    {
      queue_push (&context->queue, &task);
      return 0;
    }

  message.type = MT_SEND_JOB;
  message.alph = context->alph;
  message.hash = context->hash;
  message.task = task;
  messxml = message_to_xml (&message);
  uint32_t len = htonl (strlen (messxml) + 1);
  
  client->write_state.total = len;
  client->write_state.buffer = messxml;
  client->write_state.actual = !0;

  return !0;
}

void * write_worker (void * args)
{
  reactor_t * reactor = args;

  uint64_t client_id;
  epoll_client_t * client;
  uint32_t index, counter;
  int write_ret;
  for (;;)
    {
      client_id = event_queue_pop (&reactor->write_queue);
      index = client_id >> 32;
      counter = client_id & (((uint64_t)1 << 32) - 1);
      client = reactor->clients_pool.clients[index];
      if (NULL == client)
	{
	  continue;
	}
      pthread_mutex_lock (&client->mutex);
      if (client->counter != counter)
	{
	  pthread_mutex_unlock (&client->mutex);
	  continue;
	}
      fprintf (stdout, "write worker() : client popped, index = %d, client->counter = %u counter = %u\n", index, client->counter, counter);
      if (!client->write_state.actual)
	{
	  fprintf (stdout, "not actual\n");
	  if (!fill_write_buffer (client, reactor->context))
	    {
	      return NULL;
	    }
	}
      fprintf (stdout, "writing...\n");
      write_ret = client->write (client, reactor);
      if (write_ret == 1)
	{
	  event_queue_push (client_id, &reactor->write_queue);
	}
      else if (write_ret == -1)
	{
	  fprintf (stderr, "error: client->write()\n");
	}
      pthread_mutex_unlock (&client->mutex);
    }
}



void tem_init_clients_pool (epoll_clients_pool_t * pool)
{
  int i, j;
  for (i = EPOLL_CLIENTS_POOL_SIZE - 1, j = 0; i >= 0; i--, j++)
    {
      pool->free[i] = j;
    }
  pool->free_ptr = EPOLL_CLIENTS_POOL_SIZE - 1;
}

int tem_init_epoll (reactor_t * reactor)
{
  tem_init_clients_pool (&reactor->clients_pool);

  reactor->client_counter = 0;

  struct epoll_event ev;
  reactor->listener = malloc (sizeof (*reactor->listener));
  reactor->listener->read = listener_handler;
  reactor->listener->write = NULL;
  pthread_mutex_init (&reactor->listener->mutex, NULL);

  reactor->listener->fd = srv_create_listener (reactor->context->port, reactor->context->addr);
  if (reactor->listener->fd == -1)
    {
      fprintf (stderr, "error: srv_create_listener()\n");
      return -1;
    }
  client_state_init (&reactor->listener->read_state);
  
  reactor->listener->counter = reactor->client_counter++;
  reactor->listener_id = (uint64_t)add_to_clients_pool (reactor->listener, &reactor->clients_pool) << 32 | (uint64_t)reactor->listener->counter;

  reactor->epollfd = epoll_create1 (0);
  if (reactor->epollfd == -1)
    {
      close (reactor->listener->fd);
      fprintf (stderr, "error: epoll_create() errno = %d\n", errno);
      perror(strerror(errno));
      return -1;
    }

  ev.events = EPOLLIN;
  ev.data.u64 = reactor->listener_id;
  printf ("reactor->listener_id = %llu\n", reactor->listener_id);

  if (epoll_ctl (reactor->epollfd, EPOLL_CTL_ADD, reactor->listener->fd, &ev) == -1)
    {
      close (reactor->listener->fd);
      close (reactor->epollfd);
      printf ("error: epoll_ctl ()\n");
      return -1;
    }

  listen (reactor->listener->fd, 10);  
  
  return 0;
}

void * tem_epoll_cycle (void * args)
{
  reactor_t * reactor = args;

  event_queue_init (&reactor->read_queue);
  event_queue_init (&reactor->write_queue);

  struct epoll_event events[EPOLL_MAX_EVENTS];
  int nfds;

  for (;;)
    {
      nfds = epoll_wait (reactor->epollfd, events, EPOLL_MAX_EVENTS, -1);
      if (nfds == -1)
	{
	  fprintf (stderr, "error: epoll_wait()\n");
	  return NULL;
	}
      int i;
      for (i = 0; i < nfds; i++)
	{
	  uint64_t client_id = events[i].data.u64;
	  fprintf (stdout, "client_id = %llu, mask = ", client_id);
	  if (events[i].events & EPOLLIN)
	    {
	      fprintf (stdout, "IN ");
	      event_queue_push (client_id, &reactor->read_queue);
	    }
	  if (events[i].events & EPOLLOUT)
	    {
	      fprintf (stdout, "OUT ");
	      event_queue_push (client_id, &reactor->write_queue);
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
  reactor_t reactor = {
    .context = context,
  };

  srv_init (context);

  if (tem_init_epoll (&reactor) == -1)
    {
      fprintf (stderr, "error: tem_init_epoll()");
      return;
    }
  fprintf (stdout, "Epoll init complete. Waiting for clients...\n");

  pthread_t epoll_thread;
  pthread_create (&epoll_thread, NULL, tem_epoll_cycle, &reactor);
  pthread_detach (epoll_thread);
  fprintf (stdout, "Epoll cycle thread created.\n");

  pthread_t read_worker_thread;
  pthread_create (&read_worker_thread, NULL, read_worker, &reactor);
  pthread_detach (read_worker_thread);
  fprintf (stdout, "Read worker created.\n");

  pthread_t write_worker_thread;
  pthread_create (&write_worker_thread, NULL, write_worker, &reactor);
  pthread_detach (write_worker_thread);
  fprintf (stdout, "Write worker created.\n");

  fprintf (stdout, "Generating tasks...\n");
  srv_generate_tasks (reactor.context);
  fprintf (stdout, "Tasks generation complete. Pushing end task...");
  task_t end_task = { .final = !0 };
  queue_push (&context->queue, &end_task);
}
