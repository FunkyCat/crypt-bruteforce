#ifndef _SHARED_H_
#define _SHARED_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define __USE_GNU
#include <crypt.h>
#include <pthread.h>
#include <semaphore.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <libxml2/libxml/tree.h>
#include <libxml2/libxml/parser.h>
#include <signal.h>

#define QUEUE_SIZE (8)
#define MAX_N (100)
#define REGISTER_SIZE (8)
#define RESULT_QUEUE_SIZE (8)

typedef char password_t[MAX_N + 1];

typedef struct task_s {
  password_t password;
  int left;
  int right;
  int final;
  int id;
  int idx;
  struct crypt_data * cd;
} task_t;

typedef struct queue_s {
  task_t queue[QUEUE_SIZE];
  sem_t full;
  sem_t empty;
  int head;
  int tail;
  pthread_mutex_t head_mutex;
  pthread_mutex_t tail_mutex;
} queue_t;

typedef struct brute_result_s {
  pthread_mutex_t found_mutex;
  password_t password;
  int found;
} brute_result_t;

typedef struct result_s {
  brute_result_t result;
  int id;
  int idx;
} result_t;

typedef enum {
  MT_SEND_JOB,
  MT_REPORT_RESULT,
} cs_message_type_t;

typedef enum {
  RM_SINGLE,
  RM_MULTI,
  RM_CLIENT,
  RM_SERVER,
  RM_CLIENT_ASYNC,
  RM_SERVER_ASYNC,
} run_mode_t;

typedef struct context_s context_t;

typedef struct context_s {
  char * alph;
  int max_length;
  char * hash;
  brute_result_t result;
  int block_size;
  run_mode_t run_mode;
  short port;
  struct in_addr addr;
  int n_cpus;

  int listener_socket;
  queue_t queue;
  int tasks_in_process;
  pthread_mutex_t tasks_in_process_mutex;
  pthread_cond_t tasks_in_process_sem;
  int threads;
  pthread_mutex_t threads_mutex;
  pthread_cond_t threads_sem;
  int continue_execute;
  pthread_mutex_t continue_execute_mutex;
  pthread_cond_t continue_execute_sem;
  int id;
  void * (*srv_client_thread)(context_t *, int);
} context_t;

typedef struct tasks_register_s {
  task_t tasks[REGISTER_SIZE];
  int free;
  pthread_mutex_t free_mutex;
  sem_t empty;
  int free_idx[REGISTER_SIZE];
} tasks_register_t;

typedef struct client_s {
  tasks_register_t tasks_register;
  int fd;
  context_t * context;
  int close;
  int err;
} client_t;


typedef struct result_queue_s {
  result_t queue[RESULT_QUEUE_SIZE];
  sem_t full;
  sem_t empty;
  int head;
  int tail;
  pthread_mutex_t head_mutex;
  pthread_mutex_t tail_mutex;
} result_queue_t;

typedef struct {
  cs_message_type_t type;
  char * alph;
  char * hash;
  task_t task;
  int result;
  password_t password;
} cs_message_t;

typedef enum {
  S_SUCCESS,
  S_MEMORY_ERROR,
  S_RECV_ERROR,
  S_SEND_ERROR,
  S_CONNECTION_CLOSED
} cs_status_t;

int check_multithread (task_t * task, context_t * context);

void * multithread_checker_thread (void * arg);

int brute_block_iterative (task_t * task, int (*handler)(task_t*, context_t *), context_t * context);

void queue_pop (queue_t * queue, task_t * dst);

void queue_push (queue_t * queue, task_t * src);

void queue_init (queue_t * queue);

int queue_push_wrapper (task_t * task, context_t * context);

int check_password (task_t * task, context_t * context);

int print_task (task_t * task, context_t * context);

void check_block (task_t * task, context_t * context);

int send_message (int, cs_message_t *);

cs_status_t recv_message (int, char **);

char * message_to_xml (cs_message_t *);

void xml_to_message (char *, cs_message_t *);

char * message_type_to_string (cs_message_type_t);

cs_message_type_t string_to_message_type (char *);

void inc_sem (int *, pthread_cond_t *, pthread_mutex_t *);

void dec_sem (int *, pthread_cond_t *, pthread_mutex_t *);

void init_sem (int *, int, pthread_cond_t *, pthread_mutex_t *);

int srv_create_listener (short, struct in_addr);

int srv_queue_push (task_t *, context_t *);

void * srv_listener_thread (void *);

int srv_listener_cycle (int, context_t *);

void srv_generate_tasks (context_t *);

void srv_init (context_t *);

void srv_wait (context_t *);

void server (context_t *);

#endif
