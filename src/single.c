#include "single.h"

void single_mode (context_t * context)
{
  int length;
  struct crypt_data cd = { .initialized = 0 };
  for (length = 1; length <= context->max_length; length++)
    {
      task_t task;
      memset (task.password, context->alph[0], sizeof (task.password));
      task.password[length] = (char)0;
      task.final = 0;
      task.left = 0;
      task.right = length;
      task.cd = &cd;
      if (brute_block_iterative (&task, check_password, context))
	{
	  break;
	}
    }
}
