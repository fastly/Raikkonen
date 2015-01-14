/*-
 * Copyright (c) 2014 Fastly, Inc.
 * All rights reserved.
 *
 * Author: Devon H. O'Dell <dho@fastly.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <ck_pr.h>

#include "raikkonen.h"
#include "raikkonen_internal.h"
#include "finnish.h"

static pthread_t rk_scheduler;
static struct rk_sema rk_initialized;

struct rk_run_config rk_config;

FILE *rk_log;

static int
rk_get_config(void)
{
	struct sockaddr_storage remote;
	socklen_t sl, rsl;
	int a, fd, so;

	fd = socket(rk_config.rksa->sa_family, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("rk_get_config: socket");
		return -1;
	}

	switch (rk_config.rksa->sa_family) {
	case AF_INET:
		sl = sizeof (*rk_config.rksin4);
		break;
	case AF_INET6:
		sl = sizeof (*rk_config.rksin6);
		break;
	default:
		fprintf(rk_log, "rk_get_config: impossible sa_family %d\n",
		    rk_config.rksa->sa_family);
		return -1;
	}

	so = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &so, sizeof(so)) == -1) {
		perror("rk_get_config: setsockopt");
		return -1;
	}

	if (bind(fd, rk_config.rksa, sl) < 0) {
		perror("rk_get_config: bind");
		return -1;
	}

	if (listen(fd, 128) == -1) {
		perror("rk_get_config: listen");
		return -1;
	}

	a = accept(fd, (struct sockaddr *)&remote, &rsl);
	if (a < 0) {
		perror("rk_get_config: accept");
		return -1;
	}

	/* 
	 * Don't really care about future stuff. If there's a protocol error,
	 * accepting some other connection is only going to be more confusing.
	 */
	close(fd);

	rk_config.client_fd = a;
	if (fi_negotiate_config(&rk_config) != 0) {
		fprintf(rk_log, "rk_get_config: protocol error\n");
		return -1;
	}

	return 0;
}

static void *
rk_thread_scheduler(void *arg)
{
	struct rk_epoch *cur_epoch;
	uint32_t epoch = 0;
	bool posted;

	/* If we fail, let the program go on; we've already whined */
	if (rk_get_config() != 0) {
		if (rk_sema_post(&rk_initialized) == false) {
			perror("rk_thread_scheduler: rk_sema_post(initialized)");
		}
		return (NULL);
	}

	posted = false;
	while (1) {
		if (epoch < rk_array_len(&rk_config.epochs)) {
			struct rk_command *commands;
			uint32_t n_commands, i;

			cur_epoch = rk_array_first(&rk_config.epochs);
			cur_epoch = &cur_epoch[epoch];

			commands = rk_array_first(&cur_epoch->commands);
			n_commands = rk_array_len(&cur_epoch->commands);
			for (i = 0; i < n_commands; i++) {
				struct rk_state_handler *handler;
				struct rk_state *wakestate;
				struct rk_state_iter it;
				struct timespec rem;
				uint32_t n_wake;
				int r;

				if (posted == false &&
				    (commands[i].command == RK_COMMAND_TIMEOUT ||
				     commands[i].command == RK_COMMAND_WAITSTATE)) {
					/*
					 * Wait until we are at a sleeping point
					 * before we post. This ensures that
					 * handlers are installed before the
					 * program proceeds.
					 */
					posted = true;
					if (rk_sema_post(&rk_initialized) == false) {
						perror("rk_thread_scheduler: rk_sema_post(initialized)");
					}
				}

				switch (commands[i].command) {
				case RK_COMMAND_INSTALLHANDLER:
					wakestate = rk_array_first(&rk_config.states);
					wakestate = &wakestate[commands[i].cmd_installhandler.state_id];
					wakestate->cur_thread = 1;
					wakestate->cap_thread = commands[i].cmd_installhandler.tr_max;
					wakestate->handlers = &commands[i].cmd_installhandler.handlers;
					break;

				case RK_COMMAND_RESUME:
					n_wake = (commands[i].cmd_resume.tr_end - commands[i].cmd_resume.tr_start) + 1;
					wakestate = rk_array_first(&rk_config.states);
					wakestate = &wakestate[commands[i].cmd_resume.state_id];
					handler = rk_state_find_handler(wakestate,
					    commands[i].cmd_resume.tr_start,
					    commands[i].cmd_resume.tr_end);

					while (n_wake--) {
						if (rk_sema_post(&handler->act_sema) == false) {
							perror("rk_thread_scheduler: rk_sema_post(act)");
						}
					}

					break;
					
				case RK_COMMAND_TIMEOUT:
					do {
						r = nanosleep(&commands[i].cmd_timeout.timeout, &rem);
						commands[i].cmd_timeout.timeout.tv_sec = rem.tv_sec;
						commands[i].cmd_timeout.timeout.tv_nsec = rem.tv_nsec;
					} while (r == -1 && errno == EINTR);
					break;
					
				case RK_COMMAND_WAITSTATE:
					memset(&it, 0, sizeof (it));
					while ((wakestate = rk_config_iterate_state(&rk_config, cur_epoch, &it)) != NULL) {
						if (ck_pr_load_32(&wakestate->cap_thread) == UINT_MAX) {
							continue;
						}

						if (rk_sema_wait(&wakestate->waitstate) == false) {
							perror("rk_thread_scheduler: rk_sema_wait(waitstate)");
						}
					}
					break;
				}
			}
		} else {
			/*
			 * Once the epoch transitions out of the configured
			 * range, there's not really anything else we can do.
			 * Exit the thread to avoid spinning.
			 */
			break;
		}

		epoch++;
	}

	return NULL;
}

void
rk_start_internal(union rk_sockaddr *rk_sa)
{

	rk_log = stderr;
	setlinebuf(rk_log);

	/* Cowardly refuse to do anything if our configuration is bogus. */
	if (rk_sa == NULL || rk_array_len(&rk_config.states) == 0 ||
	    (rk_sa->rk_sa->sa_family != AF_INET &&
	     rk_sa->rk_sa->sa_family != AF_INET6)) {
		fprintf(rk_log, "Bogus configuration; not running.\n");
		return;
	}

	rk_config.rk_sa.rk_sa = rk_sa->rk_sa;

	if (rk_sema_init(&rk_initialized, 0) == false) {
		perror("rk_start: rk_sema_init");
		return;
	}
	pthread_create(&rk_scheduler, NULL, rk_thread_scheduler, NULL);
	pthread_detach(rk_scheduler);
	rk_sema_wait(&rk_initialized);
}
