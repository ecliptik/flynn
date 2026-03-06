/*
 * telnet.r - Resources for Flynn Telnet client
 */

#include "Menus.r"
#include "Dialogs.r"
#include "Processes.r"

resource 'MBAR' (128) {
	{ 128, 129, 130 }
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
	{80, 100, 260, 400},
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
		{145, 115, 165, 185},
		Button { enabled, "OK" };

		/* App name */
		{15, 60, 35, 240},
		StaticText { disabled, "Flynn" };

		/* Version */
		{40, 60, 56, 240},
		StaticText { disabled, "Version 0.5.0" };

		/* Description */
		{65, 20, 81, 280},
		StaticText { disabled, "A Telnet client for classic Macintosh" };

		/* Copyright */
		{90, 40, 106, 260},
		StaticText { disabled, "\0xA9 2026 Micheal Waltz" };

		/* Credits */
		{115, 30, 131, 270},
		StaticText { disabled, "Built with Claude Code + Retro68" };
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
	alertPositionMainScreen
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
