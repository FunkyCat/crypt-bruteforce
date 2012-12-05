#include "client_async.h"

void * cam_checker_thread (void * args)
{
  context_t * context = args;
  struct crypt_data cd = { .initialized = 0 };
  task_t task;
  result_t result;
  printf ("checher\n");
  for (;;)
    {
      queue_pop (&context->queue, &task);
      //printf ("C. pop task: [%d.%d] %s\n", task.id, task.idx, task.password);
      if (task.final)
	{
	  break;
	}
      task.cd = &cd;
      result.id = task.id;
      result.idx = task.idx;
      printf ("C. check task: [%d.%d] %s\n", task.id, task.idx, task.password);
      context->result.found = 0;
      if (brute_block_iterative (&task, check_password, context))
	{
	  result.result.found = !0;
	  strcpy (result.result.password, context->result.password);
	}
      else
	{
	  result.result.found = 0;
	}
      result_queue_push (&context->result_queue, &result);
      //printf ("C. push result: [%d.%d] %s\n", result.id, result.idx, result.result.found == 0 ? "not found" : "found");
    }
  return NULL;
}

void * cam_reader_thread (void * args)
{
  char * buffer;
  cs_message_t message;
  cs_status_t recv_status;
  server_t * server = args;
  context_t * context = server->context;
  printf ("reader\n");
  for (;;)
    {
      recv_status = recv_message (server->fd, &buffer);
      if (recv_status == S_SUCCESS)
	{
	  xml_to_message (buffer, &message);
	  printf ("R. recv task: [%d.%d] %s\n", message.task.id, message.task.idx, message.task.password);
	  free (buffer);

	  if (context->hash == NULL)
	    {
	      context->hash = message.hash;
	    }
	  else
	    {
	      free (message.hash);
	    }
	  if (context->alph == NULL)
	    {
	      context->alph = message.alph;
	    }
	  else
	    {
	      free (message.alph);
	    }
	  queue_push (&context->queue, &message.task);
	  //printf ("R. push task: [%d.%d] %s\n", message.task.id, message.task.idx, message.task.password);
	}
      else
	{
	  pthread_mutex_lock (&context->continue_execute_mutex);
	  context->continue_execute = 0;
	  pthread_mutex_unlock (&context->continue_execute_mutex);
	  pthread_cond_broadcast (&context->continue_execute_sem);
	  break;
	}
    }
  return NULL;
}

void * cam_writer_thread (void * args)
{
  result_t result;
  server_t * server = args;
  context_t * context = server->context;
  cs_message_t message;
  int send_status;
  printf ("writer\n");
  for (;;)
    {
      result_queue_pop (&context->result_queue, &result);
      //printf ("W. pop result: [%d.%d] %s\n", result.id, result.idx, result.result.found ? "found" : "not found");
      message.type = MT_REPORT_RESULT;
      message.task.id = result.id;
      message.task.idx = result.idx;
      if (result.result.found)
	{
	  message.result = !0;
	  strcpy (message.password, result.result.password);
	}
      else
	{
	  message.result = 0;
	}
      if ((send_status = send_message (server->fd, &message)) <= 0)
	{
	  pthread_mutex_lock (&context->continue_execute_mutex);
	  context->continue_execute = 0;
	  pthread_mutex_unlock (&context->continue_execute_mutex);
	  pthread_cond_broadcast (&context->continue_execute_sem);
	  break;
	}
      //printf ("W. send result: [%d.%d] %s\n", message.task.id, message.task.idx, message.result ? "found" : "not found");
    }
  return NULL;
}

void client_async_mode (context_t * context)
{
  context->alph = NULL;
  context->hash = NULL;

  queue_init (&context->queue);
  result_queue_init (&context->result_queue);
  init_sem (&context->continue_execute, 1, &context->continue_execute_sem, &context->continue_execute_mutex);

  int server_socket = cli_create_socket (context->port, context->addr);
  if (server_socket < 0)
    {
      fprintf (stderr, "error: cli_create_socket()\n");
      return;
    }

  server_t server = {
    .fd = server_socket,
    .context = context,
  };

  pthread_t checker_thread;
  pthread_create (&checker_thread, NULL, cam_checker_thread, context);
  pthread_t reader_thread;
  pthread_create (&reader_thread, NULL, cam_reader_thread, &server);
  pthread_t writer_thread;
  pthread_create (&writer_thread, NULL, cam_writer_thread, &server);
  
  pthread_mutex_lock (&context->continue_execute_mutex);
  while (context->continue_execute != 0)
    {
      pthread_cond_wait (&context->continue_execute_sem, &context->continue_execute_mutex);
    }
  pthread_mutex_unlock (&context->continue_execute_mutex);

  close (server_socket);

  printf ("Server closed connection\n");

  pthread_cancel (writer_thread);
  pthread_cancel (reader_thread);
  pthread_cancel (checker_thread);
}
