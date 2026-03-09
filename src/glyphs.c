/*
 * glyphs.c - Unicode glyph tables and bitmap data
 *
 * Contains the lookup table mapping Unicode codepoints to glyph indices,
 * glyph info descriptors, and monochrome bitmap data for emoji glyphs.
 *
 * Copyright (c) 2024-2026 Flynn project
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "glyphs.h"

/* ----------------------------------------------------------------
 * Codepoint-to-glyph mapping tables
 *
 * Split into three lookup paths for 68000 performance:
 * 1. Direct-index tables for dense box-drawing (U+2500-U+253C) and
 *    block element (U+2580-U+2593) ranges (~20 cycles vs ~600)
 * 2. BMP table (U+0080-U+FFFF) with unsigned short keys (3 bytes/entry)
 *    for binary search with smaller struct indexing
 * 3. Astral table (U+10000+) with unsigned long keys, linear search
 *    (only 11 entries -- not worth binary search overhead)
 * ---------------------------------------------------------------- */

/* BMP mapping: 2-byte codepoint + 1-byte glyph ID */
typedef struct {
	unsigned short	codepoint;
	unsigned char	glyph_id;
} GlyphMappingBMP;

/* Astral mapping: 4-byte codepoint + 1-byte glyph ID */
typedef struct {
	unsigned long	codepoint;
	unsigned char	glyph_id;
} GlyphMappingAstral;

/* ----------------------------------------------------------------
 * Direct lookup tables for dense Unicode ranges
 *
 * Box drawing U+2500-U+253C (61 slots) and block elements
 * U+2580-U+2593 (20 slots).  Gaps filled with 0xFF (no glyph).
 * Replaces ~7 binary search iterations with a single array index.
 * ---------------------------------------------------------------- */

#define GLYPH_NONE	0xFF	/* sentinel: no glyph at this offset */

/* U+2500-U+253C: box drawing light lines (61 slots) */
static const unsigned char box_drawing_glyphs[0x3D] = {
	GLYPH_BOX_H,		/* U+2500 */
	GLYPH_NONE,		/* U+2501 */
	GLYPH_BOX_V,		/* U+2502 */
	GLYPH_NONE,		/* U+2503 */
	GLYPH_NONE,		/* U+2504 */
	GLYPH_NONE,		/* U+2505 */
	GLYPH_NONE,		/* U+2506 */
	GLYPH_NONE,		/* U+2507 */
	GLYPH_NONE,		/* U+2508 */
	GLYPH_NONE,		/* U+2509 */
	GLYPH_NONE,		/* U+250A */
	GLYPH_NONE,		/* U+250B */
	GLYPH_BOX_DR,		/* U+250C */
	GLYPH_NONE,		/* U+250D */
	GLYPH_NONE,		/* U+250E */
	GLYPH_NONE,		/* U+250F */
	GLYPH_BOX_DL,		/* U+2510 */
	GLYPH_NONE,		/* U+2511 */
	GLYPH_NONE,		/* U+2512 */
	GLYPH_NONE,		/* U+2513 */
	GLYPH_BOX_UR,		/* U+2514 */
	GLYPH_NONE,		/* U+2515 */
	GLYPH_NONE,		/* U+2516 */
	GLYPH_NONE,		/* U+2517 */
	GLYPH_BOX_UL,		/* U+2518 */
	GLYPH_NONE,		/* U+2519 */
	GLYPH_NONE,		/* U+251A */
	GLYPH_NONE,		/* U+251B */
	GLYPH_BOX_VR,		/* U+251C */
	GLYPH_NONE,		/* U+251D */
	GLYPH_NONE,		/* U+251E */
	GLYPH_NONE,		/* U+251F */
	GLYPH_NONE,		/* U+2520 */
	GLYPH_NONE,		/* U+2521 */
	GLYPH_NONE,		/* U+2522 */
	GLYPH_NONE,		/* U+2523 */
	GLYPH_BOX_VL,		/* U+2524 */
	GLYPH_NONE,		/* U+2525 */
	GLYPH_NONE,		/* U+2526 */
	GLYPH_NONE,		/* U+2527 */
	GLYPH_NONE,		/* U+2528 */
	GLYPH_NONE,		/* U+2529 */
	GLYPH_NONE,		/* U+252A */
	GLYPH_NONE,		/* U+252B */
	GLYPH_BOX_DH,		/* U+252C */
	GLYPH_NONE,		/* U+252D */
	GLYPH_NONE,		/* U+252E */
	GLYPH_NONE,		/* U+252F */
	GLYPH_NONE,		/* U+2530 */
	GLYPH_NONE,		/* U+2531 */
	GLYPH_NONE,		/* U+2532 */
	GLYPH_NONE,		/* U+2533 */
	GLYPH_BOX_UH,		/* U+2534 */
	GLYPH_NONE,		/* U+2535 */
	GLYPH_NONE,		/* U+2536 */
	GLYPH_NONE,		/* U+2537 */
	GLYPH_NONE,		/* U+2538 */
	GLYPH_NONE,		/* U+2539 */
	GLYPH_NONE,		/* U+253A */
	GLYPH_NONE,		/* U+253B */
	GLYPH_BOX_VH,		/* U+253C */
};

/* U+2580-U+2593: block elements and shade characters (20 slots) */
static const unsigned char block_element_glyphs[0x14] = {
	GLYPH_BLOCK_UPPER,	/* U+2580 */
	GLYPH_NONE,		/* U+2581 */
	GLYPH_NONE,		/* U+2582 */
	GLYPH_NONE,		/* U+2583 */
	GLYPH_BLOCK_LOWER,	/* U+2584 */
	GLYPH_NONE,		/* U+2585 */
	GLYPH_NONE,		/* U+2586 */
	GLYPH_NONE,		/* U+2587 */
	GLYPH_BLOCK_FULL,	/* U+2588 */
	GLYPH_NONE,		/* U+2589 */
	GLYPH_NONE,		/* U+258A */
	GLYPH_NONE,		/* U+258B */
	GLYPH_BLOCK_LEFT,	/* U+258C */
	GLYPH_NONE,		/* U+258D */
	GLYPH_NONE,		/* U+258E */
	GLYPH_NONE,		/* U+258F */
	GLYPH_BLOCK_RIGHT,	/* U+2590 */
	GLYPH_SHADE_LIGHT,	/* U+2591 */
	GLYPH_SHADE_MEDIUM,	/* U+2592 */
	GLYPH_SHADE_DARK,	/* U+2593 */
};

/* ----------------------------------------------------------------
 * BMP codepoint table (U+0080-U+FFFF, excludes direct-lookup ranges)
 * Sorted by codepoint for binary search.
 * ---------------------------------------------------------------- */

static const GlyphMappingBMP glyph_map_bmp[] = {
	{ 0x00B7, GLYPH_DOT_MIDDLE },
	{ 0x2190, GLYPH_ARROW_LEFT },
	{ 0x2191, GLYPH_ARROW_UP },
	{ 0x2192, GLYPH_ARROW_RIGHT },
	{ 0x2193, GLYPH_ARROW_DOWN },
	{ 0x2217, GLYPH_ASTERISK_OP },
	{ 0x22EE, GLYPH_ELLIPSIS_V },
	{ 0x23F5, GLYPH_PLAY },
	/* Quadrants (not in block_element_glyphs range) */
	{ 0x2596, GLYPH_QUAD_LL },
	{ 0x2597, GLYPH_QUAD_LR },
	{ 0x2598, GLYPH_QUAD_UL },
	{ 0x259B, GLYPH_QUAD_UL_UR_LL },
	{ 0x259C, GLYPH_QUAD_UL_UR_LR },
	{ 0x259D, GLYPH_QUAD_UR },
	/* Geometric shapes */
	{ 0x25A0, GLYPH_SQUARE_FILLED },
	{ 0x25A1, GLYPH_SQUARE_EMPTY },
	{ 0x25AA, GLYPH_SQ_SM_FILLED },
	{ 0x25AB, GLYPH_SQ_SM_EMPTY },
	{ 0x25B2, GLYPH_TRI_UP },
	{ 0x25B6, GLYPH_TRI_RIGHT },
	{ 0x25BC, GLYPH_TRI_DOWN },
	{ 0x25C0, GLYPH_TRI_LEFT },
	{ 0x25CA, GLYPH_LOZENGE },
	{ 0x25CB, GLYPH_CIRCLE_EMPTY },
	{ 0x25CF, GLYPH_CIRCLE_FILLED },
	{ 0x25FB, GLYPH_SQ_MED_EMPTY },
	{ 0x25FC, GLYPH_SQ_MED_FILLED },
	/* Symbols */
	{ 0x2605, GLYPH_STAR_FILLED },
	{ 0x2606, GLYPH_STAR_EMPTY },
	{ 0x2660, GLYPH_SPADE },
	{ 0x2663, GLYPH_CLUB },
	{ 0x2665, GLYPH_HEART },
	{ 0x2666, GLYPH_DIAMOND },
	{ 0x266A, GLYPH_MUSIC_NOTE },
	{ 0x266B, GLYPH_MUSIC_NOTES },
	{ 0x26A0, GLYPH_WARNING },
	{ 0x2705, GLYPH_EMOJI_CHECK },
	{ 0x2713, GLYPH_CHECK },
	{ 0x2714, GLYPH_CHECK_HEAVY },
	{ 0x2717, GLYPH_CROSS },
	{ 0x2718, GLYPH_CROSS_HEAVY },
	/* Asterisk spinners and dingbats */
	{ 0x2722, GLYPH_ASTERISK_FOUR },
	{ 0x2733, GLYPH_ASTERISK_8 },
	{ 0x273B, GLYPH_ASTERISK_TEARDROP },
	{ 0x273D, GLYPH_ASTERISK_HEAVY },
	{ 0x274C, GLYPH_EMOJI_CROSSMARK },
	{ 0x2764, GLYPH_EMOJI_HEART },
	{ 0x276F, GLYPH_CHEVRON_RIGHT },
	{ 0x2B50, GLYPH_EMOJI_STAR },
};

#define GLYPH_BMP_COUNT	(sizeof(glyph_map_bmp) / sizeof(glyph_map_bmp[0]))

/* ----------------------------------------------------------------
 * Astral codepoint table (U+10000+, emoji)
 * Small enough for linear search (~11 entries).
 * ---------------------------------------------------------------- */

static const GlyphMappingAstral glyph_map_astral[] = {
	{ 0x1F310, GLYPH_EMOJI_GLOBE },
	{ 0x1F40D, GLYPH_EMOJI_SNAKE },
	{ 0x1F44D, GLYPH_EMOJI_THUMBSUP },
	{ 0x1F4A1, GLYPH_EMOJI_BULB },
	{ 0x1F4C1, GLYPH_EMOJI_FOLDER },
	{ 0x1F4E6, GLYPH_EMOJI_PACKAGE },
	{ 0x1F525, GLYPH_EMOJI_FIRE },
	{ 0x1F527, GLYPH_EMOJI_WRENCH },
	{ 0x1F600, GLYPH_EMOJI_GRIN },
	{ 0x1F680, GLYPH_EMOJI_ROCKET },
	{ 0x1F980, GLYPH_EMOJI_CRAB },
};

#define GLYPH_ASTRAL_COUNT	(sizeof(glyph_map_astral) / sizeof(glyph_map_astral[0]))

/* ----------------------------------------------------------------
 * Glyph info table
 * ---------------------------------------------------------------- */

/* Primitive glyphs: single-cell, drawn with QuickDraw */
static const GlyphInfo prim_info[] = {
	/* 0x00 ARROW_LEFT */    { GLYPH_CAT_PRIMITIVE, 0, '<', 0 },
	/* 0x01 ARROW_UP */      { GLYPH_CAT_PRIMITIVE, 0, '^', 0 },
	/* 0x02 ARROW_RIGHT */   { GLYPH_CAT_PRIMITIVE, 0, '>', 0 },
	/* 0x03 ARROW_DOWN */    { GLYPH_CAT_PRIMITIVE, 0, 'v', 0 },
	/* 0x04 CHECK */         { GLYPH_CAT_PRIMITIVE, 0, 'v', 0 },
	/* 0x05 CROSS */         { GLYPH_CAT_PRIMITIVE, 0, 'x', 0 },
	/* 0x06 STAR_FILLED */   { GLYPH_CAT_PRIMITIVE, 0, '*', 0 },
	/* 0x07 STAR_EMPTY */    { GLYPH_CAT_PRIMITIVE, 0, '*', 0 },
	/* 0x08 HEART */         { GLYPH_CAT_PRIMITIVE, 0, '<', 0 },
	/* 0x09 CIRCLE_FILLED */ { GLYPH_CAT_PRIMITIVE, 0, 'o', 0 },
	/* 0x0A CIRCLE_EMPTY */  { GLYPH_CAT_PRIMITIVE, 0, 'o', 0 },
	/* 0x0B SQUARE_FILLED */ { GLYPH_CAT_PRIMITIVE, 0, '#', 0 },
	/* 0x0C SQUARE_EMPTY */  { GLYPH_CAT_PRIMITIVE, 0, '[', 0 },
	/* 0x0D TRI_UP */        { GLYPH_CAT_PRIMITIVE, 0, '^', 0 },
	/* 0x0E TRI_RIGHT */     { GLYPH_CAT_PRIMITIVE, 0, '>', 0 },
	/* 0x0F TRI_DOWN */      { GLYPH_CAT_PRIMITIVE, 0, 'v', 0 },
	/* 0x10 TRI_LEFT */      { GLYPH_CAT_PRIMITIVE, 0, '<', 0 },
	/* 0x11 MUSIC_NOTE */    { GLYPH_CAT_PRIMITIVE, 0, 'd', 0 },
	/* 0x12 MUSIC_NOTES */   { GLYPH_CAT_PRIMITIVE, 0, 'd', 0 },
	/* 0x13 SPADE */         { GLYPH_CAT_PRIMITIVE, 0, 'S', 0 },
	/* 0x14 CLUB */          { GLYPH_CAT_PRIMITIVE, 0, 'C', 0 },
	/* 0x15 DIAMOND */       { GLYPH_CAT_PRIMITIVE, 0, 'D', 0 },
	/* 0x16 LOZENGE */       { GLYPH_CAT_PRIMITIVE, 0, 'D', 0 },
	/* 0x17 ELLIPSIS_V */    { GLYPH_CAT_PRIMITIVE, 0, ':', 0 },
	/* 0x18 DASH_EM */       { GLYPH_CAT_PRIMITIVE, 0, '-', 0 },
	/* Block elements */
	/* 0x19 BLOCK_FULL */    { GLYPH_CAT_PRIMITIVE, 0, '#', 0 },
	/* 0x1A BLOCK_UPPER */   { GLYPH_CAT_PRIMITIVE, 0, '=', 0 },
	/* 0x1B BLOCK_LOWER */   { GLYPH_CAT_PRIMITIVE, 0, '_', 0 },
	/* 0x1C BLOCK_LEFT */    { GLYPH_CAT_PRIMITIVE, 0, '|', 0 },
	/* 0x1D BLOCK_RIGHT */   { GLYPH_CAT_PRIMITIVE, 0, '|', 0 },
	/* Quadrants */
	/* 0x1E QUAD_UL */       { GLYPH_CAT_PRIMITIVE, 0, '.', 0 },
	/* 0x1F QUAD_UR */       { GLYPH_CAT_PRIMITIVE, 0, '.', 0 },
	/* 0x20 QUAD_LL */       { GLYPH_CAT_PRIMITIVE, 0, '.', 0 },
	/* 0x21 QUAD_LR */       { GLYPH_CAT_PRIMITIVE, 0, '.', 0 },
	/* 0x22 QUAD_UL_UR_LL */ { GLYPH_CAT_PRIMITIVE, 0, '#', 0 },
	/* 0x23 QUAD_UL_UR_LR */ { GLYPH_CAT_PRIMITIVE, 0, '#', 0 },
	/* Asterisk spinners */
	/* 0x24 ASTERISK_TEARDROP */ { GLYPH_CAT_PRIMITIVE, 0, '*', 0 },
	/* 0x25 ASTERISK_FOUR */     { GLYPH_CAT_PRIMITIVE, 0, '+', 0 },
	/* 0x26 ASTERISK_HEAVY */    { GLYPH_CAT_PRIMITIVE, 0, '*', 0 },
	/* 0x27 ASTERISK_OP */       { GLYPH_CAT_PRIMITIVE, 0, '*', 0 },
	/* Medium squares */
	/* 0x28 SQ_MED_FILLED */ { GLYPH_CAT_PRIMITIVE, 0, '#', 0 },
	/* 0x29 SQ_MED_EMPTY */  { GLYPH_CAT_PRIMITIVE, 0, '[', 0 },
	/* Claude Code UI symbols */
	/* 0x2A PLAY */          { GLYPH_CAT_PRIMITIVE, 0, '>', 0 },
	/* 0x2B ASTERISK_8 */    { GLYPH_CAT_PRIMITIVE, 0, '*', 0 },
	/* 0x2C CHEVRON_RIGHT */ { GLYPH_CAT_PRIMITIVE, 0, '>', 0 },
	/* 0x2D WARNING */       { GLYPH_CAT_PRIMITIVE, 0, '!', 0 },
	/* 0x2E CHECK_HEAVY */   { GLYPH_CAT_PRIMITIVE, 0, 'v', 0 },
	/* 0x2F CROSS_HEAVY */   { GLYPH_CAT_PRIMITIVE, 0, 'x', 0 },
	/* Small squares */
	/* 0x30 SQ_SM_FILLED */  { GLYPH_CAT_PRIMITIVE, 0, '.', 0 },
	/* 0x31 SQ_SM_EMPTY */   { GLYPH_CAT_PRIMITIVE, 0, '.', 0 },
	/* 0x32 DOT_MIDDLE */    { GLYPH_CAT_PRIMITIVE, 0, '.', 0 },
	/* Box drawing */
	/* 0x33 BOX_H */           { GLYPH_CAT_PRIMITIVE, 0, '-', 0 },
	/* 0x34 BOX_V */           { GLYPH_CAT_PRIMITIVE, 0, '|', 0 },
	/* 0x35 BOX_DR */          { GLYPH_CAT_PRIMITIVE, 0, '+', 0 },
	/* 0x36 BOX_DL */          { GLYPH_CAT_PRIMITIVE, 0, '+', 0 },
	/* 0x37 BOX_UR */          { GLYPH_CAT_PRIMITIVE, 0, '+', 0 },
	/* 0x38 BOX_UL */          { GLYPH_CAT_PRIMITIVE, 0, '+', 0 },
	/* 0x39 BOX_VR */          { GLYPH_CAT_PRIMITIVE, 0, '+', 0 },
	/* 0x3A BOX_VL */          { GLYPH_CAT_PRIMITIVE, 0, '+', 0 },
	/* 0x3B BOX_DH */          { GLYPH_CAT_PRIMITIVE, 0, '+', 0 },
	/* 0x3C BOX_UH */          { GLYPH_CAT_PRIMITIVE, 0, '+', 0 },
	/* 0x3D BOX_VH */          { GLYPH_CAT_PRIMITIVE, 0, '+', 0 },
	/* Shades */
	/* 0x3E SHADE_LIGHT */     { GLYPH_CAT_PRIMITIVE, 0, '.', 0 },
	/* 0x3F SHADE_MEDIUM */    { GLYPH_CAT_PRIMITIVE, 0, ':', 0 },
	/* 0x40 SHADE_DARK */      { GLYPH_CAT_PRIMITIVE, 0, '#', 0 },
};

/* Emoji glyphs: wide (2-cell), drawn with CopyBits */
static const GlyphInfo emoji_info[] = {
	/* 0x60 GRIN */          { GLYPH_CAT_EMOJI, GLYPH_WIDE, ':', 0 },
	/* 0x61 HEART */         { GLYPH_CAT_EMOJI, GLYPH_WIDE, '<', 0 },
	/* 0x62 THUMBSUP */      { GLYPH_CAT_EMOJI, GLYPH_WIDE, '+', 0 },
	/* 0x63 FIRE */          { GLYPH_CAT_EMOJI, GLYPH_WIDE, '*', 0 },
	/* 0x64 STAR */          { GLYPH_CAT_EMOJI, GLYPH_WIDE, '*', 0 },
	/* 0x65 CHECK */         { GLYPH_CAT_EMOJI, GLYPH_WIDE, 'Y', 0 },
	/* 0x66 CROSSMARK */     { GLYPH_CAT_EMOJI, GLYPH_WIDE, 'X', 0 },
	/* 0x67 ROCKET */        { GLYPH_CAT_EMOJI, GLYPH_WIDE, '^', 0 },
	/* 0x68 FOLDER */        { GLYPH_CAT_EMOJI, GLYPH_WIDE, 'F', 0 },
	/* 0x69 BULB */          { GLYPH_CAT_EMOJI, GLYPH_WIDE, '!', 0 },
	/* 0x6A GLOBE */         { GLYPH_CAT_EMOJI, GLYPH_WIDE, 'O', 0 },
	/* 0x6B WRENCH */        { GLYPH_CAT_EMOJI, GLYPH_WIDE, '/', 0 },
	/* 0x6C PACKAGE */       { GLYPH_CAT_EMOJI, GLYPH_WIDE, '#', 0 },
	/* 0x6D SNAKE */         { GLYPH_CAT_EMOJI, GLYPH_WIDE, 'S', 0 },
	/* 0x6E CRAB */          { GLYPH_CAT_EMOJI, GLYPH_WIDE, 'V', 0 },
};

/* ----------------------------------------------------------------
 * Bitmap data for emoji glyphs (10x10, 1-bit, 2 bytes/row)
 * ----------------------------------------------------------------
 *
 * Each bitmap is 10 pixels wide, 10 pixels tall.
 * rowBytes = 2 (word-aligned).
 * Total = 20 bytes per bitmap.
 * Bit order: MSB first (standard Mac QuickDraw).
 */

/* Grinning face: simple smiley */
static const unsigned char bmp_grin[] = {
	0x1E, 0x00,	/* ..####.. .. */
	0x21, 0x00,	/* #....#.. .. */
	0x49, 0x00,	/* .#..#..# .. */
	0x41, 0x00,	/* .#.....# .. */
	0x41, 0x00,	/* .#.....# .. */
	0x45, 0x00,	/* .#...#.# .. */
	0x22, 0x00,	/* ..#...#. .. */
	0x1C, 0x00,	/* ...###.. .. */
	0x00, 0x00,
	0x00, 0x00,
};

/* Heart */
static const unsigned char bmp_heart[] = {
	0x00, 0x00,
	0x36, 0x00,	/* .##.##.. .. */
	0x7F, 0x00,	/* .####### .. */
	0x7F, 0x00,	/* .####### .. */
	0x7F, 0x00,	/* .####### .. */
	0x3E, 0x00,	/* ..#####. .. */
	0x1C, 0x00,	/* ...###.. .. */
	0x08, 0x00,	/* ....#... .. */
	0x00, 0x00,
	0x00, 0x00,
};

/* Thumbs up */
static const unsigned char bmp_thumbsup[] = {
	0x04, 0x00,	/* .....#.. .. */
	0x08, 0x00,	/* ....#... .. */
	0x08, 0x00,	/* ....#... .. */
	0x1C, 0x00,	/* ...###.. .. */
	0x3E, 0x00,	/* ..#####. .. */
	0x3E, 0x00,	/* ..#####. .. */
	0x3E, 0x00,	/* ..#####. .. */
	0x3E, 0x00,	/* ..#####. .. */
	0x1C, 0x00,	/* ...###.. .. */
	0x00, 0x00,
};

/* Fire */
static const unsigned char bmp_fire[] = {
	0x04, 0x00,	/* .....#.. .. */
	0x0C, 0x00,	/* ....##.. .. */
	0x0E, 0x00,	/* ....###. .. */
	0x1E, 0x00,	/* ...####. .. */
	0x3F, 0x00,	/* ..###### .. */
	0x3F, 0x00,	/* ..###### .. */
	0x3F, 0x00,	/* ..###### .. */
	0x1E, 0x00,	/* ...####. .. */
	0x0C, 0x00,	/* ....##.. .. */
	0x00, 0x00,
};

/* Star (5-pointed) */
static const unsigned char bmp_star[] = {
	0x08, 0x00,	/* ....#... .. */
	0x08, 0x00,	/* ....#... .. */
	0x3E, 0x00,	/* ..#####. .. */
	0x1F, 0x00,	/* ...##### .. */
	0x3E, 0x00,	/* ..#####. .. */
	0x7F, 0x00,	/* .####### .. */
	0x36, 0x00,	/* .##.##.. .. */
	0x22, 0x00,	/* ..#...#. .. */
	0x00, 0x00,
	0x00, 0x00,
};

/* Check (in box) */
static const unsigned char bmp_check[] = {
	0x00, 0x00,
	0x01, 0x00,	/* .......# .. */
	0x02, 0x00,	/* ......#. .. */
	0x04, 0x00,	/* .....#.. .. */
	0x48, 0x00,	/* .#..#... .. */
	0x30, 0x00,	/* ..##.... .. */
	0x20, 0x00,	/* ..#..... .. */
	0x00, 0x00,
	0x00, 0x00,
	0x00, 0x00,
};

/* Cross mark (X in box) */
static const unsigned char bmp_crossmark[] = {
	0x00, 0x00,
	0x41, 0x00,	/* .#.....# .. */
	0x22, 0x00,	/* ..#...#. .. */
	0x14, 0x00,	/* ...#.#.. .. */
	0x08, 0x00,	/* ....#... .. */
	0x14, 0x00,	/* ...#.#.. .. */
	0x22, 0x00,	/* ..#...#. .. */
	0x41, 0x00,	/* .#.....# .. */
	0x00, 0x00,
	0x00, 0x00,
};

/* Rocket */
static const unsigned char bmp_rocket[] = {
	0x04, 0x00,	/* .....#.. .. */
	0x0E, 0x00,	/* ....###. .. */
	0x0E, 0x00,	/* ....###. .. */
	0x1F, 0x00,	/* ...##### .. */
	0x1F, 0x00,	/* ...##### .. */
	0x3F, 0x80,	/* ..######.# */
	0x1F, 0x00,	/* ...##### .. */
	0x0E, 0x00,	/* ....###. .. */
	0x15, 0x00,	/* ...#.#.# .. */
	0x00, 0x00,
};

/* Folder */
static const unsigned char bmp_folder[] = {
	0x00, 0x00,
	0x38, 0x00,	/* ..###... .. */
	0x7F, 0x00,	/* .####### .. */
	0x7F, 0x00,	/* .####### .. */
	0x7F, 0x00,	/* .####### .. */
	0x7F, 0x00,	/* .####### .. */
	0x7F, 0x00,	/* .####### .. */
	0x7F, 0x00,	/* .####### .. */
	0x00, 0x00,
	0x00, 0x00,
};

/* Light bulb */
static const unsigned char bmp_bulb[] = {
	0x08, 0x00,	/* ....#... .. */
	0x1C, 0x00,	/* ...###.. .. */
	0x22, 0x00,	/* ..#...#. .. */
	0x22, 0x00,	/* ..#...#. .. */
	0x22, 0x00,	/* ..#...#. .. */
	0x1C, 0x00,	/* ...###.. .. */
	0x1C, 0x00,	/* ...###.. .. */
	0x08, 0x00,	/* ....#... .. */
	0x1C, 0x00,	/* ...###.. .. */
	0x00, 0x00,
};

/* Globe: circle with latitude/longitude grid lines */
static const unsigned char bmp_globe[] = {
	0x1C, 0x00,	/* ...###.. .. */
	0x22, 0x00,	/* ..#...#. .. */
	0x49, 0x00,	/* .#..#..# .. */
	0x5D, 0x00,	/* .#.###.# .. */
	0x7F, 0x00,	/* .####### .. */
	0x5D, 0x00,	/* .#.###.# .. */
	0x49, 0x00,	/* .#..#..# .. */
	0x22, 0x00,	/* ..#...#. .. */
	0x1C, 0x00,	/* ...###.. .. */
	0x00, 0x00,
};

/* Wrench: angled wrench */
static const unsigned char bmp_wrench[] = {
	0x38, 0x00,	/* ..###... .. */
	0x44, 0x00,	/* .#...#.. .. */
	0x38, 0x00,	/* ..###... .. */
	0x10, 0x00,	/* ...#.... .. */
	0x08, 0x00,	/* ....#... .. */
	0x04, 0x00,	/* .....#.. .. */
	0x02, 0x00,	/* ......#. .. */
	0x07, 0x00,	/* .....### .. */
	0x05, 0x00,	/* .....#.# .. */
	0x07, 0x00,	/* .....### .. */
};

/* Package: box with ribbon */
static const unsigned char bmp_package[] = {
	0x08, 0x00,	/* ....#... .. */
	0x7F, 0x00,	/* .####### .. */
	0x49, 0x00,	/* .#..#..# .. */
	0x7F, 0x00,	/* .####### .. */
	0x49, 0x00,	/* .#..#..# .. */
	0x49, 0x00,	/* .#..#..# .. */
	0x49, 0x00,	/* .#..#..# .. */
	0x7F, 0x00,	/* .####### .. */
	0x00, 0x00,
	0x00, 0x00,
};

/* Snake: S-curve shape */
static const unsigned char bmp_snake[] = {
	0x00, 0x00,
	0x3C, 0x00,	/* ..####.. .. */
	0x42, 0x00,	/* .#....#. .. */
	0x60, 0x00,	/* .##..... .. */
	0x3C, 0x00,	/* ..####.. .. */
	0x06, 0x00,	/* .....##. .. */
	0x42, 0x00,	/* .#....#. .. */
	0x3C, 0x00,	/* ..####.. .. */
	0x00, 0x00,
	0x00, 0x00,
};

/* Crab: body with claws */
static const unsigned char bmp_crab[] = {
	0x42, 0x00,	/* .#....#. .. */
	0x24, 0x00,	/* ..#..#.. .. */
	0x7E, 0x00,	/* .######. .. */
	0xFF, 0x00,	/* ######## .. */
	0xDB, 0x00,	/* ##.##.## .. */
	0x7E, 0x00,	/* .######. .. */
	0x24, 0x00,	/* ..#..#.. .. */
	0x42, 0x00,	/* .#....#. .. */
	0x00, 0x00,
	0x00, 0x00,
};

/* Bitmap table: indexed by (glyph_id - GLYPH_EMOJI_BASE) */
static const GlyphBitmap emoji_bitmaps[] = {
	{ 10, 10, 2, bmp_grin },	/* 0x60 */
	{ 10, 10, 2, bmp_heart },	/* 0x61 */
	{ 10, 10, 2, bmp_thumbsup },	/* 0x62 */
	{ 10, 10, 2, bmp_fire },	/* 0x63 */
	{ 10, 10, 2, bmp_star },	/* 0x64 */
	{ 10, 10, 2, bmp_check },	/* 0x65 */
	{ 10, 10, 2, bmp_crossmark },	/* 0x66 */
	{ 10, 10, 2, bmp_rocket },	/* 0x67 */
	{ 10, 10, 2, bmp_folder },	/* 0x68 */
	{ 10, 10, 2, bmp_bulb },	/* 0x69 */
	{ 10, 10, 2, bmp_globe },	/* 0x6A */
	{ 10, 10, 2, bmp_wrench },	/* 0x6B */
	{ 10, 10, 2, bmp_package },	/* 0x6C */
	{ 10, 10, 2, bmp_snake },	/* 0x6D */
	{ 10, 10, 2, bmp_crab },	/* 0x6E */
};

/* ----------------------------------------------------------------
 * Lookup functions
 * ---------------------------------------------------------------- */

/*
 * glyph_lookup - look up Unicode codepoint in optimized glyph tables
 *
 * Returns glyph index (0x00-0x7F) or -1 if not found.
 * Caller handles braille (U+2800-U+28FF) separately.
 *
 * Optimization layers (68000 @ 8MHz):
 * 1. ASCII fast-path:   cp < 0x80 => immediate return (no lookup needed)
 * 2. Direct index:      box drawing (U+2500-U+253C) and block elements
 *                       (U+2580-U+2593) via offset tables (~20 cycles)
 * 3. BMP binary search: unsigned short keys, smaller struct (~3 bytes)
 *                       for faster MULU during array indexing
 * 4. Astral linear:     11 emoji entries, simple scan
 */
short
glyph_lookup(long cp)
{
	short lo, hi, mid;
	unsigned short cp16;
	unsigned short map_cp;
	short i;
	unsigned char g;

	/* Fast path: ASCII characters never have glyph entries */
	if (cp < 0x80)
		return -1;

	/* BMP range: use direct-index tables and binary search */
	if (cp < 0x10000L) {
		/* Direct lookup: box drawing U+2500-U+253C */
		if (cp >= 0x2500 && cp <= 0x253C) {
			g = box_drawing_glyphs[cp - 0x2500];
			if (g != GLYPH_NONE)
				return (short)g;
			return -1;
		}

		/* Direct lookup: block elements U+2580-U+2593 */
		if (cp >= 0x2580 && cp <= 0x2593) {
			g = block_element_glyphs[cp - 0x2580];
			if (g != GLYPH_NONE)
				return (short)g;
			return -1;
		}

		/* Binary search BMP table with 16-bit comparisons */
		cp16 = (unsigned short)cp;
		lo = 0;
		hi = GLYPH_BMP_COUNT - 1;

		while (lo <= hi) {
			mid = (lo + hi) / 2;
			map_cp = glyph_map_bmp[mid].codepoint;
			if (cp16 == map_cp)
				return (short)glyph_map_bmp[mid].glyph_id;
			if (cp16 < map_cp)
				hi = mid - 1;
			else
				lo = mid + 1;
		}

		return -1;
	}

	/* Astral plane: linear search (only ~11 entries) */
	for (i = 0; i < (short)GLYPH_ASTRAL_COUNT; i++) {
		if (glyph_map_astral[i].codepoint == (unsigned long)cp)
			return (short)glyph_map_astral[i].glyph_id;
	}

	return -1;
}

/*
 * glyph_get_info - return info for a glyph index
 */
const GlyphInfo *
glyph_get_info(unsigned char glyph_id)
{
	if (glyph_id < GLYPH_PRIM_COUNT)
		return &prim_info[glyph_id];
	if (glyph_id >= GLYPH_EMOJI_BASE && glyph_id < GLYPH_EMOJI_BASE + GLYPH_EMOJI_COUNT)
		return &emoji_info[glyph_id - GLYPH_EMOJI_BASE];
	return (const GlyphInfo *)0;
}

/*
 * glyph_is_wide - check if glyph occupies 2 cells
 */
short
glyph_is_wide(unsigned char glyph_id)
{
	const GlyphInfo *info;

	info = glyph_get_info(glyph_id);
	if (info)
		return (info->flags & GLYPH_WIDE) ? 1 : 0;
	return 0;
}

/*
 * glyph_get_bitmap - get bitmap for emoji glyph
 *
 * Returns NULL for non-emoji glyphs.
 */
const GlyphBitmap *
glyph_get_bitmap(unsigned char glyph_id)
{
	if (glyph_id >= GLYPH_EMOJI_BASE && glyph_id < GLYPH_EMOJI_BASE + GLYPH_EMOJI_COUNT)
		return &emoji_bitmaps[glyph_id - GLYPH_EMOJI_BASE];
	return (const GlyphBitmap *)0;
}
