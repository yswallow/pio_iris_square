cmake_minimum_required(VERSION 3.12)

set(Name "bad_apple")
set(ProjectName "${Name}-project")

if(NOT DEFINED PICO_SDK_PATH)
  file(DOWNLOAD
    https://raw.githubusercontent.com/raspberrypi/pico-sdk/master/external/pico_sdk_import.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/pico_sdk_import.cmake)
  set(PICO_SDK_FETCH_FROM_GIT 1)
  include(${CMAKE_CURRENT_BINARY_DIR}/pico_sdk_import.cmake)
else()
  include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)
endif()

project(${ProjectName})
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(${Name} main.c)
pico_generate_pio_header(${Name} ${CMAKE_CURRENT_LIST_DIR}/asm.pio)

target_link_libraries(${Name} pico_stdlib hardware_pio pico_multicore)
pico_add_extra_outputs(${Name})
