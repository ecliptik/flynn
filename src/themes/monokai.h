/*
 * themes/monokai.h - Monokai theme
 * Optional theme - not included in default build.
 * Dark warm palette with vivid accents.
 * Requires Color QuickDraw.
 *
 * To enable: add #include, theme_table[] entry, bump THEME_COUNT,
 * add THEME_MONOKAI index constant, and add menu item.
 */

static const TerminalTheme theme_monokai = {
	"Monokai",      /* name */
	1,              /* is_color */
	1,              /* is_dark */

	/* ANSI palette (Monokai Pro, cube-snapped) */
	{
		{ 0x33, 0x33, 0x00 },  /*  0: background */
		{ 0xFF, 0x33, 0x33 },  /*  1: red */
		{ 0x99, 0xCC, 0x00 },  /*  2: green */
		{ 0xFF, 0xCC, 0x00 },  /*  3: yellow */
		{ 0x66, 0x99, 0xFF },  /*  4: blue */
		{ 0xCC, 0x66, 0xFF },  /*  5: magenta */
		{ 0x66, 0xCC, 0xCC },  /*  6: cyan */
		{ 0xFF, 0xFF, 0xCC },  /*  7: white */
		{ 0x66, 0x66, 0x33 },  /*  8: bright black (comment) */
		{ 0xFF, 0x66, 0x66 },  /*  9: bright red */
		{ 0xCC, 0xFF, 0x33 },  /* 10: bright green */
		{ 0xFF, 0xFF, 0x66 },  /* 11: bright yellow */
		{ 0x99, 0xCC, 0xFF },  /* 12: bright blue */
		{ 0xFF, 0x99, 0xFF },  /* 13: bright magenta */
		{ 0x99, 0xFF, 0xFF },  /* 14: bright cyan */
		{ 0xFF, 0xFF, 0xFF },  /* 15: bright white */
	},

	{ 0xFF, 0xFF, 0xCC },  /* default_fg: warm white */
	{ 0x33, 0x33, 0x00 },  /* default_bg: dark olive */
	{ 0xFF, 0xFF, 0xCC },  /* cursor_color: warm white */
	{ 0x66, 0x66, 0x33 },  /* sel_bg: muted olive */
	{ 0xFF, 0xFF, 0xCC },  /* sel_fg: warm white */
	{ 0x00, 0x00, 0x00 },  /* bold_color: unused */
};
