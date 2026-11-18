# SPDX-License-Identifier: GPL-3.0-only
SHELL := bash
.DEFAULT_GOAL := help
PYTHON ?= python3
ROOT_DIR := $(shell $(PYTHON) -c 'import sys; from pathlib import Path; print(Path(sys.argv[1]).absolute().parent.as_posix())' '$(MAKEFILE_LIST)')
DEPS_DIR ?= $(ROOT_DIR)/.deps/exts-$(PLATFORM)
SOURCEMOD_DIR ?= $(DEPS_DIR)/sourcemod-1.12
MMSOURCE_DIR ?= $(DEPS_DIR)/mmsource-1.12
HL2SDK_DIR ?= $(DEPS_DIR)/hl2sdk-l4d2
AMBUILD_DIR ?= $(DEPS_DIR)/ambuild
VENV_DIR ?= $(DEPS_DIR)/.venv
DIST_DIR ?= $(ROOT_DIR)/dist
RELEASE_CHANNEL ?= develop
SCRIPT_DIR := $(ROOT_DIR)/scripts

deps-exts-linux build-exts-linux package-exts-linux release-linux validate-artifact-linux clean-linux sonar sonar-cppcheck test-ci: PLATFORM := linux
deps-exts-windows build-exts-windows package-exts-windows release-windows validate-artifact-windows clean-windows: PLATFORM := windows

.PHONY: help deps-exts-linux deps-exts-windows build-exts-linux build-exts-windows \
        package-exts-linux package-exts-windows release-linux release-windows \
        validate-artifact-linux validate-artifact-windows clean-linux clean-windows \
        sonar sonar-cppcheck test test-ci

help:
	@printf '%s\n' \
	  'make deps-exts-<linux|windows>        Preparar dependencias fijadas' \
	  'make build-exts-<linux|windows>       Compilar x86 en el host correspondiente' \
	  'make package-exts-<linux|windows>     Preparar staging validado' \
	  'make release-<linux|windows>          Generar ZIP local (RELEASE_CHANNEL=develop|latest)' \
	  'make validate-artifact-<linux|windows> Verificar staging y ZIP contra el build' \
	  'make test                            Probar el contrato de distribución' \
	  'make test-public-api                 Comparar el contrato público congelado' \
	  'make test-pawn-boundary              Probar traducciones y límites de Pawn en C++' \
	  'make test-pawn-boundary-vm           Probar límites de memoria en una VM local' \
	  'make test-native-sanitized           Ejecutar pruebas nativas con ASan y UBSan' \
	  'make test-native                     Ejecutar pruebas C++ puras (x86 por defecto)' \
	  'make test-abi                        Ejecutar el probe de ABI Linux x86' \
	  'make test-coverage                   Medir cobertura local C++ y Python' \
	  'make test-coverage-tooling           Validar localmente el recibo de cobertura' \
	  'make test-coexistence                Probar la cadena de hooks con SourceHook x86' \
	  'make sonar-cppcheck / sonar          Analizar el build Linux localmente' \
	  'make clean-<linux|windows>            Limpiar la salida de AMBuild' \
	  'make build-linux-container           Compilar y preparar staging en Compose'

deps-exts-linux deps-exts-windows:
	$(PYTHON) "$(SCRIPT_DIR)/deps/fetch-native.py" --deps "$(DEPS_DIR)" --venv "$(VENV_DIR)"

NATIVE_INPUTS = --sourcemod "$(SOURCEMOD_DIR)" --metamod "$(MMSOURCE_DIR)" \
                --hl2sdk "$(HL2SDK_DIR)" --ambuild "$(AMBUILD_DIR)"

build-exts-linux build-exts-windows:
	$(PYTHON) "$(SCRIPT_DIR)/build/build-native.py" "$(PLATFORM)" \
	  --build "$(BUILD_DIR)" --venv "$(VENV_DIR)" $(NATIVE_INPUTS)

ARTIFACT_TOOL = $(PYTHON) "$(SCRIPT_DIR)/package/artifact_contract.py"
ARTIFACT_ARGS = "$(PLATFORM)" --build "$(BUILD_DIR)" --dist "$(DIST_DIR)" --channel "$(RELEASE_CHANNEL)" $(NATIVE_INPUTS)

package-exts-linux package-exts-windows:
	$(ARTIFACT_TOOL) stage $(ARTIFACT_ARGS)

release-linux release-windows:
	$(ARTIFACT_TOOL) release $(ARTIFACT_ARGS)

validate-artifact-linux validate-artifact-windows:
	$(ARTIFACT_TOOL) validate $(ARTIFACT_ARGS)

test-ci: test test-public-api

.PHONY: test-public-api
test-public-api:
	$(PYTHON) -B "$(ROOT_DIR)/tests/contract.py"

test:
	$(PYTHON) -B -m unittest discover -s "$(ROOT_DIR)/tests/package"

NATIVE_CXXFLAGS ?= -m32 -std=c++17 -Wall -Wextra
NATIVE_TESTS := recording_path:natives:test-native recording_sessions:core:recording_sessions hook_context:core:hook_context print_recipient:natives:print_recipient
NATIVE_SANITIZER_FLAGS := -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=all -fno-pie -no-pie

.PHONY: test-native-sanitized
test-native-sanitized:
	$(MAKE) -f "$(ROOT_DIR)/Makefile" test-native NATIVE_CXXFLAGS="$(NATIVE_CXXFLAGS) $(NATIVE_SANITIZER_FLAGS)"

.PHONY: test-native
test-native:
	@mkdir -p "$(ROOT_DIR)/.build"
	@set -euo pipefail; \
	  output="$$(mktemp -d "$(ROOT_DIR)/.build/native.XXXXXX")"; \
	  printf 'Output: %s\n' "$$output"; \
	  { $(CXX) --version; \
	    [ -e "$(ROOT_DIR)/.git" ] && git -C "$(ROOT_DIR)" rev-parse --verify HEAD 2>/dev/null || \
	      printf 'Source SHA unavailable\n'; \
	    printf 'Flags: %s\n' "$(NATIVE_CXXFLAGS)"; \
	  } | tee "$$output/toolchain.txt"; \
	  for test in $(NATIVE_TESTS); do \
	    IFS=: read -r source include binary <<< "$$test"; \
	    $(CXX) $(NATIVE_CXXFLAGS) -I"$(ROOT_DIR)/extension/$$include" \
	      "$(ROOT_DIR)/tests/native/$$source.cpp" -o "$$output/$$binary"; \
	    "$$output/$$binary" | tee -a "$$output/result.txt"; \
	  done


.PHONY: test-pawn-boundary
test-pawn-boundary: PLATFORM := linux
test-pawn-boundary:
	@mkdir -p "$(ROOT_DIR)/.build"
	@set -euo pipefail; \
	  output="$$(mktemp -d "$(ROOT_DIR)/.build/pawn-boundary.XXXXXX")"; \
	  printf 'Output: %s\n' "$$output"; \
	  $(CXX) $(NATIVE_CXXFLAGS) -isystem "$(SOURCEMOD_DIR)/sourcepawn/include" \
	    -I"$(ROOT_DIR)/extension/natives" "$(ROOT_DIR)/tests/native/pawn_boundary.cpp" -o "$$output/pawn-boundary"; \
	  "$$output/pawn-boundary" | tee "$$output/result.txt"


PAWN_VM_CXXFLAGS ?= -std=c++17 -Wall -Wextra
PAWN_VM ?=

.PHONY: test-pawn-boundary-vm
test-pawn-boundary-vm: PLATFORM := linux
test-pawn-boundary-vm:
	@test -f "$(PAWN_VM)" || { printf '%s\n' 'Indica PAWN_VM con la biblioteca local de SourcePawn.' >&2; exit 1; }
	@mkdir -p "$(ROOT_DIR)/.build"
	@set -euo pipefail; \
	  output="$$(mktemp -d "$(ROOT_DIR)/.build/pawn-boundary-vm.XXXXXX")"; \
	  printf 'Output: %s\n' "$$output"; \
	  "$(SPCOMP)" "$(ROOT_DIR)/tests/native/pawn_boundary_fixture.sp" -o"$$output/fixture.smx"; \
	  $(CXX) $(PAWN_VM_CXXFLAGS) -isystem "$(SOURCEMOD_DIR)/sourcepawn/include" \
	    -I"$(ROOT_DIR)/extension/natives" "$(ROOT_DIR)/tests/native/pawn_boundary_vm.cpp" -ldl -o "$$output/pawn-boundary-vm"; \
	  "$$output/pawn-boundary-vm" "$$(realpath -- "$(PAWN_VM)")" "$$output/fixture.smx" | tee "$$output/result.txt"


sonar:
	$(PYTHON) "$(SCRIPT_DIR)/tools/make_sonar.py" analyze --compile-commands "$(BUILD_DIR)/compile_commands.json"

sonar-cppcheck:
	$(PYTHON) "$(SCRIPT_DIR)/tools/make_sonar.py" cppcheck --compile-commands "$(BUILD_DIR)/compile_commands.json"

clean-linux clean-windows:
	$(PYTHON) -c 'import pathlib, shutil, sys; path = pathlib.Path(sys.argv[1]).absolute(); shutil.rmtree(path) if path.is_dir() and (path / ".ambuild2").is_dir() else None' "$(BUILD_DIR)"

# Preserve the existing local Linux entry points and build folder.
deps-linux build-linux package-linux: PLATFORM := linux
build-exts-linux package-exts-linux release-linux validate-artifact-linux clean-linux sonar sonar-cppcheck build-linux package-linux: DEFAULT_BUILD_DIR := $(ROOT_DIR)/.build/linux-l4d2
BUILD_DIR ?= $(if $(DEFAULT_BUILD_DIR),$(DEFAULT_BUILD_DIR),$(ROOT_DIR)/.build/$(PLATFORM)-x86)

.PHONY: deps-linux build-linux package-linux build-linux-container clean-all
deps-linux: deps-exts-linux
build-linux: build-exts-linux
package-linux: package-exts-linux

build-linux-container:
	$(PYTHON) "$(SCRIPT_DIR)/deps/fetch-native.py" --submodules-only
	@mkdir -p "$(ROOT_DIR)/.build" "$(ROOT_DIR)/dist"
	LOCAL_UID="$$(id -u)" LOCAL_GID="$$(id -g)" \
	  docker compose -f "$(ROOT_DIR)/compose.build.yml" run --build --rm builder

clean-all: clean-linux clean-windows
	$(PYTHON) -c 'import pathlib, shutil, sys; path = pathlib.Path(sys.argv[1]); shutil.rmtree(path) if path.is_dir() else None' "$(ROOT_DIR)/.deps"

SPCOMP ?= spcomp
SPCOMP_INCLUDE_DIR ?=
DIAGNOSTIC_BUILD_DIR ?= $(ROOT_DIR)/.build/diagnostics

.PHONY: build-diagnostics
build-diagnostics:
	@mkdir -p "$(DIAGNOSTIC_BUILD_DIR)"
	"$(SPCOMP)" "$(ROOT_DIR)/tests/diagnostics/sourcetv_test.sp" \
	  -i"$(ROOT_DIR)/addons/sourcemod/scripting/include" \
	  $(if $(SPCOMP_INCLUDE_DIR),-i"$(SPCOMP_INCLUDE_DIR)") \
	  -o"$(DIAGNOSTIC_BUILD_DIR)/sourcetv_test.smx"

COVERAGE ?= coverage
PACKAGE_COVERAGE_DIR ?= $(ROOT_DIR)/.build/package-coverage

.PHONY: test-package-coverage
test-package-coverage:
	@mkdir -p "$(PACKAGE_COVERAGE_DIR)"
	COVERAGE_FILE="$(PACKAGE_COVERAGE_DIR)/.coverage" "$(COVERAGE)" run \
	  --branch --source="$(SCRIPT_DIR)/package" "$(ROOT_DIR)/tests/package/test_artifact_contract.py"
	COVERAGE_FILE="$(PACKAGE_COVERAGE_DIR)/.coverage" "$(COVERAGE)" xml \
	  -o "$(PACKAGE_COVERAGE_DIR)/coverage.xml"
	COVERAGE_FILE="$(PACKAGE_COVERAGE_DIR)/.coverage" "$(COVERAGE)" report

.PHONY: test-abi
test-abi: PLATFORM := linux
test-abi:
	@mkdir -p "$(ROOT_DIR)/.build"
	@set -euo pipefail; \
	  output="$$(mktemp -d "$(ROOT_DIR)/.build/abi.XXXXXX")"; \
	  bintools="$(SOURCEMOD_DIR)/extensions/bintools"; \
	  $(CXX) -m32 -std=c++17 -DPOSIX -DLINUX -D_LINUX -DHAVE_STDINT_H \
	    -I"$(ROOT_DIR)/extension" -isystem "$$bintools" \
	    -isystem "$(MMSOURCE_DIR)/core/sourcehook" \
	    -isystem "$(SOURCEMOD_DIR)/public" -isystem "$(SOURCEMOD_DIR)/public/extensions" \
	    -isystem "$(SOURCEMOD_DIR)/public/amtl" -isystem "$(SOURCEMOD_DIR)/public/amtl/amtl" \
	    -isystem "$(SOURCEMOD_DIR)/public/jit" -isystem "$(SOURCEMOD_DIR)/public/jit/x86" \
	    -isystem "$(SOURCEMOD_DIR)/sourcepawn/include" \
	    "$(ROOT_DIR)/tests/native/local_hltv_binding.cpp" \
	    "$$bintools/CallMaker.cpp" "$$bintools/CallWrapper.cpp" "$$bintools/jit_call.cpp" \
	    -o "$$output/local-hltv-binding"; \
	  "$$output/local-hltv-binding" | tee "$$output/result.txt"

.PHONY: test-coexistence
test-coexistence: PLATFORM := linux
test-coexistence:
	@mkdir -p "$(ROOT_DIR)/.build"
	@set -euo pipefail; \
	  output="$$(mktemp -d "$(ROOT_DIR)/.build/coexistence.XXXXXX")"; \
	  sh_dir="$(MMSOURCE_DIR)/core/sourcehook"; \
	  $(CXX) -m32 -std=c++17 -fno-devirtualize -I"$(ROOT_DIR)/extension" -isystem "$$sh_dir" \
	    "$(ROOT_DIR)/tests/native/connect_coexistence.cpp" \
	    "$$sh_dir/sourcehook.cpp" "$$sh_dir/sourcehook_impl_chookmaninfo.cpp" \
	    "$$sh_dir/sourcehook_impl_chookidman.cpp" "$$sh_dir/sourcehook_impl_cproto.cpp" \
	    "$$sh_dir/sourcehook_impl_cvfnptr.cpp" -o "$$output/connect-coexistence"; \
	  "$$output/connect-coexistence" | tee "$$output/result.txt"

COVERAGE_PYTHON ?= $(PYTHON)
GCOV ?= gcov

.PHONY: test-coverage
test-coverage: DEFAULT_BUILD_DIR := $(ROOT_DIR)/.build/linux-l4d2
test-coverage: PLATFORM := linux
.PHONY: test-coverage-tooling
test-coverage-tooling:
	$(PYTHON) -B -m unittest discover -s "$(ROOT_DIR)/tests/tools"

test-coverage: build-exts-linux
	$(PYTHON) "$(SCRIPT_DIR)/tools/local_coverage.py" --compile-commands "$(BUILD_DIR)/compile_commands.json" --coverage-python "$(COVERAGE_PYTHON)" \
	  --cxx="$(CXX)" --native-flags="$(NATIVE_CXXFLAGS)" --native-tests="$(NATIVE_TESTS)" --gcov="$(GCOV)"
