# config.mk - Build configuration for orm-glib
#
# Copyright 2025 Zach Pobiel
# SPDX-License-Identifier: AGPL-3.0-or-later

# Version information
VERSION_MAJOR := 0
VERSION_MINOR := 2
VERSION_MICRO := 0
VERSION := $(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_MICRO)
API_VERSION := $(VERSION_MAJOR).$(VERSION_MINOR)

# Library naming
LIB_NAME := orm-glib
LIB_SONAME := lib$(LIB_NAME)-$(API_VERSION).so
LIB_SHARED := $(LIB_SONAME).$(VERSION_MICRO)
LIB_STATIC := lib$(LIB_NAME)-$(API_VERSION).a

# Installation directories (can be overridden)
PREFIX ?= /usr/local
LIBDIR ?= $(PREFIX)/lib
INCLUDEDIR ?= $(PREFIX)/include/$(LIB_NAME)-$(API_VERSION)
PKGCONFIGDIR ?= $(LIBDIR)/pkgconfig
GIRDIR ?= $(PREFIX)/share/gir-1.0
TYPELIBDIR ?= $(LIBDIR)/girepository-1.0

# Build options
BUILD_SHARED ?= 1
BUILD_STATIC ?= 1
BUILD_GIR ?= 1
BUILD_TESTS ?= 1
BUILD_EXAMPLES ?= 1

# Feature flags - database drivers
ENABLE_SQLITE ?= 1
ENABLE_POSTGRES ?= 0
ENABLE_MYSQL ?= 0

# Debug/Release settings
DEBUG ?= 0
ASAN ?= 0
UBSAN ?= 0

# Compiler settings
CC := gcc
AR := ar
CSTD := -std=gnu89

# Base compiler flags
CFLAGS_BASE := $(CSTD) -Wall -Wextra -Werror
CFLAGS_BASE += -Wno-unused-parameter
CFLAGS_BASE += -Wno-discarded-qualifiers
CFLAGS_BASE += -fPIC
CFLAGS_BASE += -DORM_COMPILATION

# Debug vs Release
ifeq ($(DEBUG),1)
    CFLAGS_BASE += -g -O0 -DORM_DEBUG
else
    CFLAGS_BASE += -O2 -DNDEBUG
endif

# Address sanitizer
ifeq ($(ASAN),1)
    CFLAGS_BASE += -fsanitize=address -fno-omit-frame-pointer
    LDFLAGS_BASE += -fsanitize=address
endif

# Undefined behavior sanitizer
ifeq ($(UBSAN),1)
    CFLAGS_BASE += -fsanitize=undefined
    LDFLAGS_BASE += -fsanitize=undefined
endif

# pkg-config packages - base dependencies
PKG_DEPS := glib-2.0 gobject-2.0 gio-2.0

# SQLite support
ifeq ($(ENABLE_SQLITE),1)
    ifeq ($(shell pkg-config --exists sqlite3 && echo yes),yes)
        PKG_DEPS += sqlite3
        CFLAGS_BASE += -DORM_ENABLE_SQLITE
    else
        $(warning SQLite enabled but sqlite3 package not found - disabling)
        ENABLE_SQLITE := 0
    endif
endif

# PostgreSQL support
ifeq ($(ENABLE_POSTGRES),1)
    ifeq ($(shell pkg-config --exists libpq && echo yes),yes)
        PKG_DEPS += libpq
        CFLAGS_BASE += -DORM_ENABLE_POSTGRES
    else
        $(error PostgreSQL enabled but libpq package not found. Install libpq-devel or postgresql-devel)
    endif
endif

# MySQL/MariaDB support
ifeq ($(ENABLE_MYSQL),1)
    ifeq ($(shell pkg-config --exists mysqlclient && echo yes),yes)
        PKG_DEPS += mysqlclient
        CFLAGS_BASE += -DORM_ENABLE_MYSQL
    else ifeq ($(shell pkg-config --exists libmariadb && echo yes),yes)
        PKG_DEPS += libmariadb
        CFLAGS_BASE += -DORM_ENABLE_MYSQL
    else
        $(error MySQL/MariaDB enabled but neither mysqlclient nor libmariadb package found. Install mariadb-connector-c-devel (Fedora) or libmysqlclient-dev (Debian/Ubuntu))
    endif
endif

# pkg-config generated flags
PKG_CFLAGS := $(shell pkg-config --cflags $(PKG_DEPS))
PKG_LIBS := $(shell pkg-config --libs $(PKG_DEPS))

# Final flags
CFLAGS := $(CFLAGS_BASE) $(PKG_CFLAGS) $(EXTRA_CFLAGS)
LDFLAGS := $(LDFLAGS_BASE) $(PKG_LIBS) $(EXTRA_LDFLAGS)

# GObject Introspection tools
GIR_SCANNER := g-ir-scanner
GIR_COMPILER := g-ir-compiler

# Build directories
# Release and debug artifacts live side by side rather than overwriting
# each other, which is what lets a consumer link one without rebuilding
# the other -- cmacs's --enable-cmacs-deps-debug switches between them by
# path.  Every other in-house GLib library here uses the same layout.
ifeq ($(DEBUG),1)
    BUILD_TYPE := debug
else
    BUILD_TYPE := release
endif

BUILD_DIR := build/$(BUILD_TYPE)
OBJ_DIR := $(BUILD_DIR)/objs
TEST_DIR := $(BUILD_DIR)/tests
EXAMPLE_DIR := $(BUILD_DIR)/examples
