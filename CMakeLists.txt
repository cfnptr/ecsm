# Copyright 2022-2026 Nikita Fediuchin. All rights reserved.
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

if(TARGET ecsm-static)
	return()
endif()

project(ecsm VERSION 0.6.0 LANGUAGES CXX
	DESCRIPTION "Easy to use Entity Component System Manager C++ library"
	HOMEPAGE_URL "https://github.com/cfnptr/ecsm")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

option(ECSM_BUILD_SHARED "Build ECSM shared library" ON)
option(ECSM_BUILD_TESTS "Build ECSM library tests" ON)

add_subdirectory(libraries/robin-map)

add_library(ecsm-static STATIC source/ecsm.cpp "source/singleton.cpp")
target_link_libraries(ecsm-static PUBLIC tsl::robin_map)
target_include_directories(ecsm-static PUBLIC ${PROJECT_SOURCE_DIR}/include)

if(ECSM_BUILD_SHARED)
	add_library(ecsm-shared SHARED source/ecsm.cpp source/singleton.cpp)
	set_target_properties(ecsm-shared PROPERTIES
		OUTPUT_NAME "ecsm" WINDOWS_EXPORT_ALL_SYMBOLS ON)
	target_link_libraries(ecsm-shared PUBLIC tsl::robin_map)
	target_include_directories(ecsm-shared PUBLIC ${PROJECT_SOURCE_DIR}/include)
endif()

if(ECSM_BUILD_TESTS)
	enable_testing()

	add_executable(TestEcsm tests/test-ecsm.cpp)
	target_link_libraries(TestEcsm PUBLIC ecsm-static)
	add_test(NAME TestEcsm COMMAND TestEcsm)
endif()
