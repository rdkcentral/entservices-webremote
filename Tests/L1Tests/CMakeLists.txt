# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2026 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.8)

set(PLUGIN_NAME L1TestsWebRemote)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(${NAMESPACE}Plugins REQUIRED)

set(PLUGIN_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../plugin)

# Can be overridden with -DTESTFRAMEWORK_DIR=... when the testframework is cloned elsewhere.
if(NOT DEFINED TESTFRAMEWORK_DIR)
    set(TESTFRAMEWORK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../entservices-testframework)
endif()
set(HELPERS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../entservices-helpers)

add_library(${MODULE_NAME} SHARED
    tests/test_WebRemote.cpp)

target_include_directories(${MODULE_NAME}
    PRIVATE
        ${PLUGIN_SOURCE_DIR}
        ${CMAKE_INSTALL_PREFIX}/include/WPEFramework
        ${HELPERS_DIR}/helpers
        ${TESTFRAMEWORK_DIR}/Tests/mocks
        ${TESTFRAMEWORK_DIR}/Tests/mocks/thunder)

target_link_directories(${MODULE_NAME}
    PUBLIC
        ${CMAKE_INSTALL_PREFIX}/lib
        ${CMAKE_INSTALL_PREFIX}/lib/wpeframework/plugins)

target_link_libraries(${MODULE_NAME}
    PRIVATE
        ${NAMESPACE}Plugins::${NAMESPACE}Plugins
        ${NAMESPACE}WebRemote
        gmock
        gtest)

target_compile_definitions(${MODULE_NAME} PRIVATE UNIT_TEST)

install(TARGETS ${MODULE_NAME} DESTINATION lib)
