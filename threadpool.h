#ifndef THREADPOOL_H_
#define THREADPOOL_H_

#include <semaphore.h>

#define TASK_MAX_NUM	100
#define THREAD_IDLE_REDUNDANCE_MAX	50
#define THREAD_IDLE_REDUNDANCE_MIN	3
#define THREAD_DEF_NUM	20

typedef struct TASK_NODE{
	pthread_mutex_t mutex;
	void *arg;
	void *(*func)(void *);
	pthread_t pid;
	int work_id;
	int is_work;
	struct TASK_NODE *next;
}task_node;

typedef struct TASK_PARAMETER{
		void *arg;
		void *(*func)(void *);
}task_parameter;

typedef struct TASK_QUEUE{
	int task_queue_size;
	pthread_mutex_t mutex;
	sem_t NewTaskToExecute;
	struct TASK_NODE *head;
	struct TASK_NODE *rear;
}task_queue;

typedef struct PTHREAD_NODE{
	pthread_cond_t cond;
	pthread_mutex_t mutex;
	pthread_t pid;
	int is_execute_task;
	int pthread_exit_flag;
	struct TASK_NODE *task;
	struct PTHREAD_NODE *next;
}pthread_node;

typedef struct PTHREAD_QUEUE{
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	int pthread_queue_size;
	struct PTHREAD_NODE *head;
	struct PTHREAD_NODE *rear;
}pthread_queue;

void *single_pthread_work(void *arg);
int create_pthread_pool();
void *pthread_manager(void *arg);
int init_pthread_pool();
int AddTaskToQueue(task_node *NewTask);
void monitor_pthread_pool();

#endif
