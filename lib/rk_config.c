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

#include <stdio.h>
#include <inttypes.h>

#include "raikkonen.h"
#include "raikkonen_internal.h"

static int once;

struct rk_config *
rk_config_get_internal(void)
{
	void *pun = &rk_config;

	if (!once++) {
		rk_array_init(&rk_config.states, sizeof (struct rk_state));
		rk_array_init(&rk_config.callbacks, sizeof (void (*)()));
		rk_array_init(&rk_config.epochs, sizeof (struct rk_epoch));
	}

	return pun;
}

struct rk_state_handler *
rk_config_find_handler(struct rk_run_config *c, struct rk_epoch *max,
    uint32_t state_id, uint32_t tr_start, uint32_t tr_end)
{
	struct rk_epoch *epochs;
	uint32_t i;

	epochs = rk_array_first(&c->epochs);
	for (i = 0; i <= max->epoch; i++) {
		struct rk_command *cmds;
		uint32_t n_cmds, j;

		cmds = rk_array_first(&epochs[i].commands);
		n_cmds = rk_array_len(&epochs[i].commands);

		for (j = 0; j < n_cmds; j++) {
			struct rk_state_handler *handlers;
			uint32_t n_handlers, k;

			if (cmds[j].command != RK_COMMAND_INSTALLHANDLER ||
			    cmds[j].cmd_installhandler.state_id != state_id) {
				continue;
			}

			handlers = rk_array_first(&cmds->cmd_installhandler.handlers);
			n_handlers = rk_array_len(&cmds->cmd_installhandler.handlers);
			for (k = 0; k < n_handlers; k++) {
				if (handlers[k].tr_start != tr_start ||
				    handlers[k].tr_end != tr_end) {
					continue;
				}

				return &handlers[k];
			}
		}
	}

	return NULL;
}

struct rk_state *
rk_config_iterate_state(struct rk_run_config *c, struct rk_epoch *max,
    struct rk_state_iter *it)
{
	struct rk_epoch *epochs;
	uint32_t i;

	epochs = rk_array_first(&c->epochs);
	for (i = it->i; i <= max->epoch; i++) {
		struct rk_command *cmds;
		uint32_t n_cmds, j;

		cmds = rk_array_first(&epochs[i].commands);
		n_cmds = rk_array_len(&epochs[i].commands);

		for (j = it->j; j < n_cmds; j++) {
			struct rk_state *s;

			if (cmds[j].command != RK_COMMAND_INSTALLHANDLER) {
				continue;
			}

			s = rk_array_first(&c->states);
			s = &s[cmds[j].cmd_installhandler.state_id];

			it->i = i;
			it->j = j + 1;
			if (it->j >= n_cmds) {
				it->i++;
				it->j = 0;
			}
			return s;
		}
	}

	return NULL;
}
