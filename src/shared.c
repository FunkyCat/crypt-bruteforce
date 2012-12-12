#include "shared.h"

int check_multithread (task_t * task, context_t * context)
{
  queue_init (&context->queue);
  pthread_mutex_init (&context->result.found_mutex, NULL);

  pthread_t threads[context->n_cpus];
  int i;
  for (i = 0; i < context->n_cpus; i++)
    pthread_create (&threads[i], NULL, multithread_checker_thread, (void *) context);
  task->final = 0;
  task->left = (task->right >= context->block_size) ? (task->right - context->block_size) : 0;
  brute_block_iterative (task, queue_push_wrapper, context);
  task_t final_task = 
    {
      .final = !0,
    };
  for (i = 0; i < context->n_cpus; i++)
    queue_push (&context->queue, &final_task);
  for (i = 0; i < context->n_cpus; i++)
    pthread_join (threads[i], NULL);
  return context->result.found;
}

void * multithread_checker_thread (void * arg)
{
  context_t * context = (context_t *) arg;
  task_t task;
  struct crypt_data cd = { .initialized = 0 };
  for (;;)
    {
      queue_pop (&context->queue, &task);
      if (task.final)
	return NULL;
      task.cd = &cd;
      check_block (&task, context);
    }
  return NULL;
}


int brute_block_iterative (task_t * task, int (*handler)(task_t*, context_t*), context_t * context)
{
  int alph_length = strlen (context->alph);
  int block_int[strlen (task->password)];
  int i;

  if (task->right <= task->left)
    {
      return handler(task, context);
    }
  for (i = task->left; i < task->right; i++)
    {
      block_int[i] = 0;
      task->password[i] = context->alph[0];
    }
  for(;;)
    {
      int current_pos;
      if (handler (task, context))
	return !0;
      for (current_pos = task->right - 1; (current_pos >= task->left) && (block_int[current_pos] == alph_length - 1); --current_pos)
	{
	  block_int[current_pos] = 0;
	  task->password[current_pos] = context->alph[0];
	}
      if (current_pos < task->left)
	{
	  break;
	}
      task->password[current_pos] = context->alph[ ++block_int[current_pos] ];
    }
  return 0;
}

void queue_pop (queue_t * queue, task_t * dst)
{
  sem_wait (&queue->full);
  pthread_mutex_lock (&queue->tail_mutex);
  *dst = queue->queue[queue->tail];
  if (++queue->tail == sizeof (queue->queue) / sizeof (queue->queue[0]))
    {
      queue->tail = 0;
    }
  pthread_mutex_unlock (&queue->tail_mutex);
  sem_post (&queue->empty);
}

void queue_push (queue_t * queue, task_t * src)
{
  sem_wait (&queue->empty);
  pthread_mutex_lock (&queue->head_mutex);
  queue->queue[queue->head] = *src;
  if (++queue->head == sizeof (queue->queue) / sizeof (queue->queue[0]))
    {
      queue->head = 0;
    }
  pthread_mutex_unlock (&queue->head_mutex);
  sem_post (&queue->full);
}

void queue_init (queue_t * queue)
{
  pthread_mutex_init (&queue->head_mutex, NULL);
  pthread_mutex_init (&queue->tail_mutex, NULL);
  sem_init (&queue->full, 0, 0);
  sem_init (&queue->empty, 0, QUEUE_SIZE);
  queue->head = 0;
  queue->tail = 0;
}

void result_queue_push (result_queue_t * result_queue, result_t * src)
{
  sem_wait (&result_queue->empty);
  pthread_mutex_lock (&result_queue->head_mutex);
  result_queue->queue[result_queue->head] = *src;
  if (++result_queue->head == sizeof (result_queue->queue) / sizeof (result_queue->queue[0]))
    {
      result_queue->head = 0;
    }
    pthread_mutex_unlock (&result_queue->head_mutex);
    sem_post (&result_queue->full);
}

void result_queue_pop (result_queue_t * result_queue, result_t * dst)
{
  sem_wait (&result_queue->full);
  pthread_mutex_lock (&result_queue->tail_mutex);
  *dst = result_queue->queue[result_queue->tail];
  if (++result_queue->tail == sizeof (result_queue->queue) / sizeof (result_queue->queue[0]))
    {
      result_queue->tail = 0;
    }
  pthread_mutex_unlock (&result_queue->tail_mutex);
  sem_post (&result_queue->empty);
}

void result_queue_init (result_queue_t * result_queue)
{
  pthread_mutex_init (&result_queue->head_mutex, NULL);
  pthread_mutex_init (&result_queue->tail_mutex, NULL);
  sem_init (&result_queue->full, 0, 0);
  sem_init (&result_queue->empty, 0, RESULT_QUEUE_SIZE);
  result_queue->head = 0;
  result_queue->tail = 0;
}    

int queue_push_wrapper (task_t * task, context_t * context)
{
  queue_push (&context->queue, task);
  return (context->result.found);
}

int check_password (task_t * task, context_t * context)
{
  char * password_hash = crypt_r (task->password, context->hash, task->cd);
  if (strcmp (password_hash, context->hash) == 0)
    {
      pthread_mutex_lock (&context->result.found_mutex);
      context->result.found = !0;
      strcpy (context->result.password, task->password);
      pthread_mutex_unlock (&context->result.found_mutex);
    }
  return context->result.found;
}

int print_task (task_t * task, context_t * context)
{
  printf ("%s\n", task->password);
  return 0;
}

void check_block (task_t * task, context_t * context)
{
  task->right = task->left;
  task->left = 0;
  brute_block_iterative (task, check_password, context);
}

int send_message (int sockid, cs_message_t * message)
{
  char * messxml = message_to_xml (message);
  int32_t length = strlen (messxml) + 1;
  int32_t len = htonl (length);
  int s1_res = send (sockid, &len, sizeof (len), 0);
  if (s1_res < 0)
    {
      fprintf (stderr, "error: send()\n");
      free (messxml);
      return -1;
    }
  else if (s1_res == 0)
    {
      free(messxml);
      return 0;
    }
  int s2_res = send (sockid, messxml, length, 0);
  free (messxml);
  if (s2_res < 0)
    {
      fprintf (stderr, "error: send()\n");
      return -1;
    }
  return s1_res + s2_res;
}

cs_status_t recv_message (int sockid, char ** buffer)
{
  int32_t length;
  int bytes_read;
  int processed;
  length = 0;
  bytes_read = recv (sockid, &length, sizeof (length), 0);
  if (bytes_read == 0)
    return S_CONNECTION_CLOSED;
  if (bytes_read < 0)
    {
      fprintf (stderr, "error: recv()_1\n");
      return S_RECV_ERROR;
    }
  length = ntohl (length);
      
  *buffer = malloc (length);
  if (!*buffer)
    {
      fprintf (stderr, "error: malloc()\n");
      return S_MEMORY_ERROR;
    }

  for (processed = 0; processed < length; processed += bytes_read)
    {
      bytes_read = recv (sockid, *buffer + processed,
			 length - processed, 0);
      if (bytes_read == 0)
	{
	  fprintf (stderr, "error: bytes_read == 0\n");
	  free (*buffer);
	  return S_CONNECTION_CLOSED;
	}
      if (bytes_read < 0){
	fprintf (stderr, "error: recv()_2\n");
	free (*buffer);
	return S_RECV_ERROR;
      }
    }
  return S_SUCCESS;
}

char * message_to_xml (cs_message_t * message)
{
  xmlDocPtr doc;
  char * ret;
  char tmp[256];

  doc = xmlNewDoc ((xmlChar *) "1.0");
  
  if (message->type == MT_SEND_JOB)
    {
      xmlNodePtr msg_send = xmlNewDocNode (doc, NULL, (xmlChar *) "msg_send", NULL);
      xmlDocSetRootElement (doc, msg_send);
      xmlNewChild (msg_send, NULL, (xmlChar *) "type", (xmlChar *) "MT_SEND_JOB");
      xmlNodePtr node  = xmlNewChild (msg_send, NULL, (xmlChar *) "args", NULL);
      
      node = xmlNewChild (node, NULL, (xmlChar *) "job", NULL);
      node = xmlNewChild (node, NULL, (xmlChar *) "job", NULL);
      xmlNewChild (node, NULL, (xmlChar *) "password", (xmlChar *) message->task.password);
      sprintf (tmp, "%d", message->task.id);
      xmlNewChild (node, NULL, (xmlChar *) "id", (xmlChar *) &tmp);
      sprintf (tmp, "%d", message->task.idx);
      xmlNewChild (node, NULL, (xmlChar *) "idx", (xmlChar *) &tmp);
      xmlNewChild (node, NULL, (xmlChar *) "hash", (xmlChar *) message->hash);
      xmlNewChild (node, NULL, (xmlChar *) "alphabet", (xmlChar *) message->alph);
      sprintf (tmp, "%d", message->task.left);
      xmlNewChild (node, NULL, (xmlChar *) "from", (xmlChar *) &tmp);
      sprintf (tmp, "%d", message->task.right);
      xmlNewChild (node, NULL, (xmlChar *) "to", (xmlChar *) &tmp);
    }
  else
    {
      xmlNodePtr msg_recv = xmlNewDocNode (doc, NULL, (xmlChar *) "msg_recv", NULL);
      xmlDocSetRootElement (doc, msg_recv);
      
      xmlNewChild (msg_recv, NULL, (xmlChar *) "type", (xmlChar *) "MT_REPORT_RESULTS");
      xmlNodePtr node  = xmlNewChild (msg_recv, NULL, (xmlChar *) "args", NULL);
      node = xmlNewChild (node, NULL, (xmlChar *) "result", NULL);
      node = xmlNewChild (node, NULL, (xmlChar *) "result", NULL);
      xmlNodePtr pwd_node = xmlNewChild (node, NULL, (xmlChar *) "password", NULL);
      sprintf (tmp, "%d", message->task.id);
      xmlNewChild (node, NULL, (xmlChar *) "id", (xmlChar *) &tmp);
      sprintf (tmp, "%d", message->task.idx);
      xmlNewChild (node, NULL, (xmlChar *) "idx", (xmlChar *) &tmp);
      if (message->result)
	{
	  xmlNewChild (node, NULL, (xmlChar *) "password_found", (xmlChar *) "1");
	  xmlNodeSetContent (pwd_node, (xmlChar *) message->password);
	}
      else
	{
	  xmlNewChild (node, NULL, (xmlChar *) "password_found", (xmlChar *) "0");
	}
      
    }
  
  xmlDocDumpMemory (doc, (xmlChar **) &ret, NULL);
  xmlFreeDoc (doc);
  return ret;
}

void xml_to_message (char * xml, cs_message_t * mess)
{
  xmlDocPtr doc;
  xmlNodePtr root;
  xmlChar * tmp;

  doc = xmlParseDoc ((xmlChar *) xml);
  root = xmlDocGetRootElement (doc);
  
  if (strcmp ((char *) root->name, "msg_send") == 0)
    {
      mess->type = MT_SEND_JOB;
      xmlNodePtr node;
      for (node = root->children; node != NULL; node = node->next)
	{

	  if (strcmp ((char *) node->name, "args") == 0)
	    {
	      node = node->children->children;
	      xmlNodePtr arg;
	      char * name;
	      for (arg = node->children; arg != NULL; arg = arg->next)
		{
		  name = (char *) arg->name;		  
		  if (strcmp (name, "password") == 0)
		    {
		      tmp = xmlNodeGetContent (arg);
		      strcpy (mess->task.password, (char *) tmp);
		      free (tmp);
		    }
		  else if (strcmp (name, "id") == 0)
		    {
		      tmp = xmlNodeGetContent (arg);
		      mess->task.id = atoi ((char *) tmp);
		      free (tmp);
		    }
		  else if (strcmp (name, "idx") == 0)
		    {
		      tmp = xmlNodeGetContent (arg);
		      mess->task.idx = atoi ((char *) tmp);
		      free (tmp);
		    }
		  else if (strcmp (name, "hash") == 0)
		    {
		      mess->hash = (char *) xmlNodeGetContent (arg);
		    }
		  else if (strcmp (name, "alphabet") == 0)
		    {
		      mess->alph = (char *) xmlNodeGetContent (arg);
		    }
		  else if (strcmp (name, "from") == 0)
		    {
		      tmp = xmlNodeGetContent (arg);
		      mess->task.left = atoi ((char *) tmp);
		      free (tmp);
		    }
		  else if (strcmp (name, "to") == 0)
		    {
		      tmp = xmlNodeGetContent (arg);
		      mess->task.right = atoi ((char *) tmp);
		      free (tmp);
		    }
		}
	    }
	  else 
	    {
	      continue;
	      }

	}
    }
  else
    {
      mess->type = MT_REPORT_RESULT;
      xmlNodePtr node;
      for (node = root->children; node != NULL; node = node->next)
	{
	  if (strcmp ((char *) node->name, "args") == 0)
	    {
	      node = node->children->children;
	      xmlNodePtr arg;
	      char * name;
	      for (arg = node->children; arg != NULL; arg = arg->next)
		{
		  name = (char *) arg->name;
		  if (strcmp (name, "id") == 0)
		    {
		      tmp = xmlNodeGetContent (arg);
		      mess->task.id = atoi ((char *) tmp);
		      free (tmp);
		    }
		  else if (strcmp (name, "idx") == 0)
		    {
		      tmp = xmlNodeGetContent (arg);
		      mess->task.idx = atoi ((char *) tmp);
		      free (tmp);
		    }
		  else if (strcmp (name, "password_found") == 0)
		    {
		      tmp = xmlNodeGetContent (arg);
		      if (strcmp ((char *) tmp, "0") == 0)
			mess->result = 0;
		      else
			mess->result = !0;
		      free (tmp);
		    }
		  else if (strcmp (name, "password") == 0)
		    {
		      tmp = xmlNodeGetContent (arg);
		      if (tmp != NULL)
			{
			  strcpy (mess->password, (char *) tmp);
			  free (tmp);
			}
		    }
		}
	    }
	  else
	    {
	      continue;
	    }

	}
    }
  xmlFreeDoc (doc);
}

char * message_type_to_string (cs_message_type_t mt)
{
  switch (mt)
    {
    case MT_SEND_JOB:
      return "MT_SEND_JOB";
      break;
    case MT_REPORT_RESULT:
      return "MT_REPORT_RESULT";
      break;
    }
  return NULL;
}

cs_message_type_t string_to_message_type (char * mt)
{
  if (strcmp (mt, "MT_SEND_JOB") == 0)
    return MT_SEND_JOB;
  return MT_REPORT_RESULT;
}

void inc_sem (int * variable, pthread_cond_t * sem, pthread_mutex_t * mutex)
{
  pthread_mutex_lock (mutex);
  (*variable)++;
  pthread_mutex_unlock (mutex);
}

void dec_sem (int * variable, pthread_cond_t * sem, pthread_mutex_t * mutex)
{
  pthread_mutex_lock (mutex);
  (*variable)--;
  pthread_mutex_unlock (mutex);
  if (*variable == 0)
    pthread_cond_signal (sem);
}

void init_sem (int * variable, int value, pthread_cond_t * sem, pthread_mutex_t * mutex)
{
  (*variable) = value;
  pthread_cond_init (sem, NULL);
  pthread_mutex_init (mutex, NULL);
}
  

int srv_create_listener (short port, struct in_addr addr)
{
  int listener;
  struct sockaddr_in sock_addr;

  listener = socket (AF_INET, SOCK_STREAM, 0);
  if (listener < 0){
    fprintf (stderr, "error: socket()\n");
    return -1;
  }
  sock_addr.sin_family = AF_INET;
  sock_addr.sin_port = htons (port);
  sock_addr.sin_addr = addr;
  if (bind (listener, (struct sockaddr *) &sock_addr, sizeof (sock_addr)) < 0)
    {
      fprintf (stderr, "error: bind()\n");
      exit (EXIT_FAILURE);
    }
  return listener;
}

int srv_queue_push (task_t * task, context_t * context)
{
  inc_sem (&context->tasks_in_process, &context->tasks_in_process_sem, &context->tasks_in_process_mutex);
  int old_right = task->right;
  task->right = task->left;
  task->left = 0;
  task->id = context->id++;
  queue_push (&context->queue, task);
  task->left = task->right;
  task->right = old_right;
  return context->result.found;
}

void * srv_listener_thread (void * args)
{
  context_t * context = (context_t *) args;

  int listener;
  listener = srv_create_listener (context->port, context->addr);
  if (listener < 0)
    {
      fprintf (stderr, "error: srv_create_listener()\n");
      return NULL;
    }
  listen (listener, 1);
  printf ("listening...\n");
  if (srv_listener_cycle (listener, context) < 0)
    {
      fprintf (stderr, "error: srv_listener_cycle()\n");
      return NULL;
    }
  close (listener);

  printf ("tm_listener_thread: end\n");
  return NULL;
}

int srv_listener_cycle (int listener, context_t * context)
{
  int client;
  pthread_t thread;
  
  void * srv_client_thread_wrapper (void * arg)
  {
    return (context->srv_client_thread (context, (int)arg));
  }
  
  for (;;)
    {
      client = accept (listener, NULL, NULL);
      printf ("Client accepted: %d\n", client);
      if (client < 0)
	{
	  fprintf (stderr, "error: accept()\n");
	  return -1;
	}
      pthread_create (&thread, NULL, srv_client_thread_wrapper, (void *) client);
      pthread_detach (thread);
    }
  return 0;
}

void srv_generate_tasks (context_t * context)
{
  int length;
  for (length = 1; length <= context->max_length; length++)
    {
      task_t task;
      memset (task.password, context->alph[0], sizeof (task.password));
      task.password[length] = (char)0;
      task.final = 0;
      if (context->block_size > length)
	{
	  struct crypt_data cd = { .initialized = 0 };
	  task.left = 0;
	  task.right = length;
	  task.cd = &cd;
	  if (brute_block_iterative (&task, check_password, context))
	    {
	      break;
	    }
	}
      else
	{
	  task.left = context->block_size;
	  task.right = length;
	  if (brute_block_iterative (&task, srv_queue_push, context))
	    {
	      break;
	    }
	}
    }
}

void srv_init (context_t * context)
{
  queue_init (&context->queue);
  pthread_mutex_init (&context->result.found_mutex, NULL);
  signal (SIGPIPE, SIG_IGN);
  init_sem (&context->tasks_in_process, 0, &context->tasks_in_process_sem, &context->tasks_in_process_mutex);
  init_sem (&context->threads, 0, &context->threads_sem, &context->threads_mutex);
}

void srv_wait (context_t * context)
{
  if ((context->run_mode == RM_SERVER && context->result.found) || (context->run_mode == RM_SERVER_ASYNC && !context->result.found))
    {
      pthread_mutex_lock (&context->tasks_in_process_mutex);
      while (context->tasks_in_process != 0)
	pthread_cond_wait (&context->tasks_in_process_sem, &context->tasks_in_process_mutex);
      pthread_mutex_unlock (&context->tasks_in_process_mutex);
    }
  if (context->run_mode == RM_SERVER_ASYNC)
    {
      pthread_mutex_lock (&context->continue_execute_mutex);
      context->continue_execute = 0;
      pthread_mutex_unlock (&context->continue_execute_mutex);
      pthread_cond_broadcast (&context->continue_execute_sem);
    }
  printf ("Wait for threads...\n");
  pthread_mutex_lock (&context->threads_mutex);
  while (context->threads != 0)
    {
      pthread_cond_wait (&context->threads_sem, &context->threads_mutex);
    }
  pthread_mutex_unlock (&context->threads_mutex);
}

void server (context_t * context)
{
  srv_init (context);

  pthread_t listener_thread;
  pthread_create (&listener_thread, NULL, srv_listener_thread, context);
  pthread_detach (listener_thread);

  printf ("generation started\n");

  srv_generate_tasks (context);

  if (/*context->run_mode == RM_SERVER*/ !0)
    {
      task_t end_task = { .final = !0 };
      queue_push (&context->queue, &end_task);
    }

  srv_wait (context);
 
  pthread_cancel (listener_thread);
}

int cli_create_socket (short port, struct in_addr addr)
{
  int sock;
  struct sockaddr_in sock_addr;

  sock = socket (AF_INET, SOCK_STREAM, 0);
  if (sock < 0)
  {
    fprintf (stderr, "error: socket()\n");
    return -1;
  }

  sock_addr.sin_family = AF_INET;
  sock_addr.sin_port = htons (port);
  sock_addr.sin_addr = addr;

  if (connect (sock, (struct sockaddr *) &sock_addr, sizeof (sock_addr)) < 0)
  {
    fprintf (stderr, "error: connect()\n");
    return -1;
  }

  return sock;
}
