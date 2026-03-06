# Testing Guide

Testing environment for Flynn using classic Macintosh emulators.

## Snow Emulator (Primary)

[Snow](https://snowemu.com/) is a classic Macintosh emulator written in Rust that emulates hardware at a low level. It supports Mac 128K through Mac IIcx/SE/30.

- **Version**: v1.3.1
- **Binary**: Download from [GitHub releases](https://github.com/twvd/snow/releases)
- **Documentation**: [docs.snowemu.com](https://docs.snowemu.com/) (also built locally in `references/snow-docs/`)
- **Source**: [github.com/twvd/snow](https://github.com/twvd/snow)

### Quick Start

```bash
# Launch with Mac Plus ROM and System 6.0.8 floppy
snowemu /path/to/MacPlus.ROM --floppy "/path/to/System Tools.img"
```

### ROM Files

Snow validates ROMs against known checksums. For Mac Plus emulation:
- **Mac Plus v3 ROM** (128KB): `1986-03 - 4D1F8172 - MacPlus v3.ROM`
- Located in: `roms/68k/128k/Macintosh Plus/`

For Mac II emulation (needed for networking):
- **Mac II FDHD ROM** (256KB) + **Display Card ROM** (`341-0868.bin`)
- Mac II supports SCSI Ethernet (DaynaPORT) for MacTCP networking

### Storage

**Hard drives**: Snow uses SCSI device images (full drive images including partition table, not just volume images). Create via `Drives > SCSI #n > Create new HDD image...`. Initialize with Apple HD SC Setup from a boot floppy.

**Floppies**: Supports raw sector images, DiskCopy 4.2, MOOF, A2R flux. Load via `Drives > Drive n > Load image`.

### Networking

Snow emulates a DaynaPORT SCSI/Link Ethernet adapter for TCP/IP:

1. Attach adapter: `Drives > SCSI #n > Attach Ethernet adapter` (recommend SCSI ID 3)
2. Install DaynaPORT drivers (v7.5.3 recommended) in guest
3. Install MacTCP 2.0.6+ in guest
4. NAT mode: gateway 10.0.0.1, guest IP in 10.0.0.0/8 range
5. Configure DNS manually in MacTCP (e.g., 8.8.8.8)
6. Also supports HTTPS stripping for browsing modern sites

### File Sharing

Snow supports BlueSCSI Toolbox protocol:
1. Select host folder: `Tools > File Sharing > Select folder...`
2. In guest: use BlueSCSI SD Transfer tool (or `Tools > File Sharing > Insert toolbox floppy`)

### Keyboard

- **Command key**: Right ALT on Linux
- **Mouse**: Absolute positioning by default (patches Mac globals)
- Keyboard layout: Set to "U.S." in guest Control Panel for best results

### Workspaces

Save/load entire emulator state (machine type, ROMs, disks, window layout) via `Workspace` menu. Paths are relative to workspace file.

## System 6.0.8 Floppy Images

Source: [Macintosh Garden](https://macintoshgarden.org/apps/macintosh-system-6x) (`System_6.x_2.sit`)

### Standard System 6.0.8 (800K floppies, 4 disks)
- System Tools, Printing Tools, Utilities 1, Utilities 2
- Format: Apple DiskCopy 4.2 → extract raw with `dd bs=1 skip=84 count=819200`
- Works on Mac Plus, SE, Mac II in 24-bit addressing mode

### System 6.0.8L (1.4MB floppies, 2 disks)
- System Startup, System Additions
- Supports 32-bit addressing (for Classic, Classic II, LC, LC II, Powerbook 100 ONLY)
- **Does NOT work with Mac IIsi/IIci/IIcx** in Basilisk II

### Extracting from DiskCopy 4.2

```bash
# DiskCopy 4.2: 84-byte header + raw data + tag bytes
dd if="System Tools.image" of="System Tools.img" bs=1 skip=84 count=819200
```

### HFS Tools

`hfsutils` (built from [Distrotech/hfsutils](https://github.com/Distrotech/hfsutils)):
```bash
hmount image.img    # Mount HFS volume
hls -la             # List files
hcopy file.bin :    # Copy file to volume
humount             # Unmount
hformat -l "Name" image.hda  # Format blank image as HFS
```

## Basilisk II (Deprecated)

Basilisk II V1.1 (kanjitalk755/macemu fork) was tested extensively but has critical issues with System 6.0.8.

### Problem: 32-Bit Addressing

All 512KB ROMs (IIsi, IIci, IIfx, LC, Classic II, LC II) are `ROM_VERSION_32` in Basilisk II source (`rom_patches.h`), which forces `TwentyFourBitAddressing = false`. System 6.0.8 detects 32-bit mode and shows a dialog requiring user to click "24-Bit" button.

The 256KB Mac II FDHD ROM is `ROM_VERSION_II` (24-bit) but Basilisk II rejects it as "Unsupported ROM type".

### Problem: Synthetic Mouse Events

Basilisk II uses X11 directly (not SDL). All attempts to programmatically click the 32-bit addressing dialog failed:

| Method | Result |
|--------|--------|
| `xdotool click` | Cursor moves but click not received |
| XTest extension (`xte`) | Same — click not received |
| `python-xlib` XSendEvent | Events sent but not processed |
| `uinput` virtual mouse | Click not received |
| `uinput` virtual touchscreen | Click not received |

The `XCheckMaskEvent` in Basilisk's event loop (`video_x.cpp:2229`) should receive these events but doesn't register clicks, even though cursor movement works. Root cause unknown.

### Patching Attempt

Patching `main.cpp` to force `TwentyFourBitAddressing = true` for `ROM_VERSION_32` causes SIGSEGV — the 32-bit clean ROM uses 32-bit addresses internally and can't run in 24-bit mode.

### Configuration Reference

```
rom /path/to/Mac IIsi.ROM
disk /path/to/sys608.hda
floppy /path/to/System Tools.img
extfs /path/to/build
screen win/512/384
ramsize 8388608
modelid 5
cpu 2
fpu false
nocdrom true
nosound true
nogui true
ether slirp
```

### Basilisk ROM Version Classification

From `rom_patches.h`:
- `ROM_VERSION_64K` (0x0000): Original Mac 64KB
- `ROM_VERSION_PLUS` (0x0075): Mac Plus 128KB
- `ROM_VERSION_CLASSIC` (0x0276): SE/Classic 256/512KB
- `ROM_VERSION_II` (0x0178): Mac II non-32-bit-clean 256KB → **24-bit mode**
- `ROM_VERSION_32` (0x067c): 32-bit clean 512KB+ → **32-bit mode (incompatible with System 6)**

Check ROM version: `od -A n -t x1 -j 8 -N 2 romfile.ROM`

## Test Target

- Host: `<telnet-host>`
- User: `<username>`
- Password: `<password>`
- Service: telnetd (standard Linux)

## SCSI Hard Drive Images

Snow requires full drive images with Apple partition maps (DDM + driver + HFS), not raw HFS volumes.

**Creating a bootable drive:**
1. Download a pre-formatted blank HFS image from [savagetaylor.com](https://www.savagetaylor.com/wp-content/uploads/68k_Macintosh/Bootdisks/blank_100MB_HFS.zip)
2. Attach as SCSI target in workspace or via Drives menu
3. Boot from System Tools floppy, run Installer to install System 6.0.8
4. The Installer reads from 4 floppy disks: System Tools, Utilities 2, Utilities 1, Printing Tools

Current image: `diskimages/snow-sys608.img` (90MB) with System 6.0.8 installed.

## Display Setup

The dev system runs WindowMaker (wmaker) as window manager on X11 (DISPLAY=:0). KDE Plasma was replaced because it doesn't support `_NET_ACTIVE_WINDOW`, which breaks `xdotool windowactivate`.

```bash
# Start WindowMaker
DISPLAY=:0 wmaker &

# Allow local X connections
DISPLAY=:0 xhost +local:
```

## GUI Automation

Snow can be fully automated via X11 tools for testing. See `docs/DEVLOG.md` for the full System 6.0.8 installation done this way.

**Key rules:**
- Use `xdotool mousedown 1` + `xdotool mouseup 1` (NOT `xdotool click`)
- Double-click: two mousedown/mouseup pairs with ~100ms gap
- Screenshot pixel coords = X11 screen coords (1:1 at 1280x800)
- Mac menus: press-hold (mousedown, wait, mouseup)
- Snow host menus (egui): regular click

```bash
# Focus Snow window
SNOW_WID=$(DISPLAY=:0 xdotool search --name "Snow v1.3.1" | head -1)
DISPLAY=:0 xdotool windowactivate --sync $SNOW_WID

# Click at a position
DISPLAY=:0 xdotool mousemove <x> <y>
DISPLAY=:0 xdotool mousedown 1 && sleep 0.05 && DISPLAY=:0 xdotool mouseup 1

# Double-click (e.g., open an icon)
DISPLAY=:0 xdotool mousedown 1 && sleep 0.05 && DISPLAY=:0 xdotool mouseup 1
sleep 0.1
DISPLAY=:0 xdotool mousedown 1 && sleep 0.05 && DISPLAY=:0 xdotool mouseup 1

# Load floppy via Drives menu
# 1. Click Drives menu, hover Floppy #1, click Load image...
# 2. In file dialog: click edit icon, Ctrl+A, type path, Enter
```

## Screenshots

**Use `scrot` for screenshots** (not `import -window root`, which permanently breaks Snow mouse input by grabbing the X pointer):
```bash
DISPLAY=:0 scrot screenshot.png                        # Full desktop
DISPLAY=:0 scrot -u screenshot.png                     # Focused window
```

Find window IDs: `xwininfo -root -tree | grep -i <name>`

## Deployment Workflow

Build Flynn, deploy to the HDD image, and boot in Snow:

```bash
# 1. Build
./build.sh

# 2. Deploy to HDD image using hfsutils
hmount diskimages/snow-sys608.img
hcopy -m build/Flynn.bin :Flynn
hattrib -t APPL -c FLYN :Flynn
humount

# 3. Boot Snow
DISPLAY=:0 tools/snow/snowemu diskimages/flynn.snoww &
```

Requires `hfsutils` (built from [Distrotech/hfsutils](https://github.com/Distrotech/hfsutils)). The `-m` flag on `hcopy` preserves MacBinary resource/data fork encoding. The `hattrib` step sets the file type to `APPL` and creator to `FLYN` so System 6 recognizes it as a launchable application.

## Troubleshooting

### X11 Display Becomes Unresponsive

X11 (DISPLAY=:0) can become unresponsive, causing `xdpyinfo` to hang and `scrot` to fail with "Can't open X display". This is a known failure mode on the dev system.

**Symptoms:**
- `DISPLAY=:0 xdpyinfo` hangs indefinitely
- `DISPLAY=:0 scrot screenshot.png` fails with "Can't open X display"
- Snow cannot be launched

**Fix:**
```bash
# Restart the display manager (restarts the X server)
sudo systemctl restart sddm

# After restart, allow local X connections
DISPLAY=:0 xhost +local:

# Disable screensaver/DPMS
DISPLAY=:0 xset s off
DISPLAY=:0 xset -dpms
DISPLAY=:0 xset s noblank

# Snow can now be launched normally
DISPLAY=:0 tools/snow/snowemu diskimages/flynn.snoww &
```
