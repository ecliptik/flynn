/*
 * themes/nord.h - Nord theme
 * Arctic blue-tinted palette from nordtheme.com.
 * Canonical RGB values — may lose some distinction from Dracula at 256 colors.
 * Requires Color QuickDraw.
 */

static const TerminalTheme theme_nord = {
	"Nord",         /* name */
	1,              /* is_color */
	1,              /* is_dark */

	/* ANSI palette (canonical Nord) */
	{
		{ 0x3B, 0x42, 0x52 },  /*  0: nord1 (polar night) */
		{ 0xBF, 0x61, 0x6A },  /*  1: nord11 (aurora red) */
		{ 0xA3, 0xBE, 0x8C },  /*  2: nord14 (aurora green) */
		{ 0xEB, 0xCB, 0x8B },  /*  3: nord13 (aurora yellow) */
		{ 0x81, 0xA1, 0xC1 },  /*  4: nord9 (frost) */
		{ 0xB4, 0x8E, 0xAD },  /*  5: nord15 (aurora purple) */
		{ 0x88, 0xC0, 0xD0 },  /*  6: nord8 (frost) */
		{ 0xE5, 0xE9, 0xF0 },  /*  7: nord5 (snow storm) */
		{ 0x4C, 0x56, 0x6A },  /*  8: nord3 (polar night) */
		{ 0xBF, 0x61, 0x6A },  /*  9: nord11 (aurora red) */
		{ 0xA3, 0xBE, 0x8C },  /* 10: nord14 (aurora green) */
		{ 0xEB, 0xCB, 0x8B },  /* 11: nord13 (aurora yellow) */
		{ 0x81, 0xA1, 0xC1 },  /* 12: nord9 (frost) */
		{ 0xB4, 0x8E, 0xAD },  /* 13: nord15 (aurora purple) */
		{ 0x8F, 0xBC, 0xBB },  /* 14: nord7 (frost) */
		{ 0xEC, 0xEF, 0xF4 },  /* 15: nord6 (snow storm) */
	},

	{ 0xD8, 0xDE, 0xE9 },  /* default_fg: nord4 (snow storm) */
	{ 0x2E, 0x34, 0x40 },  /* default_bg: nord0 (polar night) */
	{ 0xD8, 0xDE, 0xE9 },  /* cursor_color: nord4 */
	{ 0x43, 0x4C, 0x5E },  /* sel_bg: nord2 */
	{ 0xD8, 0xDE, 0xE9 },  /* sel_fg: nord4 */
	{ 0x00, 0x00, 0x00 },  /* bold_color: unused */
};
