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

#include "ecsm.hpp"

using namespace ecsm;

//**********************************************************************************************************************
ID<Component> System::createComponent(ID<Entity> entity)
{
	throw EcsmError("System has no components.");
}
void System::destroyComponent(ID<Component> instance)
{
	throw EcsmError("System has no components.");
}
void System::resetComponent(View<Component> component, bool full)
{
	throw EcsmError("System has no components.");
}
void System::copyComponent(View<Component> source, View<Component> destination)
{
	throw EcsmError("System has no components.");
}

std::string_view System::getComponentName() const
{
	return "";
}
std::type_index System::getComponentType() const
{
	return typeid(Component);
}
View<Component> System::getComponent(ID<Component> instance)
{
	return {};
}
void System::disposeComponents()
{
	return;
}

//**********************************************************************************************************************
bool Entity::destroy()
{
	for (uint32_t i = 0; i < count; i++)
	{
		const auto& componentData = components[i];
		componentData.system->destroyComponent(componentData.instance);
	}

	free(components);
	return true;
}

//**********************************************************************************************************************
Manager::Manager(bool setSingleton) : Singleton(setSingleton)
{
	std::string name = "PreInit"; events.emplace(std::move(name), new Event(name));
	name = "Init"; events.emplace(std::move(name), new Event(name));
	name = "PostInit"; events.emplace(std::move(name), new Event(name));
	name = "Update"; events.emplace(std::move(name), new Event(name));
	name = "PreDeinit"; events.emplace(std::move(name), new Event(name));
	name = "Deinit"; events.emplace(std::move(name), new Event(name));
	name = "PostDeinit"; events.emplace(std::move(name), new Event(name));
	orderedEvents.push_back(events["Update"]);
}
Manager::~Manager()
{
	if (initialized)
	{
		runEvent("PreDeinit");
		runEvent("Deinit");
		runEvent("PostDeinit");
	}

	entities.clear(false);

	#ifndef NDEBUG
	assert(!isChanging); // Destruction of the systems inside other create/destroy is not allowed.
	isChanging = true;
	#endif

	for (const auto& pair : systems)
		delete pair.second;
	for (const auto& pair : events)
		delete pair.second;

	#ifndef NDEBUG
	isChanging = false;
	#endif

	unsetSingleton();
}

void Manager::addSystem(System* system, std::type_index type)
{
	auto componentType = system->getComponentType();
	if (componentType != typeid(Component))
	{
		if (!componentTypes.emplace(componentType, system).second)
		{
			throw EcsmError("Component is already registered by the other system. ("
				"componentType: " + typeToString(componentType) + ", "
				"thisSystem: " + typeToString(type) + ")");
		}
	}

	auto componentName = system->getComponentName();
	if (!componentName.empty())
	{
		if (!componentNames.emplace(componentName, system).second)
		{
			throw EcsmError("Component name is already registered by the other system. ("
				"componentName: " + std::string(componentName) + ", "
				"thisSystem: " + typeToString(type) + ")");
		}
	}

	if (!systems.emplace(type, system).second)
		throw EcsmError("System is already created. (name: " + typeToString(type) + ")");

	if (isRunning)
	{
		runEvent("PreInit");
		runEvent("Init");
		runEvent("PostInit");
	}
}

//**********************************************************************************************************************
void Manager::destroySystem(std::type_index type)
{
	#ifndef NDEBUG
	if (isChanging)
		throw EcsmError("Destruction of the system inside other create/destroy is not allowed.");
	isChanging = true;
	#endif

	auto searchResult = systems.find(type);
	if (searchResult == systems.end())
		throw EcsmError("System is not created. (type: " + typeToString(type) + ")");

	if (isRunning)
	{
		runEvent("PreDeinit");
		runEvent("Deinit");
		runEvent("PostDeinit");
	}

	auto system = searchResult->second;
	systems.erase(searchResult);
	
	auto componentName = system->getComponentName();
	if (!componentName.empty())
	{
		auto eraseResult = componentNames.erase(componentName);
		if (eraseResult != 1)
		{
			throw EcsmError("Failed to erase system component name. ("
				"componentName: " + std::string(componentName) + ", "
				"systemType: " + typeToString(type) + ")");
		}
	}

	auto componentType = system->getComponentType();
	if (componentType != typeid(Component))
	{
		auto eraseResult = componentTypes.erase(componentType);
		if (eraseResult != 1)
		{
			throw EcsmError("Failed to erase system component type. ("
				"componentType: " + typeToString(componentType) + ", "
				"systemType: " + typeToString(type) + ")");
		}
	}

	delete system;

	#ifndef NDEBUG
	isChanging = false;
	#endif
}
bool Manager::tryDestroySystem(std::type_index type)
{
	#ifndef NDEBUG
	if (isChanging)
		throw EcsmError("Destruction of the system inside other create/destroy is not allowed.");
	isChanging = true;
	#endif

	auto result = systems.find(type);
	if (result != systems.end())
	{
		#ifndef NDEBUG
		isChanging = false;
		#endif
		return false;
	}

	auto system = result->second;
	systems.erase(result);
	delete system;

	#ifndef NDEBUG
	isChanging = false;
	#endif
	return true;
}

//**********************************************************************************************************************
void Manager::addGroupSystem(std::type_index groupType, System* system)
{
	assert(system);

	auto result = systemGroups.find(groupType);
	if (result == systemGroups.end())
	{
		std::vector<System*> group = { system };
		systemGroups.emplace(groupType, std::move(group));
	}
	else
	{
		const auto& groupSystems = result->second;
		for (auto groupSystem : groupSystems)
		{
			if (system == groupSystem)
			{
				throw EcsmError("System is already added to the group. ("
					"groupType:" + typeToString(groupType) + ")");
			}
		}
		result.value().push_back(system);
	}
}
bool Manager::tryAddGroupSystem(std::type_index groupType, System* system)
{
	assert(system);

	auto result = systemGroups.find(groupType);
	if (result == systemGroups.end())
	{
		std::vector<System*> group = { system };
		systemGroups.emplace(groupType, std::move(group));
	}
	else
	{
		const auto& groupSystems = result->second;
		for (auto groupSystem : groupSystems)
		{
			if (system == groupSystem)
				return false;
		}
		result.value().push_back(system);
	}
	return true;
}

void Manager::removeGroupSystem(std::type_index groupType, System* system)
{
	auto result = systemGroups.find(groupType);
	if (result == systemGroups.end())
	{
		throw EcsmError("System group does not exist. ("
			"groupType:" + typeToString(groupType) + ")");
	}

	auto& groupSystems = result.value();
	for (auto i = groupSystems.begin(); i != groupSystems.end(); i++)
	{
		if (system != *i)
			continue;
		groupSystems.erase(i);
		return;
	}

	throw EcsmError("System is not added to the group. ("
		"groupType:" + typeToString(groupType) + ")");
}
bool Manager::tryRemoveGroupSystem(std::type_index groupType, System* system)
{
	auto result = systemGroups.find(groupType);
	if (result == systemGroups.end())
		return false;

	auto& groupSystems = result.value();
	for (auto i = groupSystems.begin(); i != groupSystems.end(); i++)
	{
		if (system != *i)
			continue;
		groupSystems.erase(i);
		return true;
	}

	return false;
}

//**********************************************************************************************************************
View<Component> Manager::add(ID<Entity> entity, std::type_index componentType)
{
	assert(entity);

	auto result = componentTypes.find(componentType);
	if (result == componentTypes.end())
	{
		throw EcsmError("Component is not registered by any system. ("
			"type: " + typeToString(componentType) + ", "
			"entity:" + std::to_string(*entity) + ")");
	}

	auto system = result->second;
	auto component = system->createComponent(entity);
	auto componentView = system->getComponent(component);
	componentView->entity = entity;

	auto entityView = entities.get(entity);
	if (entityView->findComponent(componentType.hash_code()))
	{
		throw EcsmError("Component is already added to the entity. ("
			"type: " + typeToString(componentType) + ", "
			"entity:" + std::to_string(*entity) + ")");
	}
	entityView->addComponent(componentType.hash_code(), system, component);
	return componentView;
}
void Manager::remove(ID<Entity> entity, std::type_index componentType)
{ 
	if (!entities.get(entity)->findComponent(componentType.hash_code()))
	{
		throw EcsmError("Component is not added. ("
			"type: " + typeToString(componentType) + ", "
			"entity:" + std::to_string(*entity) + ")");
	}
	if (!garbageComponents.emplace(std::make_pair(componentType.hash_code(), entity)).second)
	{
		throw EcsmError("Already removed component. ("
			"type: " + typeToString(componentType) +  ", "
			"entity: " + std::to_string(*entity) + ")");
	}
}

void Manager::copy(ID<Entity> source, ID<Entity> destination, std::type_index componentType)
{
	assert(source);
	assert(destination);
	auto srcComponentData = entities.get(source)->findComponent(componentType.hash_code());
	auto dstComponentData = entities.get(destination)->findComponent(componentType.hash_code());

	if (!srcComponentData)
	{
		throw EcsmError("Source component is not added. ("
			"type: " + typeToString(componentType) + ", "
			"entity:" + std::to_string(*source) + ")");
	}
	if (!dstComponentData)
	{
		throw EcsmError("Destination component is not added. ("
			"type: " + typeToString(componentType) + ", "
			"entity:" + std::to_string(*destination) + ")");
	}

	auto srcComponent = srcComponentData->system->getComponent(srcComponentData->instance);
	auto dstComponent = dstComponentData->system->getComponent(dstComponentData->instance);
	srcComponentData->system->resetComponent(dstComponent, false);
	srcComponentData->system->copyComponent(srcComponent, dstComponent);
}
ID<Entity> Manager::duplicate(ID<Entity> entity)
{
	auto duplicateEntity = entities.create();
	auto entityView = entities.get(entity);
	entities.get(duplicateEntity)->reserve(entityView->capacity);

	auto components = entityView->components;
	auto componentCount = entityView->count;

	for (uint32_t i = 0; i < componentCount; i++)
	{
		auto componentData = components[i];
		auto system = componentData.system;
		auto duplicateComponent = system->createComponent(duplicateEntity);
		auto sourceView = system->getComponent(componentData.instance);
		auto destinationView = system->getComponent(duplicateComponent);
		destinationView->entity = duplicateEntity;
		system->copyComponent(sourceView, destinationView);

		auto duplicateView = entities.get(duplicateEntity); // Do not optimize/move getter here!
		if (duplicateView->findComponent(componentData.type))
		{
			throw EcsmError("Component is already added to the entity. ("
				"type: " + typeToString(system->getComponentType()) + ", "
				"entity:" + std::to_string(*entity) + ")");
		}
		duplicateView->addComponent(componentData.type, system, duplicateComponent);
	}

	return duplicateEntity;
}
void Manager::resetComponents(ID<Entity> entity, bool full)
{
	auto entityView = entities.get(entity);
	auto components = entityView->components;
	auto componentCount = entityView->count;

	for (uint32_t i = 0; i < componentCount; i++)
	{
		const auto& componentData = components[i];
		auto componentView = componentData.system->getComponent(componentData.instance);
		componentData.system->resetComponent(componentView, full);
	}
}

//**********************************************************************************************************************
void Manager::registerEvent(std::string_view name)
{
	assert(!name.empty());
	if (!events.emplace(name, new Event(name, false)).second)
		throw EcsmError("Event is already registered. (name: " + std::string(name) + ")");
}
bool Manager::tryRegisterEvent(std::string_view name)
{
	assert(!name.empty());
	return events.emplace(name, new Event(name, false)).second;
}

void Manager::registerEventBefore(std::string_view newEvent, std::string_view beforeEvent)
{
	assert(!newEvent.empty());
	assert(!beforeEvent.empty());

	auto result = events.emplace(newEvent, new Event(newEvent));
	if (!result.second)
		throw EcsmError("Event is already registered. (newEvent: " + std::string(newEvent) + ")");

	for (auto i = orderedEvents.begin(); i != orderedEvents.end(); i++)
	{
		if ((*i)->name != beforeEvent)
			continue;
		orderedEvents.insert(i, result.first->second);
		return;
	}

	throw EcsmError("Before event is not registered. (newEvent: " + 
		std::string(newEvent) + ", beforeEvent: " + std::string(beforeEvent) + ")");
}
void Manager::registerEventAfter(std::string_view newEvent, std::string_view afterEvent)
{
	assert(!newEvent.empty());
	assert(!afterEvent.empty());

	auto result = events.emplace(newEvent, new Event(newEvent));
	if (!result.second)
		throw EcsmError("Event is already registered. (newEvent: " + std::string(newEvent) + ")");

	for (auto i = orderedEvents.begin(); i != orderedEvents.end(); i++)
	{
		if ((*i)->name != afterEvent)
			continue;
		orderedEvents.insert(i + 1, result.first->second);
		return;
	}

	throw EcsmError("After event is not registered. (newEvent: " + \
		std::string(newEvent) + ", afterEvent: " + std::string(afterEvent) + ")");
}

//**********************************************************************************************************************
void Manager::unregisterEvent(std::string_view name)
{
	assert(!name.empty());
	auto iterator = events.find(name);
	if (iterator == events.end())
		throw EcsmError("Event is not registered. (name: " + std::string(name) + ")");
		
	auto event = iterator->second;
	events.erase(iterator);

	if (event->isOrdered)
	{
		bool isEventFound = false;
		for (auto i = orderedEvents.begin(); i != orderedEvents.end(); i++)
		{
			if (*i != event)
				continue;
			orderedEvents.erase(i);
			isEventFound = true;
			break;
		}

		if (!isEventFound)
			throw EcsmError("Ordered event is not found. (name: " + std::string(name) + ")");
	}
		
	delete event;
}
bool Manager::tryUnregisterEvent(std::string_view name)
{
	assert(!name.empty());
	auto iterator = events.find(name);
	if (iterator == events.end())
		return false;

	auto event = iterator->second;
	events.erase(iterator);

	if (event->isOrdered)
	{
		bool isEventFound = false;
		for (auto i = orderedEvents.begin(); i != orderedEvents.end(); i++)
		{
			if (*i != event)
				continue;
			orderedEvents.erase(i);
			isEventFound = true;
			break;
		}

		if (!isEventFound)
			return false;
	}
		
	delete event;
	return true;
}

//**********************************************************************************************************************
const Manager::Event& Manager::getEvent(std::string_view name) const
{
	assert(!name.empty());
	auto result = events.find(name);
	if (result == events.end())
		throw EcsmError("Event is not registered. (name: " + std::string(name) + ")");
	return *result->second;
}
const Manager::Event* Manager::tryGetEvent(std::string_view name) const
{
	assert(!name.empty());
	auto result = events.find(name);
	if (result == events.end())
		return nullptr;
	return result->second;
}

//**********************************************************************************************************************
void Manager::runEvent(std::string_view name)
{
	assert(!name.empty());
	auto result = events.find(name);
	if (result == events.end())
		throw EcsmError("Event is not registered. (name: " + std::string(name) + ")");

	const auto& subscribers = result->second->subscribers;
	for (const auto& onEvent : subscribers)
		onEvent();
}
bool Manager::tryRunEvent(std::string_view name)
{
	assert(!name.empty());
	auto result = events.find(name);
	if (result == events.end())
		return false;

	const auto& subscribers = result->second->subscribers;
	for (const auto& onEvent : subscribers)
		onEvent();
	return true;
}
void Manager::runOrderedEvents()
{
	for (auto event : orderedEvents)
	{
		const auto& subscribers = event->subscribers;
		for (const auto& onEvent : subscribers)
			onEvent();
	}
}

//**********************************************************************************************************************
void Manager::subscribeToEvent(std::string_view name, const std::function<void()>& onEvent)
{
	assert(!name.empty());
	assert(onEvent);

	auto result = events.find(name);
	if (result == events.end())
		throw EcsmError("Event is not registered. (name: " + std::string(name) + ")");
	result->second->subscribers.push_back(onEvent);
}
void Manager::unsubscribeFromEvent(std::string_view name, const std::function<void()>& onEvent)
{
	assert(!name.empty());
	assert(onEvent);

	auto result = events.find(name);
	if (result == events.end())
		throw EcsmError("Event is not registered. (name: " + std::string(name) + ")");

	auto& subscribers = result->second->subscribers;
	for (auto i = subscribers.begin(); i != subscribers.end(); i++)
	{
		if (i->target_type() != onEvent.target_type())
			continue;
		subscribers.erase(i);
		return;
	}

	throw EcsmError("Event subscriber not found. (name: " + std::string(name) + ")");
}

//**********************************************************************************************************************
bool Manager::trySubscribeToEvent(std::string_view name, const std::function<void()>& onEvent)
{
	assert(!name.empty());
	assert(onEvent);

	auto result = events.find(name);
	if (result == events.end())
		return false;

	result->second->subscribers.push_back(onEvent);
	return true;
}
bool Manager::tryUnsubscribeFromEvent(std::string_view name, const std::function<void()>& onEvent)
{
	assert(!name.empty());
	assert(onEvent);

	auto result = events.find(name);
	if (result == events.end())
		return false;

	auto& subscribers = result->second->subscribers;
	for (auto i = subscribers.begin(); i != subscribers.end(); i++)
	{
		if (i->target_type() != onEvent.target_type())
			continue;
		subscribers.erase(i);
		return true;
	}

	return false;
}

//**********************************************************************************************************************
void Manager::initialize()
{
	if (initialized)
		throw EcsmError("Manager is already initialized.");

	runEvent("PreInit");
	runEvent("Init");
	runEvent("PostInit");
	initialized = true;
}

void Manager::update()
{
	if (!initialized)
		throw EcsmError("Manager is not initialized.");

	locker.lock();
	runOrderedEvents();
	disposeGarbageComponents();
	disposeEntities();
	disposeSystemComponents();
	locker.unlock();
}
void Manager::start()
{
	if (!initialized)
		throw EcsmError("Manager is not initialized.");

	isRunning = true;

	while (isRunning)
		update();
}

void Manager::disposeGarbageComponents()
{
	for (const auto& garbagePair : garbageComponents)
	{
		auto entityView = entities.get(garbagePair.second);
		auto componentData = entityView->findComponent(garbagePair.first);
		assert(componentData); // Corrupted entity component destruction order.
		componentData->system->destroyComponent(componentData->instance);
		entityView->removeComponent(componentData);
	}
	garbageComponents.clear();
}
void Manager::disposeSystemComponents()
{
	for (const auto& pair : systems)
		pair.second->disposeComponents();
}

//**********************************************************************************************************************
DoNotDestroySystem::DoNotDestroySystem(bool setSingleton) : Singleton(setSingleton) { }
DoNotDestroySystem::~DoNotDestroySystem() { unsetSingleton(); }

std::string_view DoNotDestroySystem::getComponentName() const
{
	return "Do Not Destroy";
}

DoNotDuplicateSystem::DoNotDuplicateSystem(bool setSingleton) : Singleton(setSingleton) { }
DoNotDuplicateSystem::~DoNotDuplicateSystem() { unsetSingleton(); }

std::string_view DoNotDuplicateSystem::getComponentName() const
{
	return "Do Not Duplicate";
}