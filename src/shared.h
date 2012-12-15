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
#include <sys/epoll.h>
#include <fcntl.h>

#define QUEUE_SIZE (8)
#define MAX_N (100)
#define REGISTER_SIZE (8)
#define RESULT_QUEUE_SIZE (8)
#define EPOLL_MAX_EVENTS (10)

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
  int final;
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
  RM_SERVER_EPOLL
} run_mode_t;

typedef struct result_queue_s {
  result_t queue[RESULT_QUEUE_SIZE];
  sem_t full;
  sem_t empty;
  int head;
  int tail;
  pthread_mutex_t head_mutex;
  pthread_mutex_t tail_mutex;
} result_queue_t;

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
  result_queue_t result_queue;
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

typedef struct server_s {
  int fd;
  context_t * context;
} server_t;

typedef struct {
  cs_message_type_t type;
  char * alph;
  char * hash;
  task_t task;
  int result;
  password_t password;
} cs_message_t;

typedef enum {
  S_SUCCESS = 0,
  S_MEMORY_ERROR,
  S_RECV_ERROR,
  S_SEND_ERROR,
  S_CONNECTION_CLOSED
} cs_status_t;

typedef enum {
  ESS_HEAD,
  ESS_BODY
} epoll_state_status_t;

typedef struct epoll_state_s {
  char * buffer;
  epoll_state_status_t status;
  int bytes;
  int32_t total;
} epoll_state_t;

struct epoll_client_s;
struct reactor_s;

typedef int (*event_handler_t)(struct epoll_client_s *, struct reactor_s *, struct epoll_event *);

typedef struct epoll_client_s {
  int fd;
  epoll_state_t read_state;
  epoll_state_t write_state;
  event_handler_t read;
  event_handler_t write;
} epoll_client_t;

typedef struct reactor_s {
  context_t * context;
  int epollfd;
} reactor_t;

int check_multithread (task_t *, context_t *);

void * multithread_checker_thread (void *);

int brute_block_iterative (task_t *, int (*handler)(task_t*, context_t *), context_t *);

void queue_pop (queue_t *, task_t *);

void queue_push (queue_t *, task_t *);

void queue_init (queue_t *);

void result_queue_pop (result_queue_t *, result_t *);

void result_queue_push (result_queue_t *, result_t *);

void result_queue_init (result_queue_t *);

int queue_push_wrapper (task_t *, context_t *);

int check_password (task_t *, context_t *);

int print_task (task_t *, context_t *);

void check_block (task_t *, context_t *);

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

int cli_create_socket (short, struct in_addr addr);

#endif
