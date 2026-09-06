cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(esp32-cyd)

# Target board (2.8"/3.5" CYD variant, panel driver) is selected in
# main/board_config.h, not here — edit that file, then `idf.py fullclean`.
