cmake_minimum_required(VERSION 3.22)

# Firmware dependencies must come from the installed ESP-IDF SDK only.
set(ENV{IDF_COMPONENT_MANAGER} "0")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(MINIMAL_BUILD ON)
project(precisionshot VERSION 1.0.0)
