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

#ifndef _RAIKKONEN_INTERNAL_H_
#define _RAIKKONEN_INTERNAL_H_

#ifdef __APPLE__
#include <dispatch/dispatch.h>
#else
#include <semaphore.h>
#endif
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

enum rk_commands {
	RK_COMMAND_INSTALLHANDLER,
	RK_COMMAND_RESUME,
	RK_COMMAND_TIMEOUT,
	RK_COMMAND_WAITSTATE,
};

enum rk_handler_actions {
	RK_HANDLER_CALLBACK,
	RK_HANDLER_CONTINUE,
	RK_HANDLER_PANIC,
	RK_HANDLER_SLEEP,
	RK_HANDLER_WAIT,
};

struct rk_sema {
#ifdef __APPLE__
	dispatch_semaphore_t	sem;
#else
	sem_t			sem;
#endif
};

struct rk_array {
	uint32_t		nelm;
	size_t			elmsize;
	void			*buf;
};

struct rk_state_handler {
	/*
	 * We store the epoch here so that we can do some sanity checking
	 * on wait / resume synchronization. It's immaterial for execution.
	 */
	uint32_t		epoch;
	uint32_t		tr_start;
	uint32_t		tr_end;

	enum rk_handler_actions	action;
	union {
		uint32_t	act_callback;
		struct rk_sema	act_sema;
		struct timespec	act_sleep;
	} u;
#define act_callback	u.act_callback
#define act_sema	u.act_sema
#define act_sleep	u.act_sleep
};

/*
 * Although we track states by ID internally, they have a readable name
 * representation. A state may have one or more handlers specified. These
 * handlers are assigned through `when` conditions in a Kimi script. When a
 * thread enters a state, it checks to see if a handler is installed for
 * that state. If n_handlers > 0, a handler is installed and the proper
 * handler may be found in next_handler.
 */
struct rk_state {
	const char		*state_name;
	uint32_t		state_id;

	uint32_t		cap_thread;
	uint32_t		cur_thread;
	struct rk_sema		waitstate;

	struct rk_array		*handlers;
};

struct rk_cmd_installhandler {
	uint32_t		state_id;
	uint32_t		tr_max;
	struct rk_array		handlers;
};

struct rk_cmd_resume {
	uint32_t		state_id;
	uint32_t		tr_start;
	uint32_t		tr_end;
};

struct rk_cmd_timeout {
	struct timespec		timeout;
};

struct rk_cmd_waitstate {
	/* empty, for now. */
};

struct rk_command {
	enum rk_commands	command;

	union {
		struct rk_cmd_installhandler	cmd_installhandler;
		struct rk_cmd_resume		cmd_resume;
		struct rk_cmd_timeout		cmd_timeout;
		struct rk_cmd_waitstate		cmd_waitstate;
	} u;
#define cmd_installhandler	u.cmd_installhandler
#define cmd_resume		u.cmd_resume
#define cmd_timeout		u.cmd_timeout
#define cmd_waitstate		u.cmd_waitstate
};

struct rk_epoch {
	uint32_t		epoch;
	bool			notify;

	struct rk_array		commands;
};

struct rk_run_config {
	struct rk_array		states;
	struct rk_array		callbacks;

	union rk_sockaddr	rk_sa;
#define rksin4	rk_sa.rk_sin4
#define rksin6	rk_sa.rk_sin6
#define rksa	rk_sa.rk_sa

	uint32_t		fi_state;
	int			client_fd;

	struct rk_array		epochs;
};

extern struct rk_run_config rk_config;
extern FILE *rk_log;

void			rk_array_init(struct rk_array *, size_t);
void			*rk_array_append(struct rk_array *);
void			*rk_array_first(struct rk_array *);
uint32_t		rk_array_len(struct rk_array *);

struct rk_command	*rk_command_create(struct rk_epoch *);

struct rk_state_handler	*rk_config_find_handler(struct rk_run_config *, struct rk_epoch *, uint32_t, uint32_t, uint32_t);
struct rk_state 	*rk_config_iterate_state(struct rk_run_config *, struct rk_epoch *, struct rk_state_iter *);

struct rk_epoch		*rk_epoch_create(struct rk_run_config *);
bool			rk_epoch_add_command(struct rk_epoch *, struct rk_command *);
void			rk_epoch_set_notify(struct rk_epoch *);

struct rk_state_handler	*rk_state_handler_create(struct rk_array *);

struct rk_state_handler	*rk_state_find_handler(struct rk_state *, uint32_t, uint32_t);

bool			rk_sema_init(struct rk_sema *, uint32_t);
bool			rk_sema_wait(struct rk_sema *);
bool			rk_sema_post(struct rk_sema *);

#endif
