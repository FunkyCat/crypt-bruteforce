#include "shared.h"
#include "single.h"
#include "multi.h"
#include "client.h"
#include "server.h"
#include "client_async.h"
#include "server_async.h"
 
int parse_args (int argc, char * argv[], context_t * context)
{
  int opt;
  while ((opt = getopt (argc, argv, "smctlbpahxy")) != -1)
    {
      switch (opt)
	{
	case 's':
	  context->run_mode = RM_SINGLE;
	  break;
	case 'm':
	  context->run_mode = RM_MULTI;
	  break;
	case 'c':
	  context->run_mode = RM_CLIENT;
	  break;
	case 't':
	  context->run_mode = RM_SERVER;
	  break;
	case 'x':
	  context->run_mode = RM_SERVER_ASYNC;
	  break;
	case 'y':
	  context->run_mode = RM_CLIENT_ASYNC;
	  break;
	case 'l':
	  context->max_length = atoi (argv[optind++]);
	  break;
	case 'b':
	  context->block_size = atoi (argv[optind++]);
	  break;
	case 'a':
	  context->addr.s_addr = inet_addr (argv[optind++]);
	  break;
	case 'p':
	  context->port = atoi (argv[optind++]);
	  break;
	case 'h':
	  context->hash = argv[optind++];
	  break;
	default:
	  fprintf (stderr, "Usage: %s [-smctlbaph]\n", argv[0]);
	  exit (EXIT_FAILURE);
	}
    }
      
  if (context->block_size < 0)
    {
      fprintf (stderr, "Block size parameter can't be less then 0\n");
      exit (EXIT_FAILURE);
    }

  if (context->max_length < 1)
    {
      fprintf (stderr, "Max length parameter can't be less then 1\n");
      exit (EXIT_FAILURE);
    }

  if (context->port < 1 || context->port > 65535)
    {
      fprintf (stderr, "Port value should be between 1 and 65535\n");
      exit (EXIT_FAILURE);
    }

  if ((context->hash == NULL) && (context->run_mode != RM_CLIENT) && (context->run_mode != RM_CLIENT_ASYNC))
    {
      fprintf (stderr, "error: hash not defined\n");
      exit (EXIT_FAILURE);
    }

  switch (context->run_mode)
    {
    case RM_SINGLE:
      printf ("run_mode = RM_SINGLE\n");
      printf ("hash = %s\n", context->hash);
      break;
    case RM_MULTI:
      printf ("run_mode = RM_MULTI\n");
      printf ("hash = %s\n", context->hash);
      break;
    case RM_CLIENT:
      printf ("run_mode = RM_CLIENT\n");
      printf ("port = %d\n", context->port);
      printf ("addr = %s\n", inet_ntoa (context->addr));
      break;
    case RM_SERVER:
      printf ("run_mode = RM_SERVER\n");
      printf ("port = %d\n", context->port);
      break;
    case RM_CLIENT_ASYNC:
      printf ("run_mode = RM_CLIENT_ASYNC\n");
      printf ("port = %d\n", context->port);
      printf ("addr = %s\n", inet_ntoa (context->addr));
      break;
    case RM_SERVER_ASYNC:
      printf ("run_mode = RM_SERVER_ASYNC\n");
      printf ("port = %d\n", context->port);
      break;
    
    }
  printf ("max_length = %d\n", context->max_length);
  printf ("block_size = %d\n", context->block_size);
  return 0;
}

int main (int argc, char* argv[])
{
  /*
    csit = aaqrSA4jvjoW2
    aaaaaa = aa4YsP554yc3s
    abcdef = aaoLCKdLho5jo
    hello = aaPwJ9XL9Y99E
    zzzz = aazMmYv6wT7dg
  */
  
  printf ("zzzz = %s\n", crypt ("zzzz", "aa"));

  context_t context = {
    .alph = "abcdefghijklmnopqrstuvwxyz",
    .max_length = 6,
    .result = {
      .found = 0,
      .password[0] = 0,
    },
    .block_size = 1,
    .run_mode = RM_MULTI,
    .port = 3456,
    .addr.s_addr = INADDR_ANY,
    .n_cpus = 1,
    .hash = NULL,
    .listener_socket = -1,
    .id = 0,
  };

  parse_args (argc, argv, &context);

  switch (context.run_mode)
    {
    case RM_SINGLE:
      single_mode (&context);
      break;
    case RM_MULTI:
      multi_mode (&context);
      break;
    case RM_CLIENT:
      client_mode (&context);
      break;
    case RM_SERVER:
      server_mode (&context);
      break;
    case RM_SERVER_ASYNC:
      server_async_mode (&context);
      break;
    case RM_CLIENT_ASYNC:
      client_async_mode (&context);
      break;
    }

  if (context.run_mode != RM_CLIENT && context.run_mode != RM_CLIENT_ASYNC)
    {
      if (context.result.found)
	{
	  printf ("Found! Password: %s\n", context.result.password);
	}
      else
	{
	  printf ("Not found\n");
	}
    }

  return (EXIT_SUCCESS); 
}

