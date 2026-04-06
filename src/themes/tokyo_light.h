/*
 * themes/tokyo_light.h - Tokyo Night Light theme
 * Light gray background with blue accents.
 * Requires Color QuickDraw.
 */

static const TerminalTheme theme_tokyo_light = {
	"Tokyo Light",  /* name */
	1,              /* is_color */
	0,              /* is_dark */

	/* ANSI palette (tokyonight day, cube-snapped) */
	{
		{ 0x00, 0x00, 0x33 },  /*  0: bg_dark */
		{ 0xCC, 0x33, 0x33 },  /*  1: red */
		{ 0x33, 0x99, 0x33 },  /*  2: green */
		{ 0x99, 0x66, 0x00 },  /*  3: yellow */
		{ 0x33, 0x66, 0xCC },  /*  4: blue */
		{ 0x99, 0x33, 0xCC },  /*  5: magenta */
		{ 0x00, 0x99, 0x99 },  /*  6: cyan */
		{ 0xCC, 0xCC, 0xCC },  /*  7: fg */
		{ 0x66, 0x66, 0x99 },  /*  8: comment */
		{ 0xFF, 0x33, 0x66 },  /*  9: bright red */
		{ 0x33, 0xCC, 0x66 },  /* 10: bright green */
		{ 0xFF, 0x99, 0x33 },  /* 11: bright yellow */
		{ 0x33, 0x99, 0xFF },  /* 12: bright blue */
		{ 0xCC, 0x66, 0xFF },  /* 13: bright magenta */
		{ 0x33, 0xCC, 0xCC },  /* 14: bright cyan */
		{ 0xFF, 0xFF, 0xFF },  /* 15: bright white */
	},

	{ 0x33, 0x66, 0xCC },  /* default_fg: blue */
	{ 0xCC, 0xCC, 0xCC },  /* default_bg: light gray */
	{ 0x33, 0x66, 0xCC },  /* cursor_color: blue */
	{ 0x33, 0x66, 0xCC },  /* sel_bg: blue */
	{ 0xFF, 0xFF, 0xFF },  /* sel_fg: white */
	{ 0x00, 0x00, 0x00 },  /* bold_color: unused */
};
