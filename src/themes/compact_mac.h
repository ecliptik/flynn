/*
 * themes/compact_mac.h - Compact Mac theme
 * Warm cream-on-dark tones evoking the P7 phosphor CRT of the
 * Macintosh Plus, SE, and Classic. Aged phosphor shifts blue-white
 * toward yellow-green, giving the distinctive warm Mac screen look.
 * Requires Color QuickDraw.
 */

static const TerminalTheme theme_compact_mac = {
	"Compact Mac",  /* name */
	1,              /* is_color */
	0,              /* is_dark */

	/* ANSI palette — warm-shifted for P7 phosphor aesthetic, cube-snapped */
	{
		{ 0x33, 0x33, 0x00 },  /*  0: warm black */
		{ 0xCC, 0x33, 0x33 },  /*  1: warm red */
		{ 0x33, 0x99, 0x33 },  /*  2: warm green */
		{ 0xCC, 0x99, 0x33 },  /*  3: warm yellow */
		{ 0x33, 0x33, 0x99 },  /*  4: warm blue */
		{ 0x99, 0x33, 0x66 },  /*  5: warm magenta */
		{ 0x33, 0x99, 0x66 },  /*  6: warm cyan */
		{ 0xCC, 0xCC, 0x99 },  /*  7: warm cream */
		{ 0x66, 0x66, 0x33 },  /*  8: warm gray */
		{ 0xCC, 0x66, 0x33 },  /*  9: warm bright red */
		{ 0x66, 0xCC, 0x66 },  /* 10: warm bright green */
		{ 0xCC, 0xCC, 0x66 },  /* 11: warm bright yellow */
		{ 0x66, 0x66, 0xCC },  /* 12: warm bright blue */
		{ 0xCC, 0x66, 0x99 },  /* 13: warm bright magenta */
		{ 0x66, 0xCC, 0x99 },  /* 14: warm bright cyan */
		{ 0xFF, 0xFF, 0xCC },  /* 15: phosphor white */
	},

	{ 0x33, 0x33, 0x00 },  /* default_fg: warm dark */
	{ 0xFF, 0xFF, 0xCC },  /* default_bg: P7 phosphor cream */
	{ 0x33, 0x33, 0x00 },  /* cursor_color: warm dark */
	{ 0x66, 0x66, 0x33 },  /* sel_bg: warm gray */
	{ 0xFF, 0xFF, 0xCC },  /* sel_fg: phosphor cream */
	{ 0x00, 0x00, 0x00 },  /* bold_color: unused */
};
