# Development Log

## 2026-03-05: Phase 5 — Terminal UI and TCP Fix

### Terminal UI Renderer (terminal_ui.c/terminal_ui.h)

New dedicated rendering module. Monaco 9pt font, 6×12 character cells, 2px margins. Per-row dirty flags for efficient partial redraw. Attribute runs scanned per row — one `DrawText()` per contiguous span. Bold/underline via `TextFace()`, inverse via `PaintRect` + `srcBic`. XOR block cursor blinks every ~30 ticks.

### Keyboard Mapping (main.c handle_key_down)

Full VT100 keyboard mapping: arrows → `ESC[A`–`ESC[D`, Home/End/PgUp/PgDn/FwdDel, Delete→DEL (0x7F), Ctrl+key→control chars (`& 0x1F`), Cmd+key preserved for menus. Retro68 header uses `ControlKey` not `controlKey`.

### TCP Blocking Bug Fix (tcp.c)

`_TCPStatus()` was missing `memset(pb, 0, sizeof(*pb))` — the only TCP function without it. Stale parameter block data caused bad `amtUnreadData` values. `_TCPRcv()` then blocked for 30 seconds (`commandTimeoutValue`), hanging the entire event loop. Fixed by adding memset and reducing timeout to 1 second.

### Drawing Strategy

Switched from `InvalRect()` → `updateEvt` to direct `term_ui_draw()` in the null event handler. The update event path had timing issues with dirty flags being cleared before the handler read them.

### Test Results

All 14 scenarios pass: launch, connect, login, echo, arrows, Ctrl+C, ls --color, nano, vi, disconnect, quit. Build size: 64KB.

---

## 2026-03-05: Telnet Protocol Engine and VT100 Terminal

### Retro68 Toolchain Rebuild

Rebuilt Retro68 from source in-repo (`Retro68-build/`) with `--no-ppc --no-carbon` for 68K-only. GCC 12.2.0 with newer Multiverse.h headers required extensive compatibility fixes to the wallops-146 code (MacTCP.h enum guards, API renames, missing headers). See `memory/retro68-compat.md` for full details.

### Telnet Protocol Engine (telnet.c/telnet.h)

Client-side IAC negotiation engine adapted from subtext-596's server-side implementation. 9-state parser handles: ECHO, SGA, BINARY, TTYPE ("VT100"), NAWS (80x24), TSPEED ("19200,19200"). Transport-independent — processes byte arrays, no MacTCP coupling.

### VT100 Terminal Emulator (terminal.c/terminal.h)

Full escape sequence parser with 5-state CSI machine. 80x24 screen buffer (2 bytes/cell) + 96-line scrollback ring buffer. Supports cursor movement, screen/line clear, insert/delete, scroll regions, text attributes. ~19KB total struct size.

### Integration

Data pipeline wired in main.c: `conn_idle() → telnet_process() → terminal_process() → InvalRect`. IAC responses sent back immediately. Basic Monaco 9pt text rendering for display. Build size: 61KB.

---

## 2026-03-05: System 6.0.8 Installation via GUI Automation

### SCSI Hard Drive Setup

Created a SCSI hard drive image for the Snow emulator. Snow expects full drive images with Apple partition maps, not just raw HFS volumes.

**Failed approaches:**
1. `hformat` creates raw HFS without Apple partition map — Mac Plus ROM can't mount
2. Custom Python script with DDM + partition map but no driver — Mac can see partitions but not mount
3. Extracting SCSI driver from Apple HD SC Setup — the resource fork contains app init code, not the standalone disk driver

**Working approach:** Downloaded a pre-formatted blank HFS disk image from savagetaylor.com that includes proper DDM (Driver Descriptor Map), Apple_Driver43 partition with SCSI driver binary, and Apple_HFS partition. The image is 90MB.

- Source: `https://www.savagetaylor.com/wp-content/uploads/68k_Macintosh/Bootdisks/blank_100MB_HFS.zip`
- File: `diskimages/snow-sys608.img` (94,371,840 bytes)
- Contains: DDM (sig 0x4552), Apple_Driver43 at blocks 64-95, Apple_HFS at blocks 96+
- Volume name: "Blank 100MB"

### Snow Workspace Configuration

Snow workspace files use the `.snoww` extension (two w's — discovered by reading source at `app.rs:437`). The workspace is a JSON file with paths relative to its parent directory.

Final workspace (`diskimages/flynn.snoww`):
```json
{
  "viewport_scale": 1.5,
  "rom_path": "../roms/68k/128k/Macintosh Plus/1986-03 - 4D1F8172 - MacPlus v3.ROM",
  "scsi_targets": [{"Disk": "snow-sys608.img"}, "None", "None", "None", "None", "None", "None"],
  "model": "Plus",
  "map_cmd_ralt": true,
  "scaling_algorithm": "NearestNeighbor",
  "framebuffer_mode": "Centered",
  "shader_configs": []
}
```

### GUI Automation Discovery

Automated the entire System 6.0.8 installation using X11 tools (`xdotool`, `import`) to interact with the Snow emulator GUI programmatically.

**Key findings:**
- **Window Manager matters:** KDE Plasma doesn't support `_NET_ACTIVE_WINDOW`, breaking `xdotool windowactivate`. Switched to **WindowMaker** (`wmaker`) which works perfectly.
- **`xdotool click` doesn't work with Snow/egui.** Must use explicit `xdotool mousedown 1` + `xdotool mouseup 1` as separate commands.
- **Double-click works** with two mousedown/mouseup pairs separated by ~100ms.
- **Screenshot coordinates = X11 screen coordinates** (1:1 mapping at 1280x800 display).
- **Mac menus** use press-hold-release (mousedown, wait, mouseup).
- **Snow host menus (egui)** use regular clicks.

### Installation Process

1. Launched Snow with workspace + System Tools floppy
2. Mac booted from floppy, showing Finder with System Tools and Blank 100MB drives
3. Double-clicked System Tools floppy icon to open it
4. Double-clicked Installer app inside
5. Apple Installer launched — "Welcome to the Apple Installer" screen
6. Clicked OK → Easy Install screen showing "Version 6.0.8, Macintosh Plus System Software"
7. Target: "Blank 100MB" hard drive
8. Clicked Install → installation began
9. Installer read from System Tools, then requested additional disks:
   - Utilities 2 → loaded via Drives > Floppy #1 > Load image...
   - Utilities 1 → loaded same way
   - Printing Tools → loaded same way
   - System Tools → reloaded for final "Updating disk..."
10. "Installation on 'Blank 100MB' was successful!"
11. Quit Installer, verified hard drive now contains System Folder

### Screenshots

| Screenshot | Description |
|-----------|-------------|
| `03-installer-welcome.png` | Apple Installer welcome screen |
| `04-easy-install.png` | Easy Install — Version 6.0.8, target Blank 100MB |
| `05-installer-reading-system.png` | Reading files from System Tools disk |
| `06-install-successful.png` | Installation successful dialog |
| `07-system608-installed.png` | Blank 100MB drive contents — Desktop Folder, System Folder, Trash |

### Next Steps

- Boot from HDD without floppy (verify standalone boot)
- Install MacTCP 2.1 for networking
- Attach DaynaPORT SCSI/Link Ethernet adapter
- Test Flynn skeleton app in the emulator
