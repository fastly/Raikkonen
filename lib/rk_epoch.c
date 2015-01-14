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
#include <stdlib.h>
#include <string.h>

#include "raikkonen.h"
#include "raikkonen_internal.h"

struct rk_epoch *
rk_epoch_create(struct rk_run_config *config)
{
	struct rk_epoch *e;

	assert(config != NULL);
	e = rk_array_append(&config->epochs);
	if (e == NULL) {
		return NULL;
	}

	rk_array_init(&e->commands, sizeof (struct rk_command));
	return e;
}

bool
rk_epoch_add_command(struct rk_epoch *e, struct rk_command *c)
{
	struct rk_command *ncmd;

	assert(e != NULL);
	assert(c != NULL);

	ncmd = rk_array_append(&e->commands);
	if (ncmd == NULL) {
		return false;
	}

	memcpy(ncmd, c, sizeof (*c));
	return true;
}

void
rk_epoch_set_notify(struct rk_epoch *e)
{

	e->notify = true;
}

