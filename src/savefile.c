/*
 * savefile.c - Save session content to text file
 */

#include <Quickdraw.h>
#include <Windows.h>
#include <Files.h>
#include <Memory.h>
#include <StandardFile.h>
#include <Multiverse.h>
#include <Gestalt.h>
#include <string.h>

#include "main.h"
#include "session.h"
#include "connection.h"
#include "terminal.h"
#include "glyphs.h"
#include "savefile.h"
#include "sysutil.h"

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
	if (CELL_IS_GLYPH(cell->attr) &&
	    cell->ch == GLYPH_WIDE_SPACER)
		return ' ';
	if (CELL_IS_GLYPH(cell->attr)) {
		const GlyphInfo *gi = glyph_get_info(cell->ch);
		return gi ? gi->copy_char : '?';
	}
	if (CELL_IS_BRAILLE(cell->attr))
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

/*
 * write_session_data - Write scrollback + screen buffer to open file.
 * Returns noErr on success, or first file error encountered.
 */
static OSErr
write_session_data(short refNum, Terminal *term)
{
	OSErr err = noErr;
	short row, si, sb_start, sb_idx;

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
				return err;
		}
	}

	/* Write screen buffer lines */
	for (row = 0; row < term->active_rows; row++) {
		err = write_row(refNum,
		    term->screen[row],
		    term->active_cols);
		if (err != noErr)
			return err;
	}

	return noErr;
}

void
do_save_session(void)
{
	Session *s = active_session;
	Terminal *term;
	Str255 default_name;
	short refNum;
	OSErr err;
	long sysver;
	Boolean use_std_file = false;

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

	/* Check for System 7+ StandardPutFile */
	if (TrapAvailable(_GestaltDispatch) &&
	    Gestalt(gestaltSystemVersion, &sysver) == noErr &&
	    sysver >= 0x0700)
		use_std_file = true;

	if (use_std_file) {
		/* System 7: StandardPutFile with FSSpec */
		StandardFileReply sf_reply;

		StandardPutFile("\pSave session text as:",
		    default_name, &sf_reply);

		if (!sf_reply.sfGood)
			return;

		/* Delete existing file (ignore error) */
		FSpDelete(&sf_reply.sfFile);

		/* Create new file */
		err = FSpCreate(&sf_reply.sfFile, 'ttxt',
		    'TEXT', smSystemScript);
		if (err != noErr) {
			ParamText("\pCould not create file.",
			    "\p", "\p", "\p");
			StopAlert(128, 0L);
			return;
		}

		/* Open for writing */
		err = FSpOpenDF(&sf_reply.sfFile, fsWrPerm,
		    &refNum);
		if (err != noErr) {
			ParamText(
			    "\pCould not open file for writing.",
			    "\p", "\p", "\p");
			StopAlert(128, 0L);
			return;
		}

		err = write_session_data(refNum, term);

		FSClose(refNum);
		FlushVol(0L, sf_reply.sfFile.vRefNum);

		if (err != noErr) {
			ParamText("\pError writing file.",
			    "\p", "\p", "\p");
			StopAlert(128, 0L);
		}
	} else {
		/* System 6: SFPutFile with SFReply */
		SFReply reply;
		Point where;

		where.h = 80;
		where.v = 80;

		SFPutFile(where, "\pSave session text as:",
		    default_name, 0L, &reply);

		if (!reply.good)
			return;

		/* Delete existing file (ignore error) */
		FSDelete(reply.fName, reply.vRefNum);

		/* Create new file */
		err = Create(reply.fName, reply.vRefNum,
		    'ttxt', 'TEXT');
		if (err != noErr) {
			ParamText("\pCould not create file.",
			    "\p", "\p", "\p");
			StopAlert(128, 0L);
			return;
		}

		/* Open for writing */
		err = FSOpen(reply.fName, reply.vRefNum,
		    &refNum);
		if (err != noErr) {
			ParamText(
			    "\pCould not open file for writing.",
			    "\p", "\p", "\p");
			StopAlert(128, 0L);
			return;
		}

		err = write_session_data(refNum, term);

		FSClose(refNum);
		FlushVol(0L, reply.vRefNum);

		if (err != noErr) {
			ParamText("\pError writing file.",
			    "\p", "\p", "\p");
			StopAlert(128, 0L);
		}
	}
}
