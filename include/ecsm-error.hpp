// Copyright 2021-2025 Nikita Fediuchin. All rights reserved.
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
 * @brief Common ECSM error (exception) functions.
 */

#pragma once
#include <string>

namespace ecsm
{

/**
 * @brief ECSM error (exception) class. 
 */
class EcsmError : public std::exception
{
	std::string message;
public:
	/**
	 * @brief Creates a new ECSM error (exception) instance. 
	 * @param message target error message
	 */
	EcsmError(const std::string& message) : message(message) { }

	/**
	 * @brief Returns ECSM error message C-string.
	 */
	const char* what() const noexcept override { return message.c_str(); }
};

} // mpio