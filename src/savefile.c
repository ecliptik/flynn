/*
 * savefile.c - Save session content to text file
 */

#include <Quickdraw.h>
#include <Windows.h>
#include <Files.h>
#include <Memory.h>
#include <StandardFile.h>
#include <Multiverse.h>
#include <string.h>

#include "main.h"
#include "session.h"
#include "connection.h"
#include "terminal.h"
#include "glyphs.h"
#include "savefile.h"

/* External references to main.c globals */
extern Session *active_session;

/*
 * cell_to_char - Convert a TermCell to a plain text character
 * Same conversion logic as clipboard.c do_copy()
 */
static char
cell_to_char(TermCell *cell)
{
	if (cell->ch == 0)
		return ' ';
	if ((cell->attr & ATTR_GLYPH) &&
	    cell->ch == GLYPH_WIDE_SPACER)
		return ' ';
	if (cell->attr & ATTR_GLYPH) {
		const GlyphInfo *gi = glyph_get_info(cell->ch);
		return gi ? gi->copy_char : '?';
	}
	if (cell->attr & ATTR_BRAILLE)
		return '.';
	return cell->ch;
}

/*
 * write_row - Extract one row of TermCells as text and write to file.
 * Trims trailing spaces. Appends CR. Returns noErr or file error.
 */
static OSErr
write_row(short refNum, TermCell *cells, short cols)
{
	char line_buf[TERM_COLS + 2];
	short col, last_nonspace, len;
	long count;

	last_nonspace = -1;
	for (col = 0; col < cols; col++) {
		line_buf[col] = cell_to_char(&cells[col]);
		if (line_buf[col] != ' ')
			last_nonspace = col;
	}
	len = last_nonspace + 1;
	line_buf[len++] = '\r';
	count = len;
	return FSWrite(refNum, &count, line_buf);
}

void
do_save_session(void)
{
	Session *s = active_session;
	Terminal *term;
	SFReply reply;
	Point where;
	Str255 default_name;
	short refNum;
	OSErr err;
	short row, si, sb_start, sb_idx;
	FInfo finfo;

	if (!s)
		return;

	term = &s->terminal;

	/* Build default filename from hostname or generic */
	if (s->conn.host[0]) {
		short hlen;

		hlen = strlen(s->conn.host);
		if (hlen > 50)
			hlen = 50;
		default_name[0] = 0;
		memcpy(&default_name[1], s->conn.host, hlen);
		default_name[0] = hlen;
	} else {
		default_name[0] = 13;
		memcpy(&default_name[1], "Flynn Session", 13);
	}

	/* Center dialog roughly */
	where.h = 80;
	where.v = 80;

	SFPutFile(where, "\pSave session text as:", default_name,
	    0L, &reply);

	if (!reply.good)
		return;

	/* Delete existing file (ignore error if not found) */
	FSDelete(reply.fName, reply.vRefNum);

	/* Create new file */
	err = Create(reply.fName, reply.vRefNum, 'ttxt', 'TEXT');
	if (err != noErr) {
		ParamText("\pCould not create file.", "\p", "\p", "\p");
		StopAlert(128, 0L);
		return;
	}

	/* Open for writing */
	err = FSOpen(reply.fName, reply.vRefNum, &refNum);
	if (err != noErr) {
		ParamText("\pCould not open file for writing.",
		    "\p", "\p", "\p");
		StopAlert(128, 0L);
		return;
	}

	/* Write scrollback lines (oldest to newest) */
	if (term->sb_count > 0) {
		sb_start = term->sb_head - term->sb_count;
		if (sb_start < 0)
			sb_start += TERM_SCROLLBACK_LINES;

		for (si = 0; si < term->sb_count; si++) {
			sb_idx = (sb_start + si) %
			    TERM_SCROLLBACK_LINES;
			err = write_row(refNum,
			    term->scrollback[sb_idx],
			    term->active_cols);
			if (err != noErr)
				break;
		}
	}

	/* Write screen buffer lines */
	if (err == noErr) {
		for (row = 0; row < term->active_rows; row++) {
			err = write_row(refNum,
			    term->screen[row],
			    term->active_cols);
			if (err != noErr)
				break;
		}
	}

	FSClose(refNum);
	FlushVol(0L, reply.vRefNum);

	if (err != noErr) {
		ParamText("\pError writing file.", "\p", "\p", "\p");
		StopAlert(128, 0L);
	}
}
