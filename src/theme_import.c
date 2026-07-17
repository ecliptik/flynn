/*
 * theme_import.c - Ghostty theme file import/export for Flynn
 *
 * Supports importing Ghostty-format theme files (plain text,
 * 22 lines of "key = #rrggbb") from disk, exporting the current
 * theme in Ghostty format, and removing custom themes.
 *
 * File I/O uses StandardGetFile/SFGetFile (System 7/6 dual path)
 * following the same pattern as savefile.c and logging.c.
 */

#ifdef FLYNN_THEMES

#include <Files.h>
#include <Menus.h>
#include <Memory.h>
#include <Quickdraw.h>
#include <StandardFile.h>
#include <Gestalt.h>
#include <Traps.h>
#include <Multiverse.h>
#include <stdio.h>
#include <string.h>
#include "theme_import.h"
#include "theme.h"
#include "color.h"
#include "settings.h"
#include "session.h"
#include "main.h"
#include "macutil.h"
#include "sysutil.h"

extern FlynnPrefs prefs;
extern Session *active_session;
extern MenuHandle theme_submenu;

/* Max file size we'll read (Ghostty themes are ~475 bytes) */
#define THEME_FILE_BUFSIZ  1024

/* ------------------------------------------------------------------ */
/* Hex parser                                                         */
/* ------------------------------------------------------------------ */

static short
hex_digit(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

/* Parse #rrggbb or rrggbb into ThemeRGB. Returns 0 on success. */
static short
parse_hex_color(const char *s, ThemeRGB *out)
{
	short d[6], i;

	if (*s == '#') s++;
	for (i = 0; i < 6; i++) {
		d[i] = hex_digit(s[i]);
		if (d[i] < 0) return -1;
	}
	out->r = (unsigned char)(d[0] * 16 + d[1]);
	out->g = (unsigned char)(d[2] * 16 + d[3]);
	out->b = (unsigned char)(d[4] * 16 + d[5]);
	return 0;
}

/* ------------------------------------------------------------------ */
/* Ghostty format parser                                              */
/* ------------------------------------------------------------------ */

/* Skip leading whitespace, return pointer to first non-space char */
static const char *
skip_ws(const char *p)
{
	while (*p == ' ' || *p == '\t') p++;
	return p;
}

/* Find end of line, return pointer past \r, \n, or \r\n */
static const char *
next_line(const char *p, const char *end)
{
	while (p < end && *p != '\r' && *p != '\n') p++;
	if (p < end && *p == '\r') p++;
	if (p < end && *p == '\n') p++;
	return p;
}

/* Match key at start of line. Returns pointer past key + whitespace,
 * or NULL if no match. */
static const char *
match_key(const char *line, const char *key, short keylen)
{
	if (strncmp(line, key, keylen) != 0)
		return 0L;
	return skip_ws(line + keylen);
}

/*
 * cube_snap - Snap an 8-bit RGB component to the nearest 6x6x6 cube value.
 *
 * The Mac 256-color system palette uses a 6x6x6 RGB cube with channel
 * values: 0, 51, 102, 153, 204, 255 (hex: 0x00, 0x33, 0x66, 0x99,
 * 0xCC, 0xFF). Colors outside this cube dither or quantize
 * unpredictably. Snapping imported theme colors to the cube ensures
 * they render cleanly on 256-color Macs.
 */
static unsigned char
cube_snap(unsigned char v)
{
	/* Cube levels: 0, 51, 102, 153, 204, 255 */
	/* Thresholds (midpoints): 26, 77, 128, 179, 230 */
	if (v < 26)  return 0;
	if (v < 77)  return 51;
	if (v < 128) return 102;
	if (v < 179) return 153;
	if (v < 230) return 204;
	return 255;
}

/* Snap a ThemeRGB to the nearest 6x6x6 cube color */
static void
cube_snap_rgb(ThemeRGB *c)
{
	c->r = cube_snap(c->r);
	c->g = cube_snap(c->g);
	c->b = cube_snap(c->b);
}

/*
 * parse_ghostty_theme - Parse Ghostty-format theme text into CustomTheme.
 *
 * Format: 22 lines of "key = value" where values are #rrggbb hex.
 * palette = N=#rrggbb (N = 0-15)
 * background = #rrggbb
 * foreground = #rrggbb
 * cursor-color = #rrggbb
 * cursor-text = #rrggbb (ignored)
 * selection-background = #rrggbb
 * selection-foreground = #rrggbb
 *
 * All colors are cube-snapped to the nearest 6x6x6 palette value
 * after parsing. This ensures clean rendering on 256-color Macs at
 * the cost of some color accuracy on truecolor displays.
 *
 * Returns 0 on success, -1 on parse error.
 * Requires: all 16 palette entries + background + foreground.
 * Optional: cursor-color, selection-*, cursor-text.
 */
short
parse_ghostty_theme(const char *buf, long len, CustomTheme *out)
{
	const char *p, *end, *line, *val;
	unsigned short pal_mask = 0; /* bits 0-15 for palette entries */
	short got_bg = 0, got_fg = 0;
	short idx;

	memset(out, 0, sizeof(CustomTheme));
	end = buf + len;

	p = buf;
	while (p < end) {
		line = skip_ws(p);
		p = next_line(p, end);

		/* Skip empty lines and comments */
		if (*line == '\0' || *line == '\r' || *line == '\n' ||
		    *line == '#')
			continue;

		/* palette = N=#rrggbb */
		val = match_key(line, "palette", 7);
		if (val) {
			/* Skip optional whitespace and '=' */
			val = skip_ws(val);
			if (*val == '=') val++;
			val = skip_ws(val);

			/* Parse index digit(s) */
			idx = 0;
			while (*val >= '0' && *val <= '9') {
				idx = idx * 10 + (*val - '0');
				val++;
			}
			if (idx < 0 || idx > 15)
				continue; /* ignore palette > 15 */

			/* Skip '=' between index and color */
			val = skip_ws(val);
			if (*val == '=') val++;
			val = skip_ws(val);

			if (parse_hex_color(val, &out->ansi[idx]) == 0)
				pal_mask |= (1 << idx);
			continue;
		}

		/* background = #rrggbb */
		val = match_key(line, "background", 10);
		if (val) {
			val = skip_ws(val);
			if (*val == '=') val++;
			val = skip_ws(val);
			if (parse_hex_color(val, &out->default_bg) == 0)
				got_bg = 1;
			continue;
		}

		/* foreground = #rrggbb */
		val = match_key(line, "foreground", 10);
		if (val) {
			val = skip_ws(val);
			if (*val == '=') val++;
			val = skip_ws(val);
			if (parse_hex_color(val, &out->default_fg) == 0)
				got_fg = 1;
			continue;
		}

		/* cursor-color = #rrggbb */
		val = match_key(line, "cursor-color", 12);
		if (val) {
			val = skip_ws(val);
			if (*val == '=') val++;
			val = skip_ws(val);
			parse_hex_color(val, &out->cursor_color);
			continue;
		}

		/* selection-background = #rrggbb */
		val = match_key(line, "selection-background", 20);
		if (val) {
			val = skip_ws(val);
			if (*val == '=') val++;
			val = skip_ws(val);
			parse_hex_color(val, &out->sel_bg);
			continue;
		}

		/* selection-foreground = #rrggbb */
		val = match_key(line, "selection-foreground", 20);
		if (val) {
			val = skip_ws(val);
			if (*val == '=') val++;
			val = skip_ws(val);
			parse_hex_color(val, &out->sel_fg);
			continue;
		}

		/* cursor-text, unknown keys: silently ignore */
	}

	/* Validate required fields */
	if (pal_mask != 0xFFFF || !got_bg || !got_fg)
		return -1;

	/* Default optional fields if not set */
	if (out->cursor_color.r == 0 && out->cursor_color.g == 0 &&
	    out->cursor_color.b == 0)
		out->cursor_color = out->default_fg;
	if (out->sel_bg.r == 0 && out->sel_bg.g == 0 &&
	    out->sel_bg.b == 0)
		out->sel_bg = out->default_fg;
	if (out->sel_fg.r == 0 && out->sel_fg.g == 0 &&
	    out->sel_fg.b == 0)
		out->sel_fg = out->default_bg;

	/* Auto-detect is_dark from background luminance
	 * Approx: (2R + 5G + B) / 8 */
	{
		short lum = (2 * (short)out->default_bg.r +
		    5 * (short)out->default_bg.g +
		    (short)out->default_bg.b) / 8;
		out->is_dark = (lum < 128) ? 1 : 0;
	}

	/* Cube-snap all colors for clean 256-color rendering */
	{
		short j;
		for (j = 0; j < 16; j++)
			cube_snap_rgb(&out->ansi[j]);
		cube_snap_rgb(&out->default_fg);
		cube_snap_rgb(&out->default_bg);
		cube_snap_rgb(&out->cursor_color);
		cube_snap_rgb(&out->sel_bg);
		cube_snap_rgb(&out->sel_fg);
	}

	/* bold_color left as {0,0,0} = unused */
	return 0;
}

/* ------------------------------------------------------------------ */
/* System version detection (shared pattern)                          */
/* ------------------------------------------------------------------ */

static short
use_standard_file(void)
{
	long sysver;

	if (TrapAvailable(_GestaltDispatch) &&
	    Gestalt(gestaltSystemVersion, &sysver) == noErr &&
	    sysver >= 0x0700)
		return 1;
	return 0;
}

/* ------------------------------------------------------------------ */
/* Name derivation from filename                                      */
/* ------------------------------------------------------------------ */

/* Convert Pascal string filename to C string theme name.
 * Strips common extensions (.txt, .theme, .ghostty).
 * Truncates to CUSTOM_THEME_NAME_LEN - 1. */
static void
name_from_filename(const unsigned char *pstr, char *name)
{
	short len, i;

	len = pstr[0];
	if (len > CUSTOM_THEME_NAME_LEN - 1)
		len = CUSTOM_THEME_NAME_LEN - 1;
	memcpy(name, pstr + 1, len);
	name[len] = '\0';

	/* Strip known extensions */
	for (i = len - 1; i >= 1; i--) {
		if (name[i] == '.') {
			if (strcmp(&name[i], ".txt") == 0 ||
			    strcmp(&name[i], ".theme") == 0 ||
			    strcmp(&name[i], ".ghostty") == 0)
				name[i] = '\0';
			break;
		}
	}
}

/* ------------------------------------------------------------------ */
/* Import Theme                                                       */
/* ------------------------------------------------------------------ */

void
do_import_theme(void)
{
	char file_buf[THEME_FILE_BUFSIZ];
	long count;
	short refNum, slot, i;
	OSErr err;
	CustomTheme parsed;
	unsigned char *fname;

	if (!g_has_color_qd)
		return;

	/* Check for empty slot */
	slot = -1;
	for (i = 0; i < MAX_CUSTOM_THEMES; i++) {
		if (!prefs.custom_themes[i].in_use) {
			slot = i;
			break;
		}
	}
	if (slot < 0) {
		show_error_alert(
		    "All 4 custom theme slots are full. "
		    "Remove a theme first.");
		return;
	}

	if (use_standard_file()) {
		/* System 7: StandardGetFile */
		StandardFileReply sf_reply;
		SFTypeList types;

		types[0] = 'TEXT';
		StandardGetFile(0L, 1, types, &sf_reply);

		if (!sf_reply.sfGood)
			return;

		err = FSpOpenDF(&sf_reply.sfFile, fsRdPerm, &refNum);
		if (err != noErr) {
			show_error_alert(
			    "Could not open theme file.");
			return;
		}

		count = THEME_FILE_BUFSIZ - 1;
		err = FSRead(refNum, &count, file_buf);
		FSClose(refNum);

		if (err != noErr && err != eofErr) {
			show_error_alert(
			    "Could not read theme file.");
			return;
		}
		file_buf[count] = '\0';
		fname = sf_reply.sfFile.name;
	} else {
		/* System 6: SFGetFile */
		SFReply reply;
		SFTypeList types;
		Point where;

		types[0] = 'TEXT';
		where.h = 80;
		where.v = 80;

		SFGetFile(where,
		    "\pSelect a theme file:",
		    0L, 1, types, 0L, &reply);

		if (!reply.good)
			return;

		err = FSOpen(reply.fName, reply.vRefNum, &refNum);
		if (err != noErr) {
			show_error_alert(
			    "Could not open theme file.");
			return;
		}

		count = THEME_FILE_BUFSIZ - 1;
		err = FSRead(refNum, &count, file_buf);
		FSClose(refNum);

		if (err != noErr && err != eofErr) {
			show_error_alert(
			    "Could not read theme file.");
			return;
		}
		file_buf[count] = '\0';
		fname = reply.fName;
	}

	/* Parse */
	if (parse_ghostty_theme(file_buf, count, &parsed) != 0) {
		show_error_alert(
		    "Could not parse theme file. "
		    "Expected Ghostty format.");
		return;
	}

	/* Store in prefs slot */
	memcpy(&prefs.custom_themes[slot], &parsed,
	    sizeof(CustomTheme));
	name_from_filename(fname, prefs.custom_themes[slot].name);
	prefs.custom_themes[slot].in_use = 1;
	prefs.custom_theme_count++;

	/* Reload and activate.  A single prefs_save() at the end of the
	 * auto-select block below persists the whole prefs struct
	 * (custom_themes slot + count + theme_id + dark_mode), so no
	 * intermediate save is needed here. */
	theme_load_custom();
	theme_rebuild_menu();

	/* Auto-select the imported theme */
	{
		short new_id = THEME_COUNT + slot;

		theme_set(new_id);
		if (active_session) {
			active_session->theme_id =
			    (unsigned char)new_id;
			active_session->terminal.dark_mode =
			    theme_is_dark();
			active_session->terminal.theme_id =
			    (unsigned char)new_id;
		}
		prefs.theme_id = (unsigned char)new_id;
		prefs.dark_mode = theme_is_dark() ? 1 : 0;
		prefs_save(&prefs);
	}
}

/* ------------------------------------------------------------------ */
/* Remove Theme                                                       */
/* ------------------------------------------------------------------ */

void
do_remove_theme(void)
{
	MenuHandle popup;
	Point pt;
	long result;
	short choice, slot, i, pos;

	if (!g_has_color_qd)
		return;
	if (prefs.custom_theme_count == 0)
		return;

	/* Build popup of custom theme names */
	popup = NewMenu(250, "\p");
	pos = 0;
	for (i = 0; i < MAX_CUSTOM_THEMES; i++) {
		if (prefs.custom_themes[i].in_use) {
			Str255 ps;
			short len;

			len = strlen(prefs.custom_themes[i].name);
			if (len > 254) len = 254;
			ps[0] = (unsigned char)len;
			memcpy(ps + 1,
			    prefs.custom_themes[i].name, len);
			AppendMenu(popup, "\p ");
			SetMenuItemText(popup, ++pos, ps);
		}
	}
	InsertMenu(popup, -1);

	pt.h = 200;
	pt.v = 100;
	result = PopUpMenuSelect(popup, pt.v, pt.h, 1);
	choice = LoWord(result);

	DeleteMenu(250);
	DisposeMenu(popup);

	if (choice <= 0)
		return;

	/* Map choice back to slot index */
	slot = -1;
	pos = 0;
	for (i = 0; i < MAX_CUSTOM_THEMES; i++) {
		if (prefs.custom_themes[i].in_use) {
			pos++;
			if (pos == choice) {
				slot = i;
				break;
			}
		}
	}
	if (slot < 0)
		return;

	/* If any session uses this theme, switch to Light */
	{
		short theme_idx = THEME_COUNT + slot;
		short si;

		for (si = 0; si < MAX_SESSIONS; si++) {
			Session *sess = session_get(si);
			if (sess &&
			    sess->theme_id == theme_idx) {
				sess->theme_id = THEME_LIGHT;
				sess->terminal.theme_id = THEME_LIGHT;
				sess->terminal.dark_mode = 0;
			}
		}
		if (prefs.theme_id == theme_idx)
			prefs.theme_id = THEME_LIGHT;
	}

	/* Reset bookmark references */
	for (i = 0; i < prefs.bookmark_count; i++) {
		if (prefs.bookmarks[i].bm_theme_id ==
		    THEME_COUNT + slot)
			prefs.bookmarks[i].bm_theme_id = -1;
	}

	/* Clear slot */
	memset(&prefs.custom_themes[slot], 0,
	    sizeof(CustomTheme));
	prefs.custom_theme_count--;

	/* Reload */
	theme_load_custom();
	theme_set(prefs.theme_id);
	prefs.dark_mode = theme_is_dark() ? 1 : 0;
	prefs_save(&prefs);
	theme_rebuild_menu();
}

/* ------------------------------------------------------------------ */
/* Export Theme                                                       */
/* ------------------------------------------------------------------ */

void
do_export_theme(void)
{
	const TerminalTheme *th;
	char buf[768];
	short len, i, refNum;
	long count;
	OSErr err;
	Str255 default_name;
	short nlen;

	if (!g_has_color_qd)
		return;

	th = theme_current();
	if (!th)
		return;

	/* Build default filename from theme name */
	nlen = strlen(th->name);
	if (nlen > 250) nlen = 250;
	default_name[0] = (unsigned char)nlen;
	memcpy(default_name + 1, th->name, nlen);

	/* Format in Ghostty format.  snprintf() returns the would-be
	 * length, so len can run past the buffer; guard every append
	 * (if len >= sizeof(buf), stop) so the size argument
	 * sizeof(buf)-len never underflows the unsigned size_t. */
	len = 0;
	for (i = 0; i < 16; i++) {
		if (len >= (short)sizeof(buf))
			break;
		len += snprintf(buf + len, sizeof(buf) - len,
		    "palette = %d=#%02x%02x%02x\n",
		    i,
		    th->ansi[i].r, th->ansi[i].g, th->ansi[i].b);
	}
	if (len < (short)sizeof(buf))
		len += snprintf(buf + len, sizeof(buf) - len,
		    "background = #%02x%02x%02x\n",
		    th->default_bg.r, th->default_bg.g, th->default_bg.b);
	if (len < (short)sizeof(buf))
		len += snprintf(buf + len, sizeof(buf) - len,
		    "foreground = #%02x%02x%02x\n",
		    th->default_fg.r, th->default_fg.g, th->default_fg.b);
	if (len < (short)sizeof(buf))
		len += snprintf(buf + len, sizeof(buf) - len,
		    "cursor-color = #%02x%02x%02x\n",
		    th->cursor_color.r, th->cursor_color.g,
		    th->cursor_color.b);
	/* cursor-text: use bg as default (text under cursor) */
	if (len < (short)sizeof(buf))
		len += snprintf(buf + len, sizeof(buf) - len,
		    "cursor-text = #%02x%02x%02x\n",
		    th->default_bg.r, th->default_bg.g, th->default_bg.b);
	if (len < (short)sizeof(buf))
		len += snprintf(buf + len, sizeof(buf) - len,
		    "selection-background = #%02x%02x%02x\n",
		    th->sel_bg.r, th->sel_bg.g, th->sel_bg.b);
	if (len < (short)sizeof(buf))
		len += snprintf(buf + len, sizeof(buf) - len,
		    "selection-foreground = #%02x%02x%02x\n",
		    th->sel_fg.r, th->sel_fg.g, th->sel_fg.b);
	/* Clamp for the FSWrite byte count below. */
	if (len > (short)sizeof(buf) - 1)
		len = (short)sizeof(buf) - 1;

	if (use_standard_file()) {
		StandardFileReply sf_reply;

		StandardPutFile("\pExport theme as:",
		    default_name, &sf_reply);

		if (!sf_reply.sfGood)
			return;

		FSpDelete(&sf_reply.sfFile);
		err = FSpCreate(&sf_reply.sfFile, 'FLYN',
		    'TEXT', smSystemScript);
		if (err != noErr) {
			show_error_alert(
			    "Could not create file.");
			return;
		}

		err = FSpOpenDF(&sf_reply.sfFile, fsWrPerm,
		    &refNum);
		if (err != noErr) {
			show_error_alert(
			    "Could not open file for writing.");
			return;
		}

		count = len;
		FSWrite(refNum, &count, buf);
		FSClose(refNum);
		FlushVol(0L, sf_reply.sfFile.vRefNum);
	} else {
		SFReply reply;
		Point where;

		where.h = 80;
		where.v = 80;
		SFPutFile(where, "\pExport theme as:",
		    default_name, 0L, &reply);

		if (!reply.good)
			return;

		FSDelete(reply.fName, reply.vRefNum);
		err = Create(reply.fName, reply.vRefNum,
		    'FLYN', 'TEXT');
		if (err != noErr) {
			show_error_alert(
			    "Could not create file.");
			return;
		}

		err = FSOpen(reply.fName, reply.vRefNum, &refNum);
		if (err != noErr) {
			show_error_alert(
			    "Could not open file for writing.");
			return;
		}

		count = len;
		FSWrite(refNum, &count, buf);
		FSClose(refNum);
		FlushVol(0L, reply.vRefNum);
	}
}

#endif /* FLYNN_THEMES */
