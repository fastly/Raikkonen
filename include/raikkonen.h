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

#ifndef _RAIKKONEN_H_
#define _RAIKKONEN_H_

#include <netinet/in.h>

#include <stdint.h>

union rk_sockaddr {
	struct sockaddr		*rk_sa;
	struct sockaddr_in	*rk_sin4;
	struct sockaddr_in6	*rk_sin6;
};

struct rk_cbdef {
	char		*cb_name;
	void		(*cb)(uint32_t state_id, void *ptr);
	uint32_t	cb_id;
};

struct rk_state_iter {
	uint32_t i;
	uint32_t j;
};

struct rk_config {
};

struct rk_config	*rk_config_get_internal(void);

uint32_t		rk_state_register_internal(struct rk_config *, const char *);
uint32_t		rk_state_enter_internal(struct rk_config *, uint32_t);

void			rk_start_internal(union rk_sockaddr *);

#ifdef RK_ENABLED
#define rk_config_get()		rk_config_get_internal()
#define rk_state_register(a, b)	rk_state_register_internal((a), (b))
#define rk_state_enter(a, b)	rk_state_enter_internal((a), (b))
#define rk_start(a)		rk_start_internal((a))
#else
#define rk_config_get()		NULL
#define rk_state_register(a, b)	0
#define rk_state_enter(a, b)	0
#define rk_start(a)
#endif

#endif
