# Pact MP-1 desktop simulator (host build, SDL2).
#   cmake -S sim -B sim/build && cmake --build sim/build -j
cmake_minimum_required(VERSION 3.20)
project(pact_sim C)

set(CMAKE_C_STANDARD 11)
add_compile_definitions(PACT_SIM=1)

set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../lib/lv_conf.h" CACHE STRING "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../lib/lvgl ${CMAKE_BINARY_DIR}/lvgl)

find_package(SDL2 REQUIRED CONFIG)
target_link_libraries(lvgl PUBLIC SDL2::SDL2)

file(GLOB APP_UI_SRC
     ${CMAKE_CURRENT_SOURCE_DIR}/../App/ui/*.c
     ${CMAKE_CURRENT_SOURCE_DIR}/../App/ui/assets/*.c)
file(GLOB APP_AUDIO_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../App/audio/*.c)

add_executable(pact_sim main.c ${APP_UI_SRC} ${APP_AUDIO_SRC})
target_include_directories(pact_sim PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../App
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib)
target_link_libraries(pact_sim PRIVATE lvgl SDL2::SDL2 m pthread)

# Headless decode/verify tool (no SDL, no LVGL)
add_executable(decode_test decode_test.c ${APP_AUDIO_SRC})
target_include_directories(decode_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../App
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib)
target_link_libraries(decode_test PRIVATE m pthread)

# Library scanner test harness (headless)
file(GLOB APP_LIB_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../App/library/*.c)
add_executable(library_test library_test.c ${APP_LIB_SRC})
target_include_directories(library_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../App)
