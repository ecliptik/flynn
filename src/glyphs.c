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
 * Codepoint-to-glyph mapping table (sorted by codepoint for bsearch)
 * ---------------------------------------------------------------- */

typedef struct {
	long		codepoint;
	unsigned char	glyph_id;
} GlyphMapping;

static const GlyphMapping glyph_map[] = {
	/* Sorted by codepoint for binary search */
	{ 0x00B7, GLYPH_DOT_MIDDLE },
	{ 0x2190, GLYPH_ARROW_LEFT },
	{ 0x2191, GLYPH_ARROW_UP },
	{ 0x2192, GLYPH_ARROW_RIGHT },
	{ 0x2193, GLYPH_ARROW_DOWN },
	{ 0x2217, GLYPH_ASTERISK_OP },
	{ 0x22EE, GLYPH_ELLIPSIS_V },
	{ 0x23F5, GLYPH_PLAY },
	/* Block elements */
	{ 0x2580, GLYPH_BLOCK_UPPER },
	{ 0x2584, GLYPH_BLOCK_LOWER },
	{ 0x2588, GLYPH_BLOCK_FULL },
	{ 0x258C, GLYPH_BLOCK_LEFT },
	{ 0x2590, GLYPH_BLOCK_RIGHT },
	/* Quadrants */
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
	/* Emoji (U+1Fxxx) */
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

#define GLYPH_MAP_COUNT	(sizeof(glyph_map) / sizeof(glyph_map[0]))

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
};

/* Emoji glyphs: wide (2-cell), drawn with CopyBits */
static const GlyphInfo emoji_info[] = {
	/* 0x40 GRIN */          { GLYPH_CAT_EMOJI, GLYPH_WIDE, ':', 0 },
	/* 0x41 HEART */         { GLYPH_CAT_EMOJI, GLYPH_WIDE, '<', 0 },
	/* 0x42 THUMBSUP */      { GLYPH_CAT_EMOJI, GLYPH_WIDE, '+', 0 },
	/* 0x43 FIRE */          { GLYPH_CAT_EMOJI, GLYPH_WIDE, '*', 0 },
	/* 0x44 STAR */          { GLYPH_CAT_EMOJI, GLYPH_WIDE, '*', 0 },
	/* 0x45 CHECK */         { GLYPH_CAT_EMOJI, GLYPH_WIDE, 'Y', 0 },
	/* 0x46 CROSSMARK */     { GLYPH_CAT_EMOJI, GLYPH_WIDE, 'X', 0 },
	/* 0x47 ROCKET */        { GLYPH_CAT_EMOJI, GLYPH_WIDE, '^', 0 },
	/* 0x48 FOLDER */        { GLYPH_CAT_EMOJI, GLYPH_WIDE, 'F', 0 },
	/* 0x49 BULB */          { GLYPH_CAT_EMOJI, GLYPH_WIDE, '!', 0 },
	/* 0x4A GLOBE */         { GLYPH_CAT_EMOJI, GLYPH_WIDE, 'O', 0 },
	/* 0x4B WRENCH */        { GLYPH_CAT_EMOJI, GLYPH_WIDE, '/', 0 },
	/* 0x4C PACKAGE */       { GLYPH_CAT_EMOJI, GLYPH_WIDE, '#', 0 },
	/* 0x4D SNAKE */         { GLYPH_CAT_EMOJI, GLYPH_WIDE, 'S', 0 },
	/* 0x4E CRAB */          { GLYPH_CAT_EMOJI, GLYPH_WIDE, 'V', 0 },
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

/* Bitmap table: indexed by (glyph_id - 0x40) */
static const GlyphBitmap emoji_bitmaps[] = {
	{ 10, 10, 2, bmp_grin },	/* 0x40 */
	{ 10, 10, 2, bmp_heart },	/* 0x41 */
	{ 10, 10, 2, bmp_thumbsup },	/* 0x42 */
	{ 10, 10, 2, bmp_fire },	/* 0x43 */
	{ 10, 10, 2, bmp_star },	/* 0x44 */
	{ 10, 10, 2, bmp_check },	/* 0x45 */
	{ 10, 10, 2, bmp_crossmark },	/* 0x46 */
	{ 10, 10, 2, bmp_rocket },	/* 0x47 */
	{ 10, 10, 2, bmp_folder },	/* 0x48 */
	{ 10, 10, 2, bmp_bulb },	/* 0x49 */
	{ 10, 10, 2, bmp_globe },	/* 0x4A */
	{ 10, 10, 2, bmp_wrench },	/* 0x4B */
	{ 10, 10, 2, bmp_package },	/* 0x4C */
	{ 10, 10, 2, bmp_snake },	/* 0x4D */
	{ 10, 10, 2, bmp_crab },	/* 0x4E */
};

/* ----------------------------------------------------------------
 * Lookup functions
 * ---------------------------------------------------------------- */

/*
 * glyph_lookup - binary search for codepoint in glyph_map
 *
 * Returns glyph index (0x00-0x5F) or -1 if not found.
 * Caller handles braille (U+2800-U+28FF) separately.
 */
short
glyph_lookup(long cp)
{
	short lo, hi, mid;
	long map_cp;

	lo = 0;
	hi = GLYPH_MAP_COUNT - 1;

	while (lo <= hi) {
		mid = (lo + hi) / 2;
		map_cp = glyph_map[mid].codepoint;
		if (cp == map_cp)
			return (short)glyph_map[mid].glyph_id;
		if (cp < map_cp)
			hi = mid - 1;
		else
			lo = mid + 1;
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
	if (glyph_id >= 0x40 && glyph_id < 0x40 + GLYPH_EMOJI_COUNT)
		return &emoji_info[glyph_id - 0x40];
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
	if (glyph_id >= 0x40 && glyph_id < 0x40 + GLYPH_EMOJI_COUNT)
		return &emoji_bitmaps[glyph_id - 0x40];
	return (const GlyphBitmap *)0;
}
