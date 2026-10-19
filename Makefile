# ps5-native-app-boilerplate - Linux/WSL build entry points.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later

SHELL := /bin/bash
.DEFAULT_GOAL := app

-include .env

APP_DEFINITIONS ?=
APP_INCLUDE_PATHS ?=
APP_STATIC_ARCHIVES ?=
APP_IMPORT_STUBS ?=
APP_RUNTIME_MODULES ?=
# Empty selects the pinned ps5-opengl release (tools/fetch-opengl-sdk.sh).
PS5_OPENGL_PREFIX ?=

# The app always builds against ps5-opengl; user APP_* values append.
OPENGL_SDK := .deps/ps5-opengl/current
override APP_DEFINITIONS := $(strip GL_GLEXT_PROTOTYPES=1 $(APP_DEFINITIONS))
override APP_INCLUDE_PATHS := $(strip src $(OPENGL_SDK)/include $(APP_INCLUDE_PATHS))
override APP_STATIC_ARCHIVES := $(strip .deps/ps5-opengl/libps5opengl-group.a $(APP_STATIC_ARCHIVES))
override APP_IMPORT_STUBS := $(strip $(OPENGL_SDK)/lib/libSceAgc.so \
	$(OPENGL_SDK)/lib/libSceAgcDriver.so $(APP_IMPORT_STUBS))
PACBREW_PACKAGES ?=
PACBREW_INCLUDE_PATHS ?=
PACBREW_STATIC_ARCHIVES ?=
# Explicit relative archives avoid pkg-config's whitespace-sensitive prefix
# handling when the project lives in the user's "CD Player" folder.
PACBREW_INCLUDE_PATHS += include
PACBREW_STATIC_ARCHIVES += lib/libcurl.a lib/libpsl.a lib/libssl.a lib/libcrypto.a lib/libzstd.a lib/libz.a
TITLE_ID ?=
APP_NAME ?=
APP_CATEGORY ?= game
CONTENT_SUFFIX ?=
HOST_CXX ?= clang++
HOST_TEST_CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror \
	-ffunction-sections -fdata-sections
HOST_TEST_LDFLAGS ?= -Wl,--gc-sections
GTEST_ARGS ?=
BUILD_JOBS ?= $(shell nproc 2>/dev/null || echo 2)
USE_CCACHE ?= 1
export BUILD_JOBS USE_CCACHE
export HOST_CXX HOST_TEST_CXXFLAGS HOST_TEST_LDFLAGS
export APP_DEFINITIONS APP_INCLUDE_PATHS APP_STATIC_ARCHIVES APP_IMPORT_STUBS APP_RUNTIME_MODULES
export PS5_OPENGL_PREFIX
export PACBREW_PACKAGES PACBREW_INCLUDE_PATHS PACBREW_STATIC_ARCHIVES
export TITLE_ID APP_NAME APP_CATEGORY CONTENT_SUFFIX

RUNTIME := runtime/libc.prx
RUNTIME_INPUTS := tools/rebuild-libc.sh tools/build-host-tools.sh tools/ninja-build.sh $(wildcard tooling/native/*.cpp tooling/native/*.hpp) $(wildcard tooling/native/runtime/*.txt)
.PHONY: app opengl libc test test-archive release
app: $(RUNTIME) opengl
	@bash tools/build.sh Folder
opengl:
	@bash tools/prepare-opengl.sh
$(RUNTIME): $(RUNTIME_INPUTS)
	@bash tools/rebuild-libc.sh
test:
	@bash tools/test-core.sh
	@bash tools/test-artwork.sh
	@bash tools/test-motion.sh
	@bash tools/test-visual-rotation.sh
	@bash tools/test-player.sh
	@bash tools/test-album-info.sh
	@bash tools/test-numeric-locale.sh
test-archive:
	@bash tools/preview.sh
	@bash tools/test-archive.sh
release:
	@bash tools/build-cdplayer.sh

libc:
	@bash tools/rebuild-libc.sh
