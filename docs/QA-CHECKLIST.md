# Flynn QA Checklist — Performance & Security Review

**Branch**: `perf-security-review`
**Date**: 2026-03-16

Use this checklist to verify Flynn functionality after the perf/security review changes.
Test on **both** System 6.0.8 (Mac Plus / Snow) and System 7.5.5 (Basilisk II) unless noted.

---

## Legend

- [ ] = Not tested
- [P] = Pass
- [F] = Fail (note details)
- **S6** = System 6.0.8 (Mac Plus, monochrome)
- **S7** = System 7.5.5 (color)

---

## 1. Launch & Startup

| # | Test | S6 | S7 | Notes |
|---|------|----|----|-------|
| 1.1 | App launches without crash | [ ] | [ ] | |
| 1.2 | Connect dialog appears on startup | [ ] | [ ] | |
| 1.3 | Menus render correctly (Chicago 12 font) | [ ] | [ ] | Regression: WMgr port font corruption |
| 1.4 | Title bar text is correct ("Flynn") | [ ] | [ ] | |
| 1.5 | About Flynn dialog displays correctly | [ ] | [ ] | |
| 1.6 | App does not crash on immediate Quit | [ ] | [ ] | Phase 2.2: term_ui_cleanup() at exit |

## 2. Connection & DNS

| # | Test | S6 | S7 | Notes |
|---|------|----|----|-------|
| 2.1 | Connect to telnet host by hostname | [ ] | [ ] | Tests DNS resolver |
| 2.2 | Connect to telnet host by IP address | [ ] | [ ] | |
| 2.3 | Connection failure shows error alert | [ ] | [ ] | Phase 5.2: show_error_alert() refactor |
| 2.4 | DNS failure shows error alert | [ ] | [ ] | |
| 2.5 | Cancel during connection works | [ ] | [ ] | |
| 2.6 | Connect timeout (~30s) works with no network | [ ] | [ ] | |
| 2.7 | Remote disconnect shows alert and clears screen | [ ] | [ ] | |
| 2.8 | Reconnect after disconnect works | [ ] | [ ] | |

## 3. Terminal Rendering

| # | Test | S6 | S7 | Notes |
|---|------|----|----|-------|
| 3.1 | Text displays correctly at 80x24 | [ ] | [ ] | |
| 3.2 | Bold text renders without spacing drift | [ ] | [ ] | Per-character MoveTo+DrawChar |
| 3.3 | Underline, reverse video, blink work | [ ] | [ ] | |
| 3.4 | Run `htop` — UI renders cleanly | [ ] | [ ] | Box-drawing chars, colors |
| 3.5 | Run `vim` — full-screen app works | [ ] | [ ] | Alt screen, cursor positioning |
| 3.6 | Run `nano` — editor displays correctly | [ ] | [ ] | |
| 3.7 | Run `tmux` — panes render correctly | [ ] | [ ] | |
| 3.8 | Run `man ls` — scrolling pager works | [ ] | [ ] | |
| 3.9 | Tab stops work correctly (hit Tab key) | [ ] | [ ] | |
| 3.10 | Cursor blink works in idle state | [ ] | [ ] | |
| 3.11 | No screen flicker during normal typing | [ ] | [ ] | Double-buffer rendering |
| 3.12 | `cat /dev/urandom` — no crash on random bytes | [ ] | [ ] | Phase 1.1: OSC response clamp |
| 3.13 | Unicode glyphs render (emoji, box-drawing) | [ ] | [ ] | |
| 3.14 | Colors render correctly (S7 only) | N/A | [ ] | 256-color support |

## 4. Scrolling & Scrollback

| # | Test | S6 | S7 | Notes |
|---|------|----|----|-------|
| 4.1 | Scroll bar appears and tracks position | [ ] | [ ] | |
| 4.2 | Click scroll bar arrows — scrolls 1 line | [ ] | [ ] | Phase 2.1: UPP leak fix |
| 4.3 | Click scroll bar page area — scrolls page | [ ] | [ ] | Phase 2.1: UPP leak fix |
| 4.4 | Drag scroll bar thumb — scrolls freely | [ ] | [ ] | |
| 4.5 | Scroll back through history (96 lines) | [ ] | [ ] | |
| 4.6 | New output auto-scrolls to bottom | [ ] | [ ] | |
| 4.7 | Repeated scroll bar clicking — no memory growth | [ ] | [ ] | Phase 2.1: UPP leak regression test |
| 4.8 | Jump scroll works on bulk data (e.g., `ls -la /usr`) | [ ] | [ ] | |

## 5. Keyboard Input

| # | Test | S6 | S7 | Notes |
|---|------|----|----|-------|
| 5.1 | Normal typing echoes correctly | [ ] | [ ] | |
| 5.2 | Arrow keys work (up/down/left/right) | [ ] | [ ] | |
| 5.3 | Backspace/Delete works | [ ] | [ ] | |
| 5.4 | Cmd+. sends Escape | [ ] | [ ] | M0110 keyboard |
| 5.5 | Option+letter sends Ctrl sequence | [ ] | [ ] | M0110 keyboard |
| 5.6 | Function keys via numpad work | [ ] | [ ] | |
| 5.7 | Rapid typing — no dropped characters | [ ] | [ ] | Keystroke batching |
| 5.8 | Ctrl menu items send correct bytes | [ ] | [ ] | Phase 5.11: table-driven ctrl handler |

## 6. Multi-Session

| # | Test | S6 | S7 | Notes |
|---|------|----|----|-------|
| 6.1 | Open 2nd session (File > New Connection) | [ ] | [ ] | |
| 6.2 | Open 3rd and 4th sessions | [ ] | [ ] | MAX_SESSIONS=4 |
| 6.3 | Switch between sessions via Window menu | [ ] | [ ] | |
| 6.4 | Switch sessions via clicking windows | [ ] | [ ] | |
| 6.5 | Each session renders independently | [ ] | [ ] | No cross-session font corruption |
| 6.6 | Different fonts per session work | [ ] | [ ] | Per-session font metrics |
| 6.7 | Close one session, others continue | [ ] | [ ] | Phase 5.8: session_destroy_and_fixup() |
| 6.8 | Close all sessions, app remains running | [ ] | [ ] | |
| 6.9 | Quit with multiple sessions connected | [ ] | [ ] | Phase 5.3: session_destroy_all() |

## 7. Clipboard

| # | Test | S6 | S7 | Notes |
|---|------|----|----|-------|
| 7.1 | Select text with mouse drag | [ ] | [ ] | |
| 7.2 | Edit > Copy (Cmd+C) copies text | [ ] | [ ] | Phase 5.4: shared cell_to_char() |
| 7.3 | Edit > Paste (Cmd+V) sends text | [ ] | [ ] | |
| 7.4 | Edit > Select All works | [ ] | [ ] | |
| 7.5 | Copy from one session, paste to another | [ ] | [ ] | |
| 7.6 | Copy/Paste menu items enable/disable correctly | [ ] | [ ] | update_menus() before MenuSelect |

## 8. Bookmarks & Settings

| # | Test | S6 | S7 | Notes |
|---|------|----|----|-------|
| 8.1 | Preferences > Bookmarks opens manager | [ ] | [ ] | |
| 8.2 | Add a new bookmark | [ ] | [ ] | |
| 8.3 | Edit existing bookmark | [ ] | [ ] | |
| 8.4 | Delete a bookmark | [ ] | [ ] | |
| 8.5 | Connect via bookmark (Connect dialog popup) | [ ] | [ ] | |
| 8.6 | Per-bookmark terminal type saved | [ ] | [ ] | Phase 5.7: ttype popup refactor |
| 8.7 | Per-bookmark font saved | [ ] | [ ] | |
| 8.8 | Per-bookmark username saved | [ ] | [ ] | |
| 8.9 | Recent bookmarks appear in File menu | [ ] | [ ] | |
| 8.10 | Settings persist across quit/relaunch | [ ] | [ ] | FlynnPrefs v8 |
| 8.11 | Preferences > DNS Server dialog works | [ ] | [ ] | |
| 8.12 | Preferences > Font changes work per-session | [ ] | [ ] | |
| 8.13 | Preferences > Terminal Type changes work | [ ] | [ ] | |
| 8.14 | Dark mode toggle works | [ ] | [ ] | |

## 9. Finger Protocol

| # | Test | S6 | S7 | Notes |
|---|------|----|----|-------|
| 9.1 | File > Finger opens finger dialog | [ ] | [ ] | |
| 9.2 | Finger query returns results | [ ] | [ ] | Phase 1.2: finger_host null-term |
| 9.3 | Finger results display in window | [ ] | [ ] | |
| 9.4 | Finger host@host@gateway forwarding works | [ ] | [ ] | |
| 9.5 | Finger dialog remembers last host/user | [ ] | [ ] | |

## 10. Save/File Operations

| # | Test | S6 | S7 | Notes |
|---|------|----|----|-------|
| 10.1 | File > Save Session saves to file | [ ] | [ ] | |
| 10.2 | Save error shows alert (e.g., disk full) | [ ] | [ ] | Phase 5.2: show_error_alert() |
| 10.3 | Saved file content matches screen | [ ] | [ ] | |

## 11. Performance Verification

| # | Test | S6 | S7 | Notes |
|---|------|----|----|-------|
| 11.1 | `cat large_file.txt` — smooth scrolling | [ ] | [ ] | Jump scroll, drain loop |
| 11.2 | `ls -laR /` — bulk output doesn't freeze UI | [ ] | [ ] | Phase 4.1: TCPiopb memset |
| 11.3 | Typing during bulk output — responsive | [ ] | [ ] | |
| 11.4 | Scroll bar responsive during data transfer | [ ] | [ ] | |
| 11.5 | No visible rendering artifacts (tearing, flash) | [ ] | [ ] | Double-buffer, shadow buffer |
| 11.6 | Idle CPU usage is minimal (no busy-wait) | [ ] | [ ] | WaitNextEvent 1-tick timeout |

## 12. Edge Cases & Stress

| # | Test | S6 | S7 | Notes |
|---|------|----|----|-------|
| 12.1 | Open/close sessions rapidly (10 cycles) | [ ] | [ ] | Memory leak regression |
| 12.2 | Connect to non-existent host — timeout | [ ] | [ ] | |
| 12.3 | Disconnect during data transfer | [ ] | [ ] | |
| 12.4 | Very long lines (>132 chars) | [ ] | [ ] | Column wrapping |
| 12.5 | Binary data doesn't crash terminal parser | [ ] | [ ] | |
| 12.6 | Resize window (if supported) | [ ] | [ ] | NAWS negotiation |
| 12.7 | 4 sessions simultaneously active | [ ] | [ ] | Memory ceiling (~292KB + session overhead) |

## 13. Regression Checks (Phase-Specific)

These tests specifically verify areas changed by the review.

| # | Test | Phase | S6 | S7 | Notes |
|---|------|-------|----|----|-------|
| 13.1 | OSC color query response (xterm-256color) | 1.1 | N/A | [ ] | `printf '\e]4;1;?\e\\'` |
| 13.2 | Finger prefs persist after save/reload | 1.2 | [ ] | [ ] | Set finger host, quit, relaunch |
| 13.3 | Scroll bar: 50 clicks without memory growth | 2.1 | [ ] | [ ] | Watch heap via About box or similar |
| 13.4 | Clean exit — no crash on Quit | 2.2 | [ ] | [ ] | |
| 13.5 | Session creation fails gracefully (4+ sessions) | 2.3 | [ ] | [ ] | |
| 13.6 | Build succeeds with removed #includes | 3.5 | [ ] | [ ] | Build verification only |
| 13.7 | Bulk data throughput (compare before/after) | 4.1 | [ ] | [ ] | `time cat /etc/services` |
| 13.8 | Error alerts show correctly from all paths | 5.2 | [ ] | [ ] | Invalid host, save errors |
| 13.9 | Quit with 4 connected sessions — clean exit | 5.3 | [ ] | [ ] | |
| 13.10 | Copy text matches original screen content | 5.4 | [ ] | [ ] | Shared cell_to_char() |

---

## Test Environment

### System 6 (Mac Plus — Snow Emulator)
- ROM: Macintosh Plus v3
- System: 6.0.8
- Network: MacTCP 2.1, DaynaPORT SCSI/Link (NAT)
- RAM: 4 MB
- Display: 512×342, monochrome

### System 7 (Basilisk II or real hardware)
- System: 7.5.5
- Network: Open Transport or MacTCP
- RAM: 8+ MB
- Display: Color (256-color or higher)

---

## Notes

- Test System 6 first — it's the primary target and most constrained
- Phase 4.1 (TCPiopb memset) is the highest-risk performance change — test network I/O thoroughly
- Phase 5 refactoring touches many files — test all UI paths after those changes
- If any test fails, note the phase that introduced the regression for easy bisection
