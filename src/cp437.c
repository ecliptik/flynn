/*
 * cp437.c - Code Page 437 character mapping table
 *
 * Maps each CP437 byte (0x00-0xFF) to its rendering method and value.
 * 512 bytes const data (code segment, not heap).
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

#include "cp437.h"
#include "glyphs.h"

/*
 * CP437 to rendering method lookup table.
 *
 * Control characters 0x07 (BEL), 0x08 (BS), 0x09 (Tab), 0x0A (LF),
 * 0x0D (CR), and 0x1B (ESC) are intercepted by the parser before
 * reaching this table.  Their entries here are the CP437 graphic
 * characters they would display if not intercepted.
 */
const CP437Entry cp437_table[256] = {
	/* 0x00 NUL */  { CP437_SPACE,    0 },
	/* 0x01 ☺ */    { CP437_GLYPH,    GLYPH_SMILEY },
	/* 0x02 ☻ */    { CP437_GLYPH,    GLYPH_SMILEY_INV },
	/* 0x03 ♥ */    { CP437_GLYPH,    GLYPH_HEART },
	/* 0x04 ♦ */    { CP437_GLYPH,    GLYPH_DIAMOND_FILLED },
	/* 0x05 ♣ */    { CP437_GLYPH,    GLYPH_CLUB },
	/* 0x06 ♠ */    { CP437_GLYPH,    GLYPH_SPADE },
	/* 0x07 • */    { CP437_GLYPH,    GLYPH_CIRCLE_FILLED },
	/* 0x08 ◘ */    { CP437_GLYPH,    GLYPH_INV_BULLET },
	/* 0x09 ○ */    { CP437_GLYPH,    GLYPH_CIRCLE_EMPTY },
	/* 0x0A ◙ */    { CP437_GLYPH,    GLYPH_INV_CIRCLE },
	/* 0x0B ♂ */    { CP437_GLYPH,    GLYPH_MALE },
	/* 0x0C ♀ */    { CP437_GLYPH,    GLYPH_FEMALE },
	/* 0x0D ♪ */    { CP437_GLYPH,    GLYPH_MUSIC_NOTE },
	/* 0x0E ♫ */    { CP437_GLYPH,    GLYPH_MUSIC_NOTES },
	/* 0x0F ☼ */    { CP437_GLYPH,    GLYPH_SUN },
	/* 0x10 ► */    { CP437_GLYPH,    GLYPH_TRI_RIGHT },
	/* 0x11 ◄ */    { CP437_GLYPH,    GLYPH_TRI_LEFT },
	/* 0x12 ↕ */    { CP437_GLYPH,    GLYPH_ARROW_UPDOWN },
	/* 0x13 ‼ */    { CP437_ASCII,    '!' },
	/* 0x14 ¶ */    { CP437_MACROMAN, 0xA6 },
	/* 0x15 § */    { CP437_MACROMAN, 0xA4 },
	/* 0x16 ▬ */    { CP437_GLYPH,    GLYPH_BAR_H },
	/* 0x17 ↨ */    { CP437_GLYPH,    GLYPH_ARROW_UPDOWN_BASE },
	/* 0x18 ↑ */    { CP437_GLYPH,    GLYPH_ARROW_UP },
	/* 0x19 ↓ */    { CP437_GLYPH,    GLYPH_ARROW_DOWN },
	/* 0x1A → */    { CP437_GLYPH,    GLYPH_ARROW_RIGHT },
	/* 0x1B ← */    { CP437_GLYPH,    GLYPH_ARROW_LEFT },
	/* 0x1C ∟ */    { CP437_GLYPH,    GLYPH_RIGHT_ANGLE },
	/* 0x1D ↔ */    { CP437_GLYPH,    GLYPH_ARROW_LEFTRIGHT },
	/* 0x1E ▲ */    { CP437_GLYPH,    GLYPH_TRI_UP },
	/* 0x1F ▼ */    { CP437_GLYPH,    GLYPH_TRI_DOWN },

	/* 0x20-0x7E: standard ASCII */
	{ CP437_ASCII, ' ' },   /* 0x20 */
	{ CP437_ASCII, '!' },   /* 0x21 */
	{ CP437_ASCII, '"' },   /* 0x22 */
	{ CP437_ASCII, '#' },   /* 0x23 */
	{ CP437_ASCII, '$' },   /* 0x24 */
	{ CP437_ASCII, '%' },   /* 0x25 */
	{ CP437_ASCII, '&' },   /* 0x26 */
	{ CP437_ASCII, '\'' },  /* 0x27 */
	{ CP437_ASCII, '(' },   /* 0x28 */
	{ CP437_ASCII, ')' },   /* 0x29 */
	{ CP437_ASCII, '*' },   /* 0x2A */
	{ CP437_ASCII, '+' },   /* 0x2B */
	{ CP437_ASCII, ',' },   /* 0x2C */
	{ CP437_ASCII, '-' },   /* 0x2D */
	{ CP437_ASCII, '.' },   /* 0x2E */
	{ CP437_ASCII, '/' },   /* 0x2F */
	{ CP437_ASCII, '0' },   /* 0x30 */
	{ CP437_ASCII, '1' },   /* 0x31 */
	{ CP437_ASCII, '2' },   /* 0x32 */
	{ CP437_ASCII, '3' },   /* 0x33 */
	{ CP437_ASCII, '4' },   /* 0x34 */
	{ CP437_ASCII, '5' },   /* 0x35 */
	{ CP437_ASCII, '6' },   /* 0x36 */
	{ CP437_ASCII, '7' },   /* 0x37 */
	{ CP437_ASCII, '8' },   /* 0x38 */
	{ CP437_ASCII, '9' },   /* 0x39 */
	{ CP437_ASCII, ':' },   /* 0x3A */
	{ CP437_ASCII, ';' },   /* 0x3B */
	{ CP437_ASCII, '<' },   /* 0x3C */
	{ CP437_ASCII, '=' },   /* 0x3D */
	{ CP437_ASCII, '>' },   /* 0x3E */
	{ CP437_ASCII, '?' },   /* 0x3F */
	{ CP437_ASCII, '@' },   /* 0x40 */
	{ CP437_ASCII, 'A' },   /* 0x41 */
	{ CP437_ASCII, 'B' },   /* 0x42 */
	{ CP437_ASCII, 'C' },   /* 0x43 */
	{ CP437_ASCII, 'D' },   /* 0x44 */
	{ CP437_ASCII, 'E' },   /* 0x45 */
	{ CP437_ASCII, 'F' },   /* 0x46 */
	{ CP437_ASCII, 'G' },   /* 0x47 */
	{ CP437_ASCII, 'H' },   /* 0x48 */
	{ CP437_ASCII, 'I' },   /* 0x49 */
	{ CP437_ASCII, 'J' },   /* 0x4A */
	{ CP437_ASCII, 'K' },   /* 0x4B */
	{ CP437_ASCII, 'L' },   /* 0x4C */
	{ CP437_ASCII, 'M' },   /* 0x4D */
	{ CP437_ASCII, 'N' },   /* 0x4E */
	{ CP437_ASCII, 'O' },   /* 0x4F */
	{ CP437_ASCII, 'P' },   /* 0x50 */
	{ CP437_ASCII, 'Q' },   /* 0x51 */
	{ CP437_ASCII, 'R' },   /* 0x52 */
	{ CP437_ASCII, 'S' },   /* 0x53 */
	{ CP437_ASCII, 'T' },   /* 0x54 */
	{ CP437_ASCII, 'U' },   /* 0x55 */
	{ CP437_ASCII, 'V' },   /* 0x56 */
	{ CP437_ASCII, 'W' },   /* 0x57 */
	{ CP437_ASCII, 'X' },   /* 0x58 */
	{ CP437_ASCII, 'Y' },   /* 0x59 */
	{ CP437_ASCII, 'Z' },   /* 0x5A */
	{ CP437_ASCII, '[' },   /* 0x5B */
	{ CP437_ASCII, '\\' },  /* 0x5C */
	{ CP437_ASCII, ']' },   /* 0x5D */
	{ CP437_ASCII, '^' },   /* 0x5E */
	{ CP437_ASCII, '_' },   /* 0x5F */
	{ CP437_ASCII, '`' },   /* 0x60 */
	{ CP437_ASCII, 'a' },   /* 0x61 */
	{ CP437_ASCII, 'b' },   /* 0x62 */
	{ CP437_ASCII, 'c' },   /* 0x63 */
	{ CP437_ASCII, 'd' },   /* 0x64 */
	{ CP437_ASCII, 'e' },   /* 0x65 */
	{ CP437_ASCII, 'f' },   /* 0x66 */
	{ CP437_ASCII, 'g' },   /* 0x67 */
	{ CP437_ASCII, 'h' },   /* 0x68 */
	{ CP437_ASCII, 'i' },   /* 0x69 */
	{ CP437_ASCII, 'j' },   /* 0x6A */
	{ CP437_ASCII, 'k' },   /* 0x6B */
	{ CP437_ASCII, 'l' },   /* 0x6C */
	{ CP437_ASCII, 'm' },   /* 0x6D */
	{ CP437_ASCII, 'n' },   /* 0x6E */
	{ CP437_ASCII, 'o' },   /* 0x6F */
	{ CP437_ASCII, 'p' },   /* 0x70 */
	{ CP437_ASCII, 'q' },   /* 0x71 */
	{ CP437_ASCII, 'r' },   /* 0x72 */
	{ CP437_ASCII, 's' },   /* 0x73 */
	{ CP437_ASCII, 't' },   /* 0x74 */
	{ CP437_ASCII, 'u' },   /* 0x75 */
	{ CP437_ASCII, 'v' },   /* 0x76 */
	{ CP437_ASCII, 'w' },   /* 0x77 */
	{ CP437_ASCII, 'x' },   /* 0x78 */
	{ CP437_ASCII, 'y' },   /* 0x79 */
	{ CP437_ASCII, 'z' },   /* 0x7A */
	{ CP437_ASCII, '{' },   /* 0x7B */
	{ CP437_ASCII, '|' },   /* 0x7C */
	{ CP437_ASCII, '}' },   /* 0x7D */
	{ CP437_ASCII, '~' },   /* 0x7E */
	/* 0x7F ⌂ house */
	{ CP437_GLYPH, GLYPH_HOUSE },

	/* 0x80-0x9F: international letters (Mac Roman mapping) */
	/* 0x80 Ç */    { CP437_MACROMAN, 0x82 },
	/* 0x81 ü */    { CP437_MACROMAN, 0x9F },
	/* 0x82 é */    { CP437_MACROMAN, 0x8E },
	/* 0x83 â */    { CP437_MACROMAN, 0x89 },
	/* 0x84 ä */    { CP437_MACROMAN, 0x8A },
	/* 0x85 à */    { CP437_MACROMAN, 0x88 },
	/* 0x86 å */    { CP437_MACROMAN, 0x8C },
	/* 0x87 ç */    { CP437_MACROMAN, 0x8D },
	/* 0x88 ê */    { CP437_MACROMAN, 0x90 },
	/* 0x89 ë */    { CP437_MACROMAN, 0x91 },
	/* 0x8A è */    { CP437_MACROMAN, 0x8F },
	/* 0x8B ï */    { CP437_MACROMAN, 0x95 },
	/* 0x8C î */    { CP437_MACROMAN, 0x94 },
	/* 0x8D ì */    { CP437_MACROMAN, 0x93 },
	/* 0x8E Ä */    { CP437_MACROMAN, 0x80 },
	/* 0x8F Å */    { CP437_MACROMAN, 0x81 },
	/* 0x90 É */    { CP437_MACROMAN, 0x83 },
	/* 0x91 æ */    { CP437_MACROMAN, 0xBE },
	/* 0x92 Æ */    { CP437_MACROMAN, 0xAE },
	/* 0x93 ô */    { CP437_MACROMAN, 0x99 },
	/* 0x94 ö */    { CP437_MACROMAN, 0x9A },
	/* 0x95 ò */    { CP437_MACROMAN, 0x98 },
	/* 0x96 û */    { CP437_MACROMAN, 0x9E },
	/* 0x97 ù */    { CP437_MACROMAN, 0x9D },
	/* 0x98 ÿ */    { CP437_MACROMAN, 0xD8 },
	/* 0x99 Ö */    { CP437_MACROMAN, 0x85 },
	/* 0x9A Ü */    { CP437_MACROMAN, 0x86 },
	/* 0x9B ¢ */    { CP437_MACROMAN, 0xA2 },
	/* 0x9C £ */    { CP437_MACROMAN, 0xA3 },
	/* 0x9D ¥ */    { CP437_MACROMAN, 0xB4 },
	/* 0x9E ₧ */    { CP437_ASCII,    'P' },
	/* 0x9F ƒ */    { CP437_MACROMAN, 0xC4 },

	/* 0xA0-0xAF: more international + symbols */
	/* 0xA0 á */    { CP437_MACROMAN, 0x87 },
	/* 0xA1 í */    { CP437_MACROMAN, 0x92 },
	/* 0xA2 ó */    { CP437_MACROMAN, 0x97 },
	/* 0xA3 ú */    { CP437_MACROMAN, 0x9C },
	/* 0xA4 ñ */    { CP437_MACROMAN, 0x96 },
	/* 0xA5 Ñ */    { CP437_MACROMAN, 0x84 },
	/* 0xA6 ª */    { CP437_MACROMAN, 0xBB },
	/* 0xA7 º */    { CP437_MACROMAN, 0xBC },
	/* 0xA8 ¿ */    { CP437_MACROMAN, 0xC0 },
	/* 0xA9 ⌐ */    { CP437_GLYPH,    GLYPH_REVERSED_NOT },
	/* 0xAA ¬ */    { CP437_MACROMAN, 0xC2 },
	/* 0xAB ½ */    { CP437_GLYPH,    GLYPH_HALF },
	/* 0xAC ¼ */    { CP437_GLYPH,    GLYPH_QUARTER },
	/* 0xAD ¡ */    { CP437_MACROMAN, 0xC1 },
	/* 0xAE « */    { CP437_MACROMAN, 0xC7 },
	/* 0xAF » */    { CP437_MACROMAN, 0xC8 },

	/* 0xB0-0xBF: shades, single box drawing, mixed box junctions */
	/* 0xB0 ░ */    { CP437_GLYPH,    GLYPH_SHADE_LIGHT },
	/* 0xB1 ▒ */    { CP437_GLYPH,    GLYPH_SHADE_MEDIUM },
	/* 0xB2 ▓ */    { CP437_GLYPH,    GLYPH_SHADE_DARK },
	/* 0xB3 │ */    { CP437_GLYPH,    GLYPH_BOX_V },
	/* 0xB4 ┤ */    { CP437_GLYPH,    GLYPH_BOX_VL },
	/* 0xB5 ╡ */    { CP437_GLYPH,    GLYPH_BOX_sVdL },
	/* 0xB6 ╢ */    { CP437_GLYPH,    GLYPH_BOX_dVsL },
	/* 0xB7 ╖ */    { CP437_GLYPH,    GLYPH_BOX_dDsL },
	/* 0xB8 ╕ */    { CP437_GLYPH,    GLYPH_BOX_sDdL },
	/* 0xB9 ╣ */    { CP437_GLYPH,    GLYPH_BOX2_VL },
	/* 0xBA ║ */    { CP437_GLYPH,    GLYPH_BOX2_V },
	/* 0xBB ╗ */    { CP437_GLYPH,    GLYPH_BOX2_DL },
	/* 0xBC ╝ */    { CP437_GLYPH,    GLYPH_BOX2_UL },
	/* 0xBD ╜ */    { CP437_GLYPH,    GLYPH_BOX_dUsL },
	/* 0xBE ╛ */    { CP437_GLYPH,    GLYPH_BOX_sUdL },
	/* 0xBF ┐ */    { CP437_GLYPH,    GLYPH_BOX_DL },

	/* 0xC0-0xCF: box drawing continued */
	/* 0xC0 └ */    { CP437_GLYPH,    GLYPH_BOX_UR },
	/* 0xC1 ┴ */    { CP437_GLYPH,    GLYPH_BOX_UH },
	/* 0xC2 ┬ */    { CP437_GLYPH,    GLYPH_BOX_DH },
	/* 0xC3 ├ */    { CP437_GLYPH,    GLYPH_BOX_VR },
	/* 0xC4 ─ */    { CP437_GLYPH,    GLYPH_BOX_H },
	/* 0xC5 ┼ */    { CP437_GLYPH,    GLYPH_BOX_VH },
	/* 0xC6 ╞ */    { CP437_GLYPH,    GLYPH_BOX_sVdR },
	/* 0xC7 ╟ */    { CP437_GLYPH,    GLYPH_BOX_dVsR },
	/* 0xC8 ╚ */    { CP437_GLYPH,    GLYPH_BOX2_UR },
	/* 0xC9 ╔ */    { CP437_GLYPH,    GLYPH_BOX2_DR },
	/* 0xCA ╩ */    { CP437_GLYPH,    GLYPH_BOX2_UH },
	/* 0xCB ╦ */    { CP437_GLYPH,    GLYPH_BOX2_DH },
	/* 0xCC ╠ */    { CP437_GLYPH,    GLYPH_BOX2_VR },
	/* 0xCD ═ */    { CP437_GLYPH,    GLYPH_BOX2_H },
	/* 0xCE ╬ */    { CP437_GLYPH,    GLYPH_BOX2_VH },
	/* 0xCF ╧ */    { CP437_GLYPH,    GLYPH_BOX_dHsU },

	/* 0xD0-0xDF: more box drawing + blocks */
	/* 0xD0 ╨ */    { CP437_GLYPH,    GLYPH_BOX_sHdU },
	/* 0xD1 ╤ */    { CP437_GLYPH,    GLYPH_BOX_dHsD },
	/* 0xD2 ╥ */    { CP437_GLYPH,    GLYPH_BOX_sHdD },
	/* 0xD3 ╙ */    { CP437_GLYPH,    GLYPH_BOX_dUsR },
	/* 0xD4 ╘ */    { CP437_GLYPH,    GLYPH_BOX_sUdR },
	/* 0xD5 ╒ */    { CP437_GLYPH,    GLYPH_BOX_sDdR },
	/* 0xD6 ╓ */    { CP437_GLYPH,    GLYPH_BOX_dDsR },
	/* 0xD7 ╫ */    { CP437_GLYPH,    GLYPH_BOX_dVsVH },
	/* 0xD8 ╪ */    { CP437_GLYPH,    GLYPH_BOX_sVdVH },
	/* 0xD9 ┘ */    { CP437_GLYPH,    GLYPH_BOX_UL },
	/* 0xDA ┌ */    { CP437_GLYPH,    GLYPH_BOX_DR },
	/* 0xDB █ */    { CP437_GLYPH,    GLYPH_BLOCK_FULL },
	/* 0xDC ▄ */    { CP437_GLYPH,    GLYPH_BLOCK_LOWER },
	/* 0xDD ▌ */    { CP437_GLYPH,    GLYPH_BLOCK_LEFT },
	/* 0xDE ▐ */    { CP437_GLYPH,    GLYPH_BLOCK_RIGHT },
	/* 0xDF ▀ */    { CP437_GLYPH,    GLYPH_BLOCK_UPPER },

	/* 0xE0-0xEF: Greek/math symbols */
	/* 0xE0 α */    { CP437_ASCII,    'a' },
	/* 0xE1 ß */    { CP437_MACROMAN, 0xA7 },
	/* 0xE2 Γ */    { CP437_GLYPH,    GLYPH_GAMMA },
	/* 0xE3 π */    { CP437_MACROMAN, 0xB9 },
	/* 0xE4 Σ */    { CP437_MACROMAN, 0xB7 },
	/* 0xE5 σ */    { CP437_ASCII,    'o' },
	/* 0xE6 µ */    { CP437_MACROMAN, 0xB5 },
	/* 0xE7 τ */    { CP437_ASCII,    't' },
	/* 0xE8 Φ */    { CP437_GLYPH,    GLYPH_PHI_UC },
	/* 0xE9 Θ */    { CP437_GLYPH,    GLYPH_THETA },
	/* 0xEA Ω */    { CP437_MACROMAN, 0xBD },
	/* 0xEB δ */    { CP437_GLYPH,    GLYPH_DELTA_LC },
	/* 0xEC ∞ */    { CP437_GLYPH,    GLYPH_INFINITY },
	/* 0xED φ */    { CP437_ASCII,    'o' },
	/* 0xEE ε */    { CP437_ASCII,    'e' },
	/* 0xEF ∩ */    { CP437_GLYPH,    GLYPH_INTERSECT },

	/* 0xF0-0xFF: more math symbols */
	/* 0xF0 ≡ */    { CP437_GLYPH,    GLYPH_IDENTICAL },
	/* 0xF1 ± */    { CP437_MACROMAN, 0xB1 },
	/* 0xF2 ≥ */    { CP437_MACROMAN, 0xB3 },
	/* 0xF3 ≤ */    { CP437_MACROMAN, 0xB2 },
	/* 0xF4 ⌠ */    { CP437_GLYPH,    GLYPH_INTEGRAL_T },
	/* 0xF5 ⌡ */    { CP437_GLYPH,    GLYPH_INTEGRAL_B },
	/* 0xF6 ÷ */    { CP437_MACROMAN, 0xD6 },
	/* 0xF7 ≈ */    { CP437_GLYPH,    GLYPH_APPROX },
	/* 0xF8 ° */    { CP437_MACROMAN, 0xA1 },
	/* 0xF9 · */    { CP437_MACROMAN, 0xE1 },
	/* 0xFA · */    { CP437_MACROMAN, 0xE1 },
	/* 0xFB √ */    { CP437_GLYPH,    GLYPH_SQRT },
	/* 0xFC ⁿ */    { CP437_GLYPH,    GLYPH_SUPER_N },
	/* 0xFD ² */    { CP437_GLYPH,    GLYPH_SUPER_2 },
	/* 0xFE ■ */    { CP437_GLYPH,    GLYPH_SQUARE_FILLED },
	/* 0xFF   */    { CP437_SPACE,    0 },
};
