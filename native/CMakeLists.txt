#
# Copyright The mod_cluster Project Authors
# SPDX-License-Identifier: Apache-2.0
#

# mod_cluster native parent project
# @author Michal Karm Babacek

CMAKE_MINIMUM_REQUIRED(VERSION 3.5)
PROJECT(mod_cluster LANGUAGES C)

IF(WIN32)
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /O2 /Wall /Z7")
    SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /Od /Wall /Zi")
ELSE()
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -fPIC -Wall -Wextra -Werror --coverage -fprofile-arcs -ftest-coverage -fPIC -g -O0 -lgcov")
    SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -p -std=gnu99 -fPIC -Wall -Wextra --coverage -fprofile-arcs -ftest-coverage -fPIC -O0 -lgcov")
ENDIF()

SET(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/modules)
SET(PROJECT_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)
SET(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules ${CMAKE_MODULE_PATH})

FIND_PACKAGE(APR REQUIRED)
FIND_PACKAGE(APACHE REQUIRED)

INCLUDE_DIRECTORIES(${APR_INCLUDE_DIR})
INCLUDE_DIRECTORIES(${APRUTIL_INCLUDE_DIR})
INCLUDE_DIRECTORIES(${APACHE_INCLUDE_DIR})

INCLUDE_DIRECTORIES("${PROJECT_INCLUDE_DIR}")
INCLUDE_DIRECTORIES("${PROJECT_SOURCE_DIR}")

ADD_LIBRARY(mod_proxy_cluster_common STATIC common/common.c)
TARGET_INCLUDE_DIRECTORIES(mod_proxy_cluster_common PUBLIC ${PROJECT_INCLUDE_DIR})

ADD_SUBDIRECTORY(mod_proxy_cluster)
ADD_SUBDIRECTORY(advertise)
ADD_SUBDIRECTORY(mod_manager)
ADD_SUBDIRECTORY(balancers)
