# (C) Copyright 2015-2016 Hewlett Packard Enterprise Development LP
#
#  Licensed under the Apache License, Version 2.0 (the "License"); you may
#  not use this file except in compliance with the License. You may obtain
#  a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
#  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
#  License for the specific language governing permissions and limitations
#  under the License.

cmake_minimum_required (VERSION 2.8)

set (OPSLACPD ops-lacpd)
project (${OPSLACPD})

set (SRC_DIR src)
set (INCL_DIR include)

# This option is specified in Yocto -- openswitch.bbclass.
OPTION( CPU_LITTLE_ENDIAN "Specifies CPU architecture is Little-Endian" OFF )
configure_file ("${PROJECT_SOURCE_DIR}/${INCL_DIR}/lacp.h.in"
	        "${PROJECT_BINARY_DIR}/${INCL_DIR}/lacp.h")

# Define compile flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -Wall -Werror -ggdb -g3 -O0")

# Rules to locate needed libraries
include(FindPkgConfig)
pkg_check_modules(OVSCOMMON REQUIRED libovscommon)
pkg_check_modules(OVSDB REQUIRED libovsdb)
pkg_check_modules(OVSDB REQUIRED opsutils)

include_directories (${PROJECT_BINARY_DIR}/${INCL_DIR}
                     ${PROJECT_SOURCE_DIR}/${INCL_DIR}
                     ${PROJECT_SOURCE_DIR}
                     ${OVSCOMMON_INCLUDE_DIRS})

# Source files to build ops-lacpd
set (SOURCES ${SRC_DIR}/avl.c ${SRC_DIR}/dlist.c ${SRC_DIR}/lacpd.c
             ${SRC_DIR}/lacp_support.c ${SRC_DIR}/lacp_task.c ${SRC_DIR}/mlacp_main.c
             ${SRC_DIR}/mlacp_recv.c ${SRC_DIR}/mlacp_send.c ${SRC_DIR}/mqueue.c
             ${SRC_DIR}/mux_fsm.c ${SRC_DIR}/mvlan_lacp.c ${SRC_DIR}/mvlan_sport.c
             ${SRC_DIR}/ovsdb_if.c ${SRC_DIR}/periodic_tx_fsm.c ${SRC_DIR}/receive_fsm.c
             ${SRC_DIR}/selection.c ${SRC_DIR}/stubs.c ${SRC_DIR}/utils.c)

# Rules to build ops-lacpd
add_executable (${OPSLACPD} ${SOURCES})

target_link_libraries (${OPSLACPD} ${OVSCOMMON_LIBRARIES} ${OVSDB_LIBRARIES}
                       ${OPSUTILS_LIBRARIES} -lsupportability
	               -lpthread -lrt)

add_subdirectory(src/cli)

# Rules to install ops-lacpd binary in rootfs
install(TARGETS ${OPSLACPD}
        RUNTIME DESTINATION bin)
