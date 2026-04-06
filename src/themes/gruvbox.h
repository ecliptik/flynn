/*
 * themes/gruvbox.h - Gruvbox Dark theme
 * Optional theme - not included in default build.
 * Dark earthy retro palette with warm accents.
 * Requires Color QuickDraw.
 *
 * To enable: add #include, theme_table[] entry, bump THEME_COUNT,
 * add THEME_GRUVBOX index constant, and add menu item.
 */

static const TerminalTheme theme_gruvbox = {
	"Gruvbox",      /* name */
	1,              /* is_color */
	1,              /* is_dark */

	/* ANSI palette (Gruvbox Dark, cube-snapped) */
	{
		{ 0x33, 0x33, 0x00 },  /*  0: bg0 */
		{ 0xCC, 0x33, 0x33 },  /*  1: red */
		{ 0x99, 0x99, 0x33 },  /*  2: green */
		{ 0xCC, 0x99, 0x33 },  /*  3: yellow */
		{ 0x66, 0x99, 0xCC },  /*  4: blue */
		{ 0xCC, 0x66, 0x99 },  /*  5: purple */
		{ 0x66, 0x99, 0x66 },  /*  6: aqua */
		{ 0xFF, 0xCC, 0x99 },  /*  7: fg */
		{ 0x66, 0x66, 0x33 },  /*  8: gray */
		{ 0xFF, 0x66, 0x33 },  /*  9: bright red (orange) */
		{ 0xCC, 0xCC, 0x66 },  /* 10: bright green */
		{ 0xFF, 0xCC, 0x66 },  /* 11: bright yellow */
		{ 0x99, 0xCC, 0xFF },  /* 12: bright blue */
		{ 0xFF, 0x99, 0xCC },  /* 13: bright purple */
		{ 0x99, 0xCC, 0x99 },  /* 14: bright aqua */
		{ 0xFF, 0xFF, 0xCC },  /* 15: bright white */
	},

	{ 0xFF, 0xCC, 0x99 },  /* default_fg: warm cream */
	{ 0x33, 0x33, 0x00 },  /* default_bg: dark brown */
	{ 0xFF, 0xCC, 0x99 },  /* cursor_color: warm cream */
	{ 0x66, 0x66, 0x33 },  /* sel_bg: muted brown */
	{ 0xFF, 0xCC, 0x99 },  /* sel_fg: warm cream */
	{ 0x00, 0x00, 0x00 },  /* bold_color: unused */
};
