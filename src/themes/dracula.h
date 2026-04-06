/*
 * themes/dracula.h - Dracula theme
 * Dark purple-accented palette from draculatheme.com.
 * Canonical RGB values — may lose some distinction from Nord at 256 colors.
 * Requires Color QuickDraw.
 */

static const TerminalTheme theme_dracula = {
	"Dracula",      /* name */
	1,              /* is_color */
	1,              /* is_dark */

	/* ANSI palette (canonical Dracula) */
	{
		{ 0x21, 0x22, 0x2C },  /*  0: background */
		{ 0xFF, 0x55, 0x55 },  /*  1: red */
		{ 0x50, 0xFA, 0x7B },  /*  2: green */
		{ 0xF1, 0xFA, 0x8C },  /*  3: yellow */
		{ 0xBD, 0x93, 0xF9 },  /*  4: purple */
		{ 0xFF, 0x79, 0xC6 },  /*  5: pink */
		{ 0x8B, 0xE9, 0xFD },  /*  6: cyan */
		{ 0xF8, 0xF8, 0xF2 },  /*  7: foreground */
		{ 0x62, 0x72, 0xA4 },  /*  8: comment */
		{ 0xFF, 0x6E, 0x6E },  /*  9: bright red */
		{ 0x69, 0xFF, 0x94 },  /* 10: bright green */
		{ 0xFF, 0xFF, 0xA5 },  /* 11: bright yellow */
		{ 0xD6, 0xAC, 0xFF },  /* 12: bright purple */
		{ 0xFF, 0x92, 0xDF },  /* 13: bright pink */
		{ 0xA4, 0xFF, 0xFF },  /* 14: bright cyan */
		{ 0xFF, 0xFF, 0xFF },  /* 15: bright white */
	},

	{ 0xF8, 0xF8, 0xF2 },  /* default_fg: foreground */
	{ 0x28, 0x2A, 0x36 },  /* default_bg: background */
	{ 0xF8, 0xF8, 0xF2 },  /* cursor_color: foreground */
	{ 0x44, 0x47, 0x5A },  /* sel_bg: selection */
	{ 0xF8, 0xF8, 0xF2 },  /* sel_fg: foreground */
	{ 0x00, 0x00, 0x00 },  /* bold_color: unused */
};
