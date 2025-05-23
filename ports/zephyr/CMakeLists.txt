# This file is part of the MicroPython project, http://micropython.org/
#
# The MIT License (MIT)
#
# Copyright 2020 NXP
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(micropython)

set(MICROPY_PORT_DIR    ${CMAKE_CURRENT_SOURCE_DIR})
set(MICROPY_DIR         ${MICROPY_PORT_DIR}/../..)
set(MICROPY_TARGET      micropython)

include(${MICROPY_DIR}/py/py.cmake)
include(${MICROPY_DIR}/extmod/extmod.cmake)

set(GEN_MACHINE_DEFS_SRC "${CMAKE_BINARY_DIR}/machine_defs.c")

set(MICROPY_SOURCE_PORT
    main.c
    help.c
    machine_i2c.c
    machine_spi.c
    machine_pin.c
    machine_uart.c
    modbluetooth_zephyr.c
    modmachine.c
    modsocket.c
    modzephyr.c
    modzsensor.c
    mphalport.c
    network_nrf91.c
    network_wlan.c
    uart_core.c
    zephyr_storage.c
)
list(TRANSFORM MICROPY_SOURCE_PORT PREPEND ${MICROPY_PORT_DIR}/)

list(APPEND MICROPY_SOURCE_PORT
    ${GEN_MACHINE_DEFS_SRC}
)

set(MICROPY_SOURCE_SHARED
    libc/printf.c
    readline/readline.c
    runtime/interrupt_char.c
    runtime/mpirq.c
    runtime/pyexec.c
    runtime/stdout_helpers.c
    timeutils/timeutils.c
)
list(TRANSFORM MICROPY_SOURCE_SHARED PREPEND ${MICROPY_DIR}/shared/)

set(MICROPY_SOURCE_LIB
    oofatfs/ff.c
    oofatfs/ffunicode.c
    littlefs/lfs1.c
    littlefs/lfs1_util.c
    littlefs/lfs2.c
    littlefs/lfs2_util.c
)
list(TRANSFORM MICROPY_SOURCE_LIB PREPEND ${MICROPY_DIR}/lib/)

set(MICROPY_SOURCE_QSTR
    ${MICROPY_SOURCE_PY}
    ${MICROPY_SOURCE_EXTMOD}
    ${MICROPY_SOURCE_SHARED}
    ${MICROPY_SOURCE_PORT}
)

zephyr_get_include_directories_for_lang(C includes)
zephyr_get_system_include_directories_for_lang(C system_includes)
zephyr_get_compile_definitions_for_lang(C definitions)
zephyr_get_compile_options_for_lang(C options)

set(MICROPY_CPP_FLAGS_EXTRA ${includes} ${system_includes} ${definitions} ${options})

zephyr_library_named(${MICROPY_TARGET})

zephyr_library_include_directories(
    ${MICROPY_INC_CORE}
    ${MICROPY_PORT_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

zephyr_library_compile_options(
    -std=gnu99 -fomit-frame-pointer
)

zephyr_library_compile_definitions(
    NDEBUG
    MP_CONFIGFILE=<${CONFIG_MICROPY_CONFIGFILE}>
    MICROPY_HEAP_SIZE=${CONFIG_MICROPY_HEAP_SIZE}
    FFCONF_H=\"${MICROPY_OOFATFS_DIR}/ffconf.h\"
    MICROPY_VFS_FAT=$<BOOL:${CONFIG_MICROPY_VFS_FAT}>
    MICROPY_VFS_LFS1=$<BOOL:${CONFIG_MICROPY_VFS_LFS1}>
    MICROPY_VFS_LFS2=$<BOOL:${CONFIG_MICROPY_VFS_LFS2}>
)

zephyr_library_sources(
    ${MICROPY_SOURCE_PY}
    ${MICROPY_SOURCE_EXTMOD}
    ${MICROPY_SOURCE_SHARED}
    ${MICROPY_SOURCE_LIB}
    ${MICROPY_SOURCE_PORT}
)

add_dependencies(${MICROPY_TARGET} zephyr_generated_headers)

include(${MICROPY_DIR}/py/mkrules.cmake)

set(GEN_MACHINE_DEFS_PY "${MICROPY_PORT_DIR}/make-machine-defs.py")
set(GEN_MACHINE_DEFS_PREFIX "${MICROPY_PORT_DIR}/machine_prefix.c")
set(GEN_MACHINE_DEFS_HDR "${MICROPY_GENHDR_DIR}/machine_defs.h")
set(GEN_MACHINE_DEFS_EDT_PICKLE "${CMAKE_BINARY_DIR}/zephyr/edt.pickle")

add_custom_command(
    OUTPUT ${GEN_MACHINE_DEFS_HDR} ${GEN_MACHINE_DEFS_SRC}
    COMMAND ${Python3_EXECUTABLE} ${GEN_MACHINE_DEFS_PY} --prefix ${GEN_MACHINE_DEFS_PREFIX} --output-source ${GEN_MACHINE_DEFS_SRC} --output-header ${GEN_MACHINE_DEFS_HDR} --edt-pickle ${GEN_MACHINE_DEFS_EDT_PICKLE}
    DEPENDS
        ${GEN_MACHINE_DEFS_PY}
        ${GEN_MACHINE_DEFS_PREFIX}
        ${GEN_MACHINE_DEFS_EDT_PICKLE}
        ${MICROPY_MPVERSION}
    VERBATIM
    COMMAND_EXPAND_LISTS
)

target_sources(app PRIVATE
    src/zephyr_start.c
    src/zephyr_getchar.c
)

target_link_libraries(app PRIVATE ${MICROPY_TARGET})
