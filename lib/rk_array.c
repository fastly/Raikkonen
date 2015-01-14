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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raikkonen.h"
#include "raikkonen_internal.h"

void
rk_array_init(struct rk_array *a, size_t elmsize)
{

	assert(a != NULL);
	a->nelm = 0;
	a->elmsize = elmsize;
}

void *
rk_array_append(struct rk_array *a)
{
	void *nbuf, *obuf;
	uint32_t nelm;
	int r;

	assert(a != NULL);
	assert(a->elmsize > 0);

	nelm = ++a->nelm;
	obuf = a->buf;

	r = posix_memalign(&nbuf, 64, a->elmsize * nelm);
	if (r != 0) {
		perror("rk_array_append: posix_memalign");
		return NULL;
	}

	if (nelm > 1) {
		memcpy(nbuf, obuf, a->elmsize * (nelm - 1));
	}

	a->buf = nbuf;
	nbuf = nbuf + (a->elmsize * (nelm - 1));
	memset(nbuf, 0, a->elmsize);

	if (obuf != NULL) {
		free(obuf);
	}

	return nbuf;
}

void *
rk_array_first(struct rk_array *a)
{

	assert(a != NULL);
	return a->buf;
}

uint32_t
rk_array_len(struct rk_array *a)
{

	assert(a != NULL);
	return a->nelm;
}
