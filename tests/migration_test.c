/*
 * migration_test.c - Native unit test for Flynn prefs layout migration.
 *
 * Compiles src/settings_migrate.c with the HOST compiler (see Makefile)
 * and drives prefs_migrate() against byte images synthesized in each
 * historical on-disk layout.  The fixture structs below are defined
 * INDEPENDENTLY of the frozen structs inside settings_migrate.c: they are
 * transcribed from the git history of src/settings.h.  If the migration
 * unit's internal notion of a layout ever disagrees with the real
 * historical layout, the two definitions diverge and the asserts fail --
 * which is exactly the regression we want to catch.
 *
 * Scope: this validates layout/stride/offset logic.  All members are
 * char/short, so the host struct layout (2-byte alignment) is identical
 * to Retro68/68k.  The test does not model 68k big-endian byte order,
 * because the on-target writer and reader are both big-endian, so byte
 * order never affects field recovery -- only self-consistency matters,
 * and the host provides that (fixtures are written and read by the same
 * compiler).
 *
 * NOT wired into the CMake cross-build (per project policy).
 */

#include <stdio.h>
#include <string.h>
#include "settings.h"

static int g_checks;
static int g_failures;

#define CHECK(cond, msg) do {                                          \
	g_checks++;                                                    \
	if (!(cond)) {                                                 \
		g_failures++;                                         \
		printf("    FAIL: %s\n", (msg));                      \
	}                                                             \
} while (0)

#define CHECK_EQ(a, b, msg) do {                                       \
	g_checks++;                                                    \
	if ((long)(a) != (long)(b)) {                                 \
		g_failures++;                                         \
		printf("    FAIL: %s (got %ld, want %ld)\n",          \
		    (msg), (long)(a), (long)(b));                    \
	}                                                             \
} while (0)

#define CHECK_STR(a, b, msg) do {                                      \
	g_checks++;                                                    \
	if (strcmp((a), (b)) != 0) {                                  \
		g_failures++;                                         \
		printf("    FAIL: %s (got \"%s\", want \"%s\")\n",    \
		    (msg), (a), (b));                                \
	}                                                             \
} while (0)

/* ---- Independent fixture layouts (from git history of settings.h) ---- */
#pragma pack(push, 2)

typedef struct {
	char		name[32];
	char		host[128];
	unsigned short	port;
} FixBmA;			/* BM-A: v3/v5/v6 */

typedef struct {
	char		name[32];
	char		host[128];
	unsigned short	port;
	char		username[64];
	short		terminal_type;
	short		font_id;
	short		font_size;
} FixBmB;			/* BM-B: v7..v15 */

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
} FixBmC;			/* BM-C: v17 */

typedef struct {
	short		version;
	char		host[256];
	short		port;
} FixV1;			/* 260 */

typedef struct {
	short		version;
	char		host[256];
	short		port;
	short		bookmark_count;
	FixBmA		bookmarks[8];
	short		font_id;
	short		font_size;
} FixV3;			/* 1562 */

typedef struct {
	short		version;
	char		host[256];
	short		port;
	short		bookmark_count;
	FixBmA		bookmarks[8];
	short		font_id;
	short		font_size;
	short		terminal_type;
	unsigned char	dark_mode;
	char		dns_server[16];
} FixV5;			/* 1582 */

typedef struct {
	short		version;
	char		host[256];
	short		port;
	short		bookmark_count;
	FixBmA		bookmarks[8];
	short		font_id;
	short		font_size;
	short		terminal_type;
	unsigned char	dark_mode;
	char		dns_server[16];
	char		username[64];
} FixV6;			/* 1646 */

typedef struct {
	short		version;
	char		host[256];
	short		port;
	short		bookmark_count;
	FixBmB		bookmarks[8];
	short		font_id;
	short		font_size;
	short		terminal_type;
	unsigned char	dark_mode;
	char		dns_server[16];
	char		username[64];
} FixV7;			/* 2206 */

typedef struct {
	short		version;
	char		host[256];
	short		port;
	short		bookmark_count;
	FixBmB		bookmarks[8];
	short		font_id;
	short		font_size;
	short		terminal_type;
	unsigned char	dark_mode;
	char		dns_server[16];
	char		username[64];
	short		recent[5];
	short		recent_count;
} FixV8;			/* 2218 */

typedef struct {
	short		version;
	char		host[256];
	short		port;
	short		bookmark_count;
	FixBmB		bookmarks[8];
	short		font_id;
	short		font_size;
	short		terminal_type;
	unsigned char	dark_mode;
	unsigned char	backspace_bs;	/* inserted before dns_server at v9 */
	char		dns_server[16];
	char		username[64];
	short		recent[5];
	short		recent_count;
} FixV9;			/* 2218 (same size as v8, different layout) */

typedef struct {
	short		version;
	char		host[256];
	short		port;
	short		bookmark_count;
	FixBmB		bookmarks[8];
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
} FixV10;			/* 2220 (early v10; late v10 == same size) */

typedef struct {
	short		version;
	char		host[256];
	short		port;
	short		bookmark_count;
	FixBmB		bookmarks[8];
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
} FixV12;			/* 2428 */

typedef struct {
	short		version;
	char		host[256];
	short		port;
	short		bookmark_count;
	FixBmB		bookmarks[8];
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
} FixV13;			/* 2436 */

typedef struct {
	short		version;
	char		host[256];
	short		port;
	short		bookmark_count;
	FixBmB		bookmarks[20];
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
} FixV14;			/* 5256 */

typedef struct {
	short		version;
	char		host[256];
	short		port;
	short		bookmark_count;
	FixBmB		bookmarks[20];
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
} FixV15;			/* 5260 */

typedef struct {
	short		version;
	char		host[256];
	short		port;
	short		bookmark_count;
	FixBmC		bookmarks[20];
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
} FixV17;			/* 5342 */

#pragma pack(pop)

/* ---------------------------------------------------------------- */

static void
test_sizes(void)
{
	printf("  layout sizes (host == 68k for char/short structs):\n");
	CHECK_EQ(sizeof(FixV1), 260, "sizeof v1");
	CHECK_EQ(sizeof(FixV3), 1562, "sizeof v3");
	CHECK_EQ(sizeof(FixV5), 1582, "sizeof v5");
	CHECK_EQ(sizeof(FixV6), 1646, "sizeof v6");
	CHECK_EQ(sizeof(FixV7), 2206, "sizeof v7");
	CHECK_EQ(sizeof(FixV8), 2218, "sizeof v8");
	CHECK_EQ(sizeof(FixV9), 2218, "sizeof v9");
	CHECK_EQ(sizeof(FixV10), 2220, "sizeof v10");
	CHECK_EQ(sizeof(FixV12), 2428, "sizeof v12");
	CHECK_EQ(sizeof(FixV13), 2436, "sizeof v13");
	CHECK_EQ(sizeof(FixV14), 5256, "sizeof v14");
	CHECK_EQ(sizeof(FixV15), 5260, "sizeof v15");
	CHECK_EQ(sizeof(FixV17), 5342, "sizeof v17");
	/* The scratch buffer prefs_load uses must hold any historical image. */
	CHECK(sizeof(FlynnPrefs) >= sizeof(FixV17), "current >= v17 image");
}

static void
test_v1(void)
{
	FixV1 f;
	FlynnPrefs p;
	int migrated;

	printf("  v1:\n");
	memset(&f, 0, sizeof(f));
	f.version = 1;
	strcpy(f.host, "host.v1.example");
	f.port = 2323;

	prefs_defaults(&p);
	migrated = prefs_migrate(&f, sizeof(f), &p);

	CHECK_EQ(migrated, 1, "v1 migrated flag");
	CHECK_EQ(p.version, PREFS_VERSION, "v1 version bumped");
	CHECK_STR(p.host, "host.v1.example", "v1 host");
	CHECK_EQ(p.port, 2323, "v1 port");
	/* Fields that did not exist take factory defaults. */
	CHECK_EQ(p.font_id, 4, "v1 font default");
	CHECK_EQ(p.show_status_bar, 1, "v1 status default");
}

static void
test_v3(void)
{
	FixV3 f;
	FlynnPrefs p;

	printf("  v3:\n");
	memset(&f, 0, sizeof(f));
	f.version = 3;
	strcpy(f.host, "v3host");
	f.port = 23;
	f.bookmark_count = 2;
	strcpy(f.bookmarks[0].name, "bm0");
	strcpy(f.bookmarks[0].host, "bm0.host");
	f.bookmarks[0].port = 111;
	strcpy(f.bookmarks[1].name, "bm1");
	f.bookmarks[1].port = 222;
	f.font_id = 7;
	f.font_size = 12;

	prefs_defaults(&p);
	prefs_migrate(&f, sizeof(f), &p);

	CHECK_STR(p.host, "v3host", "v3 host");
	CHECK_EQ(p.bookmark_count, 2, "v3 bookmark_count");
	CHECK_STR(p.bookmarks[0].name, "bm0", "v3 bm0 name");
	CHECK_STR(p.bookmarks[0].host, "bm0.host", "v3 bm0 host");
	CHECK_EQ(p.bookmarks[0].port, 111, "v3 bm0 port");
	CHECK_EQ(p.bookmarks[1].port, 222, "v3 bm1 port");
	CHECK_EQ(p.font_id, 7, "v3 font_id");
	CHECK_EQ(p.font_size, 12, "v3 font_size");
	/* BM-A had no per-bookmark extras -> defaulted. */
	CHECK_EQ(p.bookmarks[0].terminal_type, -1, "v3 bm0 tt default");
	CHECK_EQ(p.bookmarks[0].bm_theme_id, -1, "v3 bm0 theme default");
	/* dns absent -> validated to default by prefs_load, but migrate
	 * leaves it empty; check terminal_type/dark defaults. */
	CHECK_EQ(p.terminal_type, 0, "v3 tt default");
	CHECK_EQ(p.dark_mode, 0, "v3 dark default");
}

static void
test_v5(void)
{
	FixV5 f;
	FlynnPrefs p;

	printf("  v5:\n");
	memset(&f, 0, sizeof(f));
	f.version = 5;
	strcpy(f.host, "v5host");
	f.port = 992;
	f.bookmark_count = 1;
	strcpy(f.bookmarks[0].name, "b");
	f.font_id = 3;
	f.font_size = 10;
	f.terminal_type = 2;	/* VT100 */
	f.dark_mode = 1;
	strcpy(f.dns_server, "8.8.8.8");

	prefs_defaults(&p);
	prefs_migrate(&f, sizeof(f), &p);

	CHECK_STR(p.host, "v5host", "v5 host");
	CHECK_EQ(p.terminal_type, 2, "v5 tt");
	CHECK_EQ(p.dark_mode, 1, "v5 dark");
	CHECK_STR(p.dns_server, "8.8.8.8", "v5 dns recovered");
	CHECK_EQ(p.theme_id, 1, "v5 theme derived from dark");
}

static void
test_v6(void)
{
	FixV6 f;
	FlynnPrefs p;

	printf("  v6:\n");
	memset(&f, 0, sizeof(f));
	f.version = 6;
	strcpy(f.host, "v6host");
	f.terminal_type = 0;
	strcpy(f.dns_server, "9.9.9.9");
	strcpy(f.username, "alice");

	prefs_defaults(&p);
	prefs_migrate(&f, sizeof(f), &p);

	CHECK_STR(p.host, "v6host", "v6 host");
	CHECK_STR(p.dns_server, "9.9.9.9", "v6 dns");
	CHECK_STR(p.username, "alice", "v6 username recovered");
}

static void
test_v7(void)
{
	FixV7 f;
	FlynnPrefs p;

	printf("  v7:\n");
	memset(&f, 0, sizeof(f));
	f.version = 7;
	strcpy(f.host, "v7host");
	f.bookmark_count = 1;
	strcpy(f.bookmarks[0].name, "b0");
	strcpy(f.bookmarks[0].username, "bmuser");
	f.bookmarks[0].terminal_type = 3;
	f.bookmarks[0].font_id = 5;
	f.bookmarks[0].font_size = 14;
	f.terminal_type = 1;
	strcpy(f.dns_server, "1.2.3.4");
	strcpy(f.username, "bob");

	prefs_defaults(&p);
	prefs_migrate(&f, sizeof(f), &p);

	CHECK_STR(p.host, "v7host", "v7 host");
	CHECK_STR(p.bookmarks[0].username, "bmuser", "v7 bm0 username");
	CHECK_EQ(p.bookmarks[0].terminal_type, 3, "v7 bm0 tt");
	CHECK_EQ(p.bookmarks[0].font_id, 5, "v7 bm0 font_id");
	CHECK_EQ(p.bookmarks[0].font_size, 14, "v7 bm0 font_size");
	CHECK_EQ(p.bookmarks[0].bm_theme_id, -1, "v7 bm0 bm_theme default");
	CHECK_STR(p.dns_server, "1.2.3.4", "v7 dns");
	CHECK_STR(p.username, "bob", "v7 username");
}

static void
test_v8(void)
{
	FixV8 f;
	FlynnPrefs p;

	printf("  v8:\n");
	memset(&f, 0, sizeof(f));
	f.version = 8;
	strcpy(f.host, "v8host");
	f.terminal_type = 4;	/* ansi -> backspace/local_echo default on */
	strcpy(f.dns_server, "5.6.7.8");
	strcpy(f.username, "carol");
	f.recent[0] = 3;
	f.recent[1] = 1;
	f.recent_count = 2;

	prefs_defaults(&p);
	prefs_migrate(&f, sizeof(f), &p);

	CHECK_STR(p.host, "v8host", "v8 host");
	/* dns IS recoverable at v8 (no backspace_bs shift yet). */
	CHECK_STR(p.dns_server, "5.6.7.8", "v8 dns recovered (not reset)");
	CHECK_STR(p.username, "carol", "v8 username");
	CHECK_EQ(p.recent_count, 2, "v8 recent_count");
	CHECK_EQ(p.recent[0], 3, "v8 recent[0]");
	CHECK_EQ(p.recent[1], 1, "v8 recent[1]");
	/* terminal_type 4 (ansi) -> backspace/local_echo default to 1. */
	CHECK_EQ(p.backspace_bs, 1, "v8 backspace default from ansi");
	CHECK_EQ(p.local_echo, 1, "v8 local_echo default from ansi");
}

static void
test_v9(void)
{
	FixV9 f;
	FlynnPrefs p;

	printf("  v9:\n");
	memset(&f, 0, sizeof(f));
	f.version = 9;
	strcpy(f.host, "v9host");
	f.terminal_type = 2;
	f.backspace_bs = 1;
	strcpy(f.dns_server, "4.3.2.1");
	strcpy(f.username, "dave");
	f.recent_count = 1;
	f.recent[0] = 0;

	prefs_defaults(&p);
	prefs_migrate(&f, sizeof(f), &p);

	CHECK_STR(p.host, "v9host", "v9 host");
	CHECK_EQ(p.backspace_bs, 1, "v9 backspace recovered");
	/* dns is at the shifted offset now; frozen V13 struct reads it
	 * correctly. */
	CHECK_STR(p.dns_server, "4.3.2.1", "v9 dns recovered");
	CHECK_STR(p.username, "dave", "v9 username");
	CHECK_EQ(p.recent_count, 1, "v9 recent_count");
	/* local_echo absent (added v10) -> default; tt=2 not ansi -> 0. */
	CHECK_EQ(p.local_echo, 0, "v9 local_echo default");
	/* show_status_bar not trustworthy pre-v12 -> default on. */
	CHECK_EQ(p.show_status_bar, 1, "v9 status default on");
}

static void
test_v10(void)
{
	FixV10 f;
	FlynnPrefs p;

	printf("  v10:\n");
	memset(&f, 0, sizeof(f));
	f.version = 10;
	strcpy(f.host, "v10host");
	f.terminal_type = 0;
	f.backspace_bs = 0;
	strcpy(f.dns_server, "10.0.0.1");
	f.local_echo = 1;

	prefs_defaults(&p);
	prefs_migrate(&f, sizeof(f), &p);

	CHECK_STR(p.host, "v10host", "v10 host");
	CHECK_STR(p.dns_server, "10.0.0.1", "v10 dns");
	CHECK_EQ(p.local_echo, 1, "v10 local_echo recovered");
	/* show_status_bar indistinguishable from padding at v10 -> default. */
	CHECK_EQ(p.show_status_bar, 1, "v10 status default on");
}

static void
test_v12(void)
{
	FixV12 f;
	FlynnPrefs p;

	printf("  v12:\n");
	memset(&f, 0, sizeof(f));
	f.version = 12;
	strcpy(f.host, "v12host");
	strcpy(f.dns_server, "1.1.1.1");
	f.local_echo = 1;
	f.show_status_bar = 0;
	f.bookmark_protocol[0] = 1;	/* finger */
	f.bookmark_protocol[7] = 1;
	strcpy(f.finger_host, "finger.example");
	strcpy(f.finger_user, "eve");

	prefs_defaults(&p);
	prefs_migrate(&f, sizeof(f), &p);

	CHECK_STR(p.host, "v12host", "v12 host");
	CHECK_EQ(p.show_status_bar, 0, "v12 status recovered (off)");
	CHECK_EQ(p.bookmark_protocol[0], 1, "v12 protocol[0]");
	CHECK_EQ(p.bookmark_protocol[7], 1, "v12 protocol[7]");
	CHECK_STR(p.finger_host, "finger.example", "v12 finger_host");
	CHECK_STR(p.finger_user, "eve", "v12 finger_user");
	/* bookmark_verbose absent (added v13) -> default 0. */
	CHECK_EQ(p.bookmark_verbose[0], 0, "v12 verbose default");
}

static void
test_v13(void)
{
	FixV13 f;
	FlynnPrefs p;

	printf("  v13:\n");
	memset(&f, 0, sizeof(f));
	f.version = 13;
	strcpy(f.host, "v13host");
	f.bookmark_count = 8;
	strcpy(f.bookmarks[7].name, "last8");
	f.bookmarks[7].port = 700;
	strcpy(f.dns_server, "2.2.2.2");
	f.show_status_bar = 1;
	f.bookmark_verbose[0] = 1;
	f.bookmark_verbose[7] = 1;

	prefs_defaults(&p);
	prefs_migrate(&f, sizeof(f), &p);

	CHECK_STR(p.host, "v13host", "v13 host");
	CHECK_EQ(p.bookmark_count, 8, "v13 bookmark_count");
	CHECK_STR(p.bookmarks[7].name, "last8", "v13 bm7 name (last old slot)");
	CHECK_EQ(p.bookmarks[7].port, 700, "v13 bm7 port");
	CHECK_EQ(p.bookmark_verbose[0], 1, "v13 verbose[0]");
	CHECK_EQ(p.bookmark_verbose[7], 1, "v13 verbose[7]");
	/* Slots beyond the old 8 must be default (-1), not garbage. */
	CHECK_EQ(p.bookmarks[19].bm_theme_id, -1, "v13 bm19 theme default");
	CHECK_EQ(p.bookmarks[8].bm_theme_id, -1, "v13 bm8 theme default");
}

static void
test_v14(void)
{
	FixV14 f;
	FlynnPrefs p;

	printf("  v14:\n");
	memset(&f, 0, sizeof(f));
	f.version = 14;
	strcpy(f.host, "v14host");
	f.bookmark_count = 20;
	strcpy(f.bookmarks[19].name, "bm19");
	f.bookmarks[19].port = 1919;
	strcpy(f.dns_server, "3.3.3.3");
	f.show_status_bar = 1;
	f.bookmark_verbose[19] = 1;

	prefs_defaults(&p);
	prefs_migrate(&f, sizeof(f), &p);

	CHECK_STR(p.host, "v14host", "v14 host");
	CHECK_EQ(p.bookmark_count, 20, "v14 bookmark_count 20");
	CHECK_STR(p.bookmarks[19].name, "bm19", "v14 bm19 name");
	CHECK_EQ(p.bookmarks[19].port, 1919, "v14 bm19 port");
	CHECK_EQ(p.bookmark_verbose[19], 1, "v14 verbose[19]");
	/* win_x/win_y absent (added v15) -> factory default. */
	CHECK_EQ(p.win_x, 2, "v14 win_x default");
	CHECK_EQ(p.win_y, 40, "v14 win_y default");
}

static void
test_v15(void)
{
	FixV15 f;
	FlynnPrefs p;

	printf("  v15:\n");
	memset(&f, 0, sizeof(f));
	f.version = 15;
	strcpy(f.host, "v15host");
	f.dark_mode = 1;
	strcpy(f.dns_server, "6.6.6.6");
	f.win_x = 123;
	f.win_y = 456;

	prefs_defaults(&p);
	prefs_migrate(&f, sizeof(f), &p);

	CHECK_STR(p.host, "v15host", "v15 host");
	CHECK_EQ(p.win_x, 123, "v15 win_x recovered");
	CHECK_EQ(p.win_y, 456, "v15 win_y recovered");
	/* theme_id absent (added v17) -> derived from dark_mode. */
	CHECK_EQ(p.theme_id, 1, "v15 theme derived from dark");
}

static void
test_v17(void)
{
	FixV17 f;
	FlynnPrefs p;

	printf("  v17:\n");
	memset(&f, 0, sizeof(f));
	f.version = 17;
	strcpy(f.host, "v17host");
	f.bookmark_count = 3;
	strcpy(f.bookmarks[0].name, "b0");
	f.bookmarks[0].bm_theme_id = 5;
	f.bookmarks[0].bm_backspace_bs = 1;
	f.bookmarks[0].bm_local_echo = 0;
	strcpy(f.bookmarks[2].name, "b2");
	f.bookmarks[2].bm_theme_id = 2;
	f.win_x = 10;
	f.win_y = 20;
	f.theme_id = 6;		/* Amber CRT */

	prefs_defaults(&p);
	prefs_migrate(&f, sizeof(f), &p);

	CHECK_STR(p.host, "v17host", "v17 host");
	CHECK_EQ(p.bookmark_count, 3, "v17 bookmark_count");
	CHECK_STR(p.bookmarks[0].name, "b0", "v17 bm0 name");
	CHECK_EQ(p.bookmarks[0].bm_theme_id, 5, "v17 bm0 bm_theme_id");
	CHECK_EQ(p.bookmarks[0].bm_backspace_bs, 1, "v17 bm0 bm_backspace");
	CHECK_EQ(p.bookmarks[0].bm_local_echo, 0, "v17 bm0 bm_local_echo");
	CHECK_STR(p.bookmarks[2].name, "b2", "v17 bm2 name");
	CHECK_EQ(p.bookmarks[2].bm_theme_id, 2, "v17 bm2 bm_theme_id");
	CHECK_EQ(p.win_x, 10, "v17 win_x");
	CHECK_EQ(p.win_y, 20, "v17 win_y");
	CHECK_EQ(p.theme_id, 6, "v17 theme_id recovered (not derived)");
}

static void
test_current_and_unknown(void)
{
	FlynnPrefs src, p;
	int migrated;
	short bogus[4];

	printf("  current (v18) direct copy:\n");
	prefs_defaults(&src);
	strcpy(src.host, "currenthost");
	src.theme_id = 3;
	src.win_x = 99;
	src.bookmark_count = 1;
	strcpy(src.bookmarks[0].name, "keep");

	prefs_defaults(&p);
	migrated = prefs_migrate(&src, sizeof(src), &p);
	CHECK_EQ(migrated, 0, "current version: no migration");
	CHECK_STR(p.host, "currenthost", "current host preserved");
	CHECK_EQ(p.theme_id, 3, "current theme preserved");
	CHECK_EQ(p.win_x, 99, "current win_x preserved");
	CHECK_STR(p.bookmarks[0].name, "keep", "current bookmark preserved");

	printf("  unknown/corrupt version:\n");
	memset(bogus, 0, sizeof(bogus));
	bogus[0] = 9999;	/* version far in the future */
	prefs_defaults(&p);
	strcpy(p.host, "sentinel");
	migrated = prefs_migrate(bogus, sizeof(bogus), &p);
	CHECK_EQ(migrated, 0, "unknown version: no migration/save");
	/* out is left as the caller's defaults (host unchanged here). */
	CHECK_STR(p.host, "sentinel", "unknown version: out untouched");

	printf("  truncated image (short read):\n");
	prefs_defaults(&p);
	migrated = prefs_migrate(bogus, 1, &p);	/* < sizeof(short) */
	CHECK_EQ(migrated, 0, "truncated: no migration");
}

int
main(void)
{
	printf("Flynn prefs migration test\n");
	printf("  sizeof(FlynnPrefs) = %lu (host, FLYNN_THEMES off)\n\n",
	    (unsigned long)sizeof(FlynnPrefs));

	test_sizes();
	test_v1();
	test_v3();
	test_v5();
	test_v6();
	test_v7();
	test_v8();
	test_v9();
	test_v10();
	test_v12();
	test_v13();
	test_v14();
	test_v15();
	test_v17();
	test_current_and_unknown();

	printf("\n%d checks, %d failures\n", g_checks, g_failures);
	if (g_failures) {
		printf("RESULT: FAIL\n");
		return 1;
	}
	printf("RESULT: PASS\n");
	return 0;
}
