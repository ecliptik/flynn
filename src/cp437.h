/*
 * cp437.h - Code Page 437 character mapping for ANSI-BBS mode
 *
 * Maps each CP437 byte (0x00-0xFF) to its rendering method:
 * ASCII, Mac Roman, glyph primitive, or space.
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

#ifndef CP437_H
#define CP437_H

/* CP437 cell rendering method */
#define CP437_ASCII	0	/* render as ch directly (0x20-0x7E) */
#define CP437_MACROMAN	1	/* render as Mac Roman byte in value field */
#define CP437_GLYPH	2	/* render as glyph index in value field */
#define CP437_SPACE	3	/* render as space */

typedef struct {
	unsigned char	method;		/* CP437_ASCII/MACROMAN/GLYPH/SPACE */
	unsigned char	value;		/* Mac Roman byte, glyph ID, or ASCII char */
} CP437Entry;

extern const CP437Entry cp437_table[256];

#endif /* CP437_H */
