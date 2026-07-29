CXX ?= g++
CXXFLAGS ?= -Wall -Wextra -std=c++11

# Use the platform's raylib package when available. For a non-standard
# installation, override these from the command line, for example:
# make RAYLIB_CFLAGS="-I/opt/raylib/include" RAYLIB_LIBS="-L/opt/raylib/lib -lraylib"
PKG_CONFIG ?= pkg-config
RAYLIB_CFLAGS ?= $(shell $(PKG_CONFIG) --cflags raylib 2>/dev/null)
RAYLIB_LIBS ?= $(shell $(PKG_CONFIG) --libs raylib 2>/dev/null)
RAYLIB_LIBS := $(if $(strip $(RAYLIB_LIBS)),$(RAYLIB_LIBS),-lraylib)

PROJECT_DIR := neon-racer
BUILD_DIR := build
APP := $(BUILD_DIR)/neon-racer
TRACK_TEST := $(BUILD_DIR)/tests/track_tests

APP_SOURCES := $(PROJECT_DIR)/main.cpp \
	$(PROJECT_DIR)/application.cpp \
	$(PROJECT_DIR)/draft_io.cpp \
	$(PROJECT_DIR)/editor.cpp \
	$(PROJECT_DIR)/playable_export.cpp \
	$(PROJECT_DIR)/race.cpp \
	$(PROJECT_DIR)/track.cpp \
	$(PROJECT_DIR)/track_renderer.cpp \
	shared/neon.cpp

TRACK_TEST_SOURCES := $(PROJECT_DIR)/tests/test_track.cpp \
	$(PROJECT_DIR)/track.cpp \
	$(PROJECT_DIR)/draft_io.cpp \
	$(PROJECT_DIR)/playable_export.cpp

.PHONY: all build run test clean help neon-racer racer racer-test

help:
	@echo "Usage:"
	@echo "  make build                Build Neon Racer"
	@echo "  make run                  Build and run Neon Racer"
	@echo "  make test                 Build and run domain tests"
	@echo "  make clean                Remove generated build products"

all: build

build: $(APP)

$(APP): $(APP_SOURCES)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(RAYLIB_CFLAGS) -o $@ $(APP_SOURCES) $(RAYLIB_LIBS)

run: $(APP)
	./$(APP)

$(TRACK_TEST): $(TRACK_TEST_SOURCES)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $(TRACK_TEST_SOURCES)

test racer-test: $(TRACK_TEST)
	./$(TRACK_TEST)

clean:
	$(RM) -r $(BUILD_DIR)

# Temporary compatibility aliases while the source still lives in neon-racer/.
neon-racer racer: build
