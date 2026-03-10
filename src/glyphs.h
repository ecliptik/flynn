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

/* ATTR_GLYPH and ATTR_BRAILLE are defined in terminal.h */

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
#define GLYPH_PRIM_COUNT	103

/* --- Bitmap emoji indices (0x80-0x9F) --- */
#define GLYPH_EMOJI_BASE	0x80
#define GLYPH_EMOJI_GRIN	0x80	/* U+1F600 */
#define GLYPH_EMOJI_HEART	0x81	/* U+2764 */
#define GLYPH_EMOJI_THUMBSUP	0x82	/* U+1F44D */
#define GLYPH_EMOJI_FIRE	0x83	/* U+1F525 */
#define GLYPH_EMOJI_STAR	0x84	/* U+2B50 */
#define GLYPH_EMOJI_CHECK	0x85	/* U+2705 */
#define GLYPH_EMOJI_CROSSMARK	0x86	/* U+274C */
#define GLYPH_EMOJI_ROCKET	0x87	/* U+1F680 */
#define GLYPH_EMOJI_FOLDER	0x88	/* U+1F4C1 */
#define GLYPH_EMOJI_BULB	0x89	/* U+1F4A1 */
#define GLYPH_EMOJI_GLOBE	0x8A	/* U+1F310 */
#define GLYPH_EMOJI_WRENCH	0x8B	/* U+1F527 */
#define GLYPH_EMOJI_PACKAGE	0x8C	/* U+1F4E6 */
#define GLYPH_EMOJI_SNAKE	0x8D	/* U+1F40D */
#define GLYPH_EMOJI_CRAB	0x8E	/* U+1F980 */
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
 * Returns the glyph index (0x00-0x9F) or -1 if not found.
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
