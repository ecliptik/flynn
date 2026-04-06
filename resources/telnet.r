/*
 * telnet.r - Resources for Flynn Telnet client
 */

#include "Menus.r"
#include "Dialogs.r"
#include "Processes.r"

resource 'MBAR' (128) {
	{ 128, 129, 130, 132, 136, 131, 133 }
};

resource 'MENU' (128, "Apple") {
	128, textMenuProc, allEnabled, enabled, apple,
	{
		"About Flynn\311", noIcon, noKey, noMark, plain
	}
};

resource 'MENU' (129, "File") {
	129, textMenuProc, allEnabled, enabled, "File",
	{
		"New Session\311", noIcon, "N", noMark, plain;
		"Close Session", noIcon, "W", noMark, plain;
		"Reconnect", noIcon, "R", noMark, plain;
		"-", noIcon, noKey, noMark, plain;
		"Finger\311", noIcon, "I", noMark, plain;
		"-", noIcon, noKey, noMark, plain;
		"Save Contents\311", noIcon, "S", noMark, plain;
		"Start Logging\311", noIcon, "L", noMark, plain;
		"-", noIcon, noKey, noMark, plain;
		"Page Setup\311", noIcon, noKey, noMark, plain;
		"Print\311", noIcon, "P", noMark, plain;
		"-", noIcon, noKey, noMark, plain;
		"Quit", noIcon, "Q", noMark, plain
	}
};

resource 'MENU' (130, "Edit") {
	130, textMenuProc, allEnabled, enabled, "Edit",
	{
		"Undo", noIcon, "Z", noMark, plain;
		"-", noIcon, noKey, noMark, plain;
		"Cut", noIcon, "X", noMark, plain;
		"Copy", noIcon, "C", noMark, plain;
		"Paste", noIcon, "V", noMark, plain;
		"Clear", noIcon, noKey, noMark, plain;
		"-", noIcon, noKey, noMark, plain;
		"Select All", noIcon, "A", noMark, plain;
		"-", noIcon, noKey, noMark, plain;
		"Find\311", noIcon, "F", noMark, plain;
		"Find Again", noIcon, "G", noMark, plain;
		"-", noIcon, noKey, noMark, plain;
		"Clear Scrollback", noIcon, noKey, noMark, plain;
		"-", noIcon, noKey, noMark, plain;
		"Show Clipboard", noIcon, noKey, noMark, plain
	}
};

resource 'MENU' (131, "Options") {
	131, textMenuProc, allEnabled, enabled, "Options",
	{
		"Font", noIcon, noKey, noMark, plain;
		"Size", noIcon, noKey, noMark, plain;
		"Terminal Type", noIcon, noKey, noMark, plain;
		"-", noIcon, noKey, noMark, plain;
		"Theme", noIcon, noKey, noMark, plain;
		"Show Status Bar", noIcon, noKey, noMark, plain;
		"-", noIcon, noKey, noMark, plain;
		"Switch Backspace to BS", noIcon, noKey, noMark, plain;
		"Turn Local Echo On", noIcon, noKey, noMark, plain;
		"-", noIcon, noKey, noMark, plain;
		"DNS Server\311", noIcon, noKey, noMark, plain
	}
};

resource 'MENU' (134, "Font") {
	134, textMenuProc, allEnabled, enabled, "Font",
	{
		"Monaco", noIcon, noKey, noMark, plain;
		"Geneva", noIcon, noKey, noMark, plain;
		"Chicago", noIcon, noKey, noMark, plain;
		"Courier", noIcon, noKey, noMark, plain;
		"New York", noIcon, noKey, noMark, plain
		/* Helvetica, Times, Palatino appended at runtime on System 7 */
	}
};

resource 'MENU' (137, "Size") {
	137, textMenuProc, allEnabled, enabled, "Size",
	{
		"9", noIcon, noKey, noMark, plain;
		"10", noIcon, noKey, noMark, plain;
		"12", noIcon, noKey, noMark, plain;
		"14", noIcon, noKey, noMark, plain
	}
};

resource 'MENU' (135, "Terminal Type") {
	135, textMenuProc, allEnabled, enabled, "Terminal Type",
	{
		"xterm", noIcon, noKey, noMark, plain;
		"xterm-256color", noIcon, noKey, noMark, plain;
		"VT100", noIcon, noKey, noMark, plain;
		"VT220", noIcon, noKey, noMark, plain;
		"ANSI-BBS", noIcon, noKey, noMark, plain
	}
};

resource 'MENU' (136, "Favorites") {
	136, textMenuProc, allEnabled, enabled, "Favorites",
	{
		"Manage Favorites\311", noIcon, "B", noMark, plain;
		"Add Favorite\311", noIcon, "D", noMark, plain
		/* Separator + bookmark entries added dynamically */
	}
};

resource 'MENU' (138, "Theme") {
	138, textMenuProc, allEnabled, enabled, "Theme",
	{
		"Light", noIcon, noKey, noMark, plain;
		"Dark", noIcon, noKey, noMark, plain;
		"-", noIcon, noKey, noMark, plain;
		"Solarized Light", noIcon, noKey, noMark, plain;
		"Solarized Dark", noIcon, noKey, noMark, plain;
		"TokyoNight Day", noIcon, noKey, noMark, plain;
		"TokyoNight", noIcon, noKey, noMark, plain;
		"Amber CRT", noIcon, noKey, noMark, plain;
		"System 7", noIcon, noKey, noMark, plain;
		"Compact Mac", noIcon, noKey, noMark, plain;
		"Dracula", noIcon, noKey, noMark, plain;
		"Nord", noIcon, noKey, noMark, plain
	}
};

resource 'MENU' (133, "Window") {
	133, textMenuProc, allEnabled, enabled, "Window",
	{
	}
};

resource 'MENU' (132, "Control") {
	132, textMenuProc, allEnabled, enabled, "Control",
	{
		"Send Ctrl-C", noIcon, noKey, noMark, plain;
		"Send Ctrl-D", noIcon, noKey, noMark, plain;
		"Send Ctrl-H", noIcon, noKey, noMark, plain;
		"Send Ctrl-L", noIcon, noKey, noMark, plain;
		"Send Ctrl-X", noIcon, noKey, noMark, plain;
		"Send Ctrl-Z", noIcon, noKey, noMark, plain;
		"-", noIcon, noKey, noMark, plain;
		"Send Break", noIcon, noKey, noMark, plain;
		"Send Escape", noIcon, ".", noMark, plain;
		"-", noIcon, noKey, noMark, plain;
		"Reset Terminal", noIcon, noKey, noMark, plain
	}
};

/* Connection dialog */
resource 'DLOG' (129, "Connect") {
	{60, 80, 250, 420},
	dBoxProc,
	visible,
	noGoAway,
	0x0,
	129,
	"Connect",
	noAutoCenter
};

resource 'DITL' (129, "Connect") {
	{
		/* 1: OK/Connect button */
		{155, 250, 175, 320},
		Button { enabled, "Connect" };

		/* 2: Cancel button */
		{155, 165, 175, 235},
		Button { enabled, "Cancel" };

		/* 3: Host label */
		{15, 15, 31, 85},
		StaticText { disabled, "Host" };

		/* 4: Host field */
		{15, 90, 31, 325},
		EditText { enabled, "" };

		/* 5: Port label */
		{45, 15, 61, 85},
		StaticText { disabled, "Port" };

		/* 6: Port field */
		{45, 90, 61, 160},
		EditText { enabled, "23" };

		/* 7: Info text (unused, kept for DITL item ordering) */
		{0, 0, 0, 0},
		StaticText { disabled, "" };

		/* 8: Username label */
		{75, 15, 91, 85},
		StaticText { disabled, "Username" };

		/* 9: Username field */
		{75, 90, 91, 235},
		EditText { enabled, "" };

		/* 10: Favorites popup */
		{155, 15, 175, 145},
		Button { enabled, "Favorites" };

		/* 11: Terminal label */
		{105, 15, 121, 85},
		StaticText { disabled, "Terminal" };

		/* 12: Terminal type button */
		{103, 90, 123, 235},
		Button { enabled, "xterm" };

		/* 13: Default button outline (UserItem) */
		{151, 246, 179, 324},
		UserItem { disabled };
	}
};

/* About dialog */
resource 'DLOG' (130, "About Flynn") {
	{70, 100, 240, 400},
	dBoxProc,
	visible,
	noGoAway,
	0x0,
	130,
	"About Flynn",
	noAutoCenter
};

resource 'DITL' (130, "About Flynn") {
	{
		/* 1: OK button */
		{138, 115, 158, 185},
		Button { enabled, "OK" };

		/* 2: Icon */
		{10, 15, 42, 47},
		Icon { disabled, 128 };

		/* 3: App name + version */
		{10, 55, 30, 280},
		StaticText { disabled, "Flynn 1.9.8" };

		/* 4: Machine type (set at runtime) */
		{33, 55, 49, 280},
		StaticText { disabled, "" };

		/* 5: Description */
		{62, 15, 78, 290},
		StaticText { disabled, "A Telnet client for classic Macintosh" };

		/* 6: Copyright */
		{84, 15, 100, 290},
		StaticText { disabled, "\0xA9 2026 Micheal Waltz" };

		/* 7: Codeberg */
		{102, 15, 118, 290},
		StaticText { disabled, "https://codeberg.org/ecliptik/flynn" };

		/* 8: Default button outline (UserItem) */
		{134, 111, 162, 189},
		UserItem { disabled };
	}
};

/* Generic alert for ParamText messages */
resource 'ALRT' (128) {
	{60, 80, 180, 420},
	128,
	{
		OK, visible, sound1,
		OK, visible, sound1,
		OK, visible, sound1,
		OK, visible, sound1
	},
	noAutoCenter
};

resource 'DITL' (128, "Alert") {
	{
		/* OK button */
		{85, 250, 105, 320},
		Button { enabled, "OK" };

		/* Cancel button */
		{85, 160, 105, 230},
		Button { enabled, "Cancel" };

		/* Text */
		{15, 75, 70, 325},
		StaticText { disabled, "^0" };

		/* 4: Default button outline (UserItem) */
		{81, 246, 109, 324},
		UserItem { disabled };
	}
};

/* Favorites manager dialog */
resource 'DLOG' (131, "Favorites") {
	{40, 60, 300, 430},
	noGrowDocProc,
	visible,
	noGoAway,
	0x0,
	131,
	"Manage Favorites",
	noAutoCenter
};

resource 'DITL' (131, "Favorites") {
	{
		/* 1: Done button */
		{228, 280, 248, 350},
		Button { enabled, "Done" };

		/* 2: Add button */
		{15, 280, 35, 350},
		Button { enabled, "Add" };

		/* 3: Edit button */
		{45, 280, 65, 350},
		Button { enabled, "Edit" };

		/* 4: Label */
		{5, 15, 21, 120},
		StaticText { disabled, "Favorites" };

		/* 5: List area (UserItem, enabled for List Manager clicks) */
		{25, 15, 218, 265},
		UserItem { enabled };

		/* 6: Default button outline (UserItem) */
		{224, 276, 252, 354},
		UserItem { disabled };

		/* 7: Remove button (was Delete) */
		{75, 280, 95, 350},
		Button { enabled, "Remove" };

		/* 8: Move Up button */
		{115, 280, 135, 350},
		Button { enabled, "Move Up" };

		/* 9: Move Down button */
		{145, 280, 165, 350},
		Button { enabled, "Move Dn" };

		/* 10: Connect button */
		{195, 280, 215, 350},
		Button { enabled, "Connect" };
	}
};

/* Favorite add/edit dialog */
resource 'DLOG' (132, "Edit Favorite") {
	{42, 65, 318, 445},
	noGrowDocProc,
	visible,
	noGoAway,
	0x0,
	132,
	"Edit Favorite",
	noAutoCenter
};

resource 'DITL' (132, "Edit Favorite") {
	{
		/* 1: OK button */
		{234, 290, 254, 365},
		Button { enabled, "OK" };

		/* 2: Cancel button */
		{234, 205, 254, 275},
		Button { enabled, "Cancel" };

		/* 3: Name label */
		{8, 10, 24, 80},
		StaticText { disabled, "Name" };

		/* 4: Name field */
		{8, 82, 24, 365},
		EditText { enabled, "" };

		/* 5: Host label */
		{32, 10, 48, 80},
		StaticText { disabled, "Host" };

		/* 6: Host field — shares row with Port */
		{32, 82, 48, 265},
		EditText { enabled, "" };

		/* 7: Port label — same row as Host */
		{32, 275, 48, 305},
		StaticText { disabled, "Port" };

		/* 8: Port field */
		{32, 308, 48, 365},
		EditText { enabled, "23" };

		/* 9: Username label */
		{56, 10, 72, 80},
		StaticText { disabled, "User" };

		/* 10: Username field */
		{56, 82, 72, 265},
		EditText { enabled, "" };

		/* 11: Terminal label */
		{86, 10, 102, 80},
		StaticText { disabled, "Terminal" };

		/* 12: Terminal type button */
		{84, 82, 104, 220},
		Button { enabled, "xterm" };

		/* 13: Font label */
		{112, 10, 128, 80},
		StaticText { disabled, "Font" };

		/* 14: Font button */
		{110, 82, 130, 185},
		Button { enabled, "Monaco" };

		/* 15: Default button outline (UserItem) */
		{230, 286, 258, 369},
		UserItem { disabled };

		/* 16: Protocol label */
		{190, 10, 206, 80},
		StaticText { disabled, "Protocol" };

		/* 17: Protocol button */
		{188, 82, 208, 185},
		Button { enabled, "Telnet" };

		/* 18: Verbose checkbox (finger only, right of protocol) */
		{191, 195, 207, 365},
		CheckBox { enabled, "Verbose (/W)" };

		/* 19: Size label */
		{112, 195, 128, 230},
		StaticText { disabled, "Size" };

		/* 20: Size button */
		{110, 232, 130, 300},
		Button { enabled, "9" };

		/* 21: Theme label */
		{138, 10, 154, 80},
		StaticText { disabled, "Theme" };

		/* 22: Theme button */
		{136, 82, 156, 220},
		Button { enabled, "Light" };

		/* 23: Backspace label */
		{164, 10, 180, 80},
		StaticText { disabled, "Backspace" };

		/* 24: Backspace button */
		{162, 82, 182, 185},
		Button { enabled, "DEL" };

		/* 25: Echo label */
		{164, 195, 180, 230},
		StaticText { disabled, "Echo" };

		/* 26: Echo button */
		{162, 232, 182, 300},
		Button { enabled, "Off" };
	}
};

/* DNS Server dialog */
resource 'DLOG' (133, "DNS Server") {
	{80, 100, 200, 400},
	dBoxProc,
	visible,
	noGoAway,
	0x0,
	133,
	"DNS Server",
	noAutoCenter
};

resource 'DITL' (133, "DNS Server") {
	{
		/* OK button */
		{85, 210, 105, 280},
		Button { enabled, "OK" };

		/* Cancel button */
		{85, 120, 105, 190},
		Button { enabled, "Cancel" };

		/* Label */
		{15, 15, 31, 110},
		StaticText { disabled, "DNS Server" };

		/* IP address field */
		{15, 115, 31, 280},
		EditText { enabled, "1.1.1.1" };

		/* Info text */
		{50, 15, 66, 280},
		StaticText { disabled, "Enter IP address (default: 1.1.1.1)" };

		/* 6: Default button outline (UserItem) */
		{81, 206, 109, 284},
		UserItem { disabled };
	}
};

/* Finger dialog */
resource 'DLOG' (137, "Finger") {
	{80, 100, 220, 400},
	dBoxProc,
	visible,
	noGoAway,
	0x0,
	137,
	"Finger",
	noAutoCenter
};

resource 'DITL' (137, "Finger") {
	{
		/* 1: Finger button */
		{105, 210, 125, 280},
		Button { enabled, "Finger" };

		/* 2: Cancel button */
		{105, 120, 125, 190},
		Button { enabled, "Cancel" };

		/* 3: Host label */
		{15, 15, 31, 85},
		StaticText { disabled, "Host" };

		/* 4: Host field */
		{15, 90, 31, 280},
		EditText { enabled, "" };

		/* 5: Username label */
		{45, 15, 61, 85},
		StaticText { disabled, "Username" };

		/* 6: Username field */
		{45, 90, 61, 230},
		EditText { enabled, "" };

		/* 7: Verbose checkbox */
		{75, 15, 91, 155},
		CheckBox { enabled, "Verbose (/W)" };

		/* 8: Default button outline (UserItem) */
		{101, 206, 129, 284},
		UserItem { disabled };
	}
};

/* Find dialog */
resource 'DLOG' (138, "Find") {
	{100, 120, 180, 400},
	dBoxProc,
	visible,
	noGoAway,
	0x0,
	138,
	"Find",
	noAutoCenter
};

resource 'DITL' (138, "Find") {
	{
		/* 1: Find button */
		{45, 200, 65, 260},
		Button { enabled, "Find" };

		/* 2: Cancel button */
		{45, 125, 65, 185},
		Button { enabled, "Cancel" };

		/* 3: Search text label */
		{15, 10, 31, 55},
		StaticText { disabled, "Find" };

		/* 4: Search text field */
		{15, 60, 31, 260},
		EditText { enabled, "" };

		/* 5: Default button outline (UserItem) */
		{41, 196, 69, 264},
		UserItem { disabled };
	}
};

/* Disconnect alert with Reconnect button */
resource 'ALRT' (139) {
	{60, 80, 180, 420},
	139,
	{
		OK, visible, sound1,
		OK, visible, sound1,
		OK, visible, sound1,
		OK, visible, sound1
	},
	noAutoCenter
};

resource 'DITL' (139, "Disconnect Alert") {
	{
		/* 1: OK button */
		{85, 250, 105, 320},
		Button { enabled, "OK" };

		/* 2: Reconnect button */
		{85, 140, 105, 230},
		Button { enabled, "Reconnect" };

		/* 3: Text */
		{15, 75, 70, 325},
		StaticText { disabled, "^0" };
	}
};

/* Application icon - 32x32 bitmap for About dialog */
data 'ICON' (128) {
	$"00000000 03FFFF00"
	$"07FFFF80 0FFFFFC0"
	$"0E0001C0 0C0000C0"
	$"0C0000C0 0C8000C0"
	$"0CC000C0 0C6000C0"
	$"0CC000C0 0C8000C0"
	$"0C0000C0 0C0F80C0"
	$"0C0000C0 0E0001C0"
	$"0FFFFFC0 0C0000C0"
	$"0C0000C0 0C0000C0"
	$"0C007EC0 0C0000C0"
	$"0C0000C0 0FFFFFC0"
	$"07FFFF80 03FFFF00"
	$"00000000 00000000"
	$"00000000 00000000"
	$"00000000 00000000"
};

/* Application icon - Macintosh Plus with >_ prompt */
data 'ICN#' (128) {
	/* Icon bitmap (32x32) - Mac Plus, no feet, small floppy right */
	$"00000000 03FFFF00"
	$"07FFFF80 0FFFFFC0"
	$"0E0001C0 0C0000C0"
	$"0C0000C0 0C8000C0"
	$"0CC000C0 0C6000C0"
	$"0CC000C0 0C8000C0"
	$"0C0000C0 0C0F80C0"
	$"0C0000C0 0E0001C0"
	$"0FFFFFC0 0C0000C0"
	$"0C0000C0 0C0000C0"
	$"0C007EC0 0C0000C0"
	$"0C0000C0 0FFFFFC0"
	$"07FFFF80 03FFFF00"
	$"00000000 00000000"
	$"00000000 00000000"
	$"00000000 00000000"
	/* Mask bitmap (32x32) */
	$"00000000 03FFFF00"
	$"07FFFF80 0FFFFFC0"
	$"0FFFFFC0 0FFFFFC0"
	$"0FFFFFC0 0FFFFFC0"
	$"0FFFFFC0 0FFFFFC0"
	$"0FFFFFC0 0FFFFFC0"
	$"0FFFFFC0 0FFFFFC0"
	$"0FFFFFC0 0FFFFFC0"
	$"0FFFFFC0 0FFFFFC0"
	$"0FFFFFC0 0FFFFFC0"
	$"0FFFFFC0 0FFFFFC0"
	$"0FFFFFC0 0FFFFFC0"
	$"07FFFF80 03FFFF00"
	$"00000000 00000000"
	$"00000000 00000000"
	$"00000000 00000000"
};

/* Application icon - 32x32 4-bit color (shaded) */
data 'icl4' (128) {
	$"0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 00FF FFFF FFFF FFFF FFFF 0000 0000"
	$"0000 0FCC CCCC CCCC CCCC CCCF 0000 0000"
	$"0000 FCCC CCCC CCCC CCCC CCCC CF00 0000"
	$"0000 FEE0 0000 0000 0000 00EE F000 0000"
	$"0000 FE00 0000 0000 0000 0000 EF00 0000"
	$"0000 FE00 0000 0000 0000 0000 EF00 0000"
	$"0000 FE00 F000 0000 0000 0000 EF00 0000"
	$"0000 FE00 FF00 0000 0000 0000 EF00 0000"
	$"0000 FE00 0FF0 0000 0000 0000 EF00 0000"
	$"0000 FE00 FF00 0000 0000 0000 EF00 0000"
	$"0000 FE00 F000 0000 0000 0000 EF00 0000"
	$"0000 FE00 0000 0000 0000 0000 EF00 0000"
	$"0000 FE00 0000 FFFF F000 0000 EF00 0000"
	$"0000 FE00 0000 0000 0000 0000 EF00 0000"
	$"0000 FEE0 0000 0000 0000 00EE F000 0000"
	$"0000 FDDD DDDD DDDD DDDD DDDD DF00 0000"
	$"0000 FCCC CCCC CCCC CCCC CCCC CF00 0000"
	$"0000 FCCC CCCC CCCC CCCC CCCC CF00 0000"
	$"0000 FCCC CCCC CCCC CCCC CCCC CF00 0000"
	$"0000 FCCC CCCC CCCC CEEE EEEC CCF0 0000"
	$"0000 FCCC CCCC CCCC CCCC CCCC CF00 0000"
	$"0000 FCCC CCCC CCCC CCCC CCCC CF00 0000"
	$"0000 FFFF FFFF FFFF FFFF FFFF FF00 0000"
	$"0000 0FCC CCCC CCCC CCCC CCCF 0000 0000"
	$"0000 00FF FFFF FFFF FFFF FFFF 0000 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000"
};

/* Application icon - 32x32 8-bit color (shaded) */
data 'icl8' (128) {
	$"0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 0000 0000 FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF 0000 0000 0000 0000"
	$"0000 0000 00FF 2B2B 2B2B 2B2B 2B2B 2B2B 2B2B 2B2B 2B2B 2B2B FF00 0000 0000 0000"
	$"0000 0000 FF2B 2B2B 2B2B 2B2B 2B2B 2B2B 2B2B 2B2B 2B2B 2B2B 2BFF 0000 0000 0000"
	$"0000 0000 FF81 8100 0000 0000 0000 0000 0000 0000 0000 0081 81FF 0000 0000 0000"
	$"0000 0000 FF81 0000 0000 0000 0000 0000 0000 0000 0000 0000 81FF 0000 0000 0000"
	$"0000 0000 FF81 0000 0000 0000 0000 0000 0000 0000 0000 0000 81FF 0000 0000 0000"
	$"0000 0000 FF81 0000 FF00 0000 0000 0000 0000 0000 0000 0000 81FF 0000 0000 0000"
	$"0000 0000 FF81 0000 FFFF 0000 0000 0000 0000 0000 0000 0000 81FF 0000 0000 0000"
	$"0000 0000 FF81 0000 00FF FF00 0000 0000 0000 0000 0000 0000 81FF 0000 0000 0000"
	$"0000 0000 FF81 0000 FFFF 0000 0000 0000 0000 0000 0000 0000 81FF 0000 0000 0000"
	$"0000 0000 FF81 0000 FF00 0000 0000 0000 0000 0000 0000 0000 81FF 0000 0000 0000"
	$"0000 0000 FF81 0000 0000 0000 0000 0000 0000 0000 0000 0000 81FF 0000 0000 0000"
	$"0000 0000 FF81 0000 0000 0000 FFFF FFFF FF00 0000 0000 0000 81FF 0000 0000 0000"
	$"0000 0000 FF81 0000 0000 0000 0000 0000 0000 0000 0000 0000 81FF 0000 0000 0000"
	$"0000 0000 FF81 8100 0000 0000 0000 0000 0000 0000 0000 0081 81FF 0000 0000 0000"
	$"0000 0000 FF56 5656 5656 5656 5656 5656 5656 5656 5656 5656 56FF 0000 0000 0000"
	$"0000 0000 FF2B 2B2B 2B2B 2B2B 2B2B 2B2B 2B2B 2B2B 2B2B 2B2B 2BFF 0000 0000 0000"
	$"0000 0000 FF2B 2B2B 2B2B 2B2B 2B2B 2B2B 2B2B 2B2B 2B2B 2B2B 2BFF 0000 0000 0000"
	$"0000 0000 FF2B 2B2B 2B2B 2B2B 2B2B 2B2B 2B2B 2B2B 2B2B 2B2B 2BFF 0000 0000 0000"
	$"0000 0000 FF2B 2B2B 2B2B 2B2B 2B2B 2B2B 2B81 8181 8181 812B 2BFF 0000 0000 0000"
	$"0000 0000 FF2B 2B2B 2B2B 2B2B 2B2B 2B2B 2B2B 2B2B 2B2B 2B2B 2BFF 0000 0000 0000"
	$"0000 0000 FF2B 2B2B 2B2B 2B2B 2B2B 2B2B 2B2B 2B2B 2B2B 2B2B 2BFF 0000 0000 0000"
	$"0000 0000 FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF 0000 0000 0000"
	$"0000 0000 00FF 2B2B 2B2B 2B2B 2B2B 2B2B 2B2B 2B2B 2B2B 2B2B FF00 0000 0000 0000"
	$"0000 0000 0000 FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF 0000 0000 0000 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000"
};

/* Application icon - 16x16 1-bit + mask */
data 'ics#' (128) {
	$"1FF0 3FF8 6008 6808 6C08 6808 61E8 6008"
	$"7FF8 6008 63C8 6008 7FF8 3FF0 0000 0000"
	$"1FF0 3FF8 7FF8 7FF8 7FF8 7FF8 7FF8 7FF8"
	$"7FF8 7FF8 7FF8 7FF8 7FF8 3FF0 0000 0000"
};

/* Application icon - 16x16 4-bit color (shaded) */
data 'ics4' (128) {
	$"000F FFFF FFFF 0000 00FC CCCC CCCC F000"
	$"0FE0 0000 0000 F000 0FE0 F000 0000 F000"
	$"0FE0 FF00 0000 F000 0FE0 F000 0000 F000"
	$"0FE0 000F FFF0 F000 0FE0 0000 0000 F000"
	$"0FDD DDDD DDDD F000 0FCC CCCC CCCC F000"
	$"0FCC CCEE EECC F000 0FCC CCCC CCCC F000"
	$"0FFF FFFF FFFF F000 00FC CCCC CCCF 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000"
};

/* Application icon - 16x16 8-bit color (shaded) */
data 'ics8' (128) {
	$"0000 00FF FFFF FFFF FFFF FFFF 0000 0000"
	$"0000 FF2B 2B2B 2B2B 2B2B 2B2B FF00 0000"
	$"00FF 8100 0000 0000 0000 0000 FF00 0000"
	$"00FF 8100 FF00 0000 0000 0000 FF00 0000"
	$"00FF 8100 FFFF 0000 0000 0000 FF00 0000"
	$"00FF 8100 FF00 0000 0000 0000 FF00 0000"
	$"00FF 8100 0000 00FF FFFF FF00 FF00 0000"
	$"00FF 8100 0000 0000 0000 0000 FF00 0000"
	$"00FF 5656 5656 5656 5656 5656 FF00 0000"
	$"00FF 2B2B 2B2B 2B2B 2B2B 2B2B FF00 0000"
	$"00FF 2B2B 2B2B 8181 8181 2B2B FF00 0000"
	$"00FF 2B2B 2B2B 2B2B 2B2B 2B2B FF00 0000"
	$"00FF FFFF FFFF FFFF FFFF FFFF FF00 0000"
	$"0000 FF2B 2B2B 2B2B 2B2B 2BFF 0000 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000"
};

/* Preferences document icon - Terminal Prompt >_ */
data 'ICN#' (129) {
	/* Icon bitmap */
	$"00000000 00000000"
	$"0FFFE000 08001F00"
	$"08000F00 08000700"
	$"08000300 08000100"
	$"08000100 08000100"
	$"09000100 08800100"
	$"08400100 08200100"
	$"08100100 08200100"
	$"08400100 08800100"
	$"09000100 08000100"
	$"080FE100 08000100"
	$"08000100 08000100"
	$"08000100 0FFFFF00"
	$"00000000 00000000"
	$"00000000 00000000"
	$"00000000 00000000"
	/* Mask */
	$"00000000 00000000"
	$"0FFFE000 0FFFFF00"
	$"0FFFFF00 0FFFFF00"
	$"0FFFFF00 0FFFFF00"
	$"0FFFFF00 0FFFFF00"
	$"0FFFFF00 0FFFFF00"
	$"0FFFFF00 0FFFFF00"
	$"0FFFFF00 0FFFFF00"
	$"0FFFFF00 0FFFFF00"
	$"0FFFFF00 0FFFFF00"
	$"0FFFFF00 0FFFFF00"
	$"0FFFFF00 0FFFFF00"
	$"0FFFFF00 0FFFFF00"
	$"00000000 00000000"
	$"00000000 00000000"
	$"00000000 00000000"
};

/* Preferences document icon - 32x32 4-bit */
data 'icl4' (129) {
	$"0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 FFFF FFFF FFFF FFF0 0000 0000 0000"
	$"0000 F000 0000 0000 000F FFFF 0000 0000"
	$"0000 F000 0000 0000 0000 FFFF 0000 0000"
	$"0000 F000 0000 0000 0000 0FFF 0000 0000"
	$"0000 F000 0000 0000 0000 00FF 0000 0000"
	$"0000 F000 0000 0000 0000 000F 0000 0000"
	$"0000 F000 0000 0000 0000 000F 0000 0000"
	$"0000 F000 0000 0000 0000 000F 0000 0000"
	$"0000 F00F 0000 0000 0000 000F 0000 0000"
	$"0000 F000 F000 0000 0000 000F 0000 0000"
	$"0000 F000 0F00 0000 0000 000F 0000 0000"
	$"0000 F000 00F0 0000 0000 000F 0000 0000"
	$"0000 F000 000F 0000 0000 000F 0000 0000"
	$"0000 F000 00F0 0000 0000 000F 0000 0000"
	$"0000 F000 0F00 0000 0000 000F 0000 0000"
	$"0000 F000 F000 0000 0000 000F 0000 0000"
	$"0000 F00F 0000 0000 0000 000F 0000 0000"
	$"0000 F000 0000 0000 0000 000F 0000 0000"
	$"0000 F000 0000 FFFF FFF0 000F 0000 0000"
	$"0000 F000 0000 0000 0000 000F 0000 0000"
	$"0000 F000 0000 0000 0000 000F 0000 0000"
	$"0000 F000 0000 0000 0000 000F 0000 0000"
	$"0000 F000 0000 0000 0000 000F 0000 0000"
	$"0000 FFFF FFFF FFFF FFFF FFFF 0000 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000"
};

/* Preferences document icon - 16x16 1-bit + mask */
data 'ics#' (129) {
	$"0000 3FF0 2030 2010 2010 2010 2010 2410"
	$"2810 2010 2390 2010 3FF0 0000 0000 0000"
	$"0000 3FF0 3FF0 3FF0 3FF0 3FF0 3FF0 3FF0"
	$"3FF0 3FF0 3FF0 3FF0 3FF0 0000 0000 0000"
};

/* Preferences document icon - 16x16 4-bit */
data 'ics4' (129) {
	$"0000 0000 0000 0000 00FF FFFF FFFF 0000"
	$"00F0 0000 00FF 0000 00F0 0000 000F 0000"
	$"00F0 0000 000F 0000 00F0 0000 000F 0000"
	$"00F0 0000 000F 0000 00F0 0F00 000F 0000"
	$"00F0 F000 000F 0000 00F0 0000 000F 0000"
	$"00F0 00FF F00F 0000 00F0 0000 000F 0000"
	$"00FF FFFF FFFF 0000 0000 0000 0000 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000"
};

/* Preferences document icon - 32x32 8-bit */
data 'icl8' (129) {
	$"0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 0000 FFFF FFFF FFFF FFFF FFFF FFFF FFFF FF00 0000 0000 0000 0000 0000 0000"
	$"0000 0000 FF00 0000 0000 0000 0000 0000 0000 00FF FFFF FFFF 0000 0000 0000 0000"
	$"0000 0000 FF00 0000 0000 0000 0000 0000 0000 0000 FFFF FFFF 0000 0000 0000 0000"
	$"0000 0000 FF00 0000 0000 0000 0000 0000 0000 0000 00FF FFFF 0000 0000 0000 0000"
	$"0000 0000 FF00 0000 0000 0000 0000 0000 0000 0000 0000 FFFF 0000 0000 0000 0000"
	$"0000 0000 FF00 0000 0000 0000 0000 0000 0000 0000 0000 00FF 0000 0000 0000 0000"
	$"0000 0000 FF00 0000 0000 0000 0000 0000 0000 0000 0000 00FF 0000 0000 0000 0000"
	$"0000 0000 FF00 0000 0000 0000 0000 0000 0000 0000 0000 00FF 0000 0000 0000 0000"
	$"0000 0000 FF00 00FF 0000 0000 0000 0000 0000 0000 0000 00FF 0000 0000 0000 0000"
	$"0000 0000 FF00 0000 FF00 0000 0000 0000 0000 0000 0000 00FF 0000 0000 0000 0000"
	$"0000 0000 FF00 0000 00FF 0000 0000 0000 0000 0000 0000 00FF 0000 0000 0000 0000"
	$"0000 0000 FF00 0000 0000 FF00 0000 0000 0000 0000 0000 00FF 0000 0000 0000 0000"
	$"0000 0000 FF00 0000 0000 00FF 0000 0000 0000 0000 0000 00FF 0000 0000 0000 0000"
	$"0000 0000 FF00 0000 0000 FF00 0000 0000 0000 0000 0000 00FF 0000 0000 0000 0000"
	$"0000 0000 FF00 0000 00FF 0000 0000 0000 0000 0000 0000 00FF 0000 0000 0000 0000"
	$"0000 0000 FF00 0000 FF00 0000 0000 0000 0000 0000 0000 00FF 0000 0000 0000 0000"
	$"0000 0000 FF00 00FF 0000 0000 0000 0000 0000 0000 0000 00FF 0000 0000 0000 0000"
	$"0000 0000 FF00 0000 0000 0000 0000 0000 0000 0000 0000 00FF 0000 0000 0000 0000"
	$"0000 0000 FF00 0000 0000 0000 FFFF FFFF FFFF FF00 0000 00FF 0000 0000 0000 0000"
	$"0000 0000 FF00 0000 0000 0000 0000 0000 0000 0000 0000 00FF 0000 0000 0000 0000"
	$"0000 0000 FF00 0000 0000 0000 0000 0000 0000 0000 0000 00FF 0000 0000 0000 0000"
	$"0000 0000 FF00 0000 0000 0000 0000 0000 0000 0000 0000 00FF 0000 0000 0000 0000"
	$"0000 0000 FF00 0000 0000 0000 0000 0000 0000 0000 0000 00FF 0000 0000 0000 0000"
	$"0000 0000 FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF 0000 0000 0000 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000"
};

/* Preferences document icon - 16x16 8-bit */
data 'ics8' (129) {
	$"0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 FFFF FFFF FFFF FFFF FFFF 0000 0000"
	$"0000 FF00 0000 0000 0000 FFFF 0000 0000"
	$"0000 FF00 0000 0000 0000 00FF 0000 0000"
	$"0000 FF00 0000 0000 0000 00FF 0000 0000"
	$"0000 FF00 0000 0000 0000 00FF 0000 0000"
	$"0000 FF00 0000 0000 0000 00FF 0000 0000"
	$"0000 FF00 00FF 0000 0000 00FF 0000 0000"
	$"0000 FF00 FF00 0000 0000 00FF 0000 0000"
	$"0000 FF00 0000 0000 0000 00FF 0000 0000"
	$"0000 FF00 0000 FFFF FF00 00FF 0000 0000"
	$"0000 FF00 0000 0000 0000 00FF 0000 0000"
	$"0000 FFFF FFFF FFFF FFFF FFFF 0000 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000"
};

/* Preferences document ICON for About dialog */
data 'ICON' (129) {
	$"00000000 00000000"
	$"0FFFE000 08001F00"
	$"08000F00 08000700"
	$"08000300 08000100"
	$"08000100 08000100"
	$"09000100 08800100"
	$"08400100 08200100"
	$"08100100 08200100"
	$"08400100 08800100"
	$"09000100 08000100"
	$"080FE100 08000100"
	$"08000100 08000100"
	$"08000100 0FFFFF00"
	$"00000000 00000000"
	$"00000000 00000000"
	$"00000000 00000000"
};

/* File reference - APPL type, icon 0 */
data 'FREF' (128) {
	$"4150 504C 0000 00"                                  /* APPL... */
};

/* File reference - pref type, icon 1 */
data 'FREF' (129) {
	$"7072 6566 0001 00"                                  /* pref... */
};

/* Bundle - associates creator 'FLYN' with icons and file refs */
data 'BNDL' (128) {
	$"464C 594E"                                          /* FLYN */
	$"0000"                                               /* owner ID */
	$"0001"                                               /* 2 types */
	$"4652 4546"                                          /* FREF */
	$"0001"                                               /* 2 entries */
	$"0000 0080"                                          /* local 0 -> res 128 (APPL) */
	$"0001 0081"                                          /* local 1 -> res 129 (pref) */
	$"4943 4E23"                                          /* ICN# */
	$"0001"                                               /* 2 entries */
	$"0000 0080"                                          /* local 0 -> res 128 (app icon) */
	$"0001 0081"                                          /* local 1 -> res 129 (pref icon) */
};

/* Application signature string */
data 'FLYN' (0, "Owner resource") {
	$"15"                                                 /* Pascal string length */
	"Flynn - Telnet Client"
};

resource 'SIZE' (-1) {
	reserved,
	acceptSuspendResumeEvents,
	reserved,
	canBackground,
	doesActivateOnFGSwitch,
	backgroundAndForeground,
	dontGetFrontClicks,
	ignoreChildDiedEvents,
	is32BitCompatible,
	notHighLevelEventAware,
	onlyLocalHLEvents,
	notStationeryAware,
	dontUseTextEditServices,
	reserved,
	reserved,
	reserved,
	768 * 1024,
	640 * 1024
};
