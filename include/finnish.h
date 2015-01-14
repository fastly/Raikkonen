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

#ifndef _FINNISH_H_
#define _FINNISH_H_

#define FI_DIALECT	0x0000

struct fi_packet_hei {
	uint8_t		prologue[3];
	uint16_t	dialect;
} __attribute__((packed));

struct fi_packet_ota_se {
	uint8_t		prologue[6];
	uint32_t	length;
	uint32_t	crc32;
} __attribute__((packed));

struct fi_packet_loppu {
	uint8_t		prologue[5];
} __attribute__((packed));

struct fi_bytecode_timeslice {
	uint8_t		prologue[4];
	uint32_t	slice_id;
	uint8_t		notify;
};
#define FI_BYTECODE_TIMESLICE 		"\x76\x04\x6c\x00"
#define FI_BYTECODE_TIMESLICE_END	"\xde\xad\x76\x00"

#define FI_BYTECODE_WHEN	"\x6a\x6f\x73\x00"
#define FI_BYTECODE_WHEN_END	"\xde\xad\x6a\x00"

#define FI_BYTECODE_WHENCMD_CALLBACK	"\x00\x00"
#define FI_BYTECODE_WHENCMD_CONTINUE	"\x00\x01"
#define FI_BYTECODE_WHENCMD_PANIC	"\x00\x02"
#define FI_BYTECODE_WHENCMD_SLEEP	"\x00\x04"
#define FI_BYTECODE_WHENCMD_WAIT	"\x00\x08"

#define FI_BYTECODE_UNIT_SECOND		0
#define FI_BYTECODE_UNIT_MILLISECOND	1
#define FI_BYTECODE_UNIT_MICROSECOND	2
#define FI_BYTECODE_UNIT_NANOSECOND	3

#define FI_BYTECODE_RESUME	"\x6a\x04\x61\x00"
#define FI_BYTECODE_TIMEOUT	"\x75\x6e\x69\x00"
#define FI_BYTECODE_WAITSTATE	"\x6f\x05\x61\x00"

int fi_negotiate_config(struct rk_run_config *);

#endif
