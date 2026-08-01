CXX ?= g++
CXXFLAGS ?= -Wall -Wextra -std=c++11
PROJECT_CXXFLAGS := -Iinclude -Iinclude/neon_racer/editor -Isrc

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
VEHICLE_DYNAMICS_TEST := $(BUILD_DIR)/tests/vehicle_dynamics_tests
GHOST_REPLAY_TEST := $(BUILD_DIR)/tests/ghost_replay_tests
TIME_TRIAL_TEST := $(BUILD_DIR)/tests/time_trial_tests
TRACK_POSITION_TEST := $(BUILD_DIR)/tests/track_position_tests
PLAYABLE_IO_TEST := $(BUILD_DIR)/tests/playable_io_tests
MAIN_MENU_FLOW_TEST := $(BUILD_DIR)/tests/main_menu_flow_tests
PIECE_CATALOG_TEST := $(BUILD_DIR)/tests/piece_catalog_tests
REPOSITORY_HARDENING_TEST := $(BUILD_DIR)/tests/repository_hardening_tests

DRAFT_PERSISTENCE_SOURCES := src/persistence/common/atomic_file_writer.cpp \
	src/persistence/draft_io.cpp \
	src/persistence/storage_paths.cpp \
	src/persistence/track_layout_codec.cpp

PLAYABLE_PERSISTENCE_SOURCES := src/persistence/playable/playable_library_storage.cpp \
	src/persistence/playable/playable_package_codec.cpp \
	src/persistence/playable/playable_package_store.cpp \
	src/persistence/playable/playable_package_validation.cpp

PLAYABLE_SOURCES := src/playable/playable_export.cpp

RACE_VEHICLE_SOURCES := src/race/vehicle_dynamics.cpp \
	src/race/race_physics.cpp

RACE_GHOST_SOURCES := src/race/ghost_replay.cpp

TRACK_POSITION_SOURCES := src/race/time_trial/track_position_tracker.cpp

TIME_TRIAL_SOURCES := src/race/time_trial.cpp \
	$(TRACK_POSITION_SOURCES) \
	$(RACE_GHOST_SOURCES) \
	$(RACE_VEHICLE_SOURCES)

EDITOR_SOURCES := src/editor/core/editor.cpp \
	src/editor/interaction/editor_input.cpp \
	src/editor/model/editor_preview.cpp \
	src/editor/presentation/editor_scene.cpp \
	src/editor/presentation/editor_panel.cpp \
	src/editor/persistence/editor_library_ui.cpp \
	src/editor/editor_camera.cpp \
	src/editor/editor_commands.cpp \
	src/editor/editor_history.cpp \
	src/editor/editor_library.cpp \
	src/editor/piece_catalog.cpp \
	src/editor/piece_palette.cpp \
	src/editor/editor_picking.cpp

APP_SOURCES := src/app/main.cpp \
	src/app/application.cpp \
	src/app/main_menu.cpp \
	src/app/main_menu_flow.cpp \
	src/app/playable_library.cpp \
	src/app/raylib_race_input.cpp \
	$(EDITOR_SOURCES) \
	$(DRAFT_PERSISTENCE_SOURCES) \
	$(PLAYABLE_PERSISTENCE_SOURCES) \
	$(TIME_TRIAL_SOURCES) \
	$(PLAYABLE_SOURCES) \
	src/render/car_renderer.cpp \
	src/render/race_scene.cpp \
	src/render/time_trial_renderer.cpp \
	src/render/track_renderer.cpp \
	src/track/track_graph.cpp \
	src/track/track_rules.cpp \
	src/track/track_road_geometry.cpp \
	src/track/track_surface_geometry.cpp \
	src/track/track_surface_query.cpp \
	src/track/track_validation.cpp \
	src/track/track_fingerprint.cpp \
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
	src/track/track_fingerprint.cpp \
	$(DRAFT_PERSISTENCE_SOURCES)

RACE_PHYSICS_TEST_SOURCES := tests/unit/test_race_physics.cpp \
	src/race/race_physics.cpp

RACE_TRACK_SOURCES := src/track/track_layout.cpp \
	src/track/track_piece_geometry.cpp \
	src/track/track_overlap.cpp \
	src/track/track_graph.cpp \
	src/track/track_rules.cpp \
	src/track/track_road_geometry.cpp \
	src/track/track_surface_geometry.cpp \
	src/track/track_surface_query.cpp \
	src/track/track_validation.cpp \
	src/track/track_fingerprint.cpp

VEHICLE_DYNAMICS_TEST_SOURCES := tests/unit/test_vehicle_dynamics.cpp \
	$(RACE_VEHICLE_SOURCES) \
	$(RACE_TRACK_SOURCES)

GHOST_REPLAY_TEST_SOURCES := tests/unit/test_ghost_replay.cpp \
	$(RACE_GHOST_SOURCES)

TIME_TRIAL_TEST_SOURCES := tests/unit/test_time_trial.cpp \
	$(TIME_TRIAL_SOURCES) \
	$(RACE_TRACK_SOURCES)

TRACK_POSITION_TEST_SOURCES := tests/unit/test_track_position_tracker.cpp \
	$(TRACK_POSITION_SOURCES) \
	$(RACE_TRACK_SOURCES)

PLAYABLE_IO_TEST_SOURCES := tests/unit/test_playable_io.cpp \
	$(PLAYABLE_SOURCES) \
	$(PLAYABLE_PERSISTENCE_SOURCES) \
	$(DRAFT_PERSISTENCE_SOURCES) \
	$(TIME_TRIAL_SOURCES) \
	$(RACE_TRACK_SOURCES)

MAIN_MENU_FLOW_TEST_SOURCES := tests/unit/test_main_menu_flow.cpp \
	src/app/main_menu_flow.cpp

PIECE_CATALOG_TEST_SOURCES := tests/unit/test_piece_catalog.cpp \
	src/editor/piece_catalog.cpp \
	$(RACE_TRACK_SOURCES)

REPOSITORY_HARDENING_TEST_SOURCES := tests/unit/test_repository_hardening.cpp \
	$(DRAFT_PERSISTENCE_SOURCES) \
	$(RACE_TRACK_SOURCES)

.PHONY: all build run test clean help publish-tag neon-racer racer racer-test

help:
	@echo "Usage:"
	@echo "  make build                Build Neon Racer"
	@echo "  make run                  Build and run Neon Racer"
	@echo "  make test                 Build and run headless unit tests"
	@echo "  make publish-tag TAG=vX   Push main and one version tag to GitHub"
	@echo "  make clean                Remove generated build products"

all: build

build: $(APP)

$(APP): $(APP_SOURCES)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(PROJECT_CXXFLAGS) $(RAYLIB_CFLAGS) -o $@ $(APP_SOURCES) $(RAYLIB_LIBS)

run: $(APP)
	./$(APP)

$(TRACK_TEST): $(TRACK_TEST_SOURCES)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(PROJECT_CXXFLAGS) -o $@ $(TRACK_TEST_SOURCES)

$(RACE_PHYSICS_TEST): $(RACE_PHYSICS_TEST_SOURCES)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(PROJECT_CXXFLAGS) -o $@ $(RACE_PHYSICS_TEST_SOURCES)

$(VEHICLE_DYNAMICS_TEST): $(VEHICLE_DYNAMICS_TEST_SOURCES)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(PROJECT_CXXFLAGS) -o $@ $(VEHICLE_DYNAMICS_TEST_SOURCES)

$(GHOST_REPLAY_TEST): $(GHOST_REPLAY_TEST_SOURCES)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(PROJECT_CXXFLAGS) -o $@ $(GHOST_REPLAY_TEST_SOURCES)

$(TIME_TRIAL_TEST): $(TIME_TRIAL_TEST_SOURCES)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(PROJECT_CXXFLAGS) -o $@ $(TIME_TRIAL_TEST_SOURCES)

$(TRACK_POSITION_TEST): $(TRACK_POSITION_TEST_SOURCES)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(PROJECT_CXXFLAGS) -o $@ $(TRACK_POSITION_TEST_SOURCES)

$(PLAYABLE_IO_TEST): $(PLAYABLE_IO_TEST_SOURCES)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(PROJECT_CXXFLAGS) -o $@ $(PLAYABLE_IO_TEST_SOURCES)

$(MAIN_MENU_FLOW_TEST): $(MAIN_MENU_FLOW_TEST_SOURCES)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(PROJECT_CXXFLAGS) -o $@ $(MAIN_MENU_FLOW_TEST_SOURCES)

$(PIECE_CATALOG_TEST): $(PIECE_CATALOG_TEST_SOURCES)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(PROJECT_CXXFLAGS) -o $@ $(PIECE_CATALOG_TEST_SOURCES)

$(REPOSITORY_HARDENING_TEST): $(REPOSITORY_HARDENING_TEST_SOURCES)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(PROJECT_CXXFLAGS) -o $@ $(REPOSITORY_HARDENING_TEST_SOURCES)

test racer-test: $(TRACK_TEST) $(RACE_PHYSICS_TEST) $(VEHICLE_DYNAMICS_TEST) $(GHOST_REPLAY_TEST) $(TIME_TRIAL_TEST) $(TRACK_POSITION_TEST) $(PLAYABLE_IO_TEST) $(MAIN_MENU_FLOW_TEST) $(PIECE_CATALOG_TEST) $(REPOSITORY_HARDENING_TEST)
	./$(TRACK_TEST)
	./$(RACE_PHYSICS_TEST)
	./$(VEHICLE_DYNAMICS_TEST)
	./$(GHOST_REPLAY_TEST)
	./$(TIME_TRIAL_TEST)
	./$(TRACK_POSITION_TEST)
	./$(PLAYABLE_IO_TEST)
	./$(MAIN_MENU_FLOW_TEST)
	./$(PIECE_CATALOG_TEST)
	./$(REPOSITORY_HARDENING_TEST)

# GitHub Actions receives a tag only after Git pushes it. Publish exactly one
# validated main-history version tag so a release workflow gets one event.
publish-tag:
	@test -n "$(TAG)" || { echo "Usage: make publish-tag TAG=vX.Y.Z"; exit 2; }
	@case "$(TAG)" in v*) ;; *) echo "TAG must begin with v"; exit 2;; esac
	@git rev-parse --verify --quiet "refs/tags/$(TAG)" >/dev/null || { echo "Unknown tag: $(TAG)"; exit 2; }
	@git merge-base --is-ancestor "$(TAG)" main || { echo "Tag $(TAG) is not reachable from main"; exit 2; }
	git push origin main "refs/tags/$(TAG)"

clean:
	$(RM) -r $(BUILD_DIR)
