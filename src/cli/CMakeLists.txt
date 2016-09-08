# (C) Copyright 2016 Hewlett Packard Enterprise Development LP
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

set (INCL_DIR ${CMAKE_SOURCE_DIR}/include)
set (LIBLACPCLI lacp_cli)

# This option is specified in Yocto -- openswitch.bbclass.
OPTION( CPU_LITTLE_ENDIAN "Specifies CPU architecture is Little-Endian" OFF )

# Define compile flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -Wall -ggdb -g3 -O0")

add_definitions(-DHAVE_CONFIG_H -DHAVE_SOCKLEN_T -DSYSCONFDIR=\"/etc/\")

# Rules to locate needed libraries
include(FindPkgConfig)
pkg_check_modules(OVSCOMMON REQUIRED libovscommon)
pkg_check_modules(OVSDB REQUIRED libovsdb)
pkg_check_modules(OPSUTILS REQUIRED opsutils)
pkg_check_modules(OPSCLI REQUIRED ops-cli)

include_directories (${INCL_DIR}
                     ${PROJECT_SOURCE_DIR}
                     ${OVSCOMMON_INCLUDE_DIRS}
                     ${OPSUTILS_INCLUDE_DIRS}
                     ${OPSCLI_INCLUDE_DIRS}
                     )

# CLI libraries source files
set (SOURCES_CLI ${PROJECT_SOURCE_DIR}/lacp_vty.c
                 ${PROJECT_SOURCE_DIR}/qos_lag.c
                 ${PROJECT_SOURCE_DIR}/mstp_lag.c
                 ${PROJECT_SOURCE_DIR}/vtysh_ovsdb_intf_lag_context.c)

add_library (${LIBLACPCLI} SHARED ${SOURCES_CLI})

# Installation
install(TARGETS ${LIBLACPCLI}
        LIBRARY DESTINATION lib/cli/plugins)
