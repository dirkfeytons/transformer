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
#ifndef EXECUTE_H
#define EXECUTE_H

/* execute a cmd through the shell
 *
 * @param cmd : the command to execute
 * @param timeout : the number of seconds the command is allowed to run
 *
 * @return the exit code of the process or -1 if the process could not fork
 *
 * If timeout is >0 and the command does not finish within timeout seconds
 * a TERM signal is sent to the process executing the command. If the process
 * does not exit within 5 seconds a KILL signal is sent to the process.
 * If the process does net end within 5 seconds, we give up waiting for it
 * and return with exit code 250.
 *
 * If the process executing the command was terminated by a signal the exit
 * code reported will be 128+signalno. (143 for SIGTERM, or 137 for SIGKILL)
 *
 * In case the exec after fork fails the exit code will be 127
 */
int execute(const char *cmd, int timeout);

#endif
