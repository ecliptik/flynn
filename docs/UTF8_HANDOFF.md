# UTF-8 in Flynn — Status & Handoff

Audience: another Claude/dev team picking up UTF-8 work, and the Geomys team
borrowing Flynn's implementation.

## TL;DR

**Flynn already has comprehensive UTF-8 support for all network-facing text.**
There is no greenfield work required for the core display path. This document
exists to (a) record where that support lives, (b) flag the few optional
improvements still open, and (c) serve as the canonical reference for the
sibling **Geomys** project, which added UTF-8 by borrowing Flynn's code
(see `geomys/docs/UTF8_PLAN.md`).

## Where UTF-8 lives

All inbound network text — telnet, SSH session, and finger — is fed through
`terminal_process()` in `src/terminal.c`. `finger.c` does this explicitly
(`finger_process_data` → `terminal_process`, `src/finger.c`, around line 66), so there is no
second text path to retrofit.

The UTF-8 pipeline inside `terminal.c`:

| Piece | Location | Role |
|-------|----------|------|
| Stateful byte handler | `terminal.c:465–560` | accumulates 2/3/4-byte sequences across `terminal_process` calls; distinguishes UTF-8 continuation bytes from C1 controls; flushes incomplete sequences as CP437. |
| `utf8_decode(buf, len)` | `terminal.c:2853` | decodes a complete sequence → code point; rejects overlong encodings and surrogates. |
| `latin1_to_macroman[128]` | `terminal.c:2919` | U+0080–U+00FF → Mac Roman byte (`0` = unmapped). |
| `unicode_to_macroman(cp)` | `terminal.c:2940` | wraps the Latin-1 table. |
| `unicode_symbol_to_macroman(cp)` | `terminal.c:2951` | smart quotes, en/em dash, bullet, ellipsis, ™, €, ≠, ≤, ≥, π, ∆, middle dot. |
| `boxdraw_to_dec(cp)` | `terminal.c:2893` | heavy box-drawing → DEC Special Graphics (light/double handled by glyph tables). |
| `term_put_unicode(cp)` | `terminal.c:2981` | **resolution order**: invisible-absorb → emoji-modifier-absorb → Braille → `glyph_lookup` → Latin-1 → box-drawing → Unicode spaces → symbols → CJK/emoji placeholder → `'?'`. |

### Notable robustness features (already implemented)

- **CP437 fallback heuristic** (`terminal.c:472`): a 2-byte sequence decoding
  to `U+0100–U+07FF` with no glyph is treated as raw CP437 instead of UTF-8 —
  handles servers that send CP437 despite advertising `xterm-256color`.
- **Incomplete-sequence flush** (`terminal.c:526`): a new lead byte arriving
  mid-sequence flushes the buffered bytes as CP437.
- **Invisible/formatting absorption** (`terminal.c:2985`): soft hyphen, ZWSP
  family, bidi marks, variation selectors, BOM, tag characters.
- **Emoji modifier absorption** (`terminal.c:2995`): skin-tone and keycap
  modifiers folded into the preceding emoji.

## Open / optional improvements

These are enhancements, not bugs. None block normal use.

1. **ASCII transliteration fallback.** Flynn currently emits `'?'` for Latin
   code points outside Mac Roman (e.g. Latin Extended-A `ā ş ő`,
   `term_put_unicode` final fallback). Geomys now has a small
   `unicode_transliterate()` (`geomys/src/utf8.c`) that strips diacritics to a
   base letter (`ā → a`). **Recommend backporting it to Flynn**, for
   readability of e.g. Central/Eastern European BBS text.
2. **Shared module extraction (de-duplication).** Geomys has copied
   `utf8_decode`, `latin1_to_macroman`, `unicode_to_macroman`, and
   `unicode_symbol_to_macroman` out of Flynn's `terminal.c`. To avoid the two
   projects drifting, consider factoring these **pure, Toolbox-free** functions
   into a shared `utf8.c`/`unicode_map.c` that both repos compile (Geomys and
   Flynn already share `glyphs.c`/`cp437.c` by copy). The stateful terminal
   byte-handler stays in `terminal.c`; only the pure mappers move.
3. **Glyph-table coverage** is the inherent ceiling (finite, hand-built:
   ~180 primitives + 57 sextants + 18 emoji in `glyphs.h`). Anything outside
   Mac Roman and that table renders as `'?'`/placeholder. Expanding it is a
   separate, ongoing effort independent of UTF-8 decoding.

## How Geomys reuses this

Geomys processes **whole buffered lines** (Gopher menu titles and text-file
content), not a streaming terminal, so it does **not** need the stateful
continuation-byte handler. Implemented in `geomys/src/utf8.c` (as of
2026-10-19), it:

- Ports `utf8_decode`, `latin1_to_macroman`, `unicode_to_macroman`,
  `unicode_symbol_to_macroman` **verbatim** into `geomys/src/utf8.c`.
- Reuses `term_put_unicode`'s **resolution order** as the model for a
  non-terminal `utf8_transcode_line()` that returns a flat Mac Roman + ASCII
  string for `DrawText`/`DrawString` (no terminal cell grid, no bitmap glyphs
  in content — glyphs collapse to their `copy_char`).
- Adds per-line UTF-8 **auto-detection** (validate the line; fall back to CP437
  if it isn't clean UTF-8), reusing Flynn's `U+0100–U+07FF`-no-glyph heuristic.

Full Geomys plan: `geomys/docs/UTF8_PLAN.md`.

## If you do take on the optional work

- Keep the pure mappers Toolbox-free so both projects can share them.
- Preserve the CP437 fallback heuristics — they are load-bearing for real BBS
  servers that lie about their encoding.
- Test against: UTF-8 umlauts, smart quotes, an em-dash, `€`, a stray lead
  byte, raw CP437 ANSI art (must NOT be misread as UTF-8), and CJK (→ `'?'`).
