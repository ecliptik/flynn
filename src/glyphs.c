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

/* U+2550-U+256C: double-line box drawing (29 slots) */
static const unsigned char dbox_drawing_glyphs[0x1D] = {
	GLYPH_BOX2_H,		/* U+2550 ═ */
	GLYPH_BOX2_V,		/* U+2551 ║ */
	GLYPH_BOX_sDdR,	/* U+2552 ╒ */
	GLYPH_BOX_dDsR,	/* U+2553 ╓ */
	GLYPH_BOX2_DR,		/* U+2554 ╔ */
	GLYPH_BOX_sDdL,	/* U+2555 ╕ */
	GLYPH_BOX_dDsL,	/* U+2556 ╖ */
	GLYPH_BOX2_DL,		/* U+2557 ╗ */
	GLYPH_BOX_sUdR,	/* U+2558 ╘ */
	GLYPH_BOX_dUsR,	/* U+2559 ╙ */
	GLYPH_BOX2_UR,		/* U+255A ╚ */
	GLYPH_BOX_sUdL,	/* U+255B ╛ */
	GLYPH_BOX_dUsL,	/* U+255C ╜ */
	GLYPH_BOX2_UL,		/* U+255D ╝ */
	GLYPH_BOX_sVdR,	/* U+255E ╞ */
	GLYPH_BOX_dVsR,	/* U+255F ╟ */
	GLYPH_BOX2_VR,		/* U+2560 ╠ */
	GLYPH_BOX_sVdL,	/* U+2561 ╡ */
	GLYPH_BOX_dVsL,	/* U+2562 ╢ */
	GLYPH_BOX2_VL,		/* U+2563 ╣ */
	GLYPH_BOX_dHsD,	/* U+2564 ╤ */
	GLYPH_BOX_sHdD,	/* U+2565 ╥ */
	GLYPH_BOX2_DH,		/* U+2566 ╦ */
	GLYPH_BOX_dHsU,	/* U+2567 ╧ */
	GLYPH_BOX_sHdU,	/* U+2568 ╨ */
	GLYPH_BOX2_UH,		/* U+2569 ╩ */
	GLYPH_BOX_sVdVH,	/* U+256A ╪ */
	GLYPH_BOX_dVsVH,	/* U+256B ╫ */
	GLYPH_BOX2_VH,		/* U+256C ╬ */
};

/* U+2580-U+2595: block elements, shade characters, edge blocks (22 slots) */
static const unsigned char block_element_glyphs[0x16] = {
	GLYPH_BLOCK_UPPER,	/* U+2580 */
	GLYPH_BLOCK_LOWER_1,	/* U+2581 lower 1/8 */
	GLYPH_BLOCK_LOWER_2,	/* U+2582 lower 1/4 */
	GLYPH_BLOCK_LOWER_3,	/* U+2583 lower 3/8 */
	GLYPH_BLOCK_LOWER,	/* U+2584 lower half */
	GLYPH_BLOCK_LOWER_5,	/* U+2585 lower 5/8 */
	GLYPH_BLOCK_LOWER_6,	/* U+2586 lower 3/4 */
	GLYPH_BLOCK_LOWER_7,	/* U+2587 lower 7/8 */
	GLYPH_BLOCK_FULL,	/* U+2588 full block */
	GLYPH_BLOCK_LEFT_7,	/* U+2589 left 7/8 */
	GLYPH_BLOCK_LEFT_6,	/* U+258A left 3/4 */
	GLYPH_BLOCK_LEFT_5,	/* U+258B left 5/8 */
	GLYPH_BLOCK_LEFT,	/* U+258C left half */
	GLYPH_BLOCK_LEFT_3,	/* U+258D left 3/8 */
	GLYPH_BLOCK_LEFT_2,	/* U+258E left 1/4 */
	GLYPH_BLOCK_LEFT_1,	/* U+258F left 1/8 */
	GLYPH_BLOCK_RIGHT,	/* U+2590 right half */
	GLYPH_SHADE_LIGHT,	/* U+2591 */
	GLYPH_SHADE_MEDIUM,	/* U+2592 */
	GLYPH_SHADE_DARK,	/* U+2593 */
	GLYPH_BLOCK_UPPER_1,	/* U+2594 upper 1/8 */
	GLYPH_BLOCK_RIGHT_1,	/* U+2595 right 1/8 */
};

/* ----------------------------------------------------------------
 * BMP codepoint table (U+0080-U+FFFF, excludes direct-lookup ranges)
 * Sorted by codepoint for binary search.
 * ---------------------------------------------------------------- */

static const GlyphMappingBMP glyph_map_bmp[] = {
	{ 0x00B2, GLYPH_SUPER_2 },
	{ 0x00B3, GLYPH_SUPER_3 },
	{ 0x00B7, GLYPH_DOT_MIDDLE },
	{ 0x00B9, GLYPH_SUPER_1 },
	{ 0x00BC, GLYPH_QUARTER },
	{ 0x00BD, GLYPH_HALF },
	{ 0x0393, GLYPH_GAMMA },
	{ 0x0398, GLYPH_THETA },
	{ 0x03A6, GLYPH_PHI_UC },
	{ 0x03B4, GLYPH_DELTA_LC },
	{ 0x2070, GLYPH_SUPER_0 },
	{ 0x2074, GLYPH_SUPER_4 },
	{ 0x2075, GLYPH_SUPER_5 },
	{ 0x2076, GLYPH_SUPER_6 },
	{ 0x2077, GLYPH_SUPER_7 },
	{ 0x2078, GLYPH_SUPER_8 },
	{ 0x2079, GLYPH_SUPER_9 },
	{ 0x2080, GLYPH_SUB_0 },
	{ 0x2081, GLYPH_SUB_1 },
	{ 0x2082, GLYPH_SUB_2 },
	{ 0x2083, GLYPH_SUB_3 },
	{ 0x2084, GLYPH_SUB_4 },
	{ 0x2085, GLYPH_SUB_5 },
	{ 0x2086, GLYPH_SUB_6 },
	{ 0x2087, GLYPH_SUB_7 },
	{ 0x2088, GLYPH_SUB_8 },
	{ 0x2089, GLYPH_SUB_9 },
	{ 0x2190, GLYPH_ARROW_LEFT },
	{ 0x2191, GLYPH_ARROW_UP },
	{ 0x2192, GLYPH_ARROW_RIGHT },
	{ 0x2193, GLYPH_ARROW_DOWN },
	{ 0x2194, GLYPH_ARROW_LEFTRIGHT },
	{ 0x2195, GLYPH_ARROW_UPDOWN },
	{ 0x21A8, GLYPH_ARROW_UPDOWN_BASE },
	{ 0x2217, GLYPH_ASTERISK_OP },
	{ 0x221A, GLYPH_SQRT },
	{ 0x221E, GLYPH_INFINITY },
	{ 0x221F, GLYPH_RIGHT_ANGLE },
	{ 0x2229, GLYPH_INTERSECT },
	{ 0x2248, GLYPH_APPROX },
	{ 0x2261, GLYPH_IDENTICAL },
	{ 0x2295, GLYPH_CIRCLED_PLUS },
	{ 0x2296, GLYPH_CIRCLED_MINUS },
	{ 0x2297, GLYPH_CIRCLED_TIMES },
	{ 0x2299, GLYPH_CIRCLED_DOT },
	{ 0x22EE, GLYPH_ELLIPSIS_V },
	{ 0x2302, GLYPH_HOUSE },
	{ 0x2310, GLYPH_REVERSED_NOT },
	{ 0x2320, GLYPH_INTEGRAL_T },
	{ 0x2321, GLYPH_INTEGRAL_B },
	{ 0x23F1, GLYPH_EMOJI_STOPWATCH },
	{ 0x23F5, GLYPH_PLAY },
	/* Quadrants (not in block_element_glyphs range) */
	{ 0x2596, GLYPH_QUAD_LL },
	{ 0x2597, GLYPH_QUAD_LR },
	{ 0x2598, GLYPH_QUAD_UL },
	{ 0x2599, GLYPH_QUAD_UL_LL_LR },
	{ 0x259A, GLYPH_QUAD_UL_LR },
	{ 0x259B, GLYPH_QUAD_UL_UR_LL },
	{ 0x259C, GLYPH_QUAD_UL_UR_LR },
	{ 0x259D, GLYPH_QUAD_UR },
	{ 0x259E, GLYPH_QUAD_UR_LL },
	{ 0x259F, GLYPH_QUAD_UR_LL_LR },
	/* Geometric shapes */
	{ 0x25A0, GLYPH_SQUARE_FILLED },
	{ 0x25A1, GLYPH_SQUARE_EMPTY },
	{ 0x25AA, GLYPH_SQ_SM_FILLED },
	{ 0x25AB, GLYPH_SQ_SM_EMPTY },
	{ 0x25AC, GLYPH_BAR_H },
	{ 0x25B2, GLYPH_TRI_UP },
	{ 0x25B3, GLYPH_TRI_UP_EMPTY },
	{ 0x25B6, GLYPH_TRI_RIGHT },
	{ 0x25B7, GLYPH_TRI_RIGHT_EMPTY },
	{ 0x25B8, GLYPH_TRI_RIGHT_SM },
	{ 0x25BC, GLYPH_TRI_DOWN },
	{ 0x25BD, GLYPH_TRI_DOWN_EMPTY },
	{ 0x25C0, GLYPH_TRI_LEFT },
	{ 0x25C1, GLYPH_TRI_LEFT_EMPTY },
	{ 0x25C2, GLYPH_TRI_LEFT_SM },
	{ 0x25C6, GLYPH_DIAMOND_FILLED },
	{ 0x25C7, GLYPH_DIAMOND_EMPTY },
	{ 0x25C9, GLYPH_CIRCLE_DOT },
	{ 0x25CA, GLYPH_LOZENGE },
	{ 0x25CB, GLYPH_CIRCLE_EMPTY },
	{ 0x25CF, GLYPH_CIRCLE_FILLED },
	{ 0x25D0, GLYPH_CIRCLE_HALF_L },
	{ 0x25D1, GLYPH_CIRCLE_HALF_R },
	{ 0x25D2, GLYPH_CIRCLE_HALF_B },
	{ 0x25D3, GLYPH_CIRCLE_HALF_T },
	{ 0x25D8, GLYPH_INV_BULLET },
	{ 0x25D9, GLYPH_INV_CIRCLE },
	{ 0x25FB, GLYPH_SQ_MED_EMPTY },
	{ 0x25FC, GLYPH_SQ_MED_FILLED },
	/* Symbols */
	{ 0x2605, GLYPH_STAR_FILLED },
	{ 0x2606, GLYPH_STAR_EMPTY },
	{ 0x263A, GLYPH_SMILEY },
	{ 0x263B, GLYPH_SMILEY_INV },
	{ 0x263C, GLYPH_SUN },
	{ 0x2640, GLYPH_FEMALE },
	{ 0x2642, GLYPH_MALE },
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
	{ 0x2728, GLYPH_ASTERISK_8 },	/* ✨ sparkles */
	{ 0x2733, GLYPH_ASTERISK_8 },
	{ 0x2736, GLYPH_STAR_SIX },
	{ 0x273B, GLYPH_ASTERISK_TEARDROP },
	{ 0x273C, GLYPH_ASTERISK_TEARDROP },	/* ✼ open teardrop asterisk */
	{ 0x273D, GLYPH_ASTERISK_HEAVY },
	{ 0x273E, GLYPH_FLOWER },	/* ✾ six petalled florette */
	{ 0x273F, GLYPH_FLOWER },	/* ✿ black florette */
	{ 0x2740, GLYPH_FLOWER },	/* ❀ white florette */
	{ 0x2741, GLYPH_FLOWER },	/* ❁ eight petalled florette */
	{ 0x2744, GLYPH_SNOWFLAKE },	/* ❄ snowflake */
	{ 0x274B, GLYPH_ASTERISK_HEAVY },	/* ❋ heavy teardrop propeller */
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
	{ 0x1F31F, GLYPH_EMOJI_STAR },	/* 🌟 glowing star */
	{ 0x1F338, GLYPH_FLOWER },	/* 🌸 cherry blossom */
	{ 0x1F33A, GLYPH_FLOWER },	/* 🌺 hibiscus */
	{ 0x1F33C, GLYPH_FLOWER },	/* 🌼 blossom */
	{ 0x1F33F, GLYPH_EMOJI_HERB },	/* 🌿 herb */
	{ 0x1F40D, GLYPH_EMOJI_SNAKE },
	{ 0x1F44D, GLYPH_EMOJI_THUMBSUP },
	{ 0x1F4A1, GLYPH_EMOJI_BULB },
	{ 0x1F4AB, GLYPH_EMOJI_STAR },	/* 💫 dizzy */
	{ 0x1F4B0, GLYPH_EMOJI_MONEY },	/* 💰 money bag */
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
	/* Outline triangles */
	/* 0x41 TRI_UP_EMPTY */    { GLYPH_CAT_PRIMITIVE, 0, '^', 0 },
	/* 0x42 TRI_RIGHT_EMPTY */ { GLYPH_CAT_PRIMITIVE, 0, '>', 0 },
	/* 0x43 TRI_DOWN_EMPTY */  { GLYPH_CAT_PRIMITIVE, 0, 'v', 0 },
	/* 0x44 TRI_LEFT_EMPTY */  { GLYPH_CAT_PRIMITIVE, 0, '<', 0 },
	/* Small filled triangles */
	/* 0x45 TRI_RIGHT_SM */    { GLYPH_CAT_PRIMITIVE, 0, '>', 0 },
	/* 0x46 TRI_LEFT_SM */     { GLYPH_CAT_PRIMITIVE, 0, '<', 0 },
	/* Diamonds */
	/* 0x47 DIAMOND_FILLED */  { GLYPH_CAT_PRIMITIVE, 0, 'D', 0 },
	/* 0x48 DIAMOND_EMPTY */   { GLYPH_CAT_PRIMITIVE, 0, 'D', 0 },
	/* Circle variants */
	/* 0x49 CIRCLE_HALF_L */   { GLYPH_CAT_PRIMITIVE, 0, 'o', 0 },
	/* 0x4A CIRCLE_HALF_R */   { GLYPH_CAT_PRIMITIVE, 0, 'o', 0 },
	/* 0x4B CIRCLE_HALF_B */   { GLYPH_CAT_PRIMITIVE, 0, 'o', 0 },
	/* 0x4C CIRCLE_HALF_T */   { GLYPH_CAT_PRIMITIVE, 0, 'o', 0 },
	/* 0x4D CIRCLE_DOT */      { GLYPH_CAT_PRIMITIVE, 0, 'o', 0 },
	/* Six-pointed star */
	/* 0x4E STAR_SIX */        { GLYPH_CAT_PRIMITIVE, 0, '*', 0 },
	/* Circled operators */
	/* 0x4F CIRCLED_DOT */     { GLYPH_CAT_PRIMITIVE, 0, '.', 0 },
	/* 0x50 CIRCLED_PLUS */    { GLYPH_CAT_PRIMITIVE, 0, '+', 0 },
	/* 0x51 CIRCLED_MINUS */   { GLYPH_CAT_PRIMITIVE, 0, '-', 0 },
	/* 0x52 CIRCLED_TIMES */   { GLYPH_CAT_PRIMITIVE, 0, 'x', 0 },
	/* Superscript digits */
	/* 0x53 SUPER_0 */         { GLYPH_CAT_PRIMITIVE, 0, '0', 0 },
	/* 0x54 SUPER_1 */         { GLYPH_CAT_PRIMITIVE, 0, '1', 0 },
	/* 0x55 SUPER_2 */         { GLYPH_CAT_PRIMITIVE, 0, '2', 0 },
	/* 0x56 SUPER_3 */         { GLYPH_CAT_PRIMITIVE, 0, '3', 0 },
	/* 0x57 SUPER_4 */         { GLYPH_CAT_PRIMITIVE, 0, '4', 0 },
	/* 0x58 SUPER_5 */         { GLYPH_CAT_PRIMITIVE, 0, '5', 0 },
	/* 0x59 SUPER_6 */         { GLYPH_CAT_PRIMITIVE, 0, '6', 0 },
	/* 0x5A SUPER_7 */         { GLYPH_CAT_PRIMITIVE, 0, '7', 0 },
	/* 0x5B SUPER_8 */         { GLYPH_CAT_PRIMITIVE, 0, '8', 0 },
	/* 0x5C SUPER_9 */         { GLYPH_CAT_PRIMITIVE, 0, '9', 0 },
	/* Subscript digits */
	/* 0x5D SUB_0 */           { GLYPH_CAT_PRIMITIVE, 0, '0', 0 },
	/* 0x5E SUB_1 */           { GLYPH_CAT_PRIMITIVE, 0, '1', 0 },
	/* 0x5F SUB_2 */           { GLYPH_CAT_PRIMITIVE, 0, '2', 0 },
	/* 0x60 SUB_3 */           { GLYPH_CAT_PRIMITIVE, 0, '3', 0 },
	/* 0x61 SUB_4 */           { GLYPH_CAT_PRIMITIVE, 0, '4', 0 },
	/* 0x62 SUB_5 */           { GLYPH_CAT_PRIMITIVE, 0, '5', 0 },
	/* 0x63 SUB_6 */           { GLYPH_CAT_PRIMITIVE, 0, '6', 0 },
	/* 0x64 SUB_7 */           { GLYPH_CAT_PRIMITIVE, 0, '7', 0 },
	/* 0x65 SUB_8 */           { GLYPH_CAT_PRIMITIVE, 0, '8', 0 },
	/* 0x66 SUB_9 */           { GLYPH_CAT_PRIMITIVE, 0, '9', 0 },
	/* CP437 double-line box drawing */
	/* 0x67 BOX2_V */          { GLYPH_CAT_PRIMITIVE, 0, '|', 0 },
	/* 0x68 BOX2_H */          { GLYPH_CAT_PRIMITIVE, 0, '=', 0 },
	/* 0x69 BOX2_DR */         { GLYPH_CAT_PRIMITIVE, 0, '+', 0 },
	/* 0x6A BOX2_DL */         { GLYPH_CAT_PRIMITIVE, 0, '+', 0 },
	/* 0x6B BOX2_UR */         { GLYPH_CAT_PRIMITIVE, 0, '+', 0 },
	/* 0x6C BOX2_UL */         { GLYPH_CAT_PRIMITIVE, 0, '+', 0 },
	/* 0x6D BOX2_VR */         { GLYPH_CAT_PRIMITIVE, 0, '+', 0 },
	/* 0x6E BOX2_VL */         { GLYPH_CAT_PRIMITIVE, 0, '+', 0 },
	/* 0x6F BOX2_DH */         { GLYPH_CAT_PRIMITIVE, 0, '+', 0 },
	/* 0x70 BOX2_UH */         { GLYPH_CAT_PRIMITIVE, 0, '+', 0 },
	/* 0x71 BOX2_VH */         { GLYPH_CAT_PRIMITIVE, 0, '+', 0 },
	/* Mixed single/double box junctions */
	/* 0x72 BOX_sVdL */        { GLYPH_CAT_PRIMITIVE, 0, '+', 0 },
	/* 0x73 BOX_dVsL */        { GLYPH_CAT_PRIMITIVE, 0, '+', 0 },
	/* 0x74 BOX_dDsL */        { GLYPH_CAT_PRIMITIVE, 0, '+', 0 },
	/* 0x75 BOX_sDdL */        { GLYPH_CAT_PRIMITIVE, 0, '+', 0 },
	/* 0x76 BOX_dUsL */        { GLYPH_CAT_PRIMITIVE, 0, '+', 0 },
	/* 0x77 BOX_sUdL */        { GLYPH_CAT_PRIMITIVE, 0, '+', 0 },
	/* 0x78 BOX_sVdR */        { GLYPH_CAT_PRIMITIVE, 0, '+', 0 },
	/* 0x79 BOX_dVsR */        { GLYPH_CAT_PRIMITIVE, 0, '+', 0 },
	/* 0x7A BOX_dHsU */        { GLYPH_CAT_PRIMITIVE, 0, '+', 0 },
	/* 0x7B BOX_sHdU */        { GLYPH_CAT_PRIMITIVE, 0, '+', 0 },
	/* 0x7C BOX_dHsD */        { GLYPH_CAT_PRIMITIVE, 0, '+', 0 },
	/* 0x7D BOX_sHdD */        { GLYPH_CAT_PRIMITIVE, 0, '+', 0 },
	/* 0x7E BOX_dUsR */        { GLYPH_CAT_PRIMITIVE, 0, '+', 0 },
	/* 0x7F BOX_sUdR */        { GLYPH_CAT_PRIMITIVE, 0, '+', 0 },
	/* 0x80 BOX_sDdR */        { GLYPH_CAT_PRIMITIVE, 0, '+', 0 },
	/* 0x81 BOX_dDsR */        { GLYPH_CAT_PRIMITIVE, 0, '+', 0 },
	/* 0x82 BOX_dVsVH */       { GLYPH_CAT_PRIMITIVE, 0, '+', 0 },
	/* 0x83 BOX_sVdVH */       { GLYPH_CAT_PRIMITIVE, 0, '+', 0 },
	/* CP437 symbol glyphs */
	/* 0x84 SMILEY */          { GLYPH_CAT_PRIMITIVE, 0, 'O', 0 },
	/* 0x85 SMILEY_INV */      { GLYPH_CAT_PRIMITIVE, 0, 'O', 0 },
	/* 0x86 INV_BULLET */      { GLYPH_CAT_PRIMITIVE, 0, 'o', 0 },
	/* 0x87 INV_CIRCLE */      { GLYPH_CAT_PRIMITIVE, 0, 'o', 0 },
	/* 0x88 MALE */            { GLYPH_CAT_PRIMITIVE, 0, 'M', 0 },
	/* 0x89 FEMALE */          { GLYPH_CAT_PRIMITIVE, 0, 'F', 0 },
	/* 0x8A SUN */             { GLYPH_CAT_PRIMITIVE, 0, '*', 0 },
	/* 0x8B ARROW_UPDOWN */    { GLYPH_CAT_PRIMITIVE, 0, '|', 0 },
	/* 0x8C BAR_H */           { GLYPH_CAT_PRIMITIVE, 0, '-', 0 },
	/* 0x8D ARROW_UPDOWN_BASE */ { GLYPH_CAT_PRIMITIVE, 0, '|', 0 },
	/* 0x8E RIGHT_ANGLE */     { GLYPH_CAT_PRIMITIVE, 0, 'L', 0 },
	/* 0x8F ARROW_LEFTRIGHT */ { GLYPH_CAT_PRIMITIVE, 0, '-', 0 },
	/* 0x90 HOUSE */           { GLYPH_CAT_PRIMITIVE, 0, '^', 0 },
	/* CP437 math/Greek glyphs */
	/* 0x91 REVERSED_NOT */    { GLYPH_CAT_PRIMITIVE, 0, '-', 0 },
	/* 0x92 HALF */            { GLYPH_CAT_PRIMITIVE, 0, '/', 0 },
	/* 0x93 QUARTER */         { GLYPH_CAT_PRIMITIVE, 0, '/', 0 },
	/* 0x94 GAMMA */           { GLYPH_CAT_PRIMITIVE, 0, 'G', 0 },
	/* 0x95 PHI_UC */          { GLYPH_CAT_PRIMITIVE, 0, 'O', 0 },
	/* 0x96 THETA */           { GLYPH_CAT_PRIMITIVE, 0, 'O', 0 },
	/* 0x97 DELTA_LC */        { GLYPH_CAT_PRIMITIVE, 0, 'd', 0 },
	/* 0x98 INFINITY */        { GLYPH_CAT_PRIMITIVE, 0, '8', 0 },
	/* 0x99 INTERSECT */       { GLYPH_CAT_PRIMITIVE, 0, 'n', 0 },
	/* 0x9A IDENTICAL */       { GLYPH_CAT_PRIMITIVE, 0, '=', 0 },
	/* 0x9B INTEGRAL_T */      { GLYPH_CAT_PRIMITIVE, 0, '/', 0 },
	/* 0x9C INTEGRAL_B */      { GLYPH_CAT_PRIMITIVE, 0, '/', 0 },
	/* 0x9D APPROX */          { GLYPH_CAT_PRIMITIVE, 0, '~', 0 },
	/* 0x9E SQRT */            { GLYPH_CAT_PRIMITIVE, 0, 'V', 0 },
	/* 0x9F SUPER_N */         { GLYPH_CAT_PRIMITIVE, 0, 'n', 0 },
	/* Fractional lower blocks */
	/* 0xA0 BLOCK_LOWER_1 */   { GLYPH_CAT_PRIMITIVE, 0, '_', 0 },
	/* 0xA1 BLOCK_LOWER_2 */   { GLYPH_CAT_PRIMITIVE, 0, '_', 0 },
	/* 0xA2 BLOCK_LOWER_3 */   { GLYPH_CAT_PRIMITIVE, 0, '_', 0 },
	/* 0xA3 BLOCK_LOWER_5 */   { GLYPH_CAT_PRIMITIVE, 0, '#', 0 },
	/* 0xA4 BLOCK_LOWER_6 */   { GLYPH_CAT_PRIMITIVE, 0, '#', 0 },
	/* 0xA5 BLOCK_LOWER_7 */   { GLYPH_CAT_PRIMITIVE, 0, '#', 0 },
	/* Fractional left blocks */
	/* 0xA6 BLOCK_LEFT_7 */    { GLYPH_CAT_PRIMITIVE, 0, '|', 0 },
	/* 0xA7 BLOCK_LEFT_6 */    { GLYPH_CAT_PRIMITIVE, 0, '|', 0 },
	/* 0xA8 BLOCK_LEFT_5 */    { GLYPH_CAT_PRIMITIVE, 0, '|', 0 },
	/* 0xA9 BLOCK_LEFT_3 */    { GLYPH_CAT_PRIMITIVE, 0, '|', 0 },
	/* 0xAA BLOCK_LEFT_2 */    { GLYPH_CAT_PRIMITIVE, 0, '|', 0 },
	/* 0xAB BLOCK_LEFT_1 */    { GLYPH_CAT_PRIMITIVE, 0, '|', 0 },
	/* Edge blocks */
	/* 0xAC BLOCK_UPPER_1 */   { GLYPH_CAT_PRIMITIVE, 0, '-', 0 },
	/* 0xAD BLOCK_RIGHT_1 */   { GLYPH_CAT_PRIMITIVE, 0, '|', 0 },
	/* Missing quadrants */
	/* 0xAE QUAD_UL_LL_LR */   { GLYPH_CAT_PRIMITIVE, 0, '#', 0 },
	/* 0xAF QUAD_UL_LR */      { GLYPH_CAT_PRIMITIVE, 0, '.', 0 },
	/* 0xB0 QUAD_UR_LL */      { GLYPH_CAT_PRIMITIVE, 0, '.', 0 },
	/* 0xB1 QUAD_UR_LL_LR */   { GLYPH_CAT_PRIMITIVE, 0, '#', 0 },
	/* Flower and snowflake */
	/* 0xB2 FLOWER */          { GLYPH_CAT_PRIMITIVE, 0, '*', 0 },
	/* 0xB3 SNOWFLAKE */       { GLYPH_CAT_PRIMITIVE, 0, '*', 0 },
};

/* Emoji glyphs: wide (2-cell), drawn with CopyBits */
static const GlyphInfo emoji_info[] = {
	/* 0xA0 GRIN */          { GLYPH_CAT_EMOJI, GLYPH_WIDE, ':', 0 },
	/* 0xA1 HEART */         { GLYPH_CAT_EMOJI, GLYPH_WIDE, '<', 0 },
	/* 0xA2 THUMBSUP */      { GLYPH_CAT_EMOJI, GLYPH_WIDE, '+', 0 },
	/* 0xA3 FIRE */          { GLYPH_CAT_EMOJI, GLYPH_WIDE, '*', 0 },
	/* 0xA4 STAR */          { GLYPH_CAT_EMOJI, GLYPH_WIDE, '*', 0 },
	/* 0xA5 CHECK */         { GLYPH_CAT_EMOJI, GLYPH_WIDE, 'Y', 0 },
	/* 0xA6 CROSSMARK */     { GLYPH_CAT_EMOJI, GLYPH_WIDE, 'X', 0 },
	/* 0xA7 ROCKET */        { GLYPH_CAT_EMOJI, GLYPH_WIDE, '^', 0 },
	/* 0xA8 FOLDER */        { GLYPH_CAT_EMOJI, GLYPH_WIDE, 'F', 0 },
	/* 0xA9 BULB */          { GLYPH_CAT_EMOJI, GLYPH_WIDE, '!', 0 },
	/* 0xAA GLOBE */         { GLYPH_CAT_EMOJI, GLYPH_WIDE, 'O', 0 },
	/* 0xAB WRENCH */        { GLYPH_CAT_EMOJI, GLYPH_WIDE, '/', 0 },
	/* 0xAC PACKAGE */       { GLYPH_CAT_EMOJI, GLYPH_WIDE, '#', 0 },
	/* 0xAD SNAKE */         { GLYPH_CAT_EMOJI, GLYPH_WIDE, 'S', 0 },
	/* 0xAE CRAB */          { GLYPH_CAT_EMOJI, GLYPH_WIDE, 'V', 0 },
	/* 0xFC HERB */          { GLYPH_CAT_EMOJI, GLYPH_WIDE, 'H', 0 },
	/* 0xFD MONEY */         { GLYPH_CAT_EMOJI, GLYPH_WIDE, '$', 0 },
	/* 0xFE STOPWATCH */     { GLYPH_CAT_EMOJI, GLYPH_WIDE, 'T', 0 },
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

/* Herb: leaf/plant */
static const unsigned char bmp_herb[] = {
	0x02, 0x00,	/* ......#. .. */
	0x04, 0x00,	/* .....#.. .. */
	0x0E, 0x00,	/* ....###. .. */
	0x1B, 0x00,	/* ...##.## .. */
	0x35, 0x00,	/* ..##.#.# .. */
	0x6A, 0x00,	/* .##.#.#. .. */
	0x34, 0x00,	/* ..##.#.. .. */
	0x08, 0x00,	/* ....#... .. */
	0x08, 0x00,	/* ....#... .. */
	0x00, 0x00,
};

/* Money bag: bag with $ sign */
static const unsigned char bmp_money[] = {
	0x1C, 0x00,	/* ...###.. .. */
	0x08, 0x00,	/* ....#... .. */
	0x3E, 0x00,	/* ..#####. .. */
	0x7F, 0x00,	/* .####### .. */
	0x7F, 0x00,	/* .####### .. */
	0x7F, 0x00,	/* .####### .. */
	0x7F, 0x00,	/* .####### .. */
	0x3E, 0x00,	/* ..#####. .. */
	0x1C, 0x00,	/* ...###.. .. */
	0x00, 0x00,
};

/* Stopwatch: circle with button on top */
static const unsigned char bmp_stopwatch[] = {
	0x08, 0x00,	/* ....#... .. */
	0x1C, 0x00,	/* ...###.. .. */
	0x3E, 0x00,	/* ..#####. .. */
	0x63, 0x00,	/* .##...## .. */
	0x4B, 0x00,	/* .#..#.## .. */
	0x4B, 0x00,	/* .#..#.## .. */
	0x41, 0x00,	/* .#.....# .. */
	0x63, 0x00,	/* .##...## .. */
	0x3E, 0x00,	/* ..#####. .. */
	0x00, 0x00,
};

/* Bitmap table: indexed by (glyph_id - GLYPH_EMOJI_BASE) */
static const GlyphBitmap emoji_bitmaps[] = {
	{ 10, 10, 2, bmp_grin },	/* 0x80 */
	{ 10, 10, 2, bmp_heart },	/* 0x81 */
	{ 10, 10, 2, bmp_thumbsup },	/* 0x82 */
	{ 10, 10, 2, bmp_fire },	/* 0x83 */
	{ 10, 10, 2, bmp_star },	/* 0x84 */
	{ 10, 10, 2, bmp_check },	/* 0x85 */
	{ 10, 10, 2, bmp_crossmark },	/* 0x86 */
	{ 10, 10, 2, bmp_rocket },	/* 0x87 */
	{ 10, 10, 2, bmp_folder },	/* 0x88 */
	{ 10, 10, 2, bmp_bulb },	/* 0x89 */
	{ 10, 10, 2, bmp_globe },	/* 0x8A */
	{ 10, 10, 2, bmp_wrench },	/* 0x8B */
	{ 10, 10, 2, bmp_package },	/* 0x8C */
	{ 10, 10, 2, bmp_snake },	/* 0x8D */
	{ 10, 10, 2, bmp_crab },	/* 0x8E */
	{ 10, 10, 2, bmp_herb },	/* 0xFC */
	{ 10, 10, 2, bmp_money },	/* 0xFD */
	{ 10, 10, 2, bmp_stopwatch },	/* 0xFE */
};

/* ----------------------------------------------------------------
 * Lookup functions
 * ---------------------------------------------------------------- */

/*
 * glyph_lookup - look up Unicode codepoint in optimized glyph tables
 *
 * Returns glyph index (0x00-0xFE) or -1 if not found.
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

		/* Direct lookup: double-line box drawing U+2550-U+256C */
		if (cp >= 0x2550 && cp <= 0x256C) {
			g = dbox_drawing_glyphs[cp - 0x2550];
			if (g != GLYPH_NONE)
				return (short)g;
			return -1;
		}

		/* Direct lookup: block elements U+2580-U+2595 */
		if (cp >= 0x2580 && cp <= 0x2595) {
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

	/* Legacy Computing: sextant characters U+1FB00-U+1FB38 */
	if (cp >= 0x1FB00L && cp <= 0x1FB38L) {
		return GLYPH_SEXTANT_BASE + (short)(cp - 0x1FB00L);
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
	static const GlyphInfo sextant_info =
	    { GLYPH_CAT_PRIMITIVE, 0, '#', 0 };

	if (glyph_id < GLYPH_PRIM_COUNT)
		return &prim_info[glyph_id];
	if (glyph_id >= GLYPH_SEXTANT_BASE && glyph_id < GLYPH_EMOJI_BASE)
		return &sextant_info;
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
