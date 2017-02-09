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

#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* the pid of the process executing the command,
 * the one we need to kill in case it does not stop
 */
static pid_t child_pid;

enum {
  STARTED,
  TERM_SENT,
  KILL_SENT,
} child_state;

static void child_alarm(int sig)
{
  (void)sig;
  if( child_pid ) {
    /* The child is running longer than we allow, terminate it */
    if( child_state == STARTED ) {
      /* first be nice */
      kill(child_pid, SIGTERM);
      /* make arrangenments to be harsh */
      child_state = TERM_SENT;
      alarm(5);
    }
    else if( child_state == TERM_SENT ){
      /* we already tried to be nice, it didn't work */
      kill(child_pid, SIGKILL);
      /* if this doesn't work either, arrange for this process to
       * exit in order to let async continue.,
       */
      child_state = KILL_SENT;
      alarm(5);
    }
    else { /* KILL_SENT */
      _exit(250);
    }
  }
}

static void install_alarm_handler()
{
  struct sigaction sig = {
    .sa_handler = child_alarm,
  };
  sigaction(SIGALRM, &sig, NULL);
}

/* wait for the given pid to terminate */
static int wait_child_exit(pid_t pid)
{
  do {
    int status = 0;
    if( waitpid(pid, &status, 0) == pid ) {
      if( WIFEXITED(status) ) {
        return WEXITSTATUS(status);
      }
      else if( WIFSIGNALED(status) ) {
        return 128+WTERMSIG(status);
      }
      /* we did not request the status for STOPPED or CONTINUED
       * children so we get no info about those.
       * So the code can never get here.
       * We want this to be identifiable if it ever does happen.
       */
      return -2;
    }
  } while( errno == EINTR );
  return -1;
}

int execute(const char *cmd, int timeout)
{
  pid_t pid = fork();
  if( pid==0 ) {
    /* this is a new process, the timer process.
     * we can use alarm() without worrying about strange interactions.
     * We have full control over this process.
     */

    /* for again to exec the real command */
    child_pid = fork();
    if( child_pid == 0 ) {
      /* finally in the process where the command will be executed */
      if( execl("/bin/sh", "sh", "-c", cmd, NULL) == -1) {
        /* the exec failed */
        _exit(127);
      }
    }
    else if( child_pid == -1 ) {
      /* failed to fork */
      _exit(128);
    }
    /* The child doing the exec has been created.
     * wait here for the command to finish, break it off if it takes
     * too long.
     */
    if( timeout>0) {
      child_state = STARTED;
      install_alarm_handler();
      alarm(timeout);
    }
    int status = wait_child_exit(child_pid);
    if( status>=0 ) {
      _exit(status);
    }
    else {
      _exit(256+status);
    }
  }
  else if( pid==-1 ) {
    /* failed to fork to timer process*/
    return -1;
  }
  /* wait for the timer process to end */
  return wait_child_exit(pid);
}
