/*
 * Copyright (c) 2024-2026 Flynn contributors
 * Based on telnet.c from subtext by joshua stein <jcs@jcs.org>
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

/*
 * Client-side Telnet IAC negotiation engine.
 *
 * This module processes raw byte streams, stripping Telnet IAC sequences
 * and generating appropriate responses. It is transport-independent --
 * the caller reads from TCP, passes bytes through telnet_process(), and
 * sends any generated responses back over TCP.
 *
 * Adapted from the server-side implementation in subtext-596/telnet.c
 * with the negotiation direction inverted for client use:
 *   - Server WILL -> Client responds DO (accept) or DONT (reject)
 *   - Server DO   -> Client responds WILL (accept) or WONT (reject)
 *   - Server SB SEND -> Client responds SB IS <value>
 */

#include <string.h>
#include "telnet.h"

/* Telnet protocol bytes */
#define SE		240	/* end of sub-negotiation */
#define NOP		241	/* no operation */
#define DM		242	/* data mark */
#define BRK		243	/* break */
#define IP		244	/* interrupt process */
#define AO		245	/* abort output */
#define AYT		246	/* are you there */
#define EC		247	/* erase character */
#define EL		248	/* erase line */
#define GA		249	/* go ahead */
#define SB		250	/* start of sub-negotiation */
#define WILL		251
#define WONT		252
#define DO		253
#define DONT		254
#define IAC		255

/* Subnegotiation qualifiers */
#define SB_IS		0
#define SB_SEND		1

/* Telnet option codes */
#define OPT_BINARY	0
#define OPT_ECHO	1
#define OPT_SGA		3
#define OPT_TTYPE	24
#define OPT_NAWS	31
#define OPT_TSPEED	32
#define OPT_FLOWCTRL	33
#define OPT_LINEMODE	34
#define OPT_NEWENV	39

/* Terminal identity */
static const char tspeed_str[] = "19200,19200";

/*
 * Map a wire option code to our internal index, or -1 if unsupported.
 */
static short
opt_index(unsigned char opt)
{
	switch (opt) {
	case OPT_BINARY:	return TELOPT_BINARY;
	case OPT_ECHO:		return TELOPT_ECHO;
	case OPT_SGA:		return TELOPT_SGA;
	case OPT_TTYPE:		return TELOPT_TTYPE;
	case OPT_NAWS:		return TELOPT_NAWS;
	case OPT_TSPEED:	return TELOPT_TSPEED;
	default:		return -1;
	}
}

/*
 * Append bytes to the send buffer. Returns the new sendlen.
 */
static void
send_bytes(unsigned char *send, short *sendlen, const unsigned char *data,
    short len)
{
	short i;

	for (i = 0; i < len; i++)
		send[(*sendlen)++] = data[i];
}

/*
 * Send a 3-byte IAC command: IAC <cmd> <opt>
 */
static void
send_iac(unsigned char *send, short *sendlen, unsigned char cmd,
    unsigned char opt)
{
	unsigned char buf[3];

	buf[0] = IAC;
	buf[1] = cmd;
	buf[2] = opt;
	send_bytes(send, sendlen, buf, 3);
}

void
telnet_init(TelnetState *ts)
{
	memset(ts, 0, sizeof(TelnetState));
	ts->state = TELNET_STATE_NORMAL;
	ts->cols = 80;
	ts->rows = 24;
}

/*
 * Handle the server offering to do something (WILL <opt>).
 * As a client, we respond with DO if we want it, DONT otherwise.
 */
static void
handle_will(TelnetState *ts, unsigned char opt, unsigned char *send,
    short *sendlen)
{
	short idx;

	switch (opt) {
	case OPT_ECHO:
		/* Server will echo for us -- accept */
		idx = opt_index(opt);
		if (!(ts->opts[idx] & OPTFLAG_REMOTE)) {
			send_iac(send, sendlen, DO, opt);
			ts->opts[idx] |= OPTFLAG_REMOTE;
		}
		break;

	case OPT_SGA:
		/* Server will suppress go-ahead -- accept */
		idx = opt_index(opt);
		if (!(ts->opts[idx] & OPTFLAG_REMOTE)) {
			send_iac(send, sendlen, DO, opt);
			ts->opts[idx] |= OPTFLAG_REMOTE;
		}
		break;

	case OPT_BINARY:
		/* Server wants to send binary -- accept */
		idx = opt_index(opt);
		if (!(ts->opts[idx] & OPTFLAG_REMOTE)) {
			send_iac(send, sendlen, DO, opt);
			ts->opts[idx] |= OPTFLAG_REMOTE;
		}
		break;

	default:
		/* Reject anything we don't understand */
		send_iac(send, sendlen, DONT, opt);
		break;
	}
}

/*
 * Handle the server refusing to do something (WONT <opt>).
 * Acknowledge silently.
 */
static void
handle_wont(TelnetState *ts, unsigned char opt, unsigned char *send,
    short *sendlen)
{
	short idx;

	(void)send;
	(void)sendlen;

	idx = opt_index(opt);
	if (idx >= 0)
		ts->opts[idx] &= ~OPTFLAG_REMOTE;

	/* No response required for WONT */
}

/*
 * Handle the server asking us to do something (DO <opt>).
 * Respond with WILL if we support it, WONT otherwise.
 */
static void
handle_do(TelnetState *ts, unsigned char opt, unsigned char *send,
    short *sendlen)
{
	short idx;

	switch (opt) {
	case OPT_TTYPE:
		/* We will send terminal type */
		idx = opt_index(opt);
		if (!(ts->opts[idx] & OPTFLAG_LOCAL)) {
			send_iac(send, sendlen, WILL, opt);
			ts->opts[idx] |= OPTFLAG_LOCAL;
		}
		break;

	case OPT_NAWS:
		/* We will send window size */
		idx = opt_index(opt);
		if (!(ts->opts[idx] & OPTFLAG_LOCAL)) {
			send_iac(send, sendlen, WILL, opt);
			ts->opts[idx] |= OPTFLAG_LOCAL;
			/* Immediately send current window size */
			telnet_send_naws(ts, send, sendlen, ts->cols, ts->rows);
		}
		break;

	case OPT_TSPEED:
		/* We will send terminal speed */
		idx = opt_index(opt);
		if (!(ts->opts[idx] & OPTFLAG_LOCAL)) {
			send_iac(send, sendlen, WILL, opt);
			ts->opts[idx] |= OPTFLAG_LOCAL;
		}
		break;

	case OPT_SGA:
		/* We will suppress go-ahead */
		idx = opt_index(opt);
		if (!(ts->opts[idx] & OPTFLAG_LOCAL)) {
			send_iac(send, sendlen, WILL, opt);
			ts->opts[idx] |= OPTFLAG_LOCAL;
		}
		break;

	case OPT_BINARY:
		/* We can send binary */
		idx = opt_index(opt);
		if (!(ts->opts[idx] & OPTFLAG_LOCAL)) {
			send_iac(send, sendlen, WILL, opt);
			ts->opts[idx] |= OPTFLAG_LOCAL;
		}
		break;

	default:
		/* Reject anything we don't support */
		send_iac(send, sendlen, WONT, opt);
		break;
	}
}

/*
 * Handle the server telling us not to do something (DONT <opt>).
 * Acknowledge silently.
 */
static void
handle_dont(TelnetState *ts, unsigned char opt, unsigned char *send,
    short *sendlen)
{
	short idx;

	(void)send;
	(void)sendlen;

	idx = opt_index(opt);
	if (idx >= 0)
		ts->opts[idx] &= ~OPTFLAG_LOCAL;

	/* No response required for DONT */
}

/*
 * Handle a completed subnegotiation.
 * The sb_buf contains: <option> <qualifier> [data...] (without IAC SE).
 */
static void
handle_sb(TelnetState *ts, unsigned char *send, short *sendlen)
{
	unsigned char buf[64];
	short len;
	short i;

	if (ts->sb_len < 2)
		return;

	switch (ts->sb_buf[0]) {
	case OPT_TTYPE:
		/* Server sent SB TTYPE SEND -- respond with terminal type */
		if (ts->sb_buf[1] == SB_SEND) {
			const char *ttype;

			/* First request: VT220, subsequent: VT100 */
			ttype = (ts->ttype_count == 0) ? "VT220" : "VT100";
			ts->ttype_count++;

			buf[0] = IAC;
			buf[1] = SB;
			buf[2] = OPT_TTYPE;
			buf[3] = SB_IS;
			len = 4;
			for (i = 0; ttype[i] != '\0'; i++)
				buf[len++] = (unsigned char)ttype[i];
			buf[len++] = IAC;
			buf[len++] = SE;
			send_bytes(send, sendlen, buf, len);
		}
		break;

	case OPT_TSPEED:
		/* Server sent SB TSPEED SEND -- respond with our speed */
		if (ts->sb_buf[1] == SB_SEND) {
			buf[0] = IAC;
			buf[1] = SB;
			buf[2] = OPT_TSPEED;
			buf[3] = SB_IS;
			len = 4;
			for (i = 0; tspeed_str[i] != '\0'; i++)
				buf[len++] = (unsigned char)tspeed_str[i];
			buf[len++] = IAC;
			buf[len++] = SE;
			send_bytes(send, sendlen, buf, len);
		}
		break;

	case OPT_NAWS:
		/* Server shouldn't SB NAWS to us, but ignore gracefully */
		break;
	}
}

void
telnet_process(TelnetState *ts, unsigned char *in, short inlen,
    unsigned char *out, short *outlen, unsigned char *send, short *sendlen)
{
	short n;
	unsigned char c;

	for (n = 0; n < inlen; n++) {
		c = in[n];

		switch (ts->state) {
		case TELNET_STATE_NORMAL:
			if (c == IAC)
				ts->state = TELNET_STATE_IAC;
			else
				out[(*outlen)++] = c;
			break;

		case TELNET_STATE_IAC:
			switch (c) {
			case IAC:
				/* Escaped 0xFF -- pass through as data */
				out[(*outlen)++] = c;
				ts->state = TELNET_STATE_NORMAL;
				break;
			case NOP:
				ts->state = TELNET_STATE_NORMAL;
				break;
			case WILL:
				ts->state = TELNET_STATE_WILL;
				break;
			case WONT:
				ts->state = TELNET_STATE_WONT;
				break;
			case DO:
				ts->state = TELNET_STATE_DO;
				break;
			case DONT:
				ts->state = TELNET_STATE_DONT;
				break;
			case SB:
				ts->state = TELNET_STATE_SB;
				ts->sb_len = 0;
				break;
			case DM:
			case BRK:
			case IP:
			case AO:
			case AYT:
			case EC:
			case EL:
			case GA:
				/* Ignore these commands */
				ts->state = TELNET_STATE_NORMAL;
				break;
			default:
				/* Unknown command, return to normal */
				ts->state = TELNET_STATE_NORMAL;
				break;
			}
			break;

		case TELNET_STATE_WILL:
			handle_will(ts, c, send, sendlen);
			ts->state = TELNET_STATE_NORMAL;
			break;

		case TELNET_STATE_WONT:
			handle_wont(ts, c, send, sendlen);
			ts->state = TELNET_STATE_NORMAL;
			break;

		case TELNET_STATE_DO:
			handle_do(ts, c, send, sendlen);
			ts->state = TELNET_STATE_NORMAL;
			break;

		case TELNET_STATE_DONT:
			handle_dont(ts, c, send, sendlen);
			ts->state = TELNET_STATE_NORMAL;
			break;

		case TELNET_STATE_SB:
			/*
			 * First byte after SB is the option code.
			 * Transition to SB_DATA to collect the rest.
			 */
			ts->sb_buf[0] = c;
			ts->sb_len = 1;
			ts->state = TELNET_STATE_SB_DATA;
			break;

		case TELNET_STATE_SB_DATA:
			if (c == IAC) {
				ts->state = TELNET_STATE_SB_IAC;
			} else {
				if (ts->sb_len < TELNET_SB_BUFSIZ)
					ts->sb_buf[ts->sb_len++] = c;
				/* else: overflow, keep consuming until SE */
			}
			break;

		case TELNET_STATE_SB_IAC:
			if (c == SE) {
				/* End of subnegotiation */
				handle_sb(ts, send, sendlen);
				ts->state = TELNET_STATE_NORMAL;
			} else if (c == IAC) {
				/* Escaped 0xFF within subnegotiation */
				if (ts->sb_len < TELNET_SB_BUFSIZ)
					ts->sb_buf[ts->sb_len++] = IAC;
				ts->state = TELNET_STATE_SB_DATA;
			} else {
				/*
				 * Malformed sequence -- IAC followed by
				 * something other than SE or IAC inside SB.
				 * Treat as end of subnegotiation and
				 * re-process this byte as a new IAC command.
				 */
				handle_sb(ts, send, sendlen);
				ts->state = TELNET_STATE_IAC;
				/* Re-process c in IAC state */
				n--;
			}
			break;
		}
	}
}

void
telnet_send_naws(TelnetState *ts, unsigned char *send, short *sendlen,
    short cols, short rows)
{
	unsigned char buf[20];
	short len = 0;
	unsigned char hi, lo;

	ts->cols = cols;
	ts->rows = rows;

	buf[len++] = IAC;
	buf[len++] = SB;
	buf[len++] = OPT_NAWS;

	/* Width: 2 bytes, big-endian. Escape 0xFF as 0xFF 0xFF. */
	hi = (unsigned char)((cols >> 8) & 0xFF);
	lo = (unsigned char)(cols & 0xFF);
	buf[len++] = hi;
	if (hi == 0xFF)
		buf[len++] = 0xFF;
	buf[len++] = lo;
	if (lo == 0xFF)
		buf[len++] = 0xFF;

	/* Height: 2 bytes, big-endian. Escape 0xFF as 0xFF 0xFF. */
	hi = (unsigned char)((rows >> 8) & 0xFF);
	lo = (unsigned char)(rows & 0xFF);
	buf[len++] = hi;
	if (hi == 0xFF)
		buf[len++] = 0xFF;
	buf[len++] = lo;
	if (lo == 0xFF)
		buf[len++] = 0xFF;

	buf[len++] = IAC;
	buf[len++] = SE;

	send_bytes(send, sendlen, buf, len);
}
