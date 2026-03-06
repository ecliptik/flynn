#!/usr/bin/env python3
"""Final QA: fix nano Ctrl via Option key, retest arrow/tmux/utf8."""
import time
import subprocess
import os
from Xlib import X, XK, display
from Xlib.ext import xtest

os.environ['DISPLAY'] = ':0'

class SnowAuto:
    FB_LEFT = 195; FB_TOP = 191; SCALE = 1.5
    def __init__(self):
        self.d = display.Display(':0')
        self.root = self.d.screen().root
        self.sc = 0
    def mac_to_screen(self, mx, my):
        return (int(self.FB_LEFT + mx * self.SCALE), int(self.FB_TOP + my * self.SCALE))
    def _xtest_move(self, x, y):
        xtest.fake_input(self.d, X.MotionNotify, 0, X.CurrentTime, self.root, x, y)
        self.d.sync()
    def move_to(self, x, y, steps=20):
        ptr = self.root.query_pointer()
        sx, sy = ptr.root_x, ptr.root_y
        for i in range(1, steps + 1):
            self._xtest_move(int(sx + (x-sx)*i/steps), int(sy + (y-sy)*i/steps))
            time.sleep(0.016)
        time.sleep(0.1)
        self.jiggle()
    def jiggle(self):
        ptr = self.root.query_pointer()
        for dx in [1, -1, 1, -1, 0]:
            self._xtest_move(ptr.root_x + dx, ptr.root_y)
            time.sleep(0.016)
        time.sleep(0.1)
    def click(self, delay=0.05):
        xtest.fake_input(self.d, X.ButtonPress, 1, X.CurrentTime)
        self.d.sync()
        time.sleep(delay)
        xtest.fake_input(self.d, X.ButtonRelease, 1, X.CurrentTime)
        self.d.sync()
    def mouse_down(self):
        xtest.fake_input(self.d, X.ButtonPress, 1, X.CurrentTime)
        self.d.sync()
    def mouse_up(self):
        xtest.fake_input(self.d, X.ButtonRelease, 1, X.CurrentTime)
        self.d.sync()
    def click_at_mac(self, mx, my):
        sx, sy = self.mac_to_screen(mx, my)
        self.move_to(sx, sy); time.sleep(0.3); self.click()
    def cmd_key(self, key_char):
        """Right Alt (= Mac Cmd) + key."""
        alt_r_kc = self.d.keysym_to_keycode(XK.XK_Alt_R)
        key_sym = XK.string_to_keysym(key_char)
        key_kc = self.d.keysym_to_keycode(key_sym)
        xtest.fake_input(self.d, X.KeyPress, alt_r_kc, X.CurrentTime); self.d.sync(); time.sleep(0.1)
        xtest.fake_input(self.d, X.KeyPress, key_kc, X.CurrentTime); self.d.sync(); time.sleep(0.1)
        xtest.fake_input(self.d, X.KeyRelease, key_kc, X.CurrentTime); self.d.sync(); time.sleep(0.1)
        xtest.fake_input(self.d, X.KeyRelease, alt_r_kc, X.CurrentTime); self.d.sync()
    def option_key(self, key_char):
        """Left Alt (= Mac Option) + key. Used for Ctrl on M0110 keyboards."""
        alt_l_kc = self.d.keysym_to_keycode(XK.XK_Alt_L)
        key_sym = XK.string_to_keysym(key_char)
        key_kc = self.d.keysym_to_keycode(key_sym)
        xtest.fake_input(self.d, X.KeyPress, alt_l_kc, X.CurrentTime); self.d.sync(); time.sleep(0.1)
        xtest.fake_input(self.d, X.KeyPress, key_kc, X.CurrentTime); self.d.sync(); time.sleep(0.1)
        xtest.fake_input(self.d, X.KeyRelease, key_kc, X.CurrentTime); self.d.sync(); time.sleep(0.1)
        xtest.fake_input(self.d, X.KeyRelease, alt_l_kc, X.CurrentTime); self.d.sync()
    def key_press(self, keycode):
        xtest.fake_input(self.d, X.KeyPress, keycode, X.CurrentTime); self.d.sync(); time.sleep(0.05)
        xtest.fake_input(self.d, X.KeyRelease, keycode, X.CurrentTime); self.d.sync()
    def type_char(self, char):
        shift_kc = self.d.keysym_to_keycode(XK.XK_Shift_L)
        needs_shift = False
        if char == ' ': sym = XK.XK_space
        elif char == '.': sym = XK.XK_period
        elif char == '/': sym = XK.XK_slash
        elif char == '-': sym = XK.XK_minus
        elif char == '=': sym = XK.XK_equal
        elif char == '$': sym = XK.XK_dollar; needs_shift = True
        elif char == "'": sym = XK.XK_apostrophe
        elif char == '"': sym = XK.XK_quotedbl; needs_shift = True
        elif char == '\\': sym = XK.XK_backslash
        elif char == ':':
            sym = XK.string_to_keysym('semicolon')
            needs_shift = True
        elif char.isupper():
            sym = XK.string_to_keysym(char.lower())
            needs_shift = True
        else: sym = XK.string_to_keysym(char)
        if sym is None or sym == 0: return
        kc = self.d.keysym_to_keycode(sym)
        if kc == 0: return
        if needs_shift:
            xtest.fake_input(self.d, X.KeyPress, shift_kc, X.CurrentTime)
            self.d.sync(); time.sleep(0.05)
        self.key_press(kc)
        if needs_shift:
            xtest.fake_input(self.d, X.KeyRelease, shift_kc, X.CurrentTime)
            self.d.sync()
    def type_text(self, text):
        for char in text:
            self.type_char(char)
            time.sleep(0.1)
    def type_return(self):
        self.key_press(self.d.keysym_to_keycode(XK.XK_Return))
    def type_escape(self):
        """Cmd+. = Escape for M0110 keyboards."""
        self.cmd_key('period')
    def ctrl_key(self, key_char):
        """Option+key = Ctrl+key for M0110 keyboards."""
        self.option_key(key_char)
    def screenshot(self, name):
        self.sc += 1
        path = f'/tmp/qa_f_{self.sc:02d}_{name}.png'
        subprocess.run(['scrot', path], env={'DISPLAY': ':0'})
        print(f"  [{self.sc:02d}] {path}")
        return path
    def menu_select(self, menu_mac_x, item_mac_y, name=""):
        menu_sx, menu_sy = self.mac_to_screen(menu_mac_x, 8)
        item_sy = self.mac_to_screen(menu_mac_x, item_mac_y)[1]
        self.move_to(menu_sx, menu_sy); time.sleep(0.5)
        self.mouse_down(); time.sleep(0.8)
        ptr = self.root.query_pointer()
        cur_y = ptr.root_y
        steps = max(20, abs(item_sy - cur_y) // 2)
        for i in range(1, steps + 1):
            self._xtest_move(menu_sx, int(cur_y + (item_sy - cur_y)*i/steps))
            time.sleep(0.025)
        time.sleep(0.3)
        for dx in [1, -1, 1, -1, 0]:
            self._xtest_move(menu_sx + dx, item_sy)
            time.sleep(0.03)
        time.sleep(0.3)
        self.mouse_up(); time.sleep(0.5)

s = SnowAuto()

# ============================================================
# STEP 0: Disconnect, reconnect clean
# ============================================================
print("Disconnecting...")
for _ in range(5):
    s.type_escape()
    time.sleep(1)
s.menu_select(55, 47, "Session > Disconnect")
time.sleep(5)

print("Reconnecting...")
s.cmd_key('n')
time.sleep(4)
s.type_return()
time.sleep(15)

print("Logging in...")
time.sleep(3)
s.type_text("claude")
time.sleep(2)
s.type_return()
time.sleep(5)
s.type_text("claude")
time.sleep(2)
s.type_return()
time.sleep(25)
s.screenshot('logged_in')

s.click_at_mac(300, 180)
time.sleep(2)

# ============================================================
# Test: nano with Option+X for Ctrl+X
# ============================================================
print("\n--- nano test (Option+X for Ctrl+X) ---")
s.type_text("nano /tmp/nanotest4")
time.sleep(2)
s.type_return()
time.sleep(10)
s.screenshot('nano_opened')

# Exit nano: Option+X = Ctrl+X
print("Option+X to exit nano...")
s.ctrl_key('x')  # Now uses option_key('x')
time.sleep(8)
s.screenshot('nano_after_ctrl_x')

# ============================================================
# Test: arrow keys
# ============================================================
print("\n--- arrow keys test ---")
s.type_text("echo arrow1")
time.sleep(2)
s.type_return()
time.sleep(6)

s.type_text("echo arrow2")
time.sleep(2)
s.type_return()
time.sleep(6)

up_kc = s.d.keysym_to_keycode(XK.XK_Up)
s.key_press(up_kc)
time.sleep(3)
s.key_press(up_kc)
time.sleep(3)
s.screenshot('arrow_recalled')

s.type_return()
time.sleep(6)
s.screenshot('arrow_result')

# ============================================================
# Test: LANG=C tmux
# ============================================================
print("\n--- tmux LANG=C test ---")
s.type_text("LANG=C tmux")
time.sleep(3)
s.type_return()
time.sleep(10)
s.screenshot('tmux_c')

# Exit tmux
s.type_text("exit")
time.sleep(2)
s.type_return()
time.sleep(8)
s.screenshot('tmux_c_exited')

# ============================================================
# Test: tmux UTF-8
# ============================================================
print("\n--- tmux UTF-8 test ---")
s.type_text("tmux")
time.sleep(2)
s.type_return()
time.sleep(10)
s.screenshot('tmux_utf8')

s.type_text("exit")
time.sleep(2)
s.type_return()
time.sleep(8)

# ============================================================
# Test: printf accented chars
# ============================================================
print("\n--- printf UTF-8 test ---")
s.type_text("printf '\\xc3\\xa9\\xc3\\xa8\\xc3\\xab\\n'")
time.sleep(3)
s.type_return()
time.sleep(8)
s.screenshot('accented')

s.type_text("echo 'hello world'")
time.sleep(2)
s.type_return()
time.sleep(6)
s.screenshot('echo')

print("\nDone - check /tmp/qa_f_*.png")
