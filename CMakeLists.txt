cmake_minimum_required(VERSION 3.16)
project(neon_racer VERSION 0.2.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(NEON_RACER_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(NEON_RACER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(NEON_RACER_EDITOR_INCLUDE_DIR ${NEON_RACER_INCLUDE_DIR}/neon_racer/editor)

option(NEON_RACER_BUILD_APP "Build the Raylib-backed Neon Racer application" ON)

add_library(neon_racer_domain
    src/persistence/common/atomic_file_writer.cpp
    src/persistence/draft_io.cpp
    src/persistence/storage_paths.cpp
    src/persistence/track_layout_codec.cpp
    src/track/track_layout.cpp
    src/track/track_fingerprint.cpp
    src/track/track_piece_geometry.cpp
    src/track/track_overlap.cpp
    src/track/track_graph.cpp
    src/track/track_progress_graph.cpp
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
    src/persistence/playable/playable_library_storage.cpp
    src/persistence/playable/playable_package_codec.cpp
    src/persistence/playable/playable_package_store.cpp
    src/persistence/playable/playable_package_validation.cpp
    src/playable/playable_export.cpp
)
target_include_directories(neon_racer_playable PUBLIC ${NEON_RACER_INCLUDE_DIR})
target_compile_options(neon_racer_playable PRIVATE -Wall -Wextra)
target_link_libraries(neon_racer_playable PUBLIC neon_racer_domain neon_racer_ghost_replay)

add_library(neon_racer_time_trial
    src/race/time_trial.cpp
    src/race/time_trial/track_position_geometry.cpp
    src/race/time_trial/track_position_tracker.cpp
)
target_compile_options(neon_racer_time_trial PRIVATE -Wall -Wextra)
target_link_libraries(neon_racer_time_trial PUBLIC neon_racer_domain neon_racer_vehicle_dynamics neon_racer_ghost_replay)

if(NEON_RACER_BUILD_APP)
    add_library(neon_racer_raylib INTERFACE)
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
        src/app/playable_library.cpp src/app/playable_library_flow.cpp
        src/app/race_pause_menu.cpp src/app/race_pause_flow.cpp
        src/app/race_results_menu.cpp src/app/race_results_flow.cpp
        src/app/raylib_race_input.cpp
        src/editor/core/editor.cpp
        src/editor/interaction/editor_input.cpp
        src/editor/model/editor_preview.cpp
        src/editor/presentation/editor_scene.cpp
        src/editor/presentation/editor_panel.cpp
        src/editor/presentation/editor_tracking_graph.cpp
        src/editor/persistence/editor_library_ui.cpp
        src/editor/editor_camera.cpp src/editor/editor_commands.cpp src/editor/editor_history.cpp
        src/editor/editor_library.cpp src/editor/piece_catalog.cpp src/editor/piece_palette.cpp src/editor/editor_picking.cpp
        src/render/car_renderer.cpp src/render/race_scene.cpp src/render/time_trial_renderer.cpp
        src/render/track_renderer.cpp src/ui/neon.cpp
    )
    target_include_directories(neon-racer PRIVATE
        ${NEON_RACER_INCLUDE_DIR}
        ${NEON_RACER_EDITOR_INCLUDE_DIR}
        ${NEON_RACER_SOURCE_DIR}
    )
    target_compile_options(neon-racer PRIVATE -Wall -Wextra)
    target_link_libraries(neon-racer PRIVATE neon_racer_time_trial neon_racer_playable neon_racer_raylib)
endif()

enable_testing()
add_executable(neon-racer-track-tests tests/unit/test_track.cpp)
target_compile_options(neon-racer-track-tests PRIVATE -Wall -Wextra)
target_link_libraries(neon-racer-track-tests PRIVATE neon_racer_domain)
add_test(NAME neon-racer-track-tests COMMAND neon-racer-track-tests)

add_executable(neon-racer-track-progress-graph-tests tests/unit/test_track_progress_graph.cpp)
target_compile_options(neon-racer-track-progress-graph-tests PRIVATE -Wall -Wextra)
target_link_libraries(neon-racer-track-progress-graph-tests PRIVATE neon_racer_domain)
add_test(NAME neon-racer-track-progress-graph-tests COMMAND neon-racer-track-progress-graph-tests)

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

add_executable(neon-racer-time-trial-tests tests/unit/test_time_trial.cpp)
target_compile_options(neon-racer-time-trial-tests PRIVATE -Wall -Wextra)
target_link_libraries(neon-racer-time-trial-tests PRIVATE neon_racer_time_trial)
add_test(NAME neon-racer-time-trial-tests COMMAND neon-racer-time-trial-tests)

add_executable(neon-racer-track-position-tests tests/unit/test_track_position_tracker.cpp)
target_compile_options(neon-racer-track-position-tests PRIVATE -Wall -Wextra)
target_link_libraries(neon-racer-track-position-tests PRIVATE neon_racer_time_trial)
add_test(NAME neon-racer-track-position-tests COMMAND neon-racer-track-position-tests)

add_executable(neon-racer-checkpoint-recovery-tests tests/unit/test_checkpoint_recovery.cpp)
target_compile_options(neon-racer-checkpoint-recovery-tests PRIVATE -Wall -Wextra)
target_link_libraries(neon-racer-checkpoint-recovery-tests PRIVATE neon_racer_time_trial)
add_test(NAME neon-racer-checkpoint-recovery-tests COMMAND neon-racer-checkpoint-recovery-tests)

add_executable(neon-racer-playable-io-tests tests/unit/test_playable_io.cpp)
target_compile_options(neon-racer-playable-io-tests PRIVATE -Wall -Wextra)
target_link_libraries(neon-racer-playable-io-tests PRIVATE neon_racer_playable neon_racer_time_trial)
add_test(NAME neon-racer-playable-io-tests COMMAND neon-racer-playable-io-tests)

add_executable(neon-racer-main-menu-flow-tests tests/unit/test_main_menu_flow.cpp src/app/main_menu_flow.cpp)
target_compile_options(neon-racer-main-menu-flow-tests PRIVATE -Wall -Wextra)
add_test(NAME neon-racer-main-menu-flow-tests COMMAND neon-racer-main-menu-flow-tests)

add_executable(neon-racer-playable-library-flow-tests
    tests/unit/test_playable_library_flow.cpp src/app/playable_library_flow.cpp)
target_compile_options(neon-racer-playable-library-flow-tests PRIVATE -Wall -Wextra)
add_test(NAME neon-racer-playable-library-flow-tests COMMAND neon-racer-playable-library-flow-tests)

add_executable(neon-racer-race-pause-flow-tests tests/unit/test_race_pause_flow.cpp src/app/race_pause_flow.cpp)
target_compile_options(neon-racer-race-pause-flow-tests PRIVATE -Wall -Wextra)
add_test(NAME neon-racer-race-pause-flow-tests COMMAND neon-racer-race-pause-flow-tests)

add_executable(neon-racer-race-results-flow-tests tests/unit/test_race_results_flow.cpp src/app/race_results_flow.cpp)
target_compile_options(neon-racer-race-results-flow-tests PRIVATE -Wall -Wextra)
add_test(NAME neon-racer-race-results-flow-tests COMMAND neon-racer-race-results-flow-tests)

add_executable(neon-racer-piece-catalog-tests tests/unit/test_piece_catalog.cpp src/editor/piece_catalog.cpp)
target_include_directories(neon-racer-piece-catalog-tests PRIVATE
    ${NEON_RACER_INCLUDE_DIR}
    ${NEON_RACER_EDITOR_INCLUDE_DIR}
    ${NEON_RACER_SOURCE_DIR}
)
target_compile_options(neon-racer-piece-catalog-tests PRIVATE -Wall -Wextra)
target_link_libraries(neon-racer-piece-catalog-tests PRIVATE neon_racer_domain)
add_test(NAME neon-racer-piece-catalog-tests COMMAND neon-racer-piece-catalog-tests)

add_executable(neon-racer-editor-connector-guide-tests tests/unit/test_editor_connector_guides.cpp)
target_include_directories(neon-racer-editor-connector-guide-tests PRIVATE ${NEON_RACER_SOURCE_DIR})
target_compile_options(neon-racer-editor-connector-guide-tests PRIVATE -Wall -Wextra)
target_link_libraries(neon-racer-editor-connector-guide-tests PRIVATE neon_racer_domain)
add_test(NAME neon-racer-editor-connector-guide-tests COMMAND neon-racer-editor-connector-guide-tests)

add_executable(neon-racer-repository-hardening-tests tests/unit/test_repository_hardening.cpp)
target_compile_options(neon-racer-repository-hardening-tests PRIVATE -Wall -Wextra)
target_link_libraries(neon-racer-repository-hardening-tests PRIVATE neon_racer_domain)
add_test(NAME neon-racer-repository-hardening-tests COMMAND neon-racer-repository-hardening-tests)
