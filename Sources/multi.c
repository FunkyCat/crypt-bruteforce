#include "multi.h"


void multi_mode (context_t * context)
{
  context->n_cpus = (int) sysconf (_SC_NPROCESSORS_ONLN);
  printf ("n_cpus = %d\n", context->n_cpus);

  int length;
  for (length = 1; length <= context->max_length; length++)
    {
      task_t task;
      memset (task.password, context->alph[0], sizeof (task.password));
      task.password[length] = (char)0;
      task.final = 0;
      task.left = 0;
      task.right = length;
      if (check_multithread (&task, context))
	{
	  break;
	}
    }
}

