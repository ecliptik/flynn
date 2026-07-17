/*
 * printing.c - Page Setup and Print for terminal sessions
 *
 * Prints scrollback + screen buffer content using the
 * Macintosh Printing Manager.  Uses Monaco 9 for terminal
 * content and Geneva 9 for header/footer.  Paginates based
 * on the printable area reported by the printer driver.
 *
 * Follows Geomys print pattern: lazy THPrint allocation via
 * ensure_print_rec(), separate header/footer drawing, and
 * ImageWriter spool loop support.
 */

#include <Quickdraw.h>
#include <Fonts.h>
#include <Windows.h>
#include <Memory.h>
#include <Multiverse.h>
#include <string.h>
#include <stdio.h>

#include "main.h"
#include "session.h"
#include "connection.h"
#include "terminal.h"
#include "macutil.h"
#include "printing.h"

#ifdef FLYNN_PRINTING

/* External references to main.c globals */
extern Session *active_session;

/* Persistent print record -- lazily allocated, shared across sessions */
static THPrint g_hPrint = 0L;

/*
 * ensure_print_rec - Lazily allocate and initialize the THPrint handle.
 * Returns true if g_hPrint is valid, false on failure.
 * Must be called inside a PrOpen/PrClose bracket.
 */
static Boolean
ensure_print_rec(void)
{
	if (g_hPrint)
		return true;

	g_hPrint = (THPrint)NewHandle(sizeof(TPrint));
	if (!g_hPrint)
		return false;

	PrintDefault(g_hPrint);
	if (PrError() != noErr) {
		DisposeHandle((Handle)g_hPrint);
		g_hPrint = 0L;
		return false;
	}

	return true;
}

/*
 * draw_header_footer - Draw page header and footer in Geneva 9.
 *
 * Header (top of page): hostname or "Flynn Session"
 * Footer (bottom of page): centered "Page N of M"
 */
static void
draw_header_footer(Rect *rPage, const char *host, short cur_page,
    short total_pages, FontInfo *geneva_fi)
{
	char buf[128];
	short text_len, text_width;

	/* Set Geneva 9 for header/footer */
	TextFont(3);   /* Geneva */
	TextSize(9);
	TextFace(0);   /* plain */

	/* Header: hostname or session label, top-left */
	if (host[0])
		snprintf(buf, sizeof(buf), "%s", host);
	else
		snprintf(buf, sizeof(buf), "Flynn Session");

	text_len = strlen(buf);
	MoveTo(rPage->left, rPage->top + geneva_fi->ascent);
	DrawText(buf, 0, text_len);

	/* Footer: centered "Page N of M" at bottom */
	snprintf(buf, sizeof(buf), "Page %d of %d",
	    cur_page, total_pages);
	text_len = strlen(buf);
	text_width = TextWidth(buf, 0, text_len);
	MoveTo(rPage->left + (rPage->right - rPage->left - text_width) / 2,
	    rPage->bottom - geneva_fi->descent);
	DrawText(buf, 0, text_len);
}

/*
 * extract_row - Extract one row of TermCells into a char buffer.
 * Trims trailing spaces.  Returns the trimmed length.
 */
static short
extract_row(TermCell *cells, short cols, char *buf)
{
	short col, last_nonspace;

	last_nonspace = -1;
	for (col = 0; col < cols; col++) {
		buf[col] = cell_to_char(&cells[col]);
		if (buf[col] != ' ')
			last_nonspace = col;
	}
	return last_nonspace + 1;
}

/*
 * print_session_pages - Render all pages of scrollback + screen content.
 * Called after PrOpenDoc with valid prPort.
 */
static void
print_session_pages(TPPrPort prPort, Session *s, Rect *rPage)
{
	Terminal *term;
	FontInfo mono_fi, geneva_fi;
	short page_height;
	short mono_line_height, header_height, footer_height;
	short lines_per_page;
	short total_lines, total_pages;
	short cur_line, cur_page;
	short sb_start, sb_idx;
	short row, len;
	char line_buf[TERM_COLS + 2];
	short v;

	term = &s->terminal;

	/* Measure Geneva 9 for header/footer */
	TextFont(3);   /* Geneva */
	TextSize(9);
	GetFontInfo(&geneva_fi);
	header_height = geneva_fi.ascent + geneva_fi.descent +
	    geneva_fi.leading + 4;   /* header line + gap */
	footer_height = geneva_fi.ascent + geneva_fi.descent + 4;

	/* Measure Monaco 9 for terminal content */
	TextFont(4);   /* Monaco */
	TextSize(9);
	GetFontInfo(&mono_fi);
	mono_line_height = mono_fi.ascent + mono_fi.descent +
	    mono_fi.leading;
	if (mono_line_height < 1)
		mono_line_height = 12;

	/* Usable page height for terminal content */
	page_height = (rPage->bottom - rPage->top) -
	    header_height - footer_height;

	/* Lines of terminal content per page */
	lines_per_page = page_height / mono_line_height;
	if (lines_per_page < 1)
		lines_per_page = 1;

	/* Count total lines (scrollback + screen) */
	total_lines = term->sb_count + term->active_rows;

	total_pages = (total_lines + lines_per_page - 1) /
	    lines_per_page;
	if (total_pages < 1)
		total_pages = 1;

	cur_line = 0;
	cur_page = 0;

	while (cur_line < total_lines && PrError() == noErr) {
		short page_line;

		cur_page++;
		PrOpenPage(prPort, 0L);
		if (PrError() != noErr)
			break;

		/* Draw header and footer in Geneva 9 */
		draw_header_footer(rPage, s->conn.host,
		    cur_page, total_pages, &geneva_fi);

		/* Switch to Monaco 9 for terminal content */
		TextFont(4);   /* Monaco */
		TextSize(9);
		TextFace(0);   /* plain */
		TextMode(srcOr);

		/* First content line starts below header */
		v = rPage->top + header_height + mono_fi.ascent;

		/* Print lines for this page */
		for (page_line = 0;
		    page_line < lines_per_page &&
		    cur_line < total_lines;
		    page_line++, cur_line++) {

			/* Determine source: scrollback or screen */
			if (cur_line < term->sb_count) {
				/* Scrollback line */
				sb_start = term->sb_head -
				    term->sb_count;
				if (sb_start < 0)
					sb_start +=
					    TERM_SCROLLBACK_LINES;
				sb_idx = (sb_start + cur_line) %
				    TERM_SCROLLBACK_LINES;
				len = extract_row(
				    term->scrollback[sb_idx],
				    term->active_cols, line_buf);
			} else {
				/* Screen buffer line */
				row = cur_line - term->sb_count;
				len = extract_row(
				    term->screen_rows[row],
				    term->active_cols, line_buf);
			}

			if (len > 0) {
				MoveTo(rPage->left, v);
				DrawText(line_buf, 0, len);
			}

			v += mono_line_height;
		}

		PrClosePage(prPort);
	}
}

/*
 * do_page_setup - Show the Page Setup (style) dialog.
 */
void
do_page_setup(void)
{
	PrOpen();
	if (PrError() != noErr) {
		PrClose();
		return;
	}

	if (!ensure_print_rec()) {
		PrClose();
		return;
	}

	PrStlDialog(g_hPrint);

	PrClose();
}

/*
 * do_print - Print the active session's scrollback + screen buffer.
 *
 * Sequence: PrOpen -> ensure_print_rec -> PrJobDialog ->
 *           PrOpenDoc -> print_session_pages -> PrCloseDoc ->
 *           PrPicFile (spool) -> PrClose
 */
void
do_print(void)
{
	Session *s;
	TPPrPort prPort;
	GrafPtr savePort;
	Rect rPage;

	s = active_session;
	if (!s)
		return;

	/* Open Printing Manager */
	PrOpen();
	if (PrError() != noErr) {
		PrClose();
		return;
	}

	/* Ensure print record exists */
	if (!ensure_print_rec()) {
		PrClose();
		return;
	}

	/* Show job dialog -- user can cancel */
	if (!PrJobDialog(g_hPrint)) {
		PrClose();
		return;
	}

	/* Get printable area */
	rPage = (**g_hPrint).prInfo.rPage;

	/* Open the print document */
	GetPort(&savePort);
	prPort = PrOpenDoc(g_hPrint, 0L, 0L);
	if (PrError() != noErr) {
		/* PrOpenDoc failed — the document was never opened, so
		 * calling PrCloseDoc on prPort can fault in the driver.
		 * Skip straight to PrClose and restore the port. */
		PrClose();
		SetPort(savePort);
		return;
	}

	/* Render all pages */
	print_session_pages(prPort, s, &rPage);

	PrCloseDoc(prPort);

	/* ImageWriter spool printing: if driver recorded as spool,
	 * play back the spool file through PrPicFile */
	if ((**g_hPrint).prJob.bJDocLoop == bSpoolLoop &&
	    PrError() == noErr) {
		TPrStatus prStatus;
		PrPicFile(g_hPrint, 0L, 0L, 0L, &prStatus);
	}

	PrClose();
	SetPort(savePort);
}

#endif /* FLYNN_PRINTING */
