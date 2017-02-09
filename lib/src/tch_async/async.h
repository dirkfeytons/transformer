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

#ifndef ASYNC_H
#define ASYNC_H

#include <stdbool.h>

struct queue_stats {
  unsigned enqueued;
  unsigned dequeued;
  unsigned inqueue;
};

struct queue;

/* create an async command execution queue */
struct queue* async_create_queue(void);

/* schedule a command asynchronously
 *
 * @param queue : the queue to use
 * @param cmd ; the command to execute
 *
 * @returns true if command is queued or false in case of error.
 */
bool async_execute(struct queue *queue, const char *cmd);

/* get some statistics about the queue
 *
 * @param queue : the queue
 * @param stats : where to store the statistics.
 *
 * @returns true and fills stas on success, false on failure
 * fails if either queue or stats is NULL.
 */
bool async_get_stats(struct queue *queue, struct queue_stats *stats);

/* gets/sets the exec timeout
 *
 * @param queue ; the queue
 * @param timeout : the timeout, if<0 no new timeout is set
 *                  a value of 0 means no timeout.
 * @returns the old timeout value
 */
int async_exec_timeout(struct queue *queue, int timeout);

#endif
