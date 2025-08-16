# Root CMakeLists.txt (save this as CMakeLists.txt in the root directory)
cmake_minimum_required(VERSION 3.16)

# Set project version
set(PROJECT_VER "1.0.0")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(bus_display_led)