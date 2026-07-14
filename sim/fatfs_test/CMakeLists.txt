# FatFs storage-stack test: runs the DEVICE scan + pact_io paths on a
# file-backed disk image. Must compile WITHOUT PACT_SIM (that would select
# the POSIX branches); PACT_FATFS_HOST picks pthread FatFs mutexes.
set_property(DIRECTORY PROPERTY COMPILE_DEFINITIONS "")

file(GLOB FATFS_LIB_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/fatfs/*.c)
add_executable(fatfs_test
    fatfs_test.c
    host_walk.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../App/pact_io_fatfs.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../App/library/library.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../App/library/tags.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../App/library/tags_flac.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../App/library/tags_id3.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../App/library/thumbcache.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/lvgl/src/libs/tjpgd/tjpgd.c
    ${FATFS_LIB_SRC})
target_include_directories(fatfs_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../App
    ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/fatfs
    ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/lvgl)
target_compile_definitions(fatfs_test PRIVATE PACT_FATFS_HOST)
target_link_libraries(fatfs_test PRIVATE pthread)
