/*
 * themes/dracula.h - Dracula theme
 * Optional theme - not included in default build.
 * Dark purple-accented palette.
 * Requires Color QuickDraw.
 *
 * To enable: add #include, theme_table[] entry, bump THEME_COUNT,
 * add THEME_DRACULA index constant, and add menu item.
 */

static const TerminalTheme theme_dracula = {
	"Dracula",      /* name */
	1,              /* is_color */
	1,              /* is_dark */

	/* ANSI palette (Dracula terminal, cube-snapped) */
	{
		{ 0x33, 0x33, 0x66 },  /*  0: background */
		{ 0xFF, 0x33, 0x66 },  /*  1: red */
		{ 0x33, 0xFF, 0x66 },  /*  2: green */
		{ 0xFF, 0xFF, 0x33 },  /*  3: yellow */
		{ 0x99, 0x99, 0xFF },  /*  4: blue (purple) */
		{ 0xFF, 0x66, 0xFF },  /*  5: magenta (pink) */
		{ 0x66, 0xFF, 0xFF },  /*  6: cyan */
		{ 0xFF, 0xFF, 0xFF },  /*  7: white */
		{ 0x66, 0x66, 0x99 },  /*  8: bright black (comment) */
		{ 0xFF, 0x66, 0x99 },  /*  9: bright red */
		{ 0x66, 0xFF, 0x99 },  /* 10: bright green */
		{ 0xFF, 0xFF, 0x99 },  /* 11: bright yellow */
		{ 0xCC, 0xCC, 0xFF },  /* 12: bright blue */
		{ 0xFF, 0x99, 0xFF },  /* 13: bright magenta */
		{ 0x99, 0xFF, 0xFF },  /* 14: bright cyan */
		{ 0xFF, 0xFF, 0xFF },  /* 15: bright white */
	},

	{ 0xFF, 0xFF, 0xFF },  /* default_fg: white */
	{ 0x33, 0x33, 0x66 },  /* default_bg: dark purple */
	{ 0xFF, 0xFF, 0xFF },  /* cursor_color: white */
	{ 0x66, 0x66, 0xCC },  /* sel_bg: purple */
	{ 0xFF, 0xFF, 0xFF },  /* sel_fg: white */
	{ 0x00, 0x00, 0x00 },  /* bold_color: unused */
};
