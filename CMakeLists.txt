cmake_minimum_required(VERSION 3.16)
project(neon_racer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_path(RAYLIB_INCLUDE_DIR raylib.h)
find_library(RAYLIB_LIBRARY NAMES raylib)
if(NOT RAYLIB_INCLUDE_DIR OR NOT RAYLIB_LIBRARY)
    message(FATAL_ERROR "Raylib was not found. Set RAYLIB_INCLUDE_DIR and RAYLIB_LIBRARY if it is installed outside standard paths.")
endif()

add_library(neon_racer_raylib INTERFACE)
target_include_directories(neon_racer_raylib INTERFACE ${RAYLIB_INCLUDE_DIR})
target_link_libraries(neon_racer_raylib INTERFACE ${RAYLIB_LIBRARY})

add_library(neon_racer_domain
    src/persistence/draft_io.cpp
    src/track/playable_export.cpp
    src/track/track_layout.cpp
    src/track/track_piece_geometry.cpp
    src/track/track_overlap.cpp
    src/track/track_graph.cpp
    src/track/track_rules.cpp
    src/track/track_surface_geometry.cpp
    src/track/track_surface_query.cpp
    src/track/track_validation.cpp
)
target_compile_options(neon_racer_domain PRIVATE -Wall -Wextra)

add_executable(neon-racer
    src/app/main.cpp src/app/application.cpp
    src/editor/editor.cpp src/editor/editor_commands.cpp src/editor/editor_history.cpp src/editor/editor_library.cpp
    src/race/race.cpp src/race/race_input.cpp
    src/render/car_renderer.cpp src/render/race_scene.cpp src/render/track_renderer.cpp src/ui/neon.cpp
)
target_compile_options(neon-racer PRIVATE -Wall -Wextra)
target_link_libraries(neon-racer PRIVATE neon_racer_domain neon_racer_raylib)

enable_testing()
add_executable(neon-racer-track-tests tests/unit/test_track.cpp)
target_compile_options(neon-racer-track-tests PRIVATE -Wall -Wextra)
target_link_libraries(neon-racer-track-tests PRIVATE neon_racer_domain)
add_test(NAME neon-racer-track-tests COMMAND neon-racer-track-tests)
