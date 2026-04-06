# Flynn Theme Guide

This guide covers the Flynn theme system: how themes work, how to create custom themes, how importing works, and known limitations.

## Overview

Flynn uses a compile-time theme system for built-in themes and a runtime import system for custom themes. Each built-in theme is a static `TerminalTheme` struct defined in a header file under `src/themes/`. Custom themes can be imported from Ghostty-format files at runtime and are stored in preferences.

Two categories of built-in themes exist:

- **Mono themes** (Light, Dark) — work on all systems, including the Mac Plus. These use only black and white values; color fields are present but ignored on monochrome hardware.
- **Color themes** — require a Mac II or later with Color QuickDraw. Detected at runtime via `SysEnvirons()`. Color themes are only compiled when `FLYNN_COLOR=ON`.

## Built-in Themes

| Theme | Type | Description |
|-------|------|-------------|
| Light | Mono | White background, black text. Default theme, works everywhere. |
| Dark | Mono | Black background, white text. Uses `srcBic` rendering for flicker-free display. |
| Solarized Light | Color | Ethan Schoonover's Solarized palette, light variant. Canonical base colors, cube-snapped accents. |
| Solarized Dark | Color | Solarized palette, dark variant. Canonical base colors, cube-snapped accents. |
| TokyoNight Day | Color | Based on the TokyoNight color scheme, light variant. Cube-snapped. |
| TokyoNight | Color | TokyoNight, dark variant. Cube-snapped. |
| Amber CRT | Color | Amber phosphor terminal aesthetic. All ANSI colors warm-shifted. Cube-snapped. |
| System 7 | Color | Mac 16-color System CLUT palette. Canonical Mac system colors. |
| Compact Mac | Color | P7 phosphor CRT warmth (Mac Plus/SE/Classic). Cube-snapped. |
| Dracula | Color | Canonical Dracula palette from draculatheme.com. |
| Nord | Color | Canonical Nord palette from nordtheme.com. |
| Green Screen | Color | Phosphor green on black, classic CRT terminal aesthetic. Cube-snapped. |
| Classic | Color | 1990s terminal colors — standard HTML-era ANSI palette, white background. Cube-snapped. |
| Monokai | Color | Monokai Pro inspired dark theme. Cube-snapped. |
| Gruvbox | Color | Retro groove palette. Cube-snapped. |
| Platinum | Color | Mac OS 8/9 Appearance Manager inspired — gray background, purple accents. Cube-snapped. |

## Theme Import / Export

### Importing Themes

Flynn imports themes in **Ghostty format** — plain text files with `key = #rrggbb` entries. To import: Options > Theme > Import Theme, then select a `.txt`, `.theme`, or `.ghostty` file.

**Required fields** (import fails without these):
- All 16 palette entries: `palette = 0=#rrggbb` through `palette = 15=#rrggbb`
- `background = #rrggbb`
- `foreground = #rrggbb`

**Optional fields** (defaults applied if missing):
- `cursor-color` — defaults to foreground color
- `selection-background` — defaults to foreground color
- `selection-foreground` — defaults to background color
- `cursor-text` — parsed but not used

**Auto-detection:**
- `is_dark` is automatically detected from background luminance using `(2R + 5G + B) / 8 < 128`
- Theme name is derived from the filename (stripping `.txt`, `.theme`, `.ghostty` extensions)

**Storage:** Up to 4 custom themes can be stored in preferences simultaneously. Use Remove Theme to free a slot.

### Auto Cube-Snapping

All imported colors are automatically **cube-snapped** to the nearest value in the 6x6x6 RGB cube:

```
0x00 (0)    0x33 (51)    0x66 (102)    0x99 (153)    0xCC (204)    0xFF (255)
```

This means every RGB channel is rounded to one of these 6 values. For example, Dracula's canonical purple `#BD93F9` would be imported as `#CC99FF`.

**Why:** The classic Mac 256-color system palette is based on this cube. Colors outside it dither or quantize unpredictably, producing washed-out or wrong colors on 8-bit displays. Cube-snapping ensures what you see in the theme menu is what you get at any color depth.

**Trade-off:** Imported themes will look slightly different from their canonical appearance on modern truecolor terminals. Subtle colors (like Solarized's cream backgrounds) lose nuance. Bold, saturated themes (like Dracula, Monokai) survive cube-snapping well. Pastel/muted themes degrade more.

### Exporting Themes

Export Theme writes the current active theme (built-in or custom) as a Ghostty-format text file. Exported files can be:
- Shared with other Flynn users
- Re-imported after editing in a text editor
- Used in Ghostty or other terminals that support the format

Note: exported built-in themes with canonical (non-cube-snapped) values will be cube-snapped if re-imported.

### Import Limitations

- **Max 4 custom themes** — limited by preferences storage size
- **No truecolor preservation** — all imports are cube-snapped; the original exact colors are not retained
- **Light theme readability** — imported light themes may have ANSI colors 7 (white) and 15 (bright white) that match the background, making some terminal output invisible. Test with `ls --color` after importing.
- **256-color backgrounds** — very subtle background colors (close to white or close to black) will snap to pure white or pure black. Themes with distinctive backgrounds (tinted, colored) survive better.
- **No undo** — importing overwrites the slot. Re-import the original file to restore.

## Theme Architecture

### Color Representation

Colors use a compact 3-byte `ThemeRGB` struct:

```c
typedef struct {
    unsigned char r, g, b;
} ThemeRGB;
```

At runtime, values are expanded to 16-bit `RGBColor` for QuickDraw via `x * 257` multiplication (maps 0x00-0xFF to 0x0000-0xFFFF).

### Cube-Snapped vs Canonical Themes

Built-in themes use two strategies:

- **Cube-snapped** — all RGB values are multiples of 0x33. Clean rendering at every color depth. Used for original themes (TokyoNight, Amber CRT, Compact Mac, Green Screen, Classic, Monokai, Gruvbox, Platinum) and partially for Solarized (accent colors).
- **Canonical** — exact RGB values from the theme's official specification. Look correct at millions of colors, may degrade at 256 colors. Used for Dracula, Nord, and Solarized base colors. System 7 uses Mac System CLUT values which the Mac 256-color palette includes natively.

Imported themes are always cube-snapped.

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
| `cursor_color` | Text cursor color (note: cursor is currently drawn via XOR inversion, not this field) |

**Selection:**
| Field | Used for |
|-------|----------|
| `sel_bg` | Background color for text selection highlight |
| `sel_fg` | Foreground color for selected text |

**Bold:**
| Field | Used for |
|-------|----------|
| `bold_color` | Overrides default foreground for bold text. Only applies when no explicit ANSI color is set — `\e[1m` uses `bold_color`, but `\e[1;31m` uses standard bold-to-bright promotion (`index += 8`). Set to `{0,0,0}` to leave unused. |

## Creating a Built-in Theme

### Step 1: Create the Header File

Create a new file in `src/themes/`. Name it after your theme using snake_case:

```
src/themes/my_theme.h
```

Use an existing theme as a template. For best 256-color results, use cube-snapped values (multiples of 0x33). Here is a minimal example:

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

### Light Theme Readability

Light themes require special attention to ANSI colors 7 (white) and 15 (bright white). Many terminal applications emit these colors expecting a dark background. On a light theme, white-on-white text is invisible. Consider remapping colors 7 and 15 to darker values that remain readable against the light background, as done in Solarized Light.

### Selection and Cursor Visibility

- `sel_bg` and `sel_fg` should have high contrast with each other. Selection is temporary but must be clearly visible against any combination of ANSI foreground/background colors in the terminal.
- The cursor is currently drawn using XOR inversion (white XOR screen pixels). `cursor_color` is stored but not yet used for rendering.

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

Colors outside this cube may dither on 8-bit displays. Imported themes are automatically cube-snapped to these values. Note that themes only override ANSI colors 0-15 — the 216-color cube (indices 16-231) and 24-step grayscale ramp (indices 232-255) are always the standard xterm palette.

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
| `full` | ON | ON | All (mono + color + up to 4 custom) |
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
  theme_import.c         # Ghostty import/export, cube-snap, file I/O
  color.h               # Color QuickDraw detection (g_has_color_qd)
  color.c               # Runtime SysEnvirons() detection, color_get_rgb() (theme-aware for 0-15)
  themes/
    light.h             # Light (mono, default)
    dark.h              # Dark (mono)
    solarized_light.h   # Solarized Light (color, canonical base + cube-snapped accents)
    solarized_dark.h    # Solarized Dark (color, canonical base + cube-snapped accents)
    tokyo_light.h       # TokyoNight Day (color, cube-snapped)
    tokyo_dark.h        # TokyoNight (color, cube-snapped)
    amber_crt.h         # Amber CRT (color, cube-snapped)
    system7.h           # System 7 (color, canonical Mac CLUT)
    compact_mac.h       # Compact Mac (color, cube-snapped)
    dracula.h           # Dracula (color, canonical)
    nord.h              # Nord (color, canonical)
    green_screen.h      # Green Screen (color, cube-snapped)
    classic.h           # Classic (color, cube-snapped)
    monokai.h           # Monokai (color, cube-snapped)
    gruvbox.h           # Gruvbox (color, cube-snapped)
    platinum.h          # Platinum (color, cube-snapped)
resources/
  telnet.r              # Theme menu (MENU 138)
```
