//--------------------------------------------------------------------------------------------------
// Copyright 2022-2023 Nikita Fediuchin. All rights reserved.
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
//--------------------------------------------------------------------------------------------------

#pragma once
#include "linear-pool.hpp"

#include <map>
#include <string>
#include <typeinfo>
#include <typeindex>
#include <algorithm>
#include <type_traits>

namespace ecsm
{

using namespace std;

class Entity;
struct Component;
class System;
class Manager;

struct Component { };

//--------------------------------------------------------------------------------------------------
class System
{
	Manager* manager = nullptr;
	
	friend class Entity;
	friend class Manager;
protected:
	virtual ~System() = default;

	virtual void initialize() { } 
	virtual void terminate() { }
	virtual void update() { }

	virtual type_index getComponentType() const { return typeid(Component); }
	virtual ID<Component> createComponent(ID<Entity> entity) {
		throw runtime_error("System has no components."); }
	virtual void destroyComponent(ID<Component> instance) {
		throw runtime_error("System has no components."); }
	virtual View<Component> getComponent(ID<Component> instance) {
		throw runtime_error("System has no components."); }
	virtual void disposeComponents() { }
public:
	Manager* getManager() noexcept { return manager; }
	const Manager* getManager() const noexcept { return manager; }
};

//--------------------------------------------------------------------------------------------------
class Entity final
{
	map<type_index, pair<System*, ID<Component>>> components;
	friend class Manager;
public:
	bool destroy()
	{
		for (auto& component : components)
		{
			auto pair = component.second;
			pair.first->destroyComponent(pair.second);
		}
		return true;
	}

	const map<type_index, pair<System*, ID<Component>>>& getComponents()
		const noexcept { return components; }
};

//--------------------------------------------------------------------------------------------------
class Manager final
{
public:
	struct SubsystemData
	{
		System* system = nullptr;
		uint32_t isReference = false;

		SubsystemData() = default;
		SubsystemData(System* _system, uint32_t _isReference) :
			system(_system), isReference(_isReference) { }
	};
	struct SystemData
	{
		System* instance = nullptr;
		// WARNING: Manager is not responsible for subsystems init/update.
		vector<SubsystemData> subsystems;

		SystemData() = default;
		SystemData(System* _instance) : instance(_instance) { }
	};
private:
	map<type_index, SystemData> systemData;
	vector<System*> systems;
	map<type_index, System*> componentTypes;
	LinearPool<Entity> entities;
	bool isInitialized = false;
	bool isRunning = false;

	Manager(Manager&&) = default;
	Manager(const Manager&) = default;
	Manager& operator=(const Manager&) = default;
public:
//--------------------------------------------------------------------------------------------------
	Manager() = default;
	~Manager()
	{
		entities.clear(false);
		
		if (isInitialized)
		{
			for (auto i = systems.rbegin(); i != systems.rend(); i++)
			{
				auto system = *i;
				system->terminate();
				
				for (auto& data : systemData) // TODO: suboptimal
				{
					if (data.second.instance != system) continue;
					auto& subsystems = data.second.subsystems;
					for (auto j = subsystems.rbegin(); j != subsystems.rend(); j++)
					{ if (!j->isReference) delete j->system; }
					break;
				}
				
				delete system;
			}
		}
	}

	template<class T = System>
	bool has() const
	{
		static_assert(is_base_of_v<System, T>,
			"Must be derived from the System class.");
		return systemData.find(typeid(T)) != systemData.end();
	}

//--------------------------------------------------------------------------------------------------
	template<class T = System, typename... Args>
	void createSystem(Args&&... args)
	{
		static_assert(is_base_of_v<System, T>,
			"Must be derived from the System class.");

		#ifndef NDEBUG
		if (isInitialized)
			throw runtime_error("Can not create system after manager initialization.");
		if (systemData.find(typeid(T)) != systemData.end())
		{
			throw runtime_error("System is already created. ("
				"name: " + string(typeid(T).name()) + ")");
		}
		#endif

		auto system = new T(std::forward<Args>(args)...);
		system->manager = this;

		auto componentType = system->getComponentType();
		if (componentType != typeid(Component))
		{
			auto result = componentTypes.emplace(componentType, system);
			#ifndef NDEBUG
			if (!result.second)
			{
				throw runtime_error(
					"Component is already registered by the other system. ("
					"component: " + string(componentType.name()) + ", "
					"system: " + string(typeid(T).name()) + ")");
			}
			#endif
		}

		auto result = systemData.emplace(typeid(T), SystemData(system));
		assert(result.second == true);
		systems.emplace_back(system);
	}

//--------------------------------------------------------------------------------------------------
	template<class T = System, class S = System, typename... Args>
	void createSubsystem(Args&&... args)
	{
		static_assert(is_base_of_v<System, T>,
			"Must be derived from the System class.");
		static_assert(is_base_of_v<System, S>,
			"Must be derived from the System class.");

		#ifndef NDEBUG
		if (isInitialized)
			throw runtime_error("Can not create subsystem after manager initialization.");
		if (systemData.find(typeid(T)) == systemData.end())
		{
			throw runtime_error("System is not created. ("
				"system: " + string(typeid(T).name()) +
				"subsystem: " +  string(typeid(S).name()) + ")");
		}
		if (systemData.find(typeid(S)) != systemData.end())
		{
			throw runtime_error("Subsystem is already created. ("
				"system: " + string(typeid(T).name()) +
				"subsystem: " +  string(typeid(S).name()) + ")");
		}
		#endif

		auto system = new S(std::forward<Args>(args)...);
		system->manager = this;

		auto componentType = system->getComponentType();
		if (componentType != typeid(Component))
		{
			auto result = componentTypes.emplace(componentType, system);
			#ifndef NDEBUG
			if (!result.second)
			{
				throw runtime_error(
					"Component is already registered by the other system. ("
					"component: " + string(componentType.name()) + ", "
					"system: " + string(typeid(T).name()) + 
					"subsystem: " + string(typeid(S).name()) + ")");
			}
			#endif
		}
		
		auto result = systemData.emplace(typeid(S), SystemData(system));
		assert(result.second == true);
		systemData.at(typeid(T)).subsystems.emplace_back(SubsystemData(system, false));
	}

//--------------------------------------------------------------------------------------------------
	template<class T = System>
	void registerSubsystem(System* subsystem)
	{
		static_assert(is_base_of_v<System, T>,
			"Must be derived from the System class.");

		#ifndef NDEBUG
		if (isInitialized)
			throw runtime_error("Can not register subsystem after manager initialization.");
		if (systemData.find(typeid(T)) == systemData.end())
		{
			throw runtime_error("System is not created. ("
				"system: " + string(typeid(T).name()) + ")");
		}
		#endif

		systemData.at(typeid(T)).subsystems.emplace_back(SubsystemData(subsystem, true));
	}

	template<class T = System>
	T* get() const
	{
		static_assert(is_base_of_v<System, T>,
			"Must be derived from the System class.");
		#ifndef NDEBUG
		if (systemData.find(typeid(T)) == systemData.end())
		{
			throw runtime_error("System is not created. ("
				"name: " + string(typeid(T).name()) + ")");
		}
		#endif
		return (T*)systemData.at(typeid(T)).instance;
	}
	System* get(type_index type) const
	{
		#ifndef NDEBUG
		if (systemData.find(type) == systemData.end())
		{
			throw runtime_error("System is not created. ("
				"name: " + string(type.name()) + ")");
		}
		#endif
		return systemData.at(type).instance;
	}

//--------------------------------------------------------------------------------------------------
	const vector<System*>& getSystems() const noexcept { return systems; }

	template<class T = System>
	const vector<SubsystemData>& getSubsystems() const
	{
		static_assert(is_base_of_v<System, T>,
			"Must be derived from the System class.");
		#ifndef NDEBUG
		if (systemData.find(typeid(T)) == systemData.end())
		{
			throw runtime_error("System is not created. ("
				"name: " + string(typeid(T).name()) + ")");
		}
		#endif
		return systemData.at(typeid(T)).subsystems;
	}

	void disposeComponents(System* system) { system->disposeComponents(); }

//--------------------------------------------------------------------------------------------------
	ID<Entity> createEntity() { return entities.create(); }
	void destroy(ID<Entity> instance) { entities.destroy(instance); }
	View<Entity> get(ID<Entity> instance) const { return entities.get(instance); }

	bool has(ID<Entity> entity, type_index componentType) const
	{
		assert(!entity.isNull());
		auto entityView = entities.get(entity);
		auto& components = entityView->components;
		return components.find(componentType) != components.end();
	}
	template<class T = Component>
	bool has(ID<Entity> entity) const
	{
		static_assert(is_base_of_v<Component, T>,
			"Must be derived from the Component class.");
		return has(entity, typeid(T));
	}
	
	View<Component> add(ID<Entity> entity, type_index componentType)
	{
		#ifndef NDEBUG
		assert(!entity.isNull());
		if (componentTypes.find(componentType) == componentTypes.end())
		{
			throw runtime_error("Component is not registered by any system. ("
				"name: " + string(componentType.name()) +
				"entity:" + to_string(*entity) + ")");
		}
		#endif

		auto system = componentTypes.at(componentType);
		auto component = system->createComponent(entity);
		auto componentView = system->getComponent(component);
		auto entityView = entities.get(entity);

		auto result = entityView->components.emplace(
			componentType, make_pair(system, component));
		
		#ifndef NDEBUG
		if (!result.second)
		{
			throw runtime_error("Component is already added to the entity. ("
				"name: " + string(componentType.name()) +
				"entity:" + to_string(*entity) + ")");
		}
		#endif

		return componentView;
	}
	template<class T = Component>
	View<T> add(ID<Entity> entity)
	{
		static_assert(is_base_of_v<Component, T>,
			"Must be derived from the Component class.");
		return View<T>(add(entity, typeid(T)));
	}

//--------------------------------------------------------------------------------------------------
	void remove(ID<Entity> entity, type_index componentType)
	{ 
		assert(!entity.isNull());
		auto entityView = entities.get(entity);
		auto& components = entityView->components;
		auto iterator = components.find(componentType);

		#ifndef NDEBUG
		if (iterator == components.end())
		{
			throw runtime_error("Component is not added. ("
				"name: " + string(componentType.name()) +
				"entity:" + to_string(*entity) + ")");
		}
		#endif

		auto pair = iterator->second;
		components.erase(iterator);
		pair.first->destroyComponent(pair.second);
	}
	template<class T = Component>
	void remove(ID<Entity> entity)
	{ 
		static_assert(is_base_of_v<Component, T>,
			"Must be derived from the Component class.");
		remove(entity, typeid(T));
	}

//--------------------------------------------------------------------------------------------------
	View<Component> get(ID<Entity> entity, type_index componentType) const
	{
		assert(!entity.isNull());
		auto entityView = entities.get(entity);

		#ifndef NDEBUG
		if (entityView->components.find(componentType) ==
			entityView->components.end())
		{
			throw runtime_error("Component is not added. ("
				"name: " + string(componentType.name()) +
				"entity:" + to_string(*entity) + ")");
		}
		#endif

		auto pair = entityView->components.at(componentType);
		return pair.first->getComponent(pair.second);
	}
	template<class T = Component>
	View<T> get(ID<Entity> entity) const
	{
		static_assert(is_base_of_v<Component, T>,
			"Must be derived from the Component class.");
		return View<T>(get(entity, typeid(T)));
	}

	const LinearPool<Entity>& getEntities() const noexcept { return entities; }

	uint32_t getComponentCount(ID<Entity> entity) const
	{
		auto entityView = entities.get(entity);
		return (uint32_t)entityView->components.size();
	}

//--------------------------------------------------------------------------------------------------
	void initialize()
	{
		#ifndef NDEBUG
		if (isInitialized) throw runtime_error("Manager is already initialized.");
		#endif
		
		for (auto& system : systems) system->initialize();
		isInitialized = true;
	}

	void start()
	{
		#ifndef NDEBUG
		if (!isInitialized) throw runtime_error("Manager is not initialized.");
		#endif

		isRunning = true;
		while (isRunning) update();
	}
	void stop()
	{
		isRunning = false;
	}
	void update()
	{
		#ifndef NDEBUG
		if (!isInitialized) throw runtime_error("Manager is not initialized.");
		#endif

		for (auto system : systems) system->update();
		entities.dispose();
		for (auto system : systems) system->disposeComponents();
	}
};

//--------------------------------------------------------------------------------------------------
struct DoNotDestroyComponent : public Component { };

class DoNotDestroySystem : public System
{
protected:
	LinearPool<DoNotDestroyComponent, false> components;

	type_index getComponentType() const override {
		return typeid(DoNotDestroyComponent); }
	ID<Component> createComponent(ID<Entity> entity) override {
		return ID<Component>(components.create()); }
	void destroyComponent(ID<Component> instance) override {
		components.destroy(ID<DoNotDestroyComponent>(instance)); }
	View<Component> getComponent(ID<Component> instance) override {
		return View<Component>(components.get(ID<DoNotDestroyComponent>(instance))); }
	void disposeComponents() override { components.dispose(); }

	friend class ecsm::Manager;
};

} // ecsm