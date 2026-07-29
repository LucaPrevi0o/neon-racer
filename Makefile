CXX ?= g++
CXXFLAGS ?= -Wall -Wextra -std=c++11

# Use the platform's raylib package when available. For a non-standard
# installation, override these from the command line, for example:
# make RAYLIB_CFLAGS="-I/opt/raylib/include" RAYLIB_LIBS="-L/opt/raylib/lib -lraylib"
PKG_CONFIG ?= pkg-config
RAYLIB_CFLAGS ?= $(shell $(PKG_CONFIG) --cflags raylib 2>/dev/null)
RAYLIB_LIBS ?= $(shell $(PKG_CONFIG) --libs raylib 2>/dev/null)
RAYLIB_LIBS := $(if $(strip $(RAYLIB_LIBS)),$(RAYLIB_LIBS),-lraylib)

BUILD_DIR := build
APP := $(BUILD_DIR)/neon-racer
TRACK_TEST := $(BUILD_DIR)/tests/track_tests

APP_SOURCES := src/app/main.cpp \
	src/app/application.cpp \
	src/editor/editor.cpp \
	src/editor/editor_commands.cpp \
	src/editor/editor_history.cpp \
	src/editor/editor_library.cpp \
	src/persistence/draft_io.cpp \
	src/race/race.cpp \
	src/race/race_input.cpp \
	src/render/car_renderer.cpp \
	src/render/race_scene.cpp \
	src/render/track_renderer.cpp \
	src/track/playable_export.cpp \
	src/track/track_graph.cpp \
	src/track/track_rules.cpp \
	src/track/track_surface_geometry.cpp \
	src/track/track_surface_query.cpp \
	src/track/track_validation.cpp \
	src/track/track_layout.cpp \
	src/track/track.cpp \
	src/ui/neon.cpp

TRACK_TEST_SOURCES := tests/unit/test_track.cpp \
	src/track/track.cpp \
	src/track/track_layout.cpp \
	src/track/track_graph.cpp \
	src/track/track_rules.cpp \
	src/track/track_surface_geometry.cpp \
	src/track/track_surface_query.cpp \
	src/track/track_validation.cpp \
	src/persistence/draft_io.cpp \
	src/track/playable_export.cpp

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
