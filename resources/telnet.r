/*
 * telnet.r - Resources for Flynn Telnet client
 */

#include "Menus.r"
#include "Dialogs.r"
#include "Processes.r"

resource 'MBAR' (128) {
	{ 128, 129, 130, 131, 133, 132 }
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
		"-", noIcon, noKey, noMark, plain;
		"Bookmarks\311", noIcon, "B", noMark, plain
		/* Recent bookmarks + separator + Quit added dynamically */
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
		"Select All", noIcon, "A", noMark, plain
	}
};

resource 'MENU' (131, "Preferences") {
	131, textMenuProc, allEnabled, enabled, "Preferences",
	{
		"Fonts", noIcon, noKey, noMark, plain;
		"  Monaco 9", noIcon, noKey, noMark, plain;
		"  Monaco 12", noIcon, noKey, noMark, plain;
		"  Courier 10", noIcon, noKey, noMark, plain;
		"  Chicago 12", noIcon, noKey, noMark, plain;
		"  Geneva 9", noIcon, noKey, noMark, plain;
		"  Geneva 10", noIcon, noKey, noMark, plain;
		"-", noIcon, noKey, noMark, plain;
		"Terminal Type", noIcon, noKey, noMark, plain;
		"  xterm", noIcon, noKey, noMark, plain;
		"  VT220", noIcon, noKey, noMark, plain;
		"  VT100", noIcon, noKey, noMark, plain;
		"-", noIcon, noKey, noMark, plain;
		"Networking", noIcon, noKey, noMark, plain;
		"  DNS Server\311", noIcon, noKey, noMark, plain;
		"-", noIcon, noKey, noMark, plain;
		"Misc", noIcon, noKey, noMark, plain;
		"  Dark Mode", noIcon, noKey, noMark, plain
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
		"Send Ctrl-Z", noIcon, noKey, noMark, plain;
		"Send Escape", noIcon, noKey, noMark, plain;
		"Send Ctrl-L", noIcon, noKey, noMark, plain;
		"Send Break", noIcon, noKey, noMark, plain
	}
};

/* Connection dialog */
resource 'DLOG' (129, "Connect") {
	{60, 80, 220, 420},
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
		/* OK button */
		{125, 250, 145, 320},
		Button { enabled, "Connect" };

		/* Cancel button */
		{125, 165, 145, 235},
		Button { enabled, "Cancel" };

		/* Host label + field */
		{15, 15, 31, 85},
		StaticText { disabled, "Host:" };

		{15, 90, 31, 325},
		EditText { enabled, "" };

		/* Port label + field */
		{45, 15, 61, 85},
		StaticText { disabled, "Port:" };

		{45, 90, 61, 160},
		EditText { enabled, "23" };

		/* Info text (unused, kept for DITL item ordering) */
		{0, 0, 0, 0},
		StaticText { disabled, "" };

		/* Username label + field */
		{75, 15, 91, 85},
		StaticText { disabled, "Username:" };

		{75, 90, 91, 235},
		EditText { enabled, "" };

		/* Bookmarks button */
		{125, 15, 145, 145},
		Button { enabled, "Bookmarks\311" };
	}
};

/* About dialog */
resource 'DLOG' (130, "About Flynn") {
	{80, 100, 295, 400},
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
		/* OK button */
		{175, 115, 195, 185},
		Button { enabled, "OK" };

		/* Icon */
		{10, 15, 42, 47},
		Icon { disabled, 128 };

		/* App name */
		{10, 55, 30, 280},
		StaticText { disabled, "Flynn" };

		/* Version */
		{33, 55, 49, 280},
		StaticText { disabled, "Version 1.5.0" };

		/* Description */
		{62, 30, 78, 270},
		StaticText { disabled, "A Telnet client for classic Macintosh" };

		/* Copyright */
		{84, 30, 100, 270},
		StaticText { disabled, "\0xA9 2026 Micheal Waltz" };

		/* Credits */
		{106, 30, 122, 270},
		StaticText { disabled, "Built with Claude Code + Retro68" };

		/* Website */
		{128, 30, 144, 270},
		StaticText { disabled, "https://www.ecliptik.com" };
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
	}
};

/* Bookmark manager dialog */
resource 'DLOG' (131, "Bookmarks") {
	{40, 60, 290, 430},
	dBoxProc,
	visible,
	noGoAway,
	0x0,
	131,
	"Bookmarks",
	noAutoCenter
};

resource 'DITL' (131, "Bookmarks") {
	{
		/* Done button */
		{220, 280, 240, 350},
		Button { enabled, "Done" };

		/* Add button */
		{15, 280, 35, 350},
		Button { enabled, "Add" };

		/* Edit button */
		{45, 280, 65, 350},
		Button { enabled, "Edit" };

		/* Delete button */
		{75, 280, 95, 350},
		Button { enabled, "Delete" };

		/* Connect button */
		{115, 280, 135, 350},
		Button { enabled, "Connect" };

		/* Label */
		{5, 15, 21, 120},
		StaticText { disabled, "Bookmarks:" };

		/* List area (UserItem) */
		{25, 15, 210, 265},
		UserItem { disabled };
	}
};

/* Bookmark add/edit dialog */
resource 'DLOG' (132, "Edit Bookmark") {
	{40, 90, 310, 420},
	dBoxProc,
	visible,
	noGoAway,
	0x0,
	132,
	"Edit Bookmark",
	noAutoCenter
};

resource 'DITL' (132, "Edit Bookmark") {
	{
		/* 1: OK button */
		{235, 245, 255, 315},
		Button { enabled, "OK" };

		/* 2: Cancel button */
		{235, 155, 255, 225},
		Button { enabled, "Cancel" };

		/* 3: Name label */
		{15, 15, 31, 90},
		StaticText { disabled, "Name:" };

		/* 4: Name field */
		{15, 95, 31, 315},
		EditText { enabled, "" };

		/* 5: Host label */
		{45, 15, 61, 90},
		StaticText { disabled, "Host:" };

		/* 6: Host field */
		{45, 95, 61, 315},
		EditText { enabled, "" };

		/* 7: Port label */
		{75, 15, 91, 90},
		StaticText { disabled, "Port:" };

		/* 8: Port field */
		{75, 95, 91, 165},
		EditText { enabled, "23" };

		/* 9: Username label */
		{105, 15, 121, 90},
		StaticText { disabled, "Username:" };

		/* 10: Username field */
		{105, 95, 121, 255},
		EditText { enabled, "" };

		/* 11: Terminal label */
		{140, 15, 156, 90},
		StaticText { disabled, "Terminal:" };

		/* 12: Terminal type button */
		{138, 95, 158, 205},
		Button { enabled, "Default" };

		/* 13: Font label */
		{170, 15, 186, 90},
		StaticText { disabled, "Font:" };

		/* 14: Font button */
		{168, 95, 188, 205},
		Button { enabled, "Default" };
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
		StaticText { disabled, "DNS Server:" };

		/* IP address field */
		{15, 115, 31, 280},
		EditText { enabled, "1.1.1.1" };

		/* Info text */
		{50, 15, 66, 280},
		StaticText { disabled, "Enter IP address (default: 1.1.1.1)" };
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

/* File reference - APPL type, icon 0 */
data 'FREF' (128) {
	$"4150 504C 0000 00"                                  /* APPL... */
};

/* Bundle - associates creator 'FLYN' with icon and file ref */
data 'BNDL' (128) {
	$"464C 594E"                                          /* FLYN */
	$"0000"                                               /* owner ID */
	$"0001"                                               /* 2 types */
	$"4652 4546"                                          /* FREF */
	$"0000"                                               /* 1 entry */
	$"0000 0080"                                          /* local 0 -> res 128 */
	$"4943 4E23"                                          /* ICN# */
	$"0000"                                               /* 1 entry */
	$"0000 0080"                                          /* local 0 -> res 128 */
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
	512 * 1024,
	384 * 1024
};
