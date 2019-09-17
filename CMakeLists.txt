# Copyright 2019 Liu Changcheng <changcheng.liu@aliyun.com>
# Author: Liu Changcheng <changcheng.liu@aliyun.com>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required (VERSION 3.10)
project (RDMA_MESSENGER)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_COMPILER g++)
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-std=c++11 -g -pthread")

include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/include)

set(RDMA_MESSENGER_TEST_DIR ${PROJECT_SOURCE_DIR}/test)
set(RDMA_MESSENGER_SRC_DIR ${PROJECT_SOURCE_DIR}/src)

message(${RDMA_MESSENGER_SRC_DIR})
message(${RDMA_MESSENGER_TEST_DIR})

add_executable(server ${RDMA_MESSENGER_TEST_DIR}/ping_pong/server.cc ${RDMA_MESSENGER_SRC_DIR}/core/RDMAStack.cc ${RDMA_MESSENGER_SRC_DIR}/core/RDMAConnection.cc ${RDMA_MESSENGER_SRC_DIR}/core/Acceptor.cc)
target_link_libraries(server rdmacm ibverbs)

add_executable(client ${RDMA_MESSENGER_TEST_DIR}/ping_pong/client.cc ${RDMA_MESSENGER_SRC_DIR}/core/RDMAStack.cc ${RDMA_MESSENGER_SRC_DIR}/core/RDMAConnection.cc ${RDMA_MESSENGER_SRC_DIR}/core/Connector.cc)
target_link_libraries(client rdmacm ibverbs)
