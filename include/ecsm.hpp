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
 * @brief Entity Component System Manager classes.
 */

#pragma once
#include "singleton.hpp"
#include "linear-pool.hpp"
#include "tsl/robin_map.h"

#include <cassert>
#include <cstdint>
#include <set>
#include <mutex>
#include <functional>
#include <string_view>
#include <type_traits>

namespace ecsm
{

class Entity;
struct Component;
class System;
class Manager;
class SystemExt;

/**
 * @brief String view heterogeneous hash functions.
 */
struct SvHash
{
	using is_transparent = void;
	std::size_t operator()(std::string_view sv) const { return std::hash<std::string_view>{}(sv); }
	std::size_t operator()(const std::string& str) const { return std::hash<std::string>{}(str); }
};
/**
 * @brief String view heterogeneous equal functions.
 */
struct SvEqual
{
	using is_transparent = void;
	bool operator()(std::string_view lhs, std::string_view rhs) const noexcept { return lhs == rhs; }
};

/**
 * @brief Subscribes @ref System function to the event.
 */
#define ECSM_SUBSCRIBE_TO_EVENT(name, func) \
	ecsm::Manager::Instance::get()->subscribeToEvent(name, std::bind(&func, this))
/**
 * @brief Unsubscribes @ref System function from the event.
 */
#define ECSM_UNSUBSCRIBE_FROM_EVENT(name, func) \
	ecsm::Manager::Instance::get()->unsubscribeFromEvent(name, std::bind(&func, this))

/**
 * @brief Subscribes @ref System function to the event if exist.
 */
#define ECSM_TRY_SUBSCRIBE_TO_EVENT(name, func) \
	ecsm::Manager::Instance::get()->trySubscribeToEvent(name, std::bind(&func, this))
/**
 * @brief Unsubscribes @ref System function from the event if exist.
 */
#define ECSM_TRY_UNSUBSCRIBE_FROM_EVENT(name, func) \
	ecsm::Manager::Instance::get()->tryUnsubscribeFromEvent(name, std::bind(&func, this))

/***********************************************************************************************************************
 * @brief Base component structure.
 * 
 * @details
 * Components are containers for specific data or behavior. Rather than attaching behavior directly to 
 * entities, you attach components to entities to give them certain properties or functionalities. 
 * For example, in a game, you might have components for position, velocity, rendering, etc.
*/
struct Component
{
protected:
	ID<Entity> entity = {};
	friend class Manager;
public:
	/**
	 * @brief Returns component entity instance.
	 */
	ID<Entity> getEntity() const noexcept { return entity; }
};

/***********************************************************************************************************************
 * @brief Base system class.
 * 
 * @details
 * Systems are responsible for updating and processing entities with specific components. Each system 
 * typically focuses on a particular aspect of the game or simulation. Systems iterate over entities 
 * that have the necessary components and perform the required actions or calculations.
 */
class System
{
protected:
	/**
	 * @brief Destroys system instance.
	 * @warning Override it to destroy unmanaged resources.
	 */
	virtual ~System() = default;

	/**
	 * @brief Creates a new system component instance for the entity.
	 * @details You should use @ref Manager to add components to the entity.
	 * @note Override it to define a custom component of the system.
	 * @param entity target entity instance
	 */
	virtual ID<Component> createComponent(ID<Entity> entity);
	/**
	 * @brief Destroys system component instance.
	 * @details You should use @ref Manager to remove components from the entity.
	 * @note Override it to define a custom component of the system.
	 * @param instance target component instance
	 */
	virtual void destroyComponent(ID<Component> instance);
	/**
	 * @brief Resets system component data.
	 * @details You should use @ref Manager to reset entity components.
	 * @note Override it to define a custom component of the system.
	 *
	 * @param component target component view
	 * @param full reset all component data
	 */
	virtual void resetComponent(View<Component> component, bool full);
	/**
	 * @brief Copies system component data from source to destination.
	 * @details You should use @ref Manager to copy component data of entities.
	 * @note Override it to define a custom component of the system.
	 *
	 * @param source source component view (from)
	 * @param destination destination component view (to)
	 */
	virtual void copyComponent(View<Component> source, View<Component> destination);

	friend class Entity;
	friend class Manager;
	friend class SystemExt;
public:
	/**
	 * @brief Returns specific component name of the system.
	 * @note Override it to define a custom component of the system.
	 */
	virtual std::string_view getComponentName() const;
	/**
	 * @brief Returns specific component typeid() of the system.
	 * @note Override it to define a custom component of the system.
	 */
	virtual std::type_index getComponentType() const;
	/**
	 * @brief Returns specific system component @ref View.
	 * @note Override it to define a custom component of the system.
	 * @param instance target component instance
	 */
	virtual View<Component> getComponent(ID<Component> instance);
	/**
	 * @brief Actually destroys system components.
	 * @details Components are not destroyed immediately, only after the dispose call.
	 */
	virtual void disposeComponents();
};

/***********************************************************************************************************************
 * @brief An object containing components.
 * 
 * @details
 * The entity is a general-purpose object. It doesn't have any inherent behavior 
 * or data by itself. Instead, it serves as a container for components.
 */
class Entity final
{
public:
	/**
	 * @brief Entity component data container.
	 */
	struct ComponentData final
	{
		size_t type = 0;
		System* system = nullptr;
		ID<Component> instance = {};
	};
private:
	ComponentData* components = nullptr;
	uint32_t capacity = 0;
	uint32_t count = 0;

	inline static int compareComps(const void* a, const void* b) noexcept
	{
		const auto l = (const ComponentData*)a;
		const auto r = (const ComponentData*)b;
		if (l->type < r->type) return -1;
		if (l->type > r->type) return 1;
		return 0;
	}

	void reserve(uint32_t capacity)
	{
		this->capacity = capacity;
		if (this->capacity == 0)
			components = (ComponentData*)malloc(sizeof(ComponentData) * capacity);
		else
			components = (ComponentData*)realloc(components, sizeof(ComponentData) * capacity);
		if (!components) abort();
	}
	void addComponent(size_t type, System* system, ID<Component> instance)
	{
		if (count == capacity)
		{
			if (capacity == 0)
			{
				capacity = 1;
				components = (ComponentData*)malloc(sizeof(ComponentData));
			}
			else
			{
				capacity *= 2;
				components = (ComponentData*)realloc(components, sizeof(ComponentData) * capacity);
			}
			if (!components) abort();
		}
		components[count++] = { type, system, instance };
		qsort(components, count, sizeof(ComponentData), compareComps);
	}
	void removeComponent(const ComponentData* data)
	{
		for (uint32_t i = (uint32_t)(data - components), c = --count; i < c; i++)
			components[i] = components[i + 1];
	}
	
	friend class Manager;
public:
	/*******************************************************************************************************************
	 * @brief Destroys all entity components.
	 * @note Components are not destroyed immediately, only after the dispose call.
	 */
	bool destroy();

	/**
	 * @brief Returns entity components array.
	 * @details Use @ref Manager to add or remove components.
	 */
	const ComponentData* getComponents() const noexcept { return components; }
	/**
	 * @brief Returns entity components array size.
	 * @details Use @ref Manager to add or remove components.
	 */
	uint32_t getComponentCount() const noexcept { return count; }
	/**
	 * @brief Returns entity components array capacity.
	 * @details Use @ref Manager to add or remove components.
	 */
	uint32_t getComponentCapacity() const noexcept { return capacity; }

	/**
	 * @brief Returns true if entity has components.
	 */
	bool hasComponents() const noexcept { return count; }
	/**
	 * @brief Searches for the specified component type.
	 * @param type target component type hash code
	 * @return Component data on success, otherwise null.
	 */
	const ComponentData* findComponent(size_t type) const noexcept
	{
		ComponentData key = { type };
		return (const ComponentData*)bsearch(&key, components, count, sizeof(ComponentData), compareComps);
	}
};

/***********************************************************************************************************************
 * @brief Systems and entities coordinator.
 * 
 * @details
 * Manager serves as a central coordinating object responsible for overseeing various aspects of the system. 
 * It manages entity-related tasks such as creation, destruction, and component assignment.
 * Additionally, it handles the storage and retrieval of components, facilitates system initialization, 
 * configuration and supports event handling for communication and reaction to changes.
 * 
 * - PreInit event or phase is the earliest stage in the initialization process. It occurs before 
 *     most of the system's or component's initialization logic runs. This phase is typically used 
 *     for preliminary setup tasks that need to occur before the main initialization.
 * - Init event or phase is the main stage of initialization. During this phase, components and 
 *     systems perform their core setup tasks. This includes initializing internal data structures, 
 *     loading resources, or setting up dependencies with other components or systems.
 * - PostInit event or phase happens after the main initialization logic. This stage is 
 *     used for tasks that must occur after all components and systems have been initialized. 
 *     It's particularly useful for setup steps that require all other components to 
 *     be in a ready state or for cross-component communications and linking.
 */
class Manager final : public Singleton<Manager, false>
{
public:
	/**
	 * @brief Event data container.
	 */
	struct Event final
	{
		using Subscribers = std::vector<std::function<void()>>;

		std::string name;
		Subscribers subscribers;
		bool isOrdered = false;

		Event(std::string_view name, bool isOrdered = true) : 
			name(name), isOrdered(isOrdered) { }

		/**
		 * @brief Returns true if this event has subscribers. 
		 */
		bool hasSubscribers() const noexcept { return !subscribers.empty(); }
		/**
		 * @brief Calls all event subscribers.
		 */
		void run() const
		{
			for (const auto& onEvent : subscribers)
				onEvent();
		}
	};

	using Systems = tsl::robin_map<std::type_index, System*>;
	using SystemGroups = tsl::robin_map<std::type_index, std::vector<System*>>;
	using ComponentTypes = tsl::robin_map<std::type_index, System*>;
	using ComponentNames = tsl::robin_map<std::string, System*, SvHash, SvEqual>;
	using Events = tsl::robin_map<std::string, Event*, SvHash, SvEqual>;
	using OrderedEvents = std::vector<const Event*>;
	using EntityPool = LinearPool<Entity>;
	using GarbageComponent = std::pair<size_t, ID<Entity>>;
	using GarbageComponents = std::set<GarbageComponent>;
private:
	Systems systems;
	SystemGroups systemGroups;
	ComponentTypes componentTypes;
	ComponentNames componentNames;
	EntityPool entities;
	Events events;
	OrderedEvents orderedEvents;
	GarbageComponents garbageComponents;
	std::mutex locker;
	bool initialized = false;

	#ifndef NDEBUG
	bool isChanging = false;
	#endif

	void addSystem(System* system, std::type_index type);
public:
	bool isRunning = false;

	/**
	 * @brief Creates a new manager instance.
	 * @param setSingleton set manager singleton instance
	 */
	Manager(bool setSingleton = true);
	/**
	 * @brief Destroys manager and all components/entities/systems.
	 */
	~Manager();

	/*******************************************************************************************************************
	 * @brief Creates a new system instance.
	 * 
	 * @details
	 * Instantiates a new system and registers it component, 
	 * but initialization occurs only after the @ref initialize() call.
	 * 
	 * @tparam T target system type
	 * @tparam Args additional argument types
	 * @param args additional system creation arguments
	 * 
	 * @throw EcsmError if system is already created or component type registered.
	 */
	template<class T, typename... Args>
	void createSystem(Args&&... args)
	{
		static_assert(std::is_base_of_v<System, T>, "Must be derived from the System class.");
		#ifndef NDEBUG
		if (isChanging)
			throw EcsmError("Creation of the system inside other create/destroy is not allowed.");
		isChanging = true;
		#endif

		auto system = new T(std::forward<Args>(args)...);
		addSystem(system, typeid(T));

		#ifndef NDEBUG
		isChanging = false;
		#endif
	}

	/**
	 * @brief Terminates and destroys system.
	 * @param type target system typeid()
	 * @throw EcsmError if system is not found.
	 */
	void destroySystem(std::type_index type);
	/**
	 * @brief Terminates and destroys system.
	 * @tparam T target system type
	 * @throw EcsmError if system is not found.
	 */
	template<class T>
	void destroySystem()
	{
		static_assert(std::is_base_of_v<System, T>, "Must be derived from the System class.");
		destroySystem(typeid(T));
	}

	/**
	 * @brief Terminates and destroys system if exist.
	 * @param type target system typeid()
	 * @return True if system is destroyed, otherwise false.
	 */
	bool tryDestroySystem(std::type_index type);
	/**
	 * @brief Terminates and destroys system if exist.
	 * @tparam T target system type
	 * @return True if system is destroyed, otherwise false.
	 */
	template<class T>
	bool tryDestroySystem()
	{
		static_assert(std::is_base_of_v<System, T>, "Must be derived from the System class.");
		return tryDestroySystem(typeid(T));
	}

	/*******************************************************************************************************************
	 * @brief Adds specified system to the system group.
	 * 
	 * @param groupType typeid() of the system group
	 * @param[in] system target system instance
	 *
	 * @throw EcsmError if system is already added.
	 */
	void addGroupSystem(std::type_index groupType, System* system);
	/**
	 * @brief Adds specified system to the system group.
	 * 
	 * @tparam T type of the system group
	 * @param[in] system target system instance
	 *
	 * @throw EcsmError if system is already added.
	 */
	template<class T>
	void addGroupSystem(System* system)
	{
		assert(dynamic_cast<T*>(system));
		return addGroupSystem(typeid(T), system);
	}
	/**
	 * @brief Adds specified system to the system group.
	 * 
	 * @tparam G type of the system group
	 * @tparam T target system type
	 *
	 * @throw EcsmError if system is not found or already added.
	 */
	template<class G, class S>
	void addGroupSystem()
	{
		static_assert(std::is_base_of_v<System, S>, "Must be derived from the System class.");
		static_assert(std::is_base_of_v<G, S>, "System must be castable to the group class.");
		return addGroupSystem(typeid(G), get<S>());
	}

	/**
	 * @brief Adds specified system to the system group if exist.
	 * 
	 * @param groupType typeid() of the system group
	 * @param[in] system target system instance
	 *
	 * @return True if system is added to the group, otherwise false.
	 */
	bool tryAddGroupSystem(std::type_index groupType, System* system);
	/**
	 * @brief Adds specified system to the system group if exist.
	 * 
	 * @tparam T type of the system group
	 * @param[in] system target system instance
	 *
	 * @return True if system is added to the group, otherwise false.
	 */
	template<class T>
	bool tryAddGroupSystem(System* system)
	{
		assert(dynamic_cast<T*>(system));
		return tryAddGroupSystem(typeid(T), system);
	}
	/**
	 * @brief Adds specified system to the system group if exist.
	 * 
	 * @tparam G type of the system group
	 * @tparam T target system type
	 *
	 * @return True if system is added to the group, otherwise false.
	 */
	template<class G, class S>
	bool tryAddGroupSystem()
	{
		static_assert(std::is_base_of_v<System, S>, "Must be derived from the System class.");
		static_assert(std::is_base_of_v<G, S>, "System must be castable to the group class.");
		auto system = tryGet<S>();
		if (!system)
			return false;
		return tryAddGroupSystem(typeid(G), system);
	}

	/*******************************************************************************************************************
	 * @brief Removes specified system from the system group.
	 * 
	 * @param groupType typeid() of the system group
	 * @param[in] system target system instance
	 *
	 * @throw EcsmError if system group is not found, or system is not added.
	 */
	void removeGroupSystem(std::type_index groupType, System* system);
	/**
	 * @brief Removes specified system from the system group.
	 * 
	 * @tparam T type of the system group
	 * @param[in] system target system instance
	 *
	 * @throw EcsmError if system group is not found, or system is not added.
	 */
	template<class T>
	void removeGroupSystem(System* system)
	{
		assert(dynamic_cast<T*>(system));
		return removeGroupSystem(typeid(T), system);
	}
	/**
	 * @brief Removes specified system from the system group.
	 * 
	 * @tparam G type of the system group
	 * @tparam T target system type
	 *
	 * @throw EcsmError if system or group is not found, or system is not added.
	 */
	template<class G, class S>
	void removeGroupSystem()
	{
		static_assert(std::is_base_of_v<System, S>, "Must be derived from the System class.");
		static_assert(std::is_base_of_v<G, S>, "System must be castable to the group class.");
		return removeGroupSystem(typeid(G), get<S>());
	}

	/**
	 * @brief Removes specified system from the system group if exist.
	 * 
	 * @param groupType typeid() of the system group
	 * @param[in] system target system instance
	 *
	 * @return True if system is removed from the group, otherwise false.
	 */
	bool tryRemoveGroupSystem(std::type_index groupType, System* system);
	/**
	 * @brief Removes specified system from the system group if exist.
	 * 
	 * @tparam T type of the system group
	 * @param[in] system target system instance
	 *
	 * @return True if system is removed from the group, otherwise false.
	 */
	template<class T>
	bool tryRemoveGroupSystem(System* system)
	{
		assert(dynamic_cast<T*>(system));
		return tryRemoveGroupSystem(typeid(T), system);
	}
	/**
	 * @brief Removes specified system from the system group if exist.
	 * 
	 * @tparam G type of the system group
	 * @tparam T target system type
	 *
	 * @return True if system is removed from the group, otherwise false.
	 */
	template<class G, class S>
	bool tryRemoveGroupSystem()
	{
		static_assert(std::is_base_of_v<System, S>, "Must be derived from the System class.");
		static_assert(std::is_base_of_v<G, S>, "System must be castable to the group class.");
		auto system = tryGet<S>();
		if (!system)
			return false;
		return tryRemoveGroupSystem(typeid(G), system);
	}

	/*******************************************************************************************************************
	 * @brief Returns true if system group is exist.
	 * @param type target system group typeid()
	 */
	bool hasSystemGroup(std::type_index type) const noexcept { return systemGroups.find(type) != systemGroups.end(); }
	/**
	 * @brief Returns true if system group is exist.
	 * @tparam T target system group type
	 */
	template<class T>
	bool hasSystemGroup() const noexcept { return has(typeid(T)); }

	/**
	 * @brief Returns specified system group.
	 * @param type target system group typeid()
	 * @throw EcsmError if system group is not found.
	 */
	const std::vector<System*>& getSystemGroup(std::type_index type) const
	{
		auto result = systemGroups.find(type);
		if (result == systemGroups.end())
			throw EcsmError("System group is not registered. (type: " + typeToString(type) + ")");
		return result->second;
	}
	/**
	 * @brief Returns specified system group.
	 * @tparam T target system group type
	 * @throw EcsmError if system group is not found.
	 */
	template<class T>
	const std::vector<System*>& getSystemGroup() const { return getSystemGroup(typeid(T)); }

	/**
	 * @brief Returns specified system group if exist, otherwise nullptr.
	 * @param type target system group typeid()
	 */
	const std::vector<System*>* tryGetSystemGroup(std::type_index type) const noexcept
	{
		auto result = systemGroups.find(type);
		if (result == systemGroups.end())
			return nullptr;
		return &result->second;
	}
	/**
	 * @brief Returns specified system group if exist, otherwise nullptr.
	 * @tparam T target system group type
	 */
	template<class T>
	const std::vector<System*>* tryGetSystemGroup() const noexcept { return tryGetSystemGroup(typeid(T)); }

	/*******************************************************************************************************************
	 * @brief Returns true if system is created.
	 * @param type target system typeid()
	 */
	bool has(std::type_index type) const noexcept { return systems.find(type) != systems.end(); }
	/**
	 * @brief Returns true if system is created.
	 * @tparam T target system type
	 */
	template<class T>
	bool has() const noexcept
	{
		static_assert(std::is_base_of_v<System, T>, "Must be derived from the System class.");
		return has(typeid(T));
	}

	/**
	 * @brief Returns system instance.
	 * @warning Be carefull with system pointer, it can be destroyed later.
	 * @param type target system typeid()
	 * @throw EcsmError if system is not found.
	 */
	System* get(std::type_index type) const
	{
		auto result = systems.find(type);
		if (result == systems.end())
			throw EcsmError("System is not created. (type: " + typeToString(type) + ")");
		return result->second;
	}
	/**
	 * @brief Returns system instance.
	 * @tparam T target system type
	 * @throw EcsmError if system is not found.
	 */
	template<class T>
	T* get() const
	{
		static_assert(std::is_base_of_v<System, T>, "Must be derived from the System class.");
		return (T*)get(typeid(T));
	}

	/**
	 * @brief Returns system instance if created, otherwise nullptr.
	 * @param type target system typeid()
	 */
	System* tryGet(std::type_index type) const noexcept
	{
		auto result = systems.find(type);
		return result == systems.end() ? nullptr : result->second;
	}
	/**
	 * @brief Returns system instance if created, otherwise nullptr.
	 * @tparam T target system type
	 */
	template<class T>
	T* tryGet() const noexcept
	{
		static_assert(std::is_base_of_v<System, T>, "Must be derived from the System class.");
		return (T*)tryGet(typeid(T));
	}

	/*******************************************************************************************************************
	 * @brief Creates a new entity instance.
	 * @details Created entity does not have any default component.
	 */
	ID<Entity> createEntity() { return entities.create(); }
	/**
	 * @brief Destroys entity instance and it components.
	 * @note Entities are not destroyed immediately, only after the dispose call.
	 * @param instance target entity instance or null
	 */
	void destroy(ID<Entity> instance) { entities.destroy(instance); }
	/**
	 * @brief Returns entity internal data accessor. (@ref View)
	 * @warning Do not store views, use them only in place. Because entity memory can be reallocated later.
	 * @param instance target entity instance
	 */
	View<Entity> get(ID<Entity> instance) const noexcept { return entities.get(instance); }
	
	/*******************************************************************************************************************
	 * @brief Adds a new component to the entity.
	 * @details See the @ref add<T>(ID<Entity> entity).
	 * @warning Do not store views, use them only in place. Because component memory can be reallocated later.
	 * 
	 * @param entity entity instance
	 * @param componentType target component typeid()
	 * 
	 * @return Returns @ref View of the created component.
	 * @throw EcsmError if component type is not registered, or component is already added.
	 */
	View<Component> add(ID<Entity> entity, std::type_index componentType);

	/**
	 * @brief Adds a new component to the entity.
	 *
	 * @details
	 * Only one component type can be attached to the entity, but several components 
	 * with different types. Target component should be registered by some @ref System.
	 * 
	 * @warning Do not store views, use them only in place. Because component memory can be reallocated later.
	 *
	 * @param entity entity instance
	 * @tparam T target component type
	 * 
	 * @return Returns @ref View of the created component.
	 * @throw EcsmError if component type is not registered, or component is already added.
	 */
	template<class T>
	View<T> add(ID<Entity> entity)
	{
		static_assert(std::is_base_of_v<Component, T>, "Must be derived from the Component struct.");
		return View<T>(add(entity, typeid(T)));
	}

	/**
	 * @brief Removes component from the entity.
	 * @details See the @ref remove<T>(ID<Entity> entity).
	 * @note Components are not destroyed immediately, only after the dispose call.
	 *
	 * @param entity entity instance
	 * @param componentType target component typeid()
	 * 
	 * @throw EcsmError if component is not found.
	 */
	void remove(ID<Entity> entity, std::type_index componentType);
	/**
	 * @brief Removes component from the entity.
	 * @details Component data destruction is handled by the @ref System.
	 * @note Components are not destroyed immediately, only after the dispose call.
	 *
	 * @param entity entity instance
	 * @tparam T target component type
	 * 
	 * @throw EcsmError if component is not found.
	 */
	template<class T>
	void remove(ID<Entity> entity)
	{ 
		static_assert(std::is_base_of_v<Component, T>, "Must be derived from the Component struct.");
		remove(entity, typeid(T));
	}

	/**
	 * @brief Returns true if target entity component was removed and is in the garbage pool.
	 * @details See the @ref remove<T>(ID<Entity> entity).
	 * @note Components are not destroyed immediately, only after the dispose call.
	 *
	 * @param entity entity instance
	 * @param componentType target component typeid()
	 */
	bool isGarbage(ID<Entity> entity, std::type_index componentType) const noexcept
	{
		return garbageComponents.find(std::make_pair(componentType.hash_code(), entity)) != garbageComponents.end();
	}
	/**
	 * @brief Returns true if target entity component was removed and is in the garbage pool.
	 * @details See the @ref remove<T>(ID<Entity> entity).
	 * @note Components are not destroyed immediately, only after the dispose call.
	 *
	 * @param entity entity instance
	 * @tparam T target component type
	 */
	template<class T>
	bool isGarbage(ID<Entity> entity) const noexcept
	{
		static_assert(std::is_base_of_v<Component, T>, "Must be derived from the Component struct.");
		return isGarbage(entity, typeid(T));
	}

	/*******************************************************************************************************************
	 * @brief Copies component data from source entity to destination.
	 * @details See the @ref copy<T>(ID<Entity> source, ID<Entity> destination).
	 *
	 * @param source copy from entity instance
	 * @param destination copy to entity instance
	 * @param componentType target component typeid()
	 *
	 * @throw EcsmError if source or destination component is not found.
	 */
	void copy(ID<Entity> source, ID<Entity> destination, std::type_index componentType);
	/**
	 * @brief Copies component data from source entity to destination.
	 * @details Component data copying is handled by the @ref System.
	 *
	 * @param source copy from entity instance
	 * @param destination copy to entity instance
	 * @tparam T target component type
	 *
	 * @throw EcsmError if source or destination component is not found.
	 */
	template<class T>
	void copy(ID<Entity> source, ID<Entity> destination)
	{
		static_assert(std::is_base_of_v<Component, T>, "Must be derived from the Component struct.");
		copy(source, destination, typeid(T));
	}

	/**
	 * @brief Creates a duplicate of specified entity.
	 * @details Component data copying is handled by the @ref System.
	 * @param entity target entity instance to duplicate from
	 */
	ID<Entity> duplicate(ID<Entity> entity);

	/*******************************************************************************************************************
	 * @brief Returns true if entity has target component.
	 * @note It also checks for component in the garbage pool.
	 *
	 * @param entity entity instance
	 * @param componentType target component typeid()
	 */
	bool has(ID<Entity> entity, std::type_index componentType) const noexcept
	{
		auto componentData = entities.get(entity)->findComponent(componentType.hash_code());
		GarbageComponent garbagePair = std::make_pair(componentType.hash_code(), entity);
		return componentData && garbageComponents.find(garbagePair) == garbageComponents.end();
	}
	/**
	 * @brief Returns true if entity has target component.
	 * @note It also checks for component in the garbage pool.
	 *
	 * @param entity entity instance
	 * @tparam T target component type
	 */
	template<class T>
	bool has(ID<Entity> entity) const noexcept
	{
		static_assert(std::is_base_of_v<Component, T>, "Must be derived from the Component struct.");
		return has(entity, typeid(T));
	}

	/**
	 * @brief Returns component data accessor. (@ref View)
	 * @warning Do not store views, use them only in place. Because component memory can be reallocated later.
	 * 
	 * @param entity entity instance
	 * @param componentType target component typeid()
	 * 
	 * @throw EcsmError if component is not found.
	 */
	View<Component> get(ID<Entity> entity, std::type_index componentType) const
	{
		auto componentData = entities.get(entity)->findComponent(componentType.hash_code());
		if (!componentData)
		{
			throw EcsmError("Component is not added. ("
				"type: " + typeToString(componentType) + ", "
				"entity:" + std::to_string(*entity) + ")");
		}
		return componentData->system->getComponent(componentData->instance);
	}
	/**
	 * @brief Returns component data accessor. (@ref View)
	 * @warning Do not store views, use them only in place. Because component memory can be reallocated later.
	 * 
	 * @param entity entity instance
	 * @tparam T target component type
	 * 
	 * @throw EcsmError if component is not found.
	 */
	template<class T>
	View<T> get(ID<Entity> entity) const
	{
		static_assert(std::is_base_of_v<Component, T>, "Must be derived from the Component struct.");
		return View<T>(get(entity, typeid(T)));
	}

	/**
	 * @brief Returns component data accessor if exist, otherwise null. (@ref View)
	 * @warning Do not store views, use them only in place. Because component memory can be reallocated later.
	 * @note It also checks for component in the garbage pool.
	 * 
	 * @param entity entity instance
	 * @param componentType target component typeid()
	 */
	View<Component> tryGet(ID<Entity> entity, std::type_index componentType) const noexcept
	{
		auto componentData = entities.get(entity)->findComponent(componentType.hash_code());
		GarbageComponent garbagePair = std::make_pair(componentType.hash_code(), entity);
		if (!componentData || garbageComponents.find(garbagePair) != garbageComponents.end())
			return {};
		return componentData->system->getComponent(componentData->instance);
	}
	/**
	 * @brief Returns component data accessor if exist, otherwise null. (@ref View)
	 * @warning Do not store views, use them only in place. Because component memory can be reallocated later.
	 * @note It also checks for component in the garbage pool.
	 * 
	 * @param entity entity instance
	 * @tparam T target component type
	 */
	template<class T>
	View<T> tryGet(ID<Entity> entity) const noexcept
	{
		static_assert(std::is_base_of_v<Component, T>, "Must be derived from the Component struct.");
		return View<T>(tryGet(entity, typeid(T)));
	}

	/*******************************************************************************************************************
	 * @brief Returns entity component @ref ID.
	 * @details See the getID<T>(ID<Entity> entity).
	 *
	 * @param entity entity instance
	 * @param componentType target component typeid()
	 * 
	 * @throw EcsmError if component is not found.
	 */
	ID<Component> getID(ID<Entity> entity, std::type_index componentType) const
	{
		auto componentData = entities.get(entity)->findComponent(componentType.hash_code());
		if (!componentData)
		{
			throw EcsmError("Component is not added. ("
				"type: " + typeToString(componentType) + ", "
				"entity:" + std::to_string(*entity) + ")");
		}
		return componentData->instance;
	}
	/**
	 * @brief Returns entity component @ref ID.
	 * @details Useful when we need to access and store entity component IDs.
	 *
	 * @param entity entity instance
	 * @tparam T target component type
	 *
	 * @throw EcsmError if component is not found.
	 */
	template<class T>
	ID<T> getID(ID<Entity> entity) const
	{
		static_assert(std::is_base_of_v<Component, T>, "Must be derived from the Component struct.");
		return ID<T>(getID(entity, typeid(T)));
	}

	/**
	 * @brief Returns entity component @ref ID if added, otherwise null.
	 * @note It also checks for component in the garbage pool.
	 * @details See the tryGetID<T>(ID<Entity> entity).
	 *
	 * @param entity entity instance
	 * @param componentType target component typeid()
	 */
	ID<Component> tryGetID(ID<Entity> entity, std::type_index componentType) const noexcept
	{
		auto componentData = entities.get(entity)->findComponent(componentType.hash_code());
		GarbageComponent garbagePair = std::make_pair(componentType.hash_code(), entity);
		if (!componentData || garbageComponents.find(garbagePair) != garbageComponents.end())
			return {};
		return componentData->instance;
	}
	/**
	 * @brief Returns entity component @ref ID if added, otherwise null.
	 * @details Useful when we need to access and store entity component identifiers.
	 *
	 * @param entity entity instance
	 * @tparam T target component type
	 */
	template<class T>
	ID<T> tryGetID(ID<Entity> entity) const noexcept
	{
		static_assert(std::is_base_of_v<Component, T>, "Must be derived from the Component struct.");
		return ID<T>(tryGetID(entity, typeid(T)));
	}

	/*******************************************************************************************************************
	 * @brief Returns true if entity has components.
	 * @param entity target entity instance
	 */
	uint32_t hasComponents(ID<Entity> entity) const noexcept
	{
		return (uint32_t)entities.get(entity)->hasComponents();
	}
	/**
	 * @brief Returns entity component count.
	 * @param entity target entity instance
	 */
	uint32_t getComponentCount(ID<Entity> entity) const noexcept
	{
		return (uint32_t)entities.get(entity)->getComponentCount();
	}
	
	/**
	 * @brief Resets entity component data.
	 *
	 * @param entity target entity instance
	 * @param componentType target component typeid()
	 * @param full reset all component data
	 * 
	 * @throw EcsmError if component is not found.
	 */
	void reset(ID<Entity> entity, std::type_index componentType, bool full = true)
	{
		auto componentData = entities.get(entity)->findComponent(componentType.hash_code());
		if (!componentData)
		{
			throw EcsmError("Component is not added. ("
				"type: " + typeToString(componentType) + ", "
				"entity:" + std::to_string(*entity) + ")");
		}
		auto componentView = componentData->system->getComponent(componentData->instance);
		componentData->system->resetComponent(componentView, full);
	}
	/**
	 * @brief Resets entity component data.
	 * 
	 * @param entity entity instance
	 * @param full reset all component data
	 * @tparam T target component type
	 * 
	 * @throw EcsmError if component is not found.
	 */
	template<class T>
	void reset(ID<Entity> entity, bool full = true)
	{
		static_assert(std::is_base_of_v<Component, T>, "Must be derived from the Component struct.");
		reset(entity, typeid(T), full);
	}
	/**
	 * @brief Resets entity components data.
	 * @param entity target entity instance
	 * @param full reset all component data
	 */
	void resetComponents(ID<Entity> entity, bool full = true);

	/**
	 * @brief Increases entity component array capacity.
	 * 
	 * @param entity target entity instance
	 * @param capacity component array capacity
	 */
	void reserveComponents(ID<Entity> entity, uint32_t capacity)
	{
		auto entityView = entities.get(entity);
		if (capacity <= entityView->capacity)
			return;
		entityView->reserve(capacity);
	}

	/*******************************************************************************************************************
	 * @brief Registers a new unordered event.
	 * @param name target event name
	 * @throw EcsmError if event is already registered.
	 */
	void registerEvent(std::string_view name);
	/**
	 * @brief Registers a new unordered event if not exist.
	 * @param name target event name
	 * @return True if event is registered, otherwise false.
	 */
	bool tryRegisterEvent(std::string_view name);

	/**
	 * @brief Registers a new ordered event before another.
	 * 
	 * @param newEvent target event name
	 * @param beforeEvent name of the event after target event
	 * 
	 * @throw EcsmError if event is already registered.
	 */
	void registerEventBefore(std::string_view newEvent, std::string_view beforeEvent);
	/**
	 * @brief Registers a new ordered event after another.
	 *
	 * @param newEvent target event name
	 * @param afterEvent name of the event before target event
	 *
	 * @throw EcsmError if event is already registered.
	 */
	void registerEventAfter(std::string_view newEvent, std::string_view afterEvent);

	/**
	 * @brief Unregisters existing event.
	 * @param name target event name
	 * @throw EcsmError if event is not registered, or not found.
	 */
	void unregisterEvent(std::string_view name);
	/**
	 * @brief Unregisters event if exist.
	 * @param name target event name
	 * @return True if event is unregistered, otherwise false.
	 */
	bool tryUnregisterEvent(std::string_view name);

	/**
	 * @brief Returns true if event is registered.
	 * @param name target event name
	 */
	bool hasEvent(std::string_view name) const noexcept
	{
		assert(!name.empty());
		return events.find(name) != events.end();
	}
	/**
	 * @brief Returns event data container by name.
	 * @param name target event name
	 * @throw EcsmError if event is not registered.
	 */
	const Event& getEvent(std::string_view name) const;
	/**
	 * @brief Returns event data container by name if it exist, otherwise null.
	 * @param name target event name
	 */
	const Event* tryGetEvent(std::string_view name) const;

	/**
	 * @brief Calls all event subscribers.
	 * @param name target event name
	 * @throw EcsmError if event is not registered.
	 */
	void runEvent(std::string_view name);
	/**
	 * @brief Calls all event subscribers if event exist.
	 * @param name target event name
	 * @return True if event is found.
	 */
	bool tryRunEvent(std::string_view name);
	/**
	 * @brief Runs all ordered events.
	 * @details Unordered events subscribers are not called.
	 */
	void runOrderedEvents();

	/**
	 * @brief Adds a new event subscriber.
	 * 
	 * @param name target event name
	 * @param[in] onEvent on event function callback
	 * 
	 * @throw EcsmError if event is not registered.
	 */
	void subscribeToEvent(std::string_view name, const std::function<void()>& onEvent);
	/**
	 * @brief Removes existing event subscriber.
	 * 
	 * @param name target event name
	 * @param[in] onEvent on event function callback
	 * 
	 * @throw EcsmError if event is not registered, or not subscribed.
	 */
	void unsubscribeFromEvent(std::string_view name, const std::function<void()>& onEvent);

	/**
	 * @brief Adds a new event subscriber if not exist.
	 * 
	 * @param name target event name
	 * @param[in] onEvent on event function callback
	 * 
	 * @return True if subscribed to the event, otherwise false.
	 */
	bool trySubscribeToEvent(std::string_view name, const std::function<void()>& onEvent);
	/**
	 * @brief Removes existing event subscriber if exist.
	 * 
	 * @param name target event name
	 * @param[in] onEvent on event function callback
	 * 
	 * @throw True if unsubscribed from the event, otherwise false.
	 */
	bool tryUnsubscribeFromEvent(std::string_view name, const std::function<void()>& onEvent);

	/*******************************************************************************************************************
	 * @brief Returns all manager systems.
	 * @note Use manager functions to access systems.
	 */
	const Systems& getSystems() const noexcept { return systems; }
	/**
	 * @brief Returns registered system groups.
	 * @note Use manager functions to access system groups.
	 */
	const SystemGroups& getSystemGroups() const noexcept { return systemGroups; }
	/**
	 * @brief Returns all manager component types.
	 * @note Use manager functions to access components.
	 */
	const ComponentTypes& getComponentTypes() const noexcept { return componentTypes; }
	/**
	 * @brief Returns all manager component names.
	 * @note Use manager functions to access components.
	 */
	const ComponentNames& getComponentNames() const noexcept { return componentNames; }
	/**
	 * @brief Returns all manager events.
	 * @note Use manager functions to access events.
	 */
	const Events& getEvents() const noexcept { return events; }
	/**
	 * @brief Returns ordered manager events.
	 * @note Use manager functions to access events.
	 */
	const OrderedEvents& getOrderedEvents() const noexcept { return orderedEvents; }
	/**
	 * @brief Returns all manager entities.
	 * @note Use manager functions to access entities.
	 */
	const EntityPool& getEntities() const noexcept { return entities; }
	/**
	 * @brief Returns manager garbage components pool.
	 * @note Use manager functions to check if component is garbage.
	 */
	const GarbageComponents& getGarbageComponents() const noexcept { return garbageComponents; }
	/**
	 * @brief Returns true if manager is initialized.
	 */
	bool isInitialized() const noexcept { return initialized; }

	/*******************************************************************************************************************
	 * @brief Initializes all created systems.
	 * @throw EcsmError if manager is already initialized.
	 */
	void initialize();

	/**
	 * @brief Runs ordered events and disposes destroyed resources on each tick.
	 * @throw EcsmError if manager is not initialized.
	 */
	void update();

	/**
	 * @brief Enters update loop. Executes @ref update() on each tick.
	 * @throw EcsmError if manager is not initialized.
	 */
	void start();

	/*******************************************************************************************************************
	 * @brief Actually destroys garbage components.
	 * @details Components are not destroyed immediately, only after the dispose call.
	 */
	void disposeGarbageComponents();
	/**
	 * @brief Actually destroys system components and internal resources.
	 * @details System components are not destroyed immediately, only after the dispose call.
	 */
	void disposeSystemComponents();
	/**
	 * @brief Actually destroys entities.
	 * @details Entities are not destroyed immediately, only after the dispose call.
	 */
	void disposeEntities() { entities.dispose(); }

	/*******************************************************************************************************************
	 * @brief Locks manager for synchronous access. (MT-Safe)
	 * @note Use it if you want to access manager from multiple threads asynchronously.
	 */
	void lock() { locker.lock(); }
	/**
	 * @brief Tries to locks manager for synchronous access. (MT-Safe)
	 * @note Use it if you want to access manager from multiple threads asynchronously.
	 */
	bool tryLock() noexcept { return locker.try_lock(); }
	/**
	 * @brief Unlock manager after synchronous access. (MT-Safe)
	 * @note Always unlock manager after synchronous access!
	 */
	void unlock() noexcept { locker.unlock(); }

	/**
	 * @brief Sets manager singleton to this instance.
	 * @details Useful in cases when we need to switch between multiple managers.
	 */
	void setSingletonCurrent() noexcept { singletonInstance = this; }
	/**
	 * @brief Unsets manager singleton instance.
	 * @details See the @ref setSingletonCurrent().
	 */
	void unsetSingletonCurrent() noexcept { singletonInstance = nullptr; }
};

/***********************************************************************************************************************
 * @brief Base system class with components.
 * @details See the @ref System.
 *
 * @tparam T type of the system component
 * @tparam DestroyComponents system should call destroy() function of the components
 */
template<class T = Component, bool DestroyComponents = true>
class ComponentSystem : public System
{
public:
	typedef T ComponentType; /**< Type of the system component. */
	using Components = LinearPool<T, DestroyComponents>; /**< System component pool type. */
protected:
	Components components; /**< System component pool. */

	/**
	 * @brief Creates a new component instance for the entity.
	 * @details You should use @ref Manager to add components to the entity.
	 */
	ID<Component> createComponent(ID<Entity> entity) override
	{
		return ID<Component>(components.create());
	}
	/**
	 * @brief Destroys component instance.
	 * @details You should use @ref Manager to remove components from the entity.
	 */
	void destroyComponent(ID<Component> instance) override
	{
		auto component = components.get(ID<T>(instance));
		resetComponent(View<Component>(component), false);
		components.destroy(ID<T>(instance));
	}
	/**
	 * @brief Resets component data.
	 * @details You should use @ref Manager to remove components from the entity.
	 */
	void resetComponent(View<Component> component, bool full) override { }
	/**
	 * @brief Copies component data from source to destination.
	 * @details You should use @ref Manager to copy component data of entities.
	 */
	void copyComponent(View<Component> source, View<Component> destination) override { }
public:
	/**
	 * @brief Returns system component pool.
	 */
	const Components& getComponents() const noexcept { return components; }
	/**
	 * @brief Returns specific component name of the system.
	 */
	std::string_view getComponentName() const override
	{
		static const std::string name = typeToString(typeid(T));
		return name;
	}
	/**
	 * @brief Returns specific component typeid() of the system.
	 * @note Override it to define a custom component of the system.
	 */
	std::type_index getComponentType() const override { return typeid(T); }

	/**
	 * @brief Returns specific component @ref View.
	 * @param instance target system component instance
	 */
	View<Component> getComponent(ID<Component> instance) override
	{
		return View<Component>(components.get(ID<T>(instance)));
	}
	/**
	 * @brief Actually destroys system components.
	 * @details Components are not destroyed immediately, only after the dispose call.
	 */
	void disposeComponents() override { components.dispose(); }

	/**
	 * @brief Returns true if entity has specific component.
	 * @param entity target entity with component
	 * @note This function is faster than the Manager one.
	 */
	bool hasComponent(ID<Entity> entity) const
	{
		const auto entityView = Manager::Instance::get()->getEntities().get(entity);
		return entityView->findComponent(typeid(T).hash_code());
	}
	/**
	 * @brief Returns entity specific component view.
	 * @param entity target entity with component
	 * @note This function is faster than the Manager one.
	 */
	View<T> getComponent(ID<Entity> entity) const
	{
		const auto entityView = Manager::Instance::get()->getEntities().get(entity);
		auto componentData = entityView->findComponent(typeid(T).hash_code());
		if (!componentData)
		{
			throw EcsmError("Component is not added. ("
				"type: " + typeToString(typeid(T)) + ", "
				"entity:" + std::to_string(*entity) + ")");
		}
		return components.get(ID<T>(componentData->instance));
	}
	/**
	 * @brief Returns entity specific component view if exist.
	 * @param entity target entity with component
	 * @note This function is faster than the Manager one.
	 */
	View<T> tryGetComponent(ID<Entity> entity) const
	{
		const auto entityView = Manager::Instance::get()->getEntities().get(entity);
		auto componentData = entityView->findComponent(typeid(T).hash_code());
		if (!componentData)
			return {};
		return components.get(ID<T>(componentData->instance));
	}
	/**
	 * @brief Resets entity specific component data.
	 * @param entity target entity with component
	 * @param full reset all component data
	 * @note This function is faster than the Manager one.
	 */
	void resetComponentData(ID<Entity> entity, bool full = true)
	{
		const auto entityView = Manager::Instance::get()->getEntities().get(entity);
		auto componentData = entityView->findComponent(typeid(T).hash_code());
		if (!componentData)
		{
			throw EcsmError("Component is not added. ("
				"type: " + typeToString(typeid(T)) + ", "
				"entity:" + std::to_string(*entity) + ")");
		}
		auto component = components.get(ID<T>(componentData->instance));
		resetComponent(View<Component>(component), full);
	}
};

/***********************************************************************************************************************
 * @brief Component indicating that this entity should not be destroyed.
 * @details Useful in cases when we need to mark important entities like main camera.
 */
struct DoNotDestroyComponent : public Component { };

/**
 * @brief Handles entities that should not be destroyed.
 */
class DoNotDestroySystem : public ComponentSystem<DoNotDestroyComponent, false>, 
	public Singleton<DoNotDestroySystem>
{
protected:
	/**
	 * @brief Creates a new do not destroy system instance.
	 * @param setSingleton set system singleton instance
	 */
	DoNotDestroySystem(bool setSingleton = true);
	/**
	 * @brief Destroys do not destroy system instance.
	 */
	~DoNotDestroySystem() override;

	std::string_view getComponentName() const override;
	friend class ecsm::Manager;
};

/***********************************************************************************************************************
 * @brief Component indicating that this entity should not be duplicated.
 * @details Useful in cases when we need to mark important entities like main camera.
 */
struct DoNotDuplicateComponent : public Component { };

/**
 * @brief Handles entities that should not be duplicated.
 */
class DoNotDuplicateSystem : public ComponentSystem<DoNotDuplicateComponent, false>, 
	public Singleton<DoNotDuplicateSystem>
{
protected:
	/**
	 * @brief Creates a new do not duplicate system instance.
	 * @param setSingleton set system singleton instance
	 */
	DoNotDuplicateSystem(bool setSingleton = true);
	/**
	 * @brief Destroys do not duplicate system instance.
	 */
	~DoNotDuplicateSystem() override;

	std::string_view getComponentName() const override;
	friend class ecsm::Manager;
};

} // namespace ecsm