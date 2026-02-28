cmake_minimum_required(VERSION 3.20)
project(commander_chess LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(COMMANDER_ENABLE_WEBGPU "Enable WebGPU evaluator scaffolding via Dawn/webgpu.h" OFF)
set(COMMANDER_WEBGPU_INCLUDE_DIR "" CACHE PATH "Path containing webgpu/webgpu.h (Google Dawn)")
set(COMMANDER_DAWN_LIBS "" CACHE STRING "Optional Dawn native libs (semicolon-separated)")

# Homebrew prefix (Apple Silicon)
list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew")

find_package(PkgConfig REQUIRED)

pkg_check_modules(SDL2       REQUIRED sdl2)
pkg_check_modules(SDL2_IMAGE REQUIRED SDL2_image)
pkg_check_modules(SDL2_MIXER REQUIRED SDL2_mixer)
pkg_check_modules(SDL2_TTF   REQUIRED SDL2_ttf)

add_executable(commander_chess commander_chess.cpp)

target_include_directories(commander_chess PRIVATE
        ${SDL2_INCLUDE_DIRS}
        ${SDL2_IMAGE_INCLUDE_DIRS}
        ${SDL2_MIXER_INCLUDE_DIRS}
        ${SDL2_TTF_INCLUDE_DIRS}
)

target_link_directories(commander_chess PRIVATE
        ${SDL2_LIBRARY_DIRS}
        ${SDL2_IMAGE_LIBRARY_DIRS}
        ${SDL2_MIXER_LIBRARY_DIRS}
        ${SDL2_TTF_LIBRARY_DIRS}
)

target_compile_options(commander_chess PRIVATE
        ${SDL2_CFLAGS_OTHER}
        ${SDL2_IMAGE_CFLAGS_OTHER}
        ${SDL2_MIXER_CFLAGS_OTHER}
        ${SDL2_TTF_CFLAGS_OTHER}
)

target_link_libraries(commander_chess PRIVATE
        ${SDL2_LIBRARIES}
        ${SDL2_IMAGE_LIBRARIES}
        ${SDL2_MIXER_LIBRARIES}
        ${SDL2_TTF_LIBRARIES}
)

if (COMMANDER_ENABLE_WEBGPU)
    target_compile_definitions(commander_chess PRIVATE COMMANDER_ENABLE_WEBGPU=1)
    if (COMMANDER_WEBGPU_INCLUDE_DIR)
        target_include_directories(commander_chess PRIVATE ${COMMANDER_WEBGPU_INCLUDE_DIR})
    endif()
    if (COMMANDER_DAWN_LIBS)
        target_link_libraries(commander_chess PRIVATE ${COMMANDER_DAWN_LIBS})
    endif()
endif()

target_link_options(commander_chess PRIVATE
        ${SDL2_LDFLAGS_OTHER}
        ${SDL2_IMAGE_LDFLAGS_OTHER}
        ${SDL2_MIXER_LDFLAGS_OTHER}
        ${SDL2_TTF_LDFLAGS_OTHER}
)
