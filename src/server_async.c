#include "server_async.h"

void * tam_reader_thread (void * args)
{
  client_t * client = args;
  context_t * context = client->context;

  cs_status_t recv_status;
  char * buffer;
  cs_message_t message;

  for (;;)
    {
      recv_status = recv_message (client->fd, &buffer);
      if (recv_status == S_SUCCESS)
	{
	  xml_to_message (buffer, &message);
	  free (buffer);

	  pthread_setcancelstate (PTHREAD_CANCEL_DISABLE, NULL);
	  pthread_mutex_lock (&client->tasks_register.free_mutex);
	  fflush (stdout);
	  client->tasks_register.free_idx[client->tasks_register.free++] = message.task.idx;
	  pthread_mutex_unlock (&client->tasks_register.free_mutex);
	  sem_post (&client->tasks_register.empty);
	  dec_sem (&context->tasks_in_process, &context->tasks_in_process_sem, &context->tasks_in_process_mutex);
	  if (message.result)
	    {
	      context->result.found = !0;
	      strcpy (context->result.password, message.password);
	      pthread_mutex_lock (&context->continue_execute_mutex);
	      context->continue_execute = 0;
	      pthread_mutex_unlock (&context->continue_execute_mutex);
	      break;
	    }
	  pthread_setcancelstate (PTHREAD_CANCEL_ENABLE, NULL);
	}
      else
	{
	  client->close = !0;
	  if (recv_status != S_CONNECTION_CLOSED)
	    {
	      client->err = 1;
	    }
	  break;
	}
    }
  pthread_cond_broadcast (&context->continue_execute_sem);
  return NULL;
}

void * tam_writer_thread (void * args)
{
  client_t * client = args;
  context_t * context = client->context;
  task_t task;
  cs_message_t message;
  int send_status;

  for (;;)
    {
      sem_wait (&client->tasks_register.empty);
      queue_pop (&context->queue, &task);
      pthread_setcancelstate (PTHREAD_CANCEL_DISABLE, NULL);
      /*      if (task.final)
	{
	  queue_push (&context->queue, &task);
	  break;
	}
      */
      if (context->result.found)
	{
	  dec_sem (&context->tasks_in_process, &context->tasks_in_process_sem, &context->tasks_in_process_mutex);
	  sem_post (&client->tasks_register.empty);
	  continue;
	}
      pthread_mutex_lock (&client->tasks_register.free_mutex); 
      task.idx = client->tasks_register.free_idx[--client->tasks_register.free];
      client->tasks_register.tasks[task.idx] = task;
      pthread_mutex_unlock (&client->tasks_register.free_mutex);
      pthread_setcancelstate (PTHREAD_CANCEL_ENABLE, NULL);
      
      message.type = MT_SEND_JOB;
      message.alph = context->alph;
      message.hash = context->hash;
      message.task = task;
      printf ("(%d) task: [%d.%d] %s\n", client->fd, task.id, task.idx, task.password);
      fflush (stdout);
      send_status = send_message (client->fd, &message);
      if (send_status == -1)
	{
	  fprintf (stderr, "error: send_message()\n");
	  client->close = !0;
	  client->err = 1;
	  break;
	}
    }
  pthread_cond_broadcast (&context->continue_execute_sem);
  return NULL;
}

void tasks_register_init (tasks_register_t * tasks_register)
{
  pthread_mutex_init (&tasks_register->free_mutex, NULL);
  sem_init (&tasks_register->empty, 0, REGISTER_SIZE);
  tasks_register->free = REGISTER_SIZE;
  int i;
  for (i = 0; i < REGISTER_SIZE; i++)
    tasks_register->free_idx[i] = i;
}

void * tam_client_thread (context_t * context, int client_socket)
{
 printf ("(%d) connected\n", client_socket);
 fflush (stdout);
 inc_sem (&context->threads, &context->threads_sem, &context->threads_mutex);
 
  client_t client =
    {
      .fd = client_socket,
      .context = context,
      .close = 0,
      .err = 0,
    };
  tasks_register_init (&client.tasks_register);
  
  pthread_t reader_thread;
  pthread_create (&reader_thread, NULL, tam_reader_thread, &client);
  pthread_t writer_thread;
  pthread_create (&writer_thread, NULL, tam_writer_thread, &client);

  pthread_mutex_lock (&context->continue_execute_mutex);
  while (context->continue_execute != 0 && client.close == 0)
    {
      pthread_cond_wait (&context->continue_execute_sem, &context->continue_execute_mutex);
    }
  pthread_mutex_unlock (&context->continue_execute_mutex);

  close (client_socket);

  printf ("(%d) disconnected\n", client_socket);

  pthread_cancel (writer_thread);
  pthread_cancel (reader_thread);
  
  if (!context->result.found)
    {
      int i, j, found;
      for (i = 0; i < REGISTER_SIZE; i++)
	{
	  found = 0;
	  for (j = 0; j < client.tasks_register.free; j++)
	    {
	      if (client.tasks_register.free_idx[j] == i)
		{
		  found = !0;
		  break;
		}
	    }
	  if (!found)
	    {
	      printf ("(%d) pushed back: [%d.%d] %s\n", client.fd, client.tasks_register.tasks[i].id, client.tasks_register.tasks[i].idx, client.tasks_register.tasks[i].password);
	      queue_push (&context->queue, &client.tasks_register.tasks[i]);
	    }
	}
    }

  printf ("(%d) ended\n", client_socket);

  dec_sem (&context->threads, &context->threads_sem, &context->threads_mutex);
  return NULL;
}

void server_async_mode (context_t * context)
{
  context->srv_client_thread = tam_client_thread;

  init_sem (&context->continue_execute, 1, &context->continue_execute_sem, &context->continue_execute_mutex);
  server (context);
}
