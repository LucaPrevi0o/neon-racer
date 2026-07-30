cmake_minimum_required(VERSION 3.16)
project(neon_racer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(NEON_RACER_BUILD_APP "Build the Raylib-backed Neon Racer application" ON)

add_library(neon_racer_domain
    src/persistence/draft_io.cpp
    src/persistence/storage_paths.cpp
    src/persistence/track_layout_codec.cpp
    src/track/track_layout.cpp
    src/track/track_fingerprint.cpp
    src/track/track_piece_geometry.cpp
    src/track/track_overlap.cpp
    src/track/track_graph.cpp
    src/track/track_rules.cpp
    src/track/track_road_geometry.cpp
    src/track/track_surface_geometry.cpp
    src/track/track_surface_query.cpp
    src/track/track_validation.cpp
)
target_compile_options(neon_racer_domain PRIVATE -Wall -Wextra)

add_library(neon_racer_vehicle_dynamics
    src/race/race_physics.cpp
    src/race/vehicle_dynamics.cpp
)
target_compile_options(neon_racer_vehicle_dynamics PRIVATE -Wall -Wextra)
target_link_libraries(neon_racer_vehicle_dynamics PUBLIC neon_racer_domain)

add_library(neon_racer_ghost_replay src/race/ghost_replay.cpp)
target_compile_options(neon_racer_ghost_replay PRIVATE -Wall -Wextra)

add_library(neon_racer_playable
    src/persistence/playable_track_io.cpp
    src/playable/playable_export.cpp
)
target_compile_options(neon_racer_playable PRIVATE -Wall -Wextra)
target_link_libraries(neon_racer_playable PUBLIC neon_racer_domain neon_racer_ghost_replay)

add_library(neon_racer_time_trial src/race/time_trial.cpp)
target_compile_options(neon_racer_time_trial PRIVATE -Wall -Wextra)
target_link_libraries(neon_racer_time_trial PUBLIC neon_racer_domain neon_racer_vehicle_dynamics neon_racer_ghost_replay)

if(NEON_RACER_BUILD_APP)
    add_library(neon_racer_raylib INTERFACE)
    # Prefer Raylib's exported CMake target when available. It carries the
    # platform libraries needed by a static Raylib build, so tagged release
    # builds link consistently. Preserve the manual-library fallback used by
    # simple local installations.
    find_package(raylib CONFIG QUIET)
    if(TARGET raylib)
        target_link_libraries(neon_racer_raylib INTERFACE raylib)
    else()
        find_path(RAYLIB_INCLUDE_DIR raylib.h)
        find_library(RAYLIB_LIBRARY NAMES raylib)
        if(NOT RAYLIB_INCLUDE_DIR OR NOT RAYLIB_LIBRARY)
            message(FATAL_ERROR "Raylib was not found. Set RAYLIB_INCLUDE_DIR and RAYLIB_LIBRARY if it is installed outside standard paths.")
        endif()
        target_include_directories(neon_racer_raylib INTERFACE ${RAYLIB_INCLUDE_DIR})
        target_link_libraries(neon_racer_raylib INTERFACE ${RAYLIB_LIBRARY})
    endif()

    add_executable(neon-racer
        src/app/main.cpp src/app/application.cpp src/app/main_menu.cpp src/app/main_menu_flow.cpp
        src/app/playable_library.cpp src/app/raylib_race_input.cpp
        src/editor/editor.cpp src/editor/editor_camera.cpp src/editor/editor_commands.cpp src/editor/editor_history.cpp
        src/editor/editor_library.cpp src/editor/piece_catalog.cpp src/editor/piece_palette.cpp src/editor/editor_picking.cpp
        src/render/car_renderer.cpp src/render/race_scene.cpp src/render/time_trial_renderer.cpp
        src/render/track_renderer.cpp src/ui/neon.cpp
    )
    target_compile_options(neon-racer PRIVATE -Wall -Wextra)
    target_link_libraries(neon-racer PRIVATE neon_racer_time_trial neon_racer_playable neon_racer_raylib)
endif()

enable_testing()
add_executable(neon-racer-track-tests tests/unit/test_track.cpp)
target_compile_options(neon-racer-track-tests PRIVATE -Wall -Wextra)
target_link_libraries(neon-racer-track-tests PRIVATE neon_racer_domain)
add_test(NAME neon-racer-track-tests COMMAND neon-racer-track-tests)

add_executable(neon-racer-race-physics-tests tests/unit/test_race_physics.cpp src/race/race_physics.cpp)
target_compile_options(neon-racer-race-physics-tests PRIVATE -Wall -Wextra)
add_test(NAME neon-racer-race-physics-tests COMMAND neon-racer-race-physics-tests)

add_executable(neon-racer-vehicle-dynamics-tests tests/unit/test_vehicle_dynamics.cpp)
target_compile_options(neon-racer-vehicle-dynamics-tests PRIVATE -Wall -Wextra)
target_link_libraries(neon-racer-vehicle-dynamics-tests PRIVATE neon_racer_vehicle_dynamics)
add_test(NAME neon-racer-vehicle-dynamics-tests COMMAND neon-racer-vehicle-dynamics-tests)

add_executable(neon-racer-ghost-replay-tests tests/unit/test_ghost_replay.cpp)
target_compile_options(neon-racer-ghost-replay-tests PRIVATE -Wall -Wextra)
target_link_libraries(neon-racer-ghost-replay-tests PRIVATE neon_racer_ghost_replay)
add_test(NAME neon-racer-ghost-replay-tests COMMAND neon-racer-ghost-replay-tests)

add_executable(neon-racer-time-trial-tests
    tests/unit/test_time_trial.cpp
)
target_compile_options(neon-racer-time-trial-tests PRIVATE -Wall -Wextra)
target_link_libraries(neon-racer-time-trial-tests PRIVATE neon_racer_time_trial)
add_test(NAME neon-racer-time-trial-tests COMMAND neon-racer-time-trial-tests)

add_executable(neon-racer-playable-io-tests tests/unit/test_playable_io.cpp)
target_compile_options(neon-racer-playable-io-tests PRIVATE -Wall -Wextra)
target_link_libraries(neon-racer-playable-io-tests PRIVATE neon_racer_playable neon_racer_time_trial)
add_test(NAME neon-racer-playable-io-tests COMMAND neon-racer-playable-io-tests)

add_executable(neon-racer-main-menu-flow-tests tests/unit/test_main_menu_flow.cpp src/app/main_menu_flow.cpp)
target_compile_options(neon-racer-main-menu-flow-tests PRIVATE -Wall -Wextra)
add_test(NAME neon-racer-main-menu-flow-tests COMMAND neon-racer-main-menu-flow-tests)

add_executable(neon-racer-piece-catalog-tests tests/unit/test_piece_catalog.cpp src/editor/piece_catalog.cpp)
target_compile_options(neon-racer-piece-catalog-tests PRIVATE -Wall -Wextra)
target_link_libraries(neon-racer-piece-catalog-tests PRIVATE neon_racer_domain)
add_test(NAME neon-racer-piece-catalog-tests COMMAND neon-racer-piece-catalog-tests)
