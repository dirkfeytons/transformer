/*
 * Copyright (c) 2016 Technicolor Delivery Technologies, SAS
 *
 * The source code form of this Transformer component is subject
 * to the terms of the Clear BSD license.
 *
 * You can redistribute it and/or modify it under the terms of the
 * Clear BSD License (http://directory.fsf.org/wiki/License:ClearBSD)
 *
 * See LICENSE file for more details.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <syslog.h>

#include "async.h"
#include "execute.h"

#define DEFAULT_EXEC_TIMEOUT 30

struct list_element {
  struct list_element *next;
  char cmd[];
};

struct queue{
  struct list_element *head;
  struct list_element *tail;
  struct queue_stats stats;

  pthread_mutex_t mutex;
  bool thread_running;

  int exec_timeout;
};

static void start_async_task(struct queue *queue);

static void queue_init(struct queue *queue)
{
  if( queue ) {
    pthread_mutex_init(&queue->mutex, NULL);
    queue->exec_timeout = DEFAULT_EXEC_TIMEOUT;
  }
}

static void check_lock(int r, const char *action)
{
  if( r != 0 ) {
    /* this is a disaster, (un)locking the mutex failed.
     * exiting the process is the safest thing to do.
     * Doing so will probably reboot the gateway.
     * But then again, (un)locking the mutex should never fail.
     * If it does, the program became unstable.
     */
    syslog(LOG_EMERG, "mutex %s failure: %d, exiting", action, r);
    exit(126);
  }
}

static void queue_lock(struct queue *queue)
{
  check_lock(pthread_mutex_lock(&queue->mutex), "lock");
}

static void queue_unlock(struct queue *queue)
{
  check_lock(pthread_mutex_unlock(&queue->mutex), "unlock");
}

static void queue_update_stats(struct queue *queue, int delta)
{
  queue->stats.inqueue += delta;
  if(delta>0) {
    queue->stats.enqueued += delta;
  }
  else {
    queue->stats.dequeued += -delta;
  }
}

static void queue_enqueue(struct queue *queue, struct list_element *e)
{
  e->next = NULL;

  queue_lock(queue);
  if (queue->head) {
    queue->tail->next = e;
  } else {
    queue->head = e;
  }
  queue->tail = e;
  queue_update_stats(queue, 1);
  start_async_task(queue);
  queue_unlock(queue);
}

static struct list_element* queue_dequeue(struct queue *queue, int *timeout)
{
  struct list_element *result;
  queue_lock(queue);
  result = queue->head;
  if (queue->head) {
    queue_update_stats(queue, -1);
    queue->head = result->next;
    if (!queue->head) {
      queue->tail = NULL;
    }
    result->next = NULL;
  }
  else {
    queue->thread_running = false;
  }
  if( timeout ) {
    *timeout = queue->exec_timeout;
  }
  queue_unlock(queue);
  return result;
}


static void run_command(const char *cmd, int timeout)
{
  syslog(LOG_INFO, "async run: %s", cmd);
  int r = execute(cmd, timeout);
  if( r!=0 ) {
    syslog(LOG_ERR, "async exec of '%s' failed exit code=%d", cmd, r);
  }
}

static void* execute_task (void* v)
{
  struct queue *queue = (struct queue*)v;

  for(;;) {
    int timeout;
    struct list_element *elem = queue_dequeue(queue, &timeout);
    if( elem ){
      run_command(elem->cmd, timeout);
      free(elem);
    }
    else {
      /* queue empty, nothing left to do. */
      break;
    }
  }

  return NULL;
}

static void start_async_task(struct queue *queue)
{
  if( !queue->thread_running ) {
    pthread_t pid;
    if( pthread_create(&pid, 0, execute_task, queue)==0) {
      queue->thread_running = true;
      pthread_detach(pid);
    }
    else {
      syslog(LOG_CRIT, "Failed to start async thread");
    }
  }
}

static struct list_element* create_list_element(const char *cmd)
{
  struct list_element *elem;
  /* allocate an extra byte for the NUL byte at the end of cmd */
  elem = (struct list_element*) calloc(1, sizeof(*elem) + strlen(cmd) + 1);
  if( elem ) {
    strcpy(elem->cmd, cmd);
  }
  return elem;
}

struct queue* async_create_queue(void)
{
  struct queue *queue = calloc(1, sizeof(*queue));

  queue_init(queue);

  return queue;
}

bool async_execute(struct queue *queue, const char *cmd)
{
  struct list_element *e = create_list_element(cmd);

  if( e ) {
    queue_enqueue(queue, e);
    return true;
  }
  return false;
}

bool async_get_stats(struct queue *queue, struct queue_stats *stats)
{
  if( queue && stats ) {
    queue_lock(queue);
    *stats = queue->stats;
    queue_unlock(queue);
    return true;
  }
  return false;
}

int async_exec_timeout(struct queue *queue, int timeout)
{
  int current;
  queue_lock(queue);
  current = queue->exec_timeout;
  if( timeout>=0 ) {
    queue->exec_timeout = timeout;
  }
  queue_unlock(queue);
  return current;
}
