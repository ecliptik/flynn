/*
 * themes/system7.h - System 7 theme
 * ANSI palette derived from the Macintosh 16-color System CLUT (clut ID 4).
 * The classic Mac II palette: orange, brown, tan instead of bright ANSI.
 * Requires Color QuickDraw.
 */

static const TerminalTheme theme_system7 = {
	"System 7",     /* name */
	1,              /* is_color */
	0,              /* is_dark */

	/* ANSI palette mapped from Mac 16-color System CLUT */
	{
		{ 0x00, 0x00, 0x00 },  /*  0: Black */
		{ 0xDD, 0x08, 0x06 },  /*  1: Mac Red */
		{ 0x1F, 0xB7, 0x14 },  /*  2: Mac Green */
		{ 0xFC, 0xF3, 0x05 },  /*  3: Mac Yellow */
		{ 0x00, 0x00, 0xD4 },  /*  4: Mac Blue */
		{ 0xF2, 0x08, 0x84 },  /*  5: Mac Magenta (pink) */
		{ 0x02, 0xAB, 0xEA },  /*  6: Mac Cyan */
		{ 0xC0, 0xC0, 0xC0 },  /*  7: Light Gray */
		{ 0x40, 0x40, 0x40 },  /*  8: Dark Gray */
		{ 0xFF, 0x64, 0x02 },  /*  9: Mac Orange */
		{ 0x00, 0x64, 0x11 },  /* 10: Mac Dark Green */
		{ 0x90, 0x71, 0x3A },  /* 11: Mac Tan */
		{ 0x46, 0x00, 0xA5 },  /* 12: Mac Purple */
		{ 0xFF, 0x44, 0xAA },  /* 13: Bright Pink */
		{ 0x44, 0xCC, 0xFF },  /* 14: Bright Cyan */
		{ 0xFF, 0xFF, 0xFF },  /* 15: White */
	},

	{ 0x00, 0x00, 0x00 },  /* default_fg: black */
	{ 0xFF, 0xFF, 0xFF },  /* default_bg: white */
	{ 0x00, 0x00, 0x00 },  /* cursor_color: black */
	{ 0x33, 0x66, 0xCC },  /* sel_bg: System 7 blue highlight */
	{ 0xFF, 0xFF, 0xFF },  /* sel_fg: white */
	{ 0x00, 0x00, 0x00 },  /* bold_color: unused */
};
