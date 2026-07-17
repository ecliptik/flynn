/*
 * logging.c - Session logging to file
 *
 * Continuous session logging (like Unix "script") that records all
 * terminal output to a text file.  Guarded by FLYNN_LOGGING.
 */

#include <Quickdraw.h>
#include <Windows.h>
#include <Files.h>
#include <StandardFile.h>
#include <Multiverse.h>
#include <Gestalt.h>
#include <OSUtils.h>
#include <string.h>
#include <stdio.h>

#include "main.h"
#include "session.h"
#include "connection.h"
#include "macutil.h"
#include "sysutil.h"
#include "logging.h"

/* External references to main.c globals */
extern Session *active_session;

/* Lazy log-flush helper (defined below with the output filter). */
static void log_flush_for(short refNum);

/*
 * format_timestamp - Write a human-readable timestamp into buf.
 * Uses GetDateTime + SecondsToDate for Toolbox date/time.
 * Returns length of string written (not including NUL).
 */
static short
format_timestamp(char *buf, short buflen)
{
	unsigned long secs;
	DateTimeRec dt;

	GetDateTime(&secs);
	SecondsToDate(secs, &dt);

	return snprintf(buf, buflen, "%04d/%02d/%02d %02d:%02d:%02d",
	    dt.year, dt.month, dt.day,
	    dt.hour, dt.minute, dt.second);
}

/*
 * write_log_header - Write header line with timestamp and hostname.
 */
static void
write_log_header(short refNum, Session *s)
{
	char hdr[256];
	char ts[32];
	long count;
	short len;

	format_timestamp(ts, sizeof(ts));

	len = snprintf(hdr, sizeof(hdr),
	    "Flynn session log started %s\r"
	    "Host: %s:%d\r"
	    "WARNING: This log may contain sensitive information "
	    "such as passwords.\r\r",
	    ts,
	    s->conn.host[0] ? s->conn.host : "(none)",
	    s->conn.port);

	/* snprintf returns the would-be length, which can exceed the
	 * buffer for a long host — clamp so FSWrite never reads past
	 * the NUL-terminated content and leaks stack bytes. */
	if (len < 0)
		return;
	if (len > (short)sizeof(hdr) - 1)
		len = (short)sizeof(hdr) - 1;

	count = len;
	FSWrite(refNum, &count, hdr);
}

/*
 * write_log_footer - Write footer line with timestamp.
 */
static void
write_log_footer(short refNum)
{
	char ftr[128];
	char ts[32];
	long count;
	short len;

	format_timestamp(ts, sizeof(ts));

	len = snprintf(ftr, sizeof(ftr),
	    "\r\rFlynn session log ended %s\r", ts);

	/* Clamp would-be length to the buffer (see write_log_header). */
	if (len < 0)
		return;
	if (len > (short)sizeof(ftr) - 1)
		len = (short)sizeof(ftr) - 1;

	count = len;
	FSWrite(refNum, &count, ftr);
}

void
do_start_logging(void)
{
	Session *s = active_session;
	Str255 default_name;
	short refNum;
	OSErr err;
	long sysver;
	Boolean use_std_file = false;

	if (!s)
		return;

	/* If already logging, stop first */
	if (s->log_refnum) {
		do_stop_logging();
		return;
	}

	/* Build default filename from hostname or generic */
	if (s->conn.host[0]) {
		char fname[64];
		short flen;

		flen = snprintf(fname, sizeof(fname),
		    "%s.log", s->conn.host);
		if (flen > 63)
			flen = 63;
		default_name[0] = flen;
		memcpy(&default_name[1], fname, flen);
	} else {
		default_name[0] = 9;
		memcpy(&default_name[1], "Flynn.log", 9);
	}

	/* Check for System 7+ StandardPutFile */
	if (TrapAvailable(_GestaltDispatch) &&
	    Gestalt(gestaltSystemVersion, &sysver) == noErr &&
	    sysver >= 0x0700)
		use_std_file = true;

	if (use_std_file) {
		/* System 7: StandardPutFile with FSSpec */
		StandardFileReply sf_reply;

		StandardPutFile(
		    "\pLog session to:",
		    default_name, &sf_reply);

		if (!sf_reply.sfGood)
			return;

		/* Delete existing file (ignore error) */
		FSpDelete(&sf_reply.sfFile);

		/* Create new file */
		err = FSpCreate(&sf_reply.sfFile, 'ttxt',
		    'TEXT', smSystemScript);
		if (err != noErr) {
			show_error_alert(
			    "Could not create log file.");
			return;
		}

		/* Open for writing */
		err = FSpOpenDF(&sf_reply.sfFile, fsWrPerm,
		    &refNum);
		if (err != noErr) {
			show_error_alert(
			    "Could not open log file.");
			return;
		}
	} else {
		/* System 6: SFPutFile with SFReply */
		SFReply reply;
		Point where;

		where.h = 80;
		where.v = 80;

		SFPutFile(where,
		    "\pLog session to:",
		    default_name, 0L, &reply);

		if (!reply.good)
			return;

		/* Delete existing file (ignore error) */
		FSDelete(reply.fName, reply.vRefNum);

		/* Create new file */
		err = Create(reply.fName, reply.vRefNum,
		    'ttxt', 'TEXT');
		if (err != noErr) {
			show_error_alert(
			    "Could not create log file.");
			return;
		}

		/* Open for writing */
		err = FSOpen(reply.fName, reply.vRefNum,
		    &refNum);
		if (err != noErr) {
			show_error_alert(
			    "Could not open log file.");
			return;
		}
	}

	/* Write header and start logging */
	write_log_header(refNum, s);
	s->log_refnum = refNum;
}

void
do_stop_logging(void)
{
	Session *s = active_session;
	short refNum;

	if (!s || !s->log_refnum)
		return;

	refNum = s->log_refnum;
	s->log_refnum = 0;

	log_flush_for(refNum);
	write_log_footer(refNum);
	FSClose(refNum);
	FlushVol(0L, 0);
}

/*
 * Escape sequence filter states for log output.
 * Strips ANSI/VT100 escape sequences so the log file
 * contains only readable text (no box characters in
 * TeachText/SimpleText).
 */
#define LOG_NORMAL	0
#define LOG_ESC		1	/* saw ESC */
#define LOG_CSI		2	/* ESC [ ... waiting for final byte */
#define LOG_OSC		3	/* ESC ] ... waiting for ST or BEL */
#define LOG_OSC_ESC	4	/* ESC ] ... saw ESC (possible ST) */

/*
 * Filtered output is accumulated across log_write_data() calls and
 * flushed lazily (half-full, ~1s elapsed, or on stop/disconnect) to
 * avoid a blocking FSWrite per TCP segment on slow HFS/floppy media.
 * The buffer is shared across sessions; flush_ref records which log
 * file the buffered bytes belong to so a session switch flushes the
 * previous file first.  A crash can lose the unflushed tail — logs
 * are best-effort.
 */
static char          flush_buf[512];
static short         flush_len = 0;
static short         flush_ref = 0;   /* refNum flush_buf belongs to */
static unsigned long flush_tick = 0;  /* TickCount at last flush */

/* Write out any buffered bytes to their owning log file. */
static void
log_flush_buf(void)
{
	long count;

	if (flush_len > 0 && flush_ref != 0) {
		count = flush_len;
		FSWrite(flush_ref, &count, flush_buf);
		flush_len = 0;
	}
	flush_tick = TickCount();
}

/* Flush and detach if the buffer belongs to refNum (before FSClose). */
static void
log_flush_for(short refNum)
{
	if (flush_ref == refNum) {
		log_flush_buf();
		flush_ref = 0;
	}
}

void
log_write_data(Session *s, unsigned char *data, short len)
{
	short i;
	unsigned char ch;

	if (!s || !s->log_refnum || len <= 0)
		return;

	/* Buffered data from another session must reach its own file
	 * before we reuse the shared buffer. */
	if (flush_len > 0 && flush_ref != s->log_refnum)
		log_flush_buf();
	flush_ref = s->log_refnum;

	for (i = 0; i < len; i++) {
		ch = data[i];

		switch (s->log_filter_state) {
		case LOG_ESC:
			if (ch == '[') {
				s->log_filter_state = LOG_CSI;
			} else if (ch == ']') {
				s->log_filter_state = LOG_OSC;
			} else {
				/* Short ESC sequence (e.g. ESC ( B) —
				 * consume one more byte and done */
				s->log_filter_state = LOG_NORMAL;
			}
			break;

		case LOG_CSI:
			/* CSI params: 0x20-0x3F; final byte: 0x40-0x7E */
			if (ch >= 0x40 && ch <= 0x7E)
				s->log_filter_state = LOG_NORMAL;
			break;

		case LOG_OSC:
			if (ch == 0x07) {
				/* BEL terminates OSC */
				s->log_filter_state = LOG_NORMAL;
			} else if (ch == 0x1B) {
				s->log_filter_state = LOG_OSC_ESC;
			}
			break;

		case LOG_OSC_ESC:
			/* ESC \ is ST (string terminator) */
			s->log_filter_state = LOG_NORMAL;
			break;

		default: /* LOG_NORMAL */
			if (ch == 0x1B) {
				s->log_filter_state = LOG_ESC;
			} else if ((ch >= 0x20 && ch <= 0x7E) ||
			    ch == '\r' || ch == '\t') {
				/* Only pass printable ASCII + CR + TAB.
				 * Drop \n (Mac uses \r), control chars,
				 * and high bytes (UTF-8 fragments show
				 * as boxes in TeachText).
				 * Suppress consecutive CRs to avoid
				 * excess blank lines. */
				if (ch == '\r' &&
				    s->log_last_char == '\r')
					break;
				s->log_last_char = ch;
				flush_buf[flush_len++] = ch;
				if (flush_len >=
				    (short)sizeof(flush_buf))
					log_flush_buf();
			}
			/* else control chars (0x00-0x1F except
			 * CR/LF/TAB) are silently dropped */
			break;
		}
	}

	/* Lazy flush: only when over half full or ~1s (60 ticks)
	 * has elapsed since the last flush.  The remainder is held
	 * until the next call or stop/disconnect. */
	if (flush_len > (short)(sizeof(flush_buf) / 2) ||
	    (TickCount() - flush_tick) > 60)
		log_flush_buf();
}

void
log_stop_if_active(Session *s)
{
	short refNum;

	if (!s || !s->log_refnum)
		return;

	refNum = s->log_refnum;
	s->log_refnum = 0;

	log_flush_for(refNum);
	write_log_footer(refNum);
	FSClose(refNum);
	FlushVol(0L, 0);
}
