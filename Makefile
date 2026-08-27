/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */

CC ?= gcc

PREFIX      ?= /usr/local
BINDIR      ?= $(PREFIX)/bin
LIBDIR      ?= $(PREFIX)/lib
INCDIR      ?= $(PREFIX)/include/plexy

# Optional prefix containing prebuilt upstream dependencies such as libweston
# (e.g. from the plexyshell-runtime-deps tarball).
DEPS_PREFIX ?=

BASE_CFLAGS = -std=c11 -O2 -I. -Iinclude -Iinclude/stb -Isrc -I$(INCDIR)
CFLAGS += $(BASE_CFLAGS)

ifdef DEPS_PREFIX
  PKG_CONFIG_ENV = PKG_CONFIG_PATH=$(DEPS_PREFIX)/lib/pkgconfig:$(DEPS_PREFIX)/lib64/pkgconfig:$(DEPS_PREFIX)/lib/x86_64-linux-gnu/pkgconfig:$(PKG_CONFIG_PATH)
  DEPS_RPATH = -Wl,-rpath,$(DEPS_PREFIX)/lib -Wl,-rpath,$(DEPS_PREFIX)/lib64 -Wl,-rpath,$(DEPS_PREFIX)/lib/x86_64-linux-gnu
else
  PKG_CONFIG_ENV =
  DEPS_RPATH =
endif

PKG_CONFIG_CMD = $(PKG_CONFIG_ENV) pkg-config

# Detect the installed libweston API version (system or from DEPS_PREFIX).
WESTON_PKG := $(shell for api in 20 19 18 17 16 15 14 13 12 11 10; do \
                  if $(PKG_CONFIG_CMD) --exists libweston-$$api 2>/dev/null; then \
                      echo libweston-$$api; break; \
                  fi; \
                done)

PKG_MODULES = wayland-server libdrm gbm egl glesv2 xkbcommon \
              xcb xcb-composite xcb-xfixes xcb-randr xcb-xtest

ifneq ($(WESTON_PKG),)
  PKG_MODULES += $(WESTON_PKG)
endif

PKG_CFLAGS = $(shell $(PKG_CONFIG_CMD) --cflags $(PKG_MODULES))
PKG_LIBS   = $(shell $(PKG_CONFIG_CMD) --libs $(PKG_MODULES))

SRC = $(wildcard src/wayland/*.c)

all: wayland_bridge

wayland_bridge: $(SRC) $(INCDIR)/plexy.h $(INCDIR)/plexy_protocol.h
	$(CC) $(SRC) $(CFLAGS) $(PKG_CFLAGS) \
	    -L$(LIBDIR) -lplexy -lrt -lpthread -lX11 -lXcursor \
	    $(PKG_LIBS) \
	    -Wl,-rpath,'$$ORIGIN/../lib' $(DEPS_RPATH) \
	    -o $@

install: all
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 wayland_bridge $(DESTDIR)$(BINDIR)/wayland_bridge

clean:
	rm -f wayland_bridge

.PHONY: all install clean
