/*
 * color.c - 256-color support for System 7 Color QuickDraw
 *
 * Contains the xterm 256-color palette (16 ANSI + 216 color cube +
 * 24 grayscale), runtime Color QD detection, truecolor-to-256 mapping,
 * and RGB color helpers.
 *
 * All functions are no-ops or return defaults when Color QuickDraw
 * is not available (g_has_color_qd == false).
 *
 * Copyright (c) 2024-2026 Flynn project
 */

#include <OSUtils.h>
#include <Quickdraw.h>
#include <Multiverse.h>
#include "color.h"

/* Global color detection flag */
unsigned char g_has_color_qd = 0;

/*
 * Standard xterm ANSI colors (indices 0-15).
 * Indices 16-231 (6x6x6 color cube) and 232-255 (grayscale ramp)
 * are computed algorithmically in color_get_rgb() to save ~720 bytes.
 */
static const unsigned char ansi_palette[16][3] = {
	/* 0-7: Standard ANSI */
	{   0,   0,   0 },	/* 0: black */
	{ 205,   0,   0 },	/* 1: red */
	{   0, 205,   0 },	/* 2: green */
	{ 205, 205,   0 },	/* 3: yellow */
	{   0,   0, 238 },	/* 4: blue */
	{ 205,   0, 205 },	/* 5: magenta */
	{   0, 205, 205 },	/* 6: cyan */
	{ 229, 229, 229 },	/* 7: white */
	/* 8-15: Bright ANSI */
	{ 127, 127, 127 },	/* 8: bright black */
	{ 255,   0,   0 },	/* 9: bright red */
	{   0, 255,   0 },	/* 10: bright green */
	{ 255, 255,   0 },	/* 11: bright yellow */
	{  92,  92, 255 },	/* 12: bright blue */
	{ 255,   0, 255 },	/* 13: bright magenta */
	{   0, 255, 255 },	/* 14: bright cyan */
	{ 255, 255, 255 }	/* 15: bright white */
};

/* 6x6x6 color cube component values */
static const unsigned char cube_vals[6] = { 0, 95, 135, 175, 215, 255 };

/*
 * color_detect - detect Color QuickDraw at startup
 *
 * Uses SysEnvirons() which is available on all Macs from 128K ROMs.
 * On System 6 / Mac Plus, hasColorQD will be false.
 */
void
color_detect(void)
{
	SysEnvRec env;

	g_has_color_qd = 0;
	if (SysEnvirons(1, &env) == noErr && env.hasColorQD)
		g_has_color_qd = 1;
}

/*
 * color_get_rgb - convert 8-bit palette index to Mac RGBColor
 *
 * Mac RGBColor uses 16-bit components (0-65535).
 * Conversion: component16 = component8 * 257 (maps 0→0, 255→65535)
 *
 * Indices 0-15 use the static ANSI table. Indices 16-231 are computed
 * from the 6x6x6 color cube formula. Indices 232-255 are computed
 * from the grayscale ramp formula. This saves ~720 bytes vs a full
 * 256-entry static table.
 */
void
color_get_rgb(unsigned char index, RGBColor *rgb)
{
	unsigned char r, g, b;

	if (index < 16) {
		/* ANSI colors: table lookup */
		r = ansi_palette[index][0];
		g = ansi_palette[index][1];
		b = ansi_palette[index][2];
	} else if (index < 232) {
		/* 6x6x6 color cube: index = 16 + 36*R + 6*G + B */
		short ci = index - 16;
		r = cube_vals[ci / 36];
		g = cube_vals[(ci / 6) % 6];
		b = cube_vals[ci % 6];
	} else {
		/* Grayscale ramp: 8, 18, 28, ..., 238 */
		r = 8 + (index - 232) * 10;
		g = r;
		b = r;
	}

	rgb->red   = (unsigned short)r * 257;
	rgb->green = (unsigned short)g * 257;
	rgb->blue  = (unsigned short)b * 257;
}

/*
 * color_nearest_256 - map truecolor RGB (8-bit each) to nearest
 *                     xterm 256-color palette index.
 *
 * Strategy: check if color is close to grayscale first, then
 * map to the 6x6x6 color cube.
 */
unsigned char
color_nearest_256(unsigned char r, unsigned char g, unsigned char b)
{
	short cr, cg, cb;
	short gi;
	short cube_idx;
	short cube_dist, gray_dist;
	short gray_val;
	unsigned char gray_idx;

	/* Map each component to nearest cube level */
	if (r < 48) cr = 0;
	else if (r < 115) cr = 1;
	else {
		cr = (r - 35) / 40;
		if (cr > 5) cr = 5;
	}
	if (g < 48) cg = 0;
	else if (g < 115) cg = 1;
	else {
		cg = (g - 35) / 40;
		if (cg > 5) cg = 5;
	}
	if (b < 48) cb = 0;
	else if (b < 115) cb = 1;
	else {
		cb = (b - 35) / 40;
		if (cb > 5) cb = 5;
	}

	cube_idx = 16 + 36 * cr + 6 * cg + cb;

	/* Distance to cube color (squared, per-component) */
	cube_dist = (r - cube_vals[cr]) * (r - cube_vals[cr]) +
	    (g - cube_vals[cg]) * (g - cube_vals[cg]) +
	    (b - cube_vals[cb]) * (b - cube_vals[cb]);

	/* Check grayscale ramp (indices 232-255: 8, 18, 28, ..., 238) */
	gray_val = (r + g + b) / 3;
	if (gray_val < 4)
		gi = 0;
	else if (gray_val > 243)
		gi = 23;
	else
		gi = (gray_val - 3) / 10;
	gray_idx = 232 + gi;

	/* Distance to grayscale value */
	{
		short gv = 8 + gi * 10;
		gray_dist = (r - gv) * (r - gv) +
		    (g - gv) * (g - gv) +
		    (b - gv) * (b - gv);
	}

	return (gray_dist < cube_dist) ? gray_idx :
	    (unsigned char)cube_idx;
}
