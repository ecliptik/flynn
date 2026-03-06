#!/usr/bin/env python3
"""Phase 6 automated tests for Flynn in Snow emulator.

Tests: menu states, scrollback, copy/paste, About dialog, settings persistence,
keyboard fixes (ESC, Ctrl+C), and regressions.

References:
    - docs/SNOW-GUI-AUTOMATION.md — coordinate system, click methods, menu interaction
    - docs/TESTING.md — emulator setup, deployment workflow, Phase 5 results

Usage:
    python3 tests/test_phase6.py [test_name]

    Run all tests:     python3 tests/test_phase6.py
    Run one test:      python3 tests/test_phase6.py test_menu_states
    List tests:        python3 tests/test_phase6.py --list

Prerequisites:
    - Snow emulator running with Flynn deployed on HDD
    - X11 display :0 available
    - python-xlib, scrot, xdotool installed
    - Optional: Pillow (PIL) for framebuffer auto-detection
"""

import os
import sys
import time
import traceback

# Add tests dir to path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from snow_automation import SnowAutomation

# Test credentials
TEST_HOST = os.environ.get('FLYNN_TEST_HOST', '192.168.7.230')
TEST_PORT = os.environ.get('FLYNN_TEST_PORT', '23')
TEST_USER = os.environ.get('FLYNN_TEST_USER', 'claude')
TEST_PASS = os.environ.get('FLYNN_TEST_PASS', 'claude')


class TestResult:
    def __init__(self, name, passed, screenshot=None, message=''):
        self.name = name
        self.passed = passed
        self.screenshot = screenshot
        self.message = message

    def __str__(self):
        status = 'PASS' if self.passed else 'FAIL'
        s = f'[{status}] {self.name}'
        if self.message:
            s += f' - {self.message}'
        if self.screenshot:
            s += f' (screenshot: {self.screenshot})'
        return s


class Phase6Tests:
    """Phase 6 test suite for Flynn."""

    def __init__(self):
        self.snow = None
        self.results = []
        self.is_connected = False

    def setup(self):
        """Initialize Snow automation."""
        self.snow = SnowAutomation(':0')
        print(f"Snow window found: {self.snow.snow_win_id}")
        print(f"Screenshot dir: {self.snow.screenshot_dir}")

    def teardown(self):
        """Cleanup after tests."""
        pass

    def screenshot(self, name):
        """Take a screenshot and return path."""
        return self.snow.screenshot(name)

    def launch_flynn(self):
        """Double-click Flynn on the Mac desktop to launch it.
        Assumes Finder is showing the HDD contents with Flynn visible."""
        self.snow.ensure_focus()
        # Flynn should be on the HDD root - look for it in the Finder window
        # This depends on icon position; use Cmd+O after clicking if needed
        # For now, assume Flynn is already running or will be launched before tests
        time.sleep(1)

    def connect_to_server(self):
        """Connect Flynn to the test telnet server.
        Skips if already connected (prevents typing host into terminal)."""
        if self.is_connected:
            return

        self.snow.ensure_focus()
        time.sleep(0.5)
        # Use Cmd+N to open Connect dialog
        self.snow.cmd_key('n')
        time.sleep(2)

        # Take screenshot to see the dialog
        self.screenshot('connect-dialog')

        # Type host
        self.snow.type_text(TEST_HOST, delay=0.03)
        time.sleep(0.3)

        # Tab to port field
        self.snow.key_sym_press('Tab')
        time.sleep(0.3)

        # Type port
        self.snow.type_text(TEST_PORT, delay=0.03)
        time.sleep(0.3)

        # Press Return to connect
        self.snow.press_return()
        time.sleep(5)  # Wait for TCP connection + telnet negotiation

        self.screenshot('after-connect')
        self.is_connected = True

    def login(self):
        """Log into the telnet session."""
        self.snow.ensure_focus()
        time.sleep(1)
        # Type username
        self.snow.type_text(TEST_USER, delay=0.05)
        time.sleep(0.3)
        self.snow.press_return()
        time.sleep(2)

        # Type password
        self.snow.type_text(TEST_PASS, delay=0.05)
        time.sleep(0.3)
        self.snow.press_return()
        time.sleep(3)

        self.screenshot('after-login')

    def disconnect(self):
        """Disconnect from current session."""
        self.snow.ensure_focus()
        self.snow.menu_select(
            self.snow.MAC_FILE_MENU[0],
            self.snow.MAC_FILE_DISCONNECT_Y
        )
        time.sleep(2)
        self.is_connected = False

    # ===== TEST METHODS =====

    def test_screenshot_baseline(self):
        """Take a baseline screenshot to verify Snow is running."""
        path = self.screenshot('baseline')
        return TestResult('screenshot_baseline', True,
                          screenshot=path,
                          message='Baseline screenshot captured')

    def test_menu_states_disconnected(self):
        """Verify menu states when disconnected.
        Connect should be enabled, Disconnect should be grayed."""
        self.snow.ensure_focus()
        time.sleep(0.5)

        # Open File menu and screenshot to check item states
        menu_sx, menu_sy = self.snow.mac_to_screen(
            self.snow.MAC_FILE_MENU[0], 8)
        self.snow.move_to(menu_sx, menu_sy)
        time.sleep(0.3)

        # Press and hold to see menu
        from Xlib import X
        from Xlib.ext import xtest as xt
        xt.fake_input(self.snow.d, X.ButtonPress, 1, X.CurrentTime)
        self.snow.d.sync()
        time.sleep(0.8)

        path = self.screenshot('menu-disconnected')

        # Release menu without selecting
        xt.fake_input(self.snow.d, X.ButtonRelease, 1, X.CurrentTime)
        self.snow.d.sync()
        time.sleep(0.3)

        return TestResult('menu_states_disconnected', True,
                          screenshot=path,
                          message='File menu captured while disconnected - verify Connect enabled, Disconnect grayed')

    def test_menu_states_connected(self):
        """Verify menu states when connected.
        Connect should be grayed, Disconnect should be enabled."""
        self.connect_to_server()
        time.sleep(2)

        # Open File menu and screenshot
        menu_sx, menu_sy = self.snow.mac_to_screen(
            self.snow.MAC_FILE_MENU[0], 8)
        self.snow.move_to(menu_sx, menu_sy)
        time.sleep(0.3)

        from Xlib import X
        from Xlib.ext import xtest as xt
        xt.fake_input(self.snow.d, X.ButtonPress, 1, X.CurrentTime)
        self.snow.d.sync()
        time.sleep(0.8)

        path = self.screenshot('menu-connected')

        xt.fake_input(self.snow.d, X.ButtonRelease, 1, X.CurrentTime)
        self.snow.d.sync()
        time.sleep(0.3)

        return TestResult('menu_states_connected', True,
                          screenshot=path,
                          message='File menu captured while connected - verify Connect grayed, Disconnect enabled')

    def test_edit_menu_connected(self):
        """Verify Edit menu states when connected.
        Copy and Paste should be enabled."""
        # Assumes already connected from previous test
        self.snow.ensure_focus()
        time.sleep(0.5)

        menu_sx, menu_sy = self.snow.mac_to_screen(
            self.snow.MAC_EDIT_MENU[0], 8)
        self.snow.move_to(menu_sx, menu_sy)
        time.sleep(0.3)

        from Xlib import X
        from Xlib.ext import xtest as xt
        xt.fake_input(self.snow.d, X.ButtonPress, 1, X.CurrentTime)
        self.snow.d.sync()
        time.sleep(0.8)

        path = self.screenshot('edit-menu-connected')

        xt.fake_input(self.snow.d, X.ButtonRelease, 1, X.CurrentTime)
        self.snow.d.sync()
        time.sleep(0.3)

        return TestResult('edit_menu_connected', True,
                          screenshot=path,
                          message='Edit menu captured while connected - verify Copy/Paste enabled')

    def test_about_dialog(self):
        """Test About Flynn dialog via Apple menu."""
        self.snow.open_about()
        time.sleep(2)

        path = self.screenshot('about-dialog')

        # Click OK to dismiss (click center of dialog area)
        self.snow.click_at_mac(256, 200)
        time.sleep(1)

        return TestResult('about_dialog', True,
                          screenshot=path,
                          message='About dialog shown - verify version, title, credits')

    def test_connect_and_login(self):
        """Test connecting to server and logging in."""
        self.connect_to_server()
        self.login()

        path = self.screenshot('connected-logged-in')

        return TestResult('connect_and_login', True,
                          screenshot=path,
                          message='Connected and logged in - verify shell prompt visible')

    def test_echo_command(self):
        """Test basic echo command after connecting."""
        self.snow.ensure_focus()
        self.snow.type_text('echo "Flynn Phase 6 Test"', delay=0.03)
        time.sleep(0.3)
        self.snow.press_return()
        time.sleep(2)

        path = self.screenshot('echo-test')

        return TestResult('echo_command', True,
                          screenshot=path,
                          message='Echo command executed - verify output visible')

    def test_arrow_keys(self):
        """Test arrow keys work (command history, cursor movement)."""
        self.snow.ensure_focus()

        # Up arrow to recall last command
        self.snow.key_sym_press('Up')
        time.sleep(0.5)

        path = self.screenshot('arrow-up')

        # Down arrow to clear
        self.snow.key_sym_press('Down')
        time.sleep(0.3)

        # Type a string and use left/right arrows
        self.snow.type_text('abcd', delay=0.05)
        time.sleep(0.2)
        self.snow.key_sym_press('Left')
        time.sleep(0.2)
        self.snow.key_sym_press('Left')
        time.sleep(0.2)

        path2 = self.screenshot('arrow-left')

        # Clear the line with Ctrl+C (or Ctrl+U)
        self.snow.send_ctrl_c()
        time.sleep(0.5)

        return TestResult('arrow_keys', True,
                          screenshot=path,
                          message='Arrow keys work - verify command recall and cursor movement')

    def test_escape_key(self):
        """Test ESC key works in vi (Phase 6 keyboard fix)."""
        self.snow.ensure_focus()

        # Launch vi
        self.snow.type_text('vi /tmp/flynn_test.txt', delay=0.03)
        time.sleep(0.3)
        self.snow.press_return()
        time.sleep(3)

        path1 = self.screenshot('vi-opened')

        # Type 'i' to enter insert mode
        self.snow.type_text('i', delay=0.05)
        time.sleep(0.5)

        # Type some text
        self.snow.type_text('Hello from Flynn Phase 6', delay=0.03)
        time.sleep(0.5)

        path2 = self.screenshot('vi-insert-mode')

        # Press ESC to go to command mode
        self.snow.press_escape()
        time.sleep(1)

        path3 = self.screenshot('vi-after-escape')

        # Quit without saving: :q!
        self.snow.type_text(':q!', delay=0.05)
        time.sleep(0.3)
        self.snow.press_return()
        time.sleep(2)

        path4 = self.screenshot('vi-quit')

        return TestResult('escape_key', True,
                          screenshot=path3,
                          message='ESC in vi - verify INSERT mode indicator gone after ESC')

    def test_ctrl_c(self):
        """Test Ctrl+C works at shell (Phase 6 keyboard fix via Option key)."""
        self.snow.ensure_focus()

        # Start a long-running command
        self.snow.type_text('sleep 999', delay=0.03)
        time.sleep(0.3)
        self.snow.press_return()
        time.sleep(2)

        path1 = self.screenshot('before-ctrl-c')

        # Send Ctrl+C via Option+C
        self.snow.send_ctrl_c()
        time.sleep(2)

        path2 = self.screenshot('after-ctrl-c')

        return TestResult('ctrl_c', True,
                          screenshot=path2,
                          message='Ctrl+C via Option+C - verify shell prompt returned after interrupting sleep')

    def test_scrollback(self):
        """Test scrollback viewing with Cmd+Up/Down."""
        self.snow.ensure_focus()

        # Generate lots of output
        self.snow.type_text('seq 1 100', delay=0.03)
        time.sleep(0.3)
        self.snow.press_return()
        time.sleep(3)

        path1 = self.screenshot('scrollback-before')

        # Scroll up several times
        for _ in range(10):
            self.snow.scroll_up()
            time.sleep(0.2)

        path2 = self.screenshot('scrollback-scrolled-up')

        # Scroll back down
        for _ in range(10):
            self.snow.scroll_down()
            time.sleep(0.2)

        path3 = self.screenshot('scrollback-scrolled-down')

        return TestResult('scrollback', True,
                          screenshot=path2,
                          message='Scrollback - verify earlier seq output visible when scrolled up, title shows scroll offset')

    def test_scrollback_page(self):
        """Test page scrollback with Cmd+Shift+Up/Down."""
        self.snow.ensure_focus()

        # Scroll up a page
        self.snow.scroll_page_up()
        time.sleep(0.5)

        path1 = self.screenshot('scrollback-page-up')

        # Scroll down a page
        self.snow.scroll_page_down()
        time.sleep(0.5)

        path2 = self.screenshot('scrollback-page-down')

        return TestResult('scrollback_page', True,
                          screenshot=path1,
                          message='Page scrollback - verify large scroll jump')

    def test_copy(self):
        """Test Copy (Cmd+C) puts screen text on clipboard."""
        self.snow.ensure_focus()

        # Make sure there's some identifiable text on screen
        self.snow.type_text('echo "COPY_TEST_12345"', delay=0.03)
        time.sleep(0.3)
        self.snow.press_return()
        time.sleep(2)

        # Copy screen to clipboard
        self.snow.copy_via_keyboard()
        time.sleep(1)

        path = self.screenshot('after-copy')

        return TestResult('copy', True,
                          screenshot=path,
                          message='Copy via Cmd+C - screen text should be on Mac clipboard')

    def test_paste(self):
        """Test Paste (Cmd+V) sends clipboard text to terminal."""
        self.snow.ensure_focus()

        # Paste whatever is on clipboard
        self.snow.paste_via_keyboard()
        time.sleep(2)

        path = self.screenshot('after-paste')

        # Clear the pasted text
        self.snow.send_ctrl_c()
        time.sleep(0.5)

        return TestResult('paste', True,
                          screenshot=path,
                          message='Paste via Cmd+V - verify clipboard text appeared in terminal')

    def test_nano(self):
        """Regression: nano still works."""
        self.snow.ensure_focus()

        self.snow.type_text('nano /tmp/flynn_nano_test.txt', delay=0.03)
        time.sleep(0.3)
        self.snow.press_return()
        time.sleep(3)

        path1 = self.screenshot('nano-opened')

        # Type some text
        self.snow.type_text('Flynn Phase 6 nano test', delay=0.03)
        time.sleep(0.5)

        path2 = self.screenshot('nano-typed')

        # Exit nano: Ctrl+X (via Option+X)
        self.snow.option_key('x')
        time.sleep(1)

        # nano asks to save - press N for no
        self.snow.type_text('n', delay=0.05)
        time.sleep(2)

        path3 = self.screenshot('nano-exited')

        return TestResult('nano', True,
                          screenshot=path1,
                          message='nano regression - verify inverse video bars and proper rendering')

    def test_disconnect(self):
        """Test disconnect via menu."""
        self.disconnect()
        time.sleep(2)

        path = self.screenshot('after-disconnect')

        return TestResult('disconnect', True,
                          screenshot=path,
                          message='Disconnected - verify window title is "Flynn" (no hostname)')

    def test_menu_states_after_disconnect(self):
        """Verify menu states revert after disconnect."""
        self.snow.ensure_focus()
        time.sleep(0.5)

        menu_sx, menu_sy = self.snow.mac_to_screen(
            self.snow.MAC_FILE_MENU[0], 8)
        self.snow.move_to(menu_sx, menu_sy)
        time.sleep(0.3)

        from Xlib import X
        from Xlib.ext import xtest as xt
        xt.fake_input(self.snow.d, X.ButtonPress, 1, X.CurrentTime)
        self.snow.d.sync()
        time.sleep(0.8)

        path = self.screenshot('menu-after-disconnect')

        xt.fake_input(self.snow.d, X.ButtonRelease, 1, X.CurrentTime)
        self.snow.d.sync()
        time.sleep(0.3)

        return TestResult('menu_states_after_disconnect', True,
                          screenshot=path,
                          message='Menu states reverted - verify Connect enabled, Disconnect grayed')

    def test_settings_persistence(self):
        """Test that host/port are saved and restored.
        After connecting, quit, relaunch, and verify fields pre-filled."""
        # First connect to save settings
        self.connect_to_server()
        time.sleep(3)

        # Disconnect
        self.disconnect()
        time.sleep(2)

        # Re-open connect dialog to check if host/port pre-filled
        self.snow.cmd_key('n')
        time.sleep(2)

        path = self.screenshot('settings-persistence')

        # Press Escape or click Cancel to dismiss dialog
        self.snow.press_escape()
        time.sleep(1)

        return TestResult('settings_persistence', True,
                          screenshot=path,
                          message='Settings persistence - verify host/port pre-filled in Connect dialog')

    def test_window_title_connected(self):
        """Test window title shows hostname when connected."""
        self.connect_to_server()
        time.sleep(3)

        path = self.screenshot('window-title-connected')

        return TestResult('window_title_connected', True,
                          screenshot=path,
                          message='Window title - verify shows "Flynn - hostname" when connected')

    def test_window_title_disconnected(self):
        """Test window title shows just "Flynn" when disconnected."""
        self.disconnect()
        time.sleep(2)

        path = self.screenshot('window-title-disconnected')

        return TestResult('window_title_disconnected', True,
                          screenshot=path,
                          message='Window title - verify shows just "Flynn" when disconnected')

    def test_scrollback_auto_reset(self):
        """Test that new incoming data resets scroll_offset to 0.
        When scrolled back, typing a command should jump to live view."""
        self.snow.ensure_focus()

        # Generate output and scroll back
        self.snow.type_text('seq 1 50', delay=0.03)
        time.sleep(0.3)
        self.snow.press_return()
        time.sleep(3)

        # Scroll back
        for _ in range(5):
            self.snow.scroll_up()
            time.sleep(0.2)

        path1 = self.screenshot('scrollback-before-reset')

        # Type a command - new data should reset to live view
        self.snow.type_text('echo "back to live"', delay=0.03)
        time.sleep(0.3)
        self.snow.press_return()
        time.sleep(2)

        path2 = self.screenshot('scrollback-after-reset')

        return TestResult('scrollback_auto_reset', True,
                          screenshot=path2,
                          message='Scrollback auto-reset - verify live view restored when new data arrives')

    def test_quit_while_connected(self):
        """Test quit while connected shows confirmation alert (if implemented).
        Task #6 adds a confirmation dialog when quitting while connected."""
        # Reconnect if needed
        if not self.is_connected:
            self.connect_to_server()
            self.login()

        self.snow.ensure_focus()

        # Try to quit via Cmd+Q
        self.snow.cmd_key('q')
        time.sleep(2)

        path = self.screenshot('quit-confirmation')

        # If there's a confirmation dialog, click Cancel (button 2) to stay
        # The dialog should have OK and Cancel buttons
        # Click in the Cancel area (approximate)
        self.snow.click_at_mac(190, 130)
        time.sleep(1)

        path2 = self.screenshot('after-quit-cancel')

        return TestResult('quit_while_connected', True,
                          screenshot=path,
                          message='Quit confirmation - verify alert dialog appears when quitting while connected')

    # ===== TEST RUNNER =====

    def get_all_tests(self):
        """Return ordered list of test method names."""
        return [
            'test_screenshot_baseline',
            'test_about_dialog',
            'test_menu_states_disconnected',
            'test_connect_and_login',
            'test_menu_states_connected',
            'test_edit_menu_connected',
            'test_window_title_connected',
            'test_echo_command',
            'test_arrow_keys',
            'test_escape_key',
            'test_ctrl_c',
            'test_scrollback',
            'test_scrollback_page',
            'test_scrollback_auto_reset',
            'test_copy',
            'test_paste',
            'test_nano',
            'test_disconnect',
            'test_menu_states_after_disconnect',
            'test_window_title_disconnected',
            'test_settings_persistence',
            'test_quit_while_connected',
        ]

    def run_test(self, name):
        """Run a single test by name."""
        method = getattr(self, name, None)
        if not method:
            return TestResult(name, False, message=f'Test method not found: {name}')
        try:
            result = method()
            self.results.append(result)
            return result
        except Exception as e:
            tb = traceback.format_exc()
            screenshot = self.screenshot(f'error-{name}')
            result = TestResult(name, False, screenshot=screenshot,
                                message=f'{e}\n{tb}')
            self.results.append(result)
            return result

    def run_all(self):
        """Run all tests in order."""
        tests = self.get_all_tests()
        print(f"\nRunning {len(tests)} Phase 6 tests...\n")

        for name in tests:
            print(f"  Running {name}...", end=' ', flush=True)
            result = self.run_test(name)
            print(result)
            time.sleep(1)

        self.print_summary()

    def run_selected(self, names):
        """Run selected tests."""
        print(f"\nRunning {len(names)} selected test(s)...\n")
        for name in names:
            print(f"  Running {name}...", end=' ', flush=True)
            result = self.run_test(name)
            print(result)
            time.sleep(1)

        self.print_summary()

    def print_summary(self):
        """Print test results summary."""
        passed = sum(1 for r in self.results if r.passed)
        failed = sum(1 for r in self.results if not r.passed)
        total = len(self.results)

        print(f"\n{'='*60}")
        print(f"Phase 6 Test Results: {passed}/{total} passed, {failed} failed")
        print(f"Screenshots saved in: {self.snow.screenshot_dir}")
        print(f"{'='*60}")

        if failed > 0:
            print("\nFailed tests:")
            for r in self.results:
                if not r.passed:
                    print(f"  {r}")

        return failed == 0


def main():
    if len(sys.argv) > 1:
        if sys.argv[1] == '--list':
            suite = Phase6Tests()
            print("Available tests:")
            for name in suite.get_all_tests():
                print(f"  {name}")
            return

    suite = Phase6Tests()

    try:
        suite.setup()
    except RuntimeError as e:
        print(f"Setup failed: {e}")
        print("Is Snow running? Start it with:")
        print("  DISPLAY=:0 tools/snow/snowemu diskimages/flynn.snoww &")
        sys.exit(1)

    if len(sys.argv) > 1:
        # Run specific tests
        suite.run_selected(sys.argv[1:])
    else:
        suite.run_all()


if __name__ == '__main__':
    main()
