/*
 * themes/platinum.h - Mac OS 8/9 Platinum theme
 * Optional theme - not included in default build.
 * Gray background with purple accents, inspired by Platinum appearance.
 * Requires Color QuickDraw.
 *
 * To enable: add #include, theme_table[] entry, bump THEME_COUNT,
 * add THEME_PLATINUM index constant, and add menu item.
 */

static const TerminalTheme theme_platinum = {
	"Platinum",     /* name */
	1,              /* is_color */
	0,              /* is_dark */

	/* ANSI palette (Mac OS 8/9 inspired, purple accents) */
	{
		{ 0x00, 0x00, 0x00 },  /*  0: black */
		{ 0xCC, 0x33, 0x33 },  /*  1: red */
		{ 0x33, 0x99, 0x33 },  /*  2: green */
		{ 0xCC, 0x99, 0x33 },  /*  3: yellow */
		{ 0x33, 0x33, 0x99 },  /*  4: blue */
		{ 0x99, 0x33, 0x99 },  /*  5: magenta */
		{ 0x33, 0x99, 0x99 },  /*  6: cyan */
		{ 0xCC, 0xCC, 0xCC },  /*  7: white */
		{ 0x66, 0x66, 0x66 },  /*  8: bright black */
		{ 0xFF, 0x66, 0x66 },  /*  9: bright red */
		{ 0x66, 0xCC, 0x66 },  /* 10: bright green */
		{ 0xFF, 0xCC, 0x66 },  /* 11: bright yellow */
		{ 0x66, 0x66, 0xCC },  /* 12: bright blue */
		{ 0xCC, 0x66, 0xCC },  /* 13: bright magenta */
		{ 0x66, 0xCC, 0xCC },  /* 14: bright cyan */
		{ 0xFF, 0xFF, 0xFF },  /* 15: bright white */
	},

	{ 0x00, 0x00, 0x00 },  /* default_fg: black */
	{ 0xCC, 0xCC, 0xCC },  /* default_bg: platinum gray */
	{ 0x00, 0x00, 0x00 },  /* cursor_color: black */
	{ 0x99, 0x99, 0xFF },  /* sel_bg: purple highlight */
	{ 0x00, 0x00, 0x00 },  /* sel_fg: black */
	{ 0x00, 0x00, 0x00 },  /* bold_color: unused */
};
