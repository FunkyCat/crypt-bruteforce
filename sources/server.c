#include "server.h"

int tm_client_process (int client, context_t * context)
{
  char * buffer;
  cs_message_t message;
  cs_status_t recv_status;
  int send_status;
  task_t task;
  int ret = 0;
  message.alph = context->alph;
  for (;;)
    {
      queue_pop (&context->queue, &task);
      if (task.final)
	{
	  queue_push (&context->queue, &task);
	  return 0;
	}

      if (context->result.found)
	{
	  dec_sem (&context->tasks_in_process, &context->tasks_in_process_sem, &context->tasks_in_process_mutex);
	  continue;
	}

      message.type = MT_SEND_JOB;
      message.alph = context->alph;
      message.hash = context->hash;
      message.task = task;
      printf ("(%d) task: %s [%d; %d)\n", client, message.task.password, message.task.left, message.task.right);
      send_status = send_message (client, &message);
      if (send_status == -1)
	{
	  fprintf (stderr, "error: send_message()\n");
	  ret = -1;
	  break;
	}
      else if (send_status == 0)
	{
	  break;
	}
      
      recv_status = recv_message (client, &buffer);
      if (recv_status == S_SUCCESS)
	{
	  xml_to_message (buffer, &message);
	  free (buffer);
	  if (message.result)
	    {
	      context->result.found = !0;
	      strcpy (context->result.password, message.password);
	    }
	  dec_sem (&context->tasks_in_process, &context->tasks_in_process_sem, &context->tasks_in_process_mutex);
	}
      else if (recv_status == S_CONNECTION_CLOSED)
	{
	  free (buffer);
	  break;
	}
      else
	{
	  free (buffer);
	  ret = -1;
	  fprintf (stderr, "error: recv_message()\n");
	  break;
	}
    }
  queue_push (&context->queue, &task);
  return ret;
}

void * tm_client_thread (context_t * context, int client_socket)
{
  inc_sem (&context->threads, &context->threads_sem, &context->threads_mutex);

  printf ("(%d) connected\n", client_socket);
  if (tm_client_process (client_socket, context) < 0)
    {
      fprintf (stderr, "error: tm_client_process()\n");
    }

  printf ("(%d) disconnected\n", client_socket);
  close (client_socket);
  
  dec_sem (&context->threads, &context->threads_sem, &context->threads_mutex);
  return NULL;
}


void server_mode (context_t * context)
{
  context->srv_client_thread = tm_client_thread;

  server (context);
}
