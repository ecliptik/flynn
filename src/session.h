/*
 * session.h - Multi-session support for Flynn
 */

#ifndef SESSION_H
#define SESSION_H

#include <Windows.h>
#include "terminal.h"
#include "terminal_ui.h"
#include "connection.h"
#include "telnet.h"

#define MAX_SESSIONS	4

typedef struct Session {
	/* Window */
	WindowPtr	window;

	/* Core protocol state */
	Connection	conn;
	TelnetState	telnet;
	Terminal	terminal;

	/* Per-session UI state (cursor blink, selection) */
	UIState		ui;

	/* Per-session display settings */
	short		font_id;
	short		font_size;
	short		cell_width;
	short		cell_height;
	short		cell_baseline;

	/* Keystroke send buffer */
	char		key_send_buf[256];
	short		key_send_len;

	/* Session slot index (0..MAX_SESSIONS-1) */
	short		id;
} Session;

/* Create a new session with window. Returns NULL on failure. */
Session *session_new(void);

/* Destroy session: disconnect, dispose window, free memory. */
void session_destroy(Session *s);

/* Look up session from WindowPtr. Returns NULL if not a session window. */
Session *session_from_window(WindowPtr win);

/* Get session count */
short session_count(void);

/* Get session by slot index (NULL if empty) */
Session *session_get(short index);

/* Check if any session is currently connected */
Boolean session_any_connected(void);

#endif /* SESSION_H */
