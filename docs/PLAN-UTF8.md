# Plan: UTF-8 Support for Flynn

## Problem Statement

Modern Linux servers default to UTF-8 locales (`LANG=en_US.UTF-8`). Applications
like tmux, htop, and bash send UTF-8 encoded characters even when `TERM=vt220`.
Flynn currently treats all incoming bytes as single-byte characters, so multi-byte
UTF-8 sequences (2-4 bytes) render as garbage.

**Observed impact**: tmux horizontal borders show `□ĬÀ□ĬÀ` instead of lines,
because the Unicode box-drawing character ─ (U+2500) arrives as 3 bytes
(0xE2 0x94 0x80), each rendered as a separate Mac Roman glyph.

## Design Philosophy: Translate on Input

Classic Mac System 6 uses Mac Roman encoding. There is no Unicode infrastructure
in the OS, fonts, or Toolbox. Rather than expanding the internal character
representation, Flynn will **decode UTF-8 at the input boundary** and translate
each codepoint to the best available single-byte representation:

1. **ASCII** (U+0000-U+007F): pass through unchanged
2. **Box-drawing** (U+2500-U+257F): map to DEC Special Graphics char + ATTR_DEC_GRAPHICS
3. **Latin/symbol** (U+0080-U+FFFF): map to Mac Roman where possible
4. **Unmappable**: render as fallback character (middle dot ·)
5. **Wide characters** (CJK, emoji): render as fallback, consuming 2 cells if needed

This keeps TermCell at 2 bytes (zero memory increase) and requires no changes to
the rendering pipeline — box-drawing goes through the existing draw_line_char()
path, and Mac Roman characters go through DrawText as before.

### Why Not Expand TermCell?

Expanding TermCell from 2 to 4 bytes (uint16_t ch + attr + flags) would add 23KB:
- Screen: 3,840 → 7,680 bytes
- Scrollback: 15,360 → 30,720 bytes
- Alt screen: 3,840 → 7,680 bytes

That's manageable on 4MB, but provides little benefit: the Mac's fonts can only
render Mac Roman glyphs and our custom box-drawing shapes. Storing the original
Unicode codepoint doesn't help rendering — and copy/paste uses the Mac clipboard,
which is Mac Roman anyway. The complexity cost (wider cells throughout terminal.c,
terminal_ui.c, selection code) isn't justified.

## Implementation

### 1. UTF-8 Decoder State Machine (~50 lines, terminal.h + terminal.c)

Add a UTF-8 accumulator to the Terminal struct:

```c
/* UTF-8 decoder state */
unsigned char   utf8_buf[4];    /* accumulator for multi-byte sequence */
unsigned char   utf8_len;       /* bytes accumulated so far */
unsigned char   utf8_expect;    /* total bytes expected (2, 3, or 4) */
```

In `terminal_process()`, before the existing PARSE_NORMAL printable-character
check, intercept bytes >= 0x80:

```c
} else if (ch >= 0xC0 && term->utf8_expect == 0) {
    /* Start of multi-byte UTF-8 sequence */
    if (ch < 0xE0) {
        term->utf8_expect = 2;
        term->utf8_buf[0] = ch;
        term->utf8_len = 1;
    } else if (ch < 0xF0) {
        term->utf8_expect = 3;
        term->utf8_buf[0] = ch;
        term->utf8_len = 1;
    } else if (ch < 0xF8) {
        term->utf8_expect = 4;
        term->utf8_buf[0] = ch;
        term->utf8_len = 1;
    }
    /* else: invalid lead byte, ignore */
} else if (ch >= 0x80 && ch < 0xC0 && term->utf8_expect > 0) {
    /* Continuation byte */
    term->utf8_buf[term->utf8_len++] = ch;
    if (term->utf8_len == term->utf8_expect) {
        /* Complete sequence: decode and translate */
        long cp = utf8_decode(term->utf8_buf, term->utf8_expect);
        term_put_unicode(term, cp);
        term->utf8_expect = 0;
    }
} else if (ch >= 0x80) {
    /* Stray continuation byte or overlong: reset and show fallback */
    term->utf8_expect = 0;
    term_put_char(term, 0xB7);  /* middle dot fallback */
}
```

### 2. UTF-8 Decode Function (~15 lines)

```c
static long
utf8_decode(unsigned char *buf, short len)
{
    long cp;

    switch (len) {
    case 2:
        cp = ((long)(buf[0] & 0x1F) << 6) | (buf[1] & 0x3F);
        break;
    case 3:
        cp = ((long)(buf[0] & 0x0F) << 12) |
             ((long)(buf[1] & 0x3F) << 6) | (buf[2] & 0x3F);
        break;
    case 4:
        cp = ((long)(buf[0] & 0x07) << 18) |
             ((long)(buf[1] & 0x3F) << 12) |
             ((long)(buf[2] & 0x3F) << 6) | (buf[3] & 0x3F);
        break;
    default:
        cp = 0xFFFD;   /* replacement character */
    }
    return cp;
}
```

Note: `long` is 32-bit on 68000, sufficient for all Unicode codepoints.

### 3. Unicode Translation Function (~30 lines)

```c
static void
term_put_unicode(Terminal *term, long cp)
{
    unsigned char ch;

    /* Latin-1 Supplement: direct Mac Roman mapping */
    if (cp >= 0x80 && cp <= 0xFF) {
        ch = unicode_to_macroman((unsigned short)cp);
        if (ch)
            term_put_char(term, ch);
        else
            term_put_char(term, 0xB7);  /* fallback: middle dot */
        return;
    }

    /* Box-drawing: U+2500-U+257F → DEC Special Graphics */
    if (cp >= 0x2500 && cp <= 0x257F) {
        ch = boxdraw_to_dec((unsigned short)cp);
        if (ch) {
            /* Temporarily force DEC graphics charset for this char */
            unsigned char save_g0 = term->g0_charset;
            unsigned char save_gl = term->gl_charset;
            term->g0_charset = '0';
            term->gl_charset = 0;
            term_put_char(term, ch);
            term->g0_charset = save_g0;
            term->gl_charset = save_gl;
        } else {
            term_put_char(term, '-');   /* unmapped box char → dash */
        }
        return;
    }

    /* Common Unicode symbols → Mac Roman */
    if (cp >= 0x2000 && cp <= 0x20FF) {
        ch = unicode_symbol_to_macroman((unsigned short)cp);
        if (ch) {
            term_put_char(term, ch);
            return;
        }
    }

    /* Wide characters (CJK, emoji): two-cell placeholder */
    if (cp >= 0x1100 &&
        ((cp >= 0x2E80 && cp <= 0x9FFF) ||     /* CJK */
         (cp >= 0xF900 && cp <= 0xFAFF) ||      /* CJK compat */
         (cp >= 0xFE30 && cp <= 0xFE6F) ||      /* CJK forms */
         (cp >= 0x1F000))) {                     /* emoji + symbols */
        term_put_char(term, '?');
        term_put_char(term, '?');   /* consume 2 cells */
        return;
    }

    /* Everything else: fallback */
    term_put_char(term, 0xB7);  /* middle dot */
}
```

### 4. Mapping Tables (~200 bytes code)

#### Box-Drawing → DEC Special Graphics

```c
static unsigned char
boxdraw_to_dec(unsigned short cp)
{
    /* Light box-drawing */
    switch (cp) {
    case 0x2500: return 'q';    /* ─ horizontal */
    case 0x2502: return 'x';    /* │ vertical */
    case 0x250C: return 'l';    /* ┌ upper-left */
    case 0x2510: return 'k';    /* ┐ upper-right */
    case 0x2514: return 'm';    /* └ lower-left */
    case 0x2518: return 'j';    /* ┘ lower-right */
    case 0x251C: return 't';    /* ├ left tee */
    case 0x2524: return 'u';    /* ┤ right tee */
    case 0x252C: return 'w';    /* ┬ top tee */
    case 0x2534: return 'v';    /* ┴ bottom tee */
    case 0x253C: return 'n';    /* ┼ crossing */
    /* Heavy variants → same glyphs */
    case 0x2501: return 'q';    /* ━ */
    case 0x2503: return 'x';    /* ┃ */
    case 0x250F: return 'l';    /* ┏ */
    case 0x2513: return 'k';    /* ┓ */
    case 0x2517: return 'm';    /* ┗ */
    case 0x251B: return 'j';    /* ┛ */
    case 0x2523: return 't';    /* ┣ */
    case 0x252B: return 'u';    /* ┫ */
    case 0x2533: return 'w';    /* ┳ */
    case 0x253B: return 'v';    /* ┻ */
    case 0x254B: return 'n';    /* ╋ */
    /* Double-line variants → same glyphs */
    case 0x2550: return 'q';    /* ═ */
    case 0x2551: return 'x';    /* ║ */
    case 0x2554: return 'l';    /* ╔ */
    case 0x2557: return 'k';    /* ╗ */
    case 0x255A: return 'm';    /* ╚ */
    case 0x255D: return 'j';    /* ╝ */
    case 0x2560: return 't';    /* ╠ */
    case 0x2563: return 'u';    /* ╣ */
    case 0x2566: return 'w';    /* ╦ */
    case 0x2569: return 'v';    /* ╩ */
    case 0x256C: return 'n';    /* ╬ */
    /* Block elements */
    case 0x2588: return 'a';    /* █ full block → checkerboard */
    case 0x2592: return 'a';    /* ▒ medium shade → checkerboard */
    case 0x25C6: return '`';    /* ◆ diamond */
    case 0x00B7: return '~';    /* · middle dot */
    case 0x00B0: return 'f';    /* ° degree */
    case 0x00B1: return 'g';    /* ± plus/minus */
    default:     return 0;      /* unmapped */
    }
}
```

#### Unicode → Mac Roman (Latin-1 Supplement)

A 128-entry lookup table for U+0080-U+00FF:

```c
/* Index: codepoint - 0x80. Value: Mac Roman byte, or 0 if unmapped. */
static const unsigned char latin1_to_macroman[128] = {
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,          /* 0x80-0x8F: C1 controls */
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,          /* 0x90-0x9F: C1 controls */
    0xCA,0xC1,0xA2,0xA3, 0xB4,0xB4,0x7C,0xA4,    /* 0xA0-0xA7: NBSP ¡ ¢ £ ¤ ¥ ¦ § */
    0xAC,0xA9,0xBB,0xC7, 0xC2,0,0xA8,0xF8,       /* 0xA8-0xAF: ¨ © ª « ¬ ­ ® ¯ */
    0xA1,0xB1,0,0, 0xAB,0xB5,0xA6,0xE1,          /* 0xB0-0xB7: ° ± ² ³ ´ µ ¶ · */
    0xFC,0,0xBC,0xC8, 0,0,0,0xC0,                 /* 0xB8-0xBF: ¸ ¹ º » ¼ ½ ¾ ¿ */
    0xCB,0xE7,0xE5,0xCC, 0x80,0x81,0xAE,0x82,    /* 0xC0-0xC7: À Á Â Ã Ä Å Æ Ç */
    0xE9,0x83,0xE6,0xE8, 0xED,0xEA,0xEB,0xEC,    /* 0xC8-0xCF: È É Ê Ë Ì Í Î Ï */
    0,0x84,0xF1,0xEE, 0xEF,0xCD,0x85,0,          /* 0xD0-0xD7: Ð Ñ Ò Ó Ô Õ Ö × */
    0xAF,0xF4,0xF2,0xF3, 0x86,0,0,0xA7,          /* 0xD8-0xDF: Ø Ù Ú Û Ü Ý Þ ß */
    0x88,0x87,0x89,0x8B, 0x8A,0x8C,0xBE,0x8D,    /* 0xE0-0xE7: à á â ã ä å æ ç */
    0x8F,0x8E,0x90,0x91, 0x93,0x92,0x94,0x95,    /* 0xE8-0xEF: è é ê ë ì í î ï */
    0,0x96,0x98,0x97, 0x99,0x9B,0x9A,0xD6,       /* 0xF0-0xF7: ð ñ ò ó ô õ ö ÷ */
    0xBF,0x9D,0x9C,0x9E, 0x9F,0,0,0xD8,          /* 0xF8-0xFF: ø ù ú û ü ý þ ÿ */
};
```

#### Common Unicode Symbols → Mac Roman

Small switch for frequently-encountered symbols:

```c
static unsigned char
unicode_symbol_to_macroman(unsigned short cp)
{
    switch (cp) {
    case 0x2013: return 0xD0;   /* – en dash */
    case 0x2014: return 0xD1;   /* — em dash */
    case 0x2018: return 0xD4;   /* ' left single quote */
    case 0x2019: return 0xD5;   /* ' right single quote */
    case 0x201C: return 0xD2;   /* " left double quote */
    case 0x201D: return 0xD3;   /* " right double quote */
    case 0x2022: return 0xA5;   /* • bullet */
    case 0x2026: return 0xC9;   /* … ellipsis */
    case 0x2122: return 0xAA;   /* ™ trademark */
    case 0x20AC: return 0xDB;   /* € euro sign */
    case 0x2260: return 0xAD;   /* ≠ not equal */
    case 0x2264: return 0xB2;   /* ≤ less or equal */
    case 0x2265: return 0xB3;   /* ≥ greater or equal */
    case 0x03C0: return 0xB9;   /* π pi */
    case 0x2206: return 0xC6;   /* ∆ delta */
    case 0x221A: return 0xC3;   /* √ square root */
    case 0x221E: return 0xB0;   /* ∞ infinity */
    case 0x2248: return 0xC5;   /* ≈ almost equal */
    default:     return 0;
    }
}
```

### 5. Interaction with Existing Charset State

The UTF-8 decoder and the DEC charset designation (ESC(0) are independent:
- **ESC(0 / SI/SO**: Server explicitly selects DEC Special Graphics for the
  active charset. Characters 0x60-0x7E get ATTR_DEC_GRAPHICS. Works as before.
- **UTF-8**: Server sends multi-byte sequences. The UTF-8 decoder activates
  when the first byte is >= 0xC0. Box-drawing codepoints get translated to
  DEC Special Graphics chars with the ATTR_DEC_GRAPHICS flag.
- **No conflict**: A server either uses DEC ACS (single-byte with charset
  switching) or UTF-8 (multi-byte). It doesn't mix them for the same character.

## Emoji: Realistic Assessment

Emoji (U+1F600+) are 4-byte UTF-8 sequences. On a monochrome 512x342 Mac Plus
with 6x12 pixel cells, meaningful emoji rendering is not feasible:

- No Mac font contains emoji glyphs
- Each emoji would need a custom 6x12 (or 12x12) bitmap — hundreds of bitmaps
- Many emoji have skin tone modifiers and ZWJ sequences (multiple codepoints
  for one visual glyph), requiring a complex state machine
- The visual result on monochrome would be unrecognizable at 6x12 pixels

**Recommendation**: Render emoji as a 2-cell placeholder `??` or `[]`. This is
the standard behavior for terminals that don't support the glyph — the character
is present in the data stream but displayed as a replacement.

If there is future demand, a small set of common emoji (~20) could be rendered
as custom QuickDraw bitmaps in 12x12 cells (2 cells wide), but this would be a
significant art/engineering effort for minimal practical value.

### Emoji ZWJ and Modifier Handling

Emoji sequences can span many codepoints:
- Base emoji: U+1F600 (4 bytes UTF-8)
- Skin tone modifier: U+1F3FB-1F3FF (4 bytes each)
- ZWJ sequences: emoji + U+200D + emoji (up to 7+ codepoints)
- Variation selectors: U+FE0E (text) / U+FE0F (emoji presentation)

For correctness, the decoder should:
1. Render the base emoji as `??` (2 cells)
2. Consume and discard following modifiers, ZWJ, and variation selectors
3. Not render additional `??` for each modifier/ZWJ component

This requires tracking "last codepoint was emoji" state to absorb modifiers:

```c
/* In Terminal struct */
unsigned char   last_was_emoji;     /* absorb following modifiers */

/* In term_put_unicode */
if (term->last_was_emoji) {
    if (cp == 0x200D ||                           /* ZWJ */
        (cp >= 0x1F3FB && cp <= 0x1F3FF) ||       /* skin tones */
        cp == 0xFE0E || cp == 0xFE0F ||           /* variation selectors */
        (cp >= 0xE0020 && cp <= 0xE007F) ||       /* tag sequences */
        cp == 0x20E3) {                           /* combining enclosing keycap */
        return;     /* absorb silently */
    }
    term->last_was_emoji = 0;
}
```

## Memory Impact

| Component | Size |
|-----------|------|
| UTF-8 decoder state (Terminal struct) | 7 bytes |
| latin1_to_macroman table | 128 bytes |
| boxdraw_to_dec() code | ~300 bytes |
| unicode_symbol_to_macroman() code | ~150 bytes |
| utf8_decode() code | ~80 bytes |
| term_put_unicode() code | ~200 bytes |
| PARSE_NORMAL UTF-8 interception | ~100 bytes |
| **Total** | **~965 bytes** |

TermCell remains 2 bytes. No increase to screen/scrollback/alt buffer memory.

## Phase Placement

**Phase 14: UTF-8 Support** — after Phase 13 (xterm compat), because:
- TTYPE=xterm (Phase 13) makes UTF-8 content much more likely
- Phase 13's mode consumption prevents parser confusion from UTF-8 in escapes
- Independent of Phase 11 (bookmarks) and Phase 12 (fonts)
- Can run in parallel with 11/12 since it only touches terminal.c and terminal.h

However, the **box-drawing subset alone** could be pulled into Phase 13 as a
quick win, since it directly fixes the tmux border garbage observed in Phase 10
QA. The full Latin/symbol/emoji handling can follow.

### Estimated effort
- ~150 lines of new code
- ~130 bytes of lookup tables
- ~1KB total code size increase
- Files: terminal.h (struct fields), terminal.c (decoder + translation)
- No changes to terminal_ui.c (reuses existing draw_line_char and DrawText)

## Testing

1. **Box-drawing**: `tmux` borders with UTF-8 locale — should render as lines
2. **Latin accents**: `echo "café résumé naïve"` — should display correctly
3. **Symbols**: `echo "— '' "" • … €"` — should show Mac Roman equivalents
4. **Emoji**: `echo "Hello 😀🎉"` — should show `Hello ????` (placeholder)
5. **Mixed**: `ls -la` with UTF-8 filenames — accented chars shown, CJK as `??`
6. **DEC ACS still works**: Explicit ESC(0 box-drawing — unchanged behavior
7. **Invalid UTF-8**: Random 0x80-0xFF bytes — should show fallback, not crash
8. **Performance**: UTF-8 decode adds ~10 instructions per multi-byte char;
   negligible on 68000 since network I/O is the bottleneck

## Alternatives Considered

### A. Set LANG=C on server
Workaround, not a fix. Breaks non-ASCII display for all applications. Users
must remember to set it. Not viable as a long-term solution.

### B. Expand TermCell to store Unicode
+23KB memory, requires changes throughout terminal.c, terminal_ui.c, and
selection code. Still can't render the glyphs since Mac fonts don't have them.
All pain, no gain.

### C. Custom Unicode font
Create a bitmap font resource with box-drawing and common Unicode glyphs.
Massive effort, limited to pre-defined character set, takes up ROM/RAM.
Not worth it when translation-on-input covers the practical cases.

### D. Report non-UTF-8 locale to server
Could set `LANG=C` via Telnet ENVIRON option (RFC 1572). Servers would then
send ASCII/Latin-1 instead of UTF-8. But this suppresses legitimate non-ASCII
content and many modern servers ignore ENVIRON.
