// Copyright 2022-2025 Nikita Fediuchin. All rights reserved.
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
 * @brief Common singleton class functions.
 */

#pragma once
#include "ecsm-error.hpp"
#include "type-string.hpp"
#include <cassert>

namespace ecsm
{

void* getManagerSystem(std::type_index type);
bool hasManagerSystem(std::type_index type);
void* tryGetManagerSystem(std::type_index type);

/**
 * @brief Base singleton class.
 * 
 * @details
 * Singleton class is a class designed in such a way that only one instance of it can exist during the runtime of the 
 * game or application. This pattern is often used to manage systems or resources that need to be globally accessible, 
 * without having multiple copies floating around, which could lead to resource inefficiency or bugs.
 * 
 * @tparam T type of the singleton class
 * @tparam UseManager use manager system instance if singleton is not set
 */
template<class T, bool UseManager = true>
class Singleton
{
public:
	typedef T ItemType;                        /**< Type of the singleton class. */
	typedef Singleton<T, UseManager> Instance; /**< Singleton instance class type. */
protected:
	inline static T* singletonInstance = nullptr; /**< Singleton class instance. */

	/**
	 * @brief Creates a new singleton class instance.
	 * @param set is singleton instance should be set
	 */
	Singleton(bool set = true)
	{
		if (set)
			setSingleton();
	}

	/**
	 * @brief Sets a new class singleton instance.
	 */
	void setSingleton()
	{
		if (singletonInstance)
		{
			throw EcsmError("Singleton instance is already set. ("
				"type: " + typeToString(typeid(T)) + ")");
		}
		singletonInstance = (T*)this;
		return;
	}
	/**
	 * @brief Unsets this class singleton instance.
	 */
	void unsetSingleton() noexcept
	{
		if (singletonInstance != this)
			return;
		singletonInstance = nullptr;
	}
public:
	/**
	 * @brief Returns true if class singleton or manager instance is exist.
	 */
	static bool has()
	{
		if (singletonInstance)
			return true;
		if constexpr (UseManager)
			return hasManagerSystem(typeid(T));
		return false;
	}
	/**
	 * @brief Returns class singleton or manager instance.
	 */
	static T* get()
	{
		if (singletonInstance)
			return singletonInstance;
		if constexpr (UseManager)
			return (T*)getManagerSystem(typeid(T));
		throw EcsmError("Singleton instance is not set. ("
			"type: " + typeToString(typeid(T)) + ")");
	}
	/**
	 * @brief Returns class singleton or manager instance if exist.
	 */
	static T* tryGet()
	{
		if (singletonInstance)
			return singletonInstance;
		if constexpr (UseManager)
			return (T*)tryGetManagerSystem(typeid(T));
		return nullptr;
	}
};

} // namespace ecsm