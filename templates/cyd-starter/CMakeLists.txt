cmake_minimum_required(VERSION 3.16)

# The library is a component: its path is written here by tools/new-project.sh (change it if you move the library).
set(EXTRA_COMPONENT_DIRS "@CYDUI_PATH@")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(@NAME@)
