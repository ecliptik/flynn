/*
 * themes/classic.h - Classic 90s terminal theme
 * Optional theme - not included in default build.
 * White background with HTML-era standard ANSI colors.
 * Requires Color QuickDraw.
 *
 * To enable: add #include, theme_table[] entry, bump THEME_COUNT,
 * add THEME_CLASSIC index constant, and add menu item.
 */

static const TerminalTheme theme_classic = {
	"Classic",      /* name */
	1,              /* is_color */
	0,              /* is_dark */

	/* ANSI palette (HTML-era standard colors, cube-snapped) */
	{
		{ 0x00, 0x00, 0x00 },  /*  0: black */
		{ 0xCC, 0x00, 0x00 },  /*  1: red */
		{ 0x00, 0x99, 0x00 },  /*  2: green */
		{ 0xCC, 0x99, 0x00 },  /*  3: yellow */
		{ 0x00, 0x00, 0xCC },  /*  4: blue */
		{ 0xCC, 0x00, 0xCC },  /*  5: magenta */
		{ 0x00, 0xCC, 0xCC },  /*  6: cyan */
		{ 0xCC, 0xCC, 0xCC },  /*  7: white */
		{ 0x66, 0x66, 0x66 },  /*  8: bright black */
		{ 0xFF, 0x00, 0x00 },  /*  9: bright red */
		{ 0x00, 0xFF, 0x00 },  /* 10: bright green */
		{ 0xFF, 0xFF, 0x00 },  /* 11: bright yellow */
		{ 0x33, 0x33, 0xFF },  /* 12: bright blue */
		{ 0xFF, 0x00, 0xFF },  /* 13: bright magenta */
		{ 0x00, 0xFF, 0xFF },  /* 14: bright cyan */
		{ 0xFF, 0xFF, 0xFF },  /* 15: bright white */
	},

	{ 0x00, 0x00, 0x00 },  /* default_fg: black */
	{ 0xFF, 0xFF, 0xFF },  /* default_bg: white */
	{ 0x00, 0x00, 0x00 },  /* cursor_color: black */
	{ 0x33, 0x66, 0xCC },  /* sel_bg: blue */
	{ 0xFF, 0xFF, 0xFF },  /* sel_fg: white */
	{ 0x00, 0x00, 0x00 },  /* bold_color: unused */
};
