# Plan: ANSI/xterm Terminal Support

## Overview

Add xterm terminal emulation to Flynn so it can report `TERM=xterm` and correctly
handle the escape sequences that modern Linux applications (bash, tmux, vi, nano,
less, htop, mc) send when they see an xterm terminal type. This is NOT xterm-256color
— Flynn runs on a monochrome Mac Plus, so all color information is silently consumed.

## What "xterm" Means in Practice

When a Linux server sees `TERM=xterm`, the terminfo database tells applications
they can use a superset of VT100/VT220 that includes:

| Feature | VT100 has it? | Apps rely on it? | Flynn impact |
|---------|--------------|-----------------|-------------|
| Application cursor keys (DECCKM) | Yes | **Yes** — vi, tmux | Must implement |
| Alternate screen buffer (1049) | No | **Yes** — vi, less, nano, tmux | Must implement (Task #1) |
| OSC window title | No | **Yes** — bash PROMPT_COMMAND, tmux | Must implement |
| Bracketed paste (2004) | No | **Yes** — bash, zsh, tmux | Must implement |
| Extended SGR colors | No | **Yes** — nearly everything | Must silently consume |
| Secondary DA (CSI > c) | No | Sometimes — tmux, vim | Should implement |
| DECAWM (auto-wrap mode 7) | Yes | Yes — terminfo queries it | Should implement |
| Save/restore cursor (1048) | No | Sometimes | Easy, reuse ESC 7/8 |
| XTerm window ops (CSI t) | No | Rarely | Ignore |
| Mouse reporting (1000+) | No | tmux, mc, vim | Phase 2 |
| Focus events (1004) | No | tmux, vim | Low priority |

### What apps actually send with TERM=xterm

**bash/readline**: `ESC]0;user@host:dir\007` (window title), DECSET 2004 (bracketed paste),
CSI > c (secondary DA on startup)

**tmux**: DECSET 1049 (alt screen), DECSET 1 (app cursor), DECSET 2004, OSC title,
DECSET 1006 (SGR mouse — optional), focus events (1004), heavy SGR color usage

**vi/vim**: DECSET 1049, DECSET 1 (app cursor), DECSET 12 (cursor blink), DECSET 25,
CSI > c (secondary DA), DECSET 2004, `t_Co` queries for colors

**nano**: DECSET 1049, DECSET 1, SGR colors for syntax highlighting

**less**: DECSET 1049 (if LESSOPEN), heavy SGR for bold/underline

**htop**: DECSET 1049, extensive SGR color, CSI cursor positioning

**mc (Midnight Commander)**: DECSET 1049, DECSET 1000 (mouse), SGR colors

## Priority 1 — Required for Basic xterm Compatibility

### 1.1 TTYPE: Report "xterm" Instead of "VT100"

**File: `src/telnet.c:70`**

Change:
```c
static const char ttype_str[] = "VT100";
```
to:
```c
static const char ttype_str[] = "xterm";
```

**Tradeoffs**:
- "xterm" → server enables all xterm features; we must handle them or apps break
- "VT100" → safe but apps can't use alt screen, cursor keys may misbehave
- "VT220" → middle ground but still lacks alt screen, OSC, bracketed paste
- "xterm" is the right choice because it matches what we're implementing

**Risk**: If we report "xterm" but don't handle a sequence, apps may render incorrectly.
Mitigation: implement the critical sequences first, silently ignore the rest.

### 1.2 Application Cursor Keys (DECCKM — DECSET 1)

When DECCKM is set, arrow keys must send SS3 sequences (`ESC O A`) instead of
CSI sequences (`ESC [ A`). vi and tmux rely on this.

**File: `src/terminal.h`** — Add to Terminal struct:
```c
unsigned char   decckm;         /* application cursor key mode */
```

**File: `src/terminal.c`** — In `term_execute_csi`, extend DECSET/DECRST handlers:
```c
case 'h':  /* SM - set mode */
    if (term->intermediate == '?') {
        switch (p1) {
        case 1:  term->decckm = 1; break;          /* DECCKM */
        case 25: term->cursor_visible = 1; break;   /* DECTCEM */
        }
    }
    break;
case 'l':  /* RM - reset mode */
    if (term->intermediate == '?') {
        switch (p1) {
        case 1:  term->decckm = 0; break;
        case 25: term->cursor_visible = 0; break;
        }
    }
    break;
```

**File: `src/main.c`** — In `handle_key_down`, arrow key section:
```c
case 0x7E:  /* Up arrow */
    if (terminal.decckm)
        conn_send(&conn, "\033OA", 3);
    else
        conn_send(&conn, "\033[A", 3);
    return;
/* ... same for Down (OB), Right (OC), Left (OD) */
```

**Note**: Home/End/PageUp/PageDown/Delete are NOT affected by DECCKM — they stay as
`ESC[H`, `ESC[F`, `ESC[5~`, etc.

### 1.3 OSC Parsing (Window Title)

Applications send `ESC ] Ps ; Pt BEL` or `ESC ] Ps ; Pt ESC \` (ST) to set the
window title. The most common:
- `ESC]0;title\007` — set icon name and window title
- `ESC]1;title\007` — set icon name only
- `ESC]2;title\007` — set window title only

**File: `src/terminal.h`** — New parse states and buffer:
```c
#define PARSE_OSC           5   /* got ESC ] */
#define PARSE_OSC_STRING    6   /* collecting OSC payload */
#define PARSE_OSC_ESC       7   /* got ESC inside OSC (possible ST) */

#define TERM_OSC_BUFSIZ     128

/* In Terminal struct: */
char            osc_buf[TERM_OSC_BUFSIZ];
short           osc_len;
short           osc_param;      /* the Ps number (0, 1, 2, etc.) */
unsigned char   title_changed;  /* flag: UI should read new title */
char            window_title[64];
```

**File: `src/terminal.c`** — New ESC handler and OSC state machine:

In `term_process_esc`:
```c
case ']':
    term->parse_state = PARSE_OSC;
    term->osc_len = 0;
    term->osc_param = -1;
    return;
```

New function `term_process_osc`:
```c
static void term_process_osc(Terminal *term, unsigned char ch)
{
    switch (term->parse_state) {
    case PARSE_OSC:
        /* Collecting the numeric parameter before ';' */
        if (ch >= '0' && ch <= '9') {
            if (term->osc_param < 0)
                term->osc_param = 0;
            term->osc_param = term->osc_param * 10 + (ch - '0');
        } else if (ch == ';') {
            term->parse_state = PARSE_OSC_STRING;
        } else if (ch == 0x07) {
            /* BEL terminates with no string — ignore */
            term->parse_state = PARSE_NORMAL;
        } else if (ch == 0x1B) {
            term->parse_state = PARSE_OSC_ESC;
        } else {
            /* Malformed — abort */
            term->parse_state = PARSE_NORMAL;
        }
        break;

    case PARSE_OSC_STRING:
        if (ch == 0x07) {
            /* BEL terminates the string */
            term_finish_osc(term);
            term->parse_state = PARSE_NORMAL;
        } else if (ch == 0x1B) {
            term->parse_state = PARSE_OSC_ESC;
        } else {
            if (term->osc_len < TERM_OSC_BUFSIZ - 1)
                term->osc_buf[term->osc_len++] = ch;
        }
        break;

    case PARSE_OSC_ESC:
        if (ch == '\\') {
            /* ST (ESC \) terminates the string */
            term_finish_osc(term);
        }
        /* Either way, return to normal */
        term->parse_state = PARSE_NORMAL;
        break;
    }
}
```

New function `term_finish_osc`:
```c
static void term_finish_osc(Terminal *term)
{
    short len;

    term->osc_buf[term->osc_len] = '\0';

    switch (term->osc_param) {
    case 0:  /* Set icon name + window title */
    case 2:  /* Set window title */
        len = term->osc_len;
        if (len > 63) len = 63;
        memcpy(term->window_title, term->osc_buf, len);
        term->window_title[len] = '\0';
        term->title_changed = 1;
        break;
    case 1:
        /* Icon name only — ignore on classic Mac */
        break;
    default:
        /* Other OSC codes (4=color, 52=clipboard, etc.) — ignore */
        break;
    }
}
```

**File: `src/main.c`** — In the null event handler, after `terminal_process`:
```c
if (terminal.title_changed) {
    Str255 ptitle;
    char tmp[80];
    short len, i;

    if (terminal.window_title[0]) {
        len = sprintf(tmp, "Flynn - %s", terminal.window_title);
    } else {
        len = sprintf(tmp, "Flynn - %s", conn.host);
    }
    ptitle[0] = len;
    for (i = 0; i < len; i++)
        ptitle[i + 1] = tmp[i];
    SetWTitle(term_window, ptitle);
    terminal.title_changed = 0;
}
```

Also update `handle_update` to prefer `window_title` over `conn.host`.

### 1.4 Extended SGR Color Consumption

The current `term_set_attr` already ignores unknown SGR params via the `default` case.
However, **256-color and truecolor SGR use sub-parameters** that consume multiple
params from the same CSI sequence:

- `ESC[38;5;Nm` — set foreground to palette color N (3 params)
- `ESC[48;5;Nm` — set background to palette color N (3 params)
- `ESC[38;2;R;G;Bm` — set foreground to RGB (5 params)
- `ESC[48;2;R;G;Bm` — set background to RGB (5 params)

The current SGR loop processes params individually. When it hits `38`, it ignores it,
then processes `5` (which is "blink" in standard SGR) and `N` (which could be
misinterpreted). This will cause **incorrect attribute rendering**.

**File: `src/terminal.c`** — Rewrite the SGR `m` handler to skip color sub-params:

```c
case 'm':
    /* SGR - set graphic rendition */
    if (term->num_params == 0) {
        term_set_attr(term, 0);
    } else {
        short i;
        for (i = 0; i < term->num_params; i++) {
            if (term->params[i] == 38 || term->params[i] == 48) {
                /* Extended color: skip sub-parameters */
                if (i + 1 < term->num_params &&
                    term->params[i + 1] == 5) {
                    i += 2;  /* skip 5;N */
                } else if (i + 1 < term->num_params &&
                           term->params[i + 1] == 2) {
                    i += 4;  /* skip 2;R;G;B */
                }
            } else {
                term_set_attr(term, term->params[i]);
            }
        }
    }
    break;
```

Also add SGR attributes that map usefully to monochrome:

```c
case 2:   /* Dim — map to normal on monochrome */
    term->cur_attr &= ~ATTR_BOLD;
    break;
case 3:   /* Italic — map to underline on monochrome */
    term->cur_attr |= ATTR_UNDERLINE;
    break;
case 5:   /* Blink — map to bold on monochrome (visible emphasis) */
case 6:   /* Rapid blink — same */
    term->cur_attr |= ATTR_BOLD;
    break;
case 8:   /* Hidden/invisible — could use ATTR_INVERSE of bg, but just ignore */
    break;
case 9:   /* Strikethrough — ignore on monochrome */
    break;
case 23:  /* Not italic */
    term->cur_attr &= ~ATTR_UNDERLINE;
    break;
case 25:  /* Not blinking */
    /* Don't clear bold here since user-set bold should persist */
    break;
case 28:  /* Not hidden */
    break;
case 29:  /* Not strikethrough */
    break;
```

**Important**: Bright foreground colors (SGR 90-97) should map to bold since that's
how terminals traditionally render bright colors. This gives `ls --color` meaningful
visual distinction:

```c
/* Bright foreground: 90-97 → enable bold for visual distinction */
case 90: case 91: case 92: case 93:
case 94: case 95: case 96: case 97:
    term->cur_attr |= ATTR_BOLD;
    break;
```

Standard foreground (30-37), background (40-47), default (39, 49), and bright
background (100-107) remain silently consumed via the `default` case.

### 1.5 Alternate Screen Buffer (DECSET 1049)

**This is covered by Task #1** and is a prerequisite for full xterm support.
The plan here assumes it will be implemented per Task #1's design.

Key integration point: DECSET/DECRST mode 1049 in `terminal.c` must:
- Save cursor + switch to alt buffer on `ESC[?1049h`
- Restore cursor + switch to main buffer on `ESC[?1049l`
- Alt buffer has NO scrollback (standard xterm behavior)

### 1.6 Bracketed Paste Mode (DECSET 2004)

When enabled, pasted text must be wrapped in `ESC[200~` ... `ESC[201~` so the
application can distinguish paste from typed input. bash, zsh, and tmux all enable
this.

**File: `src/terminal.h`** — Add to Terminal struct:
```c
unsigned char   bracketed_paste;  /* DECSET 2004 */
```

**File: `src/terminal.c`** — In DECSET/DECRST:
```c
case 2004: term->bracketed_paste = 1; break;  /* in 'h' */
case 2004: term->bracketed_paste = 0; break;  /* in 'l' */
```

**File: `src/main.c`** — In `do_paste`:
```c
if (terminal.bracketed_paste)
    conn_send(&conn, "\033[200~", 6);

/* ... existing paste logic ... */

if (terminal.bracketed_paste)
    conn_send(&conn, "\033[201~", 6);
```

## Priority 2 — Important for Robustness

### 2.1 Extended DECSET/DECRST Private Modes

The `h`/`l` handler currently uses a simple `if (p1 == 25)` check. It needs to become
a switch to handle multiple modes. Also, **DECSET/DECRST can have multiple parameters**
in a single sequence (e.g., `ESC[?1;25h`), though this is rare.

Modes to handle:

| Mode | Name | Action |
|------|------|--------|
| 1 | DECCKM | Application cursor keys (see 1.2) |
| 5 | DECSCNM | Reverse video (entire screen) — could implement or ignore |
| 6 | DECOM | Origin mode — cursor relative to scroll region |
| 7 | DECAWM | Auto-wrap mode — currently always on |
| 12 | Cursor blink | Silently acknowledge |
| 25 | DECTCEM | Cursor visible — already implemented |
| 47 | Alt screen (old) | Map to 1049 behavior |
| 1000 | Mouse click reporting | Silently ignore (Phase 2) |
| 1002 | Mouse button-event tracking | Silently ignore |
| 1003 | Mouse all-motion tracking | Silently ignore |
| 1004 | Focus events | Silently ignore |
| 1006 | SGR mouse mode | Silently ignore |
| 1048 | Save/restore cursor | Reuse ESC 7/8 logic |
| 1049 | Alt screen + cursor | Task #1 |
| 2004 | Bracketed paste | See 1.6 |

**Implementation**: Refactor to a switch with all modes, default silently ignoring
unknown modes (which is already the current behavior for unrecognized CSI commands).

### 2.2 Secondary Device Attributes (CSI > c)

tmux and vim send `CSI > c` (Secondary DA) on startup. Without a response, they may
wait or fall back to degraded mode.

**File: `src/terminal.c`** — The `>` character is currently handled as an intermediate
byte in `term_process_csi` (it falls in the 0x30-0x3F range, but it's actually a
"parameter byte" prefix). Need to detect it:

The `>` character (0x3E) in CSI context can be detected as a modifier:
```c
if (ch == '>') {
    term->intermediate = '>';
    return;
}
```

Then in `term_execute_csi`, for `c` command:
```c
case 'c':
    if (term->intermediate == '>' || term->intermediate == 0) {
        if (term->intermediate == '>') {
            /* Secondary DA: report as xterm version 136 */
            memcpy(term->response, "\033[>0;136;0c", 11);
            term->response_len = 11;
        } else {
            /* Primary DA: report as VT100 with AVO */
            memcpy(term->response, "\033[?1;2c", 7);
            term->response_len = 7;
        }
    }
    break;
```

The `>0;136;0c` response means: terminal type 0 (VT100), firmware version 136 (arbitrary
xterm-like version), ROM revision 0.

### 2.3 DECAWM (Auto-Wrap Mode 7)

Currently auto-wrap is always enabled. Some applications toggle it off for status lines.

**File: `src/terminal.h`** — Add to Terminal struct:
```c
unsigned char   decawm;         /* auto-wrap mode (default: 1) */
```

**File: `src/terminal.c`** — In `terminal_init`:
```c
term->decawm = 1;  /* auto-wrap on by default */
```

In `term_put_char`, make wrap conditional:
```c
if (term->cur_col < TERM_COLS - 1) {
    term->cur_col++;
} else if (term->decawm) {
    term->wrap_pending = 1;
}
/* else: cursor stays at right margin, overwrites */
```

In DECSET/DECRST:
```c
case 7: term->decawm = 1; break;  /* 'h' */
case 7: term->decawm = 0; break;  /* 'l' */
```

### 2.4 DECOM (Origin Mode 6)

When origin mode is set, cursor positioning is relative to the scroll region.
Some applications (vim with split windows) use this.

**File: `src/terminal.h`** — Add to Terminal struct:
```c
unsigned char   decom;          /* origin mode */
```

In DECSET/DECRST:
```c
case 6: term->decom = 1; term->cur_row = term->scroll_top; term->cur_col = 0; break;
case 6: term->decom = 0; term->cur_row = 0; term->cur_col = 0; break;
```

In CUP/HVP (`H`/`f` commands), adjust for origin mode:
```c
if (term->decom) {
    term->cur_row = term_clamp(p1 - 1 + term->scroll_top,
        term->scroll_top, term->scroll_bottom);
} else {
    term->cur_row = term_clamp(p1 - 1, 0, TERM_ROWS - 1);
}
```

## Priority 3 — Future / Phase 2

### 3.1 Mouse Reporting

xterm mouse reporting sends click/motion events to the application. This is used by
tmux (pane selection), mc (file manager), and vim (if configured). The Mac Plus has a
single-button mouse, which limits utility.

Modes: 1000 (click), 1002 (button-event), 1003 (all-motion), 1006 (SGR format)

For Phase 2: track the active mouse mode in the Terminal struct, and in
`handle_content_click`, if a mouse mode is active, send the appropriate escape
sequence instead of starting a text selection.

### 3.2 Focus Events (DECSET 1004)

When enabled, send `ESC[I` on focus-in and `ESC[O` on focus-out. Could be implemented
by hooking `activateEvt` in the event loop. Low priority since few apps require it.

### 3.3 Window Manipulation (CSI t)

XTerm window operations like resize, move, iconify. Silently ignore all of these.
The `t` final byte is not currently handled, so it already falls through to the
default case.

## Sequences to Explicitly Ignore

These sequences should NOT cause error output or parser confusion:

| Sequence | Purpose | Action |
|----------|---------|--------|
| `CSI ? 12 h/l` | Cursor blink on/off | Silently ignore |
| `CSI ? 1000-1006 h/l` | Mouse modes | Silently ignore |
| `CSI ? 1004 h/l` | Focus events | Silently ignore |
| `CSI t` | Window manipulation | Already ignored (default) |
| `CSI > c` | Secondary DA | Respond (see 2.2) |
| `CSI ! p` | DECSTR soft reset | Could implement or ignore |
| `CSI ? s` / `CSI ? r` | Save/restore DEC private modes | Ignore |
| `ESC ] 4 ; ...` | Set/query colors | Ignore via OSC parser |
| `ESC ] 10-19 ; ...` | Set/query dynamic colors | Ignore via OSC parser |
| `ESC ] 52 ; ...` | Clipboard access | Ignore via OSC parser |
| `ESC ] 112` | Reset cursor color | Ignore via OSC parser |
| `ESC ( B` | Set G0 charset to ASCII | Already consumed |
| `ESC ( 0` | Set G0 charset to DEC Special Graphics | Consume (could implement later) |
| `ESC = ` | DECKPAM (application keypad) | Ignore for now |
| `ESC > ` | DECKPNM (normal keypad) | Ignore for now |
| `CSI ? 5 h/l` | DECSCNM reverse video | Ignore (monochrome context) |

## Parser Changes Summary

### New Parse States

```
PARSE_NORMAL (0)      — existing
PARSE_ESC (1)         — existing
PARSE_CSI (2)         — existing
PARSE_CSI_PARAM (3)   — existing
PARSE_CSI_INTERMEDIATE (4) — existing
PARSE_OSC (5)         — NEW: after ESC ]
PARSE_OSC_STRING (6)  — NEW: collecting OSC text payload
PARSE_OSC_ESC (7)     — NEW: ESC inside OSC (possible ST terminator)
```

### New Terminal Struct Fields

```c
/* xterm modes */
unsigned char   decckm;           /* DECSET 1: application cursor keys */
unsigned char   decawm;           /* DECSET 7: auto-wrap (default 1) */
unsigned char   decom;            /* DECSET 6: origin mode */
unsigned char   bracketed_paste;  /* DECSET 2004 */

/* OSC */
char            osc_buf[128];
short           osc_len;
short           osc_param;
unsigned char   title_changed;
char            window_title[64];

/* Alt screen buffer — covered by Task #1 */
```

**Memory impact**: ~200 bytes additional for OSC buffer + title + mode flags.
Negligible relative to the existing ~25KB footprint.

### Modified Switch/Dispatch in terminal_process

Add OSC states to the main `switch (term->parse_state)`:
```c
case PARSE_OSC:
case PARSE_OSC_STRING:
case PARSE_OSC_ESC:
    term_process_osc(term, ch);
    break;
```

## File Changes Summary

| File | Changes |
|------|---------|
| `src/terminal.h` | New parse state defines (5-7), new Terminal struct fields (decckm, decawm, decom, bracketed_paste, osc_buf, osc_len, osc_param, title_changed, window_title) |
| `src/terminal.c` | OSC parser (new states + handler), extended DECSET/DECRST switch, SGR color sub-param skipping, SGR bright→bold mapping, Secondary DA response, DECAWM conditional wrap, DECOM origin-relative cursor |
| `src/telnet.c` | Change ttype_str from "VT100" to "xterm" (line 70) |
| `src/main.c` | Application cursor key mode (conditional arrow sequences), bracketed paste wrapping in do_paste, OSC title → SetWTitle, optional: DECSET/ESC = keypad mode |
| `src/terminal_ui.c` | No changes required for Phase 1 |

## Implementation Order

1. **SGR color sub-parameter skipping** (terminal.c) — fixes immediate rendering bugs
   with color apps, zero risk
2. **OSC parsing + window title** (terminal.h, terminal.c, main.c) — new parser states,
   medium complexity, high visibility
3. **DECCKM application cursor keys** (terminal.h, terminal.c, main.c) — small change,
   fixes vi/tmux arrow keys
4. **Extended DECSET/DECRST switch** (terminal.c) — refactor existing if-chain to
   switch, add DECAWM/DECOM/bracketed_paste/mouse-ignore
5. **Bracketed paste** (terminal.h, terminal.c, main.c) — small, improves paste in
   bash/zsh
6. **Secondary DA** (terminal.c) — small, fixes tmux/vim startup
7. **TTYPE change** (telnet.c) — one-line change, do LAST after all sequences are
   handled, because this is what triggers apps to send xterm sequences
8. **Alternate screen buffer** — Task #1, can be done in parallel

## Testing Strategy

1. Build and deploy with TERM=xterm override (env var) before changing TTYPE,
   to test sequence handling incrementally
2. Test apps in order: `ls --color`, `bash` (title, bracketed paste), `vi` (alt screen,
   cursor keys), `tmux` (everything), `htop` (colors → bold mapping), `nano`,
   `mc` (mouse will be ignored gracefully)
3. Verify no rendering artifacts from consumed color codes
4. Verify window title updates from bash prompt
5. Verify arrow keys work correctly in vi (application cursor mode)
6. Verify paste works correctly in bash with bracketed paste

## Risks

- **TERM=xterm without full support**: Apps may send sequences we don't handle.
  Mitigation: the parser already silently ignores unknown CSI sequences via the default
  case, and the new OSC parser will consume unknown OSC codes.
- **SGR sub-parameter edge cases**: Some apps use colon-separated sub-params
  (`38:5:N` instead of `38;5;N`). This is rare and can be addressed later if needed.
- **Memory**: OSC buffer adds 128 bytes + 64 bytes title = ~200 bytes. Negligible.
- **DEC Special Graphics charset** (`ESC ( 0`): Line-drawing characters used by tmux
  and mc borders. Currently consumed but not rendered as line-drawing. Could be a
  future enhancement but apps fall back to ASCII alternatives.
