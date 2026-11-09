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
CPC_APIS_TAG="4.3.1"
TESTFRAMEWORK_TAG="2.0.5"
ls -la ${GITHUB_WORKSPACE}
cd ${GITHUB_WORKSPACE}

############################
# 1. Install dependencies and packages
apt update
apt install -y valgrind lcov clang libsystemd-dev meson curl libunwind-dev libcunit1 libcunit1-dev libcurl4-openssl-dev
pip install jsonref

############################
# Build trower-base64 (needed by entservices-helpers)
if [ ! -d "trower-base64" ]; then
git clone https://github.com/xmidt-org/trower-base64.git
fi
cd trower-base64
meson setup --warnlevel 3 --werror build
ninja -C build
ninja -C build install
cd ..

###########################################
# Clone the required repositories
git clone --branch R4_4-RDK https://github.com/rdkcentral/ThunderTools.git
git clone --branch R4_4-RDK https://github.com/rdkcentral/Thunder.git
git clone --branch develop https://github.com/rdkcentral/entservices-apis.git

# ILinchpinClientProxy is published by the private cpc-apis repository.
git clone --branch develop "https://x-access-token:${RDKE_GITHUB_TOKEN}@github.com/rdk-e/entservices-cpc-apis.git"
git -C entservices-cpc-apis checkout "refs/tags/${CPC_APIS_TAG}"

# entservices-helpers is expected next to the workspace
cd ..
git clone --branch develop https://github.com/rdkcentral/entservices-helpers.git
cd "$GITHUB_WORKSPACE"

git clone --branch ${TESTFRAMEWORK_TAG} https://github.com/rdkcentral/entservices-testframework.git

############################
# Build Thunder-Tools
echo "============================================================================="
echo "building thunderTools"

cmake -G Ninja -S ThunderTools -B build/ThunderTools \
    -DEXCEPTIONS_ENABLE=ON \
    -DCMAKE_INSTALL_PREFIX="$GITHUB_WORKSPACE/install/usr" \
    -DCMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
    -DGENERIC_CMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake"

cmake --build build/ThunderTools --target install

############################
# Build Thunder
echo "============================================================================="
echo "building thunder"

cmake -G Ninja -S Thunder -B build/Thunder \
    -DMESSAGING=ON \
    -DCMAKE_INSTALL_PREFIX="$GITHUB_WORKSPACE/install/usr" \
    -DCMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
    -DGENERIC_CMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
    -DBUILD_TYPE=Debug \
    -DBINDING=127.0.0.1 \
    -DPORT=55555 \
    -DEXCEPTIONS_ENABLE=ON

cmake --build build/Thunder --target install

############################
# Build entservices-cpc-apis
echo "============================================================================="
echo "building entservices-cpc-apis"

cmake -G Ninja -S entservices-cpc-apis -B build/entservices-cpc-apis \
    -DEXCEPTIONS_ENABLE=ON \
    -DCMAKE_INSTALL_PREFIX="$GITHUB_WORKSPACE/install/usr" \
    -DCMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake"

cmake --build build/entservices-cpc-apis --target install

############################
# Build entservices-apis
echo "============================================================================="
echo "building entservices-apis"
cd entservices-apis
rm -rf jsonrpc/DTV.json
if [ ! -f "apis/AuthService/IAuthService.h" ]; then
    patch -p1 < $GITHUB_WORKSPACE/entservices-testframework/patches/RDKEMW-1007.patch
fi
cd ..

cmake -G Ninja -S entservices-apis -B build/entservices-apis \
    -DEXCEPTIONS_ENABLE=ON \
    -DCMAKE_INSTALL_PREFIX="$GITHUB_WORKSPACE/install/usr" \
    -DCMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake"

cmake --build build/entservices-apis --target install

############################
# generating minimal mock headers
cd $GITHUB_WORKSPACE/entservices-testframework/Tests
mkdir -p headers/rdk/iarmbus
cd headers
touch secure_wrapper.h
touch wpa_ctrl.h
touch rdk_logger_milestone.h
touch rdk/iarmbus/libIARM.h
touch rdk/iarmbus/libIBus.h
touch iarm.h
cd $GITHUB_WORKSPACE

#########################################
# Build entservices-helpers
echo "============================================================================="
echo "building entservices-helpers"
cmake -G Ninja -S ../entservices-helpers -B build/entservices-helpers \
    -DEXCEPTIONS_ENABLE=ON \
    -DCMAKE_INSTALL_PREFIX="$GITHUB_WORKSPACE/install/usr" \
    -DCMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
    -DUSE_THUNDER_R4=ON \
    -DHIDE_NON_EXTERNAL_SYMBOLS=OFF \
    -DPLUGIN_HELPERS=ON \
    "-DCMAKE_CXX_FLAGS=-I$GITHUB_WORKSPACE/entservices-testframework/Tests/mocks -I$GITHUB_WORKSPACE/entservices-testframework/Tests/headers -I$GITHUB_WORKSPACE/entservices-testframework/Tests/headers/rdk/iarmbus -include $GITHUB_WORKSPACE/entservices-testframework/Tests/mocks/Iarm.h "

cmake --build build/entservices-helpers --target install

ls -la ${GITHUB_WORKSPACE}
