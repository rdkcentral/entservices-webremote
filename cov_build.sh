#!/bin/bash
#
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

set -x
set -e
##############################
GITHUB_WORKSPACE="${PWD}"
ls -la ${GITHUB_WORKSPACE}

############################
# Build entservices-webremote
echo "building entservices-webremote"

cd ${GITHUB_WORKSPACE}
cmake -G Ninja -S "$GITHUB_WORKSPACE" -B build/entservices-webremote \
-DUSE_THUNDER_R4=ON \
-DCMAKE_INSTALL_PREFIX="$GITHUB_WORKSPACE/install/usr" \
-DCMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
-DCMAKE_VERBOSE_MAKEFILE=ON \
-DCOMCAST_CONFIG=OFF \
-DRDK_SERVICES_COVERITY=ON \
-DRDK_SERVICES_L1_TEST=ON \
-DPLUGIN_WEBREMOTE=ON \
-DTESTFRAMEWORK_DIR="$GITHUB_WORKSPACE/entservices-testframework" \
-DCMAKE_CXX_FLAGS="-DEXCEPTIONS_ENABLE=ON \
--coverage -Wall -Werror -Wno-error=format \
-DTHUNDER_VERSION=4 -DTHUNDER_VERSION_MAJOR=4 -DTHUNDER_VERSION_MINOR=4"

cmake --build build/entservices-webremote --target install
echo "======================================================================================"

ls -la ${GITHUB_WORKSPACE}
exit 0
