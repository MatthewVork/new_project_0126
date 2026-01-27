cmake_minimum_required(VERSION 3.10)
project(ChessClient C)
set(CMAKE_C_STANDARD 99)
add_compile_options(-O3 -g)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

include_directories(
    . 
    ../Common
    game
    network
    ui_generated
    lvgl
    lv_drivers
)

add_definitions(-DLV_CONF_INCLUDE_SIMPLE)
add_definitions(-DLV_LVGL_H_INCLUDE_SIMPLE)

file(GLOB_RECURSE MY_SOURCES "main.c" "network/*.c" "game/*.c" "ui_generated/*.c")
file(GLOB_RECURSE LVGL_SOURCES "lvgl/src/*.c")
file(GLOB DRIVERS_SOURCES "lv_drivers/*.c" "lv_drivers/sdl/*.c")

add_executable(game_client
    ${MY_SOURCES}
    ${LVGL_SOURCES}
    ${DRIVERS_SOURCES}
    "../Common/cJSON.c"
)

target_link_libraries(game_client ${SDL2_LIBRARIES} m pthread)