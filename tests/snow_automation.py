#!/usr/bin/env python3
"""Snow emulator GUI automation library for Flynn testing.

Based on docs/SNOW-GUI-AUTOMATION.md and docs/TESTING.md.
Uses XTEST extension for reliable mouse/keyboard input to the Snow emulator
running a classic Mac Plus with System 6.0.8.

See docs/SNOW-GUI-AUTOMATION.md for:
  - Coordinate system and conversion formulas
  - Why XTEST incremental motion is required (not XWarpPointer)
  - Why jiggle is needed after moves (egui contains_pointer lag)
  - Menu click-hold-drag technique
  - Troubleshooting guide

See docs/TESTING.md for:
  - Snow emulator setup and configuration
  - Deployment workflow (build, hcopy, boot)
  - Phase 5 test results and known issues
  - Display/X11 troubleshooting

CRITICAL: Use scrot for screenshots (NOT import -window root).
CRITICAL: Use XTEST incremental motion (NOT xdotool mousemove).
"""

import os
import subprocess
import time
from Xlib import X, XK, display
from Xlib.ext import xtest

# PIL is optional, used for framebuffer auto-detection
try:
    from PIL import Image
    HAS_PIL = True
except ImportError:
    HAS_PIL = False


class SnowAutomation:
    """Control Snow emulator via X11 XTEST extension.

    Coordinate system (from docs/SNOW-GUI-AUTOMATION.md):
      Mac coords (0-511, 0-341) -> screen coords via:
        screen_x = FB_LEFT + mac_x * SCALE
        screen_y = FB_TOP + mac_y * SCALE

    FB_LEFT/FB_TOP vary per session depending on window manager placement.
    Call calibrate() or pass values to constructor. Defaults are from the
    most recent measured values in docs/SNOW-GUI-AUTOMATION.md.
    """

    # Default framebuffer offsets - these vary per session!
    # See docs/SNOW-GUI-AUTOMATION.md "Coordinate System" section
    FB_LEFT = 203
    FB_TOP = 190
    SCALE = 1.5  # viewport_scale from .snoww config

    # Mac menu bar positions (mac coords)
    MAC_APPLE_MENU = (12, 8)
    MAC_FILE_MENU = (54, 8)
    MAC_EDIT_MENU = (93, 8)
    MAC_VIEW_MENU = (132, 8)
    MAC_SPECIAL_MENU = (178, 8)

    # Mac menu item approximate Y positions
    # File menu items: Connect=~20, Disconnect=~32, separator=~42, Quit=~52
    MAC_FILE_CONNECT_Y = 20
    MAC_FILE_DISCONNECT_Y = 32
    MAC_FILE_QUIT_Y = 52

    # Edit menu items: Undo=~20, sep=~30, Cut=~40, Copy=~52, Paste=~64, Clear=~76
    MAC_EDIT_COPY_Y = 52
    MAC_EDIT_PASTE_Y = 64

    # Apple menu items
    MAC_APPLE_ABOUT_Y = 20

    def __init__(self, display_name=':0', auto_calibrate=True):
        self.display_name = display_name
        self.d = display.Display(display_name)
        self.root = self.d.screen().root
        self.snow_win_id = self._find_snow_window()
        self.snow_win = self.d.create_resource_object('window', self.snow_win_id)
        self.screenshot_dir = '/tmp/snow-screenshots'
        os.makedirs(self.screenshot_dir, exist_ok=True)
        if auto_calibrate and HAS_PIL:
            self.calibrate()

    def _find_snow_window(self):
        result = subprocess.run(
            ['xdotool', 'search', '--name', 'Snow'],
            capture_output=True, text=True,
            env={**os.environ, 'DISPLAY': self.display_name}
        )
        wids = result.stdout.strip().split('\n')
        if not wids or not wids[0]:
            raise RuntimeError("No Snow window found. Is Snow running?")
        return int(wids[0])

    def calibrate(self):
        """Auto-detect framebuffer position from a screenshot.

        The Mac Plus framebuffer (512x342 @ 1.5x = 768x513) is a bright
        rectangle inside the Snow window. We find the top-left corner of
        this rectangle by scanning for the transition from the dark Snow
        chrome to the white Mac screen.

        See docs/SNOW-GUI-AUTOMATION.md "Coordinate System" section.
        """
        path = os.path.join(self.screenshot_dir, '_calibration.png')
        subprocess.run(['scrot', path],
                       env={**os.environ, 'DISPLAY': self.display_name})
        try:
            img = Image.open(path)
            pixels = img.load()
            w, h = img.size

            # Scan for top edge of framebuffer (white row)
            # The Mac menu bar is white, so look for a long horizontal
            # run of bright pixels (>200 brightness) at least 600px wide
            fb_top = None
            fb_left = None
            for y in range(100, min(400, h)):
                bright_count = 0
                first_bright = None
                for x in range(50, min(w, 1100)):
                    r, g, b = pixels[x, y][:3]
                    brightness = (r + g + b) / 3
                    if brightness > 200:
                        if first_bright is None:
                            first_bright = x
                        bright_count += 1
                if bright_count > 600 and first_bright is not None:
                    fb_top = y
                    fb_left = first_bright
                    break

            if fb_top is not None and fb_left is not None:
                self.FB_LEFT = fb_left
                self.FB_TOP = fb_top
                print(f"Calibrated: FB_LEFT={fb_left}, FB_TOP={fb_top}")
            else:
                print(f"Calibration failed, using defaults: FB_LEFT={self.FB_LEFT}, FB_TOP={self.FB_TOP}")
        except Exception as e:
            print(f"Calibration error: {e}, using defaults")

    def mac_to_screen(self, mac_x, mac_y):
        """Convert Mac coordinates (0-511, 0-341) to X11 screen coords."""
        return (int(self.FB_LEFT + mac_x * self.SCALE),
                int(self.FB_TOP + mac_y * self.SCALE))

    def screen_to_mac(self, screen_x, screen_y):
        """Convert X11 screen coords to Mac coordinates."""
        return ((screen_x - self.FB_LEFT) / self.SCALE,
                (screen_y - self.FB_TOP) / self.SCALE)

    def _xtest_move(self, x, y):
        """Move cursor to absolute position via XTEST."""
        xtest.fake_input(self.d, X.MotionNotify, 0, X.CurrentTime,
                         self.root, x, y)
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

    def double_click(self, delay=0.05):
        """Double click at current position."""
        self.click(delay)
        time.sleep(0.1)
        self.click(delay)

    def double_click_at(self, x, y):
        """Move to screen position and double click."""
        self.move_to(x, y)
        time.sleep(0.3)
        self.double_click()

    def ensure_focus(self):
        """Give Snow keyboard focus by clicking in the framebuffer center."""
        center_x = self.FB_LEFT + 256 * self.SCALE
        center_y = self.FB_TOP + 171 * self.SCALE
        self.move_to(int(center_x), int(center_y))
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

    def key_sym_press(self, keysym):
        """Press a key by keysym name (e.g., 'Return', 'Escape')."""
        sym = XK.string_to_keysym(keysym)
        if sym == 0:
            raise ValueError(f"Unknown keysym: {keysym}")
        kc = self.d.keysym_to_keycode(sym)
        self.key_press(kc)

    def type_text(self, text, delay=0.05):
        """Type a string character by character."""
        shift_kc = self.d.keysym_to_keycode(XK.XK_Shift_L)
        for char in text:
            needs_shift = char.isupper() or char in '!@#$%^&*()_+{}|:"<>?~'
            sym = XK.string_to_keysym(char)
            if sym == 0:
                # Try char code directly
                sym = ord(char)
            kc = self.d.keysym_to_keycode(sym)
            if kc == 0:
                continue
            if needs_shift:
                xtest.fake_input(self.d, X.KeyPress, shift_kc, X.CurrentTime)
                self.d.sync()
                time.sleep(0.02)
            self.key_press(kc)
            if needs_shift:
                xtest.fake_input(self.d, X.KeyRelease, shift_kc, X.CurrentTime)
                self.d.sync()
            time.sleep(delay)

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

    def cmd_shift_key(self, key_char):
        """Press Cmd+Shift+key."""
        alt_r_kc = self.d.keysym_to_keycode(XK.XK_Alt_R)
        shift_kc = self.d.keysym_to_keycode(XK.XK_Shift_L)
        key_sym = XK.string_to_keysym(key_char)
        key_kc = self.d.keysym_to_keycode(key_sym)

        xtest.fake_input(self.d, X.KeyPress, alt_r_kc, X.CurrentTime)
        self.d.sync()
        time.sleep(0.05)
        xtest.fake_input(self.d, X.KeyPress, shift_kc, X.CurrentTime)
        self.d.sync()
        time.sleep(0.05)
        xtest.fake_input(self.d, X.KeyPress, key_kc, X.CurrentTime)
        self.d.sync()
        time.sleep(0.1)
        xtest.fake_input(self.d, X.KeyRelease, key_kc, X.CurrentTime)
        self.d.sync()
        time.sleep(0.05)
        xtest.fake_input(self.d, X.KeyRelease, shift_kc, X.CurrentTime)
        self.d.sync()
        time.sleep(0.05)
        xtest.fake_input(self.d, X.KeyRelease, alt_r_kc, X.CurrentTime)
        self.d.sync()

    def option_key(self, key_char):
        """Press Option+key (Left Alt = Option). Used for Ctrl mapping on M0110."""
        alt_l_kc = self.d.keysym_to_keycode(XK.XK_Alt_L)
        key_sym = XK.string_to_keysym(key_char)
        key_kc = self.d.keysym_to_keycode(key_sym)

        xtest.fake_input(self.d, X.KeyPress, alt_l_kc, X.CurrentTime)
        self.d.sync()
        time.sleep(0.1)
        xtest.fake_input(self.d, X.KeyPress, key_kc, X.CurrentTime)
        self.d.sync()
        time.sleep(0.1)
        xtest.fake_input(self.d, X.KeyRelease, key_kc, X.CurrentTime)
        self.d.sync()
        time.sleep(0.1)
        xtest.fake_input(self.d, X.KeyRelease, alt_l_kc, X.CurrentTime)
        self.d.sync()

    def menu_select(self, menu_mac_x, item_mac_y):
        """Open a Mac menu and select an item by click-hold-drag."""
        menu_sx, menu_sy = self.mac_to_screen(menu_mac_x, 8)
        self.move_to(menu_sx, menu_sy)
        time.sleep(0.3)

        # Press and hold
        xtest.fake_input(self.d, X.ButtonPress, 1, X.CurrentTime)
        self.d.sync()
        time.sleep(0.5)

        # Drag to item
        target_sx, target_sy = self.mac_to_screen(menu_mac_x, item_mac_y)
        ptr = self.root.query_pointer()
        steps = max(10, abs(target_sy - ptr.root_y) // 2)
        for i in range(1, steps + 1):
            ny = int(ptr.root_y + (target_sy - ptr.root_y) * i / steps)
            self._xtest_move(menu_sx, ny)
            time.sleep(0.02)
        time.sleep(0.3)

        # Release
        xtest.fake_input(self.d, X.ButtonRelease, 1, X.CurrentTime)
        self.d.sync()

    def screenshot(self, name='screenshot'):
        """Take screenshot. Returns the file path."""
        path = os.path.join(self.screenshot_dir, f'{name}.png')
        subprocess.run(
            ['scrot', path],
            env={**os.environ, 'DISPLAY': self.display_name}
        )
        return path

    # --- High-level convenience methods ---

    def connect_via_keyboard(self):
        """Open connect dialog with Cmd+N (File > Connect shortcut)."""
        self.ensure_focus()
        self.cmd_key('n')
        time.sleep(1)

    def connect_via_menu(self):
        """Open connect dialog via File > Connect menu drag."""
        self.ensure_focus()
        self.menu_select(self.MAC_FILE_MENU[0], self.MAC_FILE_CONNECT_Y)
        time.sleep(1)

    def disconnect_via_menu(self):
        """Disconnect via File > Disconnect menu drag."""
        self.ensure_focus()
        self.menu_select(self.MAC_FILE_MENU[0], self.MAC_FILE_DISCONNECT_Y)
        time.sleep(1)

    def quit_via_menu(self):
        """Quit via File > Quit menu drag."""
        self.ensure_focus()
        self.menu_select(self.MAC_FILE_MENU[0], self.MAC_FILE_QUIT_Y)
        time.sleep(1)

    def quit_via_keyboard(self):
        """Quit via Cmd+Q."""
        self.ensure_focus()
        self.cmd_key('q')
        time.sleep(1)

    def copy_via_keyboard(self):
        """Copy via Cmd+C."""
        self.cmd_key('c')
        time.sleep(0.5)

    def paste_via_keyboard(self):
        """Paste via Cmd+V."""
        self.cmd_key('v')
        time.sleep(0.5)

    def scroll_up(self):
        """Scroll back one line via Cmd+Up."""
        self.cmd_key('Up')
        time.sleep(0.3)

    def scroll_down(self):
        """Scroll forward one line via Cmd+Down."""
        self.cmd_key('Down')
        time.sleep(0.3)

    def scroll_page_up(self):
        """Scroll back one page via Cmd+Shift+Up."""
        self.cmd_shift_key('Up')
        time.sleep(0.3)

    def scroll_page_down(self):
        """Scroll forward one page via Cmd+Shift+Down."""
        self.cmd_shift_key('Down')
        time.sleep(0.3)

    def open_about(self):
        """Open About Flynn via Apple menu."""
        self.ensure_focus()
        self.menu_select(self.MAC_APPLE_MENU[0], self.MAC_APPLE_ABOUT_Y)
        time.sleep(1)

    def press_return(self):
        """Press Return key."""
        self.key_sym_press('Return')
        time.sleep(0.1)

    def press_escape(self):
        """Press Escape key."""
        self.key_sym_press('Escape')
        time.sleep(0.1)

    def send_ctrl_c(self):
        """Send Ctrl+C via Option+C (M0110 keyboard mapping)."""
        self.option_key('c')
        time.sleep(0.3)

    def wait_for_boot(self, timeout=30):
        """Wait for Mac to finish booting (heuristic: just wait)."""
        time.sleep(timeout)
