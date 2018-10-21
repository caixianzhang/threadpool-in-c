#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "threadpool.h"

pthread_queue *pthread_queue_idle;

int pthread_pool_size = THREAD_DEF_NUM;

pthread_t pthread_manager_pid;
pthread_t monitor_pthread_pool_pid;
task_queue *waiting_task_queue;

void *single_pthread_work(void * arg)
{
	pthread_detach(pthread_self());

	pthread_node *self = (pthread_node *)arg;

	pthread_mutex_lock(&self->mutex);
	self->pid = pthread_self();
	pthread_mutex_unlock(&self->mutex);

	while(1)
	{
		pthread_mutex_lock(&self->mutex);
		if(self->task == NULL)
		{
			pthread_cond_wait(&self->cond, &self->mutex);
			if(self->pthread_exit_flag == 1)
			{
				pthread_mutex_unlock(&self->mutex);
				pthread_mutex_destroy(&self->mutex);
				pthread_cond_destroy(&self->cond);
				free(self);

				return (void *)0;
			}
		}
		
		pthread_mutex_lock(&self->task->mutex);
		self->task->func(self->task->arg);
		free(self->task->arg);
		self->task->arg = NULL;
		
		pthread_mutex_unlock(&self->task->mutex);
		pthread_mutex_destroy(&self->task->mutex);

		free(self->task);
		self->task = NULL;
		self->is_execute_task = 0;
		
		pthread_mutex_lock(&pthread_queue_idle->mutex);
		if(pthread_queue_idle->pthread_queue_size == 0)
		{
			pthread_queue_idle->head = pthread_queue_idle->rear = self;
			self->next = NULL;
		}else
		{
			pthread_mutex_lock(&pthread_queue_idle->rear->mutex);
			pthread_queue_idle->rear->next = self;
			pthread_mutex_unlock(&pthread_queue_idle->rear->mutex);

			self->next = NULL;
			pthread_queue_idle->rear = self;
		}

		pthread_queue_idle->pthread_queue_size++;
		pthread_mutex_unlock(&pthread_queue_idle->mutex);
		
		pthread_mutex_unlock(&self->mutex);

		pthread_mutex_lock(&pthread_queue_idle->mutex);
		if(pthread_queue_idle->pthread_queue_size == 0)
		{
			pthread_cond_signal(&pthread_queue_idle->cond);
		}
		pthread_mutex_unlock(&pthread_queue_idle->mutex);
	}
	return (void *)0;
}

int create_pthread_pool()
{
	pthread_node *pre_pthread;
	pthread_node *next_pthread;

	int i;
	for(i = 0; i < pthread_pool_size; i++)
	{
		next_pthread = (pthread_node *)malloc(sizeof(pthread_node));
		if(i == 0)
		{
			next_pthread->pid = 0;
			next_pthread->is_execute_task = 0;
			next_pthread->pthread_exit_flag = 0;
			next_pthread->task = NULL;
			next_pthread->next = NULL;
			pthread_cond_init(&next_pthread->cond, NULL);
			pthread_mutex_init(&next_pthread->mutex, NULL);
			pthread_create(&next_pthread->pid, NULL, single_pthread_work, next_pthread);

			pthread_queue_idle->head = pre_pthread = next_pthread;

			continue;
		}

		if(i == pthread_pool_size - 1)
		{
			next_pthread->pid = 0;
			next_pthread->is_execute_task = 0;
			next_pthread->pthread_exit_flag = 0;
			next_pthread->task = NULL;
			next_pthread->next = NULL;

			pthread_mutex_lock(&pre_pthread->mutex);
			pre_pthread->next = next_pthread;
			pthread_mutex_unlock(&pre_pthread->mutex);

			pthread_cond_init(&next_pthread->cond, NULL);
			pthread_mutex_init(&next_pthread->mutex, NULL);
			pthread_create(&next_pthread->pid, NULL, single_pthread_work, next_pthread);

			pthread_queue_idle->rear = pre_pthread = next_pthread;

			continue;
		}

		next_pthread->pid = 0;
		next_pthread->is_execute_task = 0;
		next_pthread->pthread_exit_flag = 0;
		next_pthread->task = NULL;

		pthread_mutex_lock(&pre_pthread->mutex);
		pre_pthread->next = next_pthread;
		pthread_mutex_unlock(&pre_pthread->mutex);

		pthread_cond_init(&next_pthread->cond, NULL);
		pthread_mutex_init(&next_pthread->mutex, NULL);
		pthread_create(&next_pthread->pid, NULL, single_pthread_work, next_pthread);

		pre_pthread = next_pthread;
	}
	pthread_queue_idle->pthread_queue_size = pthread_pool_size;

	return 0;
}

void *pthread_manager(void * arg)
{
	pthread_detach(pthread_self());
	while(1)
	{
		sem_wait(&waiting_task_queue->NewTaskToExecute);
		pthread_node *temp_pthread = NULL;
		task_node *temp_task = NULL;
		pthread_mutex_lock(&waiting_task_queue->mutex);
		temp_task = waiting_task_queue->head;
		if(waiting_task_queue->task_queue_size == 1)
		{
			waiting_task_queue->head =waiting_task_queue->rear = NULL;
		}else
		{
			waiting_task_queue->head = temp_task->next;
		}

		waiting_task_queue->task_queue_size--;

		pthread_mutex_unlock(&waiting_task_queue->mutex);
		
		pthread_mutex_lock(&pthread_queue_idle->mutex);

		if(pthread_queue_idle->pthread_queue_size == 0)
		{
			pthread_cond_wait(&pthread_queue_idle->cond, &pthread_queue_idle->mutex);
		}

		temp_pthread = pthread_queue_idle->head;
		if(pthread_queue_idle->pthread_queue_size == 1)
		{
			pthread_queue_idle->head = pthread_queue_idle->rear = NULL;
		}else
		{
			pthread_mutex_lock(&temp_pthread->mutex);
			pthread_queue_idle->head = temp_pthread->next;
			pthread_mutex_unlock(&temp_pthread->mutex);
		}

		pthread_queue_idle->pthread_queue_size--;
		pthread_mutex_unlock(&pthread_queue_idle->mutex);

		temp_task->pid = temp_pthread->pid;
		temp_task->next = NULL;
		temp_task->is_work = 1;

		pthread_mutex_lock(&temp_pthread->mutex);
		temp_pthread->is_execute_task = 1;
		temp_pthread->task = temp_task;
		temp_pthread->next = NULL;
		pthread_cond_signal(&temp_pthread->cond);
		pthread_mutex_unlock(&temp_pthread->mutex);
	}
	return (void *)0;
}

int init_pthread_pool()
{
	pthread_queue_idle = (pthread_queue *)malloc(sizeof(pthread_queue));
	pthread_queue_idle->pthread_queue_size = 0;
	pthread_queue_idle->head = pthread_queue_idle->rear = NULL;
	pthread_mutex_init(&pthread_queue_idle->mutex, NULL);
	pthread_cond_init(&pthread_queue_idle->cond, NULL);
	
	waiting_task_queue = (task_queue *)malloc(sizeof(task_queue));
	waiting_task_queue->head = waiting_task_queue->rear = NULL;
	waiting_task_queue->task_queue_size = 0;
	pthread_mutex_init(&waiting_task_queue->mutex, NULL);
	sem_init(&waiting_task_queue->NewTaskToExecute, 0, 0);
	
	create_pthread_pool();
	
	pthread_create(&pthread_manager_pid, NULL, pthread_manager, NULL);
	
	return 0;
}

int AddTaskToQueue(task_node * NewTask)
{
	pthread_mutex_init(&NewTask->mutex, NULL);
	NewTask->pid = 0;
	NewTask->is_work = 0;
	NewTask->next = NULL;
	
	pthread_mutex_lock(&waiting_task_queue->mutex);
	NewTask->work_id = waiting_task_queue->task_queue_size + 1;
	if(NewTask->work_id > TASK_MAX_NUM)
	{
		pthread_mutex_unlock(&waiting_task_queue->mutex);
		printf("threadpool.c, int AddTaskToQueue(task_node * NewTask):task is too much\n");
		pthread_mutex_destroy(&NewTask->mutex);
		free(NewTask->arg);
		free(NewTask);
		return -1;
	}
	if(waiting_task_queue->task_queue_size == 0)
	{
		waiting_task_queue->head = waiting_task_queue->rear = NewTask;
		NewTask->next  = NULL;
		waiting_task_queue->task_queue_size++;
	}else
	{
		NewTask->next  = NULL;
		pthread_mutex_lock(&waiting_task_queue->rear->mutex);
		waiting_task_queue->rear->next = NewTask;
		pthread_mutex_unlock(&waiting_task_queue->rear->mutex);

		waiting_task_queue->rear = NewTask;
		waiting_task_queue->task_queue_size++;
	}
	sem_post(&waiting_task_queue->NewTaskToExecute);
	pthread_mutex_unlock(&waiting_task_queue->mutex);

	return 0;
}

void monitor_pthread_pool()
{
	int i = 0;
	pthread_node *temp_pthread = NULL;
	
	pthread_mutex_lock(&pthread_queue_idle->mutex);
	//printf("threadpool.c, we have %d thread work!\n", pthread_pool_size -  pthread_queue_idle->pthread_queue_size);
	if(pthread_queue_idle->pthread_queue_size >  THREAD_IDLE_REDUNDANCE_MAX)
	{
		while(pthread_queue_idle->pthread_queue_size > THREAD_IDLE_REDUNDANCE_MAX/2)
		{
			temp_pthread = pthread_queue_idle->head;
			pthread_mutex_lock(&temp_pthread->mutex);
			pthread_queue_idle->head = temp_pthread->next;
			temp_pthread->pthread_exit_flag = 1;
			pthread_cond_signal(&temp_pthread->cond);
			pthread_mutex_unlock(&temp_pthread->mutex);
			pthread_queue_idle->pthread_queue_size--;
			pthread_pool_size--;
		}
	}else if(pthread_queue_idle->pthread_queue_size < THREAD_IDLE_REDUNDANCE_MIN)
	{
		if(pthread_queue_idle->pthread_queue_size == 0)
		{
			temp_pthread =(pthread_node *)malloc(sizeof(pthread_node));
			
			temp_pthread->pid = 0;
			temp_pthread->is_execute_task = 0;
			temp_pthread->pthread_exit_flag = 0;
			temp_pthread->task = NULL;
			temp_pthread->next = NULL;
			
			pthread_queue_idle->head = pthread_queue_idle->rear = temp_pthread;
			

			pthread_cond_init(&temp_pthread->cond, NULL);
			pthread_mutex_init(&temp_pthread->mutex, NULL);
			pthread_create(&temp_pthread->pid, NULL, single_pthread_work, temp_pthread);
			pthread_queue_idle->pthread_queue_size++;
			pthread_pool_size++;
		}

		for(i = 0; i < 10; i++)
		{
			temp_pthread = (pthread_node *)malloc(sizeof(pthread_node));
			
			temp_pthread->pid = 0;
			temp_pthread->is_execute_task = 0;
			temp_pthread->pthread_exit_flag = 0;
			temp_pthread->task = NULL;
			temp_pthread->next = NULL;
			
			pthread_mutex_lock(&pthread_queue_idle->rear->mutex);
			pthread_queue_idle->rear = temp_pthread;
			pthread_mutex_unlock(&pthread_queue_idle->rear->mutex);
			
			pthread_cond_init(&temp_pthread->cond, NULL);
			pthread_mutex_init(&temp_pthread->mutex, NULL);
			pthread_create(&temp_pthread->pid, NULL, single_pthread_work, temp_pthread);
			pthread_queue_idle->pthread_queue_size++;
			pthread_pool_size++;
		}
	}else
	{
		//printf("threadpool.c, void monitor_pthread_pool():threadpool work well!\n");
	}
	pthread_mutex_unlock(&pthread_queue_idle->mutex);
}