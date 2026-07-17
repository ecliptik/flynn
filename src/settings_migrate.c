/*
 * settings_migrate.c - Preferences layout migration for Flynn
 *
 * This unit is deliberately free of any Macintosh Toolbox dependency so
 * that it can be compiled and exercised natively on a host (see
 * tests/migration_test.c).  settings.c #includes this file directly, so
 * it becomes part of the cross-compiled build without a separate CMake
 * entry.
 *
 * BACKGROUND
 * ----------
 * The old prefs_load() read the on-disk bytes straight into the *current*
 * FlynnPrefs struct and then patched fields in place.  That is only valid
 * when every layout change is a pure trailing append.  Flynn's history is
 * not that clean:
 *
 *   - the Bookmark struct grew twice (BM-A -> BM-B at v7, BM-B -> BM-C at
 *     v17), each time shifting *every* field after bookmarks[];
 *   - MAX_BOOKMARKS grew 8 -> 20 at v14, shifting everything after
 *     bookmarks[] by ~2800 bytes;
 *   - backspace_bs was *inserted* (not appended) before dns_server at v9.
 *
 * So any file older than the current layout was being misread and then
 * saved back as permanent garbage.  This unit instead reconstructs the
 * true historical on-disk layout for each version with a set of FROZEN
 * structs (never embedding a live type), overlays the raw bytes onto the
 * matching frozen struct, and copies field-by-field into a
 * prefs_defaults()-initialised current struct.
 *
 * FROZEN LAYOUT TABLE (reconstructed from git history of settings.h;
 * sizes verified by compiling each historical header on the host -- all
 * members are char/short so host and 68k layouts are identical, 2-byte
 * alignment):
 *
 *   ver    file bytes  Bookmark  MAX_BM  notes
 *   ------ ----------  --------  ------  -------------------------------
 *   1        260       (none)     -      host[256]+port only
 *   3       1562       BM-A(162)  8      +bookmarks,font_id,font_size
 *   5       1582       BM-A(162)  8      +terminal_type,dark_mode,dns
 *   6       1646       BM-A(162)  8      +username
 *   7       2206       BM-B(232)  8      Bookmark grew (user/tt/fid/fsz)
 *   8       2218       BM-B(232)  8      +recent[5],recent_count
 *   9       2218       BM-B(232)  8      backspace_bs INSERTED before dns
 *   10      2220       BM-B(232)  8      +local_echo (+show_status_bar*)
 *   12      2428       BM-B(232)  8      +show_status_bar,protocol,finger
 *   13      2436       BM-B(232)  8      +bookmark_verbose
 *   14      5256       BM-B(232) 20      MAX_BOOKMARKS 8 -> 20
 *   15      5260       BM-B(232) 20      +win_x,win_y
 *   17      5342       BM-C(236) 20      Bookmark grew (bm_*), +theme_id
 *   18      5342+      BM-C(236) 20      +custom_themes (THEMES builds)
 *
 *   Frozen structs used (each is the LARGEST layout of its shape; shorter
 *   files of the same shape overlay onto the leading bytes and the absent
 *   trailing fields are zero-filled, then defaulted by version):
 *     V1Prefs   -> v1
 *     V6Prefs   -> v3, v5, v6      (BM-A)
 *     V8Prefs   -> v7, v8          (BM-B, no backspace_bs)
 *     V13Prefs  -> v9,v10,v12,v13  (BM-B, backspace_bs, 8 bookmarks)
 *     V15Prefs  -> v14, v15        (BM-B, 20 bookmarks)
 *     V17Prefs  -> v17             (BM-C, 20 bookmarks)
 *     current   -> v18             (direct copy)
 *
 *   (*) v8/v9 and v10-early/v10-late are byte-identical in size but differ
 *       in layout/meaning; version disambiguates v8 vs v9, and the
 *       show_status_bar byte of a v10 file is indistinguishable from
 *       padding, so it is defaulted rather than trusted (see below).
 *
 * Endianness note: the native test synthesises fixtures with the same
 * host compiler that reads them back, so it validates the *layout/stride*
 * logic self-consistently; it does not model 68k big-endian byte order
 * (irrelevant on-target, where writer and reader are both big-endian).
 */

#include <string.h>
#include "settings.h"

/* Frozen historical layouts -- 2-byte packed to match 68k / Retro68.
 * These contain only char/short so packing is already natural, but the
 * pragma documents intent and guards against future edits. */
#pragma pack(push, 2)

/* Bookmark BM-A: versions 3,5,6 */
typedef struct {
	char		name[32];
	char		host[128];
	unsigned short	port;
} V6Bookmark;

/* Bookmark BM-B: versions 7..15 */
typedef struct {
	char		name[32];
	char		host[128];
	unsigned short	port;
	char		username[64];
	short		terminal_type;
	short		font_id;
	short		font_size;
} V13Bookmark;

/* Bookmark BM-C: version 17 (== current Bookmark shape, frozen here so a
 * future Bookmark change cannot silently break v17 migration) */
typedef struct {
	char		name[32];
	char		host[128];
	unsigned short	port;
	char		username[64];
	short		terminal_type;
	short		font_id;
	short		font_size;
	signed char	bm_theme_id;
	signed char	bm_backspace_bs;
	signed char	bm_local_echo;
} V17Bookmark;

typedef struct {
	short		version;
	char		host[256];
	short		port;
} V1Prefs;

typedef struct {
	short		version;
	char		host[256];
	short		port;
	short		bookmark_count;
	V6Bookmark	bookmarks[8];
	short		font_id;
	short		font_size;
	short		terminal_type;
	unsigned char	dark_mode;
	char		dns_server[16];
	char		username[64];
} V6Prefs;

typedef struct {
	short		version;
	char		host[256];
	short		port;
	short		bookmark_count;
	V13Bookmark	bookmarks[8];
	short		font_id;
	short		font_size;
	short		terminal_type;
	unsigned char	dark_mode;
	char		dns_server[16];
	char		username[64];
	short		recent[5];
	short		recent_count;
} V8Prefs;

typedef struct {
	short		version;
	char		host[256];
	short		port;
	short		bookmark_count;
	V13Bookmark	bookmarks[8];
	short		font_id;
	short		font_size;
	short		terminal_type;
	unsigned char	dark_mode;
	unsigned char	backspace_bs;
	char		dns_server[16];
	char		username[64];
	short		recent[5];
	short		recent_count;
	unsigned char	local_echo;
	unsigned char	show_status_bar;
	short		bookmark_protocol[8];
	char		finger_host[128];
	char		finger_user[64];
	unsigned char	bookmark_verbose[8];
} V13Prefs;

typedef struct {
	short		version;
	char		host[256];
	short		port;
	short		bookmark_count;
	V13Bookmark	bookmarks[20];
	short		font_id;
	short		font_size;
	short		terminal_type;
	unsigned char	dark_mode;
	unsigned char	backspace_bs;
	char		dns_server[16];
	char		username[64];
	short		recent[5];
	short		recent_count;
	unsigned char	local_echo;
	unsigned char	show_status_bar;
	short		bookmark_protocol[20];
	char		finger_host[128];
	char		finger_user[64];
	unsigned char	bookmark_verbose[20];
	short		win_x, win_y;
} V15Prefs;

typedef struct {
	short		version;
	char		host[256];
	short		port;
	short		bookmark_count;
	V17Bookmark	bookmarks[20];
	short		font_id;
	short		font_size;
	short		terminal_type;
	unsigned char	dark_mode;
	unsigned char	backspace_bs;
	char		dns_server[16];
	char		username[64];
	short		recent[5];
	short		recent_count;
	unsigned char	local_echo;
	unsigned char	show_status_bar;
	short		bookmark_protocol[20];
	char		finger_host[128];
	char		finger_user[64];
	unsigned char	bookmark_verbose[20];
	short		win_x, win_y;
	unsigned char	theme_id;
} V17Prefs;

#pragma pack(pop)

void
prefs_defaults(FlynnPrefs *prefs)
{
	memset(prefs, 0, sizeof(FlynnPrefs));
	prefs->version = PREFS_VERSION;
	prefs->host[0] = '\0';
	prefs->port = 23;
	prefs->font_id = 4;	/* Monaco */
	prefs->font_size = 9;
	prefs->terminal_type = 0;	/* xterm */
	prefs->dark_mode = 0;		/* light */
	prefs->backspace_bs = 0;	/* DEL (0x7F) for xterm */
	prefs->local_echo = 0;		/* off by default */
	prefs->show_status_bar = 1;	/* on by default */
	strncpy(prefs->dns_server, "1.1.1.1", sizeof(prefs->dns_server) - 1);
	prefs->dns_server[sizeof(prefs->dns_server) - 1] = '\0';
	prefs->win_x = 2;
	prefs->win_y = 40;
	prefs->theme_id = 0;		/* Light theme */
	/* New bookmark fields default to -1 (use global) */
	{
		short i;
		for (i = 0; i < MAX_BOOKMARKS; i++) {
			prefs->bookmarks[i].bm_theme_id = -1;
			prefs->bookmarks[i].bm_backspace_bs = -1;
			prefs->bookmarks[i].bm_local_echo = -1;
		}
	}
}

/* Copy a BM-A (v3/v5/v6) bookmark into the current Bookmark, defaulting
 * the fields that did not exist yet. */
static void
copy_bookmark_a(Bookmark *dst, const V6Bookmark *src)
{
	memcpy(dst->name, src->name, sizeof(src->name));
	memcpy(dst->host, src->host, sizeof(src->host));
	dst->port = src->port;
	dst->username[0] = '\0';
	dst->terminal_type = -1;
	dst->font_id = 0;
	dst->font_size = 0;
	dst->bm_theme_id = -1;
	dst->bm_backspace_bs = -1;
	dst->bm_local_echo = -1;
}

/* Copy a BM-B (v7..v15) bookmark into the current Bookmark. */
static void
copy_bookmark_b(Bookmark *dst, const V13Bookmark *src)
{
	memcpy(dst->name, src->name, sizeof(src->name));
	memcpy(dst->host, src->host, sizeof(src->host));
	dst->port = src->port;
	memcpy(dst->username, src->username, sizeof(src->username));
	dst->terminal_type = src->terminal_type;
	dst->font_id = src->font_id;
	dst->font_size = src->font_size;
	dst->bm_theme_id = -1;
	dst->bm_backspace_bs = -1;
	dst->bm_local_echo = -1;
}

/* Copy a BM-C (v17) bookmark into the current Bookmark. */
static void
copy_bookmark_c(Bookmark *dst, const V17Bookmark *src)
{
	memcpy(dst->name, src->name, sizeof(src->name));
	memcpy(dst->host, src->host, sizeof(src->host));
	dst->port = src->port;
	memcpy(dst->username, src->username, sizeof(src->username));
	dst->terminal_type = src->terminal_type;
	dst->font_id = src->font_id;
	dst->font_size = src->font_size;
	dst->bm_theme_id = src->bm_theme_id;
	dst->bm_backspace_bs = src->bm_backspace_bs;
	dst->bm_local_echo = src->bm_local_echo;
}

/*
 * prefs_migrate - convert a raw on-disk prefs image into the current
 * FlynnPrefs layout.
 *
 *   raw  : pointer to the bytes read from the prefs file
 *   len  : number of valid bytes in raw (may be < or > any frozen struct)
 *   out  : destination; MUST already be prefs_defaults()-initialised
 *
 * Returns 1 if the layout was migrated (caller should re-save the file),
 * 0 if no migration was needed or the image was unusable (out is left at
 * defaults / the direct copy).
 */
int
prefs_migrate(const void *raw, long len, FlynnPrefs *out)
{
	short version = 0;
	short i, bc;

	if (len >= (long)sizeof(short))
		memcpy(&version, raw, sizeof(short));

	/* Current version: straight copy of whatever fits. */
	if (version == PREFS_VERSION) {
		long n = (len < (long)sizeof(FlynnPrefs))
		    ? len : (long)sizeof(FlynnPrefs);
		if (n > 0)
			memcpy(out, raw, (size_t)n);
		out->version = PREFS_VERSION;
		return 0;
	}

	switch (version) {
	case 1: {
		V1Prefs o;
		memset(&o, 0, sizeof(o));
		memcpy(&o, raw,
		    (len < (long)sizeof(o)) ? (size_t)len : sizeof(o));
		memcpy(out->host, o.host, sizeof(o.host));
		out->port = o.port;
		break;
	}

	case 3:
	case 5:
	case 6: {
		V6Prefs o;
		memset(&o, 0, sizeof(o));
		memcpy(&o, raw,
		    (len < (long)sizeof(o)) ? (size_t)len : sizeof(o));

		memcpy(out->host, o.host, sizeof(o.host));
		out->port = o.port;
		bc = o.bookmark_count;
		if (bc < 0) bc = 0;
		if (bc > 8) bc = 8;
		out->bookmark_count = bc;
		for (i = 0; i < 8; i++)
			copy_bookmark_a(&out->bookmarks[i], &o.bookmarks[i]);
		out->font_id = o.font_id;
		out->font_size = o.font_size;
		if (version >= 5) {
			out->terminal_type = o.terminal_type;
			out->dark_mode = o.dark_mode;
			memcpy(out->dns_server, o.dns_server,
			    sizeof(o.dns_server));
		}
		if (version >= 6)
			memcpy(out->username, o.username,
			    sizeof(o.username));
		/* Fields that did not exist yet: derive as the historical
		 * ladder did. */
		out->backspace_bs = (out->terminal_type == 4) ? 1 : 0;
		out->local_echo = (out->terminal_type == 4) ? 1 : 0;
		out->theme_id = out->dark_mode ? 1 : 0;
		break;
	}

	case 7:
	case 8: {
		V8Prefs o;
		memset(&o, 0, sizeof(o));
		memcpy(&o, raw,
		    (len < (long)sizeof(o)) ? (size_t)len : sizeof(o));

		memcpy(out->host, o.host, sizeof(o.host));
		out->port = o.port;
		bc = o.bookmark_count;
		if (bc < 0) bc = 0;
		if (bc > 8) bc = 8;
		out->bookmark_count = bc;
		for (i = 0; i < 8; i++)
			copy_bookmark_b(&out->bookmarks[i], &o.bookmarks[i]);
		out->font_id = o.font_id;
		out->font_size = o.font_size;
		out->terminal_type = o.terminal_type;
		out->dark_mode = o.dark_mode;
		/* dns_server sits at the correct v7/v8 offset here (no
		 * backspace_bs yet), so it IS recoverable -- unlike the old
		 * in-place path which shifted it by one byte. */
		memcpy(out->dns_server, o.dns_server, sizeof(o.dns_server));
		memcpy(out->username, o.username, sizeof(o.username));
		if (version >= 8) {
			memcpy(out->recent, o.recent, sizeof(o.recent));
			out->recent_count = o.recent_count;
		}
		out->backspace_bs = (out->terminal_type == 4) ? 1 : 0;
		out->local_echo = (out->terminal_type == 4) ? 1 : 0;
		out->theme_id = out->dark_mode ? 1 : 0;
		break;
	}

	case 9:
	case 10:
	case 12:
	case 13: {
		V13Prefs o;
		memset(&o, 0, sizeof(o));
		memcpy(&o, raw,
		    (len < (long)sizeof(o)) ? (size_t)len : sizeof(o));

		memcpy(out->host, o.host, sizeof(o.host));
		out->port = o.port;
		bc = o.bookmark_count;
		if (bc < 0) bc = 0;
		if (bc > 8) bc = 8;
		out->bookmark_count = bc;
		for (i = 0; i < 8; i++)
			copy_bookmark_b(&out->bookmarks[i], &o.bookmarks[i]);
		out->font_id = o.font_id;
		out->font_size = o.font_size;
		out->terminal_type = o.terminal_type;
		out->dark_mode = o.dark_mode;
		out->backspace_bs = o.backspace_bs;
		memcpy(out->dns_server, o.dns_server, sizeof(o.dns_server));
		memcpy(out->username, o.username, sizeof(o.username));
		memcpy(out->recent, o.recent, sizeof(o.recent));
		out->recent_count = o.recent_count;
		if (version >= 10)
			out->local_echo = o.local_echo;
		else
			out->local_echo = (out->terminal_type == 4) ? 1 : 0;
		/* show_status_bar first appeared at v12.  In a v10 file the
		 * byte at that offset is indistinguishable from struct
		 * padding (v10-early vs v10-late are the same size), so we
		 * only trust it from v12 on; otherwise keep the default. */
		if (version >= 12) {
			out->show_status_bar = o.show_status_bar;
			for (i = 0; i < 8; i++)
				out->bookmark_protocol[i] =
				    o.bookmark_protocol[i];
			memcpy(out->finger_host, o.finger_host,
			    sizeof(o.finger_host));
			memcpy(out->finger_user, o.finger_user,
			    sizeof(o.finger_user));
		}
		if (version >= 13)
			for (i = 0; i < 8; i++)
				out->bookmark_verbose[i] =
				    o.bookmark_verbose[i];
		out->theme_id = out->dark_mode ? 1 : 0;
		break;
	}

	case 14:
	case 15: {
		V15Prefs o;
		memset(&o, 0, sizeof(o));
		memcpy(&o, raw,
		    (len < (long)sizeof(o)) ? (size_t)len : sizeof(o));

		memcpy(out->host, o.host, sizeof(o.host));
		out->port = o.port;
		bc = o.bookmark_count;
		if (bc < 0) bc = 0;
		if (bc > 20) bc = 20;
		out->bookmark_count = bc;
		for (i = 0; i < 20; i++)
			copy_bookmark_b(&out->bookmarks[i], &o.bookmarks[i]);
		out->font_id = o.font_id;
		out->font_size = o.font_size;
		out->terminal_type = o.terminal_type;
		out->dark_mode = o.dark_mode;
		out->backspace_bs = o.backspace_bs;
		memcpy(out->dns_server, o.dns_server, sizeof(o.dns_server));
		memcpy(out->username, o.username, sizeof(o.username));
		memcpy(out->recent, o.recent, sizeof(o.recent));
		out->recent_count = o.recent_count;
		out->local_echo = o.local_echo;
		out->show_status_bar = o.show_status_bar;
		for (i = 0; i < 20; i++) {
			out->bookmark_protocol[i] = o.bookmark_protocol[i];
			out->bookmark_verbose[i] = o.bookmark_verbose[i];
		}
		memcpy(out->finger_host, o.finger_host,
		    sizeof(o.finger_host));
		memcpy(out->finger_user, o.finger_user,
		    sizeof(o.finger_user));
		/* win_x/win_y first appeared at v15. */
		if (version >= 15) {
			out->win_x = o.win_x;
			out->win_y = o.win_y;
		}
		/* theme_id did not exist before v17; derive from dark_mode. */
		out->theme_id = out->dark_mode ? 1 : 0;
		break;
	}

	case 17: {
		V17Prefs o;
		memset(&o, 0, sizeof(o));
		memcpy(&o, raw,
		    (len < (long)sizeof(o)) ? (size_t)len : sizeof(o));

		memcpy(out->host, o.host, sizeof(o.host));
		out->port = o.port;
		bc = o.bookmark_count;
		if (bc < 0) bc = 0;
		if (bc > 20) bc = 20;
		out->bookmark_count = bc;
		for (i = 0; i < 20; i++)
			copy_bookmark_c(&out->bookmarks[i], &o.bookmarks[i]);
		out->font_id = o.font_id;
		out->font_size = o.font_size;
		out->terminal_type = o.terminal_type;
		out->dark_mode = o.dark_mode;
		out->backspace_bs = o.backspace_bs;
		memcpy(out->dns_server, o.dns_server, sizeof(o.dns_server));
		memcpy(out->username, o.username, sizeof(o.username));
		memcpy(out->recent, o.recent, sizeof(o.recent));
		out->recent_count = o.recent_count;
		out->local_echo = o.local_echo;
		out->show_status_bar = o.show_status_bar;
		for (i = 0; i < 20; i++) {
			out->bookmark_protocol[i] = o.bookmark_protocol[i];
			out->bookmark_verbose[i] = o.bookmark_verbose[i];
		}
		memcpy(out->finger_host, o.finger_host,
		    sizeof(o.finger_host));
		memcpy(out->finger_user, o.finger_user,
		    sizeof(o.finger_user));
		out->win_x = o.win_x;
		out->win_y = o.win_y;
		out->theme_id = o.theme_id;
		/* v18 only appends custom_themes[]/custom_theme_count, which
		 * prefs_defaults() already zero-initialised. */
		break;
	}

	default:
		/* Unknown, corrupt, or newer-than-known version: leave the
		 * caller-supplied defaults in place and do not re-save. */
		return 0;
	}

	out->version = PREFS_VERSION;
	return 1;
}
