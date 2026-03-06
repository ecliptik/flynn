/*
 * telnet.r - Resources for Flynn Telnet client
 */

#include "Menus.r"
#include "Dialogs.r"
#include "Processes.r"

resource 'MBAR' (128) {
	{ 128, 129, 130, 131 }
};

resource 'MENU' (128, "Apple") {
	128, textMenuProc, allEnabled, enabled, apple,
	{
		"About Flynn\311", noIcon, noKey, noMark, plain
	}
};

resource 'MENU' (129, "Session") {
	129, textMenuProc, allEnabled, enabled, "Session",
	{
		"Connect\311", noIcon, "N", noMark, plain;
		"Disconnect", noIcon, noKey, noMark, plain;
		"-", noIcon, noKey, noMark, plain;
		"Bookmarks\311", noIcon, "B", noMark, plain;
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
		"Clear", noIcon, noKey, noMark, plain
	}
};

resource 'MENU' (131, "Font") {
	131, textMenuProc, allEnabled, enabled, "Font",
	{
		"Monaco 9", noIcon, noKey, check, plain;
		"Monaco 12", noIcon, noKey, noMark, plain
	}
};

/* Connection dialog */
resource 'DLOG' (129, "Connect") {
	{60, 80, 210, 420},
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
		{115, 250, 135, 320},
		Button { enabled, "Connect" };

		/* Cancel button */
		{115, 165, 135, 235},
		Button { enabled, "Cancel" };

		/* Host label + field */
		{15, 15, 31, 60},
		StaticText { disabled, "Host:" };

		{15, 65, 31, 325},
		EditText { enabled, "" };

		/* Port label + field */
		{45, 15, 61, 60},
		StaticText { disabled, "Port:" };

		{45, 65, 61, 135},
		EditText { enabled, "23" };

		/* Info text */
		{80, 15, 96, 325},
		StaticText { disabled, "Enter hostname or IP address" };
	}
};

/* About dialog */
resource 'DLOG' (130, "About Flynn") {
	{80, 100, 280, 400},
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
		{165, 115, 185, 185},
		Button { enabled, "OK" };

		/* App name */
		{15, 60, 35, 240},
		StaticText { disabled, "Flynn" };

		/* Version */
		{40, 60, 56, 240},
		StaticText { disabled, "Version 0.9.1" };

		/* Description */
		{65, 20, 81, 280},
		StaticText { disabled, "A Telnet client for classic Macintosh" };

		/* Copyright */
		{90, 40, 106, 260},
		StaticText { disabled, "\0xA9 2026 Micheal Waltz" };

		/* Credits */
		{115, 30, 131, 270},
		StaticText { disabled, "Built with Claude Code + Retro68" };

		/* Website */
		{138, 30, 154, 270},
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
	{70, 90, 230, 420},
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
		/* OK button */
		{125, 245, 145, 315},
		Button { enabled, "OK" };

		/* Cancel button */
		{125, 155, 145, 225},
		Button { enabled, "Cancel" };

		/* Name label + field */
		{15, 15, 31, 60},
		StaticText { disabled, "Name:" };

		{15, 65, 31, 315},
		EditText { enabled, "" };

		/* Host label + field */
		{50, 15, 66, 60},
		StaticText { disabled, "Host:" };

		{50, 65, 66, 315},
		EditText { enabled, "" };

		/* Port label + field */
		{85, 15, 101, 60},
		StaticText { disabled, "Port:" };

		{85, 65, 101, 135},
		EditText { enabled, "23" };
	}
};

/* Application icon - Macintosh Plus with >_ prompt */
data 'ICN#' (128) {
	/* Icon bitmap (32x32) - Mac Plus, white screen, black >_ */
	$"00000000 03FFFF00"
	$"07FFFF80 0FFFFFC0"
	$"0E0001C0 0C0000C0"
	$"0C0000C0 0C8000C0"
	$"0CC000C0 0C6000C0"
	$"0CC000C0 0C8000C0"
	$"0C0000C0 0C0F80C0"
	$"0C0000C0 0E0001C0"
	$"0FFFFFC0 0C0000C0"
	$"0C0000C0 0C7FF0C0"
	$"0C7FF0C0 0C0000C0"
	$"0C0000C0 0FFFFFC0"
	$"07FFFF80 03FFFF00"
	$"00000000 01C01C00"
	$"01C01C00 00000000"
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
	$"00000000 01C01C00"
	$"01C01C00 00000000"
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
