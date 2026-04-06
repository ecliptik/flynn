/*
 * themes/green_screen.h - Green Screen (phosphor terminal) theme
 * Classic green-on-black CRT aesthetic with amber errors.
 * Requires Color QuickDraw.
 */

static const TerminalTheme theme_green_screen = {
	"Green Screen",  /* name */
	1,               /* is_color */
	1,               /* is_dark */

	/* ANSI palette (all-green phosphor with amber errors) */
	{
		{ 0x00, 0x00, 0x00 },  /*  0: black */
		{ 0xFF, 0x66, 0x33 },  /*  1: amber (errors) */
		{ 0x00, 0xCC, 0x00 },  /*  2: green */
		{ 0x66, 0xCC, 0x00 },  /*  3: yellow-green */
		{ 0x00, 0x99, 0x33 },  /*  4: dark green */
		{ 0x33, 0xCC, 0x66 },  /*  5: teal */
		{ 0x00, 0xCC, 0x66 },  /*  6: cyan-green */
		{ 0x33, 0xFF, 0x33 },  /*  7: phosphor */
		{ 0x00, 0x66, 0x00 },  /*  8: dim green */
		{ 0xFF, 0x99, 0x33 },  /*  9: bright amber */
		{ 0x33, 0xFF, 0x33 },  /* 10: bright green */
		{ 0x99, 0xFF, 0x33 },  /* 11: bright yellow-green */
		{ 0x00, 0xCC, 0x33 },  /* 12: medium green */
		{ 0x66, 0xFF, 0x66 },  /* 13: pale green */
		{ 0x33, 0xFF, 0x99 },  /* 14: bright teal */
		{ 0x99, 0xFF, 0x99 },  /* 15: white-green */
	},

	{ 0x33, 0xFF, 0x33 },  /* default_fg: phosphor green */
	{ 0x00, 0x00, 0x00 },  /* default_bg: black */
	{ 0x33, 0xFF, 0x33 },  /* cursor_color: phosphor green */
	{ 0x00, 0x66, 0x00 },  /* sel_bg: dark green */
	{ 0x66, 0xFF, 0x66 },  /* sel_fg: bright green */
	{ 0x66, 0xFF, 0x66 },  /* bold_color: bright green */
};
