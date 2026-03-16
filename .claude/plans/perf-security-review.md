# Flynn Performance & Security Review — Implementation Plan

**Branch**: `perf-security-review`
**Date**: 2026-03-16
**Scope**: Dead code removal, security fixes, memory leak fixes, performance improvements, code refactoring
**Impact**: Mono + Color (both System 6 and System 7)

---

## Phase 1: Security Fixes (Safety-Critical First)

These address potential vulnerabilities in a network-facing application. Low risk, high value.

### 1.1 OSC response buffer — clamp snprintf return value
- **File**: `terminal.c:2412-2465`
- **What**: OSC 4/10/11 color query responses write to `term->response` (256 bytes) via snprintf, but unlike the DSR response path (line 2148-2149), the return value is NOT clamped. If snprintf truncates, `response_len` could exceed buffer size, causing `conn_send()` to read past the buffer.
- **Fix**: Add the same clamping pattern after each snprintf into `term->response`:
  ```c
  if (term->response_len >= (short)sizeof(term->response))
      term->response_len = sizeof(term->response) - 1;
  ```
- **Risk**: Very Low — defense-in-depth; current format strings produce ~40 bytes max

### 1.2 Null-terminate finger_host and finger_user after prefs load
- **File**: `settings.c` (after line ~103)
- **What**: After loading preferences from disk, `host`, `dns_server`, `username`, and bookmark fields are explicitly null-terminated, but `finger_host` and `finger_user` are not. Corrupted prefs file could yield unterminated strings.
- **Fix**: Add after existing null-termination block:
  ```c
  prefs->finger_host[sizeof(prefs->finger_host) - 1] = '\0';
  prefs->finger_user[sizeof(prefs->finger_user) - 1] = '\0';
  ```
- **Risk**: Very Low

---

## Phase 2: Memory Leak Fixes

### 2.1 Pre-allocate scroll bar ControlActionUPP
- **File**: `main.c:954`
- **What**: `NewControlActionUPP(scrollbar_action)` is called inside `TrackControl()` on every scroll bar click, leaking ~10 bytes per click on System 7 (Mixed Mode Manager allocates routine descriptors).
- **Fix**: Pre-allocate as a static global at startup, reuse in TrackControl:
  ```c
  static ControlActionUPP g_scroll_action_upp = 0L;
  /* At init: */ g_scroll_action_upp = NewControlActionUPP(scrollbar_action);
  /* In TrackControl: */ TrackControl(hit_ctl, local_pt, g_scroll_action_upp);
  ```
- **Risk**: Very Low

### 2.2 Call term_ui_cleanup() at exit
- **File**: `main.c` (before ExitToShell), `terminal_ui.c:538-545`
- **What**: `term_ui_cleanup()` exists to free `g_offscreen_bits` (~18KB) and `g_color_gworld`, but is never called. Also add `g_scroll_rgn` disposal.
- **Fix**: Call `term_ui_cleanup()` in main.c shutdown path. Add `DisposeRgn(g_scroll_rgn)` to `term_ui_cleanup()`.
- **Risk**: Very Low

### 2.3 Free screen_color on NewWindow failure in session_new()
- **File**: `session.c:79-81`
- **What**: If `NewWindow`/`NewCWindow` fails after `terminal_init()` allocated `screen_color` (System 7), the color buffer leaks.
- **Fix**: Before `DisposePtr((Ptr)s)`, add disposal of color buffers:
  ```c
  if (s->terminal.screen_color) DisposePtr((Ptr)s->terminal.screen_color);
  ```
- **Risk**: Very Low

---

## Phase 3: Dead Code Removal

All items are safe to remove. No behavioral changes.

### 3.1 Remove unused #defines
- `main.h:35` — `EDIT_MENU_UNDO_ID`
- `main.h:37` — `EDIT_MENU_CUT_ID`
- `main.h:40` — `EDIT_MENU_CLEAR_ID`
- `terminal.h:55` — `CELL_IS_NORMAL(a)` macro
- `terminal.h:56` — `CELL_IS_DEC(a)` macro
- `terminal_ui.h:44` — `RIGHT_MARGIN`
- `tcp.c:24` — `OPEN_TIMEOUT`

### 3.2 Remove write-only struct fields
- `terminal.h:208-209` — `snap_rows`, `snap_cols` (written but never read)
- Remove the writes at `terminal.c:733-734`

### 3.3 Remove unused legacy types from tcp.h
- `tcp.h:10-17` — `struct hostInfo`, `NUM_ALT_ADDRS`
- `tcp.h:21-23` — `HostInfoQ`, `HostInfoQPtr`, `HostInfoQHandle`

### 3.4 Remove commented-out code
- `tcp.c:400` — commented `userDataPtr` assignment

### 3.5 Remove unnecessary #includes (build-verify first)
- `finger.c:19`, `connection.c:19`, `menus.c:7`, `dialogs.c:7` — `<Fonts.h>` (try removing, verify build)
- `savefile.c:8` — `<Memory.h>` (try removing, verify build)
- `dialogs.c:14` — `<Resources.h>` (try removing, verify build)

---

## Phase 4: Performance Optimizations

Ordered by impact on 68000 @ 8MHz. All affect both mono and color.

### 4.1 Replace TCPiopb memset with explicit field init
- **Files**: `tcp.c:161, 189, 254` (_TCPSend, _TCPRcv, _TCPStatus)
- **What**: Full `memset(pb, 0, sizeof(*pb))` (~100+ bytes) on every call. During bulk transfer, _TCPStatus + _TCPRcv happen up to 16x per tick = ~12,800 wasted cycles/tick.
- **Fix**: Replace memset with explicit assignment of only the 6-7 fields MacTCP needs. Zero only csCode, ioCompletion, ioResult, ioCRefNum, tcpStream, and the relevant csParam union members.
- **Risk**: Medium — must ensure all MacTCP fields are properly initialized. Needs careful testing.
- **Impact**: Medium — saves ~400 cycles × 32 calls/tick = ~12,800 cycles/tick during bulk data

### 4.2 Direct dirty[] access in term_ui_draw()
- **File**: `terminal_ui.c:736-741, 832-833`
- **What**: `terminal_is_row_dirty()` function call for every row, twice per frame. 50 rows × 2 × ~40 cycles = 4,000 cycles/frame.
- **Fix**: Access `term->dirty[row]` directly in the rendering loop (struct is public, same compilation context).
- **Risk**: Very Low
- **Impact**: Low-Medium

### 4.3 Use precomputed run_x instead of col_left() calls
- **File**: `terminal_ui.c:2266, 2313, 2318, 2330, 2335, 2367, 2369, 2380`
- **What**: `col_left(run_start)` called in text rendering, but `run_x` already holds the same value.
- **Fix**: Replace `col_left(run_start)` with `run_x` in all MoveTo calls.
- **Risk**: Very Low
- **Impact**: Low-Medium

### 4.4 Track has_non_space during run collection
- **File**: `terminal_ui.c:1977-1986 (color), 2012-2022 (mono)`
- **What**: After collecting a run, scans entire run again to check all-spaces. Redundant.
- **Fix**: Set a `has_non_space` flag during run collection when `cell->ch != ' '`.
- **Risk**: Very Low
- **Impact**: Low

### 4.5 Only zero params[0] on 8-bit CSI entry
- **File**: `terminal.c:434`
- **What**: `memset(term->params, 0, sizeof(term->params))` (32 bytes) on every 8-bit CSI. The ESC[ path only zeroes `params[0]`.
- **Fix**: Match ESC[ path: `term->params[0] = 0; term->num_params = 0;`
- **Risk**: Very Low
- **Impact**: Low

---

## Phase 5: Code Refactoring — Shared Code & Organization

### 5.1 Consolidate extern declarations
- **What**: `extern FlynnPrefs prefs` appears in 5 .c files, `extern Session *active_session` in 7 files. Functions already declared in headers are re-declared via extern in .c files.
- **Fix**: Remove all ad-hoc extern declarations from .c files. Ensure each .c includes the proper header. Add missing `#include "menus.h"` to dialogs.c if needed.
- **Risk**: Very Low
- **Effort**: Small

### 5.2 Extract show_error_alert() utility
- **What**: `ParamText() + StopAlert()` pattern appears 10+ times across savefile.c, dialogs.c, finger.c.
- **Fix**: Add `void show_error_alert(const char *msg)` to macutil.c/h. Replace all bare ParamText/StopAlert pairs.
- **Risk**: Very Low
- **Effort**: Small

### 5.3 Extract session_destroy_all()
- **What**: Session teardown loop duplicated in main.c:729-738 and menus.c:426-436.
- **Fix**: Add `void session_destroy_all(void)` to session.c/h. Call from both locations.
- **Risk**: Very Low
- **Effort**: Small

### 5.4 Share cell_to_char() between clipboard.c and savefile.c
- **What**: clipboard.c reimplements cell-to-character conversion inline; savefile.c has a clean `cell_to_char()` function.
- **Fix**: Export `cell_to_char()` from savefile.h or move to macutil.c. Have clipboard.c call it.
- **Risk**: Very Low
- **Effort**: Small

### 5.5 Export session_update_scrollbar()
- **What**: Static function in main.c, but similar scrollbar update patterns appear in input.c and finger.c.
- **Fix**: Move to session.c/h (or a session_ui module). Replace duplicated scrollbar update patterns.
- **Risk**: Very Low
- **Effort**: Small

### 5.6 Extract font metrics initialization guard
- **What**: Identical `if (g_cell_width == 0) { OpenPort(); term_ui_set_font(); ClosePort(); }` in dialogs.c:715-727 and finger.c:171-181.
- **Fix**: Extract `term_ui_ensure_metrics()` to terminal_ui.c/h.
- **Risk**: Low
- **Effort**: Small

### 5.7 Extract terminal type popup helper
- **What**: Popup menu creation/handling duplicated between connect_dlg_filter and bme_dlg_filter (~50 lines each).
- **Fix**: Extract `show_ttype_popup()` helper in dialogs.c.
- **Risk**: Low
- **Effort**: Medium

### 5.8 Extract session_destroy_and_fixup()
- **What**: Session destroy + active_session cleanup pattern repeated 3 times (dialogs.c ×2, finger.c ×1).
- **Fix**: Add `void session_destroy_and_fixup(Session *s)` to session.c/h.
- **Risk**: Low
- **Effort**: Small

### 5.9 Move dialog item constants to dialogs.h
- **What**: DLOG_* constants in connection.h:28-42 and main.h:93-137 are only used by dialogs.c and finger.c.
- **Fix**: Move to dialogs.h (dialog-specific) and finger.h (finger-specific). Keep menu IDs in main.h.
- **Risk**: Very Low
- **Effort**: Small

### 5.10 Replace manual c2pstr conversions
- **What**: Manual C-to-Pascal string conversion in connection.c:107-111 when c2pstr() exists in macutil.c.
- **Fix**: Use `c2pstr()` from macutil.c.
- **Risk**: Very Low
- **Effort**: Small

### 5.11 Table-driven ctrl key handler
- **What**: menus.c:496-543 has 7 switch cases doing identical `ctrl_byte = 0xNN; conn_send(...)`.
- **Fix**: Replace with static lookup table.
- **Risk**: Very Low
- **Effort**: Small

---

## Implementation Order

1. **Phase 1** (Security) — Do first, minimal risk
2. **Phase 2** (Memory leaks) — Do second, minimal risk
3. **Phase 3** (Dead code) — Do third, clean removal, build-verify
4. **Phase 4** (Performance) — Do fourth, test each change on real hardware if possible
5. **Phase 5** (Refactoring) — Do last, highest risk of regressions, most testing needed

Each phase should be a separate commit for easy bisection if issues arise.

---

## Risk Assessment

| Phase | Risk Level | Rollback Difficulty |
|-------|-----------|-------------------|
| Phase 1: Security | Very Low | Trivial (2 small changes) |
| Phase 2: Memory leaks | Very Low | Trivial (3 small changes) |
| Phase 3: Dead code | Very Low | Trivial (removals only) |
| Phase 4: Performance | Low-Medium | Easy (isolated changes) |
| Phase 5: Refactoring | Low-Medium | Moderate (cross-file changes) |

**Total estimated changes**: ~30 discrete modifications across ~15 files
**Lines added**: ~50 (helpers, guards)
**Lines removed**: ~150 (dead code, duplication)
**Net**: ~100 fewer lines of code
