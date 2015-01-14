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

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>

#include <ck_pr.h>

#include "raikkonen.h"
#include "raikkonen_internal.h"

uint32_t
rk_state_register_internal(struct rk_config *cfg, const char *name)
{
	struct rk_run_config *c;
	struct rk_state *s;
	void *pun;

	assert(cfg != NULL);
	pun = cfg;
	c = pun;

	s = rk_array_append(&c->states);
	if (s == NULL) {
		return UINT_MAX;
	}

	s->state_name = name;
	s->state_id = rk_array_len(&c->states) - 1;
	if (rk_sema_init(&s->waitstate, 0) == false) {
		perror("rk_state_register: rk_sema_init(waitstate)");
	}

	return s->state_id;
}

struct rk_state_handler *
rk_state_find_handler(struct rk_state *s, uint32_t tr_start, uint32_t tr_end)
{
	struct rk_state_handler *handlers;
	uint32_t n_handlers, i;

	handlers = rk_array_first(s->handlers);
	n_handlers = rk_array_len(s->handlers);
	for (i = 0; i < n_handlers; i++) {
		if (handlers[i].tr_start == tr_start &&
		    handlers[i].tr_end == tr_end) {
			return &handlers[i];
		}
	}

	return NULL;
}

uint32_t
rk_state_enter_internal(struct rk_config *cfg, uint32_t state_id)
{
	struct rk_state_handler *h;
	struct rk_run_config *c;
	struct rk_cbdef *cbs;
	struct timespec rem;
	struct rk_state *s;
	uint32_t td, u, i;
	uint32_t cap;
	void *pun;
	int r;

	assert(cfg != NULL);
	pun = cfg;
	c = pun;

	s = rk_array_first(&c->states);
	if (state_id >= rk_array_len(&c->states)) {
		return UINT_MAX;
	}
	s = &s[state_id];

	if (s->handlers == NULL) {
		return UINT_MAX;
	}

	td = ck_pr_faa_32(&s->cur_thread, 1);

	h = rk_array_first(s->handlers);
	u = rk_array_len(s->handlers);

	cap = ck_pr_load_32(&s->cap_thread) - 1;
	if (td >= cap) {
		ck_pr_store_32(&s->cap_thread, UINT_MAX);
		if (rk_sema_post(&s->waitstate) == false) {
			perror("rk_state_enter: rk_sema_post(waitstate)");
			return UINT_MAX;
		}
	}

	for (i = 0; i < u; i++) {
		if (td >= h[i].tr_start && td <= h[i].tr_end) {
			break;
		}
	}

	h = &h[i];

	switch (h->action) {
	case RK_HANDLER_CALLBACK:
		cbs = rk_array_first(&c->callbacks);
		cbs[h->act_callback].cb(s->state_id, NULL);
		break;

	case RK_HANDLER_CONTINUE:
		break;
	
	case RK_HANDLER_PANIC:
		assert(NULL);
		break;
	
	case RK_HANDLER_SLEEP:
		rem.tv_sec = 0;
		rem.tv_nsec = 0;
		do {
			if (rem.tv_sec > 0 || rem.tv_nsec > 0) {
				r = nanosleep(&rem, &rem);
			} else {
				r = nanosleep(&h->act_sleep, &rem);
			}
		} while (r == -1 && errno == EINTR);
		break;
	
	case RK_HANDLER_WAIT:
		if (rk_sema_wait(&h->act_sema) == false) {
			perror("rk_state_enter: rk_sema_wait(act)");
			return UINT_MAX;
		}
		break;
	default:
		fprintf(rk_log, "Invalid handler: %u\n", h->action);
		assert(0);
	}

	return td;
}
