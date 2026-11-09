###
# If not stated otherwise in this file or this component's LICENSE
# file the following copyright and licenses apply:
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
###

cmake_minimum_required(VERSION 3.3)

find_package(WPEFramework REQUIRED)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/")

string(TOLOWER ${NAMESPACE} STORAGE_DIRECTORY)

include(CmakeHelperFunctions OPTIONAL)

if(RDK_SERVICES_L1_TEST)
    add_subdirectory(Tests/L1Tests)
endif()

option(PLUGIN_WEBREMOTE "Include WebRemote plugin" OFF)

if(PLUGIN_WEBREMOTE)
    add_subdirectory(plugin)
endif()
