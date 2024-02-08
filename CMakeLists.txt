# Copyright 2022-2024 Nikita Fediuchin. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
	message(FATAL_ERROR "Prevented in-tree build")
endif()

if(TARGET ecsm)
	return()
endif()

project(ecsm VERSION 0.0.1 LANGUAGES CXX
	DESCRIPTION "Easy to use Entity Component System Manager C++ library"
	HOMEPAGE_URL "https://github.com/cfnptr/ecsm")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

option(ECSM_BUILD_TESTS "Build ECSM library tests" ON)

add_library(ecsm INTERFACE)
target_include_directories(ecsm INTERFACE ${PROJECT_SOURCE_DIR}/include)

if(ECSM_BUILD_TESTS)
	enable_testing()

	add_executable(TestEcsm tests/test-ecsm.cpp)
	target_link_libraries(TestEcsm PUBLIC ecsm)
	add_test(NAME TestEcsm COMMAND TestEcsm)
endif()