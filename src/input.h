/*
 * input.h - Keyboard and mouse input handling for Flynn
 * Extracted from main.c
 */

#ifndef INPUT_H
#define INPUT_H

#include <Events.h>
#include "session.h"

/* Buffer data for batched TCP send */
void buffer_key_send(Session *s, const char *data, short len);

/* Flush buffered keystrokes to TCP connection */
void flush_key_send(Session *s);

/* Handle a keyDown/autoKey event */
void handle_key_down(Session *s, EventRecord *event);

/* Handle mouse click in terminal content area */
void handle_content_click(Session *s, EventRecord *event);

/* Convert pixel coordinate to terminal row */
short pixel_to_row(Session *s, short v);

/* Convert pixel coordinate to terminal column */
short pixel_to_col(Session *s, short h);

#endif /* INPUT_H */
