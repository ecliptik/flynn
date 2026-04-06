# Flynn Theme Guide

This guide covers the Flynn theme system: how themes work, how to create custom themes, and how to integrate them into the build.

## Overview

Flynn uses a compile-time theme system. Each theme is a static `TerminalTheme` struct defined in a header file under `src/themes/`. Themes are compiled into the binary — there is no runtime theme loading. This keeps memory usage minimal and avoids heap allocation for theme data.

Two categories of themes exist:

- **Mono themes** (Light, Dark) — work on all systems, including the Mac Plus. These use only black and white values; color fields are present but ignored on monochrome hardware.
- **Color themes** (Solarized, Tokyo Night, Green Screen, Classic, Platinum) — require a Mac II or later with Color QuickDraw. Detected at runtime via `SysEnvirons()`. Color themes are only compiled when `FLYNN_COLOR=ON`.

## Built-in Themes

| Theme | Type | Description |
|-------|------|-------------|
| Light | Mono | White background, black text. Default theme, works everywhere. |
| Dark | Mono | Black background, white text. Uses `srcBic` rendering for flicker-free display. |
| Solarized Light | Color | Ethan Schoonover's Solarized palette, light variant. |
| Solarized Dark | Color | Solarized palette, dark variant. |
| Tokyo Night Light | Color | Based on the Tokyo Night color scheme, light variant. |
| Tokyo Night Dark | Color | Tokyo Night, dark variant. |
| Green Screen | Color | Phosphor green on black, classic CRT terminal aesthetic. |
| Classic | Color | 1990s terminal colors — standard HTML-era ANSI palette, white background. |
| Platinum | Color | Mac OS 8/9 Appearance Manager inspired — gray background, purple accents. |

## Theme Architecture

### Color Representation

Colors use a compact 3-byte `ThemeRGB` struct:

```c
typedef struct {
    unsigned char r, g, b;
} ThemeRGB;
```

At runtime, values are expanded to 16-bit `RGBColor` for QuickDraw via `x * 257` multiplication (maps 0x00-0xFF to 0x0000-0xFFFF). For best results on 256-color systems, choose colors from the 6x6x6 RGB cube (values: 0x00, 0x33, 0x66, 0x99, 0xCC, 0xFF).

### TerminalTheme Struct

Every theme defines a `TerminalTheme` struct with these fields:

```c
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
```

#### Field Reference

**Metadata:**
| Field | Purpose |
|-------|---------|
| `name` | Displayed in the Options > Theme menu. Keep it short. |
| `is_color` | Set to `1` for themes that need Color QuickDraw. Set to `0` for mono-safe themes. |
| `is_dark` | Set to `1` for dark backgrounds. Controls mono fallback rendering mode (`srcBic`). |

**ANSI palette** — overrides terminal palette indices 0-15. Colors 16-255 are unaffected and use the standard xterm palette:
| Index | Color | Index | Color |
|-------|-------|-------|-------|
| 0 | Black | 8 | Bright Black |
| 1 | Red | 9 | Bright Red |
| 2 | Green | 10 | Bright Green |
| 3 | Yellow | 11 | Bright Yellow |
| 4 | Blue | 12 | Bright Blue |
| 5 | Magenta | 13 | Bright Magenta |
| 6 | Cyan | 14 | Bright Cyan |
| 7 | White | 15 | Bright White |

**Terminal colors:**
| Field | Used for |
|-------|----------|
| `default_fg` | Default foreground when no explicit SGR color is set (`COLOR_DEFAULT`) |
| `default_bg` | Default background when no explicit SGR color is set (`COLOR_DEFAULT`) |
| `cursor_color` | Text cursor (block, underline, or bar depending on DECSCUSR style) |

**Selection:**
| Field | Used for |
|-------|----------|
| `sel_bg` | Background color for text selection highlight |
| `sel_fg` | Foreground color for selected text |

**Bold:**
| Field | Used for |
|-------|----------|
| `bold_color` | Overrides default foreground for bold text. Only applies when no explicit ANSI color is set — `\e[1m` uses `bold_color`, but `\e[1;31m` uses standard bold-to-bright promotion (`index += 8`). Set to `{0,0,0}` to leave unused. |

## Creating a Custom Theme

### Step 1: Create the Header File

Create a new file in `src/themes/`. Name it after your theme using snake_case:

```
src/themes/my_theme.h
```

Use an existing theme as a template. Here is a minimal example:

```c
/*
 * themes/my_theme.h - My Custom Theme
 * Brief description of the theme's aesthetic.
 * Requires Color QuickDraw.
 */

static const TerminalTheme theme_my_theme = {
    "My Theme",     /* name */
    1,              /* is_color (1 = color, 0 = mono) */
    0,              /* is_dark  (1 = dark,  0 = light) */

    /* ANSI palette (indices 0-15) */
    {
        { 0x00, 0x00, 0x00 },  /*  0: black */
        { 0xCC, 0x00, 0x00 },  /*  1: red */
        { 0x00, 0xCC, 0x00 },  /*  2: green */
        { 0xCC, 0xCC, 0x00 },  /*  3: yellow */
        { 0x00, 0x00, 0xCC },  /*  4: blue */
        { 0xCC, 0x00, 0xCC },  /*  5: magenta */
        { 0x00, 0xCC, 0xCC },  /*  6: cyan */
        { 0xCC, 0xCC, 0xCC },  /*  7: white */
        { 0x66, 0x66, 0x66 },  /*  8: bright black */
        { 0xFF, 0x33, 0x33 },  /*  9: bright red */
        { 0x33, 0xFF, 0x33 },  /* 10: bright green */
        { 0xFF, 0xFF, 0x33 },  /* 11: bright yellow */
        { 0x33, 0x33, 0xFF },  /* 12: bright blue */
        { 0xFF, 0x33, 0xFF },  /* 13: bright magenta */
        { 0x33, 0xFF, 0xFF },  /* 14: bright cyan */
        { 0xFF, 0xFF, 0xFF },  /* 15: bright white */
    },

    { 0x33, 0x33, 0x33 },  /* default_fg */
    { 0xFF, 0xFF, 0xFF },  /* default_bg */
    { 0x33, 0x33, 0x33 },  /* cursor_color */
    { 0x00, 0x66, 0xCC },  /* sel_bg */
    { 0xFF, 0xFF, 0xFF },  /* sel_fg */
    { 0x00, 0x00, 0x00 },  /* bold_color (unused) */
};
```

### Step 2: Register the Theme

Three files need changes to register a new theme.

**`src/theme.h`** — add a theme index constant and update the count:

```c
/* Add after the last existing theme index */
#define THEME_MY_THEME          9

/* Update THEME_COUNT (inside the #ifdef FLYNN_COLOR block) */
#define THEME_COUNT        10    /* was 9 */
```

Theme indices must be sequential starting from 0. Mono themes occupy indices 0-1; color themes follow.

**`src/theme.c`** — include the header and add to the theme table:

```c
/* Add with the other color theme includes */
#ifdef FLYNN_COLOR
#include "themes/my_theme.h"
#endif

/* Add to theme_table[] */
static const TerminalTheme *theme_table[] = {
    &theme_light,
    &theme_dark,
#ifdef FLYNN_COLOR
    &theme_solarized_light,
    /* ... existing themes ... */
    &theme_platinum,
    &theme_my_theme,        /* new */
#endif
};
```

The order in `theme_table[]` must match the `#define` indices.

**`resources/telnet.r`** — add the menu item to the Theme menu (MENU 138):

```rez
resource 'MENU' (138, "Theme") {
    138, textMenuProc, allEnabled, enabled, "Theme",
    {
        /* ... existing items ... */
        "Platinum", noIcon, noKey, noMark, plain;
        "My Theme", noIcon, noKey, noMark, plain   /* new */
    }
};
```

### Step 3: Update Menu Constants

In `src/main.h`, add a menu item constant for the new theme:

```c
#define THEME_ITEM_MY_THEME        11
#define THEME_ITEM_LAST            11    /* was 10 */
```

The menu item numbers account for the separator between mono and color themes (item 3), so color theme items start at 4.

### Step 4: Build and Test

Build with the `full` preset to include color support:

```bash
./scripts/build.sh --preset full
```

The theme will appear in Options > Theme. On monochrome systems, color themes are automatically grayed out in the menu.

## Design Guidelines

### Color Contrast

Ensure sufficient contrast between text colors and the background. Terminal content is text-heavy — readability is the primary concern. Test with syntax-highlighted code, directory listings (`ls --color`), and full-screen applications like `vi` and `tmux`.

### Dark Themes

Set `is_dark = 1` for dark-background themes. This flag controls:
- **Mono fallback**: On monochrome systems, dark themes use `PaintRect()` + `TextMode(srcBic)` for white-on-black rendering instead of `EraseRect()` + `TextMode(srcOr)` for black-on-white.
- **Default color inversion**: Dark themes swap default foreground/background semantics for cells with no explicit color.

### ANSI Palette Design

The 16 ANSI colors must be distinguishable from each other and from the background. These colors are used pervasively by terminal applications:
- **Colors 0-7** (normal): standard intensity ANSI colors. Used for most syntax highlighting and UI chrome.
- **Colors 8-15** (bright): bold-to-bright promotion maps `index += 8` for bold text with explicit ANSI colors 0-7.
- Test with `colortest` scripts, `htop`, `git diff`, `ls --color`, and vim color schemes.

### Selection and Cursor Visibility

- `sel_bg` and `sel_fg` should have high contrast with each other. Selection is temporary but must be clearly visible against any combination of ANSI foreground/background colors in the terminal.
- `cursor_color` must be visible against both the default background and common ANSI background colors.

### Bold Color Behavior

The `bold_color` field only overrides the default foreground when bold attribute is active:
- `\e[1m` (bold, no explicit color) — uses `bold_color` if non-zero, otherwise uses `default_fg`.
- `\e[1;31m` (bold + explicit red) — uses standard bold-to-bright promotion (`eff_fg += 8`), `bold_color` is ignored.
- Set `bold_color` to `{0,0,0}` to leave it unused. Themes like Solarized and Green Screen define `bold_color` for emphasized default text; others leave it zeroed.

### 256-Color Palette

For maximum compatibility on 256-color Macs, stick to the 6x6x6 RGB cube. The safe values for each channel are:

```
0x00  0x33  0x66  0x99  0xCC  0xFF
```

Colors outside this cube may dither on 8-bit displays. The built-in themes all use cube-safe values. Note that themes only override ANSI colors 0-15 — the 216-color cube (indices 16-231) and 24-step grayscale ramp (indices 232-255) are always the standard xterm palette.

### Mono Theme Constraints

If creating a mono theme (`is_color = 0`):
- All color fields must be either `{ 0x00, 0x00, 0x00 }` (black) or `{ 0xFF, 0xFF, 0xFF }` (white).
- The theme will be available on all systems including the Mac Plus.
- Mono themes are always compiled regardless of `FLYNN_COLOR`.

## Build System

Themes are controlled by two feature flags:

| Flag | Default | Effect |
|------|---------|--------|
| `FLYNN_THEMES` | ON | Compiles the theme engine (`theme.c`). When OFF, all theme API calls become no-ops via macros — zero overhead. |
| `FLYNN_COLOR` | OFF | Compiles Color QuickDraw support (`color.c`) and all color themes. When OFF, only Light and Dark are available. |

Build presets:

| Preset | `FLYNN_THEMES` | `FLYNN_COLOR` | Available Themes |
|--------|----------------|---------------|------------------|
| `full` | ON | ON | All 9 (mono + color) |
| `lite` | ON | OFF | Light, Dark only |
| `minimal` | ON | OFF | Light, Dark only |

Override with CLI flags:

```bash
./scripts/build.sh --preset lite --color     # lite features + color themes
./scripts/build.sh --no-themes               # disable theme engine entirely
```

## File Reference

```
src/
  theme.h               # TerminalTheme struct, theme API, index constants, no-op macros
  theme.c               # Theme engine: init, get/set, RGB cache, theme_table[], restore
  color.h               # Color QuickDraw detection (g_has_color_qd)
  color.c               # Runtime SysEnvirons() detection, color_get_rgb() (theme-aware for 0-15)
  themes/
    light.h             # Light (mono, default)
    dark.h              # Dark (mono)
    solarized_light.h   # Solarized Light (color)
    solarized_dark.h    # Solarized Dark (color)
    tokyo_light.h       # Tokyo Night Light (color)
    tokyo_dark.h        # Tokyo Night Dark (color)
    green_screen.h      # Green Screen (color)
    classic.h           # Classic (color)
    platinum.h          # Platinum (color)
resources/
  telnet.r              # Theme menu (MENU 138)
```
