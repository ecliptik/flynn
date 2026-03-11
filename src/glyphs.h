/*
 * glyphs.h - Unicode glyph rendering for classic Macintosh
 *
 * Provides custom rendering for Unicode symbols and emoji that have
 * no Mac Roman equivalent.  Two rendering methods:
 * - Primitive symbols: drawn with QuickDraw (LineTo, PaintOval, etc.)
 * - Bitmap emoji: 10x10 monochrome bitmaps rendered with CopyBits()
 * - Braille patterns: U+2800-U+28FF dot patterns
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

#ifndef GLYPHS_H
#define GLYPHS_H

/* CELL_TYPE_GLYPH and CELL_TYPE_BRAILLE are defined in terminal.h */

/* Glyph categories */
#define GLYPH_CAT_PRIMITIVE	0	/* QuickDraw primitives */
#define GLYPH_CAT_EMOJI		1	/* Bitmap emoji */

/* Glyph flags */
#define GLYPH_WIDE		0x01	/* occupies 2 cells */

/* Wide spacer: second cell of a 2-cell glyph */
#define GLYPH_WIDE_SPACER	0xFF

/* --- Primitive symbol indices (0x00-0x68) --- */
#define GLYPH_ARROW_LEFT	0x00	/* U+2190 */
#define GLYPH_ARROW_UP		0x01	/* U+2191 */
#define GLYPH_ARROW_RIGHT	0x02	/* U+2192 */
#define GLYPH_ARROW_DOWN	0x03	/* U+2193 */
#define GLYPH_CHECK		0x04	/* U+2713 */
#define GLYPH_CROSS		0x05	/* U+2717 */
#define GLYPH_STAR_FILLED	0x06	/* U+2605 */
#define GLYPH_STAR_EMPTY	0x07	/* U+2606 */
#define GLYPH_HEART		0x08	/* U+2665 */
#define GLYPH_CIRCLE_FILLED	0x09	/* U+25CF */
#define GLYPH_CIRCLE_EMPTY	0x0A	/* U+25CB */
#define GLYPH_SQUARE_FILLED	0x0B	/* U+25A0 */
#define GLYPH_SQUARE_EMPTY	0x0C	/* U+25A1 */
#define GLYPH_TRI_UP		0x0D	/* U+25B2 */
#define GLYPH_TRI_RIGHT		0x0E	/* U+25B6 */
#define GLYPH_TRI_DOWN		0x0F	/* U+25BC */
#define GLYPH_TRI_LEFT		0x10	/* U+25C0 */
#define GLYPH_MUSIC_NOTE	0x11	/* U+266A */
#define GLYPH_MUSIC_NOTES	0x12	/* U+266B */
#define GLYPH_SPADE		0x13	/* U+2660 */
#define GLYPH_CLUB		0x14	/* U+2663 */
#define GLYPH_DIAMOND		0x15	/* U+2666 */
#define GLYPH_LOZENGE		0x16	/* U+25CA */
#define GLYPH_ELLIPSIS_V	0x17	/* U+22EE */
#define GLYPH_DASH_EM		0x18	/* fallback for wide dash */
/* Block elements */
#define GLYPH_BLOCK_FULL	0x19	/* U+2588 */
#define GLYPH_BLOCK_UPPER	0x1A	/* U+2580 */
#define GLYPH_BLOCK_LOWER	0x1B	/* U+2584 */
#define GLYPH_BLOCK_LEFT	0x1C	/* U+258C */
#define GLYPH_BLOCK_RIGHT	0x1D	/* U+2590 */
/* Quadrants */
#define GLYPH_QUAD_UL		0x1E	/* U+2598 */
#define GLYPH_QUAD_UR		0x1F	/* U+259D */
#define GLYPH_QUAD_LL		0x20	/* U+2596 */
#define GLYPH_QUAD_LR		0x21	/* U+2597 */
#define GLYPH_QUAD_UL_UR_LL	0x22	/* U+259B */
#define GLYPH_QUAD_UL_UR_LR	0x23	/* U+259C */
/* Asterisk/star spinners */
#define GLYPH_ASTERISK_TEARDROP	0x24	/* U+273B */
#define GLYPH_ASTERISK_FOUR	0x25	/* U+2722 */
#define GLYPH_ASTERISK_HEAVY	0x26	/* U+273D */
#define GLYPH_ASTERISK_OP	0x27	/* U+2217 */
/* Medium squares */
#define GLYPH_SQ_MED_FILLED	0x28	/* U+25FC */
#define GLYPH_SQ_MED_EMPTY	0x29	/* U+25FB */
/* Claude Code UI symbols */
#define GLYPH_PLAY		0x2A	/* U+23F5 */
#define GLYPH_ASTERISK_8	0x2B	/* U+2733 */
#define GLYPH_CHEVRON_RIGHT	0x2C	/* U+276F */
#define GLYPH_WARNING		0x2D	/* U+26A0 */
#define GLYPH_CHECK_HEAVY	0x2E	/* U+2714 */
#define GLYPH_CROSS_HEAVY	0x2F	/* U+2718 */
/* Small squares */
#define GLYPH_SQ_SM_FILLED	0x30	/* U+25AA */
#define GLYPH_SQ_SM_EMPTY	0x31	/* U+25AB */
#define GLYPH_DOT_MIDDLE	0x32	/* U+00B7 */
/* Box drawing - light lines */
#define GLYPH_BOX_H		0x33	/* U+2500 ─ */
#define GLYPH_BOX_V		0x34	/* U+2502 │ */
#define GLYPH_BOX_DR		0x35	/* U+250C ┌ down+right */
#define GLYPH_BOX_DL		0x36	/* U+2510 ┐ down+left */
#define GLYPH_BOX_UR		0x37	/* U+2514 └ up+right */
#define GLYPH_BOX_UL		0x38	/* U+2518 ┘ up+left */
#define GLYPH_BOX_VR		0x39	/* U+251C ├ vert+right tee */
#define GLYPH_BOX_VL		0x3A	/* U+2524 ┤ vert+left tee */
#define GLYPH_BOX_DH		0x3B	/* U+252C ┬ down+horiz tee */
#define GLYPH_BOX_UH		0x3C	/* U+2534 ┴ up+horiz tee */
#define GLYPH_BOX_VH		0x3D	/* U+253C ┼ cross */
/* Shade characters */
#define GLYPH_SHADE_LIGHT	0x3E	/* U+2591 ░ */
#define GLYPH_SHADE_MEDIUM	0x3F	/* U+2592 ▒ */
#define GLYPH_SHADE_DARK	0x40	/* U+2593 ▓ */
/* Outline triangles */
#define GLYPH_TRI_UP_EMPTY	0x41	/* U+25B3 △ */
#define GLYPH_TRI_RIGHT_EMPTY	0x42	/* U+25B7 ▷ */
#define GLYPH_TRI_DOWN_EMPTY	0x43	/* U+25BD ▽ */
#define GLYPH_TRI_LEFT_EMPTY	0x44	/* U+25C1 ◁ */
/* Small filled triangles */
#define GLYPH_TRI_RIGHT_SM	0x45	/* U+25B8 ▸ */
#define GLYPH_TRI_LEFT_SM	0x46	/* U+25C2 ◂ */
/* Diamonds */
#define GLYPH_DIAMOND_FILLED	0x47	/* U+25C6 ◆ */
#define GLYPH_DIAMOND_EMPTY	0x48	/* U+25C7 ◇ */
/* Circle variants */
#define GLYPH_CIRCLE_HALF_L	0x49	/* U+25D0 ◐ left half */
#define GLYPH_CIRCLE_HALF_R	0x4A	/* U+25D1 ◑ right half */
#define GLYPH_CIRCLE_HALF_B	0x4B	/* U+25D2 ◒ bottom half */
#define GLYPH_CIRCLE_HALF_T	0x4C	/* U+25D3 ◓ top half */
#define GLYPH_CIRCLE_DOT	0x4D	/* U+25C9 ◉ fisheye */
/* Six-pointed star */
#define GLYPH_STAR_SIX		0x4E	/* U+2736 ✶ */
/* Circled operators */
#define GLYPH_CIRCLED_DOT	0x4F	/* U+2299 ⊙ */
#define GLYPH_CIRCLED_PLUS	0x50	/* U+2295 ⊕ */
#define GLYPH_CIRCLED_MINUS	0x51	/* U+2296 ⊖ */
#define GLYPH_CIRCLED_TIMES	0x52	/* U+2297 ⊗ */
/* Superscript digits */
#define GLYPH_SUPER_0		0x53	/* U+2070 ⁰ */
#define GLYPH_SUPER_1		0x54	/* U+00B9 ¹ */
#define GLYPH_SUPER_2		0x55	/* U+00B2 ² */
#define GLYPH_SUPER_3		0x56	/* U+00B3 ³ */
#define GLYPH_SUPER_4		0x57	/* U+2074 ⁴ */
#define GLYPH_SUPER_5		0x58	/* U+2075 ⁵ */
#define GLYPH_SUPER_6		0x59	/* U+2076 ⁶ */
#define GLYPH_SUPER_7		0x5A	/* U+2077 ⁷ */
#define GLYPH_SUPER_8		0x5B	/* U+2078 ⁸ */
#define GLYPH_SUPER_9		0x5C	/* U+2079 ⁹ */
/* Subscript digits */
#define GLYPH_SUB_0		0x5D	/* U+2080 ₀ */
#define GLYPH_SUB_1		0x5E	/* U+2081 ₁ */
#define GLYPH_SUB_2		0x5F	/* U+2082 ₂ */
#define GLYPH_SUB_3		0x60	/* U+2083 ₃ */
#define GLYPH_SUB_4		0x61	/* U+2084 ₄ */
#define GLYPH_SUB_5		0x62	/* U+2085 ₅ */
#define GLYPH_SUB_6		0x63	/* U+2086 ₆ */
#define GLYPH_SUB_7		0x64	/* U+2087 ₇ */
#define GLYPH_SUB_8		0x65	/* U+2088 ₈ */
#define GLYPH_SUB_9		0x66	/* U+2089 ₉ */

/* --- CP437 double-line box drawing (0x67-0x83) --- */
#define GLYPH_BOX2_V		0x67	/* ║ double vertical */
#define GLYPH_BOX2_H		0x68	/* ═ double horizontal */
#define GLYPH_BOX2_DR		0x69	/* ╔ double down+right */
#define GLYPH_BOX2_DL		0x6A	/* ╗ double down+left */
#define GLYPH_BOX2_UR		0x6B	/* ╚ double up+right */
#define GLYPH_BOX2_UL		0x6C	/* ╝ double up+left */
#define GLYPH_BOX2_VR		0x6D	/* ╠ double vert+right */
#define GLYPH_BOX2_VL		0x6E	/* ╣ double vert+left */
#define GLYPH_BOX2_DH		0x6F	/* ╦ double down+horiz */
#define GLYPH_BOX2_UH		0x70	/* ╩ double up+horiz */
#define GLYPH_BOX2_VH		0x71	/* ╬ double cross */
/* Mixed single/double junctions */
#define GLYPH_BOX_sVdL		0x72	/* ╡ single vert, double left */
#define GLYPH_BOX_dVsL		0x73	/* ╢ double vert, single left */
#define GLYPH_BOX_dDsL		0x74	/* ╖ double down, single left */
#define GLYPH_BOX_sDdL		0x75	/* ╕ single down, double left */
#define GLYPH_BOX_dUsL		0x76	/* ╜ double up, single left */
#define GLYPH_BOX_sUdL		0x77	/* ╛ single up, double left */
#define GLYPH_BOX_sVdR		0x78	/* ╞ single vert, double right */
#define GLYPH_BOX_dVsR		0x79	/* ╟ double vert, single right */
#define GLYPH_BOX_dHsU		0x7A	/* ╧ double horiz, single up */
#define GLYPH_BOX_sHdU		0x7B	/* ╨ single horiz, double up */
#define GLYPH_BOX_dHsD		0x7C	/* ╤ double horiz, single down */
#define GLYPH_BOX_sHdD		0x7D	/* ╥ single horiz, double down */
#define GLYPH_BOX_dUsR		0x7E	/* ╙ double up, single right */
#define GLYPH_BOX_sUdR		0x7F	/* ╘ single up, double right */
#define GLYPH_BOX_sDdR		0x80	/* ╒ single down, double right */
#define GLYPH_BOX_dDsR		0x81	/* ╓ double down, single right */
#define GLYPH_BOX_dVsVH	0x82	/* ╫ double vert, single horiz cross */
#define GLYPH_BOX_sVdVH	0x83	/* ╪ single vert, double horiz cross */

/* --- CP437 symbol glyphs (0x84-0x90) --- */
#define GLYPH_SMILEY		0x84	/* ☺ white smiley */
#define GLYPH_SMILEY_INV	0x85	/* ☻ black smiley */
#define GLYPH_INV_BULLET	0x86	/* ◘ inverse bullet */
#define GLYPH_INV_CIRCLE	0x87	/* ◙ inverse circle */
#define GLYPH_MALE		0x88	/* ♂ male sign */
#define GLYPH_FEMALE		0x89	/* ♀ female sign */
#define GLYPH_SUN		0x8A	/* ☼ sun */
#define GLYPH_ARROW_UPDOWN	0x8B	/* ↕ up-down arrow */
#define GLYPH_BAR_H		0x8C	/* ▬ horizontal bar */
#define GLYPH_ARROW_UPDOWN_BASE	0x8D	/* ↨ up-down arrow w/ base */
#define GLYPH_RIGHT_ANGLE	0x8E	/* ∟ right angle */
#define GLYPH_ARROW_LEFTRIGHT	0x8F	/* ↔ left-right arrow */
#define GLYPH_HOUSE		0x90	/* ⌂ house */

/* --- CP437 math/Greek glyphs (0x91-0x9F) --- */
#define GLYPH_REVERSED_NOT	0x91	/* ⌐ reversed not sign */
#define GLYPH_HALF		0x92	/* ½ one-half */
#define GLYPH_QUARTER		0x93	/* ¼ one-quarter */
#define GLYPH_GAMMA		0x94	/* Γ Greek capital gamma */
#define GLYPH_PHI_UC		0x95	/* Φ Greek capital phi */
#define GLYPH_THETA		0x96	/* Θ Greek capital theta */
#define GLYPH_DELTA_LC		0x97	/* δ Greek lowercase delta */
#define GLYPH_INFINITY		0x98	/* ∞ infinity */
#define GLYPH_INTERSECT		0x99	/* ∩ intersection */
#define GLYPH_IDENTICAL		0x9A	/* ≡ identical (triple bar) */
#define GLYPH_INTEGRAL_T	0x9B	/* ⌠ top integral */
#define GLYPH_INTEGRAL_B	0x9C	/* ⌡ bottom integral */
#define GLYPH_APPROX		0x9D	/* ≈ approximately equal */
#define GLYPH_SQRT		0x9E	/* √ square root */
#define GLYPH_SUPER_N		0x9F	/* ⁿ superscript n */

/* --- Fractional block elements (0xA0-0xAD) --- */
/* Lower fractional blocks */
#define GLYPH_BLOCK_LOWER_1	0xA0	/* U+2581 lower 1/8 */
#define GLYPH_BLOCK_LOWER_2	0xA1	/* U+2582 lower 1/4 */
#define GLYPH_BLOCK_LOWER_3	0xA2	/* U+2583 lower 3/8 */
#define GLYPH_BLOCK_LOWER_5	0xA3	/* U+2585 lower 5/8 */
#define GLYPH_BLOCK_LOWER_6	0xA4	/* U+2586 lower 3/4 */
#define GLYPH_BLOCK_LOWER_7	0xA5	/* U+2587 lower 7/8 */
/* Left fractional blocks */
#define GLYPH_BLOCK_LEFT_7	0xA6	/* U+2589 left 7/8 */
#define GLYPH_BLOCK_LEFT_6	0xA7	/* U+258A left 3/4 */
#define GLYPH_BLOCK_LEFT_5	0xA8	/* U+258B left 5/8 */
#define GLYPH_BLOCK_LEFT_3	0xA9	/* U+258D left 3/8 */
#define GLYPH_BLOCK_LEFT_2	0xAA	/* U+258E left 1/4 */
#define GLYPH_BLOCK_LEFT_1	0xAB	/* U+258F left 1/8 */
/* Edge blocks */
#define GLYPH_BLOCK_UPPER_1	0xAC	/* U+2594 upper 1/8 */
#define GLYPH_BLOCK_RIGHT_1	0xAD	/* U+2595 right 1/8 */
/* Missing quadrants */
#define GLYPH_QUAD_UL_LL_LR	0xAE	/* U+2599 */
#define GLYPH_QUAD_UL_LR	0xAF	/* U+259A diagonal */
#define GLYPH_QUAD_UR_LL	0xB0	/* U+259E diagonal */
#define GLYPH_QUAD_UR_LL_LR	0xB1	/* U+259F */

/* Flower and snowflake dingbats */
#define GLYPH_FLOWER		0xB2	/* U+273F ✿ flower/florette */
#define GLYPH_SNOWFLAKE		0xB3	/* U+2744 ❄ snowflake */

#define GLYPH_PRIM_COUNT	180

/* --- Sextant characters (U+1FB00-U+1FB3B): 2x3 grid patterns --- */
#define GLYPH_SEXTANT_BASE	0xB4
/* 60 packed indices (0-59), unpack to 6-bit patterns at render time */
#define GLYPH_SEXTANT_COUNT	60

/* --- Bitmap emoji indices (0xF0-0xFE) --- */
#define GLYPH_EMOJI_BASE	0xF0
#define GLYPH_EMOJI_GRIN	0xF0	/* U+1F600 */
#define GLYPH_EMOJI_HEART	0xF1	/* U+2764 */
#define GLYPH_EMOJI_THUMBSUP	0xF2	/* U+1F44D */
#define GLYPH_EMOJI_FIRE	0xF3	/* U+1F525 */
#define GLYPH_EMOJI_STAR	0xF4	/* U+2B50 */
#define GLYPH_EMOJI_CHECK	0xF5	/* U+2705 */
#define GLYPH_EMOJI_CROSSMARK	0xF6	/* U+274C */
#define GLYPH_EMOJI_ROCKET	0xF7	/* U+1F680 */
#define GLYPH_EMOJI_FOLDER	0xF8	/* U+1F4C1 */
#define GLYPH_EMOJI_BULB	0xF9	/* U+1F4A1 */
#define GLYPH_EMOJI_GLOBE	0xFA	/* U+1F310 */
#define GLYPH_EMOJI_WRENCH	0xFB	/* U+1F527 */
#define GLYPH_EMOJI_PACKAGE	0xFC	/* U+1F4E6 */
#define GLYPH_EMOJI_SNAKE	0xFD	/* U+1F40D */
#define GLYPH_EMOJI_CRAB	0xFE	/* U+1F980 */
#define GLYPH_EMOJI_COUNT	15

/* Glyph info: describes how to render a glyph */
typedef struct {
	unsigned char	category;	/* GLYPH_CAT_PRIMITIVE or _EMOJI */
	unsigned char	flags;		/* GLYPH_WIDE etc. */
	unsigned char	copy_char;	/* character for clipboard copy */
	unsigned char	reserved;
} GlyphInfo;

/* Bitmap descriptor for emoji glyphs */
typedef struct {
	short			width;		/* pixels wide */
	short			height;		/* pixels tall */
	short			rowBytes;	/* bytes per row (word-aligned) */
	const unsigned char	*bits;		/* pixel data */
} GlyphBitmap;

/*
 * glyph_lookup - look up a Unicode codepoint in the glyph table
 *
 * Returns the glyph index (0x00-0xFE) or -1 if not found.
 * Braille (U+2800-U+28FF) is handled separately in terminal.c.
 */
short glyph_lookup(long cp);

/*
 * glyph_get_info - get rendering info for a glyph index
 */
const GlyphInfo *glyph_get_info(unsigned char glyph_id);

/*
 * glyph_is_wide - check if a glyph occupies 2 cells
 */
short glyph_is_wide(unsigned char glyph_id);

/*
 * glyph_get_bitmap - get bitmap data for an emoji glyph
 *
 * Returns NULL if glyph_id is not a bitmap emoji.
 */
const GlyphBitmap *glyph_get_bitmap(unsigned char glyph_id);

#endif /* GLYPHS_H */
