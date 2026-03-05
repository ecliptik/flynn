/*
 * main.c - Telnet client for classic Macintosh
 * Targeting System 6.0.8 / Macintosh Plus with MacTCP 2.1
 */

#include <Quickdraw.h>
#include <Fonts.h>
#include <Events.h>
#include <Windows.h>
#include <Menus.h>
#include <TextEdit.h>
#include <Dialogs.h>
#include <Memory.h>
#include <SegLoad.h>
#include <OSUtils.h>
#include <ToolUtils.h>
#include <Resources.h>
#include <Multiverse.h>

#include "main.h"

/* Globals */
static MenuHandle apple_menu, file_menu, edit_menu;
static WindowPtr term_window;
static Boolean running = true;

/* Forward declarations */
static void init_toolbox(void);
static void init_menus(void);
static void init_window(void);
static void main_event_loop(void);
static Boolean handle_menu(long menu_id);
static void handle_mouse_down(EventRecord *event);
static void handle_key_down(EventRecord *event);
static void handle_update(EventRecord *event);
static void handle_activate(EventRecord *event);
static void do_about(void);
static void do_connect(void);
static void do_disconnect(void);

int
main(void)
{
	init_toolbox();
	init_menus();
	init_window();
	main_event_loop();
	return 0;
}

static void
init_toolbox(void)
{
	SetApplLimit(LMGetApplLimit() - (1024 * 8));
	InitGraf(&qd.thePort);
	InitFonts();
	FlushEvents(everyEvent, 0);
	InitWindows();
	InitMenus();
	TEInit();
	InitDialogs(0L);
	InitCursor();
	MaxApplZone();
}

static void
init_menus(void)
{
	Handle mbar;

	mbar = GetNewMBar(MBAR_ID);
	if (!mbar) {
		SysBeep(10);
		ExitToShell();
	}

	SetMenuBar(mbar);

	apple_menu = GetMenuHandle(APPLE_MENU_ID);
	if (apple_menu)
		AppendResMenu(apple_menu, 'DRVR');

	file_menu = GetMenuHandle(FILE_MENU_ID);
	edit_menu = GetMenuHandle(EDIT_MENU_ID);

	DrawMenuBar();
}

static void
init_window(void)
{
	Rect bounds;

	SetRect(&bounds, 10, 40, 10 + TERM_WIN_WIDTH, 40 + TERM_WIN_HEIGHT);
	term_window = NewWindow(0L, &bounds, "\pTelnet", true,
	    documentProc, (WindowPtr)-1L, true, 0L);

	if (term_window)
		SetPort(term_window);
}

static void
main_event_loop(void)
{
	EventRecord event;
	long wait_ticks;

	while (running) {
		wait_ticks = 5L;
		WaitNextEvent(everyEvent, &event, wait_ticks, 0L);

		switch (event.what) {
		case nullEvent:
			/* TODO: poll TCP for incoming data */
			break;
		case keyDown:
		case autoKey:
			handle_key_down(&event);
			break;
		case mouseDown:
			handle_mouse_down(&event);
			break;
		case updateEvt:
			handle_update(&event);
			break;
		case activateEvt:
			handle_activate(&event);
			break;
		case app4Evt:
			/* MultiFinder suspend/resume - handle later */
			break;
		}
	}

	ExitToShell();
}

static void
handle_key_down(EventRecord *event)
{
	char key;

	key = event->message & charCodeMask;
	if (event->modifiers & cmdKey) {
		handle_menu(MenuKey(key));
		return;
	}

	/* TODO: send keypress to terminal/telnet */
}

static void
handle_mouse_down(EventRecord *event)
{
	WindowPtr win;
	short part;

	part = FindWindow(event->where, &win);

	switch (part) {
	case inMenuBar:
		handle_menu(MenuSelect(event->where));
		break;
	case inSysWindow:
		SystemClick(event, win);
		break;
	case inDrag:
		DragWindow(win, event->where, &qd.screenBits.bounds);
		break;
	case inGoAway:
		if (TrackGoAway(win, event->where))
			running = false;
		break;
	case inContent:
		if (win != FrontWindow())
			SelectWindow(win);
		/* TODO: handle clicks in terminal content */
		break;
	}
}

static void
handle_update(EventRecord *event)
{
	WindowPtr win;
	GrafPtr old_port;

	win = (WindowPtr)event->message;
	GetPort(&old_port);
	SetPort(win);
	BeginUpdate(win);

	/* TODO: redraw terminal content */
	EraseRect(&win->portRect);

	EndUpdate(win);
	SetPort(old_port);
}

static void
handle_activate(EventRecord *event)
{
	/* TODO: handle window activation/deactivation */
}

static Boolean
handle_menu(long menu_id)
{
	short menu, item;
	Boolean handled = true;

	menu = HiWord(menu_id);
	item = LoWord(menu_id);

	switch (menu) {
	case APPLE_MENU_ID:
		if (item == APPLE_MENU_ABOUT_ID) {
			do_about();
		} else {
			Str255 da_name;
			GrafPtr save_port;

			GetMenuItemText(apple_menu, item, da_name);
			GetPort(&save_port);
			OpenDeskAcc(da_name);
			SetPort(save_port);
		}
		break;
	case FILE_MENU_ID:
		switch (item) {
		case FILE_MENU_CONNECT_ID:
			do_connect();
			break;
		case FILE_MENU_DISCONNECT_ID:
			do_disconnect();
			break;
		case FILE_MENU_QUIT_ID:
			running = false;
			break;
		}
		break;
	case EDIT_MENU_ID:
		/* TODO: handle edit menu for DA support */
		break;
	default:
		handled = false;
		break;
	}

	HiliteMenu(0);
	return handled;
}

static void
do_about(void)
{
	ParamText("\pTelnet for Macintosh\r\rA terminal client for Mac Plus",
	    "\p", "\p", "\p");
	Alert(128, 0L);
}

static void
do_connect(void)
{
	/* TODO: show connection dialog, initiate TCP connection */
}

static void
do_disconnect(void)
{
	/* TODO: close TCP connection */
}
