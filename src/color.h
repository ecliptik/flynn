/*
 * color.h - 256-color support for System 7 Color QuickDraw
 *
 * Provides xterm 256-color palette, color detection, and palette
 * management. All color functionality is conditional on runtime
 * detection of Color QuickDraw via SysEnvirons(). On System 6
 * (monochrome), all color code is skipped at zero cost.
 *
 * Copyright (c) 2024-2026 Flynn project
 */

#ifndef COLOR_H
#define COLOR_H

#include <Quickdraw.h>

/* Color index sentinel: "use terminal default fg/bg" */
#define COLOR_DEFAULT	0xFF

/* Per-cell color (2 bytes, allocated separately from TermCell) */
typedef struct {
	unsigned char	fg;	/* 0-254: xterm palette, 0xFF = default */
	unsigned char	bg;	/* 0-254: xterm palette, 0xFF = default */
} CellColor;

/* Global flag: true if Color QuickDraw is available (set at startup) */
extern unsigned char g_has_color_qd;

/* Detect Color QuickDraw at startup via SysEnvirons() */
void color_detect(void);

/* Get RGBColor for a palette index (0-255) */
void color_get_rgb(unsigned char index, RGBColor *rgb);

/* Map truecolor RGB to nearest 256-color palette index */
unsigned char color_nearest_256(unsigned char r, unsigned char g,
    unsigned char b);

#endif /* COLOR_H */
