/*
 * themes/tokyo_dark.h - Tokyo Night Dark theme
 * Deep blue-black background with pastel accents.
 * Requires Color QuickDraw.
 */

static const TerminalTheme theme_tokyo_dark = {
	"Tokyo Dark",   /* name */
	1,              /* is_color */
	1,              /* is_dark */

	/* ANSI palette (tokyonight night, cube-snapped) */
	{
		{ 0x00, 0x00, 0x33 },  /*  0: bg */
		{ 0xFF, 0x33, 0x66 },  /*  1: red */
		{ 0x33, 0xCC, 0x66 },  /*  2: green */
		{ 0xFF, 0x99, 0x33 },  /*  3: yellow */
		{ 0x33, 0x99, 0xFF },  /*  4: blue */
		{ 0xCC, 0x66, 0xFF },  /*  5: magenta */
		{ 0x33, 0xCC, 0xCC },  /*  6: cyan */
		{ 0xCC, 0xCC, 0xFF },  /*  7: fg */
		{ 0x33, 0x33, 0x66 },  /*  8: comment */
		{ 0xFF, 0x66, 0x99 },  /*  9: bright red */
		{ 0x66, 0xFF, 0x99 },  /* 10: bright green */
		{ 0xFF, 0xCC, 0x66 },  /* 11: bright yellow */
		{ 0x66, 0xCC, 0xFF },  /* 12: bright blue */
		{ 0xFF, 0x99, 0xFF },  /* 13: bright magenta */
		{ 0x66, 0xFF, 0xFF },  /* 14: bright cyan */
		{ 0xFF, 0xFF, 0xFF },  /* 15: bright white */
	},

	{ 0xCC, 0xCC, 0xFF },  /* default_fg: lavender */
	{ 0x00, 0x00, 0x33 },  /* default_bg: deep blue-black */
	{ 0xCC, 0xCC, 0xFF },  /* cursor_color: lavender */
	{ 0x33, 0x66, 0x99 },  /* sel_bg: muted blue */
	{ 0xCC, 0xCC, 0xFF },  /* sel_fg: lavender */
	{ 0x00, 0x00, 0x00 },  /* bold_color: unused */
};
