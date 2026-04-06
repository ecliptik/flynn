/*
 * theme.c - Theme engine for Flynn
 *
 * Manages named color themes with cached RGBColor application.
 * Light and Dark themes work on monochrome (no Color QD traps).
 * Color themes require runtime Color QuickDraw detection.
 * Supports up to 4 imported custom themes (Ghostty format).
 */

#ifdef FLYNN_THEMES

#include <Quickdraw.h>
#include <Menus.h>
#include <Memory.h>
#include <Multiverse.h>
#include <string.h>
#include "theme.h"
#include "color.h"
#include "settings.h"
#include "main.h"

extern FlynnPrefs prefs;

/* Include all theme definitions */
#include "themes/light.h"
#include "themes/dark.h"
#ifdef FLYNN_COLOR
#include "themes/solarized_light.h"
#include "themes/solarized_dark.h"
#include "themes/tokyonight_day.h"
#include "themes/tokyonight.h"
#include "themes/amber_crt.h"
#include "themes/system7.h"
#include "themes/compact_mac.h"
#include "themes/dracula.h"
#include "themes/nord.h"
#endif

/* Fixed-size theme table: built-in + custom slots */
static const TerminalTheme *theme_table[THEME_COUNT + MAX_CUSTOM_THEMES];

/* Runtime TerminalTheme wrappers for custom themes.
 * The .name pointers reference prefs.custom_themes[].name directly. */
static TerminalTheme g_custom_runtime[MAX_CUSTOM_THEMES];

/* Count of currently loaded custom themes */
static short g_custom_count = 0;

/* Active theme state */
static short g_theme_index = THEME_LIGHT;
static const TerminalTheme *g_theme = &theme_light;

/* Cached RGB values to skip redundant Color Manager traps.
 * Each draw pass calls theme_reset_cache() to invalidate. */
static unsigned char g_cache_fg_r, g_cache_fg_g, g_cache_fg_b;
static unsigned char g_cache_bg_r, g_cache_bg_g, g_cache_bg_b;
static short g_cache_fg_valid;
static short g_cache_bg_valid;

/* Theme submenu handle (shared with menus.c) */
extern MenuHandle theme_submenu;

/*
 * Initialize the built-in theme table entries.
 * Called once at startup before theme_load_custom().
 */
static void
theme_init_table(void)
{
	short i;

	/* Zero all slots */
	for (i = 0; i < THEME_COUNT + MAX_CUSTOM_THEMES; i++)
		theme_table[i] = 0L;

	/* Built-in themes */
	theme_table[THEME_LIGHT] = &theme_light;
	theme_table[THEME_DARK] = &theme_dark;
#ifdef FLYNN_COLOR
	theme_table[THEME_SOLARIZED_LIGHT] = &theme_solarized_light;
	theme_table[THEME_SOLARIZED_DARK] = &theme_solarized_dark;
	theme_table[THEME_TOKYO_LIGHT] = &theme_tokyo_light;
	theme_table[THEME_TOKYO_DARK] = &theme_tokyo_dark;
	theme_table[THEME_AMBER_CRT] = &theme_amber_crt;
	theme_table[THEME_SYSTEM7] = &theme_system7;
	theme_table[THEME_COMPACT_MAC] = &theme_compact_mac;
	theme_table[THEME_DRACULA] = &theme_dracula;
	theme_table[THEME_NORD] = &theme_nord;
#endif
}

void
theme_init(short theme_id)
{
	short count;

	theme_init_table();
	theme_load_custom();

	count = theme_usable_count();
	if (theme_id < 0 || theme_id >= count)
		theme_id = THEME_LIGHT;

	g_theme_index = theme_id;
	g_theme = theme_table[theme_id];
	g_cache_fg_valid = 0;
	g_cache_bg_valid = 0;
}

const TerminalTheme *
theme_current(void)
{
	return g_theme;
}

void
theme_set(short index)
{
	short count;

	count = theme_usable_count();
	if (index < 0 || index >= count)
		return;
	if (!theme_table[index])
		return;

	g_theme_index = index;
	g_theme = theme_table[index];
	g_cache_fg_valid = 0;
	g_cache_bg_valid = 0;
}

short
theme_get(void)
{
	return g_theme_index;
}

const TerminalTheme *
theme_get_by_index(short index)
{
	short count;

	count = theme_usable_count();
	if (index < 0 || index >= count)
		return &theme_light;
	if (!theme_table[index])
		return &theme_light;
	return theme_table[index];
}

short
theme_usable_count(void)
{
#ifdef FLYNN_COLOR
	if (g_has_color_qd)
		return THEME_COUNT + g_custom_count;
#endif
	return THEME_COUNT_MONO;
}

/*
 * theme_load_custom - Load custom themes from prefs into runtime table.
 *
 * Copies CustomTheme data from prefs into g_custom_runtime[] TerminalTheme
 * structs and sets the theme_table[] pointers. Called at startup and after
 * import/remove operations.
 */
void
theme_load_custom(void)
{
	short i;

	g_custom_count = 0;

	for (i = 0; i < MAX_CUSTOM_THEMES; i++) {
		if (prefs.custom_themes[i].in_use) {
			TerminalTheme *rt = &g_custom_runtime[i];

			rt->name = prefs.custom_themes[i].name;
			rt->is_color = 1;
			rt->is_dark = prefs.custom_themes[i].is_dark;
			memcpy(rt->ansi, prefs.custom_themes[i].ansi,
			    sizeof(ThemeRGB) * 16);
			rt->default_fg = prefs.custom_themes[i].default_fg;
			rt->default_bg = prefs.custom_themes[i].default_bg;
			rt->cursor_color =
			    prefs.custom_themes[i].cursor_color;
			rt->sel_bg = prefs.custom_themes[i].sel_bg;
			rt->sel_fg = prefs.custom_themes[i].sel_fg;
			rt->bold_color = prefs.custom_themes[i].bold_color;
			theme_table[THEME_COUNT + i] = rt;
			g_custom_count++;
		} else {
			theme_table[THEME_COUNT + i] = 0L;
		}
	}
}

/*
 * theme_rebuild_menu - Rebuild dynamic theme menu items.
 *
 * Deletes everything after the last built-in item (THEME_ITEM_LAST),
 * then appends: separator, custom theme names, separator,
 * Import/Remove/Export actions.
 */
void
theme_rebuild_menu(void)
{
	short count, i, nlen;
	Str255 ps;

	if (!theme_submenu)
		return;

	/* Remove all items after THEME_ITEM_LAST */
	count = CountMItems(theme_submenu);
	while (count > THEME_ITEM_LAST) {
		DeleteMenuItem(theme_submenu, count);
		count--;
	}

	/* Custom themes section */
	if (g_custom_count > 0) {
		AppendMenu(theme_submenu, "\p(-");
		for (i = 0; i < MAX_CUSTOM_THEMES; i++) {
			if (!prefs.custom_themes[i].in_use)
				continue;
			nlen = strlen(prefs.custom_themes[i].name);
			if (nlen > 254) nlen = 254;
			ps[0] = (unsigned char)nlen;
			memcpy(ps + 1,
			    prefs.custom_themes[i].name, nlen);
			/* AppendMenu interprets metacharacters in the
			 * string; use a dummy then SetMenuItemText
			 * to avoid issues with theme names containing
			 * '/', ';', '(' etc. */
			AppendMenu(theme_submenu, "\p ");
			SetMenuItemText(theme_submenu,
			    CountMItems(theme_submenu), ps);
		}
	}

	/* Action items */
	AppendMenu(theme_submenu, "\p(-");
	AppendMenu(theme_submenu, "\pImport Theme\311");
	if (g_custom_count > 0)
		AppendMenu(theme_submenu, "\pRemove Theme\311");
	AppendMenu(theme_submenu, "\pExport Theme\311");
}

/*
 * theme_id_to_menu_item - Map theme index to menu item number.
 *
 * Built-in: 0-1 → items 1-2, 2-10 → items 4-12 (separator at 3).
 * Custom: THEME_COUNT+slot → dynamic item after separator.
 */
short
theme_id_to_menu_item(short theme_id)
{
	short item;

	if (theme_id < THEME_COUNT_MONO) {
		/* Light/Dark: items 1-2 */
		return theme_id + 1;
	} else if (theme_id < THEME_COUNT) {
		/* Built-in color: items 4+ (skip separator at 3) */
		return theme_id + 2;
	} else if (theme_id >= THEME_COUNT &&
	    theme_id < THEME_COUNT + MAX_CUSTOM_THEMES) {
		/* Custom theme: after THEME_ITEM_LAST + separator,
		 * sequential among occupied slots */
		short slot = theme_id - THEME_COUNT;
		short i, pos;

		if (!prefs.custom_themes[slot].in_use)
			return -1;

		/* Count occupied slots before this one */
		pos = 0;
		for (i = 0; i < slot; i++) {
			if (prefs.custom_themes[i].in_use)
				pos++;
		}
		/* THEME_ITEM_LAST + 1 (separator) + 1-based */
		item = THEME_ITEM_LAST + 1 + pos + 1;
		return item;
	}
	return -1;
}

/*
 * theme_menu_item_to_id - Map menu item number to theme index.
 *
 * Returns theme index (0-14), or -1 for separator,
 * -2 for Import, -3 for Remove, -4 for Export.
 */
short
theme_menu_item_to_id(short item)
{
	short custom_base, custom_end, action_sep;
	short i, pos, slot;

	/* Built-in mono themes */
	if (item >= 1 && item <= THEME_ITEM_DARK)
		return item - 1;

	/* Separator */
	if (item == 3)
		return -1;

	/* Built-in color themes */
	if (item >= THEME_ITEM_SOLARIZED_LIGHT &&
	    item <= THEME_ITEM_LAST)
		return item - 2;

	/* Dynamic section: custom themes + actions */
	if (g_custom_count > 0) {
		custom_base = THEME_ITEM_LAST + 2; /* after separator */
		custom_end = custom_base + g_custom_count - 1;
		action_sep = custom_end + 1;

		if (item >= custom_base && item <= custom_end) {
			/* Map sequential position to slot index */
			pos = item - custom_base;
			slot = -1;
			for (i = 0; i < MAX_CUSTOM_THEMES; i++) {
				if (prefs.custom_themes[i].in_use) {
					if (pos == 0) {
						slot = i;
						break;
					}
					pos--;
				}
			}
			if (slot >= 0)
				return THEME_COUNT + slot;
			return -1;
		}

		/* Action items after custom themes */
		if (item == action_sep + 1) return -2; /* Import */
		if (item == action_sep + 2) return -3; /* Remove */
		if (item == action_sep + 3) return -4; /* Export */
	} else {
		/* No custom themes: actions after THEME_ITEM_LAST */
		action_sep = THEME_ITEM_LAST + 1; /* separator */
		if (item == action_sep + 1) return -2; /* Import */
		/* No Remove when no customs */
		if (item == action_sep + 2) return -4; /* Export */
	}

	return -1;
}

/*
 * theme_set_default_fg - Apply ThemeRGB as QuickDraw foreground color.
 *
 * Only calls RGBForeColor when FLYNN_COLOR is defined and
 * Color QuickDraw is available. Caches the last-set value
 * to skip redundant trap calls (significant on 68000).
 */
void
theme_set_default_fg(const ThemeRGB *c)
{
#ifdef FLYNN_COLOR
	RGBColor rgb;

	if (!g_has_color_qd)
		return;

	/* Skip if same as cached value */
	if (g_cache_fg_valid &&
	    g_cache_fg_r == c->r &&
	    g_cache_fg_g == c->g &&
	    g_cache_fg_b == c->b)
		return;

	rgb.red   = (unsigned short)c->r * 257;
	rgb.green = (unsigned short)c->g * 257;
	rgb.blue  = (unsigned short)c->b * 257;
	RGBForeColor(&rgb);

	g_cache_fg_r = c->r;
	g_cache_fg_g = c->g;
	g_cache_fg_b = c->b;
	g_cache_fg_valid = 1;
#else
	(void)c;
#endif
}

/*
 * theme_set_default_bg - Apply ThemeRGB as QuickDraw background color.
 *
 * Same caching strategy as theme_set_default_fg.
 */
void
theme_set_default_bg(const ThemeRGB *c)
{
#ifdef FLYNN_COLOR
	RGBColor rgb;

	if (!g_has_color_qd)
		return;

	/* Skip if same as cached value */
	if (g_cache_bg_valid &&
	    g_cache_bg_r == c->r &&
	    g_cache_bg_g == c->g &&
	    g_cache_bg_b == c->b)
		return;

	rgb.red   = (unsigned short)c->r * 257;
	rgb.green = (unsigned short)c->g * 257;
	rgb.blue  = (unsigned short)c->b * 257;
	RGBBackColor(&rgb);

	g_cache_bg_r = c->r;
	g_cache_bg_g = c->g;
	g_cache_bg_b = c->b;
	g_cache_bg_valid = 1;
#else
	(void)c;
#endif
}

void
theme_reset_cache(void)
{
	g_cache_fg_valid = 0;
	g_cache_bg_valid = 0;
}

short
theme_is_dark(void)
{
	return g_theme ? g_theme->is_dark : 0;
}

short
theme_is_color(void)
{
	return g_theme ? g_theme->is_color : 0;
}

/*
 * theme_restore_colors - Restore port to black fg / white bg.
 *
 * On Color QD systems, uses RGBForeColor/RGBBackColor to ensure
 * themed colors don't leak into chrome drawing (menus, title bars,
 * dialogs, scrollbars). Invalidates the theme color cache since
 * the port colors now differ from any cached theme values.
 */
void
theme_restore_colors(void)
{
#ifdef FLYNN_COLOR
	if (g_has_color_qd) {
		RGBColor black = { 0, 0, 0 };
		RGBColor white = { 0xFFFF, 0xFFFF, 0xFFFF };

		RGBForeColor(&black);
		RGBBackColor(&white);
		g_cache_fg_valid = 0;
		g_cache_bg_valid = 0;
	}
#endif
}

#endif /* FLYNN_THEMES */
