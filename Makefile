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
RACE_PHYSICS_TEST := $(BUILD_DIR)/tests/race_physics_tests
TIME_TRIAL_TEST := $(BUILD_DIR)/tests/time_trial_tests

RACE_CORE_SOURCES := src/race/race.cpp \
	src/race/race_physics.cpp

APP_SOURCES := src/app/main.cpp \
	src/app/application.cpp \
	src/app/raylib_race_input.cpp \
	src/editor/editor.cpp \
	src/editor/editor_camera.cpp \
	src/editor/editor_commands.cpp \
	src/editor/editor_history.cpp \
	src/editor/editor_library.cpp \
	src/editor/editor_picking.cpp \
	src/persistence/draft_io.cpp \
	$(RACE_CORE_SOURCES) \
	src/render/car_renderer.cpp \
	src/render/race_scene.cpp \
	src/render/track_renderer.cpp \
	src/track/playable_export.cpp \
	src/track/track_graph.cpp \
	src/track/track_rules.cpp \
	src/track/track_road_geometry.cpp \
	src/track/track_surface_geometry.cpp \
	src/track/track_surface_query.cpp \
	src/track/track_validation.cpp \
	src/track/track_layout.cpp \
	src/track/track_piece_geometry.cpp \
	src/track/track_overlap.cpp \
	src/ui/neon.cpp

TRACK_TEST_SOURCES := tests/unit/test_track.cpp \
	src/track/track_layout.cpp \
	src/track/track_piece_geometry.cpp \
	src/track/track_overlap.cpp \
	src/track/track_graph.cpp \
	src/track/track_rules.cpp \
	src/track/track_road_geometry.cpp \
	src/track/track_surface_geometry.cpp \
	src/track/track_surface_query.cpp \
	src/track/track_validation.cpp \
	src/persistence/draft_io.cpp \
	src/track/playable_export.cpp

RACE_PHYSICS_TEST_SOURCES := tests/unit/test_race_physics.cpp \
	src/race/race_physics.cpp

TIME_TRIAL_TEST_SOURCES := tests/unit/test_time_trial.cpp \
	$(RACE_CORE_SOURCES) \
	src/track/track_layout.cpp \
	src/track/track_piece_geometry.cpp \
	src/track/track_overlap.cpp \
	src/track/track_graph.cpp \
	src/track/track_rules.cpp \
	src/track/track_road_geometry.cpp \
	src/track/track_surface_geometry.cpp \
	src/track/track_surface_query.cpp \
	src/track/track_validation.cpp

.PHONY: all build run test clean help neon-racer racer racer-test

help:
	@echo "Usage:"
	@echo "  make build                Build Neon Racer"
	@echo "  make run                  Build and run Neon Racer"
	@echo "  make test                 Build and run headless unit tests"
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

$(RACE_PHYSICS_TEST): $(RACE_PHYSICS_TEST_SOURCES)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $(RACE_PHYSICS_TEST_SOURCES)

$(TIME_TRIAL_TEST): $(TIME_TRIAL_TEST_SOURCES)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $(TIME_TRIAL_TEST_SOURCES)

test racer-test: $(TRACK_TEST) $(RACE_PHYSICS_TEST) $(TIME_TRIAL_TEST)
	./$(TRACK_TEST)
	./$(RACE_PHYSICS_TEST)
	./$(TIME_TRIAL_TEST)

clean:
	$(RM) -r $(BUILD_DIR)
