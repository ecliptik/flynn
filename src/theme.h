/*
 * theme.h - Theme engine for Flynn
 *
 * Provides named color themes for the terminal emulator.
 * Light and Dark themes work on monochrome systems.
 * Color themes require Color QuickDraw (Mac II+).
 */

#ifndef THEME_H
#define THEME_H

#ifdef FLYNN_THEMES

/* Compact 3-byte RGB -- expanded to RGBColor (16-bit) at runtime via x257 */
typedef struct {
	unsigned char r, g, b;
} ThemeRGB;

/* Theme color properties for a terminal emulator */
typedef struct {
	const char    *name;           /* menu display name */
	unsigned char  is_color;       /* 1 = requires Color QuickDraw */
	unsigned char  is_dark;        /* 1 = dark background theme */
	ThemeRGB       ansi[16];       /* ANSI palette overrides (indices 0-15) */
	ThemeRGB       default_fg;     /* default foreground (when COLOR_DEFAULT) */
	ThemeRGB       default_bg;     /* default background (when COLOR_DEFAULT) */
	ThemeRGB       cursor_color;   /* text cursor color */
	ThemeRGB       sel_bg;         /* selection background */
	ThemeRGB       sel_fg;         /* selection foreground */
	ThemeRGB       bold_color;     /* bold foreground override ({0,0,0} = unused) */
} TerminalTheme;

/* Theme indices -- THEME_LIGHT must be 0 for backward compat */
#define THEME_LIGHT             0
#define THEME_DARK              1
#define THEME_SOLARIZED_LIGHT   2
#define THEME_SOLARIZED_DARK    3
#define THEME_TOKYO_LIGHT       4
#define THEME_TOKYO_DARK        5
#define THEME_AMBER_CRT         6
#define THEME_SYSTEM7           7
#define THEME_COMPACT_MAC       8
#define THEME_DRACULA           9
#define THEME_NORD              10

/* Total theme count (mono themes always available, color themes conditional) */
#define THEME_COUNT_MONO   2
#ifdef FLYNN_COLOR
#define THEME_COUNT        11
#else
#define THEME_COUNT        2
#endif

/* Custom (imported) theme support */
#define MAX_CUSTOM_THEMES      4
#define CUSTOM_THEME_NAME_LEN  32
#define THEME_CUSTOM_BASE      THEME_COUNT

/* Persistent custom theme (fixed-size, stored in prefs) */
typedef struct {
	char           name[CUSTOM_THEME_NAME_LEN];
	unsigned char  in_use;
	unsigned char  is_dark;        /* auto-detected from bg luminance */
	ThemeRGB       ansi[16];       /* 48 bytes */
	ThemeRGB       default_fg;
	ThemeRGB       default_bg;
	ThemeRGB       cursor_color;
	ThemeRGB       sel_bg;
	ThemeRGB       sel_fg;
	ThemeRGB       bold_color;     /* {0,0,0} = unused for imports */
} CustomTheme;

/* Initialize theme system -- call after color_detect() and prefs_load() */
void theme_init(short theme_id);

/* Get/set active theme */
const TerminalTheme *theme_current(void);
void theme_set(short index);
short theme_get(void);

/* Get theme by index (for menu building) */
const TerminalTheme *theme_get_by_index(short index);

/* Get count of usable themes (respects runtime color detection + custom) */
short theme_usable_count(void);

/* Apply ThemeRGB to QuickDraw foreground/background (with cache) */
void theme_set_default_fg(const ThemeRGB *c);
void theme_set_default_bg(const ThemeRGB *c);

/* Invalidate color cache (call at start of each draw pass) */
void theme_reset_cache(void);

/* Is current theme dark? (for mono fallback rendering) */
short theme_is_dark(void);

/* Is current theme a color theme? */
short theme_is_color(void);

/* Restore port colors to black fg / white bg after themed drawing.
 * Must be called on every exit path that may have set theme colors. */
void theme_restore_colors(void);

/* Custom theme management */
void theme_load_custom(void);
void theme_rebuild_menu(void);
short theme_id_to_menu_item(short theme_id);
short theme_menu_item_to_id(short item);

#else /* !FLYNN_THEMES */

#define THEME_LIGHT  0
#define THEME_COUNT  0
#define MAX_CUSTOM_THEMES 0
#define theme_init(id)              ((void)0)
#define theme_current()             ((void *)0)
#define theme_set(i)                ((void)0)
#define theme_get()                 0
#define theme_get_by_index(i)       ((void *)0)
#define theme_usable_count()        0
#define theme_set_default_fg(c)     ((void)0)
#define theme_set_default_bg(c)     ((void)0)
#define theme_reset_cache()         ((void)0)
#define theme_is_dark()             0
#define theme_is_color()            0
#define theme_restore_colors()      ((void)0)
#define theme_load_custom()         ((void)0)
#define theme_rebuild_menu()        ((void)0)

#endif /* FLYNN_THEMES */

#endif /* THEME_H */
