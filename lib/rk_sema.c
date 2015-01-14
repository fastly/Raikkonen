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

#ifdef __APPLE__
#include <dispatch/dispatch.h>
#else
#include <semaphore.h>
#include <errno.h>
#endif

#include "raikkonen.h"
#include "raikkonen_internal.h"

bool
rk_sema_init(struct rk_sema *s, uint32_t value)
{
#ifdef __APPLE__
	dispatch_semaphore_t *sem = &s->sem;

	*sem = dispatch_semaphore_create(value);
	return (*sem != NULL);
#else
	int r;

	r = sem_init(&s->sem, 0, value);

	return (r == 0);
#endif
}

bool
rk_sema_wait(struct rk_sema *s)
{
#ifdef __APPLE__
	dispatch_semaphore_wait(s->sem, DISPATCH_TIME_FOREVER);
	return true;
#else
	int r;

	do {
		r = sem_wait(&s->sem);
	} while (r == -1 && errno == EINTR);

	return (r == 0);
#endif
}

bool
rk_sema_post(struct rk_sema *s)
{
#ifdef __APPLE__
	dispatch_semaphore_signal(s->sem);
	return true;
#else
	int r;

	r = sem_post(&s->sem);

	return (r == 0);
#endif
}
