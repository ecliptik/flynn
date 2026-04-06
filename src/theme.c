/*
 * theme.c - Theme engine for Flynn
 *
 * Manages named color themes with cached RGBColor application.
 * Light and Dark themes work on monochrome (no Color QD traps).
 * Color themes require runtime Color QuickDraw detection.
 */

#ifdef FLYNN_THEMES

#include <Quickdraw.h>
#include <Multiverse.h>
#include "theme.h"
#include "color.h"

/* Include all theme definitions */
#include "themes/light.h"
#include "themes/dark.h"
#ifdef FLYNN_COLOR
#include "themes/solarized_light.h"
#include "themes/solarized_dark.h"
#include "themes/tokyo_light.h"
#include "themes/tokyo_dark.h"
#include "themes/green_screen.h"
#include "themes/classic.h"
#include "themes/platinum.h"
#endif

/* Theme table -- mono themes first, color themes after */
static const TerminalTheme *theme_table[] = {
	&theme_light,
	&theme_dark,
#ifdef FLYNN_COLOR
	&theme_solarized_light,
	&theme_solarized_dark,
	&theme_tokyo_light,
	&theme_tokyo_dark,
	&theme_green_screen,
	&theme_classic,
	&theme_platinum,
#endif
};

/* Active theme state */
static short g_theme_index = THEME_LIGHT;
static const TerminalTheme *g_theme = &theme_light;

/* Cached RGB values to skip redundant Color Manager traps.
 * Each draw pass calls theme_reset_cache() to invalidate. */
static unsigned char g_cache_fg_r, g_cache_fg_g, g_cache_fg_b;
static unsigned char g_cache_bg_r, g_cache_bg_g, g_cache_bg_b;
static short g_cache_fg_valid;
static short g_cache_bg_valid;

void
theme_init(short theme_id)
{
	short count;

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
	return theme_table[index];
}

short
theme_usable_count(void)
{
#ifdef FLYNN_COLOR
	if (g_has_color_qd)
		return THEME_COUNT;
#endif
	return THEME_COUNT_MONO;
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
