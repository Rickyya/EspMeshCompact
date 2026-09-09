cmake_minimum_required(VERSION 3.16)

# RadioLib builds every modem and protocol it supports by default. Trim the
# ones this example does not use. These must come before project().
add_compile_definitions(RADIOLIB_EXCLUDE_RF69)
add_compile_definitions(RADIOLIB_EXCLUDE_SX1231)
add_compile_definitions(RADIOLIB_EXCLUDE_AFSK)
add_compile_definitions(RADIOLIB_EXCLUDE_APRS)
add_compile_definitions(RADIOLIB_EXCLUDE_AX25)
add_compile_definitions(RADIOLIB_EXCLUDE_BELL)
add_compile_definitions(RADIOLIB_EXCLUDE_FSK4)
add_compile_definitions(RADIOLIB_EXCLUDE_HELLSCHREIBER)
add_compile_definitions(RADIOLIB_EXCLUDE_MORSE)
add_compile_definitions(RADIOLIB_EXCLUDE_PAGER)
add_compile_definitions(RADIOLIB_EXCLUDE_RTTY)
add_compile_definitions(RADIOLIB_EXCLUDE_SSTV)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(mc_receiver)
