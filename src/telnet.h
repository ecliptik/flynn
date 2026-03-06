/*
 * Copyright (c) 2024-2026 Flynn contributors
 * Based on telnet.c/telnet.h from subtext by joshua stein <jcs@jcs.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef TELNET_H
#define TELNET_H

/* IAC state machine states */
#define TELNET_STATE_NORMAL	0
#define TELNET_STATE_IAC	1
#define TELNET_STATE_WILL	2
#define TELNET_STATE_WONT	3
#define TELNET_STATE_DO		4
#define TELNET_STATE_DONT	5
#define TELNET_STATE_SB		6
#define TELNET_STATE_SB_DATA	7
#define TELNET_STATE_SB_IAC	8

/* Option indices for tracking negotiation state */
#define TELOPT_BINARY	0
#define TELOPT_ECHO	1
#define TELOPT_SGA	2
#define TELOPT_TTYPE	3
#define TELOPT_NAWS	4
#define TELOPT_TSPEED	5
#define TELOPT_COUNT	6

/* Per-option negotiation flags */
#define OPTFLAG_LOCAL	0x01	/* we have sent WILL for this option */
#define OPTFLAG_REMOTE	0x02	/* we have sent DO for this option */

/* Subnegotiation buffer size */
#define TELNET_SB_BUFSIZ	128

typedef struct {
	short		state;			/* IAC parser state */
	unsigned char	sb_buf[TELNET_SB_BUFSIZ]; /* subneg accumulator */
	short		sb_len;			/* bytes in sb_buf */
	unsigned char	sb_opt;			/* current SB option code */
	unsigned char	opts[TELOPT_COUNT];	/* per-option flags */
	short		cols;			/* terminal columns */
	short		rows;			/* terminal rows */
} TelnetState;

/* Initialize telnet state, zeroing everything */
void telnet_init(TelnetState *ts);

/*
 * Process incoming bytes from the network.
 *
 * in/inlen:     raw bytes from TCP
 * out/outlen:   clean data with IAC sequences stripped (for terminal)
 * send/sendlen: IAC response bytes to send back over TCP
 *
 * Caller must provide buffers for out and send at least as large as inlen.
 * On entry, *outlen and *sendlen should be 0 (or the current fill level
 * if appending). On return they reflect the number of bytes written.
 */
void telnet_process(TelnetState *ts, unsigned char *in, short inlen,
    unsigned char *out, short *outlen, unsigned char *send, short *sendlen);

/*
 * Generate a NAWS (window size) subnegotiation to send to the server.
 *
 * send/sendlen: buffer to write IAC SB NAWS sequence into.
 * On entry, *sendlen should be the current fill level (usually 0).
 */
void telnet_send_naws(TelnetState *ts, unsigned char *send, short *sendlen,
    short cols, short rows);

#endif /* TELNET_H */
