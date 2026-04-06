/*
 * themes/solarized_dark.h - Solarized Dark theme
 * Dark teal background with muted palette.
 * Requires Color QuickDraw.
 */

static const TerminalTheme theme_solarized_dark = {
	"Solarized Dark",   /* name */
	1,                  /* is_color */
	1,                  /* is_dark */

	/* ANSI palette — base colors canonical, accents cube-snapped */
	{
		{ 0x07, 0x36, 0x42 },  /*  0: base02 (#073642) */
		{ 0xCC, 0x33, 0x33 },  /*  1: red */
		{ 0x66, 0x99, 0x00 },  /*  2: green */
		{ 0x99, 0x99, 0x00 },  /*  3: yellow */
		{ 0x33, 0x99, 0xCC },  /*  4: blue */
		{ 0xCC, 0x33, 0xCC },  /*  5: magenta */
		{ 0x33, 0x99, 0x99 },  /*  6: cyan */
		{ 0xEE, 0xE8, 0xD5 },  /*  7: base2 (#eee8d5) */
		{ 0x00, 0x2B, 0x36 },  /*  8: base03 (#002b36) */
		{ 0xCC, 0x66, 0x00 },  /*  9: orange */
		{ 0x58, 0x6E, 0x75 },  /* 10: base01 (#586e75) */
		{ 0x65, 0x7B, 0x83 },  /* 11: base00 (#657b83) */
		{ 0x83, 0x94, 0x96 },  /* 12: base0 (#839496) */
		{ 0x66, 0x66, 0xCC },  /* 13: violet */
		{ 0x93, 0xA1, 0xA1 },  /* 14: base1 (#93a1a1) */
		{ 0xFD, 0xF6, 0xE3 },  /* 15: base3 (#fdf6e3) */
	},

	{ 0x93, 0xA1, 0xA1 },  /* default_fg: base1 (#93a1a1) */
	{ 0x00, 0x2B, 0x36 },  /* default_bg: base03 (#002b36) */
	{ 0x93, 0xA1, 0xA1 },  /* cursor_color: base1 */
	{ 0x33, 0x99, 0xCC },  /* sel_bg: blue */
	{ 0xFD, 0xF6, 0xE3 },  /* sel_fg: base3 */
	{ 0x93, 0xA1, 0xA1 },  /* bold_color: base1 */
};
