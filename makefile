## Copyright (C) 2017 Jeremiah Orians
## This file is part of gfk.
##
## gfk is free software: you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## gfk is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with gfk.  If not, see <http://www.gnu.org/licenses/>.

# Prevent rebuilding
VPATH = bin
PACKAGE = gfk

CC=gcc
CFLAGS:=$(CFLAGS) -D_GNU_SOURCE -std=c99 -ggdb -fno-common

gfk-create: gfk_create.c blocks.c buffers.c filesystem.c M2libc/bootstrappable.c | bin
	$(CC) $(CFLAGS) gfk_create.c \
	blocks.c \
	buffers.c \
	filesystem.c \
	M2libc/bootstrappable.c \
	-o bin/gfk-create

# Clean up after ourselves
.PHONY: clean
clean:
	rm -rf bin/

# A cleanup option we probably don't need
.PHONY: clean-hard
clean-hard: clean
	git reset --hard
	git clean -fd

# Directories
bin:
	mkdir -p bin

DESTDIR:=
PREFIX:=/usr/local
bindir:=$(DESTDIR)$(PREFIX)/bin
.PHONY: install
install: gfk-create
	mkdir -p $(bindir)
	cp $^ $(bindir)

###  dist
.PHONY: dist

COMMIT=$(shell git describe --dirty)
TARBALL_VERSION=$(COMMIT:Release_%=%)
TARBALL_DIR:=$(PACKAGE)-$(TARBALL_VERSION)
TARBALL=$(TARBALL_DIR).tar.gz
# Be friendly to Debian; avoid using EPOCH
MTIME=$(shell git show HEAD --format=%ct --no-patch)
# Reproducible tarball
TAR_FLAGS=--sort=name --mtime=@$(MTIME) --owner=0 --group=0 --numeric-owner --mode=go=rX,u+rw,a-s

$(TARBALL):
	(git ls-files					\
	    --exclude=$(TARBALL_DIR);			\
	    echo $^ | tr ' ' '\n')			\
	    | tar $(TAR_FLAGS)				\
	    --transform=s,^,$(TARBALL_DIR)/,S -T- -cf-	\
	    | gzip -c --no-name > $@

dist: $(TARBALL)
