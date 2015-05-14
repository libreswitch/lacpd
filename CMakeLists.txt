# Copyright (C) 2015 Hewlett-Packard Development Company, L.P.
# All Rights Reserved.
#
#    Licensed under the Apache License, Version 2.0 (the "License"); you may
#    not use this file except in compliance with the License. You may obtain
#    a copy of the License at
#
#         http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
#    WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
#    License for the specific language governing permissions and limitations
#    under the License.

cmake_minimum_required (VERSION 2.8)

set (LACPD lacpd)
project (${LACPD})

set (LIB_CYCLONE cyclone)
set (LIB_CYCLONE_DIR ${LIB_CYCLONE})
set (LIB_VPM vpm)
set (LIB_VPM_DIR ${LIB_VPM})

include_directories (${LIB_CYCLONE_DIR}/include)
include_directories (${LIB_VPM_DIR})
include_directories (include)

# Rules to locate needed libraries
include(FindPkgConfig)
pkg_check_modules(SYSTEMD libsystemd)
pkg_check_modules(CONFIG_YAML config-yaml)
pkg_check_modules(HALONUTILS halonutils)
pkg_check_modules(OVSCOMMON libovscommon)
pkg_check_modules(OVSDB libovsdb)

include_directories (${PROJECT_BINARY_DIR} ${PROJECT_SOURCE_DIR}
                     ${OVSCOMMON_INCLUDE_DIRS}
)


# Source files to build lacpd
set (SOURCES lacpd.c mlacp_main.c mlacp_recv.c mlacp_send.c lacp_task.c
     lacp_support.c periodic_tx_fsm.c receive_fsm.c mux_fsm.c
     selection.c stubs.c ovsdb_if.c)

# Rules to build lacpd
add_executable (${LACPD} ${SOURCES})
add_subdirectory (${LIB_CYCLONE_DIR})
add_subdirectory (${LIB_VPM_DIR})

target_link_libraries (${LACPD} ${HALONUTILS_LIBRARIES} ${CONFIG_YAML_LIBRARIES} ${OVSCOMMON_LIBRARIES}
                       ${OVSDB_LIBRARIES} ${OPENSSL_LIBRARIES} ${SYSTEMD_LIBRARIES} 
                       ${LIB_CYCLONE} ${LIB_VPM} -lpthread -lrt)

# Add dependencies otherwise parallell building breaks
add_dependencies(${LACPD} ${LIB_CYCLONE} ${LIB_VPM})

# Rules to install lacpd binary in rootfs
install(TARGETS ${LACPD}
	RUNTIME DESTINATION bin)
