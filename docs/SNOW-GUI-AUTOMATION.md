# Snow Emulator GUI Automation Guide

Automating the Snow (v1.3.1) classic Mac emulator via X11 for testing.

## Quick Reference

| Action | Works? | Method |
|--------|--------|--------|
| Single click | YES | XTEST incremental motion + jiggle + click |
| Double-click | PARTIAL | Works with xdotool mousedown/mouseup pairs; python-xlib XTEST needs Cmd+O workaround |
| Keyboard shortcuts | YES | XTEST KeyPress/KeyRelease (window must have focus) |
| Menu click-hold-drag | YES | XTEST ButtonPress, incremental drag, ButtonRelease |
| Text typing | YES | XTEST key events (window must have focus) |

## Coordinate System

### Window Layout (viewport_scale=1.5, window 1000x750)

```
Snow Window (1000x750)
+--------------------------------------------------+
| Snow egui menu bar                                |  y=0..20
+--------------------------------------------------+
| Snow toolbar                                      |  y=21..57
+--------------------------------------------------+
|                                                    |
|   +--------------------------------------------+  |
|   | Mac Plus framebuffer (768x513 = 512x342@1.5)|  | y=158..670
|   | x=116..883                                   |  |
|   +--------------------------------------------+  |
|                                                    |
+--------------------------------------------------+
```

### Conversion Formulas

```python
FB_LEFT = 116   # Framebuffer left edge in screen coords
FB_TOP = 158    # Framebuffer top edge in screen coords
SCALE = 1.5     # viewport_scale from .snoww config

def mac_to_screen(mac_x, mac_y):
    """Mac coords (0-511, 0-341) -> X11 screen coords"""
    return (int(FB_LEFT + mac_x * SCALE),
            int(FB_TOP + mac_y * SCALE))

def screen_to_mac(screen_x, screen_y):
    """X11 screen coords -> Mac coords"""
    return ((screen_x - FB_LEFT) / SCALE,
            (screen_y - FB_TOP) / SCALE)
```

### Key Mac UI Positions

| Element | Mac Coords | Screen Coords |
|---------|-----------|---------------|
| Apple menu | (12, 8) | (134, 170) |
| File menu | (54, 8) | (197, 170) |
| Edit menu | (93, 8) | (256, 170) |
| View menu | (132, 8) | (314, 170) |
| Special menu | (178, 8) | (383, 170) |

## Why Specific Techniques Are Required

### Incremental XTEST Motion (not XWarpPointer)

`XWarpPointer` (used by `xdotool mousemove`) moves the pointer instantly but doesn't reliably generate MotionNotify events that winit/egui processes. XTEST `fake_input(MotionNotify)` with incremental steps (~20 steps, 16ms apart) generates proper events.

### Jiggle After Move

egui's `contains_pointer()` uses the **previous frame's** `Response`. After moving to a new position, the framebuffer widget may not register the pointer until the next frame renders. Small +-1px movements ("jiggle") and a ~100ms wait ensure at least one frame renders with the correct pointer position before clicking.

### Keyboard Focus

XTEST keyboard events go to the **focused** X11 window, unlike mouse events which go to the window under the cursor. Click inside the framebuffer area first to naturally give Snow focus, or use `XSetInputFocus` via python-xlib.

### Double-Click Behavior

**With xdotool**: Double-click WORKS using explicit `mousedown`/`mouseup` pairs:
```bash
xdotool mousedown 1 && sleep 0.05 && xdotool mouseup 1
sleep 0.1
xdotool mousedown 1 && sleep 0.05 && xdotool mouseup 1
```
This was verified by opening System Tools floppy and launching the Installer during System 6 installation.

**With python-xlib XTEST**: Double-click does NOT work — egui likely coalesces two rapid XTEST clicks into a single "double-click" event internally. **Workaround**: single-click to select + `Cmd+O`, or File > Open menu drag.

### Window Manager Requirement

**WindowMaker (wmaker)** is required. KDE Plasma does not support `_NET_ACTIVE_WINDOW`, breaking `xdotool windowactivate`. Install: `sudo apt-get install -y wmaker`

## Complete Python Automation Library

```python
#!/usr/bin/env python3
"""Snow emulator GUI automation via X11 XTEST extension."""

import time
from Xlib import X, XK, display
from Xlib.ext import xtest

class SnowAutomation:
    FB_LEFT = 116
    FB_TOP = 158
    FB_WIDTH = 768
    FB_HEIGHT = 513
    SCALE = 1.5

    def __init__(self, display_name=':0', snow_window_id=None):
        self.d = display.Display(display_name)
        self.root = self.d.screen().root
        if snow_window_id:
            self.snow_win_id = snow_window_id
        else:
            self.snow_win_id = self._find_snow_window()
        self.snow_win = self.d.create_resource_object('window', self.snow_win_id)

    def _find_snow_window(self):
        import subprocess
        result = subprocess.run(
            ['xdotool', 'search', '--name', 'Snow'],
            capture_output=True, text=True,
            env={'DISPLAY': self.d.get_display_name()}
        )
        wids = result.stdout.strip().split('\n')
        if not wids or not wids[0]:
            raise RuntimeError("No Snow window found")
        return int(wids[0])

    def mac_to_screen(self, mac_x, mac_y):
        return (int(self.FB_LEFT + mac_x * self.SCALE),
                int(self.FB_TOP + mac_y * self.SCALE))

    def _xtest_move(self, x, y):
        xtest.fake_input(self.d, X.MotionNotify, 0, X.CurrentTime, self.root, x, y)
        self.d.sync()

    def move_to(self, x, y, steps=20):
        """Move cursor to screen position with incremental XTEST motion."""
        ptr = self.root.query_pointer()
        sx, sy = ptr.root_x, ptr.root_y
        for i in range(1, steps + 1):
            nx = int(sx + (x - sx) * i / steps)
            ny = int(sy + (y - sy) * i / steps)
            self._xtest_move(nx, ny)
            time.sleep(0.016)
        time.sleep(0.1)
        self.jiggle()

    def move_to_mac(self, mac_x, mac_y, steps=20):
        """Move cursor to Mac screen position."""
        sx, sy = self.mac_to_screen(mac_x, mac_y)
        self.move_to(sx, sy, steps)

    def jiggle(self):
        """Small movements to ensure egui pointer tracking updates."""
        ptr = self.root.query_pointer()
        for dx in [1, -1, 1, -1, 0]:
            self._xtest_move(ptr.root_x + dx, ptr.root_y)
            time.sleep(0.016)
        time.sleep(0.1)

    def click(self, delay=0.05):
        """Single click at current position."""
        xtest.fake_input(self.d, X.ButtonPress, 1, X.CurrentTime)
        self.d.sync()
        time.sleep(delay)
        xtest.fake_input(self.d, X.ButtonRelease, 1, X.CurrentTime)
        self.d.sync()

    def click_at(self, x, y):
        """Move to screen position and click."""
        self.move_to(x, y)
        time.sleep(0.3)
        self.click()

    def click_at_mac(self, mac_x, mac_y):
        """Move to Mac position and click."""
        sx, sy = self.mac_to_screen(mac_x, mac_y)
        self.click_at(sx, sy)

    def ensure_focus(self):
        """Give Snow keyboard focus by clicking in the framebuffer."""
        self.move_to(400, 400)
        time.sleep(0.2)
        self.click()
        time.sleep(0.3)

    def key_press(self, keycode):
        """Press and release a key by X11 keycode."""
        xtest.fake_input(self.d, X.KeyPress, keycode, X.CurrentTime)
        self.d.sync()
        time.sleep(0.05)
        xtest.fake_input(self.d, X.KeyRelease, keycode, X.CurrentTime)
        self.d.sync()

    def cmd_key(self, key_char):
        """Press Cmd+key (Right Alt = Cmd when map_cmd_ralt is true)."""
        alt_r_kc = self.d.keysym_to_keycode(XK.XK_Alt_R)
        key_sym = XK.string_to_keysym(key_char)
        key_kc = self.d.keysym_to_keycode(key_sym)

        xtest.fake_input(self.d, X.KeyPress, alt_r_kc, X.CurrentTime)
        self.d.sync()
        time.sleep(0.1)
        xtest.fake_input(self.d, X.KeyPress, key_kc, X.CurrentTime)
        self.d.sync()
        time.sleep(0.1)
        xtest.fake_input(self.d, X.KeyRelease, key_kc, X.CurrentTime)
        self.d.sync()
        time.sleep(0.1)
        xtest.fake_input(self.d, X.KeyRelease, alt_r_kc, X.CurrentTime)
        self.d.sync()

    def menu_select(self, menu_mac_x, item_mac_y):
        """Open a Mac menu and select an item by click-hold-drag."""
        menu_sx, menu_sy = self.mac_to_screen(menu_mac_x, 8)
        self.move_to(menu_sx, menu_sy)
        time.sleep(0.3)

        xtest.fake_input(self.d, X.ButtonPress, 1, X.CurrentTime)
        self.d.sync()
        time.sleep(0.5)

        target_sx, target_sy = self.mac_to_screen(menu_mac_x, item_mac_y)
        ptr = self.root.query_pointer()
        steps = max(10, abs(target_sy - ptr.root_y) // 2)
        for i in range(1, steps + 1):
            ny = int(ptr.root_y + (target_sy - ptr.root_y) * i / steps)
            self._xtest_move(menu_sx, ny)
            time.sleep(0.02)
        time.sleep(0.3)

        xtest.fake_input(self.d, X.ButtonRelease, 1, X.CurrentTime)
        self.d.sync()

    def open_selected(self):
        """Open selected icon via Cmd+O."""
        self.cmd_key('o')

    def close_window(self):
        """Close frontmost window via Cmd+W."""
        self.cmd_key('w')

    def select_all(self):
        """Select all via Cmd+A."""
        self.cmd_key('a')

    def screenshot(self, path):
        """Take screenshot of the full X11 screen."""
        import subprocess
        subprocess.run(['import', '-window', 'root', path],
                       env={'DISPLAY': self.d.get_display_name()})
```

## Common Recipes

### Select and Open a Desktop Icon

```python
snow = SnowAutomation(':1')
snow.ensure_focus()
snow.click_at_mac(460, 45)   # Click icon in upper-right
time.sleep(0.5)
snow.open_selected()          # Cmd+O
time.sleep(2)
```

### Use File > Open Menu

```python
snow = SnowAutomation(':1')
snow.ensure_focus()
snow.menu_select(54, 30)      # File menu, ~30px down for Open item
time.sleep(2)
```

### Type Text

```python
snow = SnowAutomation(':1')
snow.ensure_focus()
for char in "Hello":
    sym = XK.string_to_keysym(char)
    kc = snow.d.keysym_to_keycode(sym)
    snow.key_press(kc)
    time.sleep(0.05)
```

### Close All Windows

```python
snow = SnowAutomation(':1')
snow.ensure_focus()
for _ in range(5):
    snow.close_window()
    time.sleep(0.5)
```

## Snow Source Code References

Key files in the Snow source that affect automation behavior:

| File | Function | What It Does |
|------|----------|-------------|
| `app.rs:3080` | `raw_input_hook()` | Intercepts mouse events before egui, forwards to emulator |
| `app.rs:1811` | `get_machine_mouse_pos()` | Converts screen coords to Mac coords, checks `has_pointer()` |
| `app.rs:1772` | `poll_winit_events()` | Receives keyboard events from patched egui-winit |
| `emulator.rs:415` | `update_mouse()` | Sends MouseUpdateAbsolute to emulator core |
| `emulator.rs:450` | `update_mouse_button()` | Sends button press/release to emulator core |
| `framebuffer.rs:282` | `has_pointer()` | Returns previous frame's `contains_pointer()` state |
| `keymap.rs:148` | `map_winit_keycode()` | Maps winit keycodes to Mac scancodes (AltRight -> 0x37 Cmd) |

## Workspace Configuration

The `.snoww` workspace file affects coordinate mapping:

```json
{
    "viewport_scale": 1.5,
    "map_cmd_ralt": true,
    "scaling_algorithm": "NearestNeighbor",
    "framebuffer_mode": "Centered",
    "model": "Plus"
}
```

- `viewport_scale`: Multiply Mac coords by this value for screen coords
- `map_cmd_ralt`: Must be `true` for Right Alt = Cmd keyboard shortcuts
- `framebuffer_mode`: "Centered" means FB is centered in the CentralPanel area

## Troubleshooting

**Clicks don't register**: Ensure jiggle() runs after move_to() and wait 300ms before clicking. Check that only one Snow instance is running.

**Keyboard not working**: Click inside the framebuffer first to give Snow X11 focus. Verify `map_cmd_ralt: true` in workspace config.

**Wrong coordinates**: Re-measure FB_LEFT and FB_TOP if the window size or viewport_scale changes. The values above are for a 1000x750 window at viewport_scale=1.5.

**`ui_active` blocking input**: If a Snow dialog (file picker, about box) is open, all emulator input is blocked. Close the dialog first.
