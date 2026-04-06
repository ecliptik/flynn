/*
 * themes/nord.h - Nord theme
 * Optional theme - not included in default build.
 * Dark blue-tinted Arctic palette.
 * Requires Color QuickDraw.
 *
 * To enable: add #include, theme_table[] entry, bump THEME_COUNT,
 * add THEME_NORD index constant, and add menu item.
 */

static const TerminalTheme theme_nord = {
	"Nord",         /* name */
	1,              /* is_color */
	1,              /* is_dark */

	/* ANSI palette (Nord, cube-snapped) */
	{
		{ 0x33, 0x33, 0x66 },  /*  0: nord0 (polar night) */
		{ 0xCC, 0x66, 0x66 },  /*  1: red (aurora) */
		{ 0x99, 0xCC, 0x66 },  /*  2: green (aurora) */
		{ 0xCC, 0xCC, 0x66 },  /*  3: yellow (aurora) */
		{ 0x66, 0x99, 0xCC },  /*  4: blue (frost) */
		{ 0xCC, 0x66, 0xCC },  /*  5: magenta (aurora) */
		{ 0x66, 0xCC, 0xCC },  /*  6: cyan (frost) */
		{ 0xCC, 0xCC, 0xFF },  /*  7: nord4 (snow storm) */
		{ 0x66, 0x66, 0x99 },  /*  8: nord3 (polar night) */
		{ 0xFF, 0x99, 0x99 },  /*  9: bright red */
		{ 0xCC, 0xFF, 0x99 },  /* 10: bright green */
		{ 0xFF, 0xFF, 0x99 },  /* 11: bright yellow */
		{ 0x99, 0xCC, 0xFF },  /* 12: bright blue */
		{ 0xFF, 0x99, 0xFF },  /* 13: bright magenta */
		{ 0x99, 0xFF, 0xFF },  /* 14: bright cyan */
		{ 0xFF, 0xFF, 0xFF },  /* 15: bright white */
	},

	{ 0xCC, 0xCC, 0xFF },  /* default_fg: snow storm */
	{ 0x33, 0x33, 0x66 },  /* default_bg: polar night */
	{ 0xCC, 0xCC, 0xFF },  /* cursor_color: snow storm */
	{ 0x66, 0x66, 0x99 },  /* sel_bg: nord3 */
	{ 0xCC, 0xCC, 0xFF },  /* sel_fg: snow storm */
	{ 0x00, 0x00, 0x00 },  /* bold_color: unused */
};
