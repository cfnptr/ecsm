// Copyright 2022-2026 Nikita Fediuchin. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/***********************************************************************************************************************
 * @file
 * @brief Common type string functions.
 */

#pragma once
#include <string>
#include <cstring>
#include <typeindex>

#ifdef __GNUG__
#include <cxxabi.h>
#endif

namespace ecsm
{

/**
 * @brief Returns @ref type_index string representation.
 * @param type target type
 */
static std::string typeToString(std::type_index type)
{
	auto name = type.name();

	#ifdef __GNUG__
	int status = -4;
	auto demangledName = abi::__cxa_demangle(name, nullptr, nullptr, &status);
	if (status == 0)
		name = demangledName;
	#endif

	std::string result;
	if (strlen(name) > 0)
		result.assign(name);
	else
		result = std::to_string(type.hash_code());

	#ifdef __GNUG__
	free(demangledName);
	#endif
	return result;
}
/**
 * @brief Returns type string representation.
 * @tparam T target type
 */
template<typename T>
static std::string typeToString()
{
	return typeToString(typeid(T));
}

}; // namespace ecsm