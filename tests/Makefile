# Native (host) unit test for the prefs migration logic.
#
# This is deliberately NOT part of the Retro68 cross-build (CMake).  It
# compiles the Toolbox-free migration unit (src/settings_migrate.c) with
# the host compiler and exercises it against synthesized historical
# prefs images.  See migration_test.c for details.
#
#   make        - build the test
#   make test   - build and run
#   make clean

CC     ?= gcc
CFLAGS ?= -Wall -Wextra -std=c99 -g -I../src

migration_test: migration_test.c ../src/settings_migrate.c
	$(CC) $(CFLAGS) -o $@ migration_test.c ../src/settings_migrate.c

.PHONY: test clean
test: migration_test
	./migration_test

clean:
	rm -f migration_test
