#include "client.h"

int cm_socket_cycle (int socket, context_t * context)
{
  char * buffer;
  cs_message_t message;
  cs_status_t recv_status;
  int send_status;
  for (;;)
    {
      recv_status = recv_message (socket, &buffer);
      if (recv_status == S_SUCCESS)
	{
	  xml_to_message (buffer, &message);
	  printf ("task: %s [%d; %d)... ", message.task.password, message.task.left, message.task.right);
	  free (buffer);

	  context->hash = message.hash;
	  context->alph = message.alph;
	  context->result.found = 0;
	  check_multithread (&message.task, context);
	  free (context->hash);
	  free (context->alph);
	  
	  message.type = MT_REPORT_RESULT;

	  if (context->result.found)
	    {
	      printf ("found: %s\n", context->result.password);
	      message.result = !0;
	      strcpy (message.password, context->result.password);
	    }
	  else
	    {
	      printf ("not found\n");
	      message.result = 0;
	    }
	  if ((send_status = send_message (socket, &message)) <= 0)
	    {
	      fprintf (stderr, "error: send_message()\n");
	      return -1;
	    }
	}
      else if (recv_status == S_CONNECTION_CLOSED)
	{
	  printf ("Server closed connection\n");
	  return 0;
	}
      else
	{
	  fprintf (stderr, "error: recv_message()\n");
	  return -1;
	}
    }
  
  return 0;
}

void client_mode (context_t * context)
{
  context->n_cpus =  (int) sysconf (_SC_NPROCESSORS_ONLN);
  printf ("n_cpus = %d\n", context->n_cpus);

  int sock = cli_create_socket (context->port, context->addr);
  if (sock < 0)
    {
      fprintf (stderr, "error: cm_create_socket()\n");
      return;
    }

  if (cm_socket_cycle (sock, context) < 0)
    {
      fprintf (stderr, "error: cm_socket_cycle()\n");
      return;
    }
  
  close (sock);
}
