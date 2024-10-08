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

#include "ecsm.hpp"

using namespace ecsm;

//**********************************************************************************************************************
ID<Component> System::createComponent(ID<Entity> entity)
{
	throw runtime_error("System has no components.");
}
void System::destroyComponent(ID<Component> instance)
{
	throw runtime_error("System has no components.");
}
void System::copyComponent(View<Component> source, View<Component> destination)
{
	throw runtime_error("System has no components.");
}

const string& System::getComponentName() const
{
	static const string name = "";
	return name;
}
type_index System::getComponentType() const
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
	for (const auto& component : components)
	{
		auto pair = component.second;
		pair.first->destroyComponent(pair.second);
	}
	components.clear();
	return true;
}

//**********************************************************************************************************************
Manager::Manager(bool setSingleton) : Singleton(setSingleton)
{
	auto name = "PreInit"; events.emplace(std::move(name), new Event(name));
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

void Manager::addSystem(System* system, type_index type)
{
	auto componentType = system->getComponentType();
	if (componentType != typeid(Component))
	{
		if (!componentTypes.emplace(componentType, system).second)
		{
			throw runtime_error("Component is already registered by the other system. ("
				"componentType: " + typeToString(componentType) + ", "
				"thisSystem: " + typeToString(type) + ")");
		}
	}

	const auto& componentName = system->getComponentName();
	if (!componentName.empty())
	{
		if (!componentNames.emplace(componentName, system).second)
		{
			throw runtime_error("Component name is already registered by the other system. ("
				"componentName: " + componentName + ", "
				"thisSystem: " + typeToString(type) + ")");
		}
	}

	if (!systems.emplace(type, system).second)
		throw runtime_error("System is already created. (name: " + typeToString(type) + ")");

	if (running)
	{
		runEvent("PreInit");
		runEvent("Init");
		runEvent("PostInit");
	}
}

//**********************************************************************************************************************
void Manager::destroySystem(type_index type)
{
	#ifndef NDEBUG
	if (isChanging)
		throw runtime_error("Destruction of the system inside other create/destroy is not allowed.");
	isChanging = true;
	#endif

	auto searchResult = systems.find(type);
	if (searchResult == systems.end())
		throw runtime_error("System is not created. (type: " + typeToString(type) + ")");

	if (running)
	{
		runEvent("PreDeinit");
		runEvent("Deinit");
		runEvent("PostDeinit");
	}

	auto system = searchResult->second;
	systems.erase(searchResult);
	
	const auto& componentName = system->getComponentName();
	if (!componentName.empty())
	{
		auto eraseResult = componentNames.erase(componentName);
		if (eraseResult != 1)
		{
			throw runtime_error("Failed to erase system component name. ("
				"componentName: " + componentName + ", "
				"systemType: " + typeToString(type) + ")");
		}
	}

	auto componentType = system->getComponentType();
	if (componentType != typeid(Component))
	{
		auto eraseResult = componentTypes.erase(componentType);
		if (eraseResult != 1)
		{
			throw runtime_error("Failed to erase system component type. ("
				"componentType: " + typeToString(componentType) + ", "
				"systemType: " + typeToString(type) + ")");
		}
	}

	delete system;

	#ifndef NDEBUG
	isChanging = false;
	#endif
}
bool Manager::tryDestroySystem(type_index type)
{
	#ifndef NDEBUG
	if (isChanging)
		throw runtime_error("Destruction of the system inside other create/destroy is not allowed.");
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
View<Component> Manager::add(ID<Entity> entity, type_index componentType)
{
	assert(entity);

	auto result = componentTypes.find(componentType);
	if (result == componentTypes.end())
	{
		throw runtime_error("Component is not registered by any system. ("
			"type: " + typeToString(componentType) +
			"entity:" + to_string(*entity) + ")");
	}

	auto system = result->second;
	auto component = system->createComponent(entity);
	auto componentView = system->getComponent(component);
	componentView->entity = entity;

	auto entityView = entities.get(entity);
	if (!entityView->components.emplace(componentType, make_pair(system, component)).second)
	{
		throw runtime_error("Component is already added to the entity. ("
			"type: " + typeToString(componentType) +
			"entity:" + to_string(*entity) + ")");
	}

	return componentView;
}
void Manager::remove(ID<Entity> entity, type_index componentType)
{ 
	assert(entity);
	auto entityView = entities.get(entity);
	const auto& components = entityView->components;
	auto iterator = components.find(componentType);

	if (iterator == components.end())
	{
		throw runtime_error("Component is not added. ("
			"type: " + typeToString(componentType) +
			"entity:" + to_string(*entity) + ")");
	}

	auto result = garbageComponents.emplace(make_pair(componentType, entity));
	if (!result.second)
	{
		throw runtime_error("Already removed component. ("
			"type: " + typeToString(componentType) + 
			"entity: " + to_string(*entity) + ")");
	}
}
void Manager::copy(ID<Entity> source, ID<Entity> destination, type_index componentType)
{
	assert(source);
	assert(destination);

	const auto sourceView = entities.get(source);
	auto destinationView = entities.get(destination);
	auto sourceIter = sourceView->components.find(componentType);
	auto destinationIter = destinationView->components.find(componentType);

	if (sourceIter == sourceView->components.end())
	{
		throw runtime_error("Source component is not added. ("
			"type: " + typeToString(componentType) +
			"entity:" + to_string(*source) + ")");
	}
	if (destinationIter == destinationView->components.end())
	{
		throw runtime_error("Destination component is not added. ("
			"type: " + typeToString(componentType) +
			"entity:" + to_string(*destination) + ")");
	}

	auto system = sourceIter->second.first;
	auto sourceComponent = system->getComponent(sourceIter->second.second);
	auto destinationComponent = system->getComponent(destinationIter->second.second);
	system->copyComponent(sourceComponent, destinationComponent);
}
ID<Entity> Manager::duplicate(ID<Entity> entity)
{
	auto duplicateEntity = entities.create();
	auto entityView = entities.get(entity);
	const auto& components = entityView->components;

	for (const auto& pair : components)
	{
		auto system = pair.second.first;
		auto duplicateComponent = system->createComponent(duplicateEntity);
		auto sourceView = system->getComponent(pair.second.second);
		auto destinationView = system->getComponent(duplicateComponent);
		destinationView->entity = duplicateEntity;

		system->copyComponent(sourceView, destinationView);

		auto duplicateView = entities.get(duplicateEntity); // Do not optimize/move getter here!
		if (!duplicateView->components.emplace(pair.first, make_pair(system, duplicateComponent)).second)
		{
			throw runtime_error("Component is already added to the entity. ("
				"type: " + typeToString(pair.first) +
				"entity:" + to_string(*entity) + ")");
		}
	}

	return duplicateEntity;
}

//**********************************************************************************************************************
void Manager::registerEvent(const string& name)
{
	assert(!name.empty());
	if (!events.emplace(name, new Event(name, false)).second)
		throw runtime_error("Event is already registered. (name: " + name + ")");
}
bool Manager::tryRegisterEvent(const string& name)
{
	assert(!name.empty());
	return events.emplace(name, new Event(name, false)).second;
}

void Manager::registerEventBefore(const string& newEvent, const string& beforeEvent)
{
	assert(!newEvent.empty());
	assert(!beforeEvent.empty());

	auto result = events.emplace(newEvent, new Event(newEvent));
	if (!result.second)
		throw runtime_error("Event is already registered. (newEvent: " + newEvent + ")");

	for (auto i = orderedEvents.begin(); i != orderedEvents.end(); i++)
	{
		if ((*i)->name != beforeEvent)
			continue;
		orderedEvents.insert(i, result.first->second);
		return;
	}

	throw runtime_error("Before event is not registered. ("
		"newEvent: " + newEvent + ", beforeEvent: " + beforeEvent + ")");
}
void Manager::registerEventAfter(const string& newEvent, const string& afterEvent)
{
	assert(!newEvent.empty());
	assert(!afterEvent.empty());

	auto result = events.emplace(newEvent, new Event(newEvent));
	if (!result.second)
		throw runtime_error("Event is already registered. (newEvent: " + newEvent + ")");

	for (auto i = orderedEvents.begin(); i != orderedEvents.end(); i++)
	{
		if ((*i)->name != afterEvent)
			continue;
		orderedEvents.insert(i + 1, result.first->second);
		return;
	}

	throw runtime_error("After event is not registered. ("
		"newEvent: " + newEvent + ", afterEvent: " + afterEvent + ")");
}

//**********************************************************************************************************************
void Manager::unregisterEvent(const string& name)
{
	assert(!name.empty());
	auto iterator = events.find(name);
	if (iterator == events.end())
		throw runtime_error("Event is not registered. (name: " + name + ")");
		
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
			throw runtime_error("Ordered event is not found. (name: " + name + ")");
	}
		
	delete event;
}
bool Manager::tryUnregisterEvent(const string& name)
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
bool Manager::isEventOrdered(const string& name) const
{
	assert(!name.empty());
	auto result = events.find(name);
	if (result == events.end())
		throw runtime_error("Event is not registered. (name: " + name + ")");
	return result->second->isOrdered;
}
const Manager::Event::Subscribers& Manager::getEventSubscribers(const string& name) const
{
	assert(!name.empty());
	auto result = events.find(name);
	if (result == events.end())
		throw runtime_error("Event is not registered. (name: " + name + ")");
	return result->second->subscribers;
}
bool Manager::isEventHasSubscribers(const string& name) const
{
	assert(!name.empty());
	auto result = events.find(name);
	if (result == events.end())
		throw runtime_error("Event is not registered. (name: " + name + ")");
	return !result->second->subscribers.empty();
}

//**********************************************************************************************************************
void Manager::runEvent(const string& name)
{
	assert(!name.empty());
	auto result = events.find(name);
	if (result == events.end())
		throw runtime_error("Event is not registered. (name: " + name + ")");

	const auto& subscribers = result->second->subscribers;
	for (const auto& onEvent : subscribers)
		onEvent();
}
bool Manager::tryRunEvent(const string& name)
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
void Manager::subscribeToEvent(const string& name, const std::function<void()>& onEvent)
{
	assert(!name.empty());
	assert(onEvent);

	auto result = events.find(name);
	if (result == events.end())
		throw runtime_error("Event is not registered. (name: " + name + ")");
	result->second->subscribers.push_back(onEvent);
}
void Manager::unsubscribeFromEvent(const string& name, const std::function<void()>& onEvent)
{
	assert(!name.empty());
	assert(onEvent);

	auto result = events.find(name);
	if (result == events.end())
		throw runtime_error("Event is not registered. (name: " + name + ")");

	auto& subscribers = result->second->subscribers;
	for (auto i = subscribers.begin(); i != subscribers.end(); i++)
	{
		if (i->target_type() != onEvent.target_type())
			continue;
		subscribers.erase(i);
		return;
	}

	throw runtime_error("Event subscriber not found. (name: " + name + ")");
}

//**********************************************************************************************************************
bool Manager::trySubscribeToEvent(const string& name, const std::function<void()>& onEvent)
{
	assert(!name.empty());
	assert(onEvent);

	auto result = events.find(name);
	if (result == events.end())
		return false;

	result->second->subscribers.push_back(onEvent);
	return true;
}
bool Manager::tryUnsubscribeFromEvent(const string& name, const std::function<void()>& onEvent)
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
		throw runtime_error("Manager is already initialized.");

	runEvent("PreInit");
	runEvent("Init");
	runEvent("PostInit");
	initialized = true;
}

void Manager::update()
{
	if (!initialized)
		throw runtime_error("Manager is not initialized.");

	runOrderedEvents();
	disposeGarbageComponents();
	disposeEntities();
	disposeSystemComponents();
}
void Manager::start()
{
	if (!initialized)
		throw runtime_error("Manager is not initialized.");

	running = true;
	while (running) update();
}

void Manager::disposeGarbageComponents()
{
	for (const auto& garbagePair : garbageComponents)
	{
		auto entityView = entities.get(garbagePair.second);
		auto& components = entityView->components;
		auto iterator = components.find(garbagePair.first);
		assert(iterator != components.end()); // Corrupted entity component destruction order.
		auto pair = iterator->second;
		pair.first->destroyComponent(pair.second);
		components.erase(iterator);
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

const string& DoNotDestroySystem::getComponentName() const
{
	static const string name = "Do Not Destroy";
	return name;
}

DoNotDuplicateSystem::DoNotDuplicateSystem(bool setSingleton) : Singleton(setSingleton) { }
DoNotDuplicateSystem::~DoNotDuplicateSystem() { unsetSingleton(); }

const string& DoNotDuplicateSystem::getComponentName() const
{
	static const string name = "Do Not Duplicate";
	return name;
}