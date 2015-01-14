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

#define _BSD_SOURCE
#include <assert.h>
#ifdef __linux__
#include <endian.h>
#else
#define be32toh ntohl
#define be16toh ntohs
#endif
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "raikkonen.h"
#include "raikkonen_internal.h"
#include "finnish.h"

enum finnish_states {
	FI_STATE_HEI,
	FI_STATE_OTA_SE,
	FI_STATE_LINGER,
};

enum finnish_parse_states {
	FI_STATE_PARSE_TIMESLICE,
	FI_STATE_PARSE_COMMAND,

	FI_STATE_PARSE_WHENBODY_RANGE,
	FI_STATE_PARSE_WHENBODY_COMMAND,
};

static uint32_t last_waitstate;

#ifdef FI_DEBUG
static void
hexdump(uint8_t *buf, uint32_t len)
{

	for (int i = 0; i < len; i++) {
		fprintf(rk_log, "%02x", buf[i]);
	}

	fprintf(rk_log, "\n");
}
#endif

static int
fi_read_uint8(uint8_t *buf, uint8_t *dest, uint32_t *off, uint32_t len)
{

	if (*off + 1 > len) {
		fprintf(rk_log, "fi_read_uint32: not enough data available\n");
		return -1;
	}

	*dest = *buf;
	(*off)++;

	return 0;
}

static int
fi_read_uint32(uint8_t *buf, uint32_t *dest, uint32_t *off, uint32_t len)
{

	if (*off + 4 > len) {
		fprintf(rk_log, "fi_read_uint32: not enough data available\n");
		return -1;
	}

	memcpy(dest, buf, sizeof (*dest));
	*dest = be32toh(*dest);
	*off += 4;

	return 0;
}

static int
fi_do_timespec(struct timespec *ts, uint8_t unit, uint32_t val)
{
	uint32_t q, m;

	q = m = 0;
	switch (unit) {
	case FI_BYTECODE_UNIT_SECOND:
		q = 1;
		m = 0;
		break;

	case FI_BYTECODE_UNIT_MILLISECOND:
		q = 1000;
		m = 1000000;
		break;

	case FI_BYTECODE_UNIT_MICROSECOND:
		q = 1000000;
		m = 1000;
		break;

	case FI_BYTECODE_UNIT_NANOSECOND:
		q = 1000000000;
		m = 1;
		break;

	default:
		fprintf(rk_log, "fi_do_timespec: Invalid time unit "
		    "specifier for timeout.\n");
		return -1;
	}

	if (val < q) {
		ts->tv_sec = 0;
	} else {
		ts->tv_sec = val / q;
		val -= (val / q) * q;
	}
	ts->tv_nsec = val * m;

	return 0;
}

static int
fi_parse_when_command(struct rk_run_config *c, struct rk_epoch *e,
    uint8_t *buf, uint32_t *off, uint32_t len)
{
	struct rk_state_handler *handler;
	enum finnish_parse_states pstate;
	struct rk_command *cmd;
	uint32_t state_id;

	cmd = rk_command_create(e);
	if (cmd == NULL) {
		fprintf(rk_log, "fi_parse_when_command: Out of memory "
		    "creating when command.\n");
		return -1;
	}
	cmd->command = RK_COMMAND_INSTALLHANDLER;
	*off += 4;
	
	if (fi_read_uint32(buf + *off, &state_id, off, len)) {
		fprintf(rk_log, "fi_parse_when_command: Couldn't read "
		    "state ID.\n");
		return -1;
	}

	/* Skip NUL behind state ID */
	*off+=1;

	cmd->cmd_installhandler.state_id = state_id;
	rk_array_init(&cmd->cmd_installhandler.handlers, sizeof (struct rk_state_handler));

	pstate = FI_STATE_PARSE_WHENBODY_RANGE;
	while (*off + 4 < len) {
		switch (pstate) {
		case FI_STATE_PARSE_WHENBODY_RANGE:
			if (!memcmp(buf + *off, FI_BYTECODE_WHEN_END, 4)) {
				*off += 4;
				return 0;
			} else {
				handler = rk_state_handler_create(&cmd->cmd_installhandler.handlers);
				handler->epoch = e->epoch;
				if (handler == NULL) {
					fprintf(rk_log, "fi_parse_when_command: Out of memory "
					    "creating state handler.\n");
					return -1;
				}

				if (fi_read_uint32(buf + *off, &handler->tr_start, off, len) ||
				    fi_read_uint32(buf + *off, &handler->tr_end, off, len)) {
					fprintf(rk_log, "fi_parse_when_command: "
					    "Couldn't read thread range.\n");
					return -1;
				}

				if (handler->tr_end == UINT_MAX) {
					cmd->cmd_installhandler.tr_max = handler->tr_start;
				}

				pstate = FI_STATE_PARSE_WHENBODY_COMMAND;
			}
			break;

		case FI_STATE_PARSE_WHENBODY_COMMAND:
			if (!memcmp(buf + *off, FI_BYTECODE_WHENCMD_CALLBACK, 2)) {
				handler->action = RK_HANDLER_CALLBACK;
				*off += 2;
				if (fi_read_uint32(buf + *off, &handler->act_callback,
				    off, len)) {
					fprintf(rk_log, "fi_parse_when_command: "
					    "Couldn't read callback ID.\n");
					return -1;
				}
			} else if (!memcmp(buf + *off, FI_BYTECODE_WHENCMD_CONTINUE, 2)) {
				handler->action = RK_HANDLER_CONTINUE;
				*off += 2;
			} else if (!memcmp(buf + *off, FI_BYTECODE_WHENCMD_PANIC, 2)) {
				handler->action = RK_HANDLER_PANIC;
				*off += 2;
			} else if (!memcmp(buf + *off, FI_BYTECODE_WHENCMD_SLEEP, 2)) {
				uint32_t u32;
				uint8_t u8;

				handler->action = RK_HANDLER_SLEEP;
				*off += 2;
				if (fi_read_uint8(buf + *off, &u8, off, len) ||
				    fi_read_uint32(buf + *off, &u32, off, len)) {
					fprintf(rk_log, "fi_parse_when_command: "
					    "Couldn't read sleep timespec.\n");
					return -1;
				}

				if (fi_do_timespec(&handler->act_sleep, u8, u32)) {
					return -1;
				}
			} else if (!memcmp(buf + *off, FI_BYTECODE_WHENCMD_WAIT, 2)) {
				handler->action = RK_HANDLER_WAIT;
				*off += 2;
				if (rk_sema_init(&handler->act_sema, 0) == false) {
					perror("fi_parse_when_command: rk_sema_init");
					return -1;
				}
			} else {
				fprintf(rk_log, "fi_parse_when_command: "
				    "Invalid / unrecognized command: ");
				fprintf(rk_log, "%02x%02x%02x%02x\n",
				    *(buf + *off), *(buf + *off + 1),
				    *(buf + *off + 2), *(buf + *off + 3));
				assert(NULL);
				return -1;
			}

			pstate = FI_STATE_PARSE_WHENBODY_RANGE;
			break;

		default:
			fprintf(rk_log, "fi_parse_when_command: Invalid parse "
			    "state encountered.\n");
			return -1;
		}
	}

	fprintf(rk_log, "fi_parse_when_command: ran out of buffer while "
	    "parsing when body.\n");
	return -1;
}

static int
fi_parse_timeout_command(struct rk_run_config *c, struct rk_epoch *e,
    uint8_t *buf, uint32_t *off, uint32_t len)
{
	struct rk_command *cmd;
	uint32_t u32;
	uint8_t u8;

	cmd = rk_command_create(e);
	if (cmd == NULL) {
		fprintf(rk_log, "fi_parse_timeout_command: Out of memory "
		    "creating timeout command.\n");
		return -1;
	}
	cmd->command = RK_COMMAND_TIMEOUT;
	*off += 4;

	if (fi_read_uint8(buf + *off, &u8, off, len) ||
	    fi_read_uint32(buf + *off, &u32, off, len)) {
		fprintf(rk_log, "fi_parse_timeout_command: Could not read "
		    "timeout specification bytecode.\n");
		return -1;
	}

	if (fi_do_timespec(&cmd->cmd_timeout.timeout, u8, u32)) {
		return -1;
	}

	return 0;
}

static int
fi_parse_resume_command(struct rk_run_config *c, struct rk_epoch *e,
    uint8_t *buf, uint32_t *off, uint32_t len)
{
	struct rk_state_handler *handler;
	struct rk_command *cmd;

	cmd = rk_command_create(e);
	if (cmd == NULL) {
		fprintf(rk_log, "fi_parse_resume_command: Out of memory "
		    "creating resume command.\n");
		return -1;
	}
	cmd->command = RK_COMMAND_RESUME;
	*off += 4;

	if (fi_read_uint32(buf + *off, &cmd->cmd_resume.state_id, off, len) ||
	    fi_read_uint32(buf + *off, &cmd->cmd_resume.tr_start, off, len) ||
	    fi_read_uint32(buf + *off, &cmd->cmd_resume.tr_end, off, len)) {
		fprintf(rk_log, "fi_parse_resume_command: Couldn't read "
		    "bytecode values.\n");
		return -1;
	}

	if (cmd->cmd_resume.state_id >= rk_array_len(&c->states)) {
		fprintf(rk_log, "fi_parse_resume_command: resume state id "
		    "exceeds number of configured states.\n");
		return -1;
	}

	/* Make sure that the range we are resuming is expecting to wake up. */
	handler = rk_config_find_handler(c, e, cmd->cmd_resume.state_id,
	    cmd->cmd_resume.tr_start, cmd->cmd_resume.tr_end);
	if (handler == NULL) {
		fprintf(rk_log, "fi_parse_resume_command: invalid state "
		    "to resume.\n");
		return -1;
	} else {
		if (handler->action != RK_HANDLER_WAIT) {
			fprintf(rk_log, "fi_parse_resume_command: state "
			    "thread range is not in wait.\n");
			return -1;
		}

		if (last_waitstate < handler->epoch) {
			fprintf(rk_log, "fi_parse_resume_command: no "
			    "waitstate between waited epoch and resume "
			    "handler. This is racy and I refuse to do it.\n");
			return -1;
		}
	}

	return 0;
}

static int
fi_parse_bytecode(struct rk_run_config *config, uint8_t *bytecode, uint32_t len)
{
	struct fi_bytecode_timeslice ts;
	enum finnish_parse_states state;
	struct rk_epoch *cur_epoch;
	uint32_t off, u;

	state = FI_STATE_PARSE_TIMESLICE;
	off = 0;
	while (off < len) {
		switch (state) {
		case FI_STATE_PARSE_TIMESLICE:
			if (off + sizeof(ts) > len) {
				fprintf(rk_log, "fi_parse_bytecode: Expected "
				    "timeslice, but remaining data cannot fill "
				    "that size.\n");
				return -1;
			}

			if (memcmp(bytecode + off, FI_BYTECODE_TIMESLICE,
			    sizeof (ts.prologue))) {
				fprintf(rk_log, "fi_parse_bytecode: Expected "
				    "timeslice prologue, but got junk.\n");
				return -1;
			}

			off += sizeof(ts.prologue);

			memcpy(&ts.slice_id, bytecode + off, sizeof (ts.slice_id));
			ts.slice_id = be32toh(ts.slice_id);

			u = rk_array_len(&config->epochs) - 1;
			if (u && ts.slice_id - u != 1) {
				fprintf(rk_log, "fi_parse_bytecode: New epoch "
				    "offset invalid (%" PRIu32 " - %" PRIu32 ".\n",
				    ts.slice_id, u);
				return -1;
			}

			off += sizeof (ts.slice_id);
			ts.notify = bytecode[off];
			if (ts.notify != 0 && ts.notify != 1) {
				fprintf(rk_log, "fi_parse_bytecode: Invalid "
				    "notify value.\n");
				return -1;
			}
			off++;

			cur_epoch = rk_epoch_create(config);
			if (cur_epoch == NULL) {
				fprintf(rk_log, "fi_parse_bytecode: Out of "
				    "memory for new timeslice.\n");
				return -1;
			}
			cur_epoch->epoch = ts.slice_id;
			if (ts.notify) {
				rk_epoch_set_notify(cur_epoch);
			} else {
				cur_epoch->notify = 0;
			}

			state = FI_STATE_PARSE_COMMAND;
			break;

		case FI_STATE_PARSE_COMMAND:
			if (off + 4 > len) {
				fprintf(rk_log, "fi_parse_bytecode: Not enough"
				    " data to satisfy command.\n");
				return -1;
			}

			if (!memcmp(bytecode + off, FI_BYTECODE_TIMESLICE_END, 4)) {
				off += 4;
				state = FI_STATE_PARSE_TIMESLICE;
				continue;
			} else if (!memcmp(bytecode + off, FI_BYTECODE_RESUME, 4)) {
				if (fi_parse_resume_command(config, cur_epoch,
				    bytecode, &off, len)) {
					return -1;
				}
				continue;
			} else if (!memcmp(bytecode + off, FI_BYTECODE_TIMEOUT, 4)) {
				if (fi_parse_timeout_command(config, cur_epoch,
				    bytecode, &off, len)) {
					return -1;
				}
				continue;
			} else if (!memcmp(bytecode + off, FI_BYTECODE_WAITSTATE, 4)) {
				struct rk_command *cmd;

				last_waitstate = cur_epoch->epoch;
				cmd = rk_command_create(cur_epoch);
				if (cmd == NULL) {
					fprintf(rk_log, "fi_parse_bytecode: "
					    "Out of memory for waitstate "
					    "command.\n");
					return -1;
				}
				off += 4;

				cmd->command = RK_COMMAND_WAITSTATE;
				continue;
			} else if (!memcmp(bytecode + off, FI_BYTECODE_WHEN, 4)) {
				if (fi_parse_when_command(config, cur_epoch,
				    bytecode, &off, len)) {
					return -1;
				}
			}

			break;
		default:
			fprintf(rk_log, "fi_parse_bytecode: Internal state "
			    "machine error.\n");
			return -1;
		}
	}

	return 0;
}

static ssize_t
fi_read(int fd, void *buf, size_t size)
{
	ssize_t	n_read, off;

	off = 0;
	do {
		errno = 0;
		n_read = read(fd, buf + off, size - off);
		if (n_read < 0 && errno != EINTR) {
			perror("fi_read: read");
			return -1;
		}

		if (n_read > 0) {
			off += n_read;
		} else if (n_read == 0) {
			break;
		}
	} while (off < size);

	return off;
}

static ssize_t
fi_write(int fd, void *buf, size_t size)
{
	ssize_t	n_written, off;

	off = 0;
	do {
		errno = 0;
		n_written = write(fd, buf + off, size - off);
		if (n_written < 0 && errno != EINTR) {
			perror("fi_write: write");
			return -1;
		}

		if (n_written > 0) {
			off += n_written;
		} else if (n_written == 0) {
			break;
		}
	} while (off < size);

	return off;
}

static int
fi_write_ei(struct rk_run_config *config)
{
	uint8_t ei[2] = { 0x65, 0x69 };
	ssize_t ret;

	ret = fi_write(config->client_fd, ei, sizeof(ei));
	if (ret > 0) {
		return -1;
	}

	return ret;
}

static int
fi_write_joo(struct rk_run_config *config)
{
	uint8_t joo[3] = { 0x6a, 0x6f, 0x6f };

	return (fi_write(config->client_fd, joo, sizeof(joo)) == sizeof (joo));
}

static int
fi_wait_hei(struct rk_run_config *config)
{
	struct fi_packet_hei hei;

	if (fi_read(config->client_fd, &hei, sizeof (hei)) != sizeof (hei)) {
		return fi_write_ei(config);
	}

	if (memcmp(hei.prologue, "hei", sizeof (hei.prologue))) {
		return fi_write_ei(config);
	}

	if (be16toh(hei.dialect) != FI_DIALECT) {
		return fi_write_ei(config);
	}

	return fi_write_joo(config);
}

static int
fi_read_ota_se(struct rk_run_config *config)
{
	struct fi_packet_ota_se ota_se;
	struct fi_packet_loppu loppu;
	uint8_t *bytecode;
	uint32_t len;

	if (fi_read(config->client_fd, &ota_se, sizeof (ota_se)) !=
	    sizeof (ota_se)) {
		return fi_write_ei(config);
	}

	if (memcmp(ota_se.prologue, "ota se", sizeof (ota_se.prologue))) {
		return fi_write_ei(config);
	}

	len = be32toh(ota_se.length);
	bytecode = calloc(1, len);
	if (bytecode == NULL) {
		fprintf(rk_log, "fi_read_ota_se: couldn't allocate %" PRIu32
		    " bytes\n", len);
		return fi_write_ei(config);
	}

	if (fi_read(config->client_fd, bytecode, len) != len) {
		free(bytecode);
		return fi_write_ei(config);
	}

	if (fi_parse_bytecode(config, bytecode, len) != 0) {
		fprintf(rk_log, "fi_read_ota_se: couldn't parse bytecode\n");
		return fi_write_ei(config);
	}

	if (fi_read(config->client_fd, &loppu, sizeof (loppu)) !=
	    sizeof (loppu)) {
		fprintf(rk_log, "fi_read_ota_se: loppu missing\n");
		return fi_write_ei(config);
	}

	if (memcmp(loppu.prologue, "loppu", sizeof (loppu.prologue))) {
		fprintf(rk_log, "fi_read_ota_se: loppu invalid\n");
		return fi_write_ei(config);
	}

	return fi_write_joo(config);
}

int
fi_negotiate_config(struct rk_run_config *config)
{
	
	config->fi_state = FI_STATE_HEI;
	switch (config->fi_state) {
	case FI_STATE_HEI:
		if (fi_wait_hei(config) < 0) {
			fprintf(rk_log, "Failure waiting for hei packet\n");
			return -1;
		}

		config->fi_state = FI_STATE_OTA_SE;
		/* FALLTHROUGH */
	case FI_STATE_OTA_SE:
		if (fi_read_ota_se(config) < 0) {
			fprintf(rk_log, "Failure reading ota se\n");
			return -1;
		}
		
		config->fi_state = FI_STATE_LINGER;
		/* FALLTHROUGH */
	case FI_STATE_LINGER:
		break;
	}

	return 0;
}
