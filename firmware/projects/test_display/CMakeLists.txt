# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

list(APPEND EXTRA_COMPONENT_DIRS "../../drivers")
list(APPEND EXTRA_COMPONENT_DIRS "../../middleware")
list(APPEND EXTRA_COMPONENT_DIRS "../../u8g2")
list(APPEND EXTRA_COMPONENT_DIRS "../../components")
list(APPEND EXTRA_COMPONENT_DIRS "../../modules/interfaz_usuario")
list(APPEND EXTRA_COMPONENT_DIRS "../../modules/sensado")

# Restrict compiled components to isolate the framework and hide unwanted drivers
set(COMPONENTS main drivers middleware u8g2 display interfaz_usuario sensado
               buzzer_alarm optical_sensor drop_detection)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(test_display)
