/*
 * themes/solarized_light.h - Solarized Light theme
 * Warm cream background with muted palette.
 * Requires Color QuickDraw.
 */

static const TerminalTheme theme_solarized_light = {
	"Solarized Light",  /* name */
	1,                  /* is_color */
	0,                  /* is_dark */

	/* ANSI palette — base colors canonical, accents cube-snapped.
	 * Colors 7/15 remapped for light bg readability: apps that
	 * emit "white" text must remain visible on cream background. */
	{
		{ 0x07, 0x36, 0x42 },  /*  0: base02 (#073642) */
		{ 0xCC, 0x33, 0x33 },  /*  1: red */
		{ 0x66, 0x99, 0x00 },  /*  2: green */
		{ 0x99, 0x99, 0x00 },  /*  3: yellow */
		{ 0x33, 0x99, 0xCC },  /*  4: blue */
		{ 0xCC, 0x33, 0xCC },  /*  5: magenta */
		{ 0x33, 0x99, 0x99 },  /*  6: cyan */
		{ 0x83, 0x94, 0x96 },  /*  7: base0 — visible "white" on light bg */
		{ 0x00, 0x2B, 0x36 },  /*  8: base03 (#002b36) */
		{ 0xCC, 0x66, 0x00 },  /*  9: orange */
		{ 0x58, 0x6E, 0x75 },  /* 10: base01 (#586e75) */
		{ 0x65, 0x7B, 0x83 },  /* 11: base00 (#657b83) */
		{ 0x58, 0x6E, 0x75 },  /* 12: base01 — visible "bright black" */
		{ 0x66, 0x66, 0xCC },  /* 13: violet */
		{ 0x93, 0xA1, 0xA1 },  /* 14: base1 (#93a1a1) */
		{ 0x65, 0x7B, 0x83 },  /* 15: base00 — visible "bright white" on light bg */
	},

	{ 0x65, 0x7B, 0x83 },  /* default_fg: base00 (#657b83) */
	{ 0xFB, 0xF4, 0xDB },  /* default_bg: near-base3, avoids gray ramp at 256 */
	{ 0x65, 0x7B, 0x83 },  /* cursor_color: base00 */
	{ 0x33, 0x99, 0xCC },  /* sel_bg: blue */
	{ 0xFB, 0xF4, 0xDB },  /* sel_fg: near-base3 */
	{ 0x07, 0x36, 0x42 },  /* bold_color: base02 */
};
