// Copyright 2022-2024 Nikita Fediuchin. All rights reserved.
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

#include <set>
#include <map>
#include <mutex>
#include <algorithm>
#include <functional>
#include <type_traits>
#include <string_view>
#include <unordered_map>

namespace ecsm
{

using namespace std;

class Entity;
struct Component;
class System;
class Manager;
class SystemExt;

/**
 * @brief Subscribes @ref System function to the event.
 */
#define ECSM_SUBSCRIBE_TO_EVENT(name, func) \
	Manager::Instance::get()->subscribeToEvent(name, std::bind(&func, this))
/**
 * @brief Unsubscribes @ref System function from the event.
 */
#define ECSM_UNSUBSCRIBE_FROM_EVENT(name, func) \
	Manager::Instance::get()->unsubscribeFromEvent(name, std::bind(&func, this))

/**
 * @brief Subscribes @ref System function to the event if exist.
 */
#define ECSM_TRY_SUBSCRIBE_TO_EVENT(name, func) \
	Manager::Instance::get()->trySubscribeToEvent(name, std::bind(&func, this))
/**
 * @brief Unsubscribes @ref System function from the event if exist.
 */
#define ECSM_TRY_UNSUBSCRIBE_FROM_EVENT(name, func) \
	Manager::Instance::get()->tryUnsubscribeFromEvent(name, std::bind(&func, this))

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
	 * @brief Copies system component data from source to destination.
	 * @details You should use @ref Manager to copy component data of entities.
	 * @note Override it to define a custom component of the system.
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
	virtual const string& getComponentName() const;
	/**
	 * @brief Returns specific component typeid() of the system.
	 * @note Override it to define a custom component of the system.
	 */
	virtual type_index getComponentType() const;
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
	using SystemComponent = pair<System*, ID<Component>>;
	using Components = map<type_index, SystemComponent>;
private:
	Components components;
	friend class Manager;
public:
	/**
	 * @brief Destroys all entity components.
	 * @note Components are not destroyed immediately, only after the dispose call.
	 */
	bool destroy();

	/**
	 * @brief Returns entity components map.
	 * @details Use @ref Manager to add or remove components.
	 */
	const Components& getComponents() const noexcept { return components; }
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
	 * @brief Event subscribers holder.
	 */
	struct Event final
	{
		using Subscribers = std::vector<std::function<void()>>;

		string name;
		Subscribers subscribers;
		bool isOrdered = false;

		Event(const string& name, bool isOrdered = true)
		{
			this->name = name;
			this->isOrdered = isOrdered;
		}
	};

	using Systems = unordered_map<type_index, System*>;
	using ComponentTypes = unordered_map<type_index, System*>;
	using ComponentNames = map<string, System*>;
	using Events = unordered_map<string, Event*>;
	using OrderedEvents = vector<const Event*>;
	using EntityPool = LinearPool<Entity>;
	using GarbageComponent = pair<type_index, ID<Entity>>;
	using GarbageComponents = set<GarbageComponent>;
private:
	Systems systems;
	ComponentTypes componentTypes;
	ComponentNames componentNames;
	EntityPool entities;
	Events events;
	OrderedEvents orderedEvents;
	GarbageComponents garbageComponents;
	mutex locker;
	bool initialized = false;
	bool running = false;

	#ifndef NDEBUG
	bool isChanging = false;
	#endif

	void addSystem(System* system, type_index type);
public:
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
	 * @throw runtime_error if system is already created or component type registered.
	 */
	template<class T = System, typename... Args>
	void createSystem(Args&&... args)
	{
		static_assert(is_base_of_v<System, T>, "Must be derived from the System class.");
		#ifndef NDEBUG
		if (isChanging)
			throw runtime_error("Creation of the system inside other create/destroy is not allowed.");
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
	 * @throw runtime_error if system is not found.
	 */
	void destroySystem(type_index type);
	/**
	 * @brief Terminates and destroys system.
	 * @tparam T target system type
	 * @throw runtime_error if system is not found.
	 */
	template<class T = System>
	void destroySystem()
	{
		static_assert(is_base_of_v<System, T>, "Must be derived from the System class.");
		destroySystem(typeid(T));
	}

	/**
	 * @brief Terminates and destroys system if exist.
	 * @param type target system typeid()
	 * @return True if system is destroyed, otherwise false.
	 */
	bool tryDestroySystem(type_index type);
	/**
	 * @brief Terminates and destroys system if exist.
	 * @tparam T target system type
	 * @return True if system is destroyed, otherwise false.
	 */
	template<class T = System>
	bool tryDestroySystem()
	{
		static_assert(is_base_of_v<System, T>, "Must be derived from the System class.");
		return tryDestroySystem(typeid(T));
	}

	/*******************************************************************************************************************
	 * @brief Returns true if system is created.
	 * @param type target system typeid()
	 */
	bool has(type_index type) const noexcept
	{
		return systems.find(type) != systems.end();
	}
	/**
	 * @brief Returns true if system is created.
	 * @tparam T target system type
	 */
	template<class T = System>
	bool has() const noexcept
	{
		static_assert(is_base_of_v<System, T>, "Must be derived from the System class.");
		return has(typeid(T));
	}

	/**
	 * @brief Returns system instance.
	 * @warning Be carefull with system pointer, it can be destroyed later.
	 * @param type target system typeid()
	 * @throw runtime_error if system is not found.
	 */
	System* get(type_index type) const
	{
		auto result = systems.find(type);
		if (result == systems.end())
			throw runtime_error("System is not created. (type: " + typeToString(type) + ")");
		return result->second;
	}
	/**
	 * @brief Returns system instance.
	 * @tparam T target system type
	 * @throw runtime_error if system is not found.
	 */
	template<class T = System>
	T* get() const
	{
		static_assert(is_base_of_v<System, T>, "Must be derived from the System class.");
		return (T*)get(typeid(T));
	}

	/**
	 * @brief Returns system instance if created, otherwise nullptr.
	 * @param type target system typeid()
	 */
	System* tryGet(type_index type) const noexcept
	{
		auto result = systems.find(type);
		return result == systems.end() ? nullptr : result->second;
	}
	/**
	 * @brief Returns system instance if created, otherwise nullptr.
	 * @tparam T target system type
	 */
	template<class T = System>
	T* tryGet() const noexcept
	{
		static_assert(is_base_of_v<System, T>, "Must be derived from the System class.");
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
	 * @throw runtime_error if component type is not registered, or component is already added.
	 */
	View<Component> add(ID<Entity> entity, type_index componentType);

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
	 * @throw runtime_error if component type is not registered, or component is already added.
	 */
	template<class T = Component>
	View<T> add(ID<Entity> entity)
	{
		static_assert(is_base_of_v<Component, T>, "Must be derived from the Component struct.");
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
	 * @throw runtime_error if component is not found.
	 */
	void remove(ID<Entity> entity, type_index componentType);
	/**
	 * @brief Removes component from the entity.
	 * @details Component data destruction is handled by the @ref System.
	 * @note Components are not destroyed immediately, only after the dispose call.
	 *
	 * @param entity entity instance
	 * @tparam T target component type
	 * 
	 * @throw runtime_error if component is not found.
	 */
	template<class T = Component>
	void remove(ID<Entity> entity)
	{ 
		static_assert(is_base_of_v<Component, T>, "Must be derived from the Component struct.");
		remove(entity, typeid(T));
	}

	/**
	 * @brief Returns true if target entity component was removed and is in the garbage pool.
	 * @details See the @ref remove<T>(ID<Entity> entity).
	 * @note Components are not destroyed immediately, only after the dispose call.
	 *
	 * @param entity entity instance
	 * @param componentType target component typeid()
	 *
	 * @throw runtime_error if component is not found.
	 */
	bool isGarbage(ID<Entity> entity, type_index componentType) const noexcept
	{
		return garbageComponents.find(make_pair(componentType, entity)) != garbageComponents.end();
	}
	/**
	 * @brief Returns true if target entity component was removed and is in the garbage pool.
	 * @details See the @ref remove<T>(ID<Entity> entity).
	 * @note Components are not destroyed immediately, only after the dispose call.
	 *
	 * @param entity entity instance
	 * @tparam T target component type
	 *
	 * @throw runtime_error if component is not found.
	 */
	template<class T = Component>
	bool isGarbage(ID<Entity> entity) const noexcept
	{
		static_assert(is_base_of_v<Component, T>, "Must be derived from the Component struct.");
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
	 * @throw runtime_error if source or destination component is not found.
	 */
	void copy(ID<Entity> source, ID<Entity> destination, type_index componentType);
	/**
	 * @brief Copies component data from source entity to destination.
	 * @details Component data copying is handled by the @ref System.
	 *
	 * @param source copy from entity instance
	 * @param destination copy to entity instance
	 * @tparam T target component type
	 *
	 * @throw runtime_error if source or destination component is not found.
	 */
	template<class T = Component>
	void copy(ID<Entity> source, ID<Entity> destination)
	{
		static_assert(is_base_of_v<Component, T>, "Must be derived from the Component struct.");
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
	bool has(ID<Entity> entity, type_index componentType) const noexcept
	{
		assert(entity);
		const auto& components = entities.get(entity)->components;
		return components.find(componentType) != components.end() && garbageComponents.find(
			make_pair(componentType, entity)) == garbageComponents.end();
	}
	/**
	 * @brief Returns true if entity has target component.
	 * @note It also checks for component in the garbage pool.
	 *
	 * @param entity entity instance
	 * @tparam T target component type
	 */
	template<class T = Component>
	bool has(ID<Entity> entity) const noexcept
	{
		static_assert(is_base_of_v<Component, T>, "Must be derived from the Component struct.");
		return has(entity, typeid(T));
	}

	/**
	 * @brief Returns component data accessor. (@ref View)
	 * @warning Do not store views, use them only in place. Because component memory can be reallocated later.
	 * 
	 * @param entity entity instance
	 * @param componentType target component typeid()
	 * 
	 * @throw runtime_error if component is not found.
	 */
	View<Component> get(ID<Entity> entity, type_index componentType) const
	{
		assert(entity);
		auto entityView = entities.get(entity);
		auto result = entityView->components.find(componentType);

		if (result == entityView->components.end())
		{
			throw runtime_error("Component is not added. ("
				"type: " + typeToString(componentType) +
				"entity:" + to_string(*entity) + ")");
		}

		auto pair = result->second;
		return pair.first->getComponent(pair.second);
	}
	/**
	 * @brief Returns component data accessor. (@ref View)
	 * @warning Do not store views, use them only in place. Because component memory can be reallocated later.
	 * 
	 * @param entity entity instance
	 * @tparam T target component type
	 * 
	 * @throw runtime_error if component is not found.
	 */
	template<class T = Component>
	View<T> get(ID<Entity> entity) const
	{
		static_assert(is_base_of_v<Component, T>, "Must be derived from the Component struct.");
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
	View<Component> tryGet(ID<Entity> entity, type_index componentType) const noexcept
	{
		assert(entity);
		const auto& components = entities.get(entity)->components;
		
		auto result = components.find(componentType);
		if (result == components.end() || garbageComponents.find(
			make_pair(componentType, entity)) != garbageComponents.end())
		{
			return {};
		}

		auto pair = result->second;
		return pair.first->getComponent(pair.second);
	}
	/**
	 * @brief Returns component data accessor if exist, otherwise null. (@ref View)
	 * @warning Do not store views, use them only in place. Because component memory can be reallocated later.
	 * @note It also checks for component in the garbage pool.
	 * 
	 * @param entity entity instance
	 * @tparam T target component type
	 */
	template<class T = Component>
	View<T> tryGet(ID<Entity> entity) const noexcept
	{
		static_assert(is_base_of_v<Component, T>, "Must be derived from the Component struct.");
		return View<T>(tryGet(entity, typeid(T)));
	}

	/*******************************************************************************************************************
	 * @brief Returns entity component @ref ID.
	 * @details See the getID<T>(ID<Entity> entity).
	 *
	 * @param entity entity instance
	 * @param componentType target component typeid()
	 * 
	 * @throw runtime_error if component is not found.
	 */
	ID<Component> getID(ID<Entity> entity, type_index componentType) const
	{
		assert(entity);
		auto entityView = entities.get(entity);
		auto result = entityView->components.find(componentType);

		if (result == entityView->components.end())
		{
			throw runtime_error("Component is not added. ("
				"type: " + typeToString(componentType) +
				"entity:" + to_string(*entity) + ")");
		}

		return result->second.second;
	}
	/**
	 * @brief Returns entity component @ref ID.
	 * @details Useful when we need to access and store entity component IDs.
	 *
	 * @param entity entity instance
	 * @tparam T target component type
	 *
	 * @throw runtime_error if component is not found.
	 */
	template<class T = Component>
	ID<T> getID(ID<Entity> entity) const
	{
		static_assert(is_base_of_v<Component, T>, "Must be derived from the Component struct.");
		return ID<T>(getID(entity, typeid(T)));
	}

	/*******************************************************************************************************************
	 * @brief Returns entity component @ref ID if added, otherwise null.
	 * @note It also checks for component in the garbage pool.
	 * @details See the tryGetID<T>(ID<Entity> entity).
	 *
	 * @param entity entity instance
	 * @param componentType target component typeid()
	 */
	ID<Component> tryGetID(ID<Entity> entity, type_index componentType) const noexcept
	{
		assert(entity);
		const auto& components = entities.get(entity)->components;

		auto result = components.find(componentType);
		if (result == components.end() || garbageComponents.find(
			make_pair(componentType, entity)) != garbageComponents.end())
		{
			return {};
		}

		auto pair = result->second;
		return pair.second;
	}
	/**
	 * @brief Returns entity component @ref ID if added, otherwise null.
	 * @details Useful when we need to access and store entity component identifiers.
	 *
	 * @param entity entity instance
	 * @tparam T target component type
	 */
	template<class T = Component>
	ID<T> tryGetID(ID<Entity> entity) const noexcept
	{
		static_assert(is_base_of_v<Component, T>, "Must be derived from the Component struct.");
		return ID<T>(tryGetID(entity, typeid(T)));
	}

	/**
	 * @brief Returns entity component count.
	 * @param entity target entity instance
	 */
	uint32_t getComponentCount(ID<Entity> entity) const noexcept
	{
		return (uint32_t)entities.get(entity)->components.size();
	}

	/*******************************************************************************************************************
	 * @brief Registers a new unordered event.
	 * @param[in] name target event name
	 * @throw runtime_error if event is already registered.
	 */
	void registerEvent(const string& name);
	/**
	 * @brief Registers a new unordered event if not exist.
	 * @param[in] name target event name
	 * @return True if event is registered, otherwise false.
	 */
	bool tryRegisterEvent(const string& name);

	/**
	 * @brief Registers a new ordered event before another.
	 * 
	 * @param[in] newEvent target event name
	 * @param[in] beforeEvent name of the event after target event
	 * 
	 * @throw runtime_error if event is already registered.
	 */
	void registerEventBefore(const string& newEvent, const string& beforeEvent);
	/**
	 * @brief Registers a new ordered event after another.
	 *
	 * @param[in] newEvent target event name
	 * @param[in] afterEvent name of the event before target event
	 *
	 * @throw runtime_error if event is already registered.
	 */
	void registerEventAfter(const string& newEvent, const string& afterEvent);

	/**
	 * @brief Unregisters existing event.
	 * @param[in] name target event name
	 * @throw runtime_error if event is not registered, or not found.
	 */
	void unregisterEvent(const string& name);
	/**
	 * @brief Unregisters event if exist.
	 * @param[in] name target event name
	 * @return True if event is unregistered, otherwise false.
	 */
	bool tryUnregisterEvent(const string& name);

	/**
	 * @brief Returns true if event is registered.
	 * @param[in] name target event name
	 */
	bool hasEvent(const string& name) const noexcept
	{
		assert(!name.empty());
		return events.find(name) != events.end();
	}
	/**
	 * @brief Returns true if event is ordered.
	 * @param[in] name target event name
	 * @throw runtime_error if event is not registered.
	 */
	bool isEventOrdered(const string& name) const;
	/**
	 * @brief Returns all event subscribers.
	 * @param[in] name target event name
	 * @throw runtime_error if event is not registered.
	 */
	const Event::Subscribers& getEventSubscribers(const string& name) const;
	/**
	 * @brief Returns true if event has subscribers.
	 * @param[in] name target event name
	 * @throw runtime_error if event is not registered.
	 */
	bool isEventHasSubscribers(const string& name) const;

	/**
	 * @brief Calls all event subscribers.
	 * @param[in] name target event name
	 * @throw runtime_error if event is not registered.
	 */
	void runEvent(const string& name);
	/**
	 * @brief Calls all event subscribers if event exist.
	 * @param[in] name target event name
	 * @return True if event is found.
	 */
	bool tryRunEvent(const string& name);
	/**
	 * @brief Runs all ordered events.
	 * @details Unordered events subscribers are not called.
	 */
	void runOrderedEvents();

	/**
	 * @brief Adds a new event subscriber.
	 * 
	 * @param[in] name target event name
	 * @param[in] onEvent on event function callback
	 * 
	 * @throw runtime_error if event is not registered.
	 */
	void subscribeToEvent(const string& name, const std::function<void()>& onEvent);
	/**
	 * @brief Removes existing event subscriber.
	 * 
	 * @param[in] name target event name
	 * @param[in] onEvent on event function callback
	 * 
	 * @throw runtime_error if event is not registered, or not subscribed.
	 */
	void unsubscribeFromEvent(const string& name, const std::function<void()>& onEvent);

	/**
	 * @brief Adds a new event subscriber if not exist.
	 * 
	 * @param[in] name target event name
	 * @param[in] onEvent on event function callback
	 * 
	 * @return True if subscribed to the event, otherwise false.
	 */
	bool trySubscribeToEvent(const string& name, const std::function<void()>& onEvent);
	/**
	 * @brief Removes existing event subscriber if exist.
	 * 
	 * @param[in] name target event name
	 * @param[in] onEvent on event function callback
	 * 
	 * @throw True if unsubscribed from the event, otherwise false.
	 */
	bool tryUnsubscribeFromEvent(const string& name, const std::function<void()>& onEvent);

	/*******************************************************************************************************************
	 * @brief Returns all manager systems.
	 * @note Use manager functions to access systems.
	 */
	const Systems& getSystems() const noexcept { return systems; }
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
	/**
	 * @brief Returns true if manager is currently running.
	 */
	bool isRunning() const noexcept { return running; }

	/*******************************************************************************************************************
	 * @brief Initializes all created systems.
	 * @throw runtime_error if manager is already initialized.
	 */
	void initialize();

	/**
	 * @brief Runs ordered events and disposes destroyed resources on each tick.
	 * @throw runtime_error if manager is not initialized.
	 */
	void update();

	/**
	 * @brief Enters update loop. Executes @ref update() on each tick.
	 * @throw runtime_error if manager is not initialized.
	 */
	void start();
	/**
	 * @brief Stops update loop.
	 * @details Used to stop the update loop from some system.
	 */
	void stop() noexcept { running = false; }

	/*******************************************************************************************************************
	 * @brief Actually destroys garbage components.
	 * @details Components are not destroyed immediately, only after the dispose call.
	 */
	void disposeGarbageComponents();
	/**
	 * @brief Actually destroys system components and internal resources.
	 * @details Systen components are not destroyed immediately, only after the dispose call.
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
	void setSingletonCurrent() noexcept
	{
		singletonInstance = this;
	}
	/**
	 * @brief Unsets manager singleton instance.
	 * @details See the @ref setSingletonCurrent().
	 */
	void unsetSingletonCurrent() noexcept
	{
		singletonInstance = nullptr;
	}
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
protected:
	/**
	 * @brief System component pool.
	 */
	LinearPool<T, DestroyComponents> components;

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
		components.destroy(ID<T>(instance));
	}
	/**
	 * @brief Copies component data from source to destination.
	 * @details You should use @ref Manager to copy component data of entities.
	 */
	void copyComponent(View<Component> source, View<Component> destination) override
	{
		if constexpr (DestroyComponents)
		{
			auto destinationView = View<T>(destination);
			destinationView->destroy();
		}
	}
public:
	/**
	 * @brief Returns specific component name of the system.
	 */
	const string& getComponentName() const override
	{
		static const string name = typeToString(typeid(T));
		return name;
	}
	/**
	 * @brief Returns specific component typeid() of the system.
	 * @note Override it to define a custom component of the system.
	 */
	type_index getComponentType() const override
	{
		return typeid(T);
	}
	/**
	 * @brief Returns specific component @ref View.
	 */
	View<Component> getComponent(ID<Component> instance) override
	{
		return View<Component>(components.get(ID<T>(instance)));
	}
	/**
	 * @brief Actually destroys system components.
	 * @details Components are not destroyed immediately, only after the dispose call.
	 */
	void disposeComponents() override
	{
		components.dispose();
	}

	/**
	 * @brief Returns true if entity has specific component.
	 * @param entity target entity with component
	 * @note This function is faster than the Manager one.
	 */
	bool hasComponent(ID<Entity> entity) const
	{
		assert(entity);
		const auto entityView = Manager::Instance::get()->getEntities().get(entity);
		const auto& entityComponents = entityView->getComponents();
		return entityComponents.find(typeid(T)) != entityComponents.end();
	}
	/**
	 * @brief Returns entity specific component view.
	 * @param entity target entity with component
	 * @note This function is faster than the Manager one.
	 */
	View<T> getComponent(ID<Entity> entity) const
	{
		assert(entity);
		const auto entityView = Manager::Instance::get()->getEntities().get(entity);
		const auto& pair = entityView->getComponents().at(typeid(T));
		return components.get(ID<T>(pair.second));
	}
	/**
	 * @brief Returns entity specific component view if exist.
	 * @param entity target entity with component
	 * @note This function is faster than the Manager one.
	 */
	View<T> tryGetComponent(ID<Entity> entity) const
	{
		assert(entity);
		const auto entityView = Manager::Instance::get()->getEntities().get(entity);
		const auto& entityComponents = entityView->getComponents();
		auto result = entityComponents.find(typeid(T));
		if (result == entityComponents.end())
			return {};
		return components.get(ID<T>(result->second.second));
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
	 * @brief Creates a new do not duplicate system instance.
	 * @param setSingleton set system singleton instance
	 */
	DoNotDestroySystem(bool setSingleton = true);
	/**
	 * @brief Destroy baked transformer system instance.
	 */
	~DoNotDestroySystem() override;

	const string& getComponentName() const override;
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
	 * @brief Destroy baked transformer system instance.
	 */
	~DoNotDuplicateSystem() override;

	const string& getComponentName() const override;
	friend class ecsm::Manager;
};

} // namespace ecsm