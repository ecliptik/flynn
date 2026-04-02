/*
 * logging.h - Session logging to file
 */

#ifndef LOGGING_H
#define LOGGING_H

#ifdef FLYNN_LOGGING

struct Session;

/* Start logging active session output to a file (shows Save dialog) */
void do_start_logging(void);

/* Stop logging the active session */
void do_stop_logging(void);

/* Write raw terminal data to session log file (no-op if not logging) */
void log_write_data(struct Session *s, unsigned char *data, short len);

/* Stop logging if active -- called on disconnect and session destroy */
void log_stop_if_active(struct Session *s);

#else
#define do_start_logging() ((void)0)
#define do_stop_logging() ((void)0)
#define log_write_data(s, d, l) ((void)0)
#define log_stop_if_active(s) ((void)0)
#endif

#endif /* LOGGING_H */
