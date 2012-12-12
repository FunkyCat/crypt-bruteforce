#include "client_async.h"

void * cam_checker_thread (void * args)
{
  context_t * context = args;
  struct crypt_data cd = { .initialized = 0 };
  task_t task;
  result_t result;
  printf ("checker\n");
  inc_sem (&context->threads, &context->threads_sem, &context->threads_mutex);
  for (;;)
    {
      queue_pop (&context->queue, &task);
      //printf ("C. pop task: [%d.%d] %s\n", task.id, task.idx, task.password);
      if (task.final)
	{
	  queue_push (&context->queue, &task);
	  break;
	}
      task.cd = &cd;
      result.id = task.id;
      result.idx = task.idx;
      result.final = 0;
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
  dec_sem (&context->threads, &context->threads_sem, &context->threads_mutex);
  printf ("Checker ended.\n");
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
	  break;
	}
    }
  pthread_cond_broadcast (&context->continue_execute_sem);
  printf ("Reader ended.\n");
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
      if (result.final)
	{
	  break;
	}
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
	  break;
	}
      //printf ("W. send result: [%d.%d] %s\n", message.task.id, message.task.idx, message.result ? "found" : "not found");
    }
  pthread_cond_broadcast (&context->continue_execute_sem);
  printf ("Writer ended.\n");
  return NULL;
}

void client_async_mode (context_t * context)
{
  context->n_cpus = (int) sysconf (_SC_NPROCESSORS_ONLN);
  printf ("n_cpus = %d\n", context->n_cpus);

  context->alph = NULL;
  context->hash = NULL;

  queue_init (&context->queue);
  result_queue_init (&context->result_queue);
  init_sem (&context->continue_execute, 1, &context->continue_execute_sem, &context->continue_execute_mutex);
  init_sem (&context->threads, 0, &context->threads_sem, &context->threads_mutex);

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

  pthread_t reader_thread;
  pthread_create (&reader_thread, NULL, cam_reader_thread, &server);
  pthread_t writer_thread;
  pthread_create (&writer_thread, NULL, cam_writer_thread, &server);

  int i;
  pthread_t checker_thread;
  for (i = 0; i < context->n_cpus; i++)
    {
      pthread_create (&checker_thread, NULL, cam_checker_thread, context);
      pthread_detach (checker_thread);
    }
  
  pthread_mutex_lock (&context->continue_execute_mutex);
  while (context->continue_execute != 0)
    {
      pthread_cond_wait (&context->continue_execute_sem, &context->continue_execute_mutex);
    }
  pthread_mutex_unlock (&context->continue_execute_mutex);

  close (server_socket);

  task_t end_task = { .final = !0 };
  queue_push (&context->queue, &end_task);
  result_t end_result = { .final = !0 };
  result_queue_push (&context->result_queue, &end_result);

  printf ("Server closed connection\n");
  
  printf ("Waiting for reader thread\n");
  pthread_join (reader_thread, NULL);
  printf ("Waiting for writer thread\n");
  pthread_join (writer_thread, NULL);
  printf ("Waiting for checker threads.\n");
  pthread_mutex_lock (&context->threads_mutex);
  while (context->threads)
    {
      pthread_cond_wait (&context->threads_sem, &context->threads_mutex);
    }
  pthread_mutex_unlock (&context->threads_mutex);

  printf ("End.\n");
}
