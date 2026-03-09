# Flynn 1.0 Pre-Release Audit (Historical)

> **Note:** This audit was performed prior to the v1.0.0 release. Issues documented here have been addressed.

Consolidated findings from performance profiling and security analysis of all
source files in `src/` (22 files, ~7,400 lines of C).

## Summary

| Category | Critical | High | Medium | Low |
|----------|----------|------|--------|-----|
| Security | 4 | 3 | 3 | 2 |
| Performance | 0 | 2 | 3 | 2 |

**Memory footprint (corrected):** ~59KB total BSS (Terminal=52KB, Connection=5KB,
FlynnPrefs=1.7KB, TelnetState=154B). ~1.5% of 4MB Mac Plus RAM. No memory leaks
found across connect/disconnect cycles, font changes, or window resize.

---

## CRITICAL — Must Fix for 1.0

### SEC-1: sprintf overflow in handle_update window title
**File:** main.c:739-742
**Severity:** CRITICAL — remote-triggered stack buffer overflow
```c
char title[80];
len = sprintf(title, "Flynn - %s", conn.host);  /* host can be 253 chars */
```
`conn.host[256]` holds validated hostnames up to 253 chars (DNS max). "Flynn - "
(8 chars) + 253 = 261, overflowing the 80-byte stack buffer by 181 bytes.

**Fix:** Use snprintf or size the buffer to match:
```c
char title[270];  /* "Flynn - " + 253 + null */
```
Or better, since this is a window title (Mac Toolbox truncates anyway):
```c
len = snprintf(title, sizeof(title), "Flynn - %s", conn.host);
```
**Complexity:** Trivial

### SEC-2: sprintf overflow in bm_list_draw
**File:** main.c:1019-1026
**Severity:** CRITICAL — stack buffer overflow from bookmark data
```c
char line[64];
len = sprintf(line, "%s - %s:%u",
    p->bookmarks[i].name,     /* 31 chars max */
    p->bookmarks[i].host,     /* 127 chars max */
    p->bookmarks[i].port);    /* 5 chars max */
```
Combined: 31 + 3 + 127 + 1 + 5 = 167 chars into 64-byte buffer.

**Fix:** Increase buffer or use snprintf:
```c
char line[180];
```
**Complexity:** Trivial

### SEC-3: Telnet send buffer overflow
**File:** main.c:288, telnet.c:93-99
**Severity:** CRITICAL — network-triggered heap/stack corruption
```c
unsigned char send_buf[256];    /* main.c:288 */
```
The telnet.h contract says send buffer must be "at least as large as inlen"
(TCP_READ_BUFSIZ = 4096). A malicious server sending many IAC WILL commands
for unsupported options generates 3-byte DONT responses each, unbounded.
With 4096 bytes of IAC WILL pairs (1365 commands), responses = 4095 bytes,
far exceeding the 256-byte buffer.

`send_bytes()` in telnet.c:98 has no bounds checking:
```c
send[(*sendlen)++] = data[i];  /* no limit check */
```

**Fix (two-part):**
1. Increase send_buf to match contract:
   ```c
   unsigned char send_buf[TCP_READ_BUFSIZ];
   ```
2. Add bounds checking to send_bytes():
   ```c
   #define TELNET_SEND_BUFSIZ  4096
   if (*sendlen < TELNET_SEND_BUFSIZ)
       send[(*sendlen)++] = data[i];
   ```
**Complexity:** Small

### SEC-4: sprintf overflow in nullEvent title update
**File:** main.c:325-337
**Severity:** CRITICAL — remote-triggered via OSC title sequences
```c
char tmp[80];
tl = sprintf(tmp, "Flynn - %s", terminal.window_title);
```
`window_title` is char[64], so max = 8 + 63 = 71. Fits in 80 — **actually safe**
in current code. However, the same pattern at main.c:742 (SEC-1) is not safe.
This entry is grouped here for consistency: both should use the same fix pattern.

**Fix:** Apply snprintf uniformly to all title sprintf calls.
**Complexity:** Trivial

---

## HIGH — Should Fix for 1.0

### SEC-5: CSI parameter integer overflow
**File:** terminal.c:728
**Severity:** HIGH — signed integer overflow from network data (undefined behavior)
```c
term->params[idx] = term->params[idx] * 10 + (ch - '0');
```
`params` is `short` (16-bit signed, max 32767). Escape sequences like
`ESC[99999H` cause signed overflow, which is undefined behavior in C.
While `term_clamp()` bounds the final values, the overflow itself is UB and
GCC may optimize unpredictably.

**Fix:** Cap parameter accumulation:
```c
if (term->params[idx] < 10000)
    term->params[idx] = term->params[idx] * 10 + (ch - '0');
```
**Complexity:** Trivial

### SEC-6: Connection dialog host bounds uses hardcoded limit
**File:** connection.c:99-101
**Severity:** HIGH — brittle bounds check
```c
for (i = 0; i < host_str[0] && i < 255; i++)
    conn->host[i] = host_str[i + 1];
```
Should use `sizeof(conn->host) - 1` instead of hardcoded 255.
Currently safe (host[256], so 255 is correct), but will silently break
if the buffer size ever changes.

**Fix:**
```c
short max_len = sizeof(conn->host) - 1;
for (i = 0; i < host_str[0] && i < max_len; i++)
```
Apply same pattern to username parsing at connection.c:119.
**Complexity:** Trivial

### SEC-7: strcpy usage in settings.c defaults
**File:** settings.c:23, 77, 85
**Severity:** HIGH — unsafe pattern (currently safe with hardcoded strings)
```c
strcpy(prefs->dns_server, "1.1.1.1");  /* dns_server[16] */
```
Replace with strncpy for defense-in-depth.
**Complexity:** Trivial

### PERF-1: Startup perceived slowness — no visual feedback
**File:** main.c:66-108
**Severity:** HIGH (user experience)
The initialization sequence takes 300-500ms with no visual feedback:
- `conn_init()` / `_TCPInit()`: 100-200ms (MacTCP driver init)
- `terminal_init()`: memset of ~52KB = ~80ms on 8MHz 68000
- `prefs_load()`: file I/O, 20-30ms

User sees a blank window during this time.

**Fix:** Show the window early with "Initializing..." or reorder init so
the window appears before MacTCP init:
```c
init_toolbox();
init_menus();
init_window();          /* show window first */
/* ... then do slow init ... */
conn_init();            /* MacTCP — slow */
terminal_init();
```
**Complexity:** Small

### PERF-2: terminal_init memsets 52KB unnecessarily at startup
**File:** terminal.c:62
**Severity:** HIGH (startup time)
```c
memset(term, 0, sizeof(Terminal));  /* sizeof = ~52KB */
```
The Terminal struct is ~52KB (screen=13.2KB, scrollback=25.3KB, alt_screen=13.2KB,
plus parser state). At 8MHz 68000 byte-by-byte memset, this takes ~80ms.

At startup, only the active region (80x24 = 3.8KB) is used. The full 132x50
buffers are only needed after window resize.

**Fix:** Only clear the used portion at startup. Clear full buffers lazily
on first resize to larger dimensions. Or use `_BlockMoveData` if Retro68
supports it (longword moves on 68000 are 4x faster than byte moves).
**Complexity:** Medium

---

## MEDIUM — Recommended for 1.0

### SEC-8: Telnet ttype_count unbounded increment
**File:** telnet.c:307
**Severity:** MEDIUM — signed short overflow after 32767 TTYPE requests
```c
ts->ttype_count++;
```
A malicious server repeatedly sending SB TTYPE SEND would increment
indefinitely. After 32767, signed overflow occurs (UB).

**Fix:**
```c
if (ts->ttype_count < 10)
    ts->ttype_count++;
```
**Complexity:** Trivial

### SEC-9: conn_status_str sprintf ignores buflen
**File:** connection.c:381
**Severity:** MEDIUM — sprintf in Connected branch ignores buflen parameter
```c
sprintf(buf, "Connected to %s", ip_str);  /* ignores buflen */
```
Other branches correctly use `strncpy(buf, ..., buflen - 1)`. The Connected
branch should be consistent.

**Fix:**
```c
snprintf(buf, buflen, "Connected to %s", ip_str);
```
**Complexity:** Trivial

### SEC-10: Bookmark strings not null-terminated after prefs_load
**File:** settings.c:45, settings.h:11-15
**Severity:** MEDIUM — corrupted prefs file could cause unbounded string reads
```c
count = sizeof(FlynnPrefs);
err = FSRead(refNum, &count, (Ptr)prefs);
```
Prefs file is read directly into the struct. A corrupted or maliciously
crafted prefs file could contain non-null-terminated strings.

**Fix:** Force null termination after load:
```c
prefs->host[sizeof(prefs->host) - 1] = '\0';
prefs->dns_server[sizeof(prefs->dns_server) - 1] = '\0';
prefs->username[sizeof(prefs->username) - 1] = '\0';
for (i = 0; i < MAX_BOOKMARKS; i++) {
    prefs->bookmarks[i].name[sizeof(prefs->bookmarks[i].name) - 1] = '\0';
    prefs->bookmarks[i].host[sizeof(prefs->bookmarks[i].host) - 1] = '\0';
}
```
**Complexity:** Small

### PERF-3: do_copy allocates 6.6KB on stack
**File:** main.c:1476
**Severity:** MEDIUM — large stack allocation
```c
char buf[TERM_ROWS * (TERM_COLS + 1)];  /* 50 * 133 = 6,650 bytes */
```
At default 80x24 grid, only ~1,944 bytes are needed. This uses 6.6KB of
the ~24KB available stack (after SetApplLimit subtracts 8KB).

**Fix:** Size based on active dimensions:
```c
char buf[terminal.active_rows * (terminal.active_cols + 1)];
```
Or use a heap allocation (`NewPtr`/`DisposePtr`) for safety.
**Complexity:** Small

### PERF-4: handle_update allocates region handle every redraw
**File:** main.c:766-778
**Severity:** MEDIUM — unnecessary heap allocation on every update event
```c
RgnHandle old_clip = NewRgn();
GetClip(old_clip);
/* ... draw grow icon ... */
SetClip(old_clip);
DisposeRgn(old_clip);
```
A region handle is allocated and disposed on every window update.

**Fix:** Cache the clip region as a static or compute once at window init.
**Complexity:** Small

### PERF-5: Key mapping uses long if/else/switch chains
**File:** main.c:500-648
**Severity:** MEDIUM (code quality, minor perf)
274 lines of if/else chains and switch statements for key mapping. Could be
a static lookup table for cleaner code and marginally faster dispatch.

**Fix:** Build a static const lookup table mapping vkey codes to escape
sequences at compile time. Not a priority for 1.0 but would clean up
the largest function in the codebase.
**Complexity:** Medium

---

## LOW — Nice to Have

### PERF-6: Proportional font rendering per-character MoveTo
**File:** terminal_ui.c (draw_line_char)
**Severity:** LOW — correct but slower than monospace batching
Each character is positioned individually with `MoveTo()+DrawChar()` for
proportional fonts. This is necessary for correctness but ~2x slower than
`DrawText()` batching used for monospace fonts.

**Fix:** No easy fix — this is inherent to proportional font support.
Could consider only offering monospace fonts, but that limits user choice.
**Complexity:** N/A (design tradeoff)

### PERF-7: Preferences migration triggers redundant file I/O
**File:** settings.c:53-98
**Severity:** LOW — one-time cost on version upgrade
Each migration step calls `prefs_save()` (delete + create + write).
A v1 prefs file upgrading to v6 triggers 5 saves.

**Fix:** Migrate in-memory, save once at the end:
```c
/* After all migration steps */
if (prefs->version != PREFS_VERSION) {
    prefs->version = PREFS_VERSION;
    prefs_save(prefs);
}
```
**Complexity:** Small

### SEC-11: OSC title buffer truncation
**File:** terminal.h:143, terminal.c (OSC handling)
**Severity:** LOW — truncation is safe, but should be explicit
`window_title[64]` is filled from `osc_buf[128]`. Copying should explicitly
clamp and null-terminate.
**Complexity:** Trivial

### SEC-12: No DITL field length enforcement verification
**File:** connection.c:93-127
**Severity:** LOW — depends on resource file correctness
Dialog text fields rely on DITL resources for length limits. No C-level
validation that field lengths match buffer sizes.

**Fix:** Document required DITL maxChar settings in resource file comments.
**Complexity:** Trivial

---

## Not Actionable (Confirmed Safe)

These areas were investigated and confirmed to be correctly implemented:

- **Memory leaks:** None found. Connection lifecycle (NewPtr/DisposePtr),
  DNS UDP buffer, clipboard Handle, and font changes all properly clean up.
- **Scroll region validation:** terminal.c:951-956 uses term_clamp() and
  resets to full screen if top >= bottom. Correctly handles malformed DECSTBM.
- **Telnet subnegotiation buffer:** telnet.c:441 correctly bounds-checks
  sb_buf writes and silently discards overflow data until SE.
- **DNS response parsing:** dns.c checks packet length before all field accesses.
- **Cursor positioning:** All cursor movements pass through term_clamp() with
  active_rows/active_cols bounds.
- **Stack depth:** Deepest call chain is ~500 bytes. No recursion.
  do_copy's 6.6KB allocation (PERF-3) is the largest single frame.

---

## Implementation Plan

### Phase 1: Critical Security Fixes (must-fix before 1.0)
1. SEC-1: Fix handle_update sprintf overflow (trivial)
2. SEC-2: Fix bm_list_draw sprintf overflow (trivial)
3. SEC-3: Fix telnet send buffer size + add bounds check (small)
4. SEC-5: Cap CSI parameter accumulation (trivial)
5. SEC-6: Use sizeof for connection dialog bounds (trivial)

### Phase 2: High Priority Improvements
6. SEC-7: Replace strcpy with strncpy in settings.c (trivial)
7. PERF-1: Reorder init for faster perceived startup (small)
8. PERF-2: Optimize terminal_init memset scope (medium)
9. SEC-10: Null-terminate prefs strings after load (small)

### Phase 3: Medium Priority (if time permits)
10. SEC-8: Cap ttype_count (trivial)
11. SEC-9: Fix conn_status_str sprintf (trivial)
12. PERF-3: Reduce do_copy stack usage (small)
13. PERF-4: Cache clip region in handle_update (small)

### Phase 4: Low Priority / Post-1.0
14. PERF-5: Key mapping lookup table (medium)
15. PERF-6: Proportional font optimization (N/A)
16. PERF-7: Prefs migration single-save (small)
17. SEC-11, SEC-12: Documentation/cleanup (trivial)

---

## Assembly Optimization Assessment

The perf analyst considered whether 68000 assembly could benefit hot paths.
**Conclusion: Not recommended for 1.0.** The primary bottleneck is MacTCP
initialization and QuickDraw rendering, neither of which benefit from
inline assembly. The terminal data pipeline (TCP → telnet → terminal → UI)
processes at most 4KB per event loop tick at 17ms intervals — well within
the 68000's capability at 8MHz. The only candidate would be a longword-aligned
memset/memcpy for terminal buffer clearing, but this is a one-time cost
per screen clear and not worth the maintenance burden.

---

*Generated: 2026-03-06 by Flynn 1.0 pre-release audit team*
*Analysts: perf-analyst (Opus 4.6), security-analyst (Opus 4.6), team-lead (Opus 4.6)*
*All findings verified against source code with file:line references*
