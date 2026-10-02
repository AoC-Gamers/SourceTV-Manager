SHELL := /usr/bin/env bash

ROOT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
PYTHON ?= python3
DEPS_DIR ?= $(ROOT_DIR)/.deps/exts-linux
BUILD_DIR ?= $(ROOT_DIR)/.build/linux-l4d2
SOURCEMOD_DIR ?= $(DEPS_DIR)/sourcemod-1.12
MMSOURCE_DIR ?= $(DEPS_DIR)/mmsource-1.12
HL2SDK_DIR ?= $(DEPS_DIR)/hl2sdk-l4d2
VENV_DIR ?= $(DEPS_DIR)/.venv

.PHONY: help deps-linux build-linux package-linux build-linux-container clean-linux clean-all

help:
	@printf '%s\n' \
		'Available targets:' \
		'  make help          Show this help message' \
		'  make deps-linux    Fetch Linux L4D2 build dependencies into .deps/exts-linux/' \
		'  make build-linux   Build the Linux x86 L4D2 extension package' \
		'  make package-linux Verify the generated package contents' \
		'  make build-linux-container Build and verify the package with Docker Compose' \
		'  make clean-linux   Remove Linux build outputs' \
		'  make clean-all     Remove Linux build outputs and dependencies'

deps-linux:
	DEPS_DIR="$(DEPS_DIR)" VENV_DIR="$(VENV_DIR)" bash ./scripts/fetch-linux-deps.sh

build-linux:
	DEPS_DIR="$(DEPS_DIR)" BUILD_DIR="$(BUILD_DIR)" \
	SOURCEMOD_DIR="$(SOURCEMOD_DIR)" MMSOURCE_DIR="$(MMSOURCE_DIR)" \
	HL2SDK_DIR="$(HL2SDK_DIR)" VENV_DIR="$(VENV_DIR)" \
	bash ./scripts/build-linux-l4d2.sh

package-linux:
	@test -f "$(BUILD_DIR)/package/addons/sourcemod/extensions/sourcetvmanager.ext.2.l4d2.so"
	@test -f "$(BUILD_DIR)/package/addons/sourcemod/scripting/include/sourcetvmanager.inc"
	@test -f "$(BUILD_DIR)/package/addons/sourcemod/gamedata/sourcetvmanager.games.txt"
	@printf 'Package ready: %s/package\n' "$(BUILD_DIR)"

build-linux-container:
	@mkdir -p "$(ROOT_DIR)/.build"
	LOCAL_UID="$$(id -u)" LOCAL_GID="$$(id -g)" \
	docker compose -f compose.build.yml run --build --rm builder

clean-linux:
	rm -rf "$(BUILD_DIR)"

clean-all:
	rm -rf "$(ROOT_DIR)/.build" "$(DEPS_DIR)"
