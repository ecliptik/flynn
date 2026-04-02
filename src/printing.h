/*
 * printing.h - Page Setup and Print for terminal sessions
 */

#ifndef PRINTING_H
#define PRINTING_H

#ifdef FLYNN_PRINTING

/* Show Page Setup dialog */
void do_page_setup(void);

/* Print active session (scrollback + screen buffer) */
void do_print(void);

#else
#define do_page_setup() ((void)0)
#define do_print() ((void)0)
#endif

#endif /* PRINTING_H */
