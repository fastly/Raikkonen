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

#include <arpa/inet.h>

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#include "../include/raikkonen.h"

static uint32_t rk_state_preread;
static int gi;

void *
td(void *arg)
{
	struct rk_config *cfg = arg;
	uint32_t tdno;
	int r;

	tdno = rk_state_enter(cfg, rk_state_preread);
	r = gi++;
	fprintf(stderr, "%d %d\n", tdno, r);

	return NULL;
}

int
main(void)
{
	union rk_sockaddr rksa;
	struct sockaddr_in sin;
	struct rk_config *cfg;
	pthread_t t[16];

	cfg = rk_config_get();
	rk_state_preread = rk_state_register(cfg, "STATE_PREREAD");

        sin.sin_family = AF_INET;
	sin.sin_port = htons(28806);
	inet_pton(AF_INET, "127.0.0.1", &sin.sin_addr);

	rksa.rk_sin4 = &sin;
	rk_start(&rksa);

	for (int i = 0; i < 16; i++) {
		pthread_create(&t[i], NULL, td, cfg);
	}

	for (int i = 0; i < 16; i++) {
		pthread_join(t[i], NULL);
	}

	return 0;
}
