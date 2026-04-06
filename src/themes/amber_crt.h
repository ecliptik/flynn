/*
 * themes/amber_crt.h - Amber CRT theme
 * Amber phosphor CRT aesthetic — all ANSI colors are warm-shifted
 * through the amber spectrum. No cool tones. Cube-snapped.
 * Requires Color QuickDraw.
 */

static const TerminalTheme theme_amber_crt = {
	"Amber CRT",    /* name */
	1,              /* is_color */
	1,              /* is_dark */

	/* ANSI palette (amber-tinted, cube-snapped) */
	{
		{ 0x33, 0x33, 0x00 },  /*  0: dark amber */
		{ 0xCC, 0x66, 0x00 },  /*  1: orange (warm red) */
		{ 0x66, 0x99, 0x00 },  /*  2: olive (warm green) */
		{ 0xCC, 0x99, 0x00 },  /*  3: amber (yellow) */
		{ 0x66, 0x66, 0x33 },  /*  4: dim olive (warm blue) */
		{ 0x99, 0x66, 0x33 },  /*  5: brown (warm magenta) */
		{ 0x66, 0x99, 0x33 },  /*  6: olive-green (warm cyan) */
		{ 0xCC, 0xCC, 0x66 },  /*  7: light amber */
		{ 0x66, 0x66, 0x00 },  /*  8: medium amber */
		{ 0xFF, 0x99, 0x00 },  /*  9: bright orange */
		{ 0x99, 0xCC, 0x33 },  /* 10: bright olive-green */
		{ 0xFF, 0xCC, 0x33 },  /* 11: bright amber */
		{ 0x99, 0x99, 0x66 },  /* 12: warm gray (bright blue) */
		{ 0xCC, 0x99, 0x66 },  /* 13: warm tan (bright magenta) */
		{ 0x99, 0xCC, 0x66 },  /* 14: warm light green */
		{ 0xFF, 0xFF, 0x99 },  /* 15: warm white */
	},

	{ 0xCC, 0x99, 0x00 },  /* default_fg: amber gold */
	{ 0x00, 0x00, 0x00 },  /* default_bg: CRT black */
	{ 0xFF, 0xCC, 0x33 },  /* cursor_color: bright amber */
	{ 0x66, 0x66, 0x00 },  /* sel_bg: dark amber */
	{ 0xFF, 0xCC, 0x33 },  /* sel_fg: bright amber */
	{ 0x00, 0x00, 0x00 },  /* bold_color: unused */
};
