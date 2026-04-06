/*
 * theme_import.h - Ghostty theme file import/export for Flynn
 */

#ifndef THEME_IMPORT_H
#define THEME_IMPORT_H

#ifdef FLYNN_THEMES

#include "theme.h"

/* Parse a Ghostty-format theme file buffer into a CustomTheme.
 * Returns 0 on success, -1 on parse error. */
short parse_ghostty_theme(const char *buf, long len, CustomTheme *out);

/* Import a Ghostty theme file via Standard File dialog */
void do_import_theme(void);

/* Remove a custom theme via popup selection */
void do_remove_theme(void);

/* Export the current theme in Ghostty format via Standard File dialog */
void do_export_theme(void);

#else

#define do_import_theme()   ((void)0)
#define do_remove_theme()   ((void)0)
#define do_export_theme()   ((void)0)

#endif /* FLYNN_THEMES */

#endif /* THEME_IMPORT_H */
